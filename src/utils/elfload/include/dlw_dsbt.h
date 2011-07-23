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
/* dlw_dsbt.h                                                                */
/*                                                                           */
/* Define internal data structures used by RIDL client side for              */
/* Dynamic Static Base Table (DSBT) support.                                 */
/*****************************************************************************/
#ifndef DLW_DSBT_H
#define DLW_DSBT_H

#include "Queue.h"
#include "dload_api.h"

/*---------------------------------------------------------------------------*/
/* DSBT_Index_Request - Representation of a request for a DSBT index on      */
/*    behalf of an executable or library. This data structure will reserve   */
/*    space for the specified module in the client's model of the master     */
/*    DSBT.                                                                  */
/*---------------------------------------------------------------------------*/
typedef struct {
   char           *name;
   int32_t         file_handle;
   uint32_t        dsbt_size;
   TARGET_ADDRESS  dsbt_base;
   TARGET_ADDRESS  static_base;
   int32_t         requested_index;
   int32_t         assigned_index;
} DSBT_Index_Request;

/*---------------------------------------------------------------------------*/
/* DSBT_Entry - Representation of one slot in the client's model of the      */
/*    master DSBT. The model is built up when DSBT indices are assigned.     */
/*    The content of the master DSBT is propagated to each module who uses   */
/*    the DSBT model.                                                        */
/*---------------------------------------------------------------------------*/
typedef struct {
   DSBT_Index_Request *index_request;
} DSBT_Entry;

/*---------------------------------------------------------------------------*/
/* DSBT_index_request_queue - This is a holding area for DSBT index requests */
/*    while allocation and relocation of symbols is in progress for the top- */
/*    level module and all of its dependents. Items will be pulled off the   */
/*    queue when we are ready to make actual DSBT index assignmments.        */
/*---------------------------------------------------------------------------*/
TYPE_QUEUE_DEFINITION(DSBT_Index_Request*, dsbt_index_request_ptr)
extern dsbt_index_request_ptr_Queue DSBT_index_request_queue;

/*---------------------------------------------------------------------------*/
/* DSBT_master - client's model of DSBT master. Vector of DSBT_Entry ptrs.   */
/*---------------------------------------------------------------------------*/
extern Array_List DSBT_master;

/*---------------------------------------------------------------------------*/
/* DSBT_release_entry()                                                      */
/*                                                                           */
/*    Release an entry in the client's model of the master DSBT. This        */
/*    happens when a module is unloaded from the target. The slot will       */
/*    remain a part of the DSBT, but it will become available to other       */
/*    objects that are subsequently loaded.                                  */
/*---------------------------------------------------------------------------*/
extern void DSBT_release_entry(int32_t file_handle);

#endif
