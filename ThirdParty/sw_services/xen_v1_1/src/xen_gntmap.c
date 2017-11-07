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
#include "xen_gntmap.h"

#include "hypercall.h"
#include "xen_console.h"
#include "xen/xen.h"
#include "xen/grant_table.h"


/******** Definitions *********************************************************/


/******** Function Prototypes *************************************************/


/******** Module Variables ****************************************************/


/******** Private Functions ***************************************************/


/******** Public Functions ****************************************************/
int gntmap_map_grant_ref(unsigned long host_addr, uint32_t domid, uint32_t ref,
    int writeable)
{
    struct gnttab_map_grant_ref op;
    int status;

    op.ref = (grant_ref_t) ref;
    op.dom = (domid_t) domid;
    op.host_addr = (uint64_t) host_addr;
    op.flags = GNTMAP_host_map;
    if(!writeable)
    {
        op.flags |= GNTMAP_readonly;
    }

    status = HYPERVISOR_grant_table_op(GNTTABOP_map_grant_ref, &op, 1);
    if(status || op.status != GNTST_okay)
    {
        printk("error executing GNTTABOP_map_grant_ref hypercall\r\n");
        return -1;
    }

    return 0;
}
