/*
 * Copyright (c) 2011, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** ============================================================================
 *  @file       _IpcPower.h
 *  ============================================================================
 */

#ifndef ti_pm__IpcPower__include
#define ti_pm__IpcPower__include

#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Structures & Definitions
 * =============================================================================
 */

#define MIRQ34_SHIFT                    2
#define MIRQ37_SHIFT                    5
#define MIRQ38_SHIFT                    6
#define MIRQ39_SHIFT                    7

#define WUGEN_MAILBOX_BIT               (1 << MIRQ34_SHIFT)
#define WUGEN_GPT3_BIT                  (1 << MIRQ37_SHIFT)
#define WUGEN_GPT4_BIT                  (1 << MIRQ38_SHIFT)
#define WUGEN_GPT9_BIT                  (1 << MIRQ39_SHIFT)

/* Wake-up masks for interrupts 00-31 */
#define WUGEN_MEVT0                     0x4000100C
/* Wake-up masks for interrupts 32-63 */
#define WUGEN_MEVT1                     0x40001010

/* Enable only Mailbox interrupt as a Wakeup source for now */
#define WUGEN_INT_MASK                  (WUGEN_MAILBOX_BIT)

#define M3_SCR_REG                      0xE000ED10

#define SLEEPONEXIT_BIT                 1
#define DEEPSLEEP_BIT                   2
#define SEVONPEND_BIT                   4

#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* ti_ipc__IpcPower__include */
