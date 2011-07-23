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
/* dlw_debug.c                                                               */
/*                                                                           */
/* This source file contains the implementation of the DLL debug support.    */
/* The client side of the ELF dynamic loader will write to target memory a   */
/* list of module debug records containing the final addresses of all        */
/* segments that were loaded to target memory by the dynamic loader.         */
/*                                                                           */
/*****************************************************************************/
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dload4430.h"
#include "ArrayList.h"
#include "symtab.h"
#include "dload_api.h"
#include "util.h"
#include "dlw_debug.h"
#include "dlw_trgmem.h"

/*****************************************************************************/
/* dl_debug                                                                  */
/*                                                                           */
/* Define a LIFO linked list "class" of DL_Module_Debug_Record pointers.     */
/*****************************************************************************/
TYPE_STACK_IMPLEMENTATION(DL_Host_Module_Debug*, dl_debug)

/*****************************************************************************/
/* mirror_debug_ptr                                                          */
/*                                                                           */
/* Define a linked list "class" of DL_Host_Module_Debug pointers.            */
/*****************************************************************************/
TYPE_QUEUE_IMPLEMENTATION(DL_Host_Module_Debug*, mirror_debug_ptr)

/*---------------------------------------------------------------------------*/
/* Global flag to control debug output.                                      */
/*---------------------------------------------------------------------------*/
#if LOADER_DEBUG
extern Bool debugging_on;
#endif

/*---------------------------------------------------------------------------*/
/* Global flag to enable profiling.                                          */
/*---------------------------------------------------------------------------*/
#if LOADER_DEBUG || LOADER_PROFILE
extern Bool profiling_on;
#endif

/*****************************************************************************/
/* DLDBG_ADD_HOST_RECORD() - Construct a host version of a debug record      */
/*   that is to be associated with the specified module.  The debug       */
/*   record is placed on the top of the "context stack" while the module  */
/*   is being loaded so that debug information about the location of the  */
/*   module segments can be added during the load.                        */
/*****************************************************************************/
void DLDBG_add_host_record(void* client_handle, const char *module_name)
{
    DLoad4430_Object *clientObj = (DLoad4430_Object *)client_handle;

    /*-----------------------------------------------------------------------*/
    /* Allocate a new DL_Host_Module_Debug record from host memory.          */
    /*-----------------------------------------------------------------------*/
    DL_Host_Module_Debug *host_dbg =
             (DL_Host_Module_Debug *)DLIF_malloc(sizeof(DL_Host_Module_Debug));
    if(!host_dbg) {
#if LOADER_DEBUG
        if(debugging_on)
            DLIF_error(DLET_MISC,"malloc failed at %d\n", __LINE__);
#endif
        exit(1);
    }

    /*-----------------------------------------------------------------------*/
    /* Set up initial values.  Make a copy of the module name; everything    */
    /* else is NULL.                                                         */
    /*-----------------------------------------------------------------------*/
    host_dbg->module_name  = (char *)DLIF_malloc(strlen(module_name) + 1);
    if(host_dbg->module_name) {
        strncpy(host_dbg->module_name, module_name, strlen(module_name));
        host_dbg->module_name[strlen(module_name)] = '\0';
    }
    host_dbg->num_segments = 0;
    host_dbg->segment_list_head =
    host_dbg->segment_list_tail = NULL;

    /*-----------------------------------------------------------------------*/
    /* Push the new host version of the debug record onto the context stack. */
    /* This module is now currently being loaded.  When its segments are     */
    /* allocated and written into target memory, the debug record at the top */
    /* of the context stack will get updated with new segment information.   */
    /*-----------------------------------------------------------------------*/
    dl_debug_push(&clientObj->dl_debug_stk, host_dbg);
}

/*****************************************************************************/
/* DLDBG_ADD_TARGET_RECORD() - Host version of the debug record on the top   */
/*   of the context stack is now complete and the module associated has      */
/*   been successfully loaded.  It is now time to create a target version    */
/*   of the debug record based on the host version.  Allocate space for      */
/*   the target record and write the information from the host version of    */
/*   record into the target memory.  The host will retain a mirror copy      */
/*   of the target debug record list so that it can update pointers in       */
/*   the list when debug records are added to or removed from the list.      */
/*****************************************************************************/
void DLDBG_add_target_record(void* client_handle, int handle)
{
    int i;
    DLoad4430_Object *clientObj = (DLoad4430_Object *)client_handle;
    DL_Host_Module_Debug *host_dbg = dl_debug_pop(&clientObj->dl_debug_stk);
    struct DLOAD_MEMORY_REQUEST targ_req;
    struct DLOAD_MEMORY_SEGMENT obj_desc;
    DL_Host_Segment *host_seg = NULL;
    DL_Target_Module_Debug *targ_dbg = NULL;
    char *targ_module_name = NULL;

    /*-----------------------------------------------------------------------*/
    /* Assign handle after loading has been completed.                       */
    /*-----------------------------------------------------------------------*/
    host_dbg->handle = handle;

    /*-----------------------------------------------------------------------*/
    /* Figure out how much target memory we need to hold all of the debug    */
    /* record information for the module that just finished loading and its  */
    /* segments.                                                             */
    /*-----------------------------------------------------------------------*/
    obj_desc.memsz_in_bytes = sizeof(DL_Target_Module_Debug) +
                 (sizeof(DL_Target_Segment) * (host_dbg->num_segments - 1)) +
                (strlen(host_dbg->module_name) + 1);

    /*-----------------------------------------------------------------------*/
    /* Build up request for target memory in a local data object.            */
    /*-----------------------------------------------------------------------*/
    targ_req.fp     = NULL;
    targ_req.align  = 4;
    targ_req.flags  = DLOAD_SF_relocatable;

    /*-----------------------------------------------------------------------*/
    /* Request the target memory for the new debug record.                   */
    /*-----------------------------------------------------------------------*/
    if (!DLTMM_malloc(client_handle, &targ_req, &obj_desc))
    {
        DLIF_error(DLET_MEMORY,
                   "Failed to allocate target memory for debug record.\n");
        exit(1);
    }

    /*------------------------------------------------------------------------*/
    /* Write content of host version of the debug record into the target      */
    /* version of the debug record in target memory.                          */
    /*------------------------------------------------------------------------*/
    targ_dbg = (DL_Target_Module_Debug *)obj_desc.target_address;
    targ_dbg->tool_version = INIT_VERSION;
    targ_dbg->verification_word = VERIFICATION;
    targ_dbg->num_segments = host_dbg->num_segments;

    for (host_seg = host_dbg->segment_list_head, i = 0;
        host_seg; host_seg = host_seg->next_segment, i++)
    {
        targ_dbg->segments[i].load_address = host_seg->load_address;
        targ_dbg->segments[i].run_address  = host_seg->run_address;
    }

    if (i != host_dbg->num_segments)
    {
        DLIF_error(DLET_MISC, "Debug record segment list mismatch.\n");
        exit(1);
    }

    targ_module_name = ((char *)obj_desc.target_address +
                        sizeof(DL_Target_Module_Debug) +
                        (sizeof(DL_Target_Segment) *
                        (host_dbg->num_segments - 1)));

    memcpy(targ_module_name, host_dbg->module_name,
           strlen(host_dbg->module_name) + 1);

    /*-----------------------------------------------------------------------*/
    /* The host will hold onto access info for all target debug records so   */
    /* the debug record list can be properly managed when adding or removing */
    /* debug records to/from the list.                                       */
    /*-----------------------------------------------------------------------*/
    host_dbg->target_address = obj_desc.target_address;
    host_dbg->next_module_ptr = (uint32_t)NULL;
    host_dbg->next_module_size = 0;

    /*-----------------------------------------------------------------------*/
    /* Link the new target debug record into the module debug list.  This    */
    /* means updating the debug record currently at the end of the list to   */
    /* point at and give the size of the new debug record.                   */
    /*-----------------------------------------------------------------------*/
    if (clientObj->mirror_debug_list.size == 0)
    {
        DL_Debug_List_Header *dbg_hdr = (DL_Debug_List_Header *)clientObj->DLModules_loc;
        dbg_hdr->first_module_ptr = (uint32_t)(obj_desc.target_address);
        dbg_hdr->first_module_size = obj_desc.memsz_in_bytes;
    }
    else
    {
        DL_Host_Module_Debug   *tail_host_dbg = clientObj->mirror_debug_list.back_ptr->value;
        DL_Target_Module_Debug *tail_targ_dbg = tail_host_dbg->target_address;

        tail_targ_dbg->next_module_ptr  =
        tail_host_dbg->next_module_ptr  = (uint32_t)(obj_desc.target_address);
        tail_targ_dbg->next_module_size =
        tail_host_dbg->next_module_size = obj_desc.memsz_in_bytes;
    }

    mirror_debug_ptr_enqueue(&clientObj->mirror_debug_list, host_dbg);

#if LOADER_DEBUG
    if (debugging_on) DLDBG_dump_mirror_debug_list(client_handle);
#endif
}

/*****************************************************************************/
/* DLDBG_RM_TARGET_RECORD() - Find the host version of the module debug      */
/*   record on the mirror DLL debug list so that we can then find the     */
/*   target version of the module debug record.  We'll unlink the target  */
/*   version of the record from the DLL debug list, free the target       */
/*   memory associated with the debug record, then finally, free the      */
/*   host version of the module debug record.                             */
/*****************************************************************************/
void DLDBG_rm_target_record(void* client_handle, int handle)
{
    DLoad4430_Object *clientObj = (DLoad4430_Object *)client_handle;
    mirror_debug_ptr_Queue_Node *prev_itr = NULL;
    mirror_debug_ptr_Queue_Node *itr = clientObj->mirror_debug_list.front_ptr;
    DL_Host_Module_Debug *prev_host_dbg = NULL;
    DL_Host_Module_Debug *host_dbg = itr->value;

    /*-----------------------------------------------------------------------*/
    /* Base Image is assumed to have handle ID == 1, it won't be on the      */
    /* DLL Debug list, so don't bother looking for it.                       */
    /*-----------------------------------------------------------------------*/
    if (handle <= 1) return;

    /*-----------------------------------------------------------------------*/
    /* Find host version of the module debug record using the module handle. */
    /*-----------------------------------------------------------------------*/
    for (; itr; itr = itr->next_ptr)
    {
        host_dbg = itr->value;
        if (host_dbg->handle == handle) break;
        prev_itr = itr;
    }

    if (!itr)
    {
        DLIF_trace("Couldn't find handle %d on debug list\n", handle);
        return;
    }

    /*-----------------------------------------------------------------------*/
    /* Unlink the target version of the module debug record from the DLL     */
    /* debug list in target memory.                                          */
    /*-----------------------------------------------------------------------*/
    /* The debug record to be removed may be in the middle or at the end of  */
    /* the DLL debug list, or ...                                            */
    /*-----------------------------------------------------------------------*/
    if (prev_itr)
    {
        DL_Target_Module_Debug *prev_targ_dbg = NULL;

        prev_host_dbg = prev_itr->value;
        prev_targ_dbg = (DL_Target_Module_Debug *)prev_host_dbg->target_address;

        prev_host_dbg->next_module_ptr =
        prev_targ_dbg->next_module_ptr = host_dbg->next_module_ptr;
        prev_host_dbg->next_module_size =
        prev_targ_dbg->next_module_size = host_dbg->next_module_size;
    }

    /*-----------------------------------------------------------------------*/
    /* The debug record could be at the front of the DLL debug list.  If so, */
    /* then we'll need to update the content of the list header object.      */
    /*-----------------------------------------------------------------------*/
    else
    {
        DL_Debug_List_Header *dbg_hdr = (DL_Debug_List_Header *)clientObj->DLModules_loc;
        dbg_hdr->first_module_ptr = host_dbg->next_module_ptr;
        dbg_hdr->first_module_size = host_dbg->next_module_size;
    }

    /*-----------------------------------------------------------------------*/
    /* Free target memory associated with the target version of the module   */
    /* debug record.                                                         */
    /*-----------------------------------------------------------------------*/
    DLTMM_free(client_handle, (char *)host_dbg->target_address);

    /*-----------------------------------------------------------------------*/
    /* Find and remove the host version of the module debug record from the  */
    /* mirror version of the DLL debug list, then free the host memory       */
    /* associated with the object.                                           */
    /*-----------------------------------------------------------------------*/
    mirror_debug_ptr_remove(&clientObj->mirror_debug_list, host_dbg);

#if LOADER_DEBUG
    if (debugging_on) DLDBG_dump_mirror_debug_list(client_handle);
#endif
}

/*****************************************************************************/
/* DLDBG_ADD_SEGMENT_RECORD() - Add a new segment record for the debug       */
/*   record for the module at the top of the context stack.               */
/*****************************************************************************/
void DLDBG_add_segment_record(void* client_handle, struct DLOAD_MEMORY_SEGMENT *obj_desc)
{
    DLoad4430_Object *clientObj = (DLoad4430_Object *)client_handle;

    /*-----------------------------------------------------------------------*/
    /* Get access to the module debug record at the top of the context stack.*/
    /*-----------------------------------------------------------------------*/
    DL_Host_Module_Debug *host_dbg = clientObj->dl_debug_stk.top_ptr->value;

    /*-----------------------------------------------------------------------*/
    /* Allocate host memory for a new segment debug record.                  */
    /*-----------------------------------------------------------------------*/
    DL_Host_Segment *host_seg =
                     (DL_Host_Segment *)DLIF_malloc(sizeof(DL_Host_Segment));
    if(!host_seg) {
#if LOADER_DEBUG
        if(debugging_on)
            DLIF_error(DLET_MISC,"malloc failed at %d\n", __LINE__);
#endif
        exit(1);
    }

    /*-----------------------------------------------------------------------*/
    /* Fill load and run address fields of new segment debug record.         */
    /*-----------------------------------------------------------------------*/
    host_seg->load_address =
    host_seg->run_address  = (uint32_t)obj_desc->target_address;
    host_seg->next_segment = NULL;

    /*-----------------------------------------------------------------------*/
    /* Add the new segment debug record to the end of the segment list that  */
    /* is attached to the module debug record.                               */
    /*-----------------------------------------------------------------------*/
    if (host_dbg->num_segments == 0)
    {
        host_dbg->segment_list_head =
        host_dbg->segment_list_tail = host_seg;
    }
    else
    {
        host_dbg->segment_list_tail->next_segment = host_seg;
        host_dbg->segment_list_tail = host_seg;
    }

    host_dbg->num_segments++;
}

/*****************************************************************************/
/* DLDBG_DUMP_MIRROR_DEBUG_LIST() - Write out contents of mirror debug list  */
/*   so that we can debug what is being written to target memory on       */
/*   behalf of the DLL View support.                                      */
/*****************************************************************************/
void DLDBG_dump_mirror_debug_list(void* client_handle)
{
    DLoad4430_Object *clientObj = (DLoad4430_Object *)client_handle;
    mirror_debug_ptr_Queue_Node *itr = clientObj->mirror_debug_list.front_ptr;

    DL_Debug_List_Header *dbg_hdr = (DL_Debug_List_Header *)clientObj->DLModules_loc;
    DLIF_trace("DLL View Debug List Header at 0x%lx\n", (unsigned long)dbg_hdr);
    DLIF_trace("   first module debug record at: 0x%lx\n",
               (unsigned long)dbg_hdr->first_module_ptr);
    DLIF_trace("   first module debug record size: %d\n",
               (int)dbg_hdr->first_module_size);

    while (itr)
    {
        int i;
        DL_Host_Module_Debug *host_dbg = itr->value;
        DL_Host_Segment *host_seg = NULL;

        DLIF_trace("Module Debug Record for %s at 0x%lx\n",
                   host_dbg->module_name,
                   (unsigned long)host_dbg->target_address);
        DLIF_trace("   next module debug record at: 0x%lx\n",
                   (unsigned long)host_dbg->next_module_ptr);
        DLIF_trace("   next module debug record size: %d\n",
                   (int)host_dbg->next_module_size);

        DLIF_trace("   handle for %s is %d\n", host_dbg->module_name,
                   host_dbg->handle);
        DLIF_trace("   segment list for %s:\n", host_dbg->module_name);
        for (i = 0, host_seg = host_dbg->segment_list_head;
             host_seg; i++, host_seg = host_seg->next_segment)
        {
            DLIF_trace("      segment [%d] load address: 0x%lx\n",
                       i, (unsigned long)host_seg->load_address);
            DLIF_trace("      segment [%d] run  address: 0x%lx\n",
                       i, (unsigned long)host_seg->run_address);
        }

        itr = itr->next_ptr;
    }
}
