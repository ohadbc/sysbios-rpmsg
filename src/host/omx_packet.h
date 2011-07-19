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
 * omx_packet.h
 *
 * This is the packet structure for messages to/from OMX servers created by the
 * ServiceMgr on BIOS.
 *
 */
#ifndef OMX_PACKET_H
#define OMX_PACKET_H

/*
 *  ======== OMX_Packet ========
 *
 *  OMX_Packet.desc: the package descriptor field. Note that the
 *  format is different for out-bound and in-bound messages.
 *
 *  out-bound message descriptor
 *
 *  Bits    Description
 *  --------------------------------------------------------------------
 *  [15:12] reserved
 *  [11:8]  omx message type
 *  [7:0]   omx client protocol version
 *
 *
 *  in-bound message descriptor
 *
 *  Bits    Description
 *  --------------------------------------------------------------------
 *  [15:12] reserved
 *  [11:8]  omx server status code
 *  [7:0]   omx server protocol version
 */

/* message type values */
#define OMX_DESC_MSG        0x1       // exec sync command
#define OMX_DESC_SYM_ADD    0x3       // symbol add message
#define OMX_DESC_SYM_IDX    0x4       // query symbox index
#define OMX_DESC_CMD        0x5       // exec non-blocking command.
#define OMX_DESC_TYPE_MASK  0x0F00    // field mask
#define OMX_DESC_TYPE_SHIFT 8         // field shift width

/* omx server status codes must be 0 - 15, it has to fit in a 4-bit field */
#define OMXSERVER_STATUS_SUCCESS          ((uint16_t)0) // success
#define OMXSERVER_STATUS_INVALID_FXN      ((uint16_t)1) // invalid fxn index
#define OMXSERVER_STATUS_SYMBOL_NOT_FOUND ((uint16_t)2) // symbol not found
#define OMXSERVER_STATUS_INVALID_MSG_TYPE ((uint16_t)3) // invalid msg type
#define OMXSERVER_STATUS_MSG_FXN_ERR      ((uint16_t)4) // msg function error
#define OMXSERVER_STATUS_ERROR            ((uint16_t)5) // general failure
#define OMXSERVER_STATUS_UNPROCESSED      ((uint16_t)6) // unprocessed message

/* the packet structure (actual message sent to omx service) */
typedef struct {
    uint16_t      desc;        // descriptor, and omx service status
    uint16_t      msg_id;      // id, can be used to distinguish async replies.
    uint32_t      flags;       // Set to a fixed value for now.
    uint32_t      fxn_idx;     // Index into OMX service's fxn table.
    int32_t       result;      // The OMX function's return value is here.
    uint32_t      data_size;   // Set to max size of data in and out of the fxn.
    uint32_t      data[0];     // Payload of data_size char's passed to fxn.
} omx_packet;

#define OMX_POOLID_JOBID_NONE (0x00008000)
#define OMX_INVALIDFXNIDX ((uint32_t)(0xFFFFFFFF))

#endif /* OMX_PACKET_H */
