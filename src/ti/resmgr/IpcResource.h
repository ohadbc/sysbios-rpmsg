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
 *  @file       IpcResource.h
 *  ============================================================================
 */

#ifndef ti_IpcResource__include
#define ti_IpcResource__include

#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Structures & Definitions
 * =============================================================================
 */

/*!
 *  @brief      Used as the timeout value to specify wait forever
 */
#define IpcResource_FOREVER                 ~(0)

/*!
   *  @def    IpcResource_S_SUCCESS
 *  @brief  Operation is successful.
 */
#define IpcResource_S_SUCCESS               0

/*!
 *  @def    IpcResource_E_FAIL
 *  @brief  Operation is not successful.
 */
#define IpcResource_E_FAIL                  -1

/*!
 *  @def    IpcResource_E_MEMORY
 *  @brief  Operation resulted in memory failure.
 */
#define MessageQCopy_E_MEMORY               -3

/*!
 *  @def    IpcResource_E_TIMEOUT
 *  @brief  Operation timed out.
 */
#define IpcResource_E_TIMEOUT               -6

/*!
 *  @def    IpcResource_E_INVALARGS
 *  @brief  Invalid arguments.
 */
#define IpcResource_E_INVALARGS             -7

/*!
 *  @def    IpcResource_E_NOCONN
 *  @brief  No connection
 */
#define IpcResource_E_NOCONN                -8

/*!
 *  @def    IpcResource_E_RETMEMORY
 *  @brief  No memory at remote side
 */
#define IpcResource_E_RETMEMORY             -9

/*!
 *  @def    IpcResource_E_BUSY
 *  @brief  Resource is busy
 */
#define IpcResource_E_BUSY                  -10

/*!
 *  @def    IpcResource_E_NORESOURCE
 *  @brief  Resource does not exist
 */
#define IpcResource_E_NORESOURCE            -11

/*!
 *  @def    MAX_NUM_SDMA_CHANNELS
 *  @brief  Max number of channels can be requested
 */
#define MAX_NUM_SDMA_CHANNELS               16

#define IpcResource_C_FREQ                  0x1
#define IpcResource_C_LAT                   0x2
#define IpcResource_C_BW                    0x4

typedef enum {
    IpcResource_TYPE_GPTIMER,
    IpcResource_TYPE_IVAHD,
    IpcResource_TYPE_IVASEQ0,
    IpcResource_TYPE_IVASEQ1,
    IpcResource_TYPE_L3BUS,
    IpcResource_TYPE_ISS,
    IpcResource_TYPE_FDIF,
    IpcResource_TYPE_SL2IF,
    IpcResource_TYPE_AUXCLK,
    IpcResource_TYPE_REGULATOR,
    IpcResource_TYPE_GPIO,
    IpcResource_TYPE_SDMA,
    IpcResource_TYPE_IPU,
    IpcResource_TYPE_DSP
} IpcResource_Type;


typedef struct {
    UInt32 id;
    UInt32 srcClk;
} IpcResource_Gpt;

typedef struct {
    UInt32 clkId;
    UInt32 clkRate;
    UInt32 parentSrcClk;
    UInt32 parentSrcClkRate;
} IpcResource_Auxclk;

typedef struct {
    UInt32 regulatorId;
    UInt32 minUV;
    UInt32 maxUV;
} IpcResource_Regulator;

typedef struct {
    UInt32 gpioId;
} IpcResource_Gpio;

typedef struct {
    UInt32 numCh;
    Int32 channels[MAX_NUM_SDMA_CHANNELS];
} IpcResource_Sdma;

typedef struct {
    Int32 mask;
    Int32 frequency;
    Int32 bandwidth;
    Int32 latency;
} IpcResource_ConstraintData;

/*!
 *  @brief  IpcResource_Object type
 */
typedef struct IpcResource_Object *IpcResource_Handle;

typedef Int32 IpcResource_ResHandle;

/* =============================================================================
 *  IpcResource Functions:
 * =============================================================================
 */

IpcResource_Handle IpcResource_connect(UInt timeout);
Int IpcResource_request(IpcResource_Handle handle,
                        IpcResource_ResHandle *resHandle,
                        IpcResource_Type type, Void *resParams);
Int IpcResource_release(IpcResource_Handle handle,
                        IpcResource_ResHandle resHandle);
Int IpcResource_requestConstraints(IpcResource_Handle handle,
                                   IpcResource_ResHandle resHandle,
                                   Void *constraints);
Int IpcResource_releaseConstraints(IpcResource_Handle handle,
                                   IpcResource_ResHandle resHandle,
                                   Void *constraints);
Int IpcResource_disconnect(IpcResource_Handle handle);

#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */
#endif /* ti_ipc_IpcResource__include */
