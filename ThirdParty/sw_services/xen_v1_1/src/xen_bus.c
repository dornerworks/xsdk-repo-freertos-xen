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
#include "xen_bus.h"

#include <stdio.h>


/******** Definitions *********************************************************/


/******** Function Prototypes *************************************************/


/******** Module Variables ****************************************************/


/******** Private Functions ***************************************************/


/******** Public Functions ****************************************************/
int xenbus_switch_state(xenbus_transaction_t trans_id, const char* path,
    XenbusState state)
{
    int   again;
    int   xbt_flag = 0;
    char* strvalue = NULL;
    XenbusState current_state;

    again = 1;
    while(again)
    {
        /* Start transaction if one was not provided */
        if(trans_id == XBT_NIL)
        {
            if(xenstore_transaction_start(&trans_id) != 0)
            {
                goto error;
            }

            xbt_flag = 1;
        }

        /* Read device current state.  strvalue must be freed after use */
        strvalue = xenstore_read(trans_id, path, "state", NULL);
        if(strvalue == NULL)
        {
            goto error;
        }

        if(sscanf(strvalue, "%d", &current_state) != 1)
        {
            goto error;
        }

        free(strvalue);

        /* Write the new device state if it has changed */
        if(current_state != state)
        {
            if(xenstore_printf(trans_id, path, "state", "%d", state) < 0)
            {
                goto error;
            }
        }

        /* End transaction if it was started by this function */
        if(xbt_flag)
        {
            if(xenstore_transaction_end(trans_id, 0, &again) != 0)
            {
                goto error;
            }

            trans_id = XBT_NIL;
        }
    }

    return 0;

error:
    free(strvalue);

    if(xbt_flag && trans_id != XBT_NIL)
    {
        xenstore_transaction_end(trans_id, 0, &again);
    }

    return -1;
}
