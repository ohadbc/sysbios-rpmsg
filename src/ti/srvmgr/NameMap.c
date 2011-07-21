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

#include <string.h>

#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include <ti/ipc/MultiProc.h>
#include <ti/ipc/rpmsg/MessageQCopy.h>

#define RPMSG_NAME_SIZE 32

typedef unsigned int u32;

struct rpmsg_ns_msg {
    char name[RPMSG_NAME_SIZE]; /* name of service including 0 */
    u32 addr;                   /* address of the service */
    u32 flags;                  /* see below */
} __packed;

enum rpmsg_ns_flags {
    RPMSG_NS_CREATE = 0,
    RPMSG_NS_DESTROY = 1
};

static void sendMessage(Char * name, UInt32 port, enum rpmsg_ns_flags flags)
{
    struct rpmsg_ns_msg nsMsg;

    strncpy(nsMsg.name, name, RPMSG_NAME_SIZE);
    nsMsg.addr = port;
    nsMsg.flags = flags;

    MessageQCopy_send(MultiProc_getId("HOST"), 53, port, &nsMsg, sizeof(nsMsg));
}

void NameMap_register(Char * name, UInt32 port)
{
    System_printf("registering %s service on %d with HOST\n", name, port);
    sendMessage(name, port, RPMSG_NS_CREATE);
}

void NameMap_unregister(Char * name, UInt32 port)
{
    System_printf("un-registering %s service on %d with HOST\n", name, port);
    sendMessage(name, port, RPMSG_NS_DESTROY);
}
