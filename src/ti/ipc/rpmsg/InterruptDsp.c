/*
 * Copyright (c) 2011-2012, Texas Instruments Incorporated
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
/*
 *  ======== Interrupt.c ========
 *  OMAP4430/Ducati Interrupt Manger
 */

#include <xdc/std.h>
#include <xdc/runtime/Assert.h>

#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/family/c64p/tesla/Wugen.h>

#include <ti/ipc/MultiProc.h>

#include "InterruptDsp.h"

/* Register access method. */
#define REG16(A)   (*(volatile UInt16 *) (A))
#define REG32(A)   (*(volatile UInt32 *) (A))

#define HOSTINT                 26
#define DSPINT                  55
#define M3INT_MBX               50
#define M3INT                   19

#define DSPEVENTID              5

/* Assigned mailboxes */
#define HOST_TO_SYSM3_MBX       0 /* Rx on SysM3 from Host */
#define M3_TO_HOST_MBX          1 /* Tx to Host from M3 */
#define DSP_TO_HOST_MBX         2 /* Tx to Host from DSP */
#define HOST_TO_DSP_MBX         3 /* Rx on DSP from Host */
#define SYSM3_TO_APPM3_MBX      4 /* Rx on AppM3 from Host/SysM3 */

#define MAILBOX_BASEADDR    (0x4A0F4000)

#define MAILBOX_MESSAGE(M)  (MAILBOX_BASEADDR + 0x040 + (0x4 * M))
#define MAILBOX_STATUS(M)   (MAILBOX_BASEADDR + 0x0C0 + (0x4 * M))
#define MAILBOX_REG_VAL(M)  (0x1 << (2 * M))

#define MAILBOX_IRQSTATUS_CLR_DSP   (MAILBOX_BASEADDR + 0x114)
#define MAILBOX_IRQENABLE_SET_DSP   (MAILBOX_BASEADDR + 0x118)
#define MAILBOX_IRQENABLE_CLR_DSP   (MAILBOX_BASEADDR + 0x11C)

Hwi_FuncPtr userFxn = NULL;

/*
 *************************************************************************
 *                      Proxy functions
 *************************************************************************
 */
Void InterruptProxy_intEnable()
{
    InterruptDsp_intEnable();
}

Void InterruptProxy_intDisable()
{
    InterruptDsp_intDisable();
}

Void InterruptProxy_intRegister(Hwi_FuncPtr fxn)
{
    InterruptDsp_intRegister(fxn);
}

Void InterruptProxy_intSend(UInt16 remoteProcId, UArg arg)
{

    InterruptDsp_intSend(remoteProcId, arg);
}

UInt InterruptProxy_intClear()
{
    return InterruptDsp_intClear();
}

/*
 *************************************************************************
 *                      Module functions
 *************************************************************************
 */

/*!
 *  ======== InterruptDsp_intEnable ========
 *  Enable remote processor interrupt
 */
Void InterruptDsp_intEnable()
{
    REG32(MAILBOX_IRQENABLE_SET_DSP) = MAILBOX_REG_VAL(HOST_TO_DSP_MBX);
}

/*!
 *  ======== InterruptDsp_intDisable ========
 *  Disables remote processor interrupt
 */
Void InterruptDsp_intDisable()
{
    REG32(MAILBOX_IRQENABLE_CLR_DSP) = MAILBOX_REG_VAL(HOST_TO_DSP_MBX);
}

/*!
 *  ======== InterruptDsp_intRegister ========
 */
Void InterruptDsp_intRegister(Hwi_FuncPtr fxn)
{
    Hwi_Params  hwiAttrs;
    UInt        key;

    userFxn = fxn;

    /* Disable global interrupts */
    key = Hwi_disable();

    /*
     * DSP interrupts are edge-triggered, so clear the interrupt status to
     * regenerate an interrupt if there are some messages sent prior to
     * booting the DSP.
     */
    REG32(MAILBOX_IRQSTATUS_CLR_DSP) = MAILBOX_REG_VAL(HOST_TO_DSP_MBX);

    Hwi_Params_init(&hwiAttrs);

    hwiAttrs.eventId = DSPINT;
    Hwi_create(DSPEVENTID,
               (Hwi_FuncPtr)InterruptDsp_isr,
               &hwiAttrs,
               NULL);

    /* Enable the interrupt */
    Wugen_enableEvent(DSPINT);
    Hwi_enableInterrupt(DSPEVENTID);

    /* Enable the mailbox interrupt to the M3 core */
    InterruptDsp_intEnable();

    /* Restore global interrupts */
    Hwi_restore(key);
}

/*!
 *  ======== InterruptDsp_intSend ========
 *  Send interrupt to the remote processor
 */
Void InterruptDsp_intSend(UInt16 remoteProcId, UArg arg)
{
    if (remoteProcId == MultiProc_getId("CORE0")) {
        REG32(MAILBOX_MESSAGE(HOST_TO_SYSM3_MBX)) = arg;
    }
    else if (remoteProcId == MultiProc_getId("CORE1")) {
        REG32(MAILBOX_MESSAGE(SYSM3_TO_APPM3_MBX)) = arg;
    }
    else if (remoteProcId == MultiProc_getId("HOST")) {
        REG32(MAILBOX_MESSAGE(DSP_TO_HOST_MBX)) = arg;
    }
    else if (remoteProcId == MultiProc_getId("DSP")) {
        REG32(MAILBOX_MESSAGE(HOST_TO_DSP_MBX)) = arg;
    }
}

/*!
 *  ======== InterruptDsp_intClear ========
 *  Clear interrupt and return payload
 */
UInt InterruptDsp_intClear()
{
    UInt arg = INVALIDPAYLOAD;

    if (REG32(MAILBOX_STATUS(HOST_TO_DSP_MBX)) != 0) {
        /* If there is a message, return the argument to the caller */
        arg = REG32(MAILBOX_MESSAGE(HOST_TO_DSP_MBX));
        REG32(MAILBOX_IRQSTATUS_CLR_DSP) = MAILBOX_REG_VAL(HOST_TO_DSP_MBX);
    }

    return (arg);
}

/*!
 *  ======== InterruptDsp_isr ========
 */
Void InterruptDsp_isr(UArg arg)
{
    UArg payload;

    payload = InterruptDsp_intClear();
    if (payload != (UInt)INVALIDPAYLOAD) {
        userFxn(payload);
    }
}
