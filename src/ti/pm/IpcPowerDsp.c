/*
 * Copyright (c) 2012, Texas Instruments Incorporated
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
 *  @file       IpcPowerDsp.c
 *
 *  @brief      A simple Power Managment which responses to the host commands.
 *
 *  TODO:
 *     - Add suspend/resume notifications
 *  ============================================================================
 */

#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/Main.h>

#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/family/c64p/tesla/Power.h>
#include <ti/sysbios/family/c64p/tesla/Wugen.h>
#include <ti/sysbios/family/c64p/tesla/package/internal/Power.xdc.h>

#include <ti/ipc/MultiProc.h>
#include <ti/pm/IpcPower.h>
#include "_IpcPower.h"

#define REG32(A)   (*(volatile UInt32 *) (A))

#define PDCCMD_REG      0x01810000
#define SLEEP_MODE      0x15555
#define GPT5_IRQ        51
#define MBX_DSP_IRQ     55

static Swi_Handle suspendResumeSwi;

/* Module ref count: */
static Int curInit = 0;

/*
 *  ======== IpcPower_suspendSwi ========
 */
static Void IpcPower_suspendSwi(UArg arg0, UArg arg1)
{
    /* Disable wakeup events */
    Wugen_disableEvent(GPT5_IRQ);
    Wugen_disableEvent(MBX_DSP_IRQ);

    /* Invoke the BIOS suspend routine */
    Power_suspend(Power_Suspend_HIBERNATE);

    /* Re-enable wakeup events */
    Wugen_enableEvent(GPT5_IRQ);
    Wugen_enableEvent(MBX_DSP_IRQ);
}

/*
 * =============================================================================
 *  IpcPower Functions:
 * =============================================================================
 */

/*
 *  ======== IpcPower_init ========
 */
Void IpcPower_init()
{
    Swi_Params swiParams;

    if (curInit++) {
        return;
    }

    Swi_Params_init(&swiParams);
    swiParams.priority = Swi_numPriorities - 1; /* Max Priority Swi */
    suspendResumeSwi = Swi_create(IpcPower_suspendSwi, &swiParams, NULL);
}

/*
 *  ======== IpcPower_exit ========
 */
Void IpcPower_exit()
{
    --curInit;
}

/*
 *  ======== IpcPower_suspend ========
 */
Void IpcPower_suspend()
{
    Assert_isTrue((curInit > 0) , NULL);

    Swi_post(suspendResumeSwi);
}

/*
 *  ======== IpcPower_idle ========
 */
Void IpcPower_idle()
{
    REG32(PDCCMD_REG) = SLEEP_MODE;
    REG32(PDCCMD_REG);

    /* Enable wakeup events */
    Wugen_enableEvent(GPT5_IRQ);
    Wugen_enableEvent(MBX_DSP_IRQ);

    asm(" idle");
}
