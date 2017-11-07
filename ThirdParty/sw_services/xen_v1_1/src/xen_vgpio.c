/*
 * Copyright (c) 2017, DornerWorks, Ltd.
 *
 * THIS SOFTWARE IS PROVIDED BY DORNERWORKS FOR USE ON THE CONTRACTED PROJECT,
 * AND ANY EXPRESS OR IMPLIED WARRANTY IS LIMITED TO THIS USE. FOR ALL OTHER
 * USES THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL DORNERWORKS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/******** Includes ************************************************************/
#include "xen_vgpio.h"

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arm64_ops.h"
#include "mm.h"
#include "xen_events.h"
#include "xen_gnttab.h"
#include "xen_store.h"
#include "xen_bus.h"
#include "xen/io/gpioif.h"


/******** Definitions *********************************************************/
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define MAX_IRQ_REQUESTS 32

struct irq_map
{
    bool in_use;
    unsigned gpio;
    evtchn_port_t evtchn;
};

struct vgpio_dev
{
    domid_t otherdom;

    char node[256];

    struct xen_gpioif_sring * intf;
    grant_ref_t gref;
    evtchn_port_t evtch;

    int req_id;

    struct irq_map irq_map_table[MAX_IRQ_REQUESTS];
};


/******** Function Prototypes *************************************************/
static int talk_to_gpioback(struct vgpio_dev * dev);

static void write_req_buf(struct vgpio_dev * dev, const void * data,
    size_t len);
static void read_rsp_buf(struct vgpio_dev * dev, void * data, size_t len);

static int send_request(struct vgpio_dev * dev, uint16_t op, uint32_t pin,
    uint32_t * data);


/******** Module Variables ****************************************************/
static int vpgio_frontends = 0;


/******** Private Functions ***************************************************/
static int talk_to_gpioback(struct vgpio_dev * dev)
{
    xenbus_transaction_t trans_id = XBT_NIL;
    int again;

    /* Try until successfully written to xenstore */
    again = 1;
    while(again)
    {
        if(xenstore_transaction_start(&trans_id) != 0)
        {
            goto error;
        }

        if(xenstore_printf(trans_id, dev->node,
            "grant-ref", "%u", dev->gref) < 0)
        {
            goto error;
        }

        if(xenstore_printf(trans_id, dev->node,
            "event-channel", "%u", dev->evtch) < 0)
        {
            goto error;
        }

        if(xenstore_transaction_end(trans_id, 0, &again) != 0)
        {
            goto error;
        }
        trans_id = XBT_NIL;
    }

    return 0;

error:
    if(trans_id != XBT_NIL)
    {
        xenstore_transaction_end(trans_id, 1, &again);
    }

    return -1;
}

static void write_req_buf(struct vgpio_dev * dev, const void * data, size_t len)
{
    size_t index;
    struct xen_gpioif_sring * intf = dev->intf;
    xen_gpioif_sring_idx cons;
    xen_gpioif_sring_idx prod;

    prod = intf->req_prod;
    cons = intf->req_cons;
    mb();

    /* Poll until there is enough space in the ring */
    while((prod - cons) >= (XEN_GPIOIF_SRING_SIZE - len))
    {
        cons = intf->req_cons;
        mb();
    }

    /* Can't use memcpy because of ring wraparound */
    for(index = 0; index < len; index++)
    {
        intf->req[XEN_GPIOIF_SRING_MASK_IDX(prod)] = ((const char*)data)[index];
        prod++;
    }

    wmb();
    intf->req_prod = prod;

    wmb();
    notify_evtch(dev->evtch);

    return;
}

static void read_rsp_buf(struct vgpio_dev * dev, void * data, size_t len)
{
    size_t index;
    struct xen_gpioif_sring * intf = dev->intf;
    xen_gpioif_sring_idx cons;
    xen_gpioif_sring_idx prod;

    cons = intf->rsp_cons;
    prod = intf->rsp_prod;
    mb();

    /* Poll until full message in ring */
    while((prod - cons) < len)
    {
        prod = intf->rsp_prod;
        mb();
    }

    /* Can't use memcpy because of ring wraparound */
    for(index = 0; index < len; index++)
    {
        ((char *)data)[index] = intf->rsp[XEN_GPIOIF_SRING_MASK_IDX(cons)];
        cons++;
    }

    mb();
    intf->rsp_cons = cons;

    wmb();
    notify_evtch(dev->evtch);

    return;
}

static int send_request(struct vgpio_dev * dev, uint16_t op, uint32_t pin,
    uint32_t * data)
{
    struct xen_gpioif_request  req = {0};
    struct xen_gpioif_response rsp;

    /* Send request message */
    req.id = dev->req_id;
    req.op = op;
    req.pin = pin;

    if(data != NULL)
    {
        req.data = *data;
    }

    dev->req_id++;

    write_req_buf(dev, &req, sizeof(req));

    while(1)
    {
        /* Wait for response */
        read_rsp_buf(dev, &rsp, sizeof(rsp));
        if(rsp.id == req.id)
        {
            break;
        }
    }

    if(rsp.op != req.op)
    {
        /* Response didn't match the request */
        return -1;
    }

    if(rsp.status != XEN_GPIOIF_STATUS_SUCCESS)
    {
        /* Request failed */
        return -1;
    }

    if(data != NULL)
    {
        *data = rsp.data;
    }

    return 0;
}


/******** Public Functions ****************************************************/

struct vgpio_dev * vgpio_init(char * nodename)
{
    struct vgpio_dev * dev = NULL;

    dev = calloc(1, sizeof(struct vgpio_dev));
    if(dev == NULL)
    {
        goto error;
    }

    /* Format xenstore path */
    if(nodename == NULL)
    {
        snprintf(dev->node, sizeof(dev->node), "device/vgpio/%d", vpgio_frontends);
    }
    else
    {
        strncpy(dev->node, nodename, sizeof(dev->node));
        dev->node[sizeof(dev->node) - 1] = '\0'; /* Ensure string is NULL terminated */
    }
    vpgio_frontends++;

    dev->otherdom = xenstore_read_int(XBT_NIL, dev->node, "backend-id");

    /* Setup shared memory interface with backend */
    dev->intf = valloc(PAGE_SIZE);
    if(dev->intf == NULL)
    {
        goto error;
    }

    memset(dev->intf, 0, PAGE_SIZE);

    dev->gref = gnttab_grant_access(dev->otherdom, VA_TO_GUEST_PAGE(dev->intf), 0);
    if(dev->gref == INVALID_GREF)
    {
        goto error;
    }

    if(evtchn_alloc_ubound(dev->otherdom, &dev->evtch) != 0)
    {
        goto error;
    }

    /* Write driver paramters to xenstore */
    if(talk_to_gpioback(dev) != 0)
    {
        goto error;
    }

    /* Let backend know we are ready */
    xenbus_switch_state(XBT_NIL, dev->node, XenbusStateConnected);

    return dev;

error:
    if(dev != NULL)
    {
        if(dev->gref != INVALID_GREF)
        {
            gnttab_end_access(dev->gref);
        }

        free(dev->intf);
        free(dev);
    }

    return NULL;
}

bool vgpio_is_valid(struct vgpio_dev * dev, int number)
{
    uint32_t data = 0;

    if(send_request(dev, XEN_GPIOIF_OP_IS_VALID, number, &data) < 0)
    {
        return false;
    }

    if(data == 0)
    {
        return false;
    }

    return true;
}

int vgpio_request(struct vgpio_dev * dev, unsigned gpio)
{
    if(send_request(dev, XEN_GPIOIF_OP_REQUEST, gpio, NULL) < 0)
    {
        return -1;
    }

    return 0;
}

int vgpio_free(struct vgpio_dev * dev, unsigned gpio)
{
    if(send_request(dev, XEN_GPIOIF_OP_FREE, gpio, NULL) < 0)
    {
        return -1;
    }

    return 0;
}

int vgpio_direction_input(struct vgpio_dev * dev, unsigned gpio)
{
    if(send_request(dev, XEN_GPIOIF_OP_DIRECTION_INPUT, gpio, NULL) < 0)
    {
        return -1;
    }

    return 0;
}

int vgpio_direction_output(struct vgpio_dev * dev, unsigned gpio, int value)
{
    uint32_t data = (uint32_t)value;

    if(send_request(dev, XEN_GPIOIF_OP_DIRECTION_OUTPUT, gpio, &data) < 0)
    {
        return -1;
    }

    return 0;
}

int vgpio_get_value(struct vgpio_dev * dev, unsigned gpio)
{
    uint32_t data;

    if(send_request(dev, XEN_GPIOIF_OP_GET_VALUE, gpio, &data) < 0)
    {
        return -1;
    }

    return data;
}

int vgpio_set_value(struct vgpio_dev * dev, unsigned gpio, int value)
{
    uint32_t data = (uint32_t)value;

    if(send_request(dev, XEN_GPIOIF_OP_SET_VALUE, gpio, &data) < 0)
    {
        return -1;
    }

    return 0;
}

int vgpio_request_irq(struct vgpio_dev * dev, unsigned gpio,
    evtchn_handler_t handler, void * dev_id)
{
    int index;
    struct irq_map * map = NULL;
    evtchn_port_t evtchn;
    uint32_t data;

    /* Find a free entry in the irq map table */
    for(index = 0; index < MAX_IRQ_REQUESTS; index++)
    {
        if(dev->irq_map_table[index].in_use == false)
        {
            map = &dev->irq_map_table[index];
            break;
        }
    }

    if(map == NULL)
    {
        return -1;
    }

    if(evtchn_alloc_ubound(dev->otherdom, &evtchn) < 0)
    {
        return -1;
    }

    data = (uint32_t)evtchn;
    if(send_request(dev, XEN_GPIOIF_OP_REQUEST_IRQ, gpio, &data) < 0)
    {
        return -1;
    }

    if(register_event_handler(evtchn, handler, dev_id) < 0)
    {
        return -1;
    }

    map->in_use = true;
    map->gpio = gpio;
    map->evtchn = evtchn;

    unmask_evtchn(evtchn);

    return 0;
}

int vgpio_free_irq(struct vgpio_dev * dev, unsigned gpio)
{
    int index;
    struct irq_map * map = NULL;

    /* Look up gpio in irq map table */
    for(index = 0; index < MAX_IRQ_REQUESTS; index++)
    {
        if(dev->irq_map_table[index].in_use == true && dev->irq_map_table[index].gpio == gpio)
        {
            map = &dev->irq_map_table[index];
            break;
        }
    }

    if(map == NULL)
    {
        return -1;
    }

    if(send_request(dev, XEN_GPIOIF_OP_FREE_IRQ, gpio, NULL) < 0)
    {
        return -1;
    }

    unbind_evtchn(map->evtchn);

    map->in_use = false;
    map->gpio = 0;
    map->evtchn = 0;

    return 0;
}

int vgpio_enable_irq(struct vgpio_dev * dev, unsigned gpio)
{
    if(send_request(dev, XEN_GPIOIF_OP_ENABLE_IRQ, gpio, NULL) < 0)
    {
        return -1;
    }

    return 0;
}

int vgpio_disable_irq(struct vgpio_dev * dev, unsigned gpio)
{
    if(send_request(dev, XEN_GPIOIF_OP_DISABLE_IRQ, gpio, NULL) < 0)
    {
        return -1;
    }

    return 0;
}

int vgpio_set_irq_type(struct vgpio_dev * dev, unsigned gpio, unsigned int type)
{
    uint32_t data = (uint32_t)type;

    if(send_request(dev, XEN_GPIOIF_OP_SET_IRQ_TYPE, gpio, &data) < 0)
    {
        return -1;
    }

    return 0;
}
