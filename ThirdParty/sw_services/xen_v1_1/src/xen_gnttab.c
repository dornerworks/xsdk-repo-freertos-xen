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
#include "xen_gnttab.h"

#include <string.h>

#include "arm64_ops.h"
#include "hypercall.h"
#include "mm.h"
#include "types.h"
#include "xzd_bmc.h"
#include "xen_console.h"
#include "xen/memory.h"
#include "xen/grant_table.h"


/******** Definitions *********************************************************/
#define GRANT_TABLE_BASE    0x38000000
#define GRANT_TABLE_FRAMES  4

#define GRANT_ENTRIES       (GRANT_TABLE_FRAMES * PAGE_SIZE / sizeof(grant_entry_v1_t))


/******** Function Prototypes *************************************************/
static void put_free_entry(grant_ref_t gref);
static grant_ref_t get_free_entry(void);


/******** Module Variables ****************************************************/
static grant_entry_v1_t * gnttab_table;
static grant_ref_t        gnttab_list[GRANT_ENTRIES];


/******** Private Functions ***************************************************/
static void put_free_entry(grant_ref_t gref)
{
    local_irq_disable();

    gnttab_list[gref] = gnttab_list[0];
    gnttab_list[0] = gref;

    local_irq_enable();
    return;
}

static grant_ref_t get_free_entry(void)
{
    grant_ref_t gref;

    local_irq_disable();

    gref = gnttab_list[0];
    if(gref < GNTTAB_NR_RESERVED_ENTRIES || gref >= GRANT_ENTRIES)
    {
        local_irq_enable();
        return INVALID_GREF;
    }

    gnttab_list[0] = gnttab_list[gref];

    local_irq_enable();
    return gref;
}


/******** Public Functions ****************************************************/
grant_ref_t gnttab_grant_access(domid_t domid, unsigned long pfn, int readonly)
{
    grant_ref_t gref;

    gref = get_free_entry();
    if(gref == INVALID_GREF)
    {
        return INVALID_GREF;
    }

    gnttab_table[gref].frame = pfn;
    gnttab_table[gref].domid = domid;
    wmb();

    if(readonly)
    {
        gnttab_table[gref].flags = GTF_permit_access | GTF_readonly;
    }
    else
    {
        gnttab_table[gref].flags = GTF_permit_access;
    }

    return gref;
}

int gnttab_end_access(grant_ref_t gref)
{
    grant_entry_v1_t * entry;
    uint16_t flags;
    uint16_t nflags;

    if(gref < GNTTAB_NR_RESERVED_ENTRIES || gref >= GRANT_ENTRIES)
    {
        return -1;
    }

    entry = &gnttab_table[gref];

    nflags = entry->flags;
    do
    {
        flags = nflags;

        if(flags & (GTF_reading | GTF_writing))
        {
            return -1;
        }
    } while((nflags = synch_cmpxchg(&entry->flags, flags, 0)) != flags);

    put_free_entry(gref);

    return 0;
}

void gnttab_init(void)
{
    int    i;
    struct xen_add_to_physmap xatp;
    struct gnttab_setup_table setup;
    xen_pfn_t frames[GRANT_TABLE_FRAMES];

    /* Initialize grant table free list */
    memset(gnttab_list, 0, sizeof(gnttab_list));
    for(i = GNTTAB_NR_RESERVED_ENTRIES; i < GRANT_ENTRIES; i++)
    {
        put_free_entry(i);
    }

    /* Map the grant table pages */
    if(bmc_mem_mapper((void*)GRANT_TABLE_BASE, (void*)GRANT_TABLE_BASE,
        PAGE_SIZE * GRANT_TABLE_FRAMES, 2))
    {
        printk("grant table mem map failed\r\n");
        return;
    }

    /* Setup the grant table map with the Xen kernel */
    for(i = 0; i < GRANT_TABLE_FRAMES; i++)
    {
        xatp.domid = DOMID_SELF;
        xatp.size  = 0; /* Seems to be unused */
        xatp.space = XENMAPSPACE_grant_table;
        xatp.idx   = i;
        xatp.gpfn  = (GRANT_TABLE_BASE >> PAGE_SHIFT) + i;

        if(HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp))
        {
            printk("error executing XENMEM_add_to_physmap hypercall\r\n");
            return;
        }
    }

    setup.dom = DOMID_SELF;
    setup.nr_frames = GRANT_TABLE_FRAMES;
    set_xen_guest_handle(setup.frame_list, frames);
    HYPERVISOR_grant_table_op(GNTTABOP_setup_table, &setup, 1);
    if(setup.status)
    {
        printk("error executing GNTTABOP_setup_table hypercall\r\n");
        return;
    }

    /* BMC set up a direct VA:PA mapping */
    gnttab_table = (grant_entry_v1_t *)GRANT_TABLE_BASE;

    return;
}
