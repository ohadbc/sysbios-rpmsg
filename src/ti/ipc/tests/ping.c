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
/*
 *  ======== ping.c ========
 *
 *  Works with the rpmsg_proto* Linux samples over the rpmsg-proto socket.
 *  rpmsg_proto* samples are in the host/ directory of this tree.
 */

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>

#include <ti/ipc/MultiProc.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <ti/ipc/rpmsg/VirtQueue.h>
#include <ti/srvmgr/NameMap.h>
#include <ti/ipc/rpmsg/MessageQCopy.h>

typedef UInt32 u32;
#include <ti/resources/rsc_table.h>

static UInt16 dstProc;
static MessageQCopy_Handle handle = NULL;
static UInt32 myEndpoint = 0;
static UInt32 counter = 0;

/* Send me a zero length data payload to tear down the MesssageQCopy object: */
static Void pingCallbackFxn(MessageQCopy_Handle h, UArg arg, Ptr data,
	UInt16 len, UInt32 src)
{
    Char  buffer[128];

    memcpy(buffer, data, len);
    buffer[len] = '\0';
    System_printf("%d: Received data: %s, len:%d\n", counter++, buffer, len);

    /* Send data back to remote endpoint: */
    MessageQCopy_send(dstProc, src, myEndpoint, (Ptr)buffer, len);
}

Void pingTaskFxn(UArg arg0, UArg arg1)
{
    System_printf("ping_task at port %d: Entered...\n", arg0);

    /* Create the messageQ for receiving, and register callback: */
    handle = MessageQCopy_createEx(arg0, pingCallbackFxn, NULL, &myEndpoint);
    if (!handle) {
        System_abort("MessageQCopy_createEx failed\n");
    }

    /* Announce we are here: */
    NameMap_register("rpmsg-proto", arg0);

    /* Note: we don't get a chance to teardown with MessageQCopy_destroy() */
}

Int main(Int argc, char* argv[])
{

    System_printf("%s starting..\n", MultiProc_getName(MultiProc_self()));

    System_printf("%d resources at 0x%x\n", resources.num, resources);

    /* Plug vring interrupts, and spin until host handshake complete. */
    VirtQueue_startup(0);

    dstProc = MultiProc_getId("HOST");
    MessageQCopy_init(dstProc);

    BIOS_start();

    return (0);
}
