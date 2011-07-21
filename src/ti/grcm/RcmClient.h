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
 *  ======== RcmClient.h ========
 *
 */

/*!
 *  @file ti/grcm/RcmClient.h
 *
 *  @brief Remote Command Message Client Module. An RcmClient is used
 *  for sending messages to an RcmServer for processing.
 *
 *  @sa RcmServer.h <br>
 *  @ref ti_grcm "RCM Overview"
 */

/*!
 *  @defgroup ti_grcm_RcmClient RcmClient
 *
 *  @brief Remote Command Message Client Module. An RcmClient is used
 *  for sending messages to an RcmServer for processing.
 *
 *  The RcmClient module is used to send messages to an RcmServer for
 *  processing. An RcmClient instance must be created which will
 *  communicate with a single RcmServer instance. An RcmServer instance
 *  can receive messages from many RcmClient instances but an RcmClient
 *  instance can send messages to only one RcmServer instance.
 *
 *  <a name="error_handling"></a><h2>Error Handling</h2>
 *
 *  Errors may be raised at various points in the execution flow of a
 *  single message. These are the types of errors raised by RCM:
 *  - RCM Errors
 *  - User Errors
 *  - Message Function Errors
 *  - Library Function Errors
 *
 *  The following diagram illustrates the call flow and highlights the
 *  points at which errors can be raised.
 *
 *  @image html ti/grcm/doc-files/ExecFlow.png "Diagram 1: Execution Flow"
 *
 *  RCM errors are raised by the RCM implementation. For example, when
 *  calling RcmClient_exec(), it may raise an error if it
 *  is unable to send the message. This will be reported immediately to
 *  the caller, the server is never involved. This is highlighted in the
 *  diagram at point [A].
 *
 *  RCM errors may also be raised by the RcmServer. Once RcmClient_exec()
 *  sends a message, it waits for the return message. When the server
 *  receives the message, it has to decode its type. If this fails, the
 *  server records this error in the message and sends it back to the
 *  client. This is highlighted at point [B] in the diagram. When
 *  RcmClient_exec() receives the return message, it inspects the status
 *  field and recognizes the error. It then raises an appropriate error
 *  to the caller.
 *
 *  User errors are raised by RCM as a result of an invalid runtime
 *  condition. They are typically detected and raised by the server and
 *  then sent back to the client for processing. For example, when the
 *  server receives a message, it will lookup the function index in it's
 *  table and invoke the corresponding message function. If the index is
 *  invalid, this is detected by the server and reported as an error.
 *  The message function is never invoked. The message is returned to
 *  the client and RcmClient_exec() will raise the appropriate error.
 *  This error is raised at point [B] in the diagram.
 *
 *  Message function errors are raised by the message function itself.
 *  This is highlighted at point [C] in the diagram. In the body of the
 *  message function, an error is encountered either when calling a
 *  framework function or when unmarshalling the arguments from the
 *  message payload. In both cases, the message function will abort and
 *  return an error value as its return value. The error value must be
 *  less than zero (< 0). When control returns to the server, it inspects
 *  the message function's return value. If the message was sent by
 *  RcmClient_exec(), then the server simply stores the return value in
 *  the message and returns it to the client. However, if the message was
 *  send by RcmClient_execCmd(), the behavior is slightly
 *  different. If the return value is < 0 (error condition), then the
 *  server behaves the same as for the RcmClient_exec() case. However,
 *  if there is no error, the server simply frees the message. The message
 *  is never returned to the client.
 *
 *  Library functions errors are raised by the library functions invoked
 *  by the message function, highlighted at point [D] in the diagram. It
 *  is the responsibility of the message function to detect a library
 *  function error and to marshal the appropriate error code into the
 *  message payload. The message function must also return an error value
 *  (< 0) so that server can do proper error handing as described
 *  in the preceeding paragraph. It is the stub function's responsibility
 *  to unmarshal the error value from the message payload and take the
 *  appropriate action.
 *
 *  Understanding the types of errors raised by RCM and where they are
 *  raised is important when writing the upper layer software. The following
 *  code fragment illustrates how to check for the errors discussed above.
 *  See <a href"#error_handling">Error Handling</a> above.
 *
 *  @code
 *  RcmClient_Message *msg;
 *
 *  // send message
 *  status = RcmClient_exec(h, msg, &msg);
 *
 *  // check for error
 *  switch (status) {
 *
 *      case RcmClient_S_SUCCESS:
 *          // no error, all is well
 *          break;
 *
 *      case RcmClient_E_EXECFAILED:
 *          // RCM error, unable to send message
 *          :
 *          break;
 *
 *      case RcmClient_E_INVALIDFXNIDX:
 *          // user error, bad function index
 *          :
 *          break;
 *
 *      case RcmClient_E_MSGFXNERROR:
 *          // decode message function error
 *          msgFxnErr = msg->result;
 *          :
 *          // decode library function error
 *          libFxnErr = msg->data[0];
 *          :
 *          break;
 *  }
 *
 *  // free message if one was returned
 *  if (msg != NULL) {
 *      RcmClient_free(h, msg);
 *      msg = NULL;
 *  }
 *  @endcode
 *
 *  The upper layer software begins by calling RcmClient_exec()
 *  to send the message to the server. If RCM encounters a transport error
 *  and is unable to send the message, an RcmClient_E_EXECFAILED error is
 *  returned.
 *
 *  If the server encounters an error while decoding the message, say an
 *  invalid function index, the server returns the message and an
 *  RcmClient_E_INVALIDFXNIDX error is returned.
 *
 *  If the message function or the library function encounter an error,
 *  the message is returned and an RcmClient_E_MSGFXNERROR error is
 *  returned. The upper layer software must decode the results field to
 *  see what error the message function returned. If the library function
 *  returned an error, its error code must be unmarshalled from the payload.
 *  This is illustrated by decoding the first word in the payload
 *  (msg->data[0]). The actual marshal format is implementation specific.
 *
 *  The message function must return >=0 for success, <0 for error.
 *  Here is a very simple message function which simply turns on an LED.
 *
 *  @code
 *  Int32 Led_On(UInt32 size, UInt32 *data)
 *  {
 *      Ptr led = <addr>;
 *
 *      *led = 1;  // turn on LED
 *      return(0); // success
 *  }
 *  @endcode
 *
 *  In this example, the message payload is not used. The message function
 *  does all the work, it just sets a bit in the LED control register, and
 *  returns success.
 *
 *  This next example illustrates a very simple RPC style skel function.
 *  It unmarshalls two arguments and invokes the libraray function. If the
 *  library function succeeds, the message function returns 0, otherwise it
 *  returns -1 to indicate failure. In both cases, the library function's
 *  return value is marshalled into the payload. The calling stub function
 *  can umarshal the return value to get the library function's error code.
 *
 *  @code
 *  Int32 MediaSkel_process(UInt32 size, UInt32 *data)
 *  {
 *      Int a, b, x;
 *      Int32 status = 0;  // success
 *
 *      // unmarshal args
 *      a = (Int)data[1];
 *      b = (Int)data[2];
 *
 *      // invoke library function, returns >=0 on success
 *      x = Media_process(a, b);
 *
 *      // marshal return value
 *      data[0] = (Int32)x;
 *
 *      // return status
 *      return(x >= 0 ? 0 : -1);
 *  }
 *  @endcode
 *
 *  @sa ti_grcm_RcmServer <br>
 *  @ref ti_grcm "RCM Overview"
 */

#ifndef ti_grcm_RcmClient__include
#define ti_grcm_RcmClient__include

#include <xdc/runtime/knl/GateThread.h>


/** @ingroup ti_grcm_RcmClient */
/*@{*/

#if defined (__cplusplus)
extern "C" {
#endif


// -------- status codes --------

/*!
 *  @brief Success return code
 */
#define RcmClient_S_SUCCESS (0)

/*!
 *  @brief General failure return code
 */
#define RcmClient_E_FAIL (-1)

/*!
 *  @brief The client has not been configured for asynchronous notification
 *
 *  In order to use the RcmClient_execAsync() function, the RcmClient
 *  must be configured with callbackNotification set to true in the
 *  instance create parameters.
 */
#define RcmClient_E_EXECASYNCNOTENABLED (-2)

/*!
 *  @brief The client was unable to send the command message to the server
 *
 *  An IPC transport error occurred. The message was never sent to the server.
 */
#define RcmClient_E_EXECFAILED (-3)

/*!
 *  @brief A heap id must be provided in the create params
 *
 *  When an RcmClient instance is created, a heap id must be given
 *  in the create params. This heap id must be registered with MessageQ
 *  before calling RcmClient_create().
 */
#define RcmClient_E_INVALIDHEAPID (-4)

/*!
 *  @brief Invalid function index
 *
 *  An RcmClient_Message was sent to the server which contained a
 *  function index value (in the fxnIdx field) that was not found
 *  in the server's function table.
 */
#define RcmClient_E_INVALIDFXNIDX (-5)

/*!
 *  @brief Message function error
 *
 *  There was an error encountered in either the message function or
 *  the library function invoked by the message function. The semantics
 *  of the error code are implementation dependent.
 */
#define RcmClient_E_MSGFXNERROR (-6)

/*!
 *  @brief An unknown error has been detected from the IPC layer
 *
 *  Check the error log for additional information.
 */
#define RcmClient_E_IPCERROR (-7)

/*!
 *  @brief Failed to create the list object
 */
#define RcmClient_E_LISTCREATEFAILED (-8)

/*!
 *  @brief The expected reply message from the server was lost
 *
 *  A command message was sent to the RcmServer but the reply
 *  message was not received. This is an internal error.
 */
#define RcmClient_E_LOSTMSG (-9)

/*!
 *  @brief Insufficient memory to allocate a message
 *
 *  The message heap cannot allocate a buffer of the requested size.
 *  The reported size it the requested data size and the underlying
 *  message header size.
 */
#define RcmClient_E_MSGALLOCFAILED (-10)

/*!
 *  @brief The client message queue could not be created
 *
 *  Each RcmClient instance must create its own message queue for
 *  receiving return messages from the RcmServer. The creation of
 *  this message queue failed, thus failing the RcmClient instance
 *  creation.
 */
#define RcmClient_E_MSGQCREATEFAILED (-11)

/*!
 *  @brief The server message queue could not be opened
 *
 *  Each RcmClient instance must open the server's message queue.
 *  This error is raised when an internal error occurred while trying
 *  to open the server's message queue.
 */
#define RcmClient_E_MSGQOPENFAILED (-12)

/*!
 *  @brief The server returned an unknown error code
 *
 *  The server encountered an error with the given message but
 *  the error code is not recognized by the client.
 */
#define RcmClient_E_SERVERERROR (-13)

/*!
 *  @brief The server specified in the create params was not found
 *
 *  When creating an RcmClient instance, the specified server could not
 *  be found. This could occur if the server name is incorrect, or
 *  if the RcmClient instance is created before the RcmServer. In such an
 *  instance, the client can retry when the RcmServer is expected to
 *  have been created.
 */
#define RcmClient_E_SERVERNOTFOUND (-14)

/*!
 *  @brief The given symbol was not found in the server symbol table
 *
 *  This error could occur if the symbol spelling is incorrect or
 *  if the RcmServer is still loading its symbol table.
 */
#define RcmClient_E_SYMBOLNOTFOUND (-15)

/*!
 *  @brief There is insufficient memory left in the heap
 */
#define RcmClient_E_NOMEMORY (-16)

/*!
 *  @brief The given job id was not found on the server
 *
 *  When releasing a job id with a call to RcmClient_releaseJobId(),
 *  this error return value indicates that the given job id was not
 *  previously allocated with a call to RcmClient_acquireJobId().
 */
#define RcmClient_E_JOBIDNOTFOUND (-17)


// -------- constants and types --------

/*!
 *  @brief Invalid function index
 */
#define RcmClient_INVALIDFXNIDX ((UInt32)(0xFFFFFFFF))

/*!
 *  @brief Invalid heap id
 */
#define RcmClient_INVALIDHEAPID ((UInt16)(0xFFFF))

/*!
 *  @brief Invalid message id
 */
#define RcmClient_INVALIDMSGID (0)

/*!
 *  @brief Default worker pool id
 *
 *  The default worker pool is used to process all anonymous messages.
 *  When a new message is allocated, the pool id property is
 *  initialized to this value.
 */
#define RcmClient_DEFAULTPOOLID ((UInt16)(0x8000))

/*!
 *  @brief Invalid job stream id
 *
 *  All discrete messages must have their jobId property set to this value.
 *  When a new message is allocated, the jobId property is initialized tothis value.
 */
#define RcmClient_DISCRETEJOBID (0)

/*!
 *  @brief RcmClient instance object handle
 */
typedef struct RcmClient_Object_tag *RcmClient_Handle;

/*!
 *  @brief Remote Command Message structure
 *
 *  An RcmClient needs to fill in this message before sending it
 *  to the RcmServer for execution.
 */
typedef struct {
    /*!
     *  @brief The worker pool id that will process this message.
     *
     *  The message will be processed by a worker thread from the worker
     *  pool specified in this field. The default value is the default
     *  pool id.
     */
    UInt16      poolId;

    /*!
     *  @brief The job id associated with this message.
     *
     *  All messages beloging to a job id must have this field set to
     *  that id. Use the value RcmClient_DISCRETEJOBID if the message
     *  does not belong to any job.
     */
    UInt16      jobId;

    /*!
     *  @brief The index of the remote function to execute.
     */
    UInt32      fxnIdx;

    /*!
     *  @brief The return value of the remote message function.
     */
    Int32       result;

    /*!
     *  @brief The size of the data buffer (in chars).
     *
     *  This field should be considered as read-only. It is set by
     *  the call to the RcmClient_alloc() function.
     */
    UInt32      dataSize;

    /*!
     *  @brief The data buffer containing the message payload.
     *
     *  The size of this field is dataSize chars. The space is allocated
     *  by the call to the RcmClient_alloc() function.
     */
    UInt32      data[1];

} RcmClient_Message;

/*!
 *  @brief Callback function type
 *
 *  When using callback notification, the application must supply a
 *  callback function of this type. The callback will be invoked with
 *  the pointer to the RcmClient_Message returned from the server and
 *  the application data pointer supplied in the call to RcmClient_execAsync().
 */
typedef Void (*RcmClient_CallbackFxn)(RcmClient_Message *, Ptr);

/*!
 *  @brief Instance create parameters
 */
typedef struct {
    /*!
     *  @brief The heapId used by this instance for allocating messages
     *
     *  If sending messages to a remote server, the specified heap must
     *  be compatible with the transport used for delivering messages
     *  to the remote processor.
     */
    UInt16 heapId;

    /*!
     *  @brief Asynchronous callback notification support
     *
     *  When remote functions submitted with RcmClient_execAsync()
     *  complete, the given callback function is invoked. The callback
     *  function executes in the context of this RcmClient instance's
     *  callback server thread.
     *
     *  This config param must be set to true when using RcmClient_execAsync()
     *  to execute remote functions.
     *
     *  When set to false, the callback server thread is not created.
     */
    Bool callbackNotification;

} RcmClient_Params;

/*!
 *  @brief Opaque client structure large enough to hold an instance object
 *
 *  Use this structure to define an embedded RcmClient object.
 *
 *  @sa RcmClient_construct
 */
typedef struct {
    xdc_runtime_knl_GateThread_Struct   _f1;
    Ptr                 _f2;
    Ptr                 _f3;
    UInt16              _f4;
    Ptr                 _f5;
    UInt32              _f6;
    Bool                _f7;
    UInt16              _f8;
    Ptr                 _f9;
    Ptr                 _f10;
    Ptr                 _f11;
    Ptr                 _f12;
} RcmClient_Struct;


// -------- functions --------

/*
 *  ======== RcmClient_acquireJobId ========
 */
/*!
 *  @brief Get a job id from the server
 *
 *  Acquire a unique job id from the server. The job id is used to associate
 *  messages with a common job id. The server will process all messages for
 *  a given job id in sequence.
 */
Int RcmClient_acquireJobId(
        RcmClient_Handle        handle,
        UInt16 *                jobId
    );

/*
 *  ======== RcmClient_addSymbol ========
 */
/*!
 *  @brief Add a symbol and its address to the server table
 *
 *  This function is used by the client to dynamically load a new
 *  function address into the server's function pointer table. The
 *  given address must be in the server's address space. The function
 *  must already be loaded into the server's memory.
 *
 *  This function is useful when dynamically loading code onto the
 *  remote processor (as in the case of DLL's).
 *
 *  @param[in] handle Handle to an instance object
 *
 *  @param[in] name The function's name.
 *
 *  @param[in] addr The function's address as specified in the
 *  remote processor's address space.
 *
 *  @param[out] index The function's index value to be used in the
 *  RcmClient_Message.fxnIdx field.
 *
 *  @retval RcmClient_S_SUCCESS Success
 *  @retval RcmClient_E_FAIL Failure
 */
Int RcmClient_addSymbol(
        RcmClient_Handle        handle,
        String                  name,
        Fxn                     addr,
        UInt32 *                index
    );

/*
 *  ======== RcmClient_alloc ========
 */
/*!
 *  @brief Allocate a message from the heap configured for this instance
 *
 *  When a message is allocated, the RcmClient instance is the owner
 *  of the message. All messages must be returned to the heap by
 *  calling RcmClient_free().
 *
 *  During a call to all of the exec functions, the ownership of the
 *  message is temporarily transfered to the server. If the exec
 *  function returns an RcmClient_Message pointer, then ownership of
 *  the message is returned to the instance. For the other exec
 *  functions, the client acquires ownership of the return message
 *  by calling RcmClient_waitUntilDone().
 *
 *  A message should not be accessed when ownership has been given
 *  away. Once ownership has been reacquired, the message can be
 *  either reused or returned to the heap.
 *
 *  @param[in] handle Handle to an instance object
 *
 *  @param[in] dataSize Specifies (in chars) how much space to allocate
 *  for the RcmClient_Message.data array. The actual memory allocated
 *  from the heap will be larger as it includes the size of the
 *  internal message header.
 *
 *  @param[out] message A pointer to the allocated message or NULL on error.
 */
Int RcmClient_alloc(
        RcmClient_Handle        handle,
        UInt32                  dataSize,
        RcmClient_Message **    message
    );

/*
 *  ======== RcmClient_checkForError ========
 */
/*!
 *  @brief Check if an error message has been returned from the server
 *
 *  When using RcmClient_execCmd() to send messages to the server, the
 *  message will be freed by the server unless an error occurs. In the
 *  case of an error, the message is returned to the client. Use this
 *  function to check for and to retrieve these error messages.
 *
 *  Note that the latency of the return message is dependent on many
 *  system factors. In particular, the server's response time to processing
 *  a message will be a significant factor. It is possible to call
 *  RcmClient_execCmd() several times before any error message is returned.
 *  There is no way to know when all the messages have been processed.
 *
 *  The return value of RcmClient_checkForError() is designed to mimic
 *  the return value of RcmClient_exec(). When an error message is returned
 *  to the caller, the return value of RcmClient_checkForError() will be
 *  the appropriate error code as if the error had occured during a call
 *  to RcmClient_exec(). For example, if a message is prepared with an
 *  incorrect function index, the return value from RcmClient_exec() would
 *  be RcmClient_E_INVALIDFXNIDX. However, the same message sent with
 *  RcmClient_execCmd() will not return an error, because the function does
 *  not wait for the return message. When the server receives the message
 *  and detects the function index error, it will return the message to
 *  the client on a special error queue. The subsequent call to
 *  RcmClient_checkForError() will pickup this error message and return
 *  with a status value of RcmClient_E_INVALIDFXNIDX, just as the call
 *  to RcmClient_exec() would have done.
 *
 *  A return value of RcmClient_S_SUCCESS means there are no error messages.
 *  This function will never return a message and a success status code at
 *  the same time.
 *
 *  When this function returns an error message, the caller must return
 *  the message to the heap by calling RcmClient_free().
 *
 *  It is possible that RcmClient_checkForError() will return with an error
 *  but without an error message. This can happen when an internal error
 *  occurs in RcmClient_checkForError() before it has checked the error
 *  queue. In this case, an error is returned but the returnMsg argument
 *  will be set to NULL.
 *
 *  @param[in] handle Handle to an instance object
 *
 *  @param[out] returnMsg A pointer to the error message or NULL if there
 *  are no error messages in the queue.
 *
 *  @retval RcmClient_S_SUCCESS
 *  @retval RcmClient_E_IPCERROR
 *  @retval RcmClient_E_INVALIDFXNIDX
 *  @retval RcmClient_E_MSGFXNERROR
 *  @retval RcmClient_E_SERVERERROR
 */
Int RcmClient_checkForError(
        RcmClient_Handle        handle,
        RcmClient_Message **    returnMsg
    );

/*
 *  ======== RcmClient_construct ========
 */
/*!
 *  @brief Initialize a new instance object inside the provided structure
 *
 *  This function is the same as RcmClient_create() except that it does not
 *  allocate memory for the instance object. The instance object is
 *  constructed inside the provided structure. Call RcmClient_destruct()
 *  to finalize a constructed instance object.
 *
 *  @param[in] structPtr A pointer to an allocated structure.
 *
 *  @param[in] server The name of the server that messages will be sent to for
 *  executing commands. The name must be a system-wide unique name.
 *
 *  @param[in] params The create params used to customize the instance object.
 *
 *  @sa RcmClient_create
 */
Int RcmClient_construct(
        RcmClient_Struct *      structPtr,
        String                  server,
        const RcmClient_Params *params
    );

/*
 *  ======== RcmClient_create ========
 */
/*!
 *  @brief Create an RcmClient instance
 *
 *  The RcmClient instance is used by the application to send messages to
 *  an RcmServer for executing remote functions. A given
 *  instance can send messages only to the server it was configured
 *  for. If an application needs to send messages to multiple servers,
 *  then create an RcmClient instance for each server.
 *
 *  The assigned server to this instance must already exist and be
 *  running before creating RcmClient instances which send messages to it.
 *
 *  @param[in] server The name of the server that messages will be sent to for
 *  executing commands. The name must be a system-wide unique name.
 *
 *  @param[in] params The create params used to customize the instance object.
 *
 *  @param[out] handle An opaque handle to the created instance object.
 */
Int RcmClient_create(
        String                  server,
        const RcmClient_Params *params,
        RcmClient_Handle *      handle
    );

/*
 *  ======== RcmClient_delete ========
 */
/*!
 *  @brief Delete an RcmClient instance
 *
 *  @param[in,out] handlePtr Handle to the instance object to delete.
 */
Int RcmClient_delete(
        RcmClient_Handle *      handlePtr
    );

/*
 *  ======== RcmClient_destruct ========
 */
/*!
 *  @brief Finalize the instance object inside the provided structure
 *
 *  @param[in] structPtr A pointer to the structure containing the
 *  instance object to finalize.
 */
Int RcmClient_destruct(
        RcmClient_Struct *      structPtr
    );

/*
 *  ======== RcmClient_exec ========
 */
/*!
 *  @brief Execute a command message on the server
 *
 *  The message is sent to the server for processing. This call will
 *  block until the remote function has completed. When this function
 *  returns, the message will contain the return value of the remote
 *  function as well as a possibly modified context.
 *
 *  After calling exec, the message can be either reused for another
 *  call to exec or it can be freed.
 *
 *  @param[in] handle Handle to an instance object
 *
 *  @param[in] cmdMsg Pointer to an RcmClient_Message structure.
 *
 *  @param[out] returnMsg A pointer to the return message or NULL on
 *  error. The client must free this message. The return value of the
 *  message function is stored in the results field. The return value
 *  of the library function is marshalled into the data field.
 *
 *  @retval RcmClient_S_SUCCESS
 *  @retval RcmClient_E_EXECFAILED
 *  @retval RcmClient_E_INVALIDFXNIDX
 *  @retval RcmClient_E_LOSTMSG
 *  @retval RcmClient_E_MSGFXNERROR
 *  @retval RcmClient_E_SERVERERROR
 */
Int RcmClient_exec(
        RcmClient_Handle        handle,
        RcmClient_Message *     cmdMsg,
        RcmClient_Message **    returnMsg
    );

/*
 *  ======== RcmClient_execAsync ========
 */
/*!
 *  @brief Execute a command message and use a callback for notification
 *
 *  The message is sent to the server for execution, but this call does
 *  not wait for the remote function to execute. This call returns
 *  as soon as the message has been dispatched to the transport. Upon
 *  returning from this function, the ownership of the message has been
 *  lost; do not access the message at this time.
 *
 *  When the remote function completes, the given callback function is
 *  invoked by this RcmClient instance's callback server thread. The
 *  callback function is used to asynchronously notify the client that
 *  the remote function has completed.
 *
 *  The RcmClient instance must be create with callbackNotification
 *  set to true in order to use this function.
 *
 *  @param[in] handle Handle to an instance object
 *
 *  @param[in] cmdMsg Pointer to an RcmClient_Message structure.
 *
 *  @param[in] callback A callback function pointer supplied by the
 *  application. It will be invoked by the callback server thread to
 *  notify the application that the remote function has completed.
 *
 *  @param[in] appData A private data pointer supplied by the application.
 *  This allows the application to provide its own context when
 *  receiving the callback.
 */
Int RcmClient_execAsync(
        RcmClient_Handle        handle,
        RcmClient_Message *     cmdMsg,
        RcmClient_CallbackFxn   callback,
        Ptr                     appData
    );

/*
 *  ======== RcmClient_execCmd ========
 */
/*!
 *  @brief Execute a one-way command message on the server
 *
 *  The message is sent to the server for processing but this function
 *  does not wait for the return message. This function is non-blocking.
 *  The server will processes the message and then free it, unless an
 *  error occurs. The return value from the remote function is discarded.
 *
 *  If an error occurs on the server while processing the message, the server
 *  will return the message to the client. Use RcmClient_checkForError()
 *  to collect these return error messages.
 *
 *  When this function returns, ownership of the message has been transfered
 *  to the server. Do not access the message after this function returns,
 *  it could cause cache inconsistencies or a memory access violation.
 *
 *  @param[in] handle Handle to an instance object
 *
 *  @param[in] cmdMsg Pointer to an RcmClient_Message structure.
 *
 *  @retval RcmClient_S_SUCCESS
 *  @retval RcmClient_E_IPCERROR
 */
Int RcmClient_execCmd(
        RcmClient_Handle        handle,
        RcmClient_Message *     cmdMsg
    );

/*
 *  ======== RcmClient_execDpc ========
 */
/*!
 *  @brief Execute a deferred procedure call on the server
 *
 *  The return field of the message is not used.
 *
 *  @param[in] handle Handle to an instance object
 *
 *  @param[in] cmdMsg Pointer to an RcmClient_Message structure.
 *
 *  @param[out] returnMsg A pointer to the return message or NULL
 *  on error. The client must free this message.
 */
Int RcmClient_execDpc(
        RcmClient_Handle        handle,
        RcmClient_Message *     cmdMsg,
        RcmClient_Message **    returnMsg
    );

/*
 *  ======== RcmClient_execNoWait ========
 */
/*!
 *  @brief Submit a command message to the server and return immediately
 *
 *  The message is sent to the server for execution but this call does
 *  not wait for the remote function to execute. The call returns
 *  as soon as the message has been dispatched to the transport. Upon
 *  returning from this function, the ownership of the message has been
 *  lost; do not access the message at this time.
 *
 *  Using this call to execute a remote message does not require a
 *  callback server thread. The application must call
 *  RcmClient_waitUntilDone() to get the return message from the remote
 *  function.
 *
 *  @param[in] handle Handle to an instance object
 *
 *  @param[in] cmdMsg A pointer to an RcmClient_Message structure.
 *
 *  @param[out] msgId Pointer used for storing the message id. Use
 *  the message id in a call to RcmClient_WaitUntilDone() to retrieve
 *  the return value of the remote function.
 */
Int RcmClient_execNoWait(
        RcmClient_Handle        handle,
        RcmClient_Message *     cmdMsg,
        UInt16 *                msgId
    );

/*
 *  ======== RcmClient_exit ========
 */
/*!
 *  @brief Finalize the RcmClient module
 *
 *  This function is used to finalize the RcmClient module. Any resources
 *  acquired by RcmClient_init() will be released. Do not call any RcmClient
 *  functions after calling RcmClient_exit().
 *
 *  This function must be serialized by the caller.
 */
Void RcmClient_exit(Void);

/*
 *  ======== RcmClient_free ========
 */
/*!
 *  @brief Free the given message
 *
 *  @param[in] handle Handle to an instance object
 *
 *  @param msg Pointer to an RcmClient_Message structure.
 */
Int RcmClient_free(
        RcmClient_Handle        handle,
        RcmClient_Message *     msg
    );

/*
 *  ======== RcmClient_getSymbolIndex ========
 */
/*!
 *  @brief Return the function index from the server
 *
 *  Query the server for the given function name and return its index.
 *  Use the index in the fxnIdx field of the RcmClient_Message struct.
 *
 *  @param[in] handle Handle to an instance object
 *
 *  @param[in] name The function's name.
 *
 *  @param[out] index The function's index.
 */
Int RcmClient_getSymbolIndex(
        RcmClient_Handle        handle,
        String                  name,
        UInt32 *                index
    );

/*
 *  ======== RcmClient_init ========
 */
/*!
 *  @brief Initialize the RcmClient module
 *
 *  This function is used to initialize the RcmClient module. Call this
 *  function before calling any other RcmClient function.
 *
 *  This function must be serialized by the caller
 */
Void RcmClient_init(Void);

/*
 *  ======== RcmClient_Parmas_init ========
 */
/*!
 *  @brief Initialize the instance create params structure
 */
Void RcmClient_Params_init(
        RcmClient_Params *      params
    );

/*
 *  ======== RcmClient_releaseJobId ========
 */
/*!
 *  @brief Return a job id to the server and release all resources
 *
 *  @param[in] handle Handle to an instance object
 *
 *  @param[in] jobId The job id to be released
 */
Int RcmClient_releaseJobId(
        RcmClient_Handle        handle,
        UInt16                  jobId
    );

/*
 *  ======== RcmClient_removeSymbol ========
 */
/*!
 *  @brief Remove a symbol and from the server function table
 *
 *  Useful when unloading a DLL from the server.
 *
 *  @param[in] handle Handle to an instance object
 *
 *  @param[in] name The function name.
 */
Int RcmClient_removeSymbol(
        RcmClient_Handle        handle,
        String                  name
    );

/*
 *  ======== RcmClient_waitUntilDone ========
 */
/*!
 *  @brief Block until the specified message has been executed
 *
 *  This function will wait until the remote function invoked by the
 *  specified message has completed. Upon return from this call, the
 *  message will contain the return value and the return context of
 *  the remote function.
 *
 *  @param[in] handle Handle to an instance object
 *
 *  @param[in] msgId The message ID to wait for.
 *
 *  @param[out] returnMsg A pointer to the return message or NULL
 *  on error. The client must free this message.
 */
Int RcmClient_waitUntilDone(
        RcmClient_Handle        handle,
        UInt16                  msgId,
        RcmClient_Message **    returnMsg
    );


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

/*@}*/

#endif /* ti_grcm_RcmClient__include */
