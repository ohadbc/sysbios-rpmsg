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
/*
 *  ======== resmgr_task.c ========
 *
 *  Test for the rpmsg_resmgr Linux service.
 */

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <ti/resmgr/IpcResource.h>

void IpcResourceTaskFxn(UArg arg0, UArg arg1)
{
    UInt32 status;
    IpcResource_Handle ipcResHandle;
    IpcResource_Gpt gpt;
    IpcResource_Auxclk auxclk;
    IpcResource_Regulator regulator;
    IpcResource_Gpio gpio;
    IpcResource_Sdma sdma;
    IpcResource_ResHandle gptId;
    IpcResource_ResHandle ivaId, ivasq0Id, ivasq1Id;
    IpcResource_ConstraintData ivaConstraints;
    IpcResource_ResHandle issId;
    IpcResource_ConstraintData issConstraints;
    IpcResource_ResHandle fdifId;
    IpcResource_ConstraintData fdifConstraints;
    IpcResource_ResHandle sl2ifId;
    IpcResource_ResHandle auxClkId;
    IpcResource_ResHandle regulatorId;
    IpcResource_ResHandle gpioId;
    IpcResource_ResHandle sdmaId;
    IpcResource_ResHandle ipuId;
    IpcResource_ConstraintData ipuConstraints;

    System_printf("Connecting to resmgr server ...\n");
    while (1) {
        ipcResHandle = IpcResource_connect(0);
        if (ipcResHandle) break;
        Task_sleep(1000);
    }
    System_printf("...connected to resmgr server.\n");

    System_printf("Requesting IPU \n");
    status = IpcResource_request(ipcResHandle, &ipuId,
                                 IpcResource_TYPE_IPU, NULL);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    ipuConstraints.mask = IpcResource_C_LAT | IpcResource_C_BW;
    ipuConstraints.latency = 10;
    ipuConstraints.bandwidth = 1000;

    System_printf("Requesting Constraints for IPU\n");
    status = IpcResource_requestConstraints(ipcResHandle, ipuId, &ipuConstraints);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    gpt.id = 3;
    gpt.srcClk = 0;

    System_printf("Requesting GPT  %d\n", gpt.id);
    status = IpcResource_request(ipcResHandle, &gptId,
                                 IpcResource_TYPE_GPTIMER, &gpt);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    System_printf("Releasing GPT  %d\n", gpt.id);
    IpcResource_release(ipcResHandle, gptId);

    Task_sleep(1000);

    auxclk.clkId = 3;
    auxclk.clkRate = 24;
    auxclk.parentSrcClk = 0x2;
    auxclk.parentSrcClkRate = 192;

    System_printf("Requesting AuxClk  %d\n", auxclk.clkId);
    status = IpcResource_request(ipcResHandle, &auxClkId,
                                 IpcResource_TYPE_AUXCLK, &auxclk);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    System_printf("Releasing Aux  %d\n", auxclk.clkId);
    IpcResource_release(ipcResHandle, auxClkId);

    Task_sleep(1000);

    auxclk.clkId = 1;
    auxclk.clkRate = 24;
    auxclk.parentSrcClk = 0x2;
    auxclk.parentSrcClkRate = 192;


    System_printf("Requesting AuxClk  %d\n", auxclk.clkId);
    status = IpcResource_request(ipcResHandle, &auxClkId,
                                 IpcResource_TYPE_AUXCLK, &auxclk);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    System_printf("Releasing Aux  %d\n", auxclk.clkId);
    IpcResource_release(ipcResHandle, auxClkId);

    Task_sleep(1000);

    regulator.regulatorId = 1;
    regulator.minUV = 2000000;
    regulator.maxUV = 2500000;

    System_printf("Requesting regulator  %d\n", regulator.regulatorId);
    status = IpcResource_request(ipcResHandle, &regulatorId,
                                 IpcResource_TYPE_REGULATOR, &regulator);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    System_printf("Releasing regulator  %d\n", regulator.regulatorId);
    IpcResource_release(ipcResHandle, regulatorId);

    Task_sleep(1000);

    gpio.gpioId = 50;

    System_printf("Requesting gpio  %d\n", gpio.gpioId);
    status = IpcResource_request(ipcResHandle, &gpioId,
                                 IpcResource_TYPE_GPIO, &gpio);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    System_printf("Releasing gpio  %d\n", gpio.gpioId);
    IpcResource_release(ipcResHandle, gpioId);

    Task_sleep(1000);

    sdma.numCh = 5;

    System_printf("Requesting %d sdma channels\n", sdma.numCh);
    status = IpcResource_request(ipcResHandle, &sdmaId,
                                 IpcResource_TYPE_SDMA, &sdma);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    System_printf("Releasing %d sdma channels\n", sdma.numCh);
    IpcResource_release(ipcResHandle, sdmaId);

    Task_sleep(1000);

    System_printf("Requesting IVAHD\n");
    status = IpcResource_request(ipcResHandle, &ivaId,
                                 IpcResource_TYPE_IVAHD, NULL);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    System_printf("Requesting IVASEQ0\n");
    status = IpcResource_request(ipcResHandle, &ivasq0Id,
                                 IpcResource_TYPE_IVASEQ0, NULL);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    System_printf("Requesting IVASEQ1\n");
    status = IpcResource_request(ipcResHandle, &ivasq1Id,
                                 IpcResource_TYPE_IVASEQ1, NULL);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    System_printf("Requesting SL2IF\n");
    status = IpcResource_request(ipcResHandle, &sl2ifId,
                                 IpcResource_TYPE_SL2IF, NULL);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    ivaConstraints.mask = IpcResource_C_FREQ | IpcResource_C_LAT | IpcResource_C_BW;
    ivaConstraints.frequency = 266100000;
    ivaConstraints.latency = 10;
    ivaConstraints.bandwidth = 1000;

    System_printf("Requesting Constraints for IVA\n");
    status = IpcResource_requestConstraints(ipcResHandle, ivaId, &ivaConstraints);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    System_printf("Releasing IVASEQ1\n");
    IpcResource_release(ipcResHandle, ivasq1Id);

    Task_sleep(1000);

    System_printf("Releasing IVASEQ0\n");
    IpcResource_release(ipcResHandle, ivasq0Id);

    Task_sleep(1000);

    System_printf("Releasing SL2IF\n");
    IpcResource_release(ipcResHandle, sl2ifId);

    Task_sleep(1000);

    System_printf("Releasing IVAHD\n");
    IpcResource_release(ipcResHandle, ivaId);

    Task_sleep(1000);

    System_printf("Requesting ISS\n");
    status = IpcResource_request(ipcResHandle, &issId,
                                 IpcResource_TYPE_ISS, NULL);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    issConstraints.mask = IpcResource_C_LAT | IpcResource_C_BW;
    issConstraints.latency = 10;
    issConstraints.bandwidth = 1000;

    System_printf("Requesting Constraints for ISS\n");
    status = IpcResource_requestConstraints(ipcResHandle, issId, &issConstraints);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    issConstraints.mask = IpcResource_C_FREQ | IpcResource_C_LAT | IpcResource_C_BW;
    issConstraints.frequency = 400000000;
    issConstraints.latency = 10;
    issConstraints.bandwidth = 1000;

    System_printf("Requesting Constraints for ISS\n");
    status = IpcResource_requestConstraints(ipcResHandle, issId, &issConstraints);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    System_printf("Releasing ISS\n");
    IpcResource_release(ipcResHandle, issId);

    System_printf("Requesting FDIF\n");
    status = IpcResource_request(ipcResHandle, &fdifId,
                                 IpcResource_TYPE_FDIF, NULL);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    fdifConstraints.mask = IpcResource_C_BW;
    fdifConstraints.bandwidth = 1000;

    System_printf("Requesting Constraints for FDIF\n");
    status = IpcResource_requestConstraints(ipcResHandle, fdifId, &fdifConstraints);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    fdifConstraints.mask = IpcResource_C_LAT;
    fdifConstraints.latency = 1000;

    System_printf("Requesting Constraints for FDIF\n");
    status = IpcResource_requestConstraints(ipcResHandle, fdifId, &fdifConstraints);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    fdifConstraints.mask = IpcResource_C_LAT;
    fdifConstraints.latency = -1;

    System_printf("Requesting Constraints for FDIF\n");
    status = IpcResource_requestConstraints(ipcResHandle, fdifId, &fdifConstraints);
    System_printf("status %d\n", status);

    Task_sleep(1000);

    System_printf("Releasing FDIF\n");
    IpcResource_release(ipcResHandle, fdifId);

    IpcResource_disconnect(ipcResHandle);

    return;
}

void start_resmgr_task()
{
    Task_Params params;

    Task_Params_init(&params);
    params.instance->name = "IpcResource";
    params.priority = 3;
    Task_create(IpcResourceTaskFxn, &params, NULL);
}
