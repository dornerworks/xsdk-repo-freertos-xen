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
#ifndef _XEN_EVENTS_H_
#define _XEN_EVENTS_H_

#include "types.h"
#include "xen/event_channel.h"

#define EVENT_IRQ	31

typedef void (*evtchn_handler_t)(void * data);

int evtchn_alloc_ubound(domid_t remote_dom, evtchn_port_t * remote_port);
int evtchn_bind_interdomain(domid_t remote_dom, evtchn_port_t remote_port, evtchn_port_t * local_port);
int register_event_handler(evtchn_port_t evtch_id, evtchn_handler_t fptr, void * data);
void unbind_evtchn(evtchn_port_t port);

void notify_evtch(evtchn_port_t evtch_id);

void mask_evtchn(evtchn_port_t port);
void unmask_evtchn(evtchn_port_t port);
void clear_evtchn(evtchn_port_t port);

void handle_event_irq(void* data);

void init_events(void);


#endif /* _XEN_EVENTS_H_ */
