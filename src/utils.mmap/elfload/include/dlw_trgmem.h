/*
 *  Syslink-IPC for TI OMAP Processors
 *
 *  Copyright (c) 2008-2010, Texas Instruments Incorporated
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  *  Neither the name of Texas Instruments Incorporated nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*****************************************************************************/
/* dlw_trgmem.h                                                              */
/*                                                                           */
/* Define internal data structures used by RIDL client side for target       */
/* memory management.                                                        */
/*****************************************************************************/
#ifndef DLW_TRGMEM_H
#define DLW_TRGMEM_H

#include "dload_api.h"

/*---------------------------------------------------------------------------*/
/* MIN_BLOCK is the minimum size of a packet.                                */
/*---------------------------------------------------------------------------*/
#define MIN_BLOCK       4

/*---------------------------------------------------------------------------*/
/* TRG_PACKET is the template for a data packet.  Packet size contains the   */
/* number of bytes allocated for the user.  Packets are always allocated     */
/* memory in MIN_BLOCK byte chunks.  The list is ordered by packet address   */
/* which refers to the target address associated with the first byte of the  */
/* packet.  The list itself is allocated out of host memory and is a doubly  */
/* linked list to help with easy splitting and merging of elements.          */
/*---------------------------------------------------------------------------*/
typedef struct _trg_packet
{
   /* Need to change this to TARGET_ADDRESS  packet_addr */
   uint32_t             packet_addr;   /* target address of packet          */
   uint32_t             packet_size;   /* number of bytes in this packet    */
   struct _trg_packet  *prev_packet;   /* prev packet in trg mem list       */
   struct _trg_packet  *next_packet;   /* next packet in trg mem list       */
   BOOL                 used_packet;   /* has packet been allocated?        */
} TRG_PACKET;

/*---------------------------------------------------------------------------*/
/* Interface into client's target memory manager.                            */
/*---------------------------------------------------------------------------*/
extern BOOL DLTMM_init(void* client_handle, uint32_t dynMemAddr, uint32_t size);
extern BOOL DLTMM_deinit(void* client_handle);
extern BOOL DLTMM_malloc(void* client_handle,
                         struct DLOAD_MEMORY_REQUEST *targ_req,
                         struct DLOAD_MEMORY_SEGMENT *obj_desc);
extern void DLTMM_free(void* client_handle, TARGET_ADDRESS ptr);

extern void DLTMM_fwrite_trg_mem(FILE *fp);
extern void DLTMM_fread_trg_mem(FILE *fp);
extern void DLTMM_dump_trg_mem(uint32_t offset, uint32_t nbytes,
                               FILE* fp);

#endif /* DLW_TRGMEM_H */
