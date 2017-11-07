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

#ifndef _XEN_STORE_H_
#define _XEN_STORE_H_

/******** Includes ************************************************************/
#include <stdlib.h>


/******** Definitions *********************************************************/
typedef unsigned long xenbus_transaction_t;

#define XBT_NIL ((xenbus_transaction_t)0)


/******** Public Functions ****************************************************/
int xenstore_transaction_start(xenbus_transaction_t * trans_id);
int xenstore_transaction_end(xenbus_transaction_t trans_id, int abort,
    int * again);

char * xenstore_read(xenbus_transaction_t trans_id, const char * dir,
    const char * node, size_t * len);
int    xenstore_read_int(xenbus_transaction_t trans_id, const char * dir,
    const char * node);

int xenstore_write(xenbus_transaction_t trans_id, const char * dir,
    const char * node, const char * value, size_t len);
int xenstore_printf(xenbus_transaction_t trans_id, const char * dir,
    const char * node, const char* format, ...);

void xenstore_init(void);

#endif /* _XEN_STORE_H_ */
