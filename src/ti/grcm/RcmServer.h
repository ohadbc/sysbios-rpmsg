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
 *  ======== RcmServer.h ========
 *
 */

/*!
 *  @file ti/grcm/RcmServer.h
 *
 *  @brief Remote Command Message Server Module. An RcmServer processes
 *  inbound messages received from an RcmClient.
 *
 *  The RcmServer processes inbound messages received from an RcmClient.
 *  After processing a message, the server will return the message to
 *  the client.
 *
 *  @sa RcmClient.h <br>
 *  @ref ti_grcm "RCM Overview"
 */

/*!
 *  @defgroup ti_grcm_RcmServer RcmServer
 *
 *  @brief Remote Command Message Server Module. An RcmServer processes
 *  inbound messages received from an RcmClient.
 *
 *  The RcmServer module is used to create an server instance. RcmClients
 *  send their messages to only one RcmServer instance for processing. The
 *  server instance can be created with a function dispatch table and
 *  additional tables can be added as requested by the clients.
 *
 *  Diagnostics
 *
 *  Diags_INFO - full detail log events
 *  Diags_USER1 - message processing
 *
 *  @sa ti_grcm_RcmClient <br>
 *  @ref ti_grcm "RCM Overview"
 */

#ifndef ti_grcm_RcmServer__include
#define ti_grcm_RcmServer__include

#include <xdc/runtime/knl/GateThread.h>
#include <xdc/runtime/knl/Thread.h>


/** @ingroup ti_grcm_RcmServer */
/*@{*/

#if defined (__cplusplus)
extern "C" {
#endif


// -------- success and failure codes --------

/*!
 *  @brief Success return code
 */
#define RcmServer_S_SUCCESS (0)

/*!
 *  @brief General failure return code
 */
#define RcmServer_E_FAIL (-1)

/*!
 *  @brief The was insufficient memory left on the heap
 */
#define RcmServer_E_NOMEMORY (-2)

/*!
 *  @brief The given symbol was not found in the server's symbol table
 *
 *  This error could occur if the symbol spelling is incorrect or
 *  if the RcmServer is still loading its symbol table.
 */
#define RcmServer_E_SYMBOLNOTFOUND (-3)

/*!
 *  @brief The given symbols is in the static table, it cannot be removed
 *
 *  All symbols installed at instance create time are added to the
 *  static symbol table. They cannot be removed. The statis symbol
 *  table must remain intact for the lifespan of the server instance.
 */
#define RcmServer_E_SYMBOLSTATIC (-4)

/*!
 *  @brief The server's symbol table is full
 *
 *  The symbol table is full. You must remove some symbols before
 *  any new symbols can be added.
 */
#define RcmServer_E_SYMBOLTABLEFULL (-5)


// -------- constants and types --------

/*!
 *  @brief Remote function type
 *
 *  All functions executed by the RcmServer must be of this
 *  type. Typically, these functions are simply wrappers to the vendor
 *  function. The server invokes this remote function by passing in
 *  the RcmClient_Message.dataSize field and the address of the
 *  RcmClient_Message.data array.
 *
 *  @code
 *  RcmServer_MsgFxn fxn = ...;
 *  RcmClient_Message *msg = ...;
 *  msg->result = (*fxn)(msg->dataSize, msg->data);
 *  @endcode
 */

typedef Int32 (*RcmServer_MsgFxn)(UInt32, UInt32 *);

#if USE_MESSAGEQCOPY
/* This is a special function which gets the RcmServer handle as first parameter
 * so that OMX_GetHandle() or other creation functions can store the RcmServer
 * handle in their own context.  This is needed to allow functions to send
 * asynchrounous callbacks back to the RcmServer client.
 */
typedef Int32 (*RcmServer_MsgCreateFxn)(Void *, UInt32, UInt32 *);
#endif



/*!
 *  @brief Function descriptor
 *
 *  This function descriptor is used internally in the server. Its
 *  values are defined either at config time by the server's
 *  RcmServer_Params.fxns create parameter or at runtime through a call to
 *  RcmClient_addSymbol().
 */
typedef struct {
    /*!
     *  @brief The name of the function
     *
     *  The name is used for table lookup, it does not search the
     *  actual symbol table. You can provide any string as long as it
     *  is unique and the client uses the same name for lookup.
     */
    String name;

    /*!
     *  @brief The function address in the server's address space.
     *
     *  The server will ultimately branch to this address.
     */
#if USE_MESSAGEQCOPY
    union  {
       RcmServer_MsgFxn         fxn;
       RcmServer_MsgCreateFxn   createFxn;
    }addr;
#else
    RcmServer_MsgFxn addr;
#endif

} RcmServer_FxnDesc;

/*!
 *  @brief Function descriptor array
 */
typedef struct {
    /*!
     *  @brief The length of the array
     */
    Int length;

    /*!
     *  @brief Pointer to the array
     */
    RcmServer_FxnDesc *elem;

} RcmServer_FxnDescAry;

/*!
 *  @brief Worker pool descriptor
 *
 *  Use this data structure to define a worker pool to be created either
 *  at the server create time or dynamically at runtime.
 */
typedef struct {
    /*!
     *  @brief The name of the worker pool.
     */
    String name;

    /*!
     *  @brief The number of worker threads in the pool.
     */
    UInt count;

    /*!
     *  @brief The priority of all threads in the worker pool.
     *
     *  This value is Operating System independent. It determines the
     *  execution priority of all the worker threads in the pool.
     */
    Thread_Priority priority;

    /*!
     *  @brief The priority (OS-specific) of all threads in the worker pool
     *
     *  This value is Operating System specific. It determines the execution
     *  priority of all the worker threads in the pool. If this property is
     *  set, it takes precedence over the priority property above.
     */
    Int osPriority;

    /*!
     *  @brief The stack size in bytes of a worker thread.
     */
    SizeT stackSize;

    /*!
     *  @brief The worker thread stack placement.
     */
    String stackSeg;

} RcmServer_ThreadPoolDesc;

/*!
 *  @brief Worker pool descriptor array
 */
typedef struct {
    /*!
     *  @brief The length of the array
     */
    Int length;

    /*!
     *  @brief Pointer to the array
     */
    RcmServer_ThreadPoolDesc *elem;

} RcmServer_ThreadPoolDescAry;


/*!
 *  @brief RcmServer instance object handle
 */
typedef struct RcmServer_Object_tag *RcmServer_Handle;

/*!
 *  @brief RcmServer Instance create parameters
 */
typedef struct {
    /*!
     *  @brief Server thread priority.
     *
     *  This value is Operating System independent. It determines the execution
     *  priority of the server thread. The server thread reads the incoming
     *  message and then either executes the message function (in-band
     *  execution) or dispatches the message to a thread pool (out-of-band
     *  execution). The server thread then wait on the next message.
     */
    Thread_Priority priority;

    /*!
     *  @brief Server thread priority (OS-specific).
     *
     *  This value is Operating System specific. It determines the execution
     *  priority of the server thread. If this attribute is set, it takes
     *  precedence over the priority attribute below. The server thread reads
     *  the incoming message and then either executes the message function
     *  (in-band execution) or dispatches the message to a thread pool
     *  (out-of-band execution). The server thread then wait on the next
     *  message.
     */
    Int osPriority;

    /*!
     *  @brief The stack size in bytes of the server thread.
     */
    SizeT stackSize;

    /*!
     *  @brief The server thread stack placement.
     */
    String stackSeg;

    /*!
     *  @brief The default thread pool used for anonymous messages.
     */
    RcmServer_ThreadPoolDesc defaultPool;

    /*!
     *  @brief Array of thread pool descriptors
     *
     *  The worker pools declared with this instance parameter are statically
     *  bound to the server, they persist for the life of the server. They
     *  cannot be removed with a call to RcmServer_deletePool(). However,
     *  worker threads may be created or deleted at runtime.
     *
     */
    RcmServer_ThreadPoolDescAry workerPools;

    /*!
     *  @brief Array of function names to install into the server
     *
     *  The functions declared with this instance parameter are statically
     *  bound to the server, they persist for the life of the server. They
     *  cannot be removed with a call to RcmServer_removeSymbol().
     *
     *  To specify a function address, use the & character in the string
     *  as in the following example.
     *
     *  @code
     *  RcmServer_FxnDesc serverFxnAry[] = {
     *      { "LED_on",         LED_on  },
     *      { "LED_off",        LED_off }
     *  };
     *
     *  #define serverFxnAryLen (sizeof serverFxnAry / sizeof serverFxnAry[0])
     *
     *  RcmServer_FxnDescAry Server_fxnTab = {
     *      serverFxnAryLen,
     *      serverFxnAry
     *  };
     *
     *  RcmServer_Params rcmServerP;
     *  RcmServer_Params_init(&rcmServerP);
     *  rcmServerP.fxns.length = Server_fxnTab.length;
     *  rcmServerP.fxns.elem = Server_fxnTab.elem;
     *
     *  RcmServer_Handle rcmServerH;
     *  rval = RcmServer_create("ServerA", &rcmServerP, &(rcmServerH));
     *  @endcode
     */
    RcmServer_FxnDescAry fxns;

} RcmServer_Params;

/*!
 *  @brief Opaque client structure large enough to hold an instance object
 *
 *  Use this structure to define an embedded RcmServer object.
 *
 *  @sa RcmServer_construct
 */
typedef struct {
    GateThread_Struct   _f1;
    Ptr                 _f2;
    Ptr                 _f3;
    Ptr                 _f4;
    struct {
        Int     _f1;
        Ptr     _f2;
    }                   _f5;
    Ptr                 _f6[9];
    UInt16              _f7;
    UInt16              _f8;
    Bool                _f9;
    Int                 _f10;
    Ptr                 _f11[4];
    Ptr                 _f12;
} RcmServer_Struct;


// -------- functions --------

/*
 *  ======== RcmServer_addSymbol ========
 */
/*!
 *  @brief Add a symbol to the server's function table
 *
 *  This function adds a new symbol to the server's function table.
 *  This is useful for supporting Dynamic Load Libraries (DLLs).
 *
 *  @param[in] handle Handle to an instance object.
 *
 *  @param[in] name The function's name.
 *
 *  @param[in] addr The function's address in the server's address space.
 *
 *  @param[out] index The function's index value to be used in the
 *  RcmClient_Message.fxnIdx field.
 *
 *  @retval RcmClient_S_SUCCESS
 *  @retval RcmServer_E_NOMEMORY
 *  @retval RcmServer_E_SYMBOLTABLEFULL
 */
Int RcmServer_addSymbol(
        RcmServer_Handle        handle,
        String                  name,
        RcmServer_MsgFxn        addr,
        UInt32 *                index
    );

/*
 *  ======== RcmServer_construct ========
 */
/*!
 *  @brief Initialize a new instance object inside the provided structure
 *
 *  This function is the same as RcmServer_create() except that it does not
 *  allocate memory for the instance object. The instance object is
 *  constructed inside the provided structure. Call RcmServer_destruct()
 *  to finalize a constructed instance object.
 *
 *  @param[in] structPtr A pointer to an allocated structure.
 *
 *  @param[in] name The name of the server. The RcmClient will
 *  locate a server instance using this name. It must be unique to
 *  the system.
 *
 *  @param[in] params The create params used to customize the instance object.
 *
 *  @retval RcmClient_S_SUCCESS
 *  @retval RcmServer_E_FAIL
 *  @retval RcmServer_E_NOMEMORY
 *
 *  @sa RcmServer_create
 */
Int RcmServer_construct(
        RcmServer_Struct *      structPtr,
        String                  name,
        const RcmServer_Params *params
    );

/*
 *  ======== RcmServer_create ========
 */
/*!
 *  @brief Create an RcmServer instance
 *
 *  A server instance is used to execute functions on behalf of an
 *  RcmClient instance. There can be multiple server instances
 *  on any given CPU. The servers typically reside on a remote CPU
 *  from the RcmClient instance.
 *
 *  @param[in] name The name of the server. The RcmClient will
 *  locate a server instance using this name. It must be unique to
 *  the system.
 *
 *  @param[in] params The create params used to customize the instance object.
 *
 *  @param[out] handle An opaque handle to the created instance object.
 *
 *  @retval RcmClient_S_SUCCESS
 *  @retval RcmServer_E_FAIL
 *  @retval RcmServer_E_NOMEMORY
 */
Int RcmServer_create(
        String                  name,
        RcmServer_Params *      params,
        RcmServer_Handle *      handle
    );

/*
 *  ======== RcmServer_delete ========
 */
/*!
 *  @brief Delete an RcmServer instance
 *
 *  @param[in,out] handlePtr Handle to the instance object to delete.
 */
Int RcmServer_delete(
        RcmServer_Handle *      handlePtr
    );

/*
 *  ======== RcmServer_destruct ========
 */
/*!
 *  @brief Finalize the instance object inside the provided structure
 *
 *  @param[in] structPtr A pointer to the structure containing the
 *  instance object to finalize.
 */
Int RcmServer_destruct(
        RcmServer_Struct *      structPtr
    );

/*
 *  ======== RcmServer_exit ========
 */
/*!
 *  @brief Finalize the RcmServer module
 *
 *  This function is used to finalize the RcmServer module. Any resources
 *  acquired by RcmServer_init() will be released. Do not call any RcmServer
 *  functions after calling RcmServer_exit().
 *
 *  This function must be serialized by the caller.
 */
Void RcmServer_exit(Void);

/*
 *  ======== RcmServer_init ========
 */
/*!
 *  @brief Initialize the RcmServer module
 *
 *  This function is used to initialize the RcmServer module. Call this
 *  function before calling any other RcmServer function.
 *
 *  This function must be serialized by the caller
 */
Void RcmServer_init(Void);

/*
 *  ======== RcmServer_Params_init ========
 */
/*!
 *  @brief Initialize the instance create params structure
 */
Void RcmServer_Params_init(
        RcmServer_Params *      params
    );

/*
 *  ======== RcmServer_removeSymbol ========
 */
/*!
 *  @brief Remove a symbol and from the server's function table
 *
 *  Useful when unloading a DLL from the server.
 *
 *  @param[in] handle Handle to an instance object.
 *
 *  @param[in] name The function's name.
 *
 *  @retval RcmClient_S_SUCCESS
 *  @retval RcmServer_E_SYMBOLNOTFOUND
 *  @retval RcmServer_E_SYMBOLSTATIC
 */
Int RcmServer_removeSymbol(
        RcmServer_Handle        handle,
        String                  name
    );

/*
 *  ======== RcmServer_start ========
 */
/*!
 *  @brief Start the server
 *
 *  The server is created in stop mode. It will not start
 *  processing messages until it has been started.
 *
 *  @param[in] handle Handle to an instance object.
 *
 *  @retval RcmClient_S_SUCCESS
 *  @retval RcmServer_E_FAIL
 */
Int RcmServer_start(
        RcmServer_Handle        handle
    );


#if USE_MESSAGEQCOPY
/*
 *  ======== RcmServer_getLocalAddress ========
 */
/*!
 *  @brief Get the messageQ endpoint created for this server.
 *
 *  @param[in] handle Handle to an instance object.
 *
 *  @retval 32 bit address.
 */
UInt32 RcmServer_getLocalAddress(
       RcmServer_Handle        handle
    );

/*
 *  ======== RcmServer_getRemoteAddress ========
 */
/*!
 *  @brief Get the remote messageQ endpoint for the client of this server.
 *
 *  @param[in] handle Handle to an instance object.
 *
 *  @retval 32 bit address.
 */
UInt32 RcmServer_getRemoteAddress(
       RcmServer_Handle        handle
    );

/*
 *  ======== RcmServer_getRemoteProc ========
 */
/*!
 *  @brief Get the remote procId for the client of this server.
 *
 *  @param[in] handle Handle to an instance object.
 *
 *  @retval 32 bit address.
 */
UInt16  RcmServer_getRemoteProc(
       RcmServer_Handle        handle
    );


#endif


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

/*@}*/
#endif /* ti_grcm_RcmServer__include */
