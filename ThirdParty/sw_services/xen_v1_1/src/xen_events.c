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

#include "xen/xen.h"
#include "hypercall.h"
#include "arm64_ops.h"
#include "mm.h"
#include "xen_events.h"
#include "xen_console.h"
#include "xen/memory.h"
#include "xen/event_channel.h"

#define active_evtchns(cpu,sh,idx)              \
    ((sh)->evtchn_pending[idx] &                \
     ~(sh)->evtchn_mask[idx])

#define MAX_EVTCHN      32

struct event_action
{
	evtchn_port_t port;
    evtchn_handler_t handler;
    void * data;
};
typedef struct event_action event_action_t;

static event_action_t event_action_table[MAX_EVTCHN] = {{0}};
static __attribute__((aligned(0x1000))) u8 shared_info_page[1<<PAGE_SHIFT];
static struct shared_info* shared_info = NULL;
int in_callback;

static int do_event(evtchn_port_t port);
static void force_evtchn_callback(void);

// This is where we would call the handler for the specific event, but for now only supporting 1 event
static int do_event(evtchn_port_t port)
{
    event_action_t * action;

    clear_evtchn(port);

    if(port >= MAX_EVTCHN)
    {
        return 0;
    }

    action = &event_action_table[port];

    /* call the handler */
    if(port == action->port && action->handler != NULL)
    {
    	action->handler(action->data);
    }

    return 1;
}

 /*
 * Force a proper event-channel callback from Xen after clearing the
 * callback mask. We do this in a very simple manner, by making a call
 * down into Xen. The pending flag will be checked by Xen on return.
 */
static void force_evtchn_callback(void)
{
	#ifdef XEN_HAVE_PV_UPCALL_MASK
		int save;
	#endif
		vcpu_info_t *vcpu;
		vcpu = &shared_info->vcpu_info[smp_processor_id()];
	#ifdef XEN_HAVE_PV_UPCALL_MASK
		save = vcpu->evtchn_upcall_mask;
	#endif

    while (vcpu->evtchn_upcall_pending)
    {
		#ifdef XEN_HAVE_PV_UPCALL_MASK
				vcpu->evtchn_upcall_mask = 1;
		#endif
				barrier();
				handle_event_irq(NULL);
				 barrier();
		#ifdef XEN_HAVE_PV_UPCALL_MASK
				vcpu->evtchn_upcall_mask = save;
				barrier();
		#endif
    };
}

int evtchn_alloc_ubound(domid_t remote_dom, evtchn_port_t * remote_port)
{
    evtchn_alloc_unbound_t op;

    op.dom = DOMID_SELF;
    op.remote_dom = remote_dom;
    if(HYPERVISOR_event_channel_op(EVTCHNOP_alloc_unbound, &op) != 0)
    {
        printk("error executing EVTCHNOP_alloc_unbound hypercall\r\n");
        return -1;
    }

    *remote_port = op.port;

    return 0;
}

int evtchn_bind_interdomain(domid_t remote_dom, evtchn_port_t remote_port, evtchn_port_t * local_port)
{
    evtchn_bind_interdomain_t op;

    op.remote_dom = remote_dom;
    op.remote_port = remote_port;
    if(HYPERVISOR_event_channel_op(EVTCHNOP_bind_interdomain, &op) != 0)
    {
        printk("error executing EVTCHNOP_bind_interdomain hypercall\r\n");
        return -1;
    }

    *local_port = op.local_port;

    return 0;
}

// This function is used to register a callback function for a particular event channel
int register_event_handler(evtchn_port_t port, evtchn_handler_t handler, void * data)
{
    event_action_t * action;

    if(port >= MAX_EVTCHN)
    {
        return -1;
    }

    action = &event_action_table[port];

	if(0 != action->port && port != action->port)
	{
		return -1;
	}

	action->port = port;
	action->handler = handler;
    action->data = data;

	return 0;
}

void unbind_evtchn(evtchn_port_t port)
{
    event_action_t * action = &event_action_table[port];
    struct evtchn_close close;

    mask_evtchn(port);
    clear_evtchn(port);

    action->handler = NULL;
    action->data = NULL;
    wmb();
    action->port = 0;

    close.port = port;
    HYPERVISOR_event_channel_op(EVTCHNOP_close, &close);
}

// This function is used by the guest to notify Xen that an event has occurred
void notify_evtch(evtchn_port_t port)
{
    u32 op = (u32)port;
    HYPERVISOR_event_channel_op(EVTCHNOP_send, &op);
}

inline void mask_evtchn(evtchn_port_t port)
{
    struct shared_info* s = shared_info;
    synch_set_bit(port, &s->evtchn_mask[0]);
}

// This function makes it so an event can cause an interrupt to the guest
inline void unmask_evtchn(evtchn_port_t port)
{
	struct shared_info* s = shared_info;
    vcpu_info_t *vcpu_info = &s->vcpu_info[0];

    synch_clear_bit(port, &s->evtchn_mask[0]);

    /*
     * The following is basically the equivalent of 'hw_resend_irq'. Just like
     * a real IO-APIC we 'lose the interrupt edge' if the channel is masked.s
     */
    if (  synch_test_bit        (port,    &s->evtchn_pending[0]) &&
         !synch_test_and_set_bit(port / (sizeof(unsigned long) * 8),
              &vcpu_info->evtchn_pending_sel) )
    {
        vcpu_info->evtchn_upcall_pending = 1;
#ifdef XEN_HAVE_PV_UPCALL_MASK
        if ( !vcpu_info->evtchn_upcall_mask )
#endif
       force_evtchn_callback();
    }
}

// Clears the event flag so Xen can raise it again later
inline void clear_evtchn(evtchn_port_t port)
{
    shared_info_t *s = shared_info;
    synch_clear_bit(port, &s->evtchn_pending[0]);
}

// The IRQ handler for the interrupt that Xen uses for events (IRQ 31).
void handle_event_irq(void* data)
{
	unsigned long  l1, l2, l1i, l2i;
	unsigned int   port;
	int            cpu = 0;
	shared_info_t *s = shared_info;
	vcpu_info_t   *vcpu_info = &s->vcpu_info[cpu];

	vcpu_info->evtchn_upcall_pending = 0;
	wmb();

	l1 = xchg(&vcpu_info->evtchn_pending_sel, 0);
	while ( l1 != 0 )
	{
		l1i = __ffs(l1);
		l1 &= ~(1UL << l1i);
		while ( (l2 = active_evtchns(cpu, s, l1i)) != 0 )
		{
			l2i = __ffs(l2);
			l2 &= ~(1UL << l2i);

			port = (l1i * (sizeof(unsigned long) * 8)) + l2i;
			do_event(port);

		}
	}
}

// Initialize the guest's event framework
void init_events(void)
{
    xen_add_to_physmap_t xatp;

    /* Map shared_info page */
    xatp.domid = DOMID_SELF;
    xatp.idx = 0;
    xatp.space = XENMAPSPACE_shared_info;
    xatp.gpfn = VA_TO_GUEST_PAGE(shared_info_page);

    if(0 != HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp))
    {
    	printk("ERROR setting up shared memory!\r\n");
    }

    shared_info = (struct shared_info *)shared_info_page;
}

