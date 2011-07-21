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
 *  ======== RcmClient.c ========
 */

/* this define must precede inclusion of any xdc header file */
#define Registry_CURDESC ti_rcm_RcmClient__Desc
#define MODULE_NAME "ti.rcm.RcmClient"

#define xdc_runtime_Memory__nolocalnames  /* short name clashes with SysLink */

/* rtsc header files */
#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Diags.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/IHeap.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/Registry.h>
#include <xdc/runtime/Startup.h>
#include <xdc/runtime/knl/GateThread.h>
#include <xdc/runtime/knl/Semaphore.h>
#include <xdc/runtime/knl/SemThread.h>
#include <xdc/runtime/knl/SyncSemThread.h>
#include <xdc/runtime/knl/ISemaphore.h>

/* package header files */
#include <ti/ipc/MessageQ.h>

#if defined(RCM_ti_ipc)
#include <ti/sdo/utils/List.h>

#elif defined(RCM_ti_syslink)
#include <ti/syslink/utils/List.h>

#else
    #error "undefined ipc binding"
#endif

/* local header files */
#include <ti/rcm/RcmServer.h>
#include "RcmTypes.h"
#include "RcmClient.h"

/* recipient list element structure */
typedef struct {
    List_Elem elem;
    UInt16 msgId;
    RcmClient_Message *msg;
    SemThread_Struct event;
} Recipient;

typedef struct RcmClient_Object_tag {
    GateThread_Struct   gate;           // instance gate
    MessageQ_Handle     msgQue;         // message queue
    MessageQ_Handle     errorMsgQue;    // error message queue
    UInt16              heapId;         // heap used for message allocation
    Ptr                 sync;           // synchronizer for message queue
    UInt32              serverMsgQ;     // server message queue
    Bool                cbNotify;       // callback notification
    UInt16              msgId;          // last used message id
    ISemaphore_Handle   mbxLock;        // mailbox lock
    ISemaphore_Handle   queueLock;      // message queue lock
    List_Handle         recipients;     // list of waiting recipients
    List_Handle         newMail;        // list of undelivered messages
} RcmClient_Object;

typedef struct RcmClient_Module_tag {
    String              name;
    Bits16              id;
    Bits16              diagsMask;
    IHeap_Handle        heap;
} RcmClient_Module;

/* private functions */
static
UInt16 RcmClient_genMsgId_P(
        RcmClient_Object *      obj
    );

static inline
RcmClient_Packet *RcmClient_getPacketAddr_P(
        RcmClient_Message *     msg
    );

static inline
RcmClient_Packet *getPacketAddrMsgqMsg(
        MessageQ_Msg            msg
    );

static inline
RcmClient_Packet *getPacketAddrElem(
        List_Elem *             elem
    );

static Int RcmClient_getReturnMsg_P(
        RcmClient_Object *      obj,
        const UInt16            msgId,
        RcmClient_Message **    returnMsg
    );

static
Int RcmClient_Instance_init(
        RcmClient_Object *              obj,
        String                          server,
        const RcmClient_Params *        params
    );

static
Int RcmClient_Instance_finalize(
        RcmClient_Object *      obj
    );


#define RcmClient_Module_heap() (RcmClient_Mod.heap)

/* static objects */
static Int curInit = 0;

static RcmClient_Module RcmClient_Mod = {
    "ti.rcm.RcmClient",     /* name */
    (Bits16)0,                  /* id */
    (Bits16)0,                  /* diagsMask */
    (IHeap_Handle)NULL          /* heap */
};

/* module diags mask */
Registry_Desc Registry_CURDESC;


/*
 *  ======== RcmClient_init ========
 *
 *  This function *must* be serialized by the caller
 */
Void RcmClient_init(Void)
{
    Registry_Result result;


    if (curInit++ != 0) {
        return; /* module already initialized */
    }

    /* register with xdc.runtime to get a diags mask */
    result = Registry_addModule(&Registry_CURDESC, MODULE_NAME);
    Assert_isTrue(result == Registry_SUCCESS, (Assert_Id)NULL);

    /* the size of object and struct must be the same */
    Assert_isTrue(sizeof(RcmClient_Object) == sizeof(RcmClient_Struct), NULL);
}


/*
 *  ======== RcmClient_exit ========
 *
 *  This function *must* be serialized by the caller
 */
Void RcmClient_exit(Void)
{
//  Registry_Result result;


    if (--curInit != 0) {
        return; /* module still in use */
    }

    /* unregister from xdc.runtime */
//  result = Registry_removeModule(MODULE_NAME);
//  Assert_isTrue(result == Registry_SUCCESS, (Assert_Id)NULL);
}


/*
 *  ======== RcmClient_Params_init ========
 */
Void RcmClient_Params_init(RcmClient_Params *params)
{
    params->heapId = RcmClient_INVALIDHEAPID;
    params->callbackNotification = FALSE;
}


/*
 *  ======== RcmClient_create ========
 */
#define FXNN "RcmClient_create"
Int RcmClient_create(String server, const RcmClient_Params *params,
        RcmClient_Handle *handle)
{
    RcmClient_Object *obj;
    Error_Block eb;
    Int status = RcmClient_S_SUCCESS;


    Log_print1(Diags_ENTRY, "--> %s: ()", (IArg)FXNN);

    /* initialize the error block */
    Error_init(&eb);
    *handle = (RcmClient_Handle)NULL;

    /* TODO: add check that params was initialized correctly */

    /* allocate the object */
    obj = (RcmClient_Handle)xdc_runtime_Memory_calloc(RcmClient_Module_heap(),
        sizeof(RcmClient_Object), sizeof(Int), &eb);

    if (NULL == obj) {
        Log_error2(FXNN": out of memory: heap=0x%x, size=%u",
            (IArg)RcmClient_Module_heap(), sizeof(RcmClient_Object));
        status = RcmClient_E_NOMEMORY;
        goto leave;
    }

    Log_print1(Diags_LIFECYCLE, FXNN": instance create: 0x%x", (IArg)obj);

    /* object-specific initialization */
    status = RcmClient_Instance_init(obj, server, params);

    if (status < 0) {
        RcmClient_Instance_finalize(obj);
        xdc_runtime_Memory_free(RcmClient_Module_heap(),
            (Ptr)obj, sizeof(RcmClient_Object));
        goto leave;
    }

    /* success, return opaque pointer */
    *handle = (RcmClient_Handle)obj;


leave:
    Log_print2(Diags_EXIT, "<-- %s: %d", (IArg)FXNN, (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_construct ========
 */
#define FXNN "RcmClient_construct"
Int RcmClient_construct(RcmClient_Struct *structPtr, String server,
    const RcmClient_Params *params)
{
    RcmClient_Object *obj = (RcmClient_Object*)structPtr;
    Int status = RcmClient_S_SUCCESS;


    Log_print1(Diags_ENTRY, "--> %s: ()", (IArg)FXNN);

    /* TODO: add check that params was initialized correctly */

    Log_print1(Diags_LIFECYCLE, FXNN": instance construct: 0x%x", (IArg)obj);

    /* ensure the constructed object is zeroed */
    _memset((Void *)obj, 0, sizeof(RcmClient_Object));

    /* object-specific initialization */
    status = RcmClient_Instance_init(obj, server, params);

    if (status < 0) {
        RcmClient_Instance_finalize(obj);
        goto leave;
    }


leave:
    Log_print2(Diags_EXIT, "<-- %s: %d", (IArg)FXNN, (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_Instance_init ========
 */
#define FXNN "RcmClient_Instance_init"
Int RcmClient_Instance_init(RcmClient_Object *obj, String server,
        const RcmClient_Params *params)
{
    Error_Block eb;
    MessageQ_Params mqParams;
    SyncSemThread_Params syncParams;
    SemThread_Params semParams;
    SemThread_Handle semHndl;
    List_Params listP;
    Int rval;
    Int status = RcmClient_S_SUCCESS;


    Log_print2(Diags_ENTRY, "--> %s: (obj=0x%x)", (IArg)FXNN, (IArg)obj);

    /* must initialize error block */
    Error_init(&eb);

    /* initialize instance data */
    obj->msgId = 0xFFFF;
    obj->sync = NULL;
    obj->serverMsgQ = MessageQ_INVALIDMESSAGEQ;
    obj->msgQue = NULL;
    obj->errorMsgQue = NULL;
    obj->mbxLock = NULL;
    obj->queueLock = NULL;
    obj->recipients = NULL;
    obj->newMail = NULL;

    /* create the instance gate */
    GateThread_construct(&obj->gate, NULL, &eb);

    if (Error_check(&eb)) {
        Log_error0(FXNN": could not create gate object");
        status = RcmClient_E_FAIL;
        goto leave;
    }

    /* create a synchronizer for the message queue */
    SyncSemThread_Params_init(&syncParams);
    obj->sync = SyncSemThread_create(&syncParams, &eb);

    if (Error_check(&eb)) {
        status = RcmClient_E_FAIL;
        goto leave;
    }

    /* create the message queue for return messages */
    MessageQ_Params_init(&mqParams);
    obj->msgQue = MessageQ_create(NULL, &mqParams);

    if (obj->msgQue == NULL) {
        Log_error0(FXNN": could not create return message queue");
        status = RcmClient_E_MSGQCREATEFAILED;
        goto leave;
    }

    /* create the message queue for error messages */
    MessageQ_Params_init(&mqParams);
    obj->errorMsgQue = MessageQ_create(NULL, &mqParams);

    if (NULL == obj->errorMsgQue) {
        Log_error0(FXNN": could not create error message queue");
        status = RcmClient_E_MSGQCREATEFAILED;
        goto leave;
    }

    /* locate server message queue */
    rval = MessageQ_open(server, (MessageQ_QueueId *)(&obj->serverMsgQ));

    if (MessageQ_E_NOTFOUND == rval) {
        Log_error1(FXNN": given server not found, server=0x%x", (IArg)server);
        status = RcmClient_E_SERVERNOTFOUND;
        goto leave;
    }
    else if (status < 0) {
        Log_error1(FXNN": could not open server message queue, server=0x%x",
            (IArg)server);
        status = RcmClient_E_MSGQOPENFAILED;
        goto leave;
    }

    /* create callback server */
    if ((obj->cbNotify = params->callbackNotification)) {
        /* TODO create callback server thread */
        /* make sure to free resources acquired by thread */
        Error_raise(&eb, Error_E_generic, "Not Implemented", 0);
        goto leave;
    }

    /* register the heapId used for message allocation */
    if ((obj->heapId = params->heapId) == RcmClient_INVALIDHEAPID) {
        Log_error0(FXNN": must specify a heap id in create params");
        status = RcmClient_E_INVALIDHEAPID;
        goto leave;
    }

    /* create the mailbox lock */
    SemThread_Params_init(&semParams);
    semHndl = SemThread_create(1, &semParams, &eb);
    if (Error_check(&eb)) {
        status = RcmClient_E_FAIL;
        goto leave;
    }
    obj->mbxLock = SemThread_Handle_upCast(semHndl);

    /* create the message queue lock */
    SemThread_Params_init(&semParams);
    semHndl = SemThread_create(1, &semParams, &eb);
    if (Error_check(&eb)) {
        status = RcmClient_E_FAIL;
        goto leave;
    }
    obj->queueLock = SemThread_Handle_upCast(semHndl);

    /* create the return message recipient list */
#if defined(RCM_ti_ipc)
    List_Params_init(&listP);
    obj->recipients = List_create(&listP, &eb);

    if (Error_check(&eb)) {
        Log_error0(FXNN": could not create list object");
        status = RcmClient_E_LISTCREATEFAILED;
        goto leave;
    }
#elif defined(RCM_ti_syslink)
    List_Params_init(&listP);
    obj->recipients = List_create(&listP, NULL);

    if (NULL == obj->recipients) {
        Log_error0(FXNN": could not create list object");
        status = RcmClient_E_LISTCREATEFAILED;
        goto leave;
    }
#endif

    /* create list of undelivered messages (new mail) */
#if defined(RCM_ti_ipc)
    List_Params_init(&listP);
    obj->newMail = List_create(&listP, &eb);

    if (Error_check(&eb)) {
        Log_error0(FXNN": could not create list object");
        status = RcmClient_E_LISTCREATEFAILED;
        goto leave;
    }
#elif defined(RCM_ti_syslink)
    List_Params_init(&listP);
    obj->newMail = List_create(&listP, NULL);

    if (NULL == obj->newMail) {
        Log_error0(FXNN": could not create list object");
        status = RcmClient_E_LISTCREATEFAILED;
        goto leave;
    }
#endif

leave:
    Log_print2(Diags_EXIT, "<-- %s: %d", (IArg)FXNN, (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_delete ========
 */
#define FXNN "RcmClient_delete"
Int RcmClient_delete(RcmClient_Handle *handlePtr)
{
    RcmClient_Object *obj = (RcmClient_Object *)(*handlePtr);
    Int status = RcmClient_S_SUCCESS;


    Log_print1(Diags_ENTRY, "--> %s: ()", (IArg)FXNN);

    /* finalize the object */
    status = RcmClient_Instance_finalize(obj);

    /* free the object memory */
    Log_print1(Diags_LIFECYCLE, FXNN": instance delete: 0x%x", (IArg)obj);

    xdc_runtime_Memory_free(RcmClient_Module_heap(),
        (Ptr)obj, sizeof(RcmClient_Object));
    *handlePtr = NULL;

    Log_print2(Diags_EXIT, "<-- %s: %d", (IArg)FXNN, (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_destruct ========
 */
#define FXNN "RcmClient_destruct"
Int RcmClient_destruct(RcmClient_Struct *structPtr)
{
    RcmClient_Object *obj = (RcmClient_Object *)(structPtr);
    Int status = RcmClient_S_SUCCESS;


    Log_print1(Diags_ENTRY, "--> %s: ()", (IArg)FXNN);
    Log_print1(Diags_LIFECYCLE, FXNN": instance destruct: 0x%x", (IArg)obj);

    /* finalize the object */
    status = RcmClient_Instance_finalize(obj);

    Log_print2(Diags_EXIT, "<-- %s: %d", (IArg)FXNN, (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_Instance_finalize ========
 */
#define FXNN "RcmClient_Instance_finalize"
Int RcmClient_Instance_finalize(RcmClient_Object *obj)
{
    SemThread_Handle semH;
    Int status = RcmClient_S_SUCCESS;


    Log_print1(Diags_ENTRY, "--> "FXNN": (obj=0x%x)", (IArg)obj);

    if (NULL != obj->newMail) {
        List_delete(&obj->newMail);
    }

    if (NULL != obj->recipients) {
        List_delete(&obj->recipients);
    }

    if (NULL != obj->queueLock) {
        semH = SemThread_Handle_downCast(obj->queueLock);
        SemThread_delete(&semH);
        obj->queueLock = NULL;
    }

    if (NULL != obj->mbxLock) {
        semH = SemThread_Handle_downCast(obj->mbxLock);
        SemThread_delete(&semH);
        obj->mbxLock = NULL;
    }

    if (MessageQ_INVALIDMESSAGEQ != obj->serverMsgQ) {
        MessageQ_close((MessageQ_QueueId *)(&obj->serverMsgQ));
    }

    if (NULL != obj->errorMsgQue) {
        MessageQ_delete(&obj->errorMsgQue);
    }

    if (NULL != obj->msgQue) {
        MessageQ_delete(&obj->msgQue);
    }

    if (NULL != obj->sync) {
        SyncSemThread_delete((SyncSemThread_Handle *)(&obj->sync));
    }

    /* destruct the instance gate */
    GateThread_destruct(&obj->gate);

    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_acquireJobId ========
 */
#define FXNN "RcmClient_acquireJobId"
Int RcmClient_acquireJobId(RcmClient_Object *obj, UInt16 *jobIdPtr)
{
    RcmClient_Message *msg;
    RcmClient_Packet *packet;
    MessageQ_Msg msgqMsg;
    UInt16 msgId;
    Int rval;
    UInt16 serverStatus;
    Int status = RcmClient_S_SUCCESS;


    Log_print2(Diags_ENTRY,
        "--> "FXNN": (obj=0x%x, jobIdPtr=0x%x)", (IArg)obj, (IArg)jobIdPtr);

    /* allocate a message */
    status = RcmClient_alloc(obj, sizeof(UInt16), &msg);

    if (status < 0) {
        goto leave;
    }

    /* classify this message */
    packet = RcmClient_getPacketAddr_P(msg);
    packet->desc |= RcmClient_Desc_JOB_ACQ << RcmClient_Desc_TYPE_SHIFT;
    msgId = packet->msgId;

    /* set the return address to this instance's message queue */
    msgqMsg = (MessageQ_Msg)packet;
    MessageQ_setReplyQueue(obj->msgQue, msgqMsg);

    /* send the message to the server */
    rval = MessageQ_put((MessageQ_QueueId)obj->serverMsgQ, msgqMsg);

    if (rval < 0) {
        Log_error0(FXNN": unable to the send message to the server");
        status = RcmClient_E_FAIL;
        goto leave;
    }

    /* get the return message from the server */
    status = RcmClient_getReturnMsg_P(obj, msgId, &msg);

    if (status < 0) {
        goto leave;
    }

    /* check message status for error */
    packet = RcmClient_getPacketAddr_P(msg);
    serverStatus = (RcmClient_Desc_TYPE_MASK & packet->desc) >>
            RcmClient_Desc_TYPE_SHIFT;

    switch (serverStatus) {
        case RcmServer_Status_SUCCESS:
            break;

        default:
            Log_error1(FXNN": server returned error %d", (IArg)serverStatus);
            status = RcmClient_E_SERVERERROR;
            goto leave;
    }

    /* extract return value */
    *jobIdPtr = (UInt16)(msg->data[0]);


leave:
    if (msg != NULL) {
        RcmClient_free(obj, msg);
    }
    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_addSymbol ========
 */
#define FXNN "RcmClient_addSymbol"
Int RcmClient_addSymbol(RcmClient_Object *obj, String name,
        Fxn address, UInt32 *index)
{
//  Int status = RcmClient_S_SUCCESS;
    Int status = RcmClient_E_FAIL;


    Log_print2(Diags_ENTRY, "--> %s: (obj=0x%x)", (IArg)FXNN, (IArg)obj);

    /* TODO */

    Log_print2(Diags_EXIT, "<-- %s: %d", (IArg)FXNN, (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_alloc ========
 */
#define FXNN "RcmClient_alloc"
Int RcmClient_alloc(RcmClient_Object *obj, UInt32 dataSize,
    RcmClient_Message **message)
{
    SizeT totalSize;
    RcmClient_Packet *packet;
    Int status = RcmClient_S_SUCCESS;


    Log_print3(Diags_ENTRY,
        "--> RcmClient_alloc: obj: 0x%x, dataSize: %d, message: 0x%x",
        (IArg)obj, (IArg)dataSize, (IArg)message);

    /* ensure minimum size of UInt32[1] */
    dataSize  = (dataSize < sizeof(UInt32) ? sizeof(UInt32) : dataSize);

    /* total memory size (in chars) needed for headers and payload */
    totalSize = sizeof(RcmClient_Packet) - sizeof(UInt32) + dataSize;

    /* allocate the message */
    packet = (RcmClient_Packet*)MessageQ_alloc(obj->heapId, totalSize);

    if (NULL == packet) {
        Log_error1(FXNN": could not allocate message, size = %u",
            (IArg)totalSize);
        status = RcmClient_E_MSGALLOCFAILED;
        goto leave;
    }

    /* Log_info() */
    Log_print2(Diags_INFO,
        FXNN": RcmMessage allocated: addr=0x%x, size=%u (total size)",
        (IArg)packet, (IArg)totalSize);

    /* initialize the packet structure */
    packet->desc = 0;
    packet->msgId = RcmClient_genMsgId_P(obj);
    packet->message.poolId = RcmClient_DEFAULTPOOLID;
    packet->message.jobId = RcmClient_DISCRETEJOBID;
    packet->message.fxnIdx = RcmClient_INVALIDFXNIDX;
    packet->message.result = 0;
    packet->message.dataSize = dataSize;

    /* set message pointer to start of the message struct */
    *message = (RcmClient_Message *)(&(packet->message));


leave:
    Log_print2(Diags_EXIT, "<-- %s: %d", (IArg)FXNN, (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_checkForError ========
 *  If success, no message will be returned. If error message returned,
 *  then function return value will always be < 0.
 *
 *  status      msgPtr
 *  --------------------
 *  success     null (always)
 *  < 0         null (internal error)
 *  < 0         msg  (error message returned)
 */
#define FXNN "RcmClient_checkForError"
Int RcmClient_checkForError(RcmClient_Object *obj, RcmClient_Message **rtnMsg)
{
    RcmClient_Message *rcmMsg;
    RcmClient_Packet *packet;
    MessageQ_Msg msgqMsg;
    UInt16 serverStatus;
    Int rval;
    Int status = RcmClient_S_SUCCESS;


    Log_print1(Diags_ENTRY,
        "--> "FXNN": (obj=0x%x, rtnMsg=0x%x)", (IArg)rtnMsg);

    *rtnMsg = NULL;

    /* get error message if available (non-blocking) */
    rval = MessageQ_get(obj->errorMsgQue, &msgqMsg, 0);

    if ((MessageQ_E_TIMEOUT != rval) && (rval < 0)) {
        Log_error1(FXNN": MessageQ_get() returned error %d", (IArg)rval);
        status = RcmClient_E_IPCERROR;
        goto leave;
    }
    else if (msgqMsg == NULL) {
        goto leave;
    }

    /* received an error message */
    packet = getPacketAddrMsgqMsg(msgqMsg);
    rcmMsg = &packet->message;
    *rtnMsg = rcmMsg;

    /* check the server status stored in the packet header */
    serverStatus = (RcmClient_Desc_TYPE_MASK & packet->desc) >>
            RcmClient_Desc_TYPE_SHIFT;

    switch (serverStatus) {
        case RcmServer_Status_JobNotFound:
            Log_error1(FXNN": job id=%d not found", (IArg)rcmMsg->jobId);
            break;

        case RcmServer_Status_PoolNotFound:
            Log_error1(FXNN": pool id=0x%x not found", (IArg)rcmMsg->poolId);
            break;

        case RcmServer_Status_INVALID_FXN:
            Log_error1(FXNN": invalid function index: 0x%x",
                (IArg)rcmMsg->fxnIdx);
            status = RcmClient_E_INVALIDFXNIDX;
            break;

        case RcmServer_Status_MSG_FXN_ERR:
            Log_error1(FXNN": message function error %d", (IArg)rcmMsg->result);
            status = RcmClient_E_MSGFXNERROR;
            break;

        default:
            Log_error1(FXNN": server returned error %d", (IArg)serverStatus);
            status = RcmClient_E_SERVERERROR;
            goto leave;
    }


leave:
    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_exec ========
 */
#define FXNN "RcmClient_exec"
Int RcmClient_exec(RcmClient_Object *obj,
        RcmClient_Message *cmdMsg, RcmClient_Message **returnMsg)
{
    RcmClient_Packet *packet;
    RcmClient_Message *rtnMsg;
    MessageQ_Msg msgqMsg;
    UInt16 msgId;
    UInt16 serverStatus;
    Int rval;
    Int status = RcmClient_S_SUCCESS;


    Log_print2(Diags_ENTRY,
        "--> "FXNN": (cmdMsg=0x%x, rtnMsg=0x%x)", (IArg)obj, (IArg)returnMsg);

    /* classify this message */
    packet = RcmClient_getPacketAddr_P(cmdMsg);
    packet->desc |= RcmClient_Desc_RCM_MSG << RcmClient_Desc_TYPE_SHIFT;
    msgId = packet->msgId;

    /* set the return address to this instance's message queue */
    msgqMsg = (MessageQ_Msg)packet;
    MessageQ_setReplyQueue(obj->msgQue, msgqMsg);

    /* send the message to the server */
    status = MessageQ_put((MessageQ_QueueId)obj->serverMsgQ, msgqMsg);

    if (status < 0) {
        Log_error0(FXNN": unable to the send message to the server");
        status = RcmClient_E_EXECFAILED;
        goto leave;
    }

    /* get the return message from the server */
    rval = RcmClient_getReturnMsg_P(obj, msgId, &rtnMsg);

    if (rval < 0) {
        *returnMsg = NULL;
        status = rval;
        goto leave;
    }
    *returnMsg = rtnMsg;

    /* check the server's status stored in the packet header */
    packet = RcmClient_getPacketAddr_P(rtnMsg);
    serverStatus = (RcmClient_Desc_TYPE_MASK & packet->desc) >>
            RcmClient_Desc_TYPE_SHIFT;

    switch (serverStatus) {
        case RcmServer_Status_SUCCESS:
            break;

        case RcmServer_Status_INVALID_FXN:
            Log_error1(FXNN": invalid function index: 0x%x",
                (IArg)rtnMsg->fxnIdx);
            status = RcmClient_E_INVALIDFXNIDX;
            goto leave;

        case RcmServer_Status_MSG_FXN_ERR:
            Log_error1(FXNN": message function error %d", (IArg)rtnMsg->result);
            status = RcmClient_E_MSGFXNERROR;
            goto leave;

        default:
            Log_error1(FXNN": server returned error %d", (IArg)serverStatus);
            status = RcmClient_E_SERVERERROR;
            goto leave;
    }

leave:
    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_execAsync ========
 */
#define FXNN "RcmClient_execAsync"
Int RcmClient_execAsync(RcmClient_Object *obj, RcmClient_Message *cmdMsg,
    RcmClient_CallbackFxn callback, Ptr appData)
{
    RcmClient_Packet *packet;
    MessageQ_Msg msgqMsg;
    Int rval;
    Int status = RcmClient_S_SUCCESS;


    Log_print3(Diags_ENTRY,
        "--> %s: 0x%x, 0x%x", (IArg)FXNN, (IArg)obj, (IArg)cmdMsg);

    /* cannot use this function if callback notification is false */
    if (!obj->cbNotify) {
        Log_error0(FXNN": asynchronous notification not enabled");
        status = RcmClient_E_EXECASYNCNOTENABLED;
        goto leave;
    }

    /* classify this message */
    packet = RcmClient_getPacketAddr_P(cmdMsg);
    packet->desc |= RcmClient_Desc_RCM_MSG << RcmClient_Desc_TYPE_SHIFT;

    /* set the return address to this instance's message queue */
    msgqMsg = (MessageQ_Msg)packet;
    MessageQ_setReplyQueue(obj->msgQue, msgqMsg);

    /* send the message to the server */
    rval = MessageQ_put((MessageQ_QueueId)obj->serverMsgQ, msgqMsg);

    if (rval < 0) {
        Log_error0(FXNN": unable to the send message to the server");
        status = RcmClient_E_EXECFAILED;
        goto leave;
    }

    /* TODO finish this function */

leave:
    Log_print2(Diags_EXIT, "<-- %s: %d", (IArg)FXNN, (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_execCmd ========
 */
#define FXNN "RcmClient_execCmd"
Int RcmClient_execCmd(RcmClient_Object *obj, RcmClient_Message *msg)
{
    RcmClient_Packet *packet;
    MessageQ_Msg msgqMsg;
    Int rval;
    Int status = RcmClient_S_SUCCESS;


    Log_print2(Diags_ENTRY | Diags_INFO,
        "--> "FXNN": (obj=0x%x, msg=0x%x)", (IArg)obj, (IArg)msg);

    /* classify this message */
    packet = RcmClient_getPacketAddr_P(msg);
    packet->desc |= RcmClient_Desc_CMD << RcmClient_Desc_TYPE_SHIFT;

    /* set the return address to the error message queue */
    msgqMsg = (MessageQ_Msg)packet;
    MessageQ_setReplyQueue(obj->errorMsgQue, msgqMsg);

    /* send the message to the server */
    rval = MessageQ_put((MessageQ_QueueId)obj->serverMsgQ, msgqMsg);

    if (rval < 0) {
        Log_error0(FXNN": unable to send message to server");
        status = RcmClient_E_IPCERROR;
    }

    Log_print1(Diags_EXIT | Diags_INFO, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_execDpc ========
 */
#define FXNN "RcmClient_execDpc"
Int RcmClient_execDpc(RcmClient_Object *obj,
    RcmClient_Message *cmdMsg, RcmClient_Message **returnMsg)
{
    RcmClient_Packet *packet;
    RcmClient_Message *rtnMsg;
    MessageQ_Msg msgqMsg;
    UInt16 msgId;
    Int rval;
    Int status = RcmClient_S_SUCCESS;


    Log_print3(Diags_ENTRY,
        "--> %s: 0x%x, 0x%x", (IArg)FXNN, (IArg)obj, (IArg)returnMsg);

    /* classify this message */
    packet = RcmClient_getPacketAddr_P(cmdMsg);
    packet->desc |= RcmClient_Desc_DPC << RcmClient_Desc_TYPE_SHIFT;
    msgId = packet->msgId;

    /* set the return address to this instance's message queue */
    msgqMsg = (MessageQ_Msg)packet;
    MessageQ_setReplyQueue(obj->msgQue, msgqMsg);

    /* send the message to the server */
    rval = MessageQ_put((MessageQ_QueueId)obj->serverMsgQ, msgqMsg);

    if (rval < 0) {
        /* Log_error() */
        Log_error0(FXNN": unable to the send message to the server");
        status = RcmClient_E_EXECFAILED;
        goto leave;
    }

    /* get the return message from the server */
    rval = RcmClient_getReturnMsg_P(obj, msgId, &rtnMsg);

    if (rval < 0) {
        *returnMsg = NULL;
        status = rval;
        goto leave;
    }
    *returnMsg = rtnMsg;


leave:
    Log_print2(Diags_EXIT, "<-- %s: %d", (IArg)FXNN, (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_execNoWait ========
 */
#define FXNN "RcmClient_execNoWait"
Int RcmClient_execNoWait(RcmClient_Object *obj, RcmClient_Message *cmdMsg,
    UInt16 *msgId)
{
    RcmClient_Packet *packet;
    MessageQ_Msg msgqMsg;
    Int rval;
    Int status = RcmClient_S_SUCCESS;


    Log_print4(Diags_ENTRY,
        "--> %s: 0x%x, 0x%x, 0x%x",
        (IArg)FXNN, (IArg)obj, (IArg)cmdMsg, (IArg)msgId);

    /* classify this message */
    packet = RcmClient_getPacketAddr_P(cmdMsg);
    packet->desc |= RcmClient_Desc_RCM_MSG << RcmClient_Desc_TYPE_SHIFT;
    *msgId = packet->msgId;

    /* set the return address to this instance's message queue */
    msgqMsg = (MessageQ_Msg)packet;
    MessageQ_setReplyQueue(obj->msgQue, msgqMsg);

    /* send the message to the server */
    Log_print1(Diags_ANALYSIS, "%s: >>> MessageQ_put", (IArg)FXNN);
    rval = MessageQ_put((MessageQ_QueueId)obj->serverMsgQ, msgqMsg);
    Log_print1(Diags_ANALYSIS, "%s: <<< MessageQ_put", (IArg)FXNN);

    if (rval < 0) {
        *msgId = RcmClient_INVALIDMSGID;
        Log_error0(FXNN": unable to the send message to the server");
        status = RcmClient_E_EXECFAILED;
        goto leave;
    }

leave:
    Log_print2(Diags_EXIT, "<-- %s: %d", (IArg)FXNN, (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_free ========
 */
#define FXNN "RcmClient_free"
Int RcmClient_free(RcmClient_Object *obj, RcmClient_Message *msg)
{
    Int rval;
    MessageQ_Msg msgqMsg;
    Int status = RcmClient_S_SUCCESS;


    Log_print2(Diags_ENTRY,
        "--> "FXNN": (obj=0x%x, msg=0x%x)", (IArg)obj, (IArg)msg);

    msgqMsg = (MessageQ_Msg)RcmClient_getPacketAddr_P(msg);
    rval = MessageQ_free(msgqMsg);

    if (rval < 0) {
        Log_error1(FXNN": ipc returned error %d", (IArg)rval);
        status = RcmClient_E_IPCERROR;
        goto leave;
    }


leave:
    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_getSymbolIndex ========
 */
#define FXNN "RcmClient_getSymbolIndex"
Int RcmClient_getSymbolIndex(RcmClient_Object *obj, String name, UInt32 *index)
{
    SizeT len;
    RcmClient_Packet *packet;
    UInt16 msgId;
    MessageQ_Msg msgqMsg;
    Int rval;
    UInt16 serverStatus;
    RcmClient_Message *rcmMsg = NULL;
    Int status = RcmClient_S_SUCCESS;


    Log_print3(Diags_ENTRY,
        "--> "FXNN": (obj=0x%x, name=0x%x, index=0x%x",
        (IArg)obj, (IArg)name, (IArg)index);

    /* allocate a message */
    len = _strlen(name) + 1;
    rval = RcmClient_alloc(obj, len, &rcmMsg);

    if (rval < 0) {
        status = rval;
        goto leave;
    }

    /* copy the function name into the message payload */
    rcmMsg->dataSize = len;  //TODO this is not proper!
    _strcpy((Char *)rcmMsg->data, name);

    /* classify this message */
    packet = RcmClient_getPacketAddr_P(rcmMsg);
    packet->desc |= RcmClient_Desc_SYM_IDX << RcmClient_Desc_TYPE_SHIFT;
    msgId = packet->msgId;

    /* set the return address to this instance's message queue */
    msgqMsg = (MessageQ_Msg)packet;
    MessageQ_setReplyQueue(obj->msgQue, msgqMsg);

    /* send the message to the server */
    rval = MessageQ_put((MessageQ_QueueId)obj->serverMsgQ, msgqMsg);

    if (rval < 0) {
        Log_error0(FXNN": unable to the send message to the server");
        status = RcmClient_E_EXECFAILED;
        goto leave;
    }

    /* get the return message from the server */
    rval = RcmClient_getReturnMsg_P(obj, msgId, &rcmMsg);

    if (rval < 0) {
        status = rval;
        goto leave;
    }

    /* check message status for error */
    packet = RcmClient_getPacketAddr_P(rcmMsg);
    serverStatus = (RcmClient_Desc_TYPE_MASK & packet->desc) >>
            RcmClient_Desc_TYPE_SHIFT;

    switch (serverStatus) {
        case RcmServer_Status_SUCCESS:
            break;

        case RcmServer_Status_SYMBOL_NOT_FOUND:
            Log_error1(FXNN": symbol not found, name=0x%x", (IArg)name);
            status = RcmClient_E_SYMBOLNOTFOUND;
            goto leave;

        default:
            Log_error1(FXNN": server returned error %d", (IArg)serverStatus);
            status = RcmClient_E_SERVERERROR;
            goto leave;
    }

    /* extract return value */
    *index = rcmMsg->data[0];


leave:
    if (rcmMsg != NULL) {
        RcmClient_free(obj, rcmMsg);
    }
    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_releaseJobId ========
 */
#define FXNN "RcmClient_releaseJobId"
Int RcmClient_releaseJobId(RcmClient_Object *obj, UInt16 jobId)
{
    RcmClient_Message *msg;
    RcmClient_Packet *packet;
    MessageQ_Msg msgqMsg;
    UInt16 msgId;
    Int rval;
    UInt16 serverStatus;
    Int status = RcmClient_S_SUCCESS;


    Log_print2(Diags_ENTRY,
        "--> "FXNN": (obj=0x%x, jobId=0x%x)", (IArg)obj, (IArg)jobId);

    /* allocate a message */
    status = RcmClient_alloc(obj, sizeof(UInt16), &msg);

    if (status < 0) {
        goto leave;
    }

    /* classify this message */
    packet = RcmClient_getPacketAddr_P(msg);
    packet->desc |= RcmClient_Desc_JOB_REL << RcmClient_Desc_TYPE_SHIFT;
    msgId = packet->msgId;

    /* set the return address to this instance's message queue */
    msgqMsg = (MessageQ_Msg)packet;
    MessageQ_setReplyQueue(obj->msgQue, msgqMsg);

    /* marshal the job id into the message payload */
    *(UInt16 *)(&msg->data[0]) = jobId;

    /* send the message to the server */
    rval = MessageQ_put((MessageQ_QueueId)obj->serverMsgQ, msgqMsg);

    if (rval < 0) {
        Log_error0(FXNN": unable to the send message to the server");
        status = RcmClient_E_FAIL;
        goto leave;
    }

    /* get the return message from the server */
    status = RcmClient_getReturnMsg_P(obj, msgId, &msg);

    if (status < 0) {
        goto leave;
    }

    /* check message status for error */
    packet = RcmClient_getPacketAddr_P(msg);
    serverStatus = (RcmClient_Desc_TYPE_MASK & packet->desc) >>
            RcmClient_Desc_TYPE_SHIFT;

    switch (serverStatus) {
        case RcmServer_Status_SUCCESS:
            break;

        case RcmServer_Status_JobNotFound:
            Log_error1(FXNN": jobId=%d not found on server", (IArg)jobId);
            status = RcmClient_E_JOBIDNOTFOUND;
            break;

        default:
            Log_error1(FXNN": server returned error %d", (IArg)serverStatus);
            status = RcmClient_E_SERVERERROR;
            break;
    }


leave:
    if (msg != NULL) {
        RcmClient_free(obj, msg);
    }
    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);

}
#undef FXNN

/*
 *  ======== RcmClient_removeSymbol ========
 */
#define FXNN "RcmClient_removeSymbol"
Int RcmClient_removeSymbol(RcmClient_Object *obj, String name)
{
//  Int status = RcmClient_S_SUCCESS;
    Int status = RcmClient_E_FAIL;


    Log_print2(Diags_ENTRY, "--> %s: (obj=0x%x)", (IArg)FXNN, (IArg)obj);

    /* TODO */

    Log_print2(Diags_EXIT, "<-- %s: %d", (IArg)FXNN, (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_waitUntilDone ========
 */
#define FXNN "RcmClient_waitUntilDone"
Int RcmClient_waitUntilDone(RcmClient_Object *obj,
    UInt16 msgId, RcmClient_Message **returnMsg)
{
    RcmClient_Message *rtnMsg;
    Int rval;
    Int status = RcmClient_S_SUCCESS;


    Log_print4(Diags_ENTRY, "--> %s: (obj=0x%x, msgId=%d, msg=0x%x)",
        (IArg)FXNN, (IArg)obj, (IArg)msgId, (IArg)returnMsg);

    /* get the return message from the server */
    rval = RcmClient_getReturnMsg_P(obj, msgId, &rtnMsg);

    if (rval < 0) {
        *returnMsg = NULL;
        status = rval;
        goto leave;
    }
    *returnMsg = rtnMsg;

leave:
    Log_print2(Diags_EXIT, "<-- %s: %d", (IArg)FXNN, (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_genMsgId_P ========
 */
UInt16 RcmClient_genMsgId_P(RcmClient_Object *obj)
{
    GateThread_Handle gateH;
    IArg key;
    UInt16 msgId;


    gateH = GateThread_handle(&obj->gate);
    key = GateThread_enter(gateH);

    msgId = (obj->msgId == 0xFFFF ? obj->msgId = 1 : ++(obj->msgId));

    GateThread_leave(gateH, key);

    return(msgId);
}


/*
 *  ======== RcmClient_getReturnMsg_P ========
 *  A thread safe algorithm for message delivery
 *
 *  This function is called to pickup a specified return message from
 *  the server. Messages are taken from the queue and either delivered
 *  to the mailbox if not the specified message or returned to the caller.
 *  The calling thread might take the role of mailman or simply wait
 *  on a list of recipients.
 *
 *  This algorithm guarantees that a waiting recipient is released as soon
 *  as his message arrives. All new unclaimed mail is placed in the mailbox
 *  until the recipient arrives to collect it. The messages can arrive in
 *  any order and will be processed as they arrive. Message delivery is never
 *  stalled waiting on an absent recipient.
 */
#define FXNN "RcmClient_getReturnMsg_P"
Int RcmClient_getReturnMsg_P(RcmClient_Object *obj, const UInt16 msgId,
    RcmClient_Message **returnMsg)
{
    List_Elem *elem;
    Recipient *recipient;
    RcmClient_Packet *packet;
    Bool messageDelivered;
    MessageQ_Msg msgqMsg = NULL;
    Bool messageFound = FALSE;
    Int queueLockAcquired = 0;
    Error_Block eb;
    Int rval;
    Int status = RcmClient_S_SUCCESS;


    Log_print3(Diags_ENTRY,
        "--> "FXNN": (obj=0x%x, msgId=%d, returnMsgPtr=0x%x",
        (IArg)obj, (IArg)msgId, (IArg)returnMsg);

    Error_init(&eb);
    *returnMsg = NULL;

    /* keep trying until message found */
    while (!messageFound) {

        /* acquire the mailbox lock */
        Semaphore_pend(obj->mbxLock, Semaphore_FOREVER, &eb);
        if (Error_check(&eb)) {
            /* TODO */
            goto leave;
        }

        /* search new mail list for message */
        elem = NULL;
        while ((elem = List_next(obj->newMail, elem)) != NULL) {
            packet = getPacketAddrElem(elem);
            if (msgId == packet->msgId) {
                List_remove(obj->newMail, elem);
                *returnMsg = &packet->message;
                messageFound = TRUE;
                break;
            }
        }

        if (messageFound) {
            /* release the mailbox lock */
            Semaphore_post(obj->mbxLock, &eb);
            if (Error_check(&eb)) {
                /* TODO */
            }
        }
        else {
            /* attempt the message queue lock */
            queueLockAcquired = Semaphore_pend(obj->queueLock, 0, &eb);
            if (Error_check(&eb)) {
                /* TODO */
                goto leave;
            }

            if (1 == queueLockAcquired) {
                /*
                 * mailman role
                 */

                /* deliver new mail until message found */
                while (!messageFound) {

                    /* get message from queue if available (non-blocking) */
                    if (NULL == msgqMsg) {
                        rval = MessageQ_get(obj->msgQue, &msgqMsg, 0);

                        if ((MessageQ_E_TIMEOUT != rval) && (rval < 0)) {
                            Log_error0(FXNN": lost return message");
                            status = RcmClient_E_LOSTMSG;
                            goto leave;
                        }
                        Log_print0(Diags_INFO, FXNN": return message received");
                    }

                    while (NULL != msgqMsg) {

                        /* check if message found */
                        packet = getPacketAddrMsgqMsg(msgqMsg);
                        messageFound = (msgId == packet->msgId);

                        if (messageFound) {
                            *returnMsg = &packet->message;

                            /* search wait list for new mailman */
                            elem = NULL;
                            while ((elem =
                                List_next(obj->recipients, elem)) != NULL) {
                                recipient = (Recipient *)elem;
                                if (NULL == recipient->msg) {
                                    /* signal recipient's event */
                                    SemThread_post(SemThread_handle(
                                        &recipient->event), &eb);
                                    break;
                                }
                            }

                            /* release the message queue lock */
                            Semaphore_post(obj->queueLock, &eb);
                            if (Error_check(&eb)) {
                                /* TODO */
                            }

                            /* release the mailbox lock */
                            Semaphore_post(obj->mbxLock, &eb);
                            if (Error_check(&eb)) {
                                /* TODO */
                            }

                            break;
                        }
                        else {
                            /*
                             * deliver message to mailbox
                             */

                            /* search recipient list for message owner */
                            elem = NULL;
                            messageDelivered = FALSE;
                            while ((elem =
                                List_next(obj->recipients, elem)) != NULL) {
                                recipient = (Recipient *)elem;
                                if (recipient->msgId == packet->msgId) {
                                    recipient->msg = &packet->message;
                                    /* signal the recipient's event */
                                    SemThread_post(SemThread_handle(
                                        &recipient->event), &eb);
                                    messageDelivered = TRUE;
                                    break;
                                }
                            }

                            /* add undelivered message to new mail list */
                            if (!messageDelivered) {
                                /* use the elem in the MessageQ header */
                                elem = (List_Elem *)&packet->msgqHeader;
                                List_put(obj->newMail, elem);
                            }
                        }

                        /* get next message from queue if available */
                        rval = MessageQ_get(obj->msgQue, &msgqMsg, 0);

                        if ((MessageQ_E_TIMEOUT != rval) && (rval < 0)) {
                            Log_error0(FXNN": lost return message");
                            status = RcmClient_E_LOSTMSG;
                            goto leave;
                        }
                        Log_print0(Diags_INFO, FXNN": return message received");
                    }

                    if (!messageFound) {
                        /*
                         * message queue empty
                         */

                        /* release the mailbox lock */
                        Semaphore_post(obj->mbxLock, &eb);
                        if (Error_check(&eb)) {
                            /* TODO */
                        }

                        /* get next message, this blocks the thread */
                        rval = MessageQ_get(obj->msgQue, &msgqMsg,
                            MessageQ_FOREVER);

                        if (rval < 0) {
                            Log_error0(FXNN": lost return message");
                            status = RcmClient_E_LOSTMSG;
                            goto leave;
                        }
                        Log_print0(Diags_INFO, FXNN": return message received");

                        if (msgqMsg == NULL) {
                            Log_error0(FXNN": reply message has been lost");
                            status = RcmClient_E_LOSTMSG;
                            goto leave;
                        }

                        /* acquire the mailbox lock */
                        Semaphore_pend(obj->mbxLock, Semaphore_FOREVER, &eb);
                        if (Error_check(&eb)) {
                            goto leave;  /* TODO */
                        }
                    }
                }
            }
            else {
                /* construct recipient on local stack */
                Recipient self;
                self.msgId = msgId;
                self.msg = NULL;
                SemThread_construct(&self.event, 0, NULL, &eb);
                if (Error_check(&eb)) {
                    /* TODO */
                }

                /* add recipient to wait list */
                elem = &self.elem;
                List_put(obj->recipients, elem);

                /* release the mailbox lock */
                Semaphore_post(obj->mbxLock, &eb);
                if (Error_check(&eb)) {
                    /* TODO */
                }

                /* wait on event */
                SemThread_pend(SemThread_handle(&self.event),
                    Semaphore_FOREVER, &eb);
                if (Error_check(&eb)) {
                    /* TODO */
                }

                /* acquire the mailbox lock */
                Semaphore_pend(obj->mbxLock, Semaphore_FOREVER, &eb);
                if (Error_check(&eb)) {
                    goto leave;  /* TODO */
                }

                if (NULL != self.msg) {
                    /* pickup message */
                    *returnMsg = self.msg;
                    messageFound = TRUE;
                }

                /* remove recipient from wait list */
                List_remove(obj->recipients, elem);
                SemThread_destruct(&self.event);

                /* release the mailbox lock */
                Semaphore_post(obj->mbxLock, &eb);
                if (Error_check(&eb)) {
                    /* TODO */
                }
            }
        }
    } /* while (!messageFound) */

leave:
    Log_print2(Diags_EXIT, "<-- %s: %d", (IArg)FXNN, (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmClient_getPacketAddr_P ========
 */
RcmClient_Packet *RcmClient_getPacketAddr_P(RcmClient_Message *msg)
{
    Int offset = (Int)&(((RcmClient_Packet *)0)->message);
    return ((RcmClient_Packet *)((Char *)msg - offset));
}


/*
 *  ======== getPacketAddrMsgqMsg ========
 */
static inline
RcmClient_Packet *getPacketAddrMsgqMsg(MessageQ_Msg msg)
{
    Int offset = (Int)&(((RcmClient_Packet *)0)->msgqHeader);
    return ((RcmClient_Packet *)((Char *)msg - offset));
}


/*
 *  ======== getPacketAddrElem ========
 */
static inline
RcmClient_Packet *getPacketAddrElem(List_Elem *elem)
{
    Int offset = (Int)&(((RcmClient_Packet *)0)->msgqHeader);
    return ((RcmClient_Packet *)((Char *)elem - offset));
}
