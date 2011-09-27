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

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/Assert.h>
#include <ti/ipc/MultiProc.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <ti/ipc/rpmsg/MessageQCopy.h>
#include "IpcResource.h"
#include "_IpcResource.h"

#define DEFAULT_TIMEOUT 500
#define MAXMSGSIZE 84

static const UInt32 IpcResource_server = 100;

static UInt16 IpcResource_resLen(IpcResource_Type type)
{
    switch (type) {
    case IpcResource_TYPE_GPTIMER:
        return sizeof(IpcResource_Gpt);
    case IpcResource_TYPE_AUXCLK:
        return sizeof(IpcResource_Auxclk);
    case IpcResource_TYPE_REGULATOR:
        return sizeof(IpcResource_Regulator);
    case IpcResource_TYPE_GPIO:
        return sizeof(IpcResource_Gpio);
    case IpcResource_TYPE_SDMA:
        return sizeof(IpcResource_Sdma);
    }
    return 0;
}

static Int _IpcResource_translateError(Int kstatus)
{
    switch (kstatus) {
    case 0:
        return IpcResource_S_SUCCESS;
    case -ENOENT:
        return IpcResource_E_NORESOURCE;
    case -ENOMEM:
        return IpcResource_E_RETMEMORY;
    case -EBUSY:
        return IpcResource_E_BUSY;
    case -EINVAL:
        return IpcResource_E_INVALARGS;
    }
    return IpcResource_E_FAIL;
}

IpcResource_Handle IpcResource_connect(UInt timeout)
{
    UInt16 dstProc;
    UInt16 len;
    UInt32 remote;
    IpcResource_Handle handle;
    IpcResource_Req req;
    IpcResource_Ack ack;
    Int status;

    handle = Memory_alloc(NULL, sizeof(*handle), 0, NULL);
    if (!handle) {
        System_printf("IpcResource_connect: No memory");
        return NULL;
    }

    handle->timeout = (!timeout) ? DEFAULT_TIMEOUT :
                      (timeout == IpcResource_FOREVER) ? MessageQCopy_FOREVER :
                      timeout;

    dstProc = MultiProc_getId("HOST");

    handle->msgq= MessageQCopy_create(MessageQCopy_ASSIGN_ANY,
                                      &handle->endPoint);
    req.resType = 0;
    req.reqType = IpcResource_REQ_TYPE_CONN;
    req.resHandle = 0;
    status = MessageQCopy_send(dstProc, IpcResource_server,
                      handle->endPoint, &req, sizeof(req));
    if (status) {
        System_printf("IpcResource_connect: MessageQCopy_send "
                      " failed status %d\n", status);
        goto err;
    }

    status = MessageQCopy_recv(handle->msgq, &ack, &len, &remote,
                               handle->timeout);
    if (status) {
        System_printf("IpcResource_connect: MessageQCopy_recv "
                      "failed status %d\n", status);
        goto err;
    }
    status = _IpcResource_translateError(ack.status);
    if (status) {
        System_printf("IpcResource_connect: A9 Resource Manager "
                      "failed status %d\n", status);
        goto err;
    }

    return handle;
err:
    Memory_free(NULL, handle, sizeof(*handle));
    return NULL;
}

Int IpcResource_disconnect(IpcResource_Handle handle)
{
    Int status;
    IpcResource_Req req;

    if (!handle) {
        System_printf("IpcResource_disconnect: handle is NULL\n");
    }

    req.resType = 0;
    req.reqType = IpcResource_REQ_TYPE_DISCONN;

    status = MessageQCopy_send(MultiProc_getId("HOST"), IpcResource_server,
                        handle->endPoint, &req, sizeof(req));
    if (status) {
        System_printf("IpcResource_disconnect: MessageQCopy_send "
                      "failed status %d\n", status);
        return status;
    }

    status = MessageQCopy_delete(&handle->msgq);
    if (status) {
        System_printf("IpcResource_disconnect: MessageQCopy_delete "
                      "failed status %d\n", status);
        return status;
    }

    Memory_free(NULL, handle, sizeof(*handle));

    return 0;
}

Int IpcResource_request(IpcResource_Handle handle,
                        IpcResource_ResHandle *resHandle,
                        IpcResource_Type type, Void *resParams)
{
    Char msg[MAXMSGSIZE];
    IpcResource_Req *req = (Void *)msg;
    IpcResource_Ack *ack = (Void *)msg;
    UInt16 hlen = sizeof(*req);
    UInt16 alen = sizeof(*ack);
    UInt16 rlen = IpcResource_resLen(type);
    UInt16 len;
    UInt32 remote;
    Int status;

    if (!handle || !resHandle) {
        System_printf("IpcResource_request: Invalid paramaters\n");
        return IpcResource_E_INVALARGS;
    }

    if (rlen && !resParams) {
        System_printf("IpcResource_request: resource type %d "
                      "needs parameters\n", type);
        return IpcResource_E_INVALARGS;
    }

    req->resType = type;
    req->reqType = IpcResource_REQ_TYPE_ALLOC;

    memcpy(req->resParams, resParams, rlen);
    status = MessageQCopy_send(MultiProc_getId("HOST"), IpcResource_server,
                        handle->endPoint, req, hlen + rlen);
    if (status) {
        System_printf("IpcResource_request: MessageQCopy_send "
                      "failed status %d\n", status);
        status = IpcResource_E_FAIL;
        goto end;
    }

    status = MessageQCopy_recv(handle->msgq, ack, &len, &remote,
                               handle->timeout);
    if (status) {
        System_printf("IpcResource_request: MessageQCopy_recv "
                      "failed status %d\n", status);
        status = (status == MessageQCopy_E_TIMEOUT) ? IpcResource_E_TIMEOUT :
                 IpcResource_E_FAIL;
        goto end;
    }
    status = _IpcResource_translateError(ack->status);
    if (status) {
        System_printf("IpcResource_request: error from Host "
                      "failed status %d\n", status);
        goto end;
    }

    Assert_isTrue(len == (rlen + alen), NULL);

    *resHandle = ack->resHandle;
    memcpy(resParams, ack->resParams, rlen);
end:
    return status;
}

Int IpcResource_setConstraints(IpcResource_Handle handle,
                               IpcResource_ResHandle resHandle,
                               UInt32 action,
                               Void *constraints)
{
    Char msg[MAXMSGSIZE];
    IpcResource_Ack *ack = (Void *)msg;
    IpcResource_Req *req = (Void *)msg;
    UInt16 rlen = sizeof(IpcResource_ConstraintData);
    UInt16 alen = sizeof(*ack);
    UInt16 len;
    UInt32 remote;
    Int status;

    if (!handle) {
        System_printf("IpcResource_setConstraints: Invalid paramaters\n");
        return IpcResource_E_INVALARGS;
    }

    if (rlen && !constraints) {
        System_printf("IpcResource_setConstraints: needs parameters\n");
        return IpcResource_E_INVALARGS;
    }

    req->resType = 0;
    req->reqType = action;
    req->resHandle = resHandle;

    memcpy(req->resParams, constraints, rlen);
    status = MessageQCopy_send(MultiProc_getId("HOST"), IpcResource_server,
                        handle->endPoint, req, sizeof(*req) + rlen);
    if (status) {
        System_printf("IpcResource_setConstraints: MessageQCopy_send "
                      "failed status %d\n", status);
        status = IpcResource_E_FAIL;
        goto end;
    }

    if (action == IpcResource_REQ_TYPE_REL_CONSTRAINTS)
        goto end;

    status = MessageQCopy_recv(handle->msgq, ack, &len, &remote,
                               handle->timeout);
    if (status) {
        System_printf("IpcResource_setConstraints: MessageQCopy_recv "
                      "failed status %d\n", status);
        status = (status == MessageQCopy_E_TIMEOUT) ? IpcResource_E_TIMEOUT :
                 IpcResource_E_FAIL;
        goto end;
    }

    Assert_isTrue(len == (rlen + alen), NULL);

    status = _IpcResource_translateError(ack->status);

end:
    return status;
}

Int IpcResource_requestConstraints(IpcResource_Handle handle,
                                   IpcResource_ResHandle resHandle,
                                   Void *constraints)
{
    return IpcResource_setConstraints(handle,
                                      resHandle,
                                      IpcResource_REQ_TYPE_REQ_CONSTRAINTS,
                                      constraints);
}

Int IpcResource_releaseConstraints(IpcResource_Handle handle,
                                   IpcResource_ResHandle resHandle,
                                   Void *constraints)
{
    return IpcResource_setConstraints(handle,
                                      resHandle,
                                      IpcResource_REQ_TYPE_REL_CONSTRAINTS,
                                      constraints);
}

Int IpcResource_release(IpcResource_Handle handle,
                        IpcResource_ResHandle resHandle)
{
    Int status;
    IpcResource_Req req;

    if (!handle) {
        System_printf("IpcResource_release: handle is NULL\n");
        return IpcResource_E_INVALARGS;
    }

    req.resType = 0;
    req.reqType = IpcResource_REQ_TYPE_FREE;
    req.resHandle = resHandle;

    status = MessageQCopy_send(MultiProc_getId("HOST"), IpcResource_server,
                      handle->endPoint, &req, sizeof(req));
    if (status) {
        System_printf("IpcResource_release: MessageQCopy_send "
                      "failed status %d\n", status);
    }

    return status;
}
