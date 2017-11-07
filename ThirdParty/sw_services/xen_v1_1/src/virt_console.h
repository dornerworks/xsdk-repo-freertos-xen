
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
#ifndef _VIRT_CONSOLE_H_
#define _VIRT_CONSOLE_H_
#include "xen_console.h"
#include "xen_events.h"
void virt_console_init(void);

/*
Example code to hook up the interrupt handler for a Xilinx SDK standalone 
application using the standalone_v5_5.
################################################
#include "xparameters.h"
#include "xscugic.h"
#include "virt_console.h"
void enable_interrupt(void)
{
	Xil_ExceptionInit();
	XScuGic_DeviceInitialize(XPAR_SCUGIC_SINGLE_DEVICE_ID);

	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT,
			(Xil_ExceptionHandler)XScuGic_DeviceInterruptHandler,
			(void *)XPAR_SCUGIC_SINGLE_DEVICE_ID);

	XScuGic_RegisterHandler(XPAR_SCUGIC_0_CPU_BASEADDR, EVENT_IRQ,
					(Xil_ExceptionHandler)handle_event_irq,
					NULL);

	XScuGic_EnableIntr(XPAR_SCUGIC_0_DIST_BASEADDR, EVENT_IRQ);
	Xil_ExceptionEnableMask(XIL_EXCEPTION_IRQ);
	XScuGic_EnableIntr(XPAR_SCUGIC_0_DIST_BASEADDR, EVENT_IRQ);

}
*/

#endif /* VIRT_CONSOLE_H */ 
