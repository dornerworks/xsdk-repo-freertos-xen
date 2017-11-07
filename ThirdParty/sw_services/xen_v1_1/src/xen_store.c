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
#include "xen_store.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "arm64_ops.h"
#include "hypercall.h"
#include "mm.h"
#include "xen_console.h"
#include "xen_events.h"
#include "xzd_bmc.h"

#include "xen/xen.h"
#include "xen/hvm/hvm_op.h"
#include "xen/hvm/params.h"
#include "xen/io/xs_wire.h"


/******** Definitions *********************************************************/
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

typedef struct writeReq
{
    const void * data;
    size_t       len;
} write_req_t;


/******** Function Prototypes *************************************************/
char* path_join(const char* dir, const char* name);

static void write_req_buf(const void * data, size_t len);
static void read_rsp_buf(void * data, size_t len);
static int  send_request(enum xsd_sockmsg_type type,
    xenbus_transaction_t trans_id, write_req_t* req, size_t nr_reqs,
    struct xsd_sockmsg * resp);

static int get_hv_param(int paramid, uint64_t * value);


/******** Module Variables ****************************************************/
static evtchn_port_t                      xenstore_evtch;
static struct xenstore_domain_interface * xenstore_buf;
static uint32_t                           xenstore_req_id = 0;


/******** Private Functions ***************************************************/
char* path_join(const char* dir, const char* name)
{
    char* buffer;

    buffer = malloc(strlen(dir) + strlen(name) + 2); /* +2 for '/' and null character */
    if(buffer == NULL)
    {
        return NULL;
    }

    if(strlen(name) > 0)
    {
        snprintf(buffer, XENSTORE_ABS_PATH_MAX, "%s/%s", dir, name);
    }
    else
    {
        snprintf(buffer, XENSTORE_ABS_PATH_MAX, "%s", dir);
    }

    return buffer;
}

static void write_req_buf(const void * data, size_t len)
{
    XENSTORE_RING_IDX cons;
    XENSTORE_RING_IDX prod;
    size_t written = 0;

    while(written < len)
    {
        cons = xenstore_buf->req_cons;
        prod = xenstore_buf->req_prod;
        mb();

        if((prod - cons) >= XENSTORE_RING_SIZE)
        {
            /* Buffer is full, try again */
            continue;
        }

        while(((prod - cons) < XENSTORE_RING_SIZE) && (written < len))
        {
            xenstore_buf->req[MASK_XENSTORE_IDX(prod)] = ((const char*)data)[written];
            prod++;
            written++;
        }

        wmb();
        xenstore_buf->req_prod = prod;

        wmb();
        notify_evtch(xenstore_evtch);
    }

    return;
}

static void read_rsp_buf(void * data, size_t len)
{
    XENSTORE_RING_IDX cons;
    XENSTORE_RING_IDX prod;
    size_t read = 0;
    size_t skip;

    while(read < len)
    {
        cons = xenstore_buf->rsp_cons;
        prod = xenstore_buf->rsp_prod;
        mb();

        if(cons == prod)
        {
            /* Buffer is empty, try again */
            continue;
        }

        if(data != NULL)
        {
            /* Copy from the ring buffer */
            while((cons != prod) && (read < len))
            {
                ((char *)data)[read] = xenstore_buf->rsp[MASK_XENSTORE_IDX(cons)];
                cons++;
                read++;
            }
        }
        else
        {
            /* Skip over the data in the ring buffer */
            skip = MIN(len - read, prod - cons);
            cons += skip;
            read += skip;
        }

        mb();
        xenstore_buf->rsp_cons = cons;

        wmb();
        notify_evtch(xenstore_evtch);
    }

    return;
}

static int send_request(enum xsd_sockmsg_type type,
    xenbus_transaction_t trans_id, write_req_t* req, size_t nr_reqs,
    struct xsd_sockmsg * resp)
{
    int                index;
    struct xsd_sockmsg msg;

    /* Build request header */
    xenstore_req_id++;

    msg.type   = type;
    msg.req_id = xenstore_req_id;
    msg.tx_id  = trans_id;
    msg.len    = 0;

    for(index = 0; index < nr_reqs; index++)
    {
        msg.len += req[index].len;
    }

    /* Exceeding this limit will cause xenstore to kill the connection */
    if(msg.len > XENSTORE_PAYLOAD_MAX)
    {
        return -1;
    }

    /* Write the request header and payload */
    write_req_buf(&msg, sizeof(msg));
    for(index = 0; index < nr_reqs; index++)
    {
        write_req_buf(req[index].data, req[index].len);
    }

    /* Wait for the response */
    while(1)
    {
        /* Read message header */
        read_rsp_buf(resp, sizeof(struct xsd_sockmsg));
        if(resp->req_id == xenstore_req_id && resp->type != XS_WATCH_EVENT)
        {
            /* Received expected response */
            break;
        }

        /* Unexpected message, skip over message payload */
        read_rsp_buf(NULL, resp->len);
    }

    return 0;
}

static int get_hv_param(int paramid, uint64_t * value)
{
    xen_hvm_param_t hvmParam;

    hvmParam.domid = DOMID_SELF;
    hvmParam.index = paramid;

    if(HYPERVISOR_hvm_op(HVMOP_get_param, &hvmParam) != 0)
    {
        return -1;
    }

    *value = hvmParam.value;

    return 0;
}


/******** Public Functions ****************************************************/
int xenstore_transaction_start(xenbus_transaction_t * trans_id)
{
    int                retval = -1;
    write_req_t        payload = { "", 1 }; /* Null payload */
    struct xsd_sockmsg resp;
    char *             data = NULL;

    /* Send a request and wait for the response */
    if(send_request(XS_TRANSACTION_START, XBT_NIL, &payload, 1, &resp) != 0)
    {
        goto exit;
    }

    /* Allocate buffer and read response into it */
    data = malloc(resp.len + 1); /* +1 for null char */
    if(data == NULL)
    {
        read_rsp_buf(NULL, resp.len);
        goto exit;
    }

    read_rsp_buf(data, resp.len);
    data[resp.len] = '\0';

    if(resp.type == XS_ERROR)
    {
        /* xenstore responded with an error */
        goto exit;
    }

    /* Read the transaction ID */
    if(sscanf(data, "%u", trans_id) != 1)
    {
        goto exit;
    }

    retval = 0;

exit:
    free(data);

    return retval;
}

int xenstore_transaction_end(xenbus_transaction_t trans_id,
    int abort, int * again)
{
    int                retval = -1;
    write_req_t        payload;
    struct xsd_sockmsg resp;
    char *             data = NULL;

    *again = 0;

    payload.data = abort ? "F" : "T";
    payload.len  = strlen(payload.data) + 1; /* +1 for null char */

    /* Send a request and wait for the response */
    if(send_request(XS_TRANSACTION_END, trans_id, &payload, 1, &resp) != 0)
    {
        goto exit;
    }

    /* Allocate buffer and read response into it */
    data = malloc(resp.len + 1); /* +1 for null char */
    if(data == NULL)
    {
        read_rsp_buf(NULL, resp.len);
        goto exit;
    }

    read_rsp_buf(data, resp.len);
    data[resp.len] = '\0';

    if(resp.type == XS_ERROR)
    {
        if(strcmp(data, "EAGAIN") == 0)
        {
            /* Transaction needs to be repeated */ 
            *again = 1;
            retval = 0;
        }
        else
        {
            /* Unknown error */
            retval = -1;
        }
    }
    else
    {
        /* Success */
        retval = 0;
    }

exit:
    free(data);

    return retval;
}

char * xenstore_read(xenbus_transaction_t trans_id, const char * dir,
    const char * node, size_t * len)
{
    char *             retval = NULL;
    char *             path = NULL;
    write_req_t        payload;
    struct xsd_sockmsg resp;

    path = path_join(dir, node);
    if(path == NULL)
    {
        goto exit;
    }

    /* Send a request and wait for the response */
    payload.data = path;
    payload.len = strlen(path) + 1; /* +1 for null char */
    if(send_request(XS_READ, trans_id, &payload, 1, &resp) != 0)
    {
        goto exit;
    }

    /* After this point, the response must be read */

    if(resp.type == XS_ERROR)
    {
        /* xenstore responded with an error, skip over the response data */
        read_rsp_buf(NULL, resp.len);
        goto exit;
    }

    if(len != NULL)
    {
        *len = resp.len;
    }

    /* Allocate buffer and read response into it */
    retval = malloc(resp.len + 1); /* +1 for null char */
    if(retval == NULL)
    {
        read_rsp_buf(NULL, resp.len);
        goto exit;
    }

    read_rsp_buf(retval, resp.len);

    /* Value can be binary or ascii data - null terminate in case of ascii data */
    retval[resp.len] = '\0';

exit:
    free(path);

    return retval;
}

int xenstore_read_int(xenbus_transaction_t trans_id, const char * dir,
    const char * node)
{
    char * strval = NULL;
    int retval = -1;

    strval = xenstore_read(trans_id, dir, node, NULL);
    if(strval == NULL)
    {
        goto exit;
    }

    sscanf(strval, "%d", &retval);

exit:
    free(strval);

    return retval;
}

int xenstore_write(xenbus_transaction_t trans_id, const char * dir,
    const char * node, const char * value, size_t len)
{
    int                retval = -1;
    char *             path = NULL;
    write_req_t        payload[2];
    struct xsd_sockmsg resp;

    path = path_join(dir, node);
    if(path == NULL)
    {
        goto exit;
    }

    /* Send a request and wait for the response */
    payload[0].data = path;
    payload[0].len  = strlen(path) + 1; /* +1 for null char */
    payload[1].data = value;
    payload[1].len  = len;

    if(send_request(XS_WRITE, trans_id, payload, 2, &resp) != 0)
    {
        goto exit;
    }

    /* After this point, the response must be read */

    if(resp.type == XS_ERROR)
    {
        /* xenstore responded with an error */
        read_rsp_buf(NULL, resp.len);
        goto exit;
    }

    /* Read the response, should be "OK" */
    read_rsp_buf(NULL, resp.len);

    retval = 0;

exit:
    free(path);

    return retval;
}

int xenstore_printf(xenbus_transaction_t trans_id, const char * dir,
    const char * node, const char* format, ...)
{
    int     retval = 0;
    char *  value = NULL;
    int     size;
    va_list args;

    va_start(args, format);
    size = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if(size < 0)
    {
        retval = -1;
        goto exit;
    }

    value = malloc(size + 1);
    if(value == NULL)
    {
        retval = -1;
        goto exit;
    }

    va_start(args, format);
    retval = vsnprintf(value, size + 1, format, args);
    va_end(args);

    if(xenstore_write(trans_id, dir, node, value, strlen(value)) != 0)
    {
        retval = -1;
        goto exit;
    }

exit:
    free(value);

    return retval;
}

void xenstore_init(void)
{
    uint64_t param;

    /* Set up event channel with Xen store */
    if(get_hv_param(HVM_PARAM_STORE_EVTCHN, &param) != 0)
    {
        printk("error getting HVM_PARAM_STORE_EVTCHN parameter\r\n");
        return;
    }

    xenstore_evtch = (evtchn_port_t) param;

    /* Set up the shared memory ring buffer with Xen store */
    if(get_hv_param(HVM_PARAM_STORE_PFN, &param) != 0)
    {
        printk("error getting HVM_PARAM_STORE_PFN parameter\r\n");        
        return;
    }

    xenstore_buf = (struct xenstore_domain_interface *)(param * PAGE_SIZE);

    /* Map the ring buffer */
    if(bmc_mem_mapper(xenstore_buf, xenstore_buf, PAGE_SIZE, 2))
    {
        printk("grant table mem map failed\r\n");
        return;
    }

    return;
}
