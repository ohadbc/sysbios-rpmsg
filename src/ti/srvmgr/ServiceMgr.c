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
 *  ======== ServiceMgr.c ========
 *  Simple server that handles requests to create threads (services) by name
 *  and provide an endpoint to the new thread.
 *
 */

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>

#include <ti/ipc/MultiProc.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

#include <ti/grcm/RcmTypes.h>
#include <ti/grcm/RcmServer.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include <ti/ipc/rpmsg/MessageQCopy.h>
#include "rpmsg_omx.h"
#include "NameMap.h"
#include "ServiceMgr.h"

/* Hard coded to match Host side, but can be moved into the BIOS resource tbl */
#define  SERVICE_MGR_PORT   60

#define  MAX_NAMELEN       64

/* This artificially limits the number of service instances on this core: */
#define  MAX_TUPLES        256
#define  FREE_TUPLE_KEY    0xFFFFFFFF

struct ServiceDef {
    Char                   name[MAX_NAMELEN];
    RcmServer_Params       rcmServerParams;
    Bool                   taken;
};

struct ServiceDef serviceDefs[ServiceMgr_NUMSERVICETYPES];

struct Tuple {
    UInt32   key;
    UInt32   value;
};
struct Tuple Tuples[MAX_TUPLES];


void serviceMgrTaskFxn(UArg arg0, UArg arg1);

Void ServiceMgr_init()
{
    Task_Params params;
    UInt     i;

    RcmServer_init();

    for (i = 0; i < ServiceMgr_NUMSERVICETYPES; i++) {
       serviceDefs[i].taken = FALSE;
    }

    for (i = 0; i < MAX_TUPLES; i++) {
       Tuples[i].key = FREE_TUPLE_KEY;
    }

    /* Create our ServiceMgr Thread: */
    Task_Params_init(&params);
    params.instance->name = "ServiceMgr";
    params.priority = 1;   /* Lowest priority thread */
    Task_create(serviceMgrTaskFxn, &params, NULL);
}

Bool ServiceMgr_register(String name, RcmServer_Params  *rcmServerParams)
{
    UInt              i;
    Bool              found = FALSE;
    struct ServiceDef *sd;

    if (strlen(name) >= MAX_NAMELEN) {
        System_printf("ServiceMgr_register: service name longer than %d\n",
                       MAX_NAMELEN );
    }
    else {
        /* Search the array for a free slot: */
        for (i = 0; (i < ServiceMgr_NUMSERVICETYPES) && (found == FALSE); i++) {
            if (!serviceDefs[i].taken) {
                sd = &serviceDefs[i];
                strcpy(sd->name, name);
                sd->rcmServerParams = *rcmServerParams;
                sd->taken = TRUE;
                found = TRUE;
                break;
            }
        }
    }

    return(found);
}

Void ServiceMgr_send(Service_Handle srvc, Ptr data, UInt16 len)
{
    UInt32 local;
    UInt32 remote;
    struct omx_msg_hdr * hdr = (struct omx_msg_hdr *)data;
    UInt16 dstProc;

    /* Get reply endpoint and local address for sending: */
    remote  = RcmServer_getRemoteAddress(srvc);
    dstProc = RcmServer_getRemoteProc(srvc);
    local   = RcmServer_getLocalAddress(srvc);

    /* Set special rpmsg_omx header so Linux side can strip it off: */
    hdr->type    = OMX_RAW_MSG;
    hdr->len     = len;
    hdr->flags   = 0;

    /* Send it off (and no response expected): */
    MessageQCopy_send(dstProc, remote, local, data, HDRSIZE+len);
}


/* Tuple store/retrieve fxns:  */

static Bool storeTuple(UInt32 key, UInt32 value)
{
    UInt              i;
    Bool              stored = FALSE;

    if (key == FREE_TUPLE_KEY) {
        System_printf("storeTuple: reserved key %d\n", key);
    }
    else {
        /* Search the array for a free slot: */
        for (i = 0; (i < MAX_TUPLES) && (stored == FALSE); i++) {
           if (Tuples[i].key == FREE_TUPLE_KEY) {
               Tuples[i].key = key;
               Tuples[i].value = value;
               stored = TRUE;
               break;
           }
        }
    }

    return(stored);
}

static Bool removeTuple(UInt32 key, UInt32 * value)
{
    UInt              i;
    Bool              found = FALSE;

    /* Search the array for tuple matching key: */
    for (i = 0; (i < MAX_TUPLES) && (found == FALSE); i++) {
       if (Tuples[i].key == key) {
           found = TRUE;
           *value = Tuples[i].value;
           /* and free it... */
           Tuples[i].value = 0;
           Tuples[i].key = FREE_TUPLE_KEY;
           break;
       }
    }

    return(found);
}

static UInt32 createService(Char * name, UInt32 * endpt)
{
    Int i;
    Int status = 0;
    struct ServiceDef *sd;
    RcmServer_Handle  rcmSrvHandle;

    for (i = 0; i < ServiceMgr_NUMSERVICETYPES; i++) {
       if (!strcmp(name, serviceDefs[i].name)) {
           sd = &serviceDefs[i];
           break;
       }
    }

    if (i >= ServiceMgr_NUMSERVICETYPES) {
       System_printf("createService: unrecognized service name: %s\n", name);
       return OMX_NOTSUPP;
    }

    /* Create the RcmServer instance. */
#if 0
    System_printf("createService: Calling RcmServer_create with name = %s\n"
                  "priority = %d\n"
                  "osPriority = %d\n"
                  "rcmServerParams.fxns.length = %d\n",
                  sd->name, sd->rcmServerParams.priority,
                  sd->rcmServerParams.osPriority,
                  sd->rcmServerParams.fxns.length);
#endif
    status = RcmServer_create(sd->name, &sd->rcmServerParams, &rcmSrvHandle);

    if (status < 0) {
        System_printf("createService: RcmServer_create() returned error %d\n",
                       status);
        return OMX_FAIL;
    }

    /* Get endpoint allowing clients to send messages to this new server: */
    *endpt = RcmServer_getLocalAddress(rcmSrvHandle);

    /* Store Server's endpoint with handle so we can cleanup on disconnect: */
    if (!storeTuple(*endpt, (UInt32)rcmSrvHandle))  {
        System_printf("createService: Limit reached on max instances!\n");
        RcmServer_delete(&rcmSrvHandle);
        return OMX_FAIL;
    }

    /* start the server */
    RcmServer_start(rcmSrvHandle);

    System_printf("createService: new OMX Service at endpoint: %d\n", *endpt);

    return OMX_SUCCESS;
}


static UInt32 deleteService(UInt32 addr)
{
    Int status = 0;
    RcmServer_Handle  rcmSrvHandle;

    if (!removeTuple(addr, (UInt32 *)&rcmSrvHandle))  {
       System_printf("deleteService: could not find service instance at"
                     " address: 0x%x\n", addr);
       return OMX_FAIL;
    }

    /* Destroy the RcmServer instance. */
    status = RcmServer_delete(&rcmSrvHandle);
    if (status < 0) {
        System_printf("deleteService: RcmServer_delete() returned error %d\n",
                       status);
        return OMX_FAIL;
    }

    System_printf("deleteService: removed RcmServer at endpoint: %d\n", addr);

    return OMX_SUCCESS;
}

void serviceMgrTaskFxn(UArg arg0, UArg arg1)
{
    MessageQCopy_Handle msgq;
    UInt32 local;
    UInt32 remote;
    Char msg[HDRSIZE + sizeof(struct omx_connect_req)];
    struct omx_msg_hdr * hdr = (struct omx_msg_hdr *)msg;
    struct omx_connect_rsp * rsp = (struct omx_connect_rsp *)hdr->data;
    struct omx_connect_req * req = (struct omx_connect_req *)hdr->data;
    struct omx_disc_req * disc_req = (struct omx_disc_req *)hdr->data;
    struct omx_disc_rsp * disc_rsp = (struct omx_disc_rsp *)hdr->data;
    UInt16 dstProc;
    UInt16 len;
    UInt32 newAddr = 0;


#ifdef BIOS_ONLY_TEST
    dstProc = MultiProc_self();
#else
    dstProc = MultiProc_getId("HOST");
#endif

    msgq = MessageQCopy_create(SERVICE_MGR_PORT, &local);

    System_printf("serviceMgr: started on port: %d\n", SERVICE_MGR_PORT);

    NameMap_register("rpmsg-omx", SERVICE_MGR_PORT);

    while (1) {
       MessageQCopy_recv(msgq, (Ptr)msg, &len, &remote, MessageQCopy_FOREVER);
       System_printf("serviceMgr: received msg type: %d from addr: %d\n",
                      hdr->type, remote);
       switch (hdr->type) {
           case OMX_CONN_REQ:
            /* This is a request to create a new service, and return
             * it's connection endpoint.
             */
            System_printf("serviceMgr: CONN_REQ: len: %d, name: %s\n",
                 hdr->len, req->name);

            rsp->status = createService(req->name, &newAddr);

            hdr->type = OMX_CONN_RSP;
            rsp->addr = newAddr;
            hdr->len = sizeof(struct omx_connect_rsp);
            len = HDRSIZE + hdr->len;
            break;

           case OMX_DISC_REQ:
            /* Destroy the service instance at given service addr: */
            System_printf("serviceMgr: OMX_DISCONNECT: len %d, addr: %d\n",
                 hdr->len, disc_req->addr);

            disc_rsp->status = deleteService(disc_req->addr);

            /* currently, no response expected from rpmsg_omx: */
            continue;
#if 0       // rpmsg_omx is not listening for this ... yet.
            hdr->type = OMX_DISC_RSP;
            hdr->len = sizeof(struct omx_disc_rsp);
            len = HDRSIZE + hdr->len;
            break;
#endif

           default:
            System_printf("unexpected msg type: %d\n", hdr->type);
            hdr->type = OMX_NOTSUPP;
            break;
       }

       System_printf("serviceMgr: Replying with msg type: %d to addr: %d "
                      " from: %d\n",
                      hdr->type, remote, local);
       MessageQCopy_send(dstProc, remote, local, msg, len);
    }
}
