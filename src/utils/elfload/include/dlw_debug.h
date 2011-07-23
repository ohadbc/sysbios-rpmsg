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
/* dlw_debug.h                                                               */
/*                                                                           */
/* Define internal data structures used by DLW client side for DLL debug.    */
/*****************************************************************************/
#ifndef DLW_DEBUG_H
#define DLW_DEBUG_H

#include "dload_api.h"
#include "Queue.h"
#include "Stack.h"

/*---------------------------------------------------------------------------*/
/* DLL Debug Support                                                         */
/*                                                                           */
/* A host copy of the DLL View debug support data structure that will be     */
/* written into target memory to assist the debugger with mapping symbol     */
/* definitions to their dynamically loaded location in target memory.        */
/*---------------------------------------------------------------------------*/
#define INIT_VERSION    2
#define VERIFICATION    0x79

/*---------------------------------------------------------------------------*/
/* DL_Debug_List_Header - Address of this structure in target memory is      */
/*      recorded in DLModules symbol.  Provides directions to first          */
/*      DL_Module_Record in the list.                                        */
/*---------------------------------------------------------------------------*/
typedef struct {
   uint32_t     first_module_ptr;
   uint16_t     first_module_size;
   uint16_t     update_flag;
} DL_Debug_List_Header;

/*---------------------------------------------------------------------------*/
/* DL_Segment - Debug information recorded about each segment in a module.   */
/*---------------------------------------------------------------------------*/
typedef struct {
   uint32_t     load_address;
   uint32_t     run_address;
} DL_Target_Segment;

typedef struct _DL_Host_Segment {
   uint32_t                      load_address;
   uint32_t                      run_address;
   struct _DL_Host_Segment      *next_segment;
} DL_Host_Segment;

/*---------------------------------------------------------------------------*/
/* DL_Module_Debug_Record - Debug information about each module that has     */
/*      been loaded.                                                         */
/*---------------------------------------------------------------------------*/
/* We have a host version of the debug record which is built up while the    */
/* module is being loaded, and a target version of the debug record which    */
/* is built after the load has completed and we know all of the information  */
/* that needs to be written to target memory for this module.                */
/*---------------------------------------------------------------------------*/
typedef struct {
   int                   handle;
   char                 *module_name;
   TARGET_ADDRESS        target_address;
   uint32_t              next_module_ptr;
   uint16_t              next_module_size;
   int                   num_segments;
   DL_Host_Segment      *segment_list_head;
   DL_Host_Segment      *segment_list_tail;
} DL_Host_Module_Debug;

typedef struct {
   uint32_t             next_module_ptr;
   uint16_t             next_module_size;
   uint16_t             tool_version;
   uint16_t             verification_word;
   uint16_t             num_segments;
   uint32_t             timestamp;
   DL_Target_Segment    segments[1];
} DL_Target_Module_Debug;

/*---------------------------------------------------------------------------*/
/* DLL Debug Support - context stack of module records.  We need to maintain */
/*      a stack of modules while creating the DLL module record list for     */
/*      debug support.  While we are building up a module record for one     */
/*      module, the loader may be asked to load dependent modules.  Note     */
/*      that we cannot emit a module record to target memory until loading   */
/*      of the module has been completed (need to know how many segments     */
/*      are in the module before we can allocate target memory for the       */
/*      module record).                                                      */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* dl_debug                                                                  */
/*                                                                           */
/* Define a LIFO linked list "class" of DL_Module_Debug_Record pointers.     */
/*---------------------------------------------------------------------------*/
TYPE_STACK_DEFINITION(DL_Host_Module_Debug*, dl_debug)

/*---------------------------------------------------------------------------*/
/* mirror_debug_ptr                                                          */
/*                                                                           */
/* Define a linked list "class" of DL_Host_Module_Debug pointers.            */
/*---------------------------------------------------------------------------*/
TYPE_QUEUE_DEFINITION(DL_Host_Module_Debug*, mirror_debug_ptr)

/*---------------------------------------------------------------------------*/
/* Management functions for DLL debug support data structures.               */
/*---------------------------------------------------------------------------*/
extern void     DLDBG_add_host_record(void* client_handle,
                                      const char *module_name);
extern void     DLDBG_add_target_record(void* client_handle,
                                        int handle);
extern void     DLDBG_rm_target_record(void* client_handle,
                                       int handle);
extern void     DLDBG_add_segment_record(void* client_handle,
                                         struct DLOAD_MEMORY_SEGMENT *obj_desc);
extern void     DLDBG_dump_mirror_debug_list(void* client_handle);
#endif
