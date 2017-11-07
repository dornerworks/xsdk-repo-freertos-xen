# Copyright (c) 2017, DornerWorks, Ltd.
#
# THIS SOFTWARE IS PROVIDED BY DORNERWORKS FOR USE ON THE CONTRACTED PROJECT,
# AND ANY EXPRESS OR IMPLIED WARRANTY IS LIMITED TO THIS USE. FOR ALL OTHER
# USES THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL DORNERWORKS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#if defined(__LP64__) || defined(_LP64)
.equ lock, 1
.equ unlock, 0

.global _trylock_mutex
_trylock_mutex:
    ldr     w1, =lock
    ldxr    w2, [x0]
    cmp     w2, w1
    b.eq    locked
    stxr    w2, w1, [x0]
locked:
    dmb     sy
    mov     w0, w2
    ret

.global _unlock_mutex
_unlock_mutex:
    ldr     w1, =unlock
    dmb     sy
    str     w1, [x0]
    ret
#else
.equ lock, 1
.equ unlock, 0

.global _trylock_mutex
_trylock_mutex:
    ldr     r1, =lock
    ldrex   r2, [r0]
    cmp     r2, r1
    strexne r2, r1, [r0]
locked:
    dmb
    mov     r0, r2
    bx      lr

.global _unlock_mutex
_unlock_mutex:
    ldr     r1, =unlock
    dmb
    str     r1, [r0]
    bx      lr
#endif
