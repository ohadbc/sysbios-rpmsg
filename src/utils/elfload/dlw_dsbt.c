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
/* dlw_dsbt.c                                                                */
/*                                                                           */
/* Implementation of client functions required by dynamic loader API         */
/* to support the Dynamic Static Base Table (DSBT) model.                    */
/* Please see list of client-required API functions in dload_api.h.          */
/*                                                                           */
/*****************************************************************************/
#include "Queue.h"
#include "ArrayList.h"
#include "dload_api.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "dlw_dsbt.h"

/*****************************************************************************/
/* DSBT_index_request_queue - This is a holding area for DSBT index requests */
/*    while allocation and relocation of symbols is in progress for the top- */
/*    level module and all of its dependents. Items will be pulled off the   */
/*    queue when we are ready to make actual DSBT index assignments in       */
/*    DLIF_assign_dsbt_indices().                                            */
/*****************************************************************************/
TYPE_QUEUE_IMPLEMENTATION(DSBT_Index_Request*, dsbt_index_request_ptr)
dsbt_index_request_ptr_Queue DSBT_index_request_queue;

/*****************************************************************************/
/* DSBT_Master - This is the master copy of the DSBT created by the client   */
/*    after all object modules have been allocated and their symbols have    */
/*    been relocated.                                                        */
/*****************************************************************************/
static int32_t DSBT_first_avail_index = 0;
Array_List DSBT_master;

#if LOADER_DEBUG
static void dump_master_dsbt(void);
#endif

/*****************************************************************************/
/* DLIF_register_dsbt_index_request()                                        */
/*                                                                           */
/*    Register a request for a DSBT index from a dynamic executable or a     */
/*    dynamic library. An executable must make a specific request for the    */
/*    0th slot in the DSBT. Dynamic libraries can make a specific index      */
/*    request or have the client assign an index on its behalf when the      */
/*    allocation and relocation of symbols is completed for a top-level      */
/*    load (load invoked by client's "load" command, for example).           */
/*                                                                           */
/*    If a specific request is made for an index that has already been       */
/*    assigned or specifically requested by an earlier request, then an      */
/*    error will be emitted and the loader core should fail the load.        */
/*                                                                           */
/*    The information provided with the request will include the requesting  */
/*    module's so_name and file handle, along with the index requested and   */
/*    the index assigned.                                                    */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/*                                                                           */
/*    It is assumed that AL_initialize has been called to set up the initial */
/*    state of the client's model of the DSBT master.                        */
/*                                                                           */
/*****************************************************************************/
BOOL DLIF_register_dsbt_index_request(DLOAD_HANDLE handle,
                                      const char *requestor_name,
                                      int32_t     requestor_file_handle,
                                      int32_t     requested_dsbt_index)
{
    DSBT_Index_Request *new_request = NULL;

    /*-----------------------------------------------------------------------*/
    /* If requesting a specific DSBT index, check existing list of DSBT index*/
    /* requests to see if we've already seen a request for the specified     */
    /* DSBT index or if a request has already been received on behalf of the */
    /* specified file. Both cases constitute an error and will abort the     */
    /* load.                                                                 */
    /*-----------------------------------------------------------------------*/
    if (requested_dsbt_index != DSBT_INDEX_INVALID)
    {
        dsbt_index_request_ptr_Queue_Node *ptr;

        /*-------------------------------------------------------------------*/
        /* If the client's master DSBT model already has content, then check */
        /* to see if the requested DSBT index is available in the master     */
        /* DSBT.                                                             */
        /*-------------------------------------------------------------------*/
            if (AL_size(&DSBT_master) > requested_dsbt_index)
            {
                DSBT_Entry *client_dsbt = (DSBT_Entry *)(DSBT_master.buf);
                if (client_dsbt[requested_dsbt_index].index_request != NULL)
                {
                    DLIF_error(DLET_MISC,
                         "%s is requesting a DSBT index, %d, that is already "
                         "being used by an active module, %s",
                         requestor_name, requested_dsbt_index,
                         client_dsbt[requested_dsbt_index].index_request->name);
                    return FALSE;
                }
            }

        for (ptr = DSBT_index_request_queue.front_ptr;
            ptr != NULL; ptr = ptr->next_ptr)
        {
            DSBT_Index_Request *existing_request = ptr->value;

            /*---------------------------------------------------------------*/
            /* Have we seen a request for this file already? That would be a */
            /* problem (likely internal).                                    */
            /*---------------------------------------------------------------*/
            if (requestor_file_handle == existing_request->file_handle)
            {
                DLIF_error(DLET_MISC,
                           "A DSBT index has already been requested on behalf "
                           "of %s; cannot make a second DSBT index request for "
                           "the same module", existing_request->name);
                return FALSE;
            }

            /*---------------------------------------------------------------*/
            /* Have we seen a specific request for this DSBT index already?  */
            /* Report a conflict among specific requests in the same load.   */
            /*---------------------------------------------------------------*/
            if (requested_dsbt_index == existing_request->requested_index)
            {
                DLIF_error(DLET_MISC,
                           "Requested DSBT index, %d, requested by %s has "
                           "already been requested by %s; load aborted",
                           requested_dsbt_index,
                           requestor_name,
                           existing_request->name);
                return FALSE;
            }
        }
    }

    /*-----------------------------------------------------------------------*/
    /* If specified module is requesting a specific DSBT index that hasn't   */
    /* been encountered yet, or if it is making a general DSBT index request */
    /* (to be assigned by the client when the current top-level load is      */
    /* sucessfully completed), make a DSBT index request entry for the       */
    /* current module and add it to the DSBT_Index_Request_List.             */
    /*-----------------------------------------------------------------------*/
    new_request = (DSBT_Index_Request *)DLIF_malloc(sizeof(DSBT_Index_Request));
    if (NULL == new_request) {
        DLIF_error(DLET_MISC,
                   "Could not allocate memory for DSBT index request");
        return FALSE;
    }
    new_request->name = (char *)DLIF_malloc(strlen(requestor_name) + 1);
    if (NULL == new_request->name) {
        DLIF_free(new_request);
        DLIF_error(DLET_MISC,
                   "Could not allocate memory for DSBT index request name");
        return FALSE;
    }
    strcpy(new_request->name, requestor_name);
    new_request->file_handle = requestor_file_handle;

    new_request->dsbt_size = DLOAD_get_dsbt_size(handle, requestor_file_handle);
    if (!DLOAD_get_dsbt_base(handle, requestor_file_handle, &new_request->dsbt_base))
    {
        DLIF_error(DLET_MISC,
                   "Could not resolve DSBT base value for %s",
                   requestor_name);
        DLIF_free(new_request->name);
        new_request->name = NULL;
        DLIF_free(new_request);
        new_request = NULL;
        return FALSE;
    }

    if (!DLOAD_get_static_base(handle, requestor_file_handle, &new_request->static_base))
    {
        DLIF_error(DLET_MISC,
                   "Could not resolve static base value for %s",
                   requestor_name);
        DLIF_free(new_request->name);
        new_request->name = NULL;
        DLIF_free(new_request);
        new_request = NULL;
        return FALSE;
    }

    new_request->requested_index = requested_dsbt_index;
    new_request->assigned_index = DSBT_INDEX_INVALID;

    dsbt_index_request_ptr_enqueue(&DSBT_index_request_queue, new_request);

    return TRUE;
}

/*****************************************************************************/
/* new_DSBT_Entry()                                                          */
/*                                                                           */
/*    Construct a DSBT_Entry data structure and initialize it with specified */
/*    DSBT_Index_Request pointer.                                            */
/*                                                                           */
/*****************************************************************************/
static void add_dsbt_entry(DSBT_Index_Request *request)
{
    DSBT_Entry new_entry;
    new_entry.index_request = request;
    AL_append(&DSBT_master, &new_entry);
}

/*****************************************************************************/
/* assign_dsbt_entry()                                                       */
/*                                                                           */
/*    Assign an entry in the client's model of the DSBT master to the        */
/*    given DSBT index request. If the DSBT master needs to grow in order    */
/*    to accommodate the request, then it will do so.                        */
/*                                                                           */
/*****************************************************************************/
static void assign_dsbt_entry(DSBT_Index_Request *request)
{
    DSBT_Entry *client_dsbt = NULL;

    /*-----------------------------------------------------------------------*/
    /* For a specific DSBT index request, assign the specified slot in the   */
    /* DSBT master to the given request. If we need to, we will grow the     */
    /* master DSBT to a size that can accommodate the specific request.      */
    /*-----------------------------------------------------------------------*/
    if (request->requested_index != DSBT_INDEX_INVALID)
    {
        while (AL_size(&DSBT_master) <= request->requested_index)
               add_dsbt_entry(NULL);

        client_dsbt = (DSBT_Entry *)(DSBT_master.buf);
        client_dsbt[request->requested_index].index_request = request;
        request->assigned_index = request->assigned_index;
    }

    /*-----------------------------------------------------------------------*/
    /* For a general DSBT index request, find the first available slot in the*/
    /* master DSBT and assign it to the request, or grow the master DSBT and */
    /* assign the new slot to the request.                                   */
    /*-----------------------------------------------------------------------*/
    else
    {
        int i;
        client_dsbt = (DSBT_Entry *)(DSBT_master.buf);
        for (i = DSBT_first_avail_index; i < AL_size(&DSBT_master); i++)
        {
            if (client_dsbt[i].index_request == NULL)
            {
                client_dsbt[i].index_request = request;
                break;
            }

            DSBT_first_avail_index++;
        }

        if (i == AL_size(&DSBT_master))
            add_dsbt_entry(request);

        request->assigned_index = i;
    }
}

/*****************************************************************************/
/* DLIF_assign_dsbt_indices()                                                */
/*                                                                           */
/*    When the core loader completes allocation of the top-level object      */
/*    being loaded and the allocation for all dependent files, this function */
/*    is called to bind objects that have just been allocated to their DSBT  */
/*    index (as determined by the client). We will first honor any specific  */
/*    index requests that have been made. Then remaining DSBT entries will   */
/*    be assigned in the order that they were encountered during the load    */
/*    to each available slot in the master DSBT.                             */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/*                                                                           */
/*    It is assumed that AL_initialize has been called to set up the initial */
/*    state of the client's model of the DSBT master.                        */
/*                                                                           */
/*    Error conditions should have been detected during registration of each */
/*    DSBT index request. I don't think there are any error/warning          */
/*    situations that need to be handled within this function.               */
/*                                                                           */
/*****************************************************************************/
void DLIF_assign_dsbt_indices(void)
{
    /*-----------------------------------------------------------------------*/
    /* Spin through DSBT index request queue, processing any specific DSBT   */
    /* index requests. If we need to grow the DSBT master model to handle a  */
    /* request, then do so.                                                  */
    /*-----------------------------------------------------------------------*/
    dsbt_index_request_ptr_Queue_Node *ptr = DSBT_index_request_queue.front_ptr;
    dsbt_index_request_ptr_Queue_Node *next_ptr = NULL;
    DSBT_Index_Request *curr_req = NULL;

    for (; ptr != NULL; ptr = next_ptr)
    {
        curr_req = ptr->value;
        next_ptr = ptr->next_ptr;

        if (curr_req->requested_index == DSBT_INDEX_INVALID) continue;

        assign_dsbt_entry(curr_req);
        dsbt_index_request_ptr_remove(&DSBT_index_request_queue, curr_req);
    }

    /*-----------------------------------------------------------------------*/
    /* Spin through what remains of the DSBT index request queue to process  */
    /* all general DSBT index requests. This time we can dequeue entries     */
    /* off the index request queue as we proceed.                            */
    /*-----------------------------------------------------------------------*/
    curr_req = dsbt_index_request_ptr_dequeue(&DSBT_index_request_queue);
    while (curr_req != NULL)
    {
        assign_dsbt_entry(curr_req);
        curr_req = dsbt_index_request_ptr_dequeue(&DSBT_index_request_queue);
    }

#if LOADER_DEBUG
    if (debugging_on)
    {
        DLIF_trace("After completed assignment of DSBT indices ...\n");
        dump_master_dsbt();
    }
#endif
}

/*****************************************************************************/
/* DLIF_get_dsbt_index()                                                     */
/*                                                                           */
/*    Find specified file handle among the list of DSBT request entries.     */
/*    Then return the DSBT index that has been assigned to that file         */
/*    handle. Emit an error if the file handle is not found among the list   */
/*    of DSBT request entries or if a DSBT assignment has not been made      */
/*    for the specified file yet.                                            */
/*                                                                           */
/*****************************************************************************/
int32_t DLIF_get_dsbt_index(int32_t file_handle)
{
    /*-----------------------------------------------------------------------*/
    /* Find specified file handle among client's model of the DSBT master.   */
    /*-----------------------------------------------------------------------*/
    int32_t i;
    DSBT_Entry *client_dsbt = (DSBT_Entry *)(DSBT_master.buf);
    for (i = 0; i < AL_size(&DSBT_master); i++)
    {
        DSBT_Index_Request *curr_req = client_dsbt[i].index_request;

        if (curr_req == NULL) continue;

        if (curr_req->file_handle == file_handle)
            return curr_req->assigned_index;
    }

    /*-----------------------------------------------------------------------*/
    /* Otherwise, we either did not find the specified file handle, or a     */
    /* valid DSBT index has not yet been assigned to the file handle.        */
    /*-----------------------------------------------------------------------*/
    return DSBT_INDEX_INVALID;
}

/*****************************************************************************/
/* DLIF_update_all_dsbts()                                                   */
/*                                                                           */
/*    Update all DSBTs for the application and all libraries that use the    */
/*    DSBT model. Each DSBT index request entry was provided with the        */
/*    address and size of the DSBT contained in the loaded application.      */
/*    The client simply needs to copy the content of its master copy of the  */
/*    DSBT to each module's own DSBT. The client will check the size of      */
/*    each module's DSBT to see if it is big enough to hold the master copy  */
/*    of the DSBT before actually copying the master to the module's DSBT.   */
/*    An error will be emitted if a module's allocated DSBT is not big       */
/*    enough to hold the master DSBT.                                        */
/*                                                                           */
/*****************************************************************************/
BOOL DLIF_update_all_dsbts()
{
    /*-----------------------------------------------------------------------*/
    /* Spin through the client's master copy of the DSBT. For each entry in  */
    /* the table:                                                            */
    /*                                                                       */
    /*    1. Check the DSBT size for the module that is associated with the  */
    /*       current slot in the DSBT to see if its DSBT size is large enough*/
    /*       to hold a copy of the master DSBT.                              */
    /*                                                                       */
    /*    2. Query the core loader for the static base value associated with */
    /*       the module that has been assigned to the current index in the   */
    /*       DSBT. This static base value is recorded in the client's DSBT   */
    /*       model.                                                          */
    /*                                                                       */
    /*    3. Query the core loader for the DSBT base value associated with   */
    /*       the module that has been assigned to the current index in the   */
    /*       master DSBT. We should only look this value up once while the   */
    /*       file is still open and its dynamic module object is still       */
    /*       available.                                                      */
    /*                                                                       */
    /*-----------------------------------------------------------------------*/
    int32_t i;
    int32_t master_dsbt_size = AL_size(&DSBT_master);
    DSBT_Entry *client_dsbt = (DSBT_Entry *)(DSBT_master.buf);
    DSBT_Index_Request *curr_req = NULL;

#if LOADER_DEBUG
    if (debugging_on)
    {
        DLIF_trace("Starting DLIF_update_all_dsbts() ... \n");
        DLIF_trace("Size of master DSBT is %d\n", master_dsbt_size);
        dump_master_dsbt();
    }
#endif

    /*-----------------------------------------------------------------------*/
    /* Spin through the master DSBT model and fill in details about the DSBT */
    /* base and the static base associated with each module that has been    */
    /* assigned a slot in the master DSBT.                                   */
    /*-----------------------------------------------------------------------*/
    for (i = 0; i < master_dsbt_size; i++)
    {
        curr_req = client_dsbt[i].index_request;

        /*-------------------------------------------------------------------*/
        /* We only need to worry about filling in details for slots that have*/
        /* actually been assigned to an object module (if this slot doesn't  */
        /* have a DSBT index request record associated with it, then it is   */
        /* "available").                                                     */
        /*-------------------------------------------------------------------*/
        if (curr_req != NULL)
        {
            /*---------------------------------------------------------------*/
            /* If the DSBT size has not been filled in for the module that   */
            /* is assigned to this slot, look it up in the local symbol      */
            /* table of the module. We have to do this while the dynamic     */
            /* module object for the module is still open (it has a copy of  */
            /* the local symbol table).                                      */
            /*---------------------------------------------------------------*/
            uint32_t curr_dsbt_size   = curr_req->dsbt_size;
            if (curr_dsbt_size < master_dsbt_size)
            {
                DLIF_error(DLET_MISC,
                           "DSBT allocated for %s is not large enough to hold "
                           "entire DSBT", curr_req->name);
                return FALSE;
            }
        }
    }

    /*-----------------------------------------------------------------------*/
    /* Now write a copy of the DSBT for each module that uses the DSBT model.*/
    /* We need to find the DSBT base for each module represented in the      */
    /* master DSBT, then we can write the content of the master DSBT to each */
    /* DSBT base location.                                                   */
    /*-----------------------------------------------------------------------*/
    for (i = 0; i < master_dsbt_size; i++)
    {
        curr_req = client_dsbt[i].index_request;

        /*-------------------------------------------------------------------*/
        /* Write content of master DSBT to location of module's DSBT.        */
        /*-------------------------------------------------------------------*/
        if (curr_req != NULL)
        {
            int j;
#if LOADER_DEBUG
            if (debugging_on)
                DLIF_trace("Writing master DSBT to 0x%08lx for module: %s\n",
                           curr_req->dsbt_base, curr_req->name);
#endif

            for (j = 0; j < master_dsbt_size; j++)
            {
                DSBT_Index_Request *j_req = client_dsbt[j].index_request;

                if (j_req != NULL)
                    *((TARGET_ADDRESS *)(curr_req->dsbt_base) + j) =
                                      (j_req != NULL) ? j_req->static_base : 0;
            }
        }
    }

#if LOADER_DEBUG
    if (debugging_on) dump_master_dsbt();
#endif

    return TRUE;
}

/*****************************************************************************/
/* dump_master_dsbt()                                                        */
/*****************************************************************************/
#if LOADER_DEBUG
static void dump_master_dsbt(void)
{
   int i;
   DSBT_Entry *client_dsbt = (DSBT_Entry *)(DSBT_master.buf);
   DLIF_trace("Dumping master DSBT ...\n");
   for (i = 0; i < AL_size(&DSBT_master); i++)
   {
      DSBT_Index_Request *i_req = client_dsbt[i].index_request;
      if (i_req != NULL)
      {
         DLIF_trace("  slot %d has dsbt_base: 0x%08lx; static base: 0x%08lx;\n"
                "  index request from: %s\n",
                i, i_req->dsbt_base, i_req->static_base, i_req->name);
      }
      else
      {
         DLIF_trace("  slot %d is AVAILABLE\n", i);
      }
   }
}
#endif

/*****************************************************************************/
/* DSBT_release_entry()                                                      */
/*                                                                           */
/*    Once a file is unloaded from the target, make its DSBT entry in the    */
/*    master DSBT available to objects that may be subsequently loaded. If   */
/*    we don't find the file handle among the master DSBT, then we assume    */
/*    that the file does not use the DSBT model.                             */
/*                                                                           */
/*****************************************************************************/
void DSBT_release_entry(int32_t file_handle)
{
    int32_t i;
    DSBT_Entry *client_dsbt = (DSBT_Entry *)(DSBT_master.buf);
    for (i = 0; i < AL_size(&DSBT_master); i++)
    {
        DSBT_Index_Request *curr_req = client_dsbt[i].index_request;

        if (curr_req && (curr_req->file_handle == file_handle))
        {
            client_dsbt[i].index_request = NULL;
            if (i < DSBT_first_avail_index) DSBT_first_avail_index = i;
            DLIF_free(curr_req);
        }
    }
}
