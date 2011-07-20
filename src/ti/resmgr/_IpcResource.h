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
 *  @file       _IpcResource.h
 *  ============================================================================
 */

#ifndef ti__IpcResource__include
#define ti__IpcResource__include

#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Structures & Definitions
 * =============================================================================
 */

/*
 * Linux error codes that can be returned by
 * RM running on A9 side
 */
#define ENOENT                              2
#define ENOMEM                              12
#define EBUSY                               16
#define EINVAL                              22

typedef enum {
    IpcResource_REQ_TYPE_CONN,
    IpcResource_REQ_TYPE_ALLOC,
    IpcResource_REQ_TYPE_FREE,
    IpcResource_REQ_TYPE_DISCONN,
    IpcResource_REQ_TYPE_REQ_CONSTRAINTS,
    IpcResource_REQ_TYPE_REL_CONSTRAINTS
} IpcResource_ReqType;

typedef struct {
    UInt32 resType;
    UInt32 reqType;
    UInt32 resHandle;
    Char   resParams[];
} IpcResource_Req;

typedef struct {
    UInt32 status;
    UInt32 resType;
    UInt32 resHandle;
    UInt32 base;
    Char   resParams[];
} IpcResource_Ack;

/*!
 *  @brief  IpcResource_Object type
 */
struct IpcResource_Object {
    UInt32 endPoint;
    UInt timeout;
    MessageQCopy_Handle msgq;
};

#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */
#endif /* ti_ipc__IpcResource__include */
