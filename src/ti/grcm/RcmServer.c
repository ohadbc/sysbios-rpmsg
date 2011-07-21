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
 *  ======== RcmServer.c ========
 *
 */

/* this define must precede inclusion of any xdc header file */
#define Registry_CURDESC ti_grcm_RcmServer__Desc
#define MODULE_NAME "ti.grcm.RcmServer"

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
#include <xdc/runtime/knl/ISemaphore.h>
#include <xdc/runtime/knl/Semaphore.h>
#include <xdc/runtime/knl/SemThread.h>
#include <xdc/runtime/knl/Thread.h>
#include <xdc/runtime/System.h>

#define MSGBUFFERSIZE    512   // Make global and move to MessageQCopy.h

#if defined(RCM_ti_ipc)
#include <ti/sdo/utils/List.h>
#include <ti/ipc/MultiProc.h>

#elif defined(RCM_ti_syslink)
#include <ti/syslink/utils/List.h>
#define List_Struct List_Object
#define List_handle(exp) (exp)

#else
    #error "undefined ipc binding";
#endif

/* local header files */
#include "RcmClient.h"
#include "RcmTypes.h"
#include "RcmServer.h"

#if USE_MESSAGEQCOPY
#include <ti/srvmgr/rpmsg_omx.h>
#endif

#define _RCM_KeyResetValue 0x07FF       // key reset value
#define _RCM_KeyMask 0x7FF00000         // key mask in function index
#define _RCM_KeyShift 20                // key bit position in function index

#define RcmServer_MAX_TABLES 9          // max number of function tables
#define RcmServer_POOL_MAP_LEN 4        // pool map length

#define RcmServer_E_InvalidFxnIdx       (-101)
#define RcmServer_E_JobIdNotFound       (-102)
#define RcmServer_E_PoolIdNotFound      (-103)

typedef struct {                        // function table element
    String                      name;
#if USE_MESSAGEQCOPY
    union  {
       RcmServer_MsgFxn         fxn;
       RcmServer_MsgCreateFxn   createFxn;
    }addr;
#else
    RcmServer_MsgFxn            addr;
#endif
    UInt16                      key;
} RcmServer_FxnTabElem;

typedef struct {
    Int                         length;
    RcmServer_FxnTabElem *      elem;
} RcmServer_FxnTabElemAry;

typedef struct {
    String                      name;       // pool name
    Int                         count;      // thread count (at create time)
    Thread_Priority             priority;   // thread priority
    Int                         osPriority;
    SizeT                       stackSize;  // thread stack size
    String                      stackSeg;   // thread stack placement
    ISemaphore_Handle           sem;        // message semaphore (counting)
    List_Struct                 threadList; // list of worker threads
    List_Struct                 readyQueue; // queue of messages
} RcmServer_ThreadPool;

typedef struct RcmServer_Object_tag {
    GateThread_Struct           gate;       // instance gate
    Ptr                         run;        // run semaphore for the server
#if USE_MESSAGEQCOPY
    MessageQCopy_Handle         serverQue;  // inbound message queue
    UInt32                      localAddr;  // inbound message queue address
    UInt32                      replyAddr;  // Reply address (same per inst.)
    UInt32                      dstProc;    // Reply processor.
#else
    MessageQ_Handle             serverQue;  // inbound message queue
#endif
    Thread_Handle               serverThread; // server thread object
    RcmServer_FxnTabElemAry     fxnTabStatic; // static function table
    RcmServer_FxnTabElem *      fxnTab[RcmServer_MAX_TABLES]; // base pointers
    UInt16                      key;        // function index key
    UInt16                      jobId;      // job id tracker
    Bool                        shutdown;   // server shutdown flag
    Int                         poolMap0Len;// length of static table
    RcmServer_ThreadPool *      poolMap[RcmServer_POOL_MAP_LEN];
    List_Handle                 jobList;    // list of job stream queues
} RcmServer_Object;

typedef struct {
    List_Elem                   elem;
    UInt16                      jobId;      // current job stream id
    Thread_Handle               thread;     // server thread object
    Bool                        terminate;  // thread terminate flag
    RcmServer_ThreadPool*       pool;       // worker pool
    RcmServer_Object *          server;     // server instance
} RcmServer_WorkerThread;

typedef struct {
    List_Elem                   elem;
    UInt16                      jobId;      // job stream id
    Bool                        empty;      // true if no messages on server
    List_Struct                 msgQue;     // queue of messages
} RcmServer_JobStream;

typedef struct RcmServer_Module_tag {
    String              name;
    IHeap_Handle        heap;
} RcmServer_Module;


/* private functions */
static
Int RcmServer_Instance_init_P(
        RcmServer_Object *              obj,
        String                          name,
        const RcmServer_Params *        params
    );

static
Int RcmServer_Instance_finalize_P(
        RcmServer_Object *              obj
    );

static
Int RcmServer_acqJobId_P(
        RcmServer_Object *              obj,
        UInt16 *                        jobIdPtr
    );

static
Int RcmServer_dispatch_P(
        RcmServer_Object *              obj,
        RcmClient_Packet *              packet
    );

static
Int RcmServer_execMsg_I(
        RcmServer_Object *              obj,
        RcmClient_Message *             msg
    );

static
Int RcmServer_getFxnAddr_P(
        RcmServer_Object *              obj,
        UInt32                          fxnIdx,
        RcmServer_MsgFxn *              addrPtr,
        RcmServer_MsgCreateFxn *        createPtr
    );

static
UInt16 RcmServer_getNextKey_P(
        RcmServer_Object *              obj
    );

static
Int RcmServer_getSymIdx_P(
        RcmServer_Object *              obj,
        String                          name,
        UInt32 *                        index
    );

static
Int RcmServer_getPool_P(
        RcmServer_Object *              obj,
        RcmClient_Packet *              packet,
        RcmServer_ThreadPool **         poolP
    );

static
Void RcmServer_process_P(
        RcmServer_Object *              obj,
        RcmClient_Packet *              packet
    );

static
Int RcmServer_relJobId_P(
        RcmServer_Object *              obj,
        UInt16                          jobId
    );

static
Void RcmServer_serverThrFxn_P(
        IArg                            arg
    );

static inline
Void RcmServer_setStatusCode_I(
        RcmClient_Packet *              packet,
        UInt16                          code
    );

static
Void RcmServer_workerThrFxn_P(
        IArg                            arg
    );


#define RcmServer_Module_heap() (RcmServer_Mod.heap)


/* static objects */
static Int curInit = 0;
static Char ti_grcm_RcmServer_Name[] = {
        't','i','.','s','d','o','.','r','c','m','.',
        'R','c','m','S','e','r','v','e','r','\0'
    };

static RcmServer_Module RcmServer_Mod = {
    MODULE_NAME,        /* name */
    (IHeap_Handle)NULL  /* heap */
};

/* module diags mask */
Registry_Desc Registry_CURDESC;


/*
 *  ======== RcmServer_init ========
 *
 *  This function must be serialized by the caller
 */
Void RcmServer_init(Void)
{
    Registry_Result result;


    if (curInit++ != 0) {
        return; /* module already initialized */
    }

    /* register with xdc.runtime to get a diags mask */
//  result = Registry_addModule(&Registry_CURDESC, MODULE_NAME);
    result = Registry_addModule(&Registry_CURDESC, ti_grcm_RcmServer_Name);
    Assert_isTrue(result == Registry_SUCCESS, (Assert_Id)NULL);

    /* the size of object and struct must be the same */
    Assert_isTrue(sizeof(RcmServer_Object) == sizeof(RcmServer_Struct), NULL);
}


/*
 *  ======== RcmServer_exit ========
 *
 *  This function must be serialized by the caller
 */
Void RcmServer_exit(Void)
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
 *  ======== RcmServer_Params_init ========
 */
Void RcmServer_Params_init(RcmServer_Params *params)
{
    /* server thread */
    params->priority = Thread_Priority_HIGHEST;
    params->osPriority = Thread_INVALID_OS_PRIORITY;
    params->stackSize = 0;  // use system default
    params->stackSeg = "";

    /* default pool */
    params->defaultPool.name = NULL;
    params->defaultPool.count = 0;
    params->defaultPool.priority = Thread_Priority_NORMAL;
    params->defaultPool.osPriority = Thread_INVALID_OS_PRIORITY;
    params->defaultPool.stackSize = 0;  // use system default
    params->defaultPool.stackSeg = "";

    /* worker pools */
    params->workerPools.length = 0;
    params->workerPools.elem = NULL;

    /* function table */
    params->fxns.length = 0;
    params->fxns.elem = NULL;
}


/*
 *  ======== RcmServer_create ========
 */
#define FXNN "RcmServer_create"
Int RcmServer_create(String name, RcmServer_Params *params,
        RcmServer_Handle *handle)
{
    RcmServer_Object *obj;
    Error_Block eb;
    Int status = RcmServer_S_SUCCESS;


    Log_print3(Diags_ENTRY, "--> "FXNN": (name=0x%x, params=0x%x, hP=0x%x)",
        (IArg)name, (IArg)params, (IArg)handle);

    /* initialize the error block */
    Error_init(&eb);
    *handle = (RcmServer_Handle)NULL;

    /* check for valid params */
    if (NULL == params) {
        Log_error0(FXNN": params ptr must not be NULL");
        status = RcmServer_E_FAIL;
        goto leave;
    }
    if (NULL == handle) {
        Log_error0(FXNN": Invalid pointer");
        status = RcmServer_E_FAIL;
        goto leave;
    }
    if (NULL == name) {
        Log_error0(FXNN": name passed is NULL!");
        status = RcmServer_E_FAIL;
        goto leave;
    }

    /* allocate the object */
    obj = (RcmServer_Handle)xdc_runtime_Memory_calloc(RcmServer_Module_heap(),
        sizeof(RcmServer_Object), sizeof(Int), &eb);

    if (NULL == obj) {
        Log_error2(FXNN": out of memory: heap=0x%x, size=%u",
            (IArg)RcmServer_Module_heap(), sizeof(RcmServer_Object));
        status = RcmServer_E_NOMEMORY;
        goto leave;
    }

    Log_print1(Diags_LIFECYCLE, FXNN": instance create: 0x%x", (IArg)obj);

    /* object-specific initialization */
    status = RcmServer_Instance_init_P(obj, name, params);

    if (status < 0) {
        RcmServer_Instance_finalize_P(obj);
        xdc_runtime_Memory_free(RcmServer_Module_heap(),
            (Ptr)obj, sizeof(RcmServer_Object));
        goto leave;
    }

    /* success, return opaque pointer */
    *handle = (RcmServer_Handle)obj;


leave:
    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmServer_construct ========
 */
#define FXNN "RcmServer_construct"
Int RcmServer_construct(RcmServer_Struct *structPtr, String name,
    const RcmServer_Params *params)
{
    RcmServer_Object *obj = (RcmServer_Object*)structPtr;
    Int status = RcmServer_S_SUCCESS;


    Log_print1(Diags_ENTRY, "--> "FXNN": (structPtr=0x%x)", (IArg)structPtr);
    Log_print1(Diags_LIFECYCLE, FXNN": instance construct: 0x%x", (IArg)obj);

    /* ensure the constructed object is zeroed */
    _memset((Void *)obj, 0, sizeof(RcmServer_Object));

    /* object-specific initialization */
    status = RcmServer_Instance_init_P(obj, name, params);

    if (status < 0) {
        RcmServer_Instance_finalize_P(obj);
        goto leave;
    }


leave:
    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmServer_Instance_init_P ========
 */
#define FXNN "RcmServer_Instance_init_P"
Int RcmServer_Instance_init_P(RcmServer_Object *obj, String name,
        const RcmServer_Params *params)
{
    Error_Block eb;
    List_Params listP;
#if USE_MESSAGEQCOPY == 0
    MessageQ_Params mqParams;
#endif
    Thread_Params threadP;
    SemThread_Params semThreadP;
    SemThread_Handle semThreadH;
    Int i, j;
    SizeT size;
    Char *cp;
    RcmServer_ThreadPool *poolAry;
    RcmServer_WorkerThread *worker;
    List_Handle listH;
    Int status = RcmServer_S_SUCCESS;


    Log_print1(Diags_ENTRY, "--> "FXNN": (obj=0x%x)", (IArg)obj);

    Error_init(&eb);

    /* initialize instance state */
    obj->shutdown = FALSE;
    obj->key = 0;
    obj->jobId = 0xFFFF;
    obj->run = NULL;
    obj->serverQue = NULL;
    obj->serverThread = NULL;
    obj->fxnTabStatic.length = 0;
    obj->fxnTabStatic.elem = NULL;
    obj->poolMap0Len = 0;
    obj->jobList = NULL;


    /* initialize the function table */
    for (i = 0; i < RcmServer_MAX_TABLES; i++) {
        obj->fxnTab[i] = NULL;
    }

    /* initialize the worker pool map */
    for (i = 0; i < RcmServer_POOL_MAP_LEN; i++) {
        obj->poolMap[i] = NULL;
    }

    /* create the instance gate */
    GateThread_construct(&obj->gate, NULL, &eb);

    if (Error_check(&eb)) {
        Log_error0(FXNN": could not create gate object");
        status = RcmServer_E_FAIL;
        goto leave;
    }

    /* create list for job objects */
#if defined(RCM_ti_ipc)
    List_Params_init(&listP);
    obj->jobList = List_create(&listP, &eb);

    if (Error_check(&eb)) {
        Log_error0(FXNN": could not create list object");
        status = RcmServer_E_FAIL;
        goto leave;
    }
#elif defined(RCM_ti_syslink)
    List_Params_init(&listP);
    obj->jobList = List_create(&listP, NULL);

    if (obj->jobList == NULL) {
        Log_error0(FXNN": could not create list object");
        status = RcmServer_E_FAIL;
        goto leave;
    }
#endif

    /* create the static function table */
    if (params->fxns.length > 0) {
        obj->fxnTabStatic.length = params->fxns.length;

        /* allocate static function table */
        size = params->fxns.length * sizeof(RcmServer_FxnTabElem);
        obj->fxnTabStatic.elem = xdc_runtime_Memory_alloc(
            RcmServer_Module_heap(), size, sizeof(Ptr), &eb);

        if (Error_check(&eb)) {
            Log_error2(FXNN": out of memory: heap=0x%x, size=%u",
                (IArg)RcmServer_Module_heap(), size);
            status = RcmServer_E_NOMEMORY;
            goto leave;
        }
        obj->fxnTabStatic.elem[0].name = NULL;

        /* allocate a single block to store all name strings */
        for (size = 0, i = 0; i < params->fxns.length; i++) {
            size += _strlen(params->fxns.elem[i].name) + 1;
        }
        cp = xdc_runtime_Memory_alloc(
            RcmServer_Module_heap(), size, sizeof(Ptr), &eb);

        if (Error_check(&eb)) {
            Log_error2(FXNN": out of memory: heap=0x%x, size=%u",
                (IArg)RcmServer_Module_heap(), size);
            status = RcmServer_E_NOMEMORY;
            goto leave;
        }

        /* copy function table data into allocated memory blocks */
        for (i = 0; i < params->fxns.length; i++) {
            _strcpy(cp, params->fxns.elem[i].name);
            obj->fxnTabStatic.elem[i].name = cp;
            cp += (_strlen(params->fxns.elem[i].name) + 1);
            obj->fxnTabStatic.elem[i].addr.fxn = params->fxns.elem[i].addr.fxn;
            obj->fxnTabStatic.elem[i].key = 0;
        }

        /* hook up the static function table */
        obj->fxnTab[0] = obj->fxnTabStatic.elem;
    }

    /* create static worker pools */
    if ((params->workerPools.length + 1) > RcmServer_POOL_MAP_LEN) {
        Log_error1(FXNN": Exceeded maximum number of worker pools =%d",
            (IArg) (params->workerPools.length) );
        status = RcmServer_E_NOMEMORY;
        goto leave;
    }
    obj->poolMap0Len = params->workerPools.length + 1; /* workers + default */

    /* allocate the static worker pool table */
    size = obj->poolMap0Len * sizeof(RcmServer_ThreadPool);
    obj->poolMap[0] = (RcmServer_ThreadPool *)xdc_runtime_Memory_alloc(
        RcmServer_Module_heap(), size, sizeof(Ptr), &eb);

    if (Error_check(&eb)) {
        Log_error2(FXNN": out of memory: heap=0x%x, size=%u",
            (IArg)RcmServer_Module_heap(), size);
        status = RcmServer_E_NOMEMORY;
        goto leave;
    }

    /* convenience alias */
    poolAry = obj->poolMap[0];

    /* allocate a single block to store all name strings         */
    /* Buffer format is: [SIZE][DPN][\0][WPN1][\0][WPN2][\0].... */
    /* DPN = Default Pool Name, WPN = Worker Pool Name           */
    /* In case, DPN is NULL, format is: [SIZE][\0][WPN1][\0].... */
    /* In case, WPN is NULL, format is: [SIZE][DPN][\0]          */
    size = sizeof(SizeT) /* block size */
        + (params->defaultPool.name == NULL ? 1 :
        _strlen(params->defaultPool.name) + 1);

    for (i = 0; i < params->workerPools.length; i++) {
        size += (params->workerPools.elem[i].name == NULL ? 0 :
            _strlen(params->workerPools.elem[i].name) + 1);
    }
    cp = xdc_runtime_Memory_alloc(
        RcmServer_Module_heap(), size, sizeof(Ptr), &eb);

    if (Error_check(&eb)) {
        Log_error2(FXNN": out of memory: heap=0x%x, size=%u",
            (IArg)RcmServer_Module_heap(), size);
        status = RcmServer_E_NOMEMORY;
        goto leave;
    }

    *(SizeT *)cp = size;
    cp += sizeof(SizeT);

    /* initialize the default worker pool, poolAry[0] */
    if (params->defaultPool.name != NULL) {
        _strcpy(cp, params->defaultPool.name);
        poolAry[0].name = cp;
        cp += (_strlen(params->defaultPool.name) + 1);
    }
    else {
        poolAry[0].name = cp;
        *cp++ = '\0';
    }

    poolAry[0].count = params->defaultPool.count;
    poolAry[0].priority = params->defaultPool.priority;
    poolAry[0].osPriority = params->defaultPool.osPriority;
    poolAry[0].stackSize = params->defaultPool.stackSize;
    poolAry[0].stackSeg = NULL;
    poolAry[0].sem = NULL;

    List_construct(&(poolAry[0].threadList), NULL);
    List_construct(&(poolAry[0].readyQueue), NULL);

    SemThread_Params_init(&semThreadP);
    semThreadP.mode = SemThread_Mode_COUNTING;

    semThreadH = SemThread_create(0, &semThreadP, &eb);

    if (Error_check(&eb)) {
        Log_error0(FXNN": could not create semaphore");
        status = RcmServer_E_FAIL;
        goto leave;
    }

    poolAry[0].sem = SemThread_Handle_upCast(semThreadH);

    /* initialize the static worker pools, poolAry[1..(n-1)] */
    for (i = 0; i < params->workerPools.length; i++) {
        if (params->workerPools.elem[i].name != NULL) {
            _strcpy(cp, params->workerPools.elem[i].name);
            poolAry[i+1].name = cp;
            cp += (_strlen(params->workerPools.elem[i].name) + 1);
        }
        else {
            poolAry[i+1].name = NULL;
        }

        poolAry[i+1].count = params->workerPools.elem[i].count;
        poolAry[i+1].priority = params->workerPools.elem[i].priority;
        poolAry[i+1].osPriority =params->workerPools.elem[i].osPriority;
        poolAry[i+1].stackSize = params->workerPools.elem[i].stackSize;
        poolAry[i+1].stackSeg = NULL;

        List_construct(&(poolAry[i+1].threadList), NULL);
        List_construct(&(poolAry[i+1].readyQueue), NULL);

        SemThread_Params_init(&semThreadP);
        semThreadP.mode = SemThread_Mode_COUNTING;

        semThreadH = SemThread_create(0, &semThreadP, &eb);

        if (Error_check(&eb)) {
            Log_error0(FXNN": could not create semaphore");
            status = RcmServer_E_FAIL;
            goto leave;
        }

        poolAry[i+1].sem = SemThread_Handle_upCast(semThreadH);
    }

    /* create the worker threads in each static pool */
    for (i = 0; i < obj->poolMap0Len; i++) {
        for (j = 0; j < poolAry[i].count; j++) {

            /* allocate worker thread object */
            size = sizeof(RcmServer_WorkerThread);
            worker = (RcmServer_WorkerThread *)xdc_runtime_Memory_alloc(
                RcmServer_Module_heap(), size, sizeof(Ptr), &eb);

            if (Error_check(&eb)) {
                Log_error2(FXNN": out of memory: heap=0x%x, size=%u",
                    (IArg)RcmServer_Module_heap(), size);
                status = RcmServer_E_NOMEMORY;
                goto leave;
            }

            /* initialize worker thread object */
            worker->jobId = RcmClient_DISCRETEJOBID;
            worker->thread = NULL;
            worker->terminate = FALSE;
            worker->pool = &(poolAry[i]);
            worker->server = obj;

            /* add worker thread to worker pool */
            listH = List_handle(&(poolAry[i].threadList));
            List_putHead(listH, &(worker->elem));

            /* create worker thread */
            Thread_Params_init(&threadP);
            threadP.arg = (IArg)worker;
            threadP.priority = poolAry[i].priority;
            threadP.osPriority = poolAry[i].osPriority;
            threadP.stackSize = poolAry[i].stackSize;
            threadP.instance->name = "RcmServer_workerThr";

            worker->thread = Thread_create(
                (Thread_RunFxn)(RcmServer_workerThrFxn_P), &threadP, &eb);

            if (Error_check(&eb)) {
                Log_error2(FXNN": could not create worker thread, "
                    "pool=%d, thread=%d", (IArg)i, (IArg)j);
                status = RcmServer_E_FAIL;
                goto leave;
            }
        }
    }

    /* create the semaphore used to release the server thread */
    SemThread_Params_init(&semThreadP);
    semThreadP.mode = SemThread_Mode_COUNTING;

    obj->run = SemThread_create(0, &semThreadP, &eb);

    if (Error_check(&eb)) {
        Log_error0(FXNN": could not create semaphore");
        status = RcmServer_E_FAIL;
        goto leave;
    }

    /* create the message queue for inbound messages */
#if USE_MESSAGEQCOPY
    obj->serverQue = MessageQCopy_create(MessageQCopy_ASSIGN_ANY,
                                         &obj->localAddr);
#ifdef BIOS_ONLY_TEST
    obj->dstProc = MultiProc_self();
#else
    obj->dstProc = MultiProc_getId("HOST");
#endif

#else
    MessageQ_Params_init(&mqParams);
    obj->serverQue = MessageQ_create(name, &mqParams);
#endif

    if (NULL == obj->serverQue) {
        Log_error0(FXNN": could not create server message queue");
        status = RcmServer_E_FAIL;
        goto leave;
    }

    /* create the server thread */
    Thread_Params_init(&threadP);
    threadP.arg = (IArg)obj;
    threadP.priority = params->priority;
    threadP.osPriority = params->osPriority;
    threadP.stackSize = params->stackSize;
    threadP.instance->name = "RcmServer_serverThr";

    obj->serverThread = Thread_create(
        (Thread_RunFxn)(RcmServer_serverThrFxn_P), &threadP, &eb);

    if (Error_check(&eb)) {
        Log_error0(FXNN": could not create server thread");
        status = RcmServer_E_FAIL;
        goto leave;
    }


leave:
    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmServer_delete ========
 */
#define FXNN "RcmServer_delete"
Int RcmServer_delete(RcmServer_Handle *handlePtr)
{
    RcmServer_Object *obj = (RcmServer_Object *)(*handlePtr);
    Int status = RcmClient_S_SUCCESS;


    Log_print1(Diags_ENTRY, "--> "FXNN": (handlePtr=0x%x)", (IArg)handlePtr);

    /* finalize the object */
    status = RcmServer_Instance_finalize_P(obj);

    /* free the object memory */
    Log_print1(Diags_LIFECYCLE, FXNN": instance delete: 0x%x", (IArg)obj);

    xdc_runtime_Memory_free(RcmServer_Module_heap(),
        (Ptr)obj, sizeof(RcmServer_Object));
    *handlePtr = NULL;

    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmServer_destruct ========
 */
#define FXNN "RcmServer_destruct"
Int RcmServer_destruct(RcmServer_Struct *structPtr)
{
    RcmServer_Object *obj = (RcmServer_Object *)(structPtr);
    Int status = RcmClient_S_SUCCESS;


    Log_print1(Diags_ENTRY, "--> "FXNN": (structPtr=0x%x)", (IArg)structPtr);
    Log_print1(Diags_LIFECYCLE, FXNN": instance destruct: 0x%x", (IArg)obj);

    /* finalize the object */
    status = RcmServer_Instance_finalize_P(obj);

    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmServer_Instance_finalize_P ========
 */
#define FXNN "RcmServer_Instance_finalize_P"
Int RcmServer_Instance_finalize_P(RcmServer_Object *obj)
{
    Int i, j;
    Int size;
    Char *cp;
    UInt tabCount;
    RcmServer_FxnTabElem *fdp;
    Error_Block eb;
    RcmServer_ThreadPool *poolAry;
    RcmServer_WorkerThread *worker;
    List_Elem *elem;
    List_Handle listH;
    List_Handle msgQueH;
    RcmClient_Packet *packet;
#if USE_MESSAGEQCOPY == 0
    MessageQ_Msg msgqMsg;
#endif
    SemThread_Handle semThreadH;
    RcmServer_JobStream *job;
    Int rval;
    Int status = RcmClient_S_SUCCESS;

    Log_print1(Diags_ENTRY, "--> "FXNN": (obj=0x%x)", (IArg)obj);

    /* must initialize the error block before using it */
    Error_init(&eb);

    /* block until server thread exits */
    obj->shutdown = TRUE;

    if (obj->serverThread != NULL) {
#if USE_MESSAGEQCOPY
        MessageQCopy_unblock(obj->serverQue);
#else
        MessageQ_unblock(obj->serverQue);
#endif
        Thread_join(obj->serverThread, &eb);

        if (Error_check(&eb)) {
            Log_error0(FXNN": server thread did not exit properly");
            status = RcmServer_E_FAIL;
            goto leave;
        }
    }

    /* delete any remaining job objects (there should not be any) */
    while ((elem = List_get(obj->jobList)) != NULL) {
        job = (RcmServer_JobStream *)elem;

        /* return any remaining messages (there should not be any) */
        msgQueH = List_handle(&job->msgQue);

        while ((elem = List_get(msgQueH)) != NULL) {
            packet = (RcmClient_Packet *)elem;
            Log_warning2(
                FXNN": returning unprocessed message, jobId=0x%x, packet=0x%x",
                (IArg)job->jobId, (IArg)packet);

            RcmServer_setStatusCode_I(packet, RcmServer_Status_Unprocessed);
#if USE_MESSAGEQCOPY
            packet->hdr.type = OMX_RAW_MSG;
            packet->hdr.len = PACKET_DATA_SIZE + packet->message.dataSize;
            rval = MessageQCopy_send(obj->dstProc, obj->replyAddr,
                                 obj->localAddr, (Ptr)&packet->hdr,
                                 PACKET_HDR_SIZE + packet->message.dataSize);
#else
            msgqMsg = &packet->msgqHeader;
            rval = MessageQ_put(MessageQ_getReplyQueue(msgqMsg), msgqMsg);
#endif
            if (rval < 0) {
                Log_error1(FXNN": unknown ipc error, 0x%x", (IArg)rval);
            }
        }

        /* finalize the job stream object */
        List_destruct(&job->msgQue);

        xdc_runtime_Memory_free(RcmServer_Module_heap(),
            (Ptr)job, sizeof(RcmServer_JobStream));
    }
    List_delete(&(obj->jobList));

    /* convenience alias */
    poolAry = obj->poolMap[0];

    /* free all the static pool resources */
    for (i = 0; i < obj->poolMap0Len; i++) {

        /* free all the worker thread objects */
        listH = List_handle(&(poolAry[i].threadList));

        /* mark each worker thread for termination */
        elem = NULL;
        while ((elem = List_next(listH, elem)) != NULL) {
            worker = (RcmServer_WorkerThread *)elem;
            worker->terminate = TRUE;
        }

        /* unblock each worker thread so it can terminate */
        elem = NULL;
        while ((elem = List_next(listH, elem)) != NULL) {
            Semaphore_post(poolAry[i].sem, &eb);

            if (Error_check(&eb)) {
                Log_error0(FXNN": post failed on thread");
                status = RcmServer_E_FAIL;
                goto leave;
            }
        }

        /* wait for each worker thread to terminate */
        elem = NULL;
        while ((elem = List_get(listH)) != NULL) {
            worker = (RcmServer_WorkerThread *)elem;

            Thread_join(worker->thread, &eb);

            if (Error_check(&eb)) {
                Log_error1(
                    FXNN": worker thread did not exit properly, thread=0x%x",
                    (IArg)worker->thread);
                status = RcmServer_E_FAIL;
                goto leave;
            }

            Thread_delete(&worker->thread);

            /* free the worker thread object */
            xdc_runtime_Memory_free(RcmServer_Module_heap(), (Ptr)worker,
                sizeof(RcmServer_WorkerThread));
        }

        /* free up pool resources */
        semThreadH = SemThread_Handle_downCast(poolAry[i].sem);
        SemThread_delete(&semThreadH);
        List_destruct(&(poolAry[i].threadList));

        /* return any remaining messages on the readyQueue */
        msgQueH = List_handle(&poolAry[i].readyQueue);

        while ((elem = List_get(msgQueH)) != NULL) {
            packet = (RcmClient_Packet *)elem;
            Log_warning2(
                FXNN": returning unprocessed message, msgId=0x%x, packet=0x%x",
                (IArg)packet->msgId, (IArg)packet);

            RcmServer_setStatusCode_I(packet, RcmServer_Status_Unprocessed);
#if USE_MESSAGEQCOPY
            packet->hdr.type = OMX_RAW_MSG;
            packet->hdr.len = PACKET_DATA_SIZE + packet->message.dataSize;
            rval = MessageQCopy_send(obj->dstProc, obj->replyAddr,
                                 obj->localAddr, (Ptr)&packet->hdr,
                                 PACKET_HDR_SIZE + packet->message.dataSize);
#else
            msgqMsg = &packet->msgqHeader;
            rval = MessageQ_put(MessageQ_getReplyQueue(msgqMsg), msgqMsg);
#endif
            if (rval < 0) {
                Log_error1(FXNN": unknown ipc error, 0x%x", (IArg)rval);
            }
        }

        List_destruct(&(poolAry[i].readyQueue));
    }

    /* free the name block for the static pools */
    if ((obj->poolMap[0] != NULL) && (obj->poolMap[0]->name != NULL)) {
        cp = obj->poolMap[0]->name;
        cp -= sizeof(SizeT);
        xdc_runtime_Memory_free(RcmServer_Module_heap(), (Ptr)cp, *(SizeT *)cp);
    }

    /* free the static worker pool table */
    if (obj->poolMap[0] != NULL) {
        xdc_runtime_Memory_free(RcmServer_Module_heap(), (Ptr)(obj->poolMap[0]),
            obj->poolMap0Len * sizeof(RcmServer_ThreadPool));
        obj->poolMap[0] = NULL;
    }

#if 0
    /* free all dynamic worker pools */
    for (p = 1; p < RcmServer_POOL_MAP_LEN; p++) {
        if ((poolAry = obj->poolMap[p]) == NULL) {
            continue;
        }
    }
#endif

    /* free up the dynamic function tables and any leftover name strings */
    for (i = 1; i < RcmServer_MAX_TABLES; i++) {
        if (obj->fxnTab[i] != NULL) {
            tabCount = (1 << (i + 4));
            for (j = 0; j < tabCount; j++) {
                if (((obj->fxnTab[i])+j)->name != NULL) {
                    cp = ((obj->fxnTab[i])+j)->name;
                    size = _strlen(cp) + 1;
                    xdc_runtime_Memory_free(RcmServer_Module_heap(), cp, size);
                }
            }
            fdp = obj->fxnTab[i];
            size = tabCount * sizeof(RcmServer_FxnTabElem);
            xdc_runtime_Memory_free(RcmServer_Module_heap(), fdp, size);
            obj->fxnTab[i] = NULL;
        }
    }

    if (NULL != obj->serverThread) {
        Thread_delete(&obj->serverThread);
    }

    if (NULL != obj->serverQue) {
#if USE_MESSAGEQCOPY
        MessageQCopy_delete(&obj->serverQue);
#else
        MessageQ_delete(&obj->serverQue);
#endif
    }

    if (NULL != obj->run) {
        SemThread_delete((SemThread_Handle *)(&obj->run));
    }

    /* free the name block for the static function table */
    if ((NULL != obj->fxnTabStatic.elem) &&
        (NULL != obj->fxnTabStatic.elem[0].name)) {
        for (size = 0, i = 0; i < obj->fxnTabStatic.length; i++) {
            size += _strlen(obj->fxnTabStatic.elem[i].name) + 1;
        }
        xdc_runtime_Memory_free(
            RcmServer_Module_heap(),
            obj->fxnTabStatic.elem[0].name, size);
    }

    /* free the static function table */
    if (NULL != obj->fxnTabStatic.elem) {
        xdc_runtime_Memory_free(RcmServer_Module_heap(),
            obj->fxnTabStatic.elem,
            obj->fxnTabStatic.length * sizeof(RcmServer_FxnTabElem));
    }

    /* destruct the instance gate */
    GateThread_destruct(&obj->gate);


leave:
    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmServer_addSymbol ========
 */
#define FXNN "RcmServer_addSymbol"
Int RcmServer_addSymbol(RcmServer_Object *obj, String funcName,
        RcmServer_MsgFxn addr, UInt32 *index)
{
    GateThread_Handle gateH;
    IArg key;
    Int len;
    UInt i, j;
    UInt tabCount;
    SizeT tabSize;
    UInt32 fxnIdx = 0xFFFF;
    RcmServer_FxnTabElem *slot = NULL;
    Error_Block eb;
    Int status = RcmServer_S_SUCCESS;


    Log_print3(Diags_ENTRY,
        "--> "FXNN": (obj=0x%x, name=0x%x, addr=0x%x)",
        (IArg)obj, (IArg)funcName, (IArg)addr);

    Error_init(&eb);

    /* protect the symbol table while changing it */
    gateH = GateThread_handle(&obj->gate);
    key = GateThread_enter(gateH);

    /* look for an empty slot to use */
    for (i = 1; i < RcmServer_MAX_TABLES; i++) {
        if (obj->fxnTab[i] != NULL) {
            for (j = 0; j < (1 << (i + 4)); j++) {
                if (((obj->fxnTab[i])+j)->addr.fxn == 0) {
                    slot = (obj->fxnTab[i]) + j;  // found empty slot
                    break;
                }
            }
        }
        else {
            /* all previous tables are full, allocate a new table */
            tabCount = (1 << (i + 4));
            tabSize = tabCount * sizeof(RcmServer_FxnTabElem);
            obj->fxnTab[i] = (RcmServer_FxnTabElem *)xdc_runtime_Memory_alloc(
                RcmServer_Module_heap(), tabSize, sizeof(Ptr), &eb);

            if (Error_check(&eb)) {
                Log_error0(FXNN": unable to allocate new function table");
                obj->fxnTab[i] = NULL;
                status = RcmServer_E_NOMEMORY;
                goto leave;
            }

            /* initialize the new table */
            for (j = 0; j < tabCount; j++) {
                ((obj->fxnTab[i])+j)->addr.fxn = 0;
                ((obj->fxnTab[i])+j)->name = NULL;
                ((obj->fxnTab[i])+j)->key = 0;
            }

            /* use first slot in new table */
            j = 0;
            slot = (obj->fxnTab[i])+j;
        }

        /* if new slot found, break out of loop */
        if (slot != NULL) {
            break;
        }
    }

    /* insert new symbol into slot */
    if (slot != NULL) {
        slot->addr.fxn = addr;
        len = _strlen(funcName) + 1;
        slot->name = (String)xdc_runtime_Memory_alloc(
            RcmServer_Module_heap(), len, sizeof(Char *), &eb);

        if (Error_check(&eb)) {
            Log_error0(FXNN": unable to allocate function name");
            slot->name = NULL;
            status = RcmServer_E_NOMEMORY;
            goto leave;
        }

        _strcpy(slot->name, funcName);
        slot->key = RcmServer_getNextKey_P(obj);
        fxnIdx = (slot->key << _RCM_KeyShift) | (i << 12) | j;
    }

    /* error, no more room to add new symbol */
    else {
        Log_error0(FXNN": cannot add symbol, table is full");
        status = RcmServer_E_SYMBOLTABLEFULL;
        goto leave;
    }


leave:
    GateThread_leave(gateH, key);

    /* on success, return new function index */
    if (status >= 0) {
        *index = fxnIdx;
    }
    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmServer_removeSymbol ========
 */
#define FXNN "RcmServer_removeSymbol"
Int RcmServer_removeSymbol(RcmServer_Object *obj, String name)
{
    GateThread_Handle gateH;
    IArg key;
    UInt32 fxnIdx;
    UInt tabIdx, tabOff;
    RcmServer_FxnTabElem *slot;
    Int status = RcmServer_S_SUCCESS;


    Log_print2(Diags_ENTRY,
        "--> "FXNN": (obj=0x%x, name=0x%x)", (IArg)obj, (IArg)name);

    /* protect the symbol table while changing it */
    gateH = GateThread_handle(&obj->gate);
    key = GateThread_enter(gateH);

    /* find the symbol in the table */
    status = RcmServer_getSymIdx_P(obj, name, &fxnIdx);

    if (status < 0) {
        Log_error0(FXNN": given symbol not found");
        goto leave;
    }

    /* static symbols have bit-31 set, cannot remove these symbols */
    if (fxnIdx & 0x80000000) {
        Log_error0(FXNN": cannot remove static symbol");
        status = RcmServer_E_SYMBOLSTATIC;
        goto leave;
    }

    /* get slot pointer */
    tabIdx = (fxnIdx & 0xF000) >> 12;
    tabOff = (fxnIdx & 0xFFF);
    slot = (obj->fxnTab[tabIdx]) + tabOff;

    /* clear the table index */
    slot->addr.fxn = 0;
    if (slot->name != NULL) {
        xdc_runtime_Memory_free(
            RcmServer_Module_heap(), slot->name, _strlen(slot->name) + 1);
        slot->name = NULL;
    }
    slot->key = 0;

leave:
    GateThread_leave(gateH, key);
    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmServer_start ========
 */
#define FXNN "RcmServer_start"
Int RcmServer_start(RcmServer_Object *obj)
{
    Error_Block eb;
    Int status = RcmServer_S_SUCCESS;


    Log_print1(Diags_ENTRY, "--> "FXNN": (obj=0x%x)", (IArg)obj);

    Error_init(&eb);

    /* unblock the server thread */
    Semaphore_post(obj->run, &eb);

    if (Error_check(&eb)) {
        Log_error0(FXNN": semaphore post failed");
        status = RcmServer_E_FAIL;
    }

    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmServer_acqJobId_P ========
 */
#define FXNN "RcmServer_acqJobId_P"
Int RcmServer_acqJobId_P(RcmServer_Object *obj, UInt16 *jobIdPtr)
{
    Error_Block eb;
    GateThread_Handle gateH;
    IArg key;
    Int count;
    UInt16 jobId;
    List_Elem *elem;
    RcmServer_JobStream *job;
    Int status = RcmServer_S_SUCCESS;


    Log_print2(Diags_ENTRY,
        "--> "FXNN": (obj=0x%x, jobIdPtr=0x%x)", (IArg)obj, (IArg)jobIdPtr);

    Error_init(&eb);
    gateH = GateThread_handle(&obj->gate);

    /* enter critical section */
    key = GateThread_enter(gateH);

    /* compute new job id */
    for (count = 0xFFFF; count > 0; count--) {

        /* generate a new job id */
        jobId = (obj->jobId == 0xFFFF ? obj->jobId = 1 : ++(obj->jobId));

        /* verify job id is not in use */
        elem = NULL;
        while ((elem = List_next(obj->jobList, elem)) != NULL) {
            job = (RcmServer_JobStream *)elem;
            if (jobId == job->jobId) {
                jobId = RcmClient_DISCRETEJOBID;
                break;
            }
        }

        if (jobId != RcmClient_DISCRETEJOBID) {
            break;
        }
    }

    /* check if job id was acquired */
    if (jobId == RcmClient_DISCRETEJOBID) {
        *jobIdPtr = RcmClient_DISCRETEJOBID;
        Log_error0(FXNN": no job id available");
        status = RcmServer_E_FAIL;
        GateThread_leave(gateH, key);
        goto leave;
    }

    /* create a new job steam object */
    job = xdc_runtime_Memory_alloc(RcmServer_Module_heap(),
        sizeof(RcmServer_JobStream), sizeof(Ptr), &eb);

    if (Error_check(&eb)) {
        Log_error2(FXNN": out of memory: heap=0x%x, size=%u",
            (IArg)RcmServer_Module_heap(), sizeof(RcmServer_JobStream));
        status = RcmServer_E_NOMEMORY;
        GateThread_leave(gateH, key);
        goto leave;
    }

    /* initialize new job stream object */
    job->jobId = jobId;
    job->empty = TRUE;
    List_construct(&(job->msgQue), NULL);

    /* put new job stream object at end of server list */
    List_put(obj->jobList, (List_Elem *)job);

    /* leave critical section */
    GateThread_leave(gateH, key);

    /* return new job id */
    *jobIdPtr = jobId;


leave:
    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmServer_dispatch_P ========
 *
 *  Return Value
 *      < 0: error
 *        0: success, job stream queue
 *      > 0: success, ready queue
 *
 *  Pool id description
 *
 *  Static Worker Pools
 *  --------------------------------------------------------------------
 *  15      1 = static pool
 *  14:8    reserved
 *  7:0     offset: 0 - 255
 *
 *  Dynamic Worker Pools
 *  --------------------------------------------------------------------
 *  15      0 = dynamic pool
 *  14:7    key: 0 - 255
 *  6:5     index: 1 - 3
 *  4:0     offset: 0 - [7, 15, 31]
 */
#define FXNN "RcmServer_dispatch_P"
Int RcmServer_dispatch_P(RcmServer_Object *obj, RcmClient_Packet *packet)
{
    GateThread_Handle gateH;
    IArg key;
    List_Elem *elem;
    List_Handle listH;
    RcmServer_ThreadPool *pool;
    UInt16 jobId;
    RcmServer_JobStream *job;
    Error_Block eb;
    Int status = RcmServer_S_SUCCESS;


    Log_print2(Diags_ENTRY,
        "--> "FXNN": (obj=0x%x, packet=0x%x)", (IArg)obj, (IArg)packet);

    Error_init(&eb);

    /* get the target pool id from the message */
    status = RcmServer_getPool_P(obj, packet, &pool);

    if (status < 0) {
        goto leave;
    }

    System_printf("Rcm dispatch: p:%d j:%d f:%d l:%d\n",
                  packet->message.poolId, packet->message.jobId,
                  packet->message.fxnIdx, packet->message.dataSize);

    /* discrete jobs go on the end of the ready queue */
    jobId = packet->message.jobId;

    if (jobId == RcmClient_DISCRETEJOBID) {
        listH = List_handle(&pool->readyQueue);
        List_put(listH, (List_Elem *)packet);

        /* dispatch a new worker thread */
        Semaphore_post(pool->sem, &eb);

        if (Error_check(&eb)) {
            Log_error0(FXNN": semaphore post failed");
        }
    }

    /* must be a job stream message */
    else {
        /* must protect job list while searching it */
        gateH = GateThread_handle(&obj->gate);
        key = GateThread_enter(gateH);

        /* find the job stream object in the list */
        elem = NULL;
        while ((elem = List_next(obj->jobList, elem)) != NULL) {
            job = (RcmServer_JobStream *)elem;
            if (job->jobId == jobId) {
                break;
            }
        }

        if (elem == NULL) {
            Log_error1(FXNN": failed to find jobId=%d", (IArg)jobId);
            status = RcmServer_E_JobIdNotFound;
        }

        /* if job object is empty, place message directly on ready queue */
        else if (job->empty) {
            job->empty = FALSE;
            listH = List_handle(&pool->readyQueue);
            List_put(listH, (List_Elem *)packet);

            /* dispatch a new worker thread */
            Semaphore_post(pool->sem, &eb);

            if (Error_check(&eb)) {
                Log_error0(FXNN": semaphore post failed");
            }
        }

        /* place message on job queue */
        else {
            listH = List_handle(&job->msgQue);
            List_put(listH, (List_Elem *)packet);
        }

        GateThread_leave(gateH, key);
    }


leave:
    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmServer_execMsg_I ========
 */
Int RcmServer_execMsg_I(RcmServer_Object *obj, RcmClient_Message *msg)
{
    RcmServer_MsgFxn fxn;
#if USE_MESSAGEQCOPY
    RcmServer_MsgCreateFxn createFxn = NULL;
#endif
    Int status;

    status = RcmServer_getFxnAddr_P(obj, msg->fxnIdx, &fxn, &createFxn);

    if (status >= 0) {
#if 0
        System_printf("RcmServer_execMsg_I: Calling fxnIdx: %d\n",
                      (msg->fxnIdx & 0x0000FFFF));
#endif
#if USE_MESSAGEQCOPY
        if (createFxn)  {
            msg->result = (*createFxn)(obj, msg->dataSize, msg->data);
        }
        else {
            msg->result = (*fxn)(msg->dataSize, msg->data);
        }
#else
        msg->result = (*fxn)(msg->dataSize, msg->data);
#endif
    }

    return(status);
}


/*
 *  ======== RcmServer_getFxnAddr_P ========
 *
 *  The function index (fxnIdx) uses the following format. Note that the
 *  format differs for static vs. dynamic functions. All static functions
 *  are in fxnTab[0], all dynamic functions are in fxnTab[1 - 8].
 *
 *  Bits    Description
 *
 *  Static Function Index
 *  --------------------------------------------------------------------
 *  31      static/dynamic function flag
 *              0 = dynamic function
 *              1 = static function
 *  30:20   reserved
 *  19:16   reserved
 *  15:0    offset: 0 - 65,535
 *
 *  Dynamic Function Index
 *  --------------------------------------------------------------------
 *  31      static/dynamic function flag
 *              0 = dynamic function
 *              1 = static function
 *  30:20   key
 *  19:16   reserved
 *  15:12   index: 1 - 8
 *  11:0    offset: 0 - [31, 63, 127, 255, 511, 1023, 2047, 4095]
 */
#define FXNN "RcmServer_getFxnAddr_P"
Int RcmServer_getFxnAddr_P(RcmServer_Object *obj, UInt32 fxnIdx,
        RcmServer_MsgFxn *addrPtr, RcmServer_MsgCreateFxn *createPtr)
{
    UInt i, j;
    UInt16 key;
    RcmServer_FxnTabElem *slot;
    RcmServer_MsgFxn addr = NULL;
    RcmServer_MsgCreateFxn createAddr = NULL;
    Int status = RcmServer_S_SUCCESS;

    /* static functions have bit-31 set */
    if (fxnIdx & 0x80000000) {
        j = (fxnIdx & 0x0000FFFF);
        if (j < (obj->fxnTabStatic.length)) {

            /* fetch the function address from the table */
            slot = (obj->fxnTab[0])+j;
            if (j == 0)  {
                 createAddr = slot->addr.createFxn;
            }
            else {
                 addr = slot->addr.fxn;
            }
        }
        else {
            Log_error1(FXNN": invalid function index 0x%x", (IArg)fxnIdx);
            status = RcmServer_E_InvalidFxnIdx;
            goto leave;
        }
    }
    /* must be a dynamic function */
    else {
        /* extract the key from the function index */
        key = (fxnIdx & _RCM_KeyMask) >> _RCM_KeyShift;

        i = (fxnIdx & 0xF000) >> 12;
        if ((i > 0) && (i < RcmServer_MAX_TABLES) && (obj->fxnTab[i] != NULL)) {

            /* fetch the function address from the table */
            j = (fxnIdx & 0x0FFF);
            slot = (obj->fxnTab[i])+j;
            addr = slot->addr.fxn;

            /* validate the key */
            if (key != slot->key) {
                Log_error1(FXNN": invalid function index 0x%x", (IArg)fxnIdx);
                status = RcmServer_E_InvalidFxnIdx;
                goto leave;
            }
        }
        else {
            Log_error1(FXNN": invalid function index 0x%x", (IArg)fxnIdx);
            status = RcmServer_E_InvalidFxnIdx;
            goto leave;
        }
    }

leave:
    if (status >= 0) {
       if (j == 0)  {
           *createPtr = createAddr;
       }
       else {
           *addrPtr = addr;
       }
    }
    return(status);
}
#undef FXNN


/* *  ======== RcmServer_getSymIdx_P ========
 *
 *  Must have table gate before calling this function.
 */
#define FXNN "RcmServer_getSymIdx_P"
Int RcmServer_getSymIdx_P(RcmServer_Object *obj, String name, UInt32 *index)
{
    UInt i, j, len;
    RcmServer_FxnTabElem *slot;
    UInt32 fxnIdx = 0xFFFFFFFF;
    Int status = RcmServer_S_SUCCESS;


    Log_print3(Diags_ENTRY,
        "--> "FXNN": (obj=0x%x, name=0x%x, index=0x%x)",
        (IArg)obj, (IArg)name, (IArg)index);

    /* search tables for given function name */
    for (i = 0; i < RcmServer_MAX_TABLES; i++) {
        if (obj->fxnTab[i] != NULL) {
            len = (i == 0) ? obj->fxnTabStatic.length : (1 << (i + 4));
            for (j = 0; j < len; j++) {
                slot = (obj->fxnTab[i]) + j;
                if ((((obj->fxnTab[i])+j)->name != NULL) &&
                    (_strcmp(((obj->fxnTab[i])+j)->name, name) == 0)) {
                    /* found function name */
                    if (i == 0) {
                        fxnIdx = 0x80000000 | j;
                    } else {
                        fxnIdx = (slot->key << _RCM_KeyShift) | (i << 12) | j;
                    }
                    break;
                }
            }
        }

        if (0xFFFFFFFF != fxnIdx) {
            break;
        }
    }

    /* log an error if the symbol was not found */
    if (fxnIdx == 0xFFFFFFFF) {
        Log_error0(FXNN": given symbol not found");
        status = RcmServer_E_SYMBOLNOTFOUND;
    }

    /* if success, return symbol index */
    if (status >= 0) {
        *index = fxnIdx;
    }

    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmServer_getNextKey_P ========
 */
UInt16 RcmServer_getNextKey_P(RcmServer_Object *obj)
{
    GateThread_Handle gateH;
    IArg gateKey;
    UInt16 key;


    gateH = GateThread_handle(&obj->gate);
    gateKey = GateThread_enter(gateH);

    if (obj->key <= 1) {
        obj->key = _RCM_KeyResetValue;  /* don't use 0 as a key value */
    }
    else {
        (obj->key)--;
    }
    key = obj->key;

    GateThread_leave(gateH, gateKey);

    return(key);
}


/*
 *  ======== RcmServer_getPool_P ========
 */
#define FXNN "RcmServer_getPool_P"
Int RcmServer_getPool_P(RcmServer_Object *obj,
        RcmClient_Packet *packet, RcmServer_ThreadPool **poolP)
{
    UInt16 poolId;
    UInt16 offset;
    Int status = RcmServer_S_SUCCESS;


    poolId = packet->message.poolId;

    /* static pools have bit-15 set */
    if (poolId & 0x8000) {
        offset = (poolId & 0x00FF);
        if (offset < obj->poolMap0Len) {
            *poolP = &(obj->poolMap[0])[offset];
        }
        else {
            Log_error1(FXNN": pool id=0x%x not found", (IArg)poolId);
            *poolP = NULL;
            status = RcmServer_E_PoolIdNotFound;
            goto leave;
        }
    }

leave:
    return(status);
}
#undef FXNN


/*
 *  ======== RcmServer_process_P ========
 */
#define FXNN "RcmServer_process_P"
Void RcmServer_process_P(RcmServer_Object *obj, RcmClient_Packet *packet)
{
    String name;
    UInt32 fxnIdx;
    RcmServer_MsgFxn fxn;
    RcmClient_Message *rcmMsg;
#if USE_MESSAGEQCOPY
    RcmServer_MsgCreateFxn createFxn = NULL;
#endif
#if USE_MESSAGEQCOPY == 0
    MessageQ_Msg msgqMsg;
#endif
    UInt16 messageType;
    Error_Block eb;
    UInt16 jobId;
    Int rval;
    Int status = RcmServer_S_SUCCESS;


    Log_print2(Diags_ENTRY,
        "--> "FXNN": (obj=0x%x, packet=0x%x)", (IArg)obj, (IArg)packet);

    Error_init(&eb);

    /* decode the message */
    rcmMsg = &packet->message;
#if USE_MESSAGEQCOPY == 0
    msgqMsg = &packet->msgqHeader;
#endif
    Log_print1(Diags_INFO, FXNN": message desc=0x%x", (IArg)packet->desc);

    /* extract the message type from the packet descriptor field */
    messageType = (RcmClient_Desc_TYPE_MASK & packet->desc) >>
        RcmClient_Desc_TYPE_SHIFT;

    /* process the given message */
    switch (messageType) {

        case RcmClient_Desc_RCM_MSG:
            rval = RcmServer_execMsg_I(obj, rcmMsg);

            if (rval < 0) {
                switch (rval) {
                    case RcmServer_E_InvalidFxnIdx:
                        RcmServer_setStatusCode_I(
                            packet, RcmServer_Status_INVALID_FXN);
                        break;
                    default:
                        RcmServer_setStatusCode_I(
                            packet, RcmServer_Status_Error);
                        break;
                }
            }
            else if (rcmMsg->result < 0) {
                RcmServer_setStatusCode_I(packet, RcmServer_Status_MSG_FXN_ERR);
            }
            else {
                RcmServer_setStatusCode_I(packet, RcmServer_Status_SUCCESS);
            }

#if USE_MESSAGEQCOPY
#if 0
            System_printf("RcmServer_process_P: Sending reply from: %d to: %d\n",
                      obj->localAddr, obj->replyAddr);
#endif

            packet->hdr.type = OMX_RAW_MSG;
            packet->hdr.len = PACKET_DATA_SIZE + packet->message.dataSize;
            status = MessageQCopy_send(obj->dstProc, obj->replyAddr,
                                 obj->localAddr, (Ptr)&packet->hdr,
                                 PACKET_HDR_SIZE + packet->message.dataSize);
#else
            status = MessageQ_put(MessageQ_getReplyQueue(msgqMsg), msgqMsg);
#endif
            if (status < 0) {
                Log_error1(FXNN": unknown ipc error, 0x%x", (IArg)status);
            }
            break;

        case RcmClient_Desc_CMD:
            status = RcmServer_execMsg_I(obj, rcmMsg);

            /* if all went well, free the message */
            if ((status >= 0) && (rcmMsg->result >= 0)) {

#if USE_MESSAGEQCOPY == 0
                status = MessageQ_free(msgqMsg);
#endif
                if (status < 0) {
                    Log_error1(
                        FXNN": MessageQ_free returned error %d", (IArg)status);
                }
            }

            /* an error occurred, must return message to client */
            else {
                if (status < 0) {
                    /* error trying to process the message */
                    switch (status) {
                        case RcmServer_E_InvalidFxnIdx:
                            RcmServer_setStatusCode_I(
                                packet, RcmServer_Status_INVALID_FXN);
                            break;
                        default:
                            RcmServer_setStatusCode_I(
                                packet, RcmServer_Status_Error);
                            break;
                    }
                }
                else  {
                    /* error in message function */
                    RcmServer_setStatusCode_I(
                        packet, RcmServer_Status_MSG_FXN_ERR);
                }

                /* send error message back to client */
#if USE_MESSAGEQCOPY
                packet->hdr.type = OMX_RAW_MSG;
                packet->hdr.len = PACKET_DATA_SIZE + packet->message.dataSize;
                status = MessageQCopy_send(obj->dstProc, obj->replyAddr,
                                 obj->localAddr, (Ptr)&packet->hdr,
                                 PACKET_HDR_SIZE + packet->message.dataSize);
#else
                status = MessageQ_put(MessageQ_getReplyQueue(msgqMsg), msgqMsg);
#endif
                if (status < 0) {
                    Log_error1(FXNN": unknown ipc error, 0x%x", (IArg)status);
                }
            }
            break;

        case RcmClient_Desc_DPC:
            rval = RcmServer_getFxnAddr_P(obj, rcmMsg->fxnIdx, &fxn,
                                          &createFxn);

            if (rval < 0) {
                RcmServer_setStatusCode_I(
                    packet, RcmServer_Status_SYMBOL_NOT_FOUND);
                Error_init(&eb);
            }

#if USE_MESSAGEQCOPY
            packet->hdr.type = OMX_RAW_MSG;
            packet->hdr.len = PACKET_DATA_SIZE + packet->message.dataSize;
            status = MessageQCopy_send(obj->dstProc, obj->replyAddr,
                                 obj->localAddr, (Ptr)&packet->hdr,
                                 PACKET_HDR_SIZE + packet->message.dataSize);
#else
            status = MessageQ_put(MessageQ_getReplyQueue(msgqMsg), msgqMsg);
#endif
            if (status < 0) {
                Log_error1(FXNN": unknown ipc error, 0x%x", (IArg)status);
            }

            /* invoke the function with a null context */
#if USE_MESSAGEQCOPY
            if (createFxn)  {
                 (*createFxn)(obj, 0, NULL);
            }
            else {
                 (*fxn)(0, NULL);
            }
#else
            (*fxn)(0, NULL);
#endif
            break;

        case RcmClient_Desc_SYM_ADD:
            break;

        case RcmClient_Desc_SYM_IDX:
            name = (String)rcmMsg->data;
            rval = RcmServer_getSymIdx_P(obj, name, &fxnIdx);

            if (rval < 0) {
                RcmServer_setStatusCode_I(
                    packet, RcmServer_Status_SYMBOL_NOT_FOUND);
            }
            else {
                RcmServer_setStatusCode_I(packet, RcmServer_Status_SUCCESS);
                rcmMsg->data[0] = fxnIdx;
                rcmMsg->result = 0;
            }

#if USE_MESSAGEQCOPY
            packet->hdr.type = OMX_RAW_MSG;
            packet->hdr.len = PACKET_DATA_SIZE + packet->message.dataSize;
            status = MessageQCopy_send(obj->dstProc, obj->replyAddr,
                                 obj->localAddr, (Ptr)&packet->hdr,
                                 PACKET_HDR_SIZE + packet->message.dataSize);
#else
            status = MessageQ_put(MessageQ_getReplyQueue(msgqMsg), msgqMsg);
#endif
            if (status < 0) {
                Log_error1(FXNN": unknown ipc error, 0x%x", (IArg)status);
            }
            break;

        case RcmClient_Desc_JOB_ACQ:
            rval = RcmServer_acqJobId_P(obj, &jobId);

            if (rval < 0) {
                RcmServer_setStatusCode_I(packet, RcmServer_Status_Error);
            }
            else {
                RcmServer_setStatusCode_I(packet, RcmServer_Status_SUCCESS);
                *(UInt16 *)(&rcmMsg->data[0]) = jobId;
                rcmMsg->result = 0;
            }

#if USE_MESSAGEQCOPY
            packet->hdr.type = OMX_RAW_MSG;
            packet->hdr.len = PACKET_DATA_SIZE + packet->message.dataSize;
            status = MessageQCopy_send(obj->dstProc, obj->replyAddr,
                                 obj->localAddr, (Ptr)&packet->hdr,
                                 PACKET_HDR_SIZE + packet->message.dataSize);
#else
            status = MessageQ_put(MessageQ_getReplyQueue(msgqMsg), msgqMsg);
#endif
            if (status < 0) {
                Log_error1(FXNN": unknown ipc error, 0x%x", (IArg)status);
            }
            break;

        case RcmClient_Desc_JOB_REL:
            jobId = (UInt16)(rcmMsg->data[0]);
            rval = RcmServer_relJobId_P(obj, jobId);

            if (rval < 0) {
                switch (rval) {
                    case RcmServer_E_JobIdNotFound:
                        RcmServer_setStatusCode_I(
                            packet, RcmServer_Status_JobNotFound);
                        break;
                    default:
                        RcmServer_setStatusCode_I(
                            packet, RcmServer_Status_Error);
                        break;
                }
                rcmMsg->result = rval;
            }
            else {
                RcmServer_setStatusCode_I(packet, RcmServer_Status_SUCCESS);
                rcmMsg->result = 0;
            }

#if USE_MESSAGEQCOPY
            packet->hdr.type = OMX_RAW_MSG;
            packet->hdr.len = PACKET_DATA_SIZE + packet->message.dataSize;
            status = MessageQCopy_send(obj->dstProc, obj->replyAddr,
                                 obj->localAddr, (Ptr)&packet->hdr,
                                 PACKET_HDR_SIZE + packet->message.dataSize);
#else
            status = MessageQ_put(MessageQ_getReplyQueue(msgqMsg), msgqMsg);
#endif
            if (status < 0) {
                Log_error1(FXNN": unknown ipc error, 0x%x", (IArg)status);
            }
            break;

        default:
            Log_error1(FXNN": unknown message type recieved, 0x%x",
                (IArg)messageType);
            break;
    }

    Log_print0(Diags_EXIT, "<-- "FXNN":");
}
#undef FXNN


/*
 *  ======== RcmServer_relJobId_P ========
 */
#define FXNN "RcmServer_relJobId_P"
Int RcmServer_relJobId_P(RcmServer_Object *obj, UInt16 jobId)
{
    GateThread_Handle gateH;
    IArg key;
    List_Elem *elem;
    List_Handle msgQueH;
    RcmClient_Packet *packet;
    RcmServer_JobStream *job;
#if USE_MESSAGEQCOPY == 0
    MessageQ_Msg msgqMsg;
#endif
    Int rval;
    Int status = RcmServer_S_SUCCESS;


    Log_print2(Diags_ENTRY,
        "--> "FXNN": (obj=0x%x, jobId=0x%x)", (IArg)obj, (IArg)jobId);


    /* must protect job list while searching and modifying it */
    gateH = GateThread_handle(&obj->gate);
    key = GateThread_enter(gateH);

    /* find the job stream object in the list */
    elem = NULL;
    while ((elem = List_next(obj->jobList, elem)) != NULL) {
        job = (RcmServer_JobStream *)elem;

        /* remove the job stream object from the list */
        if (job->jobId == jobId) {
            List_remove(obj->jobList, elem);
            break;
        }
    }

    GateThread_leave(gateH, key);

    if (elem == NULL) {
        status = RcmServer_E_JobIdNotFound;
        Log_error1(FXNN": failed to find jobId=%d", (IArg)jobId);
        goto leave;
    }

    /* return any pending messages on the message queue */
    msgQueH = List_handle(&job->msgQue);

    while ((elem = List_get(msgQueH)) != NULL) {
        packet = (RcmClient_Packet *)elem;
        Log_warning2(
            FXNN": returning unprocessed message, jobId=0x%x, packet=0x%x",
            (IArg)jobId, (IArg)packet);

        RcmServer_setStatusCode_I(packet, RcmServer_Status_Unprocessed);

#if USE_MESSAGEQCOPY
        packet->hdr.type = OMX_RAW_MSG;
        packet->hdr.len = PACKET_DATA_SIZE + packet->message.dataSize;
        rval = MessageQCopy_send(obj->dstProc, obj->replyAddr,
                                 obj->localAddr, (Ptr)&packet->hdr,
                                 PACKET_HDR_SIZE + packet->message.dataSize);
#else
        msgqMsg = &packet->msgqHeader;
        rval = MessageQ_put(MessageQ_getReplyQueue(msgqMsg), msgqMsg);
#endif
        if (rval < 0) {
            Log_error1(FXNN": unknown ipc error, 0x%x", (IArg)rval);
        }
    }

    /* finalize the job stream object */
    List_destruct(&job->msgQue);

    xdc_runtime_Memory_free(RcmServer_Module_heap(),
        (Ptr)job, sizeof(RcmServer_JobStream));


leave:
    Log_print1(Diags_EXIT, "<-- "FXNN": %d", (IArg)status);
    return(status);
}
#undef FXNN


/*
 *  ======== RcmServer_serverThrFxn_P ========
 */
#define FXNN "RcmServer_serverThrFxn_P"
Void RcmServer_serverThrFxn_P(IArg arg)
{
    Error_Block eb;
    RcmClient_Packet *packet;
#if USE_MESSAGEQCOPY
    Char         recvBuf[MSGBUFFERSIZE];
    UInt16       len;
#else
    MessageQ_Msg msgqMsg = NULL;
#endif
    Int rval;
    Bool running = TRUE;
    RcmServer_Object *obj = (RcmServer_Object *)arg;
    Int dataSize;

#if USE_MESSAGEQCOPY
    packet = (RcmClient_Packet *)&recvBuf[0];
#endif

    Log_print1(Diags_ENTRY, "--> "FXNN": (arg=0x%x)", arg);

    Error_init(&eb);

    /* wait until ready to run */
    Semaphore_pend(obj->run, Semaphore_FOREVER, &eb);

    if (Error_check(&eb)) {
        Log_error0(FXNN": Semaphore_pend failure in server thread");
    }

    /* main processing loop */
    while (running) {
        Log_print1(Diags_INFO,
            FXNN": waiting for message, thread=0x%x",
            (IArg)(obj->serverThread));

        /* block until message arrives */
        do {
#if USE_MESSAGEQCOPY
            rval = MessageQCopy_recv(obj->serverQue, (Ptr)&packet->hdr, &len,
                      &obj->replyAddr, MessageQCopy_FOREVER);
#if 0
            System_printf("RcmServer_serverThrFxn_P: Received msg of len %d "
                          "from: %d\n",
                         len, obj->replyAddr);

            System_printf("hdr - t:%d f:%d l:%d\n", packet->hdr.type,
                          packet->hdr.flags, packet->hdr.len);

            System_printf("pkt - d:%d m:%d\n", packet->desc, packet->msgId);
#endif
            if (packet->hdr.type == OMX_DISC_REQ) {
                System_printf("RcmServer_serverThrFxn_P: Got OMX_DISCONNECT\n");
            }
            Assert_isTrue((len <= MSGBUFFERSIZE), NULL);
            Assert_isTrue((packet->hdr.type == OMX_RAW_MSG) ||
                          (packet->hdr.type == OMX_DISC_REQ) , NULL);

            if ((rval < 0) && (rval != MessageQCopy_E_UNBLOCKED)) {
#else
            rval = MessageQ_get(obj->serverQue, &msgqMsg, MessageQ_FOREVER);
            if ((rval < 0) && (rval != MessageQ_E_UNBLOCKED)) {
#endif
                Log_error1(FXNN": ipc error 0x%x", (IArg)rval);
                /* keep running and hope for the best */
            }
#if USE_MESSAGEQCOPY
        } while (FALSE);
#else
        } while ((msgqMsg == NULL) && !obj->shutdown);
#endif

        /* if shutdown, exit this thread */
#if USE_MESSAGEQCOPY
        if (obj->shutdown || packet->hdr.type == OMX_DISC_REQ) {
            running = FALSE;
            Log_print1(Diags_INFO,
                FXNN": terminating, thread=0x%x", (IArg)(obj->serverThread));
            continue;
        }
#else
        if (obj->shutdown) {
            running = FALSE;
            Log_print1(Diags_INFO,
                FXNN": terminating, thread=0x%x", (IArg)(obj->serverThread));
            if (msgqMsg == NULL ) {
                continue;
            }
        }
#endif

#if USE_MESSAGEQCOPY == 0
        packet = (RcmClient_Packet *)msgqMsg;
#endif

        Log_print2(Diags_INFO,
            FXNN": message received, thread=0x%x packet=0x%x",
            (IArg)(obj->serverThread), (IArg)packet);

        if ((packet->message.poolId == RcmClient_DEFAULTPOOLID)
            && ((obj->poolMap[0])[0].count == 0)) {

            /* in-band (server thread) message processing */
            RcmServer_process_P(obj, packet);
        }
        else {
            /* out-of-band (worker thread) message processing */
            rval = RcmServer_dispatch_P(obj, packet);

            /* if error, message was not dispatched; must return to client */
            if (rval < 0) {
                switch (rval) {
                    case RcmServer_E_JobIdNotFound:
                        RcmServer_setStatusCode_I(
                            packet, RcmServer_Status_JobNotFound);
                        break;

                    case RcmServer_E_PoolIdNotFound:
                        RcmServer_setStatusCode_I(
                            packet, RcmServer_Status_PoolNotFound);
                        break;

                    default:
                        RcmServer_setStatusCode_I(
                            packet, RcmServer_Status_Error);
                        break;
                }
                packet->message.result = rval;

                /* return the message to the client */
#if USE_MESSAGEQCOPY
#if 0
                System_printf("RcmServer_serverThrFxn_P: "
                              "Sending response from: %d to: %d\n",
                              obj->localAddr, obj->replyAddr);
                System_printf("sending: %d + %d\n", PACKET_HDR_SIZE,
                              packet->message.dataSize);
#endif
                packet->hdr.type = OMX_RAW_MSG;
                if (rval < 0) {
                    packet->hdr.len = sizeof(UInt32);
                    packet->desc = 0x4142;
                    packet->msgId = 0x0044;
                    dataSize = sizeof(struct rpmsg_omx_hdr) + sizeof(UInt32);
                }
                else {
                    packet->hdr.len = PACKET_DATA_SIZE +
                        packet->message.dataSize;
                    dataSize = PACKET_HDR_SIZE + packet->message.dataSize;
                }
                rval = MessageQCopy_send(obj->dstProc, obj->replyAddr,
                                 obj->localAddr, (Ptr)&packet->hdr, dataSize);
#else
                rval = MessageQ_put(MessageQ_getReplyQueue(msgqMsg), msgqMsg);
#endif
                if (rval < 0) {
                    Log_error1(FXNN": unknown ipc error, 0x%x", (IArg)rval);
                }
            }
        }
    }

    System_printf("RcmServer_serverThrFxn_P: Exiting thread.\n");

    Log_print0(Diags_EXIT, "<-- "FXNN":");
}
#undef FXNN


/*
 *  ======== RcmServer_setStatusCode_I ========
 */
Void RcmServer_setStatusCode_I(RcmClient_Packet *packet, UInt16 code)
{

    /* code must be 0 - 15, it has to fit in a 4-bit field */
    Assert_isTrue((code < 16), NULL);

    packet->desc &= ~(RcmClient_Desc_TYPE_MASK);
    packet->desc |= ((code << RcmClient_Desc_TYPE_SHIFT)
        & RcmClient_Desc_TYPE_MASK);
}


/*
 *  ======== RcmServer_workerThrFxn_P ========
 */
#define FXNN "RcmServer_workerThrFxn_P"
Void RcmServer_workerThrFxn_P(IArg arg)
{
    Error_Block eb;
    RcmClient_Packet *packet;
    List_Elem *elem;
    List_Handle listH;
    List_Handle readyQueueH;
    UInt16 jobId;
    GateThread_Handle gateH;
    IArg key;
    RcmServer_ThreadPool *pool;
    RcmServer_JobStream *job;
    RcmServer_WorkerThread *obj;
    Bool running;
    Int rval;


    Log_print1(Diags_ENTRY, "--> "FXNN": (arg=0x%x)", arg);

    Error_init(&eb);
    obj = (RcmServer_WorkerThread *)arg;
    readyQueueH = List_handle(&obj->pool->readyQueue);
    packet = NULL;
    running = TRUE;

    /* main processing loop */
    while (running) {
        Log_print1(Diags_INFO,
            FXNN": waiting for job, thread=0x%x", (IArg)(obj->thread));

        /* if no current message, wait until signaled to run */
        if (packet == NULL) {
            Semaphore_pend(obj->pool->sem, Semaphore_FOREVER, &eb);

            if (Error_check(&eb)) {
                Log_error0(FXNN": semaphore pend failed");
            }
        }

        /* check if thread should terminate */
        if (obj->terminate) {
            running = FALSE;
            Log_print1(Diags_INFO,
                FXNN": terminating, thread=0x%x", (IArg)(obj->thread));
            continue;
        }

        /* get next message from ready queue */
        if (packet == NULL) {
            packet = (RcmClient_Packet *)List_get(readyQueueH);
        }

        if (packet == NULL) {
            Log_error1(FXNN": ready queue is empty, thread=0x%x",
                (IArg)(obj->thread));
            continue;
        }

        Log_print2(Diags_INFO, FXNN": job received, thread=0x%x packet=0x%x",
            (IArg)obj->thread, (IArg)packet);

        /* remember the message job id */
        jobId = packet->message.jobId;

        /* process the message */
        RcmServer_process_P(obj->server, packet);
        packet = NULL;

        /* If this worker thread just finished processing a job message,
         * queue up the next message for this job id. As an optimization,
         * if the message is addressed to this worker's pool, then don't
         * signal the semaphore, just get the next message from the queue
         * and processes it. This keeps the current thread running instead
         * of switching to another thread.
         */
        if (jobId != RcmClient_DISCRETEJOBID) {

            /* must protect job list while searching it */
            gateH = GateThread_handle(&obj->server->gate);
            key = GateThread_enter(gateH);

            /* find the job object in the list */
            elem = NULL;
            while ((elem = List_next(obj->server->jobList, elem)) != NULL) {
                job = (RcmServer_JobStream *)elem;
                if (job->jobId == jobId) {
                    break;
                }
            }

            /* if job object not found, it is not an error */
            if (elem == NULL) {
                GateThread_leave(gateH, key);
                continue;
            }

            /* found the job object */
            listH = List_handle(&job->msgQue);

            /* get next job message and either process it or queue it */
            do {
                elem = List_get(listH);

                if (elem == NULL) {
                    job->empty = TRUE;  /* no more messages */
                    break;
                }
                else {
                    /* get target pool id */
                    packet = (RcmClient_Packet *)elem;
                    rval = RcmServer_getPool_P(obj->server, packet, &pool);

                    /* if error, return the message to the client */
                    if (rval < 0) {
                        switch (rval) {
                            case RcmServer_E_PoolIdNotFound:
                                RcmServer_setStatusCode_I(
                                    packet, RcmServer_Status_PoolNotFound);
                                break;

                            default:
                                RcmServer_setStatusCode_I(
                                    packet, RcmServer_Status_Error);
                                break;
                        }
                        packet->message.result = rval;

#if USE_MESSAGEQCOPY
                        packet->hdr.type = OMX_RAW_MSG;
                        packet->hdr.len = PACKET_DATA_SIZE + packet->message.dataSize;
                        rval = MessageQCopy_send(
                                 (obj->server)->dstProc,
                                 (obj->server)->replyAddr,
                                 (obj->server)->localAddr, (Ptr)&packet->hdr,
                                 PACKET_HDR_SIZE + packet->message.dataSize);
#else
                        rval = MessageQ_put(
                            MessageQ_getReplyQueue(&packet->msgqHeader),
                            &packet->msgqHeader);
#endif
                        if (rval < 0) {
                            Log_error1(
                                FXNN": unknown ipc error, 0x%x", (IArg)rval);
                        }
                    }
                    /* packet is valid, queue it in the corresponding pool's
                     * ready queue */
                    else {
                        listH = List_handle(&pool->readyQueue);
                        List_put(listH, elem);
                        packet = NULL;
                        Semaphore_post(pool->sem, &eb);

                        if (Error_check(&eb)) {
                            Log_error0(FXNN": semaphore post failed");
                        }
                    }

                    /* loop around and wait to be run again */
                }

            } while (rval < 0);

            GateThread_leave(gateH, key);
        }
    }  /* while (running) */

    Log_print0(Diags_EXIT, "<-- "FXNN":");
}
#undef FXNN


/*
 *  ======== RcmServer_getLocalAddress ========
 */
UInt32 RcmServer_getLocalAddress(RcmServer_Object *obj)
{
    return(obj->localAddr);
}


/*
 *  ======== RcmServer_getRemoteAddress ========
 */
UInt32 RcmServer_getRemoteAddress(RcmServer_Object *obj)
{
    return(obj->replyAddr);
}



/*
 *  ======== RcmServer_getRemoteProc ========
 */
UInt16  RcmServer_getRemoteProc(RcmServer_Object *obj)
{
    return(obj->dstProc);
}
