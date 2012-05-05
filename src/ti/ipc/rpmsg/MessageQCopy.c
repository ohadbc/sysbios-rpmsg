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
 *  @file       MessageQCopy.c
 *
 *  @brief      A simple copy-based MessageQ, to work with Linux virtio_rp_msg.
 *
 *  ============================================================================
 */

/* this define must precede inclusion of any xdc header file */
#define Registry_CURDESC ti_ipc_rpmsg_MessageQCopy__Desc
#define MODULE_NAME "ti.ipc.rpmsg.MessageQCopy"

#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/Main.h>
#include <xdc/runtime/Registry.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/Diags.h>

#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/heaps/HeapBuf.h>
#include <ti/sysbios/gates/GateSwi.h>

#include <ti/sdo/utils/List.h>
#include <ti/ipc/MultiProc.h>

#include "MessageQCopy.h"
#include "VirtQueue.h"

/* =============================================================================
 * Structures & Enums
 * =============================================================================
 */

/* Various arbitrary limits: */
#define MAXMESSAGEQOBJECTS     256
#define MAXMESSAGEBUFFERS      512
#define MSGBUFFERSIZE          512   // Max payload + sizeof(ListElem)
#define MAXHEAPSIZE            (MAXMESSAGEBUFFERS * MSGBUFFERSIZE)
#define HEAPALIGNMENT          8

/* The MessageQCopy Object */
typedef struct MessageQCopy_Object {
    UInt32           queueId;      /* Unique id (procId | queueIndex)       */
    Semaphore_Handle semHandle;    /* I/O Completion                        */
    MessageQCopy_callback cb;      /* MessageQ Callback */
    UArg             arg;          /* Callback argument */
    List_Handle      queue;        /* Queue of pending messages             */
    Bool             unblocked;    /* Use with signal to unblock _receive() */
} MessageQCopy_Object;

/* Module_State */
typedef struct MessageQCopy_Module {
    /* Instance gate: */
    GateSwi_Handle gateSwi;
    /* Array of messageQObjects in the system: */
    struct MessageQCopy_Object  *msgqObjects[MAXMESSAGEQOBJECTS];
    /* Heap from which to allocate free messages for copying: */
    HeapBuf_Handle              heap;
} MessageQCopy_Module;

/* Message Header: Must match mp_msg_hdr in virtio_rp_msg.h on Linux side. */
typedef struct MessageQCopy_MsgHeader {
    Bits32 srcAddr;                 /* source endpoint addr               */
    Bits32 dstAddr;                 /* destination endpoint addr          */
    Bits32 reserved;                /* reserved                           */
    Bits16 dataLen;                 /* data length                        */
    Bits16 flags;                   /* bitmask of different flags         */
    UInt8  payload[];               /* Data payload                       */
} MessageQCopy_MsgHeader;

typedef MessageQCopy_MsgHeader *MessageQCopy_Msg;

/* Element to hold payload copied onto receiver's queue.                  */
typedef struct Queue_elem {
    List_Elem    elem;              /* Allow list linking.                */
    UInt         len;               /* Length of data                     */
    UInt32       src;               /* Src address/endpt of the msg       */
    Char         data[];            /* payload begins here                */
} Queue_elem;

/* Combine transport related objects into a struct for future migration: */
typedef struct MessageQCopy_Transport  {
    Swi_Handle       swiHandle;
    VirtQueue_Handle virtQueue_toHost;
    VirtQueue_Handle virtQueue_fromHost;
} MessageQCopy_Transport;


/* module diags mask */
Registry_Desc Registry_CURDESC;

static MessageQCopy_Module      module;
static MessageQCopy_Transport   transport;

/* We create a fixed size heap over this memory for copying received msgs */
#pragma DATA_ALIGN (recv_buffers, HEAPALIGNMENT)
static UInt8 recv_buffers[MAXHEAPSIZE];

/* Module ref count: */
static Int curInit = 0;

/*
 *  ======== MessageQCopy_swiFxn ========
 */
#define FXNN "MessageQCopy_swiFxn"
static Void MessageQCopy_swiFxn(UArg arg0, UArg arg1)
{
    Int16             token;
    MessageQCopy_Msg  msg;
    UInt16            dstProc = MultiProc_self();
    Bool              usedBufAdded = FALSE;
    int len;

    Log_print0(Diags_ENTRY, "--> "FXNN);

    /* Process all available buffers: */
    while ((token = VirtQueue_getAvailBuf(transport.virtQueue_fromHost,
                                         (Void **)&msg, &len))
         >= 0) {

        Log_print3(Diags_INFO, FXNN": \n\tReceived msg: from: 0x%x, "
                   "to: 0x%x, dataLen: %d",
                  (IArg)msg->srcAddr, (IArg)msg->dstAddr, (IArg)msg->dataLen);

        /* Pass to desitination queue (on this proc), or callback: */
        MessageQCopy_send(dstProc, msg->dstAddr, msg->srcAddr,
                         (Ptr)msg->payload, msg->dataLen);

        VirtQueue_addUsedBuf(transport.virtQueue_fromHost, token, RP_MSG_BUF_SIZE);
        usedBufAdded = TRUE;
    }

    if (usedBufAdded)  {
       /* Tell host we've processed the buffers: */
       VirtQueue_kick(transport.virtQueue_fromHost);
    }

    Log_print0(Diags_EXIT, "<-- "FXNN);
}
#undef FXNN

#define FXNN "callback_availBufReady"
static Void callback_availBufReady(VirtQueue_Handle vq)
{

    if (vq == transport.virtQueue_fromHost)  {
       /* Post a SWI to process all incoming messages */
        Log_print0(Diags_INFO, FXNN": virtQueue_fromHost kicked");
        Swi_post(transport.swiHandle);
    }
    else if (vq == transport.virtQueue_toHost) {
       /* Note: We post nothing for transport.virtQueue_toHost, as we assume the
        * host has already made all buffers available for sending.
        */
        Log_print0(Diags_INFO, FXNN": virtQueue_toHost kicked");
    }
}
#undef FXNN

/* =============================================================================
 *  MessageQCopy Functions:
 * =============================================================================
 */

/*
 *  ======== MessasgeQCopy_init ========
 *
 *
 */
#define FXNN "MessageQCopy_init"
Void MessageQCopy_init(UInt16 remoteProcId)
{
    GateSwi_Params gatePrms;
    HeapBuf_Params prms;
    int     i;
    Registry_Result result;

    Log_print1(Diags_ENTRY, "--> "FXNN": (remoteProcId=%d)",
                (IArg)remoteProcId);

    if (curInit++ != 0) {
        return; /* module already initialized */
    }

    /* register with xdc.runtime to get a diags mask */
    result = Registry_addModule(&Registry_CURDESC, MODULE_NAME);
    Assert_isTrue(result == Registry_SUCCESS, (Assert_Id)NULL);

    /* Gate to protect module object and lists: */
    GateSwi_Params_init(&gatePrms);
    module.gateSwi = GateSwi_create(&gatePrms, NULL);

    /* Initialize Module State: */
    for (i = 0; i < MAXMESSAGEQOBJECTS; i++) {
       module.msgqObjects[i] = NULL;
    }

    HeapBuf_Params_init(&prms);
    prms.blockSize    = MSGBUFFERSIZE;
    prms.numBlocks    = MAXMESSAGEBUFFERS;
    prms.buf          = recv_buffers;
    prms.bufSize      = MAXHEAPSIZE;
    prms.align        = HEAPALIGNMENT;
    module.heap       = HeapBuf_create(&prms, NULL);
    if (module.heap == 0) {
       System_abort("MessageQCopy_init: HeapBuf_create returned 0\n");
    }

    /*
     * Create a pair VirtQueues (one for sending, one for receiving).
     *
     * Note: order of these calls determines the virtqueue indices identifying
     * the vrings toHost and fromHost:  toHost is first!
     */
    transport.virtQueue_toHost   = VirtQueue_create(callback_availBufReady,
                                                    remoteProcId,
						    ID_SYSM3_TO_A9);
    transport.virtQueue_fromHost = VirtQueue_create(callback_availBufReady,
                                                    remoteProcId,
						    ID_A9_TO_SYSM3);

    /* construct the Swi to process incoming messages: */
    transport.swiHandle = Swi_create(MessageQCopy_swiFxn, NULL, NULL);

    Log_print0(Diags_EXIT, "<-- "FXNN);
}
#undef FXNN

/*
 *  ======== MessasgeQCopy_finalize ========
 */
#define FXNN "MessageQCopy_finalize"
Void MessageQCopy_finalize()
{

   Log_print0(Diags_ENTRY, "--> "FXNN);
   if (--curInit != 0) {
        return; /* module still in use */
   }

   /* Tear down Module: */
   HeapBuf_delete(&(module.heap));

   Swi_delete(&(transport.swiHandle));

   GateSwi_delete(&module.gateSwi);

    Log_print0(Diags_EXIT, "<-- "FXNN);
}
#undef FXNN

/*
 *  ======== MessageQCopy_createEx ========
 */
#define FXNN "MessageQCopy_create"
MessageQCopy_Handle MessageQCopy_createEx(UInt32 reserved,
                                        MessageQCopy_callback cb,
                                        UArg arg,
                                        UInt32 * endpoint)
{
    MessageQCopy_Object    *obj = NULL;
    Bool                   found = FALSE;
    Int                    i;
    UInt16                 queueIndex = 0;
    IArg key;

    Log_print3(Diags_ENTRY, "--> "FXNN": (reserved=%d, cb=0x%x, endpoint=0x%x)",
                (IArg)reserved, (IArg)cb, (IArg)endpoint);

    Assert_isTrue((curInit > 0) , NULL);

    key = GateSwi_enter(module.gateSwi);

    if (reserved == MessageQCopy_ASSIGN_ANY)  {
       /* Search the array for a free slot above reserved: */
       for (i = MessageQCopy_MAX_RESERVED_ENDPOINT + 1;
           (i < MAXMESSAGEQOBJECTS) && (found == FALSE) ; i++) {
           if (module.msgqObjects[i] == NULL) {
            queueIndex = i;
            found = TRUE;
            break;
           }
       }
    }
    else if ((queueIndex = reserved) <= MessageQCopy_MAX_RESERVED_ENDPOINT) {
       if (module.msgqObjects[queueIndex] == NULL) {
           found = TRUE;
       }
    }

    if (found)  {
       obj = Memory_alloc(NULL, sizeof(MessageQCopy_Object), 0, NULL);
       if (obj != NULL) {
           if (cb) {
               /* Store callback and it's arg instead of semaphore: */
               obj->cb = cb;
               obj->arg= arg;
           }
           else {
               /* Allocate a semaphore to signal when messages received: */
               obj->semHandle = Semaphore_create(0, NULL, NULL);

               /* Create our queue of to be received messages: */
               obj->queue = List_create(NULL, NULL);
           }

           /* Store our endpoint, and object: */
           obj->queueId = queueIndex;
           module.msgqObjects[queueIndex] = obj;

           /* See MessageQCopy_unblock() */
           obj->unblocked = FALSE;

           *endpoint    = queueIndex;
           Log_print1(Diags_LIFECYCLE, FXNN": endPt created: %d",
                        (IArg)queueIndex);
       }
    }

    GateSwi_leave(module.gateSwi, key);

    Log_print1(Diags_EXIT, "<-- "FXNN": 0x%x", (IArg)obj);
    return (obj);
}
#undef FXNN

/*
 *  ======== MessageQCopy_delete ========
 */
#define FXNN "MessageQCopy_delete"
Int MessageQCopy_delete(MessageQCopy_Handle *handlePtr)
{
    Int                    status = MessageQCopy_S_SUCCESS;
    MessageQCopy_Object    *obj;
    Queue_elem             *payload;
    IArg                   key;

    Log_print1(Diags_ENTRY, "--> "FXNN": (handlePtr=0x%x)", (IArg)handlePtr);

    Assert_isTrue((curInit > 0) , NULL);

    if (handlePtr && (obj = (MessageQCopy_Object *)(*handlePtr)))  {

       if (obj->cb) {
           obj->cb = NULL;
           obj->arg= NULL;
       }
       else {
           Semaphore_delete(&(obj->semHandle));

           /* Free/discard all queued message buffers: */
           while ((payload = (Queue_elem *)List_get(obj->queue)) != NULL) {
               HeapBuf_free(module.heap, (Ptr)payload, MSGBUFFERSIZE);
           }

           List_delete(&(obj->queue));
       }

       /* Null out our slot: */
       key = GateSwi_enter(module.gateSwi);
       module.msgqObjects[obj->queueId] = NULL;
       GateSwi_leave(module.gateSwi, key);

       Log_print1(Diags_LIFECYCLE, FXNN": endPt deleted: %d",
                        (IArg)obj->queueId);

       /* Now free the obj */
       Memory_free(NULL, obj, sizeof(MessageQCopy_Object));

       *handlePtr = NULL;
    }

    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN

/*
 *  ======== MessageQCopy_recv ========
 */
#define FXNN "MessageQCopy_recv"
Int MessageQCopy_recv(MessageQCopy_Handle handle, Ptr data, UInt16 *len,
                      UInt32 *rplyEndpt, UInt timeout)
{
    Int                 status = MessageQCopy_S_SUCCESS;
    MessageQCopy_Object *obj = (MessageQCopy_Object *)handle;
    Bool                semStatus;
    Queue_elem          *payload;

    Log_print5(Diags_ENTRY, "--> "FXNN": (handle=0x%x, data=0x%x, len=0x%x,"
               "rplyEndpt=0x%x, timeout=%d)", (IArg)handle, (IArg)data,
               (IArg)len, (IArg)rplyEndpt, (IArg)timeout);

    Assert_isTrue((curInit > 0) , NULL);
    /* A callback was set: client should not be calling this fxn! */
    Assert_isTrue((!obj->cb), NULL);

    /* Check vring for pending messages before we block: */
    Swi_post(transport.swiHandle);

    /*  Block until notified. */
    semStatus = Semaphore_pend(obj->semHandle, timeout);

    if (semStatus == FALSE)  {
       status = MessageQCopy_E_TIMEOUT;
       Log_print0(Diags_STATUS, FXNN": Sem pend timeout!");
    }
    else if (obj->unblocked) {
       status = MessageQCopy_E_UNBLOCKED;
    }
    else  {
       payload = (Queue_elem *)List_get(obj->queue);

       if (!payload) {
           System_abort("MessageQCopy_recv: got a NULL payload\n");
       }
    }

    if (status == MessageQCopy_S_SUCCESS)  {
       /* Now, copy payload to client and free our internal msg */
       memcpy(data, payload->data, payload->len);
       *len = payload->len;
       *rplyEndpt = payload->src;

       HeapBuf_free(module.heap, (Ptr)payload,
                    (payload->len + sizeof(Queue_elem)));
    }

    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return (status);
}
#undef FXNN

/*
 *  ======== MessageQCopy_send ========
 */
#define FXNN "MessageQCopy_send"
Int MessageQCopy_send(UInt16 dstProc,
                      UInt32 dstEndpt,
                      UInt32 srcEndpt,
                      Ptr    data,
                      UInt16 len)
{
    Int               status = MessageQCopy_S_SUCCESS;
    MessageQCopy_Object   *obj;
    Int16             token = 0;
    MessageQCopy_Msg  msg;
    Queue_elem        *payload;
    UInt              size;
    IArg              key;
    int length;

    Log_print5(Diags_ENTRY, "--> "FXNN": (dstProc=%d, dstEndpt=%d, "
               "srcEndpt=%d, data=0x%x, len=%d", (IArg)dstProc, (IArg)dstEndpt,
               (IArg)srcEndpt, (IArg)data, (IArg)len);

    Assert_isTrue((curInit > 0) , NULL);

    if (dstProc != MultiProc_self()) {
        /* Send to remote processor: */
        key = GateSwi_enter(module.gateSwi);  // Protect vring structs.
        token = VirtQueue_getAvailBuf(transport.virtQueue_toHost,
                                      (Void **)&msg, &length);
        GateSwi_leave(module.gateSwi, key);

        if (token >= 0) {
            /* Copy the payload and set message header: */
            memcpy(msg->payload, data, len);
            msg->dataLen = len;
            msg->dstAddr = dstEndpt;
            msg->srcAddr = srcEndpt;
            msg->flags = 0;
            msg->reserved = 0;

            key = GateSwi_enter(module.gateSwi);  // Protect vring structs.
            VirtQueue_addUsedBuf(transport.virtQueue_toHost, token, RP_MSG_BUF_SIZE);
            VirtQueue_kick(transport.virtQueue_toHost);
            GateSwi_leave(module.gateSwi, key);
        }
        else {
            status = MessageQCopy_E_FAIL;
            Log_print0(Diags_STATUS, FXNN": getAvailBuf failed!");
        }
    }
    else {
        /* Protect from MessageQCopy_delete */
        key = GateSwi_enter(module.gateSwi);
        obj = module.msgqObjects[dstEndpt];
        GateSwi_leave(module.gateSwi, key);

        if (obj == NULL) {
            Log_print1(Diags_STATUS, FXNN": no object for endpoint: %d",
                   (IArg)dstEndpt);
            status = MessageQCopy_E_NOENDPT;
            return status;
        }

        /* If callback registered, call it: */
        if (obj->cb) {
            Log_print2(Diags_INFO, FXNN": calling callback with data len: "
                            "%d, from: %d\n", len, srcEndpt);
            obj->cb(obj, obj->arg, data, len, srcEndpt);
        }
        else {
            /* else, put on a Message queue on this processor: */

            /* Allocate a buffer to copy the payload: */
            size = len + sizeof(Queue_elem);

            /* HeapBuf_alloc() is non-blocking, so needs protection: */
            key = GateSwi_enter(module.gateSwi);
            payload = (Queue_elem *)HeapBuf_alloc(module.heap, size, 0, NULL);
            GateSwi_leave(module.gateSwi, key);

            if (payload != NULL)  {
                memcpy(payload->data, data, len);
                payload->len = len;
                payload->src = srcEndpt;

                /* Put on the endpoint's queue and signal: */
                List_put(obj->queue, (List_Elem *)payload);
                Semaphore_post(obj->semHandle);
            }
            else {
                status = MessageQCopy_E_MEMORY;
                Log_print0(Diags_STATUS, FXNN": HeapBuf_alloc failed!");
            }
        }
    }

    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return (status);
}
#undef FXNN

/*
 *  ======== MessageQCopy_unblock ========
 */
#define FXNN "MessageQCopy_unblock"
Void MessageQCopy_unblock(MessageQCopy_Handle handle)
{
    MessageQCopy_Object *obj = (MessageQCopy_Object *)handle;

    Log_print1(Diags_ENTRY, "--> "FXNN": (handle=0x%x)", (IArg)handle);

    Assert_isTrue((!obj->cb), NULL);

    /* Set instance to 'unblocked' state, and post */
    obj->unblocked = TRUE;
    Semaphore_post(obj->semHandle);
    Log_print0(Diags_EXIT, "<-- "FXNN);
}
#undef FXNN
