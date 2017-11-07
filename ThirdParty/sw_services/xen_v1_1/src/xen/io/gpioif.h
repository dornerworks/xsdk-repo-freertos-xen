/*
 * Copyright DornerWorks 2016
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1.   Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
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

#ifndef __XEN_PUBLIC_IO_XEN_GPIOIF_H__
#define __XEN_PUBLIC_IO_XEN_GPIOIF_H__

/*
 * Operation codes
 */

#define XEN_GPIOIF_OP_IS_VALID           0
#define XEN_GPIOIF_OP_REQUEST            1
#define XEN_GPIOIF_OP_FREE               2
#define XEN_GPIOIF_OP_DIRECTION_INPUT    3
#define XEN_GPIOIF_OP_DIRECTION_OUTPUT   4
#define XEN_GPIOIF_OP_GET_VALUE          5
#define XEN_GPIOIF_OP_SET_VALUE          6
#define XEN_GPIOIF_OP_REQUEST_IRQ        7
#define XEN_GPIOIF_OP_FREE_IRQ           8
#define XEN_GPIOIF_OP_ENABLE_IRQ         9
#define XEN_GPIOIF_OP_DISABLE_IRQ       10

/*
 * XEN_GPIOIF_OP_SET_IRQ_TYPE
 * --------------------------------------
 *
 * This is sent by the frontend to configure the pin interrupt trigger
 *
 * Request:
 *
 *  op   = XEN_GPIOIF_OP_SET_IRQ_TYPE
 *  pin  = pin number
 *  data = XEN_GPIOIF_IRQ_TYPE_*
 *
 * Response:
 *
 *  status = XEN_GPIOIF_STATUS_SUCCESS     - Operation successful
 *           XEN_GPIOIF_STATUS_INVALID_PIN - Invalid vgpio pin specified
 *           XEN_GPIOIF_STATUS_GPIO_ERROR  - Gpio driver error
 */

#define XEN_GPIOIF_OP_SET_IRQ_TYPE      11

/*
 * XEN_GPIOIF_OP_SET_IRQ_TYPE flags
 */

#define XEN_GPIOIF_IRQ_TYPE_NONE            0x00000000
#define XEN_GPIOIF_IRQ_TYPE_EDGE_RISING     0x00000001
#define XEN_GPIOIF_IRQ_TYPE_EDGE_FALLING    0x00000002
#define XEN_GPIOIF_IRQ_TYPE_EDGE_BOTH       (XEN_GPIOIF_IRQ_TYPE_EDGE_RISING | \
                                             XEN_GPIOIF_IRQ_TYPE_EDGE_FALLING)
#define XEN_GPIOIF_IRQ_TYPE_LEVEL_HIGH      0x00000004
#define XEN_GPIOIF_IRQ_TYPE_LEVEL_LOW       0x00000008

/*
 * Status return codes
 */
/* Operation completed successfully */
#define XEN_GPIOIF_STATUS_SUCCESS           0
/* Operation failed due to invalid pin number */
#define XEN_GPIOIF_STATUS_INVALID_PIN       1
/* Operation failed due to gpio driver error */
#define XEN_GPIOIF_STATUS_GPIO_ERROR        2
/* Operation not supported by backend */
#define XEN_GPIOIF_STATUS_NOT_SUPPORTED     3

/*
 * GPIO requests (struct xen_gpioif_request)
 * ================================================
 *
 * All requests have the following format:
 *
 *    0     1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |    id     |    op     |          pin          |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |         data          |
 * +-----+-----+-----+-----+
 *
 * id: the request identifier, echoed in response.
 * op: the requested operation
 * pin: the GPIO pin number to operate on
 * data: any data associated with the request (determined by op)
 */

struct xen_gpioif_request {
    uint16_t id;
    uint16_t op;
    uint32_t pin;
    uint32_t data;
};

/*
 * GPIO responses (struct xen_gpioif_response)
 * ==================================================
 *
 * All responses have the following format:
 *
 *    0     1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |    id     |    op     |        status         |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |         data          |
 * +-----+-----+-----+-----+
 *
 * id: the corresponding request identifier
 * op: the operation of the corresponding request
 * status: the status of request processing
 * data: any data associated with the response (determined by type and status)
 */

struct xen_gpioif_response {
    uint16_t id;
    uint16_t op; 
    uint32_t status;
    uint32_t data;
};

/*
 * GPIO shared ring buffer interface
 */

#define XEN_GPIOIF_SRING_SIZE           1024
#define XEN_GPIOIF_SRING_MASK_IDX(idx)  ((idx) & (XEN_GPIOIF_SRING_SIZE-1))

typedef uint32_t xen_gpioif_sring_idx;

struct xen_gpioif_sring
{
    char req[XEN_GPIOIF_SRING_SIZE];
    char rsp[XEN_GPIOIF_SRING_SIZE];
    xen_gpioif_sring_idx req_cons, req_prod;
    xen_gpioif_sring_idx rsp_cons, rsp_prod;
};

#endif /* __XEN_PUBLIC_IO_XEN_GPIOIF_H__ */
