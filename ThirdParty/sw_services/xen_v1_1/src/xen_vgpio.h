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

#ifndef _XEN_VGPIO_H_
#define _XEN_VGPIO_H_

/******** Includes ************************************************************/
#include <stdbool.h>

#include "xen_events.h"
#include "xen/io/gpioif.h"


/******** Definitions *********************************************************/
struct vgpio_dev;

/* IRQ type definitions */
#define VGPIO_IRQ_TYPE_NONE         (XEN_GPIOIF_IRQ_TYPE_NONE)
#define VGPIO_IRQ_TYPE_EDGE_RISING  (XEN_GPIOIF_IRQ_TYPE_EDGE_RISING)
#define VGPIO_IRQ_TYPE_EDGE_FALLING (XEN_GPIOIF_IRQ_TYPE_EDGE_FALLING)
#define VGPIO_IRQ_TYPE_EDGE_BOTH    (XEN_GPIOIF_IRQ_TYPE_EDGE_BOTH)
#define VGPIO_IRQ_TYPE_LEVEL_HIGH   (XEN_GPIOIF_IRQ_TYPE_LEVEL_HIGH)
#define VGPIO_IRQ_TYPE_LEVEL_LOW    (XEN_GPIOIF_IRQ_TYPE_LEVEL_LOW)


/******** Public Functions ****************************************************/
struct vgpio_dev * vgpio_init(char * nodename);

bool vgpio_is_valid(struct vgpio_dev * dev, int number);
int vgpio_request(struct vgpio_dev * dev, unsigned gpio);
int vgpio_free(struct vgpio_dev * dev, unsigned gpio);

int vgpio_direction_input(struct vgpio_dev * dev, unsigned gpio);
int vgpio_direction_output(struct vgpio_dev * dev, unsigned gpio, int value);
int vgpio_get_value(struct vgpio_dev * dev, unsigned gpio);
int vgpio_set_value(struct vgpio_dev * dev, unsigned gpio, int value);

int vgpio_request_irq(struct vgpio_dev * dev, unsigned gpio,
    evtchn_handler_t handler, void * data);
int vgpio_free_irq(struct vgpio_dev * dev, unsigned gpio);
int vgpio_enable_irq(struct vgpio_dev * dev, unsigned gpio);
int vgpio_disable_irq(struct vgpio_dev * dev, unsigned gpio);
int vgpio_set_irq_type(struct vgpio_dev * dev, unsigned gpio,
    unsigned int type);


#endif /* _XEN_VGPIO_H_ */
