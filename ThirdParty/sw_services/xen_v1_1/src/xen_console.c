/*
 ****************************************************************************
 * (C) 2006 - Grzegorz Milos - Cambridge University
 ****************************************************************************
 *
 *        File: console.h
 *      Author: Grzegorz Milos
 *     Changes:
 *
 *        Date: Mar 2006
 *
 * Environment: Xen Minimal OS
 * Description: Console interface.
 *
 * Handles console I/O. Defines printk.
 *
 ****************************************************************************
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/*
Copyright DornerWorks 2016

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
following conditions are met:
1.	 Redistributions of source code must retain the above copyright notice, this list of conditions and the
following disclaimer.

THIS SOFTWARE IS PROVIDED BY DORNERWORKS FOR USE ON THE CONTRACTED PROJECT, AND ANY EXPRESS OR IMPLIED WARRANTY
IS LIMITED TO THIS USE. FOR ALL OTHER USES THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL DORNERWORKS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "hypercall.h"
#include "arm64_ops.h"
#include "xen/xen.h"
#include "xen/hvm/params.h"
#include "xen/io/console.h"
#include "xen_console.h"
#include "xen_events.h"
#include "xzd_bmc.h"
#include "mm.h"

//#define ECHO_TO_UART 1

// structure for asking Xen for some values
struct xen_param {
    u16  domid;
    u32  id;
    u64	 val;
};

static int console_initialised = 0;
static u64 guest_phys_page = -1;
static u32 cons_evtch = -1;

static void default_console_input_callback(char* data, int size)
{
	u8 byte;
	int i;
	static int crlf = 0;

	for(i = 0; i < size; i++)
	{
		byte = data[i];
		if(byte == '\r')
		{
			printk("\n");
#if ECHO_TO_UART
			printf("\n");
#endif
			crlf = 1;
		}
		else
		{
			crlf = 0;
		}

		if(!crlf || byte != '\n')
		{
			printk("%c", byte);
#if ECHO_TO_UART
			printf("%c", byte);
#endif
		}

	}
#if ECHO_TO_UART
	fflush(stdout);
#endif

}

static callback_func console_input_callback = default_console_input_callback;

void register_console_callback(callback_func fptr)
{
	if(NULL != fptr)
	{
		console_input_callback = fptr;
	}
}


// Ask Xen for a parameter that it knows about
static int get_hv_param(int param_id, uint64_t *value)
{
    struct xen_param xp;
    int ret;

    xp.domid = DOMID_SELF;
    xp.id = param_id;
    ret = HYPERVISOR_hvm_op(HVMOP_get_param, &xp);
    if(0 == ret)
    {
    	*value = xp.val;
    }

    return ret;
}

// Copies the outgoing characters into the ring buffer
static int xencons_ring_send_no_notify(const u8 *data, unsigned int len)
{
    int sent = 0;
	struct xencons_interface *intf;
	u32 cons, prod;


	intf = mfn_to_virt(guest_phys_page);
	cons = intf->out_cons;
	prod = intf->out_prod;
	mb();
	BUG_ON((prod - cons) > sizeof(intf->out));

	while ((sent < len) && ((prod - cons) < sizeof(intf->out)))
	{
		intf->out[MASK_XENCONS_IDX(prod++, intf->out)] = data[sent++];
	}
	wmb();
	intf->out_prod = prod;

    return sent;
}

// Copies the characters then raises an event to Xen
static int xencons_ring_send(const u8 *data, unsigned len)
{
    int sent;

    sent = xencons_ring_send_no_notify(data, len);
    notify_evtch(cons_evtch);

    return sent;
}

// Does some data reformatting to handle newline
static void console_print(char *data, int length)
{
    u8 *curr_char, saved_char;
    u8 copied_str[length+1];
    u8 *copied_ptr;
    int part_len;
    int (*ring_send_fn)(const u8 *data, unsigned length);

    if(!console_initialised)
        ring_send_fn = xencons_ring_send_no_notify;
    else
        ring_send_fn = xencons_ring_send;

    copied_ptr = copied_str;
    memcpy(copied_ptr, data, length);

    for(curr_char = copied_ptr; curr_char < copied_ptr+length-1; curr_char++)
    {
        if(*curr_char == '\n')
        {
            *curr_char = '\r';
            saved_char = *(curr_char+1);
            *(curr_char+1) = '\n';
            part_len = curr_char - copied_ptr + 2;
            ring_send_fn(copied_ptr, part_len);
            *(curr_char+1) = saved_char;
            copied_ptr = curr_char+1;
            length -= part_len - 1;
        }
    }

    if (copied_ptr[length-1] == '\n') {
        copied_ptr[length-1] = '\r';
        copied_ptr[length] = '\n';
        length++;
    }

    ring_send_fn(copied_ptr, length);
}

// This function will print to Xen's console one way or another
static void pvc_print(int direct, const char *fmt, va_list args)
{
    static char   buf[1024];

    (void)vsnprintf(buf, sizeof(buf), fmt, args);

    if(direct)
    {
    	// if console ring buffer/event not ready,  use the Xen console IO hypercall
        (void)HYPERVISOR_console_io(CONSOLEIO_write, strlen(buf), buf);
    }
    else
    {
        console_print(buf, strlen(buf));
    }
}

// Use this function to print to the Xen console
void printk(const char *fmt, ...)
{
    va_list       args;
	va_start(args, fmt);
    pvc_print(console_initialised!=1, fmt, args);
    va_end(args);
}


// Register this function with event handler to take care of console events
static void console_handle_input(void * arg)
{
	struct xencons_interface *intf = mfn_to_virt(guest_phys_page);
	u32 cons, prod;
	u8 byte;
	static int cr = 0;
	char data[sizeof(intf->in)+1] = {0};
	int i;


	cons = intf->in_cons;
	prod = intf->in_prod;
	mb();
	BUG_ON((prod - cons) > sizeof(intf->in));

	i = 0;
	while (cons != prod)
	{
		byte =  *(intf->in+MASK_XENCONS_IDX(cons,intf->in));
		cons++;
		data[i++] = byte;
	}

	mb();
	intf->in_cons = cons;
	notify_evtch(cons_evtch);
	console_input_callback(data, i);
}

// Initializes the Xen virtual console
void init_console(void)
{
	u64 val;
	int rv;

	init_events();

	// get the page where the console ring buffer is
    if(0 == get_hv_param(HVM_PARAM_CONSOLE_PFN, &val))
    {
    	guest_phys_page = val;
   	}

    // map that page where the console ring buffer is
    rv = bmc_mem_mapper(PAGE_TO_ADDR(guest_phys_page), PAGE_TO_ADDR(guest_phys_page), PAGE_SIZE, 2);
	if(rv)
	{
		printk("mmu init failed %d\r\n",rv);
		return;
	}

	// get event channel info necessary to do the things
    if(0 == get_hv_param(HVM_PARAM_CONSOLE_EVTCHN, &val))
    {
    	cons_evtch = val;
        register_event_handler(cons_evtch, console_handle_input, NULL);
    }
    else
    {
    	printk("error getting console event channel ID\r\n");
    }

    unmask_evtchn(cons_evtch);
    console_initialised = 1;
}
