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
/* dload.c                                                                   */
/*                                                                           */
/* Core Dynamic Loader reference implementation.                             */
/*                                                                           */
/* This implementation of the core dynamic loader is platform independent,   */
/* but it is object file format dependent.  In particular, this              */
/* implementation supports ELF object file format.                           */
/*****************************************************************************/

#include <limits.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ArrayList.h"
#include "Queue.h"

#include "symtab.h"
#include "dload_endian.h"
#include "elf32.h"
#include "dload.h"
#include "relocate.h"
#include "dload_api.h"

#ifdef ARM_TARGET
#include "arm_dynamic.h"
#endif

#ifdef C60_TARGET
#include "c60_dynamic.h"
#endif

/*---------------------------------------------------------------------------*/
/* Contains objects (type DLIMP_Loaded_Module) that the system has loaded    */
/* into target memory.                                                       */
/*---------------------------------------------------------------------------*/
TYPE_QUEUE_IMPLEMENTATION(DLIMP_Loaded_Module*, loaded_module_ptr)

/*---------------------------------------------------------------------------*/
/* Global flag to control debug output.                                      */
/*---------------------------------------------------------------------------*/
#if LOADER_DEBUG
Bool debugging_on = 1;
#endif


/*---------------------------------------------------------------------------*/
/* Global flag to enable profiling.                                          */
/*---------------------------------------------------------------------------*/
#if LOADER_DEBUG || LOADER_PROFILE
Bool profiling_on = 0;
#endif

#if LOADER_DEBUG || LOADER_PROFILE
int DLREL_relocations;
time_t DLREL_total_reloc_time;
#endif


/*---------------------------------------------------------------------------*/
/* Dependency Graph Queue - FIFO queue of dynamic modules that are loaded    */
/* when client asks to load a dynamic executable or library. Note that       */
/* dependents that have already been loaded with another module will not     */
/* appear on this queue.                                                     */
/*---------------------------------------------------------------------------*/
TYPE_STACK_IMPLEMENTATION(DLIMP_Dynamic_Module*, dynamic_module_ptr)

/*---------------------------------------------------------------------------*/
/* Support for profiling performance of dynamic loader core.                 */
/*---------------------------------------------------------------------------*/
#if LOADER_PROFILE || LOADER_DEBUG
static clock_t cycle0 = 0;
static clock_t cycle_end = 0;
#define profile_start_clock() (cycle0 = clock())
#define profile_stop_clock()  (cycle_end = clock())
#define profile_cycle_count() (cycle_end - cycle0)
#endif

/*---------------------------------------------------------------------------*/
/* DLOAD_create()                                                            */
/*                                                                           */
/*    Create an instance of the dynamic loader core.                         */
/*                                                                           */
/*    client_handle:  Private client token to be returned during select DLIF */
/*                   function calls.                                         */
/*                                                                           */
/*    returns: an opaque DLOAD core loader handle, identifying this instance.*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
DLOAD_HANDLE  DLOAD_create(void * client_handle)
{
    LOADER_OBJECT     * pLoaderObject;

    pLoaderObject = DLIF_malloc(sizeof(LOADER_OBJECT));

    /* Fill out the Loader Object: */
    if (pLoaderObject != NULL) {
        /*-------------------------------------------------------------------*/
        /* Set up initial objects_loading queue.                             */
        /*-------------------------------------------------------------------*/
        AL_initialize(&(pLoaderObject->DLIMP_module_dependency_list),
                      sizeof (const char*), 1);

        /* Initialize Loaded Module Ptr Queue */
        loaded_module_ptr_initialize_queue(&pLoaderObject->DLIMP_loaded_objects);

        /* Initialize Dynamic Module Ptr Stack */
        dynamic_module_ptr_initialize_stack(&pLoaderObject->DLIMP_dependency_stack);

        pLoaderObject->file_handle = 1;

        pLoaderObject->DLOAD_TARGET_MACHINE = DLOAD_DEFAULT_TARGET_MACHINE;

        /* Store client token, so it can be handed back during DLIF calls */
        pLoaderObject->client_handle = client_handle;
    }

    return((DLOAD_HANDLE)pLoaderObject);
}

/*---------------------------------------------------------------------------*/
/* DLOAD_destroy()                                                           */
/*                                                                           */
/*    Remove an instance of the dynamic loader core, and free all resources  */
/*    allocated during DLOAD_create().                                       */
/*                                                                           */
/*    client_handle:  Private client token to be returned during select DLIF */
/*                   function calls.                                         */
/*    Preconditions: 1) handle must be valid.                                */
/*                   2) Loader instance must be in "UNLOADED" state.         */
/*                                                                           */
/*---------------------------------------------------------------------------*/
void  DLOAD_destroy(DLOAD_HANDLE handle)
{
    LOADER_OBJECT     * pLoaderObject;

    pLoaderObject = (LOADER_OBJECT *)handle;

    AL_destroy(&(pLoaderObject->DLIMP_module_dependency_list));

    /* Free the instance object */
    DLIF_free (pLoaderObject);
}

/*****************************************************************************/
/* DLIMP_get_first_dyntag()                                                  */
/*                                                                           */
/*    Return value for first tag entry in the given dynamic table whose      */
/*    tag type matches the given key.                                        */
/*                                                                           */
/*****************************************************************************/
uint32_t DLIMP_get_first_dyntag(int tag, struct Elf32_Dyn* dyn_table)
{
    /*-----------------------------------------------------------------------*/
    /* Spin through dynamic segment looking for a specific dynamic tag.      */
    /* Return the value associated with the tag, if the tag is found.        */
    /*-----------------------------------------------------------------------*/
    struct Elf32_Dyn *dtp = dyn_table;
    while (dtp->d_tag != DT_NULL)
    {
        if (dtp->d_tag == tag) return dtp->d_un.d_val;
        else dtp++;
    }

    /*-----------------------------------------------------------------------*/
    /* Tag wasn't found, return a known bogus value for the tag.             */
    /*-----------------------------------------------------------------------*/
    return INT_MAX;
}

/*****************************************************************************/
/* dload_and_allocate_dependencies()                                         */
/*                                                                           */
/*    If not already loaded, load each dependent file identified in the      */
/*    dynamic segment with a DT_NEEDED tag.  Dependent files are listed in   */
/*    order and should be loaded in the same order that they appear in the   */
/*    dynamic segment.                                                       */
/*                                                                           */
/*****************************************************************************/
static BOOL dload_and_allocate_dependencies(DLOAD_HANDLE handle,
                                            DLIMP_Dynamic_Module *dyn_module)
{
    /*-----------------------------------------------------------------------*/
    /* Spin through each dynamic tag entry in the dynamic segment.           */
    /*-----------------------------------------------------------------------*/
    struct Elf32_Dyn* dyn_nugget = dyn_module->dyntab;
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;

#if LOADER_DEBUG
    if (debugging_on)
        DLIF_trace("Starting dload_and_allocate_dependencies() for %s ...\n",
                   dyn_module->name);
#endif

    while(dyn_nugget->d_tag != DT_NULL)
    {
        /*-------------------------------------------------------------------*/
        /* For each DT_NEEDED dynamic tag that we find in the dynamic        */
        /* segment, load the dependent file identified by the so_name value  */
        /* attached to the DT_NEEDED dynamic tag.                            */
        /*-------------------------------------------------------------------*/
        if (dyn_nugget->d_tag == DT_NEEDED)
        {
            loaded_module_ptr_Queue_Node* ptr;

#if LOADER_DEBUG
            if (debugging_on)
                DLIF_trace("Found DT_NEEDED: %s\n",
                           dyn_module->strtab+dyn_nugget->d_un.d_val);
#endif

            /*---------------------------------------------------------------*/
            /* Find out if the file named by the DT_NEEDED tag has already   */
            /* been loaded.  If it has, then we only have to bump the use    */
            /* count of the named dependent file.                            */
            /*---------------------------------------------------------------*/
            for (ptr = pHandle->DLIMP_loaded_objects.front_ptr; ptr != NULL;
                 ptr = ptr->next_ptr)
            {
                if (!strcmp(ptr->value->name,
                            dyn_module->strtab + dyn_nugget->d_un.d_val))
                {
                    ptr->value->use_count++;
                    AL_append(&(dyn_module->loaded_module->dependencies),
                              &(ptr->value->file_handle));
                    break;
                }
            }

            /*---------------------------------------------------------------*/
            /* If the named dependent file has not been loaded, then we ask  */
            /* the client to invoke a load of the dependent file on our      */
            /* behalf.                                                       */
            /*---------------------------------------------------------------*/
            if (ptr == NULL)
            {
                int32_t dependent_handle = DLIF_load_dependent(
                                                       pHandle->client_handle,
                                                       dyn_module->strtab +
                                                       dyn_nugget->d_un.d_val);
                AL_append(&(dyn_module->loaded_module->dependencies),
                          &dependent_handle);
                if (dependent_handle == 0) return FALSE;
            }
        }

        dyn_nugget++;
    }

#if LOADER_DEBUG
    if (debugging_on)
        DLIF_trace("Finished dload_and_allocate_dependencies() for %s\n",
                   dyn_module->name);
#endif

   return TRUE;
}

/*****************************************************************************/
/* load_object()                                                             */
/*                                                                           */
/*    Finish the process of loading an object file.                          */
/*                                                                           */
/*****************************************************************************/
static int load_object(LOADER_FILE_DESC *fd, DLIMP_Dynamic_Module *dyn_module)
{
    /*-----------------------------------------------------------------------*/
    /* With the dynamic loader already running on the target, we are able to */
    /* relocate directly into target memory, so there is nothing more to be  */
    /* done (at least in the bare-metal dynamic linking ABI model).          */
    /*-----------------------------------------------------------------------*/
    return 1;
}

/*****************************************************************************/
/* initialize_loaded_module()                                                */
/*                                                                           */
/*    Initialize DLIMP_Loaded_Module internal data object associated with a  */
/*    dynamic module.  This function will also set up a queue of             */
/*    DLIMP_Loaded_Segment(s) associated with the loaded module.             */
/*    This function is called as we are getting ready to actually load the   */
/*    object file contents into target memory.  Each segment will get a      */
/*    target memory request that it can use to ask the client for target     */
/*    memory space.  This function will also assign a file handle to the     */
/*    loaded module.                                                         */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/*                                                                           */
/* In applications that use the DSBT model, this function will also need to  */
/* negotiate the module's DSBT index with the client.                        */
/*                                                                           */
/*****************************************************************************/
static void initialize_loaded_module(DLOAD_HANDLE handle,
                                     DLIMP_Dynamic_Module *dyn_module)
{
    int i;
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;

    /*-----------------------------------------------------------------------*/
    /* Allocate a DLIMP_Loaded_Module data structure for the specified ELF   */
    /* file and assign a file handle for it (bumping the file handle counter */
    /* as we go).                                                            */
    /*-----------------------------------------------------------------------*/
    DLIMP_Loaded_Module *loaded_module =
          dyn_module->loaded_module = DLIF_malloc(sizeof(DLIMP_Loaded_Module));

   if(loaded_module == NULL) {
#if LOADER_DEBUG || LOADER_PROFILE
      if(debugging_on)
         DLIF_error(DLET_MISC, "Error allocating memory %d...\n",__LINE__);
#endif
      exit(1);
   }
#if LOADER_DEBUG || LOADER_PROFILE
    /*-----------------------------------------------------------------------*/
    /* Start clock on initialization of loaded module object.                */
    /*-----------------------------------------------------------------------*/
    if (debugging_on || profiling_on)
    {
        DLIF_trace("Starting initialize_loaded_module() ...\n");
        if (profiling_on) profile_start_clock();
    }
#endif

    loaded_module->name = DLIF_malloc(strlen(dyn_module->name) + 1);
    if (NULL == loaded_module->name) {
        DLIF_error(DLET_MISC, "Error allocating memory %d...\n",__LINE__);
        exit(1);
    }
    strcpy(loaded_module->name, dyn_module->name);

    loaded_module->file_handle = pHandle->file_handle++;
    loaded_module->direct_dependent_only = dyn_module->direct_dependent_only;
    loaded_module->use_count = 1;

    /*-----------------------------------------------------------------------*/
    /* In case we wrapped around the file handle, return error.              */
    /*-----------------------------------------------------------------------*/
    if (pHandle->file_handle == 0)
        DLIF_error(DLET_MISC, "DLOAD File handle overflowed.\n");

    /*-----------------------------------------------------------------------*/
    /* Initially the loaded module does not have access to its global        */
    /* symbols.  These need to be copied from the dynamic module (see call   */
    /* to DLSYM_copy_globals() below).                                       */
    /*                                                                       */
    /* THESE INITIALIZATIONS SHOULD BE MOVED TO AN INIT ROUTINE FOR THE      */
    /* LOADED MODULE                                                         */
    /*-----------------------------------------------------------------------*/
    loaded_module->gsymtab = NULL;
    loaded_module->gstrtab = NULL;
    loaded_module->gsymnum = loaded_module->gstrsz = 0;

    /*-----------------------------------------------------------------------*/
    /* Initialize the Array_List of dependencies.                            */
    /*-----------------------------------------------------------------------*/
    AL_initialize(&(loaded_module->dependencies), sizeof(int), 1);

    if (dyn_module->symtab)
        DLSYM_copy_globals(dyn_module);

    /*-----------------------------------------------------------------------*/
    /* Initialize the module loaded segments Array_List.                     */
    /*-----------------------------------------------------------------------*/
    AL_initialize(&(loaded_module->loaded_segments),
                  sizeof(DLIMP_Loaded_Segment), dyn_module->phnum);

    /*-----------------------------------------------------------------------*/
    /* Spin thru segment headers and process each load segment encountered.  */
    /*-----------------------------------------------------------------------*/
    for (i = 0; i < dyn_module->phnum; i++)
        if (dyn_module->phdr[i].p_type == PT_LOAD)
        {
            /*---------------------------------------------------------------*/
            /* Note that this is parallel to and does not supplant the ELF   */
            /* phdr tables.                                                  */
            /*---------------------------------------------------------------*/
            DLIMP_Loaded_Segment seg;
            seg.obj_desc = DLIF_malloc(sizeof(struct DLOAD_MEMORY_SEGMENT));
            seg.phdr.p_vaddr = dyn_module->phdr[i].p_vaddr;
            seg.phdr.p_offset = dyn_module->phdr[i].p_offset;
            seg.modified = 0;
            if(seg.obj_desc) {
                seg.obj_desc->target_page = 0; /*not used*/
                seg.phdr.p_filesz = seg.obj_desc->objsz_in_bytes
                                  = dyn_module->phdr[i].p_filesz;
                seg.phdr.p_memsz = seg.obj_desc->memsz_in_bytes
                                 = dyn_module->phdr[i].p_memsz;
            }
            seg.phdr.p_align = dyn_module->phdr[i].p_align;
            seg.phdr.p_flags = dyn_module->phdr[i].p_flags;
            seg.input_vaddr = 0;
            seg.phdr.p_paddr = 0;
            seg.phdr.p_type = PT_LOAD;
            seg.reloc_offset = 0;
            AL_append(&(loaded_module->loaded_segments), &seg);
#if LOADER_DEBUG
            if (debugging_on)
                DLIF_trace("%s:seg.phdr.p_vaddr 0x%x\n",__func__,
                           seg.phdr.p_vaddr);
#endif
        }

    /*-----------------------------------------------------------------------*/
    /* Initialize the DSO termination information for this module.           */
    /* It will be copied over from the enclosing dyn_module object when      */
    /* placement is completed and dyn_module's local copy of the dynamic     */
    /* table is updated.                                                     */
    /*-----------------------------------------------------------------------*/
    loaded_module->fini_array = (Elf32_Addr)NULL;
    loaded_module->fini_arraysz = 0;
    loaded_module->fini = (Elf32_Addr) NULL;

#if LOADER_DEBUG || LOADER_PROFILE
    if (debugging_on || profiling_on)
    {
        DLIF_trace("Finished initialize_loaded_module()\n");
        if (profiling_on)
        {
            profile_stop_clock();
            DLIF_trace("Took %d cycles.\n", (int32_t)profile_cycle_count());
        }
    }
#endif

}

/*****************************************************************************/
/* load_static_segment()                                                     */
/*                                                                           */
/*    The core dynamic loader requires that a statically linked executable   */
/*    be placed in target memory at the location that was determined during  */
/*    the static link that created the executable.  Failure to get the       */
/*    required target memory where the static executable is to be loaded     */
/*    will cause the dynamic loader to emit an error and abort the load.     */
/*                                                                           */
/*****************************************************************************/
static BOOL load_static_segment(DLOAD_HANDLE handle, LOADER_FILE_DESC *fd,
                                DLIMP_Dynamic_Module *dyn_module)
{
    int i;
    DLIMP_Loaded_Segment* seg = (DLIMP_Loaded_Segment*)
                               (dyn_module->loaded_module->loaded_segments.buf);
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;

#if LOADER_DEBUG
    if (debugging_on) {
        DLIF_trace("dynmodule is 0x%x\n",(UInt32) dyn_module);
        DLIF_trace("loaded_module is 0x%x\n",(UInt32)dyn_module->loaded_module);
        DLIF_trace("loaded_segments is 0x%x\n",
                   (UInt32)&dyn_module->loaded_module->loaded_segments);
        DLIF_trace("seg is 0x%x\n",
                   (UInt32)dyn_module->loaded_module->loaded_segments.buf);
    }
#endif

    /*-----------------------------------------------------------------------*/
    /* For each segment in the loaded module, build up a target memory       */
    /* request for the segment, get rights to target memory where we want    */
    /* to load the segment from the client, then get the client to write the */
    /* segment contents out to target memory to the appropriate address.     */
    /*-----------------------------------------------------------------------*/

    for (i = 0; i < dyn_module->loaded_module->loaded_segments.size; i++)
    {
        struct DLOAD_MEMORY_REQUEST targ_req;
        seg[i].obj_desc->target_page = 0;
        targ_req.flags = 0;

        /*-------------------------------------------------------------------*/
        /* This is a static executable.  DLIF_allocate should give us the    */
        /* address we ask for or fail.                                       */
        /*-------------------------------------------------------------------*/
        if (seg[i].phdr.p_flags & PF_X) targ_req.flags |= DLOAD_SF_executable;

        targ_req.align = seg[i].phdr.p_align;
        seg[i].obj_desc->target_address = (TARGET_ADDRESS)seg[i].phdr.p_vaddr;
        targ_req.flags &= ~DLOAD_SF_relocatable;
        targ_req.fp = fd;
        targ_req.segment = seg[i].obj_desc;
        targ_req.offset = seg[i].phdr.p_offset;
        targ_req.flip_endian = dyn_module->wrong_endian;
#if LOADER_DEBUG
        if (debugging_on) {
            DLIF_trace("============================================\n");
            DLIF_trace("targ_req.align %d\n", targ_req.align);
            DLIF_trace("targ_req.segment 0x%x\n", (UInt32) targ_req.segment);
            DLIF_trace("targ_req.offset 0x%x\n", targ_req.offset);
            DLIF_trace("targ_req.flags 0x%x\n", targ_req.flags);
        }
#endif
        /*-------------------------------------------------------------------*/
        /* Ask the client side of the dynamic loader to allocate target      */
        /* memory for this segment to be loaded into.                        */
        /*-------------------------------------------------------------------*/
        if (!DLIF_allocate(pHandle->client_handle, &targ_req)) return FALSE;

        /*-------------------------------------------------------------------*/
        /* If there is any initialized data in the segment, we'll first write*/
        /* it into a host writable buffer (DLIF_copy()) and then flush it to */
        /* target memory.                                                    */
        /*-------------------------------------------------------------------*/
        if (seg[i].phdr.p_filesz)
        {
            DLIF_copy(pHandle->client_handle, &targ_req);
            DLIF_write(pHandle->client_handle, &targ_req);
        }
    }

    return TRUE;
}

/*****************************************************************************/
/* relocate_target_dynamic_tag_info()                                        */
/*                                                                           */
/*    Update a target specific dynamic tag value that happens to be a        */
/*    virtual address of a section. Returns TRUE if the tag was updated or   */
/*    is not a virtual address and FALSE if it was not successfully updated  */
/*    or was not recognized.                                                 */
/*****************************************************************************/
static BOOL relocate_target_dynamic_tag_info(DLIMP_Dynamic_Module *dyn_module,
                                             int i)
{
#ifdef ARM_TARGET
    if (is_arm_module(&dyn_module->fhdr))
        return DLDYN_arm_relocate_dynamic_tag_info(dyn_module, i);
#endif

#ifdef C60_TARGET
    if (is_c60_module(&dyn_module->fhdr))
        return DLDYN_c60_relocate_dynamic_tag_info(dyn_module, i);
#endif

   return FALSE;
}

/*****************************************************************************/
/* DLIMP_update_dyntag_section_address()                                     */
/*                                                                           */
/*    Given the index of a dynamic tag which we happen to know points to a   */
/*    section address, find the program header table entry associated with   */
/*    the specified address and update the tag value with the real address   */
/*    of the section.                                                        */
/*                                                                           */
/*****************************************************************************/
BOOL DLIMP_update_dyntag_section_address(DLIMP_Dynamic_Module *dyn_module,
                                         int32_t i)
{
    int j;
    DLIMP_Loaded_Segment *seg = (DLIMP_Loaded_Segment *)
                              (dyn_module->loaded_module->loaded_segments.buf);
    for (j = 0; j < dyn_module->loaded_module->loaded_segments.size; j++)
    {
        if ((dyn_module->dyntab[i].d_un.d_ptr >= seg[j].input_vaddr) &&
            (dyn_module->dyntab[i].d_un.d_ptr <
             (seg[j].input_vaddr + seg[j].phdr.p_memsz)))
        {
            dyn_module->dyntab[i].d_un.d_ptr +=
                                    (seg[j].phdr.p_vaddr - seg[j].input_vaddr);
            return TRUE;
        }
    }

    return FALSE;
}

/*****************************************************************************/
/* relocate_dynamic_tag_info()                                               */
/*                                                                           */
/*    Once segment allocation has been completed, we'll need to go through   */
/*    the dynamic table and update any tag values that happen to be virtual  */
/*    addresses of segments (DT_C6000_DSBT_BASE, for example).               */
/*                                                                           */
/*****************************************************************************/
static BOOL relocate_dynamic_tag_info(LOADER_FILE_DESC *fd,
                                      DLIMP_Dynamic_Module *dyn_module)
{
    /*-----------------------------------------------------------------------*/
    /* Spin through dynamic table loking for tags that have a value which is */
    /* the virtual address of a section. After the sections are allocated,   */
    /* we'll need to update these values with the new address of the section.*/
    /*-----------------------------------------------------------------------*/
    int i;
    for (i = 0; dyn_module->dyntab[i].d_tag != DT_NULL; i++)
    {
        switch (dyn_module->dyntab[i].d_tag)
        {
            /*---------------------------------------------------------------*/
            /* Only tag values that are virtual addresses will be affected.  */
            /*---------------------------------------------------------------*/
            case DT_NEEDED:
            case DT_PLTRELSZ:
            case DT_HASH:
            case DT_STRTAB:
            case DT_SYMTAB:
            case DT_RELA:
            case DT_RELASZ:
            case DT_RELAENT:
            case DT_STRSZ:
            case DT_SYMENT:
            case DT_SONAME:
            case DT_RPATH:
            case DT_SYMBOLIC:
            case DT_REL:
            case DT_RELSZ:
            case DT_RELENT:
            case DT_PLTREL:
            case DT_DEBUG:
            case DT_TEXTREL:
            case DT_BIND_NOW:
            case DT_INIT_ARRAYSZ:
            case DT_RUNPATH:
            case DT_FLAGS:
            case DT_PREINIT_ARRAYSZ:
               continue;

            /*---------------------------------------------------------------*/
            /* NOTE!!!                                                       */
            /* case DT_ENCODING:  -- tag type has same "id" as               */
            /* DT_PREINIT_ARRAY                                              */
            /*---------------------------------------------------------------*/

            /*---------------------------------------------------------------*/
            /* This is a generic dynamic tag whose value is a virtual address*/
            /* of a section. It needs to be relocated to the section's actual*/
            /* address in target memory.                                     */
            /*---------------------------------------------------------------*/
            case DT_PREINIT_ARRAY:
            case DT_INIT:
            case DT_INIT_ARRAY:
                if (!DLIMP_update_dyntag_section_address(dyn_module, i))
                    return FALSE;

                continue;

            /*---------------------------------------------------------------*/
            /* Once we have resolved the actual address of termination       */
            /* function sections, we need to copy their addresses over to    */
            /* the loaded module object (dyn_module will be deleted before   */
            /* we get to unloading the module).                              */
            /*---------------------------------------------------------------*/
            case DT_FINI_ARRAY:
            case DT_FINI:
                if (!DLIMP_update_dyntag_section_address(dyn_module, i))
                    return FALSE;

                if (dyn_module->dyntab[i].d_tag == DT_FINI)
                    dyn_module->loaded_module->fini =
                                             dyn_module->dyntab[i].d_un.d_ptr;
                else
                    dyn_module->loaded_module->fini_array =
                                             dyn_module->dyntab[i].d_un.d_ptr;

                continue;

            case DT_FINI_ARRAYSZ:
                dyn_module->loaded_module->fini_arraysz =
                                             dyn_module->dyntab[i].d_un.d_val;
                continue;

            /*---------------------------------------------------------------*/
            /* Is this a virtual address???                                  */
            /*---------------------------------------------------------------*/
            case DT_JMPREL: /* is this a virtual address??? */
                continue;

            /*---------------------------------------------------------------*/
            /* The remaining dynamic tag types should be target specific. If */
            /* something generic slips through to here, then the handler for */
            /* relocating target specific dynamic tags should fail.          */
            /*---------------------------------------------------------------*/
            default:
                if (!relocate_target_dynamic_tag_info(dyn_module, i))
                    return FALSE;
        }
    }

    /*-----------------------------------------------------------------------*/
    /* We've gotten through all of the dynamic table without incident.       */
    /* All dynamic tag values that were virtual section addresses should have*/
    /* been updated with the final address of the section that they point to.*/
    /*-----------------------------------------------------------------------*/
    return TRUE;
}

/*****************************************************************************/
/* allocate_dynamic_segments_and relocate_symbols()                          */
/*                                                                           */
/*    Allocate target memory for each segment in this module, getting a      */
/*    host-accessible space to copy the content of each segment into.  Then  */
/*    update the symbol table and program header table to reflect the new    */
/*    target address for each segment.  Processing of the dynamic relocation */
/*    entries will wait until all dependent files have been loaded and       */
/*    allocated into target memory.                                          */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/*                                                                           */
/* The relocation entries in the ELF file do not handle the necessary        */
/* adjustments to the memory addresses in the program header or symbol       */
/* tables.  These must be done manually.                                     */
/*                                                                           */
/* This is harder for us than for most dynamic loaders, because we have to   */
/* work in environments without virtual memory and thus where the offsets    */
/* between segments in memory may be different than they were in the file.   */
/* So, even though a dynamic loader usually only has to adjust all the       */
/* segments by a single fixed offset, we need to offset the symbols and      */
/* program header addresses segment by segment.  This job is done by the     */
/* function below.                                                           */
/*                                                                           */
/*****************************************************************************/
static BOOL allocate_dynamic_segments_and_relocate_symbols
                                             (DLOAD_HANDLE handle,
                                              LOADER_FILE_DESC *fd,
                                              DLIMP_Dynamic_Module *dyn_module)
{
    int i,j;
    DLIMP_Loaded_Segment* seg = (DLIMP_Loaded_Segment*)
                             (dyn_module->loaded_module->loaded_segments.buf);
    struct Elf32_Ehdr *fhdr = &(dyn_module->fhdr);
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;

#if LOADER_DEBUG || LOADER_PROFILE
    if (debugging_on || profiling_on)
    {
        DLIF_trace("Dynamic executable found.\n"
           "Starting allocate_dynamic_segments_and_relocate_symbols() ...\n");
        if (profiling_on) profile_start_clock();
    }
#endif

    /*-----------------------------------------------------------------------*/
    /* Spin through the list of loaded segments from the current module.     */
    /*-----------------------------------------------------------------------*/
    for (i = 0; i < dyn_module->loaded_module->loaded_segments.size; i++)
    {
        /*-------------------------------------------------------------------*/
        /* Allocate target memory for segment via client-provided target     */
        /* memory API.                                                       */
        /*-------------------------------------------------------------------*/
        int32_t addr_offset;
        struct DLOAD_MEMORY_REQUEST targ_req;
        seg[i].obj_desc->target_page = 0;
        targ_req.flags = 0;
        if (seg[i].phdr.p_flags & PF_X) targ_req.flags |= DLOAD_SF_executable;
        targ_req.align = 0x20;
        seg[i].obj_desc->target_address = (TARGET_ADDRESS)seg[i].phdr.p_vaddr;
        targ_req.flags |= DLOAD_SF_relocatable;

        targ_req.fp = fd;
        targ_req.segment = seg[i].obj_desc;
        targ_req.offset = seg[i].phdr.p_offset;
        targ_req.flip_endian = dyn_module->wrong_endian;

#if LOADER_DEBUG
        if (debugging_on)
            DLIF_trace("Segment %d flags 0x%x\n", i, targ_req.flags);
#endif
        if (!DLIF_allocate(pHandle->client_handle, &targ_req))
        {
            DLIF_error(DLET_MEMORY, "DLIF allocation failure.\n");
            return FALSE;
        }

        /*-------------------------------------------------------------------*/
        /* Calculate the offset we need to adjust segment header and symbol  */
        /* table addresses.                                                  */
        /*-------------------------------------------------------------------*/
        addr_offset = (int32_t)(seg[i].obj_desc->target_address) -
                                               (int32_t)(seg[i].phdr.p_vaddr);

#if LOADER_DEBUG
        if (debugging_on)
        {
            DLIF_trace("Segment %d (at 0x%x, 0x%x bytes) relocated to 0x%x\n",
                       i,
                       (int32_t)(seg[i].phdr.p_vaddr),
                       (int32_t)(seg[i].phdr.p_memsz),
                       (int32_t)(seg[i].obj_desc->target_address));
            DLIF_trace( "Addr Offset is 0x%x\n", addr_offset);
        }
#endif

        /*-------------------------------------------------------------------*/
        /* Update program entry point if needed.  Need to replace to deal    */
        /* with full ELF initialization routine.                             */
        /*-------------------------------------------------------------------*/
        if (dyn_module->relocate_entry_point &&
            fhdr->e_entry >= (Elf32_Addr)(seg[i].phdr.p_vaddr) &&
            fhdr->e_entry < (Elf32_Addr)((uint8_t*)(seg[i].phdr.p_vaddr) +
                                         (uint32_t)(seg[i].phdr.p_memsz)))
        {
#if LOADER_DEBUG
            if (debugging_on)
            {
                DLIF_trace("Entry point 0x%x relocated to 0x%x\n",
                           fhdr->e_entry, fhdr->e_entry + addr_offset);
            }
#endif
            fhdr->e_entry += addr_offset;

            /*---------------------------------------------------------------*/
            /* Mark the entry point as being relocated so we will not do it  */
            /* again.                                                        */
            /*---------------------------------------------------------------*/
            dyn_module->relocate_entry_point = FALSE;
        }

        /*-------------------------------------------------------------------*/
        /* Fix program header entries in segment and Elf32_Phdr structs.     */
        /*-------------------------------------------------------------------*/
        for (j = 0; j < fhdr->e_phnum; j++)
            if (dyn_module->phdr[j].p_vaddr == (Elf32_Addr)seg[i].phdr.p_vaddr)
            {
                dyn_module->phdr[j].p_vaddr += addr_offset;
                dyn_module->phdr[i].p_paddr += addr_offset;
                break;
            }

        seg[i].input_vaddr = (Elf32_Addr)(seg[i].phdr.p_vaddr);
        seg[i].phdr.p_vaddr += addr_offset;

        /*-------------------------------------------------------------------*/
        /* Great, now the hard part: fix offsets in symbols.  It would be    */
        /* nice if there were an easier way to deal with this.               */
        /*-------------------------------------------------------------------*/
        {
            struct Elf32_Sym *gsymtab =
                    ((struct Elf32_Sym*)(dyn_module->loaded_module->gsymtab));
            Elf32_Addr segment_start = (Elf32_Addr)seg[i].phdr.p_vaddr;
            Elf32_Addr segment_end   = (Elf32_Addr)seg[i].phdr.p_vaddr +
                                       seg[i].phdr.p_memsz;
            Elf32_Word global_index  = dyn_module->symnum -
                                       dyn_module->loaded_module->gsymnum;

            for (j = 0; j < dyn_module->symnum; j++)
            {
                /*-----------------------------------------------------------*/
                /* Get the relocated symbol value.                           */
                /*-----------------------------------------------------------*/
                Elf32_Addr symval_adj = dyn_module->symtab[j].st_value +
                                        addr_offset;

                /*-----------------------------------------------------------*/
                /* If the symbol is defined in this segment, update the      */
                /* symbol value and mark the symbol so that we don't         */
                /* relocate it again.                                        */
                /*-----------------------------------------------------------*/
                if (symval_adj >= segment_start && symval_adj <  segment_end &&
                    dyn_module->symtab[j].st_shndx != INT16_MAX)
                {
                    dyn_module->symtab[j].st_value = symval_adj;

                    /*-------------------------------------------------------*/
                    /* The module symbol table only has the global symbols.  */
                    /*-------------------------------------------------------*/
                    if (j >= global_index)
                       gsymtab[j-global_index].st_value = symval_adj;

                    /*-------------------------------------------------------*/
                    /* Mark the symbol as relocated.                         */
                    /*-------------------------------------------------------*/
                    dyn_module->symtab[j].st_shndx = INT16_MAX;
                }
            }
        }
    }

    /*-----------------------------------------------------------------------*/
    /* Update dynamic tag information. Some dynamic tags have values which   */
    /* are virtual addresses of sections. These values need to be updated    */
    /* once segment allocation is completed and the new segment addresses are*/
    /* known.                                                                */
    /*-----------------------------------------------------------------------*/
    /* We should only traverse through the dynamic table once because we want*/
    /* to avoid the possibility of updating the same tag multiple times (an  */
    /* error, if it happens).                                                */
    /*-----------------------------------------------------------------------*/
    if (!relocate_dynamic_tag_info(fd, dyn_module))
    {
        DLIF_error(DLET_MISC, "Failed dynamic table update.\n");
        return FALSE;
    }

#if LOADER_DEBUG || LOADER_PROFILE
    if (debugging_on || profiling_on)
    {
        DLIF_trace("allocate_dynamic_segments_and_relocate_symbols() Done\n");
        if (profiling_on)
        {
            profile_stop_clock();
            DLIF_trace("Took %d cycles.\n", (int)profile_cycle_count());
        }
    }
#endif

    return TRUE;
}

/*****************************************************************************/
/* delete_DLIMP_Loaded_Module()                                              */
/*                                                                           */
/*    Free host memory associated with a DLIMP_Loaded_Module data structure  */
/*    and all of the DLIMP_Loaded_Segment objects that are associated with   */
/*    it.                                                                    */
/*                                                                           */
/*****************************************************************************/
static void delete_DLIMP_Loaded_Module(DLOAD_HANDLE handle,
                                       DLIMP_Loaded_Module **pplm)
{
    DLIMP_Loaded_Module *loaded_module = *pplm;
    DLIMP_Loaded_Segment *segments = (DLIMP_Loaded_Segment*)
                                          (loaded_module->loaded_segments.buf);
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;

    /*-----------------------------------------------------------------------*/
    /* Spin through the segments attached to this loaded module, freeing up  */
    /* any target memory that was allocated by the client for the segment.   */
    /*-----------------------------------------------------------------------*/
    int i;
    for (i = 0; i < loaded_module->loaded_segments.size; i++)
    {
        if (!DLIF_release(pHandle->client_handle, segments[i].obj_desc))
            DLIF_error(DLET_MISC, "Failed call to DLIF_release!\n");;
        DLIF_free(segments[i].obj_desc);
    }

    /*-----------------------------------------------------------------------*/
    /* Hacky way of indicating that the base image is no longer available.   */
    /* WHHHHAAAAAAATTT!?!?!?!?!?!                                            */
    /*-----------------------------------------------------------------------*/
    if (loaded_module->file_handle == DLIMP_application_handle)
        DLIMP_application_handle = 0;

    /*-----------------------------------------------------------------------*/
    /* Free host heap memory that was allocated for the internal loaded      */
    /* module data structure members.                                        */
    /*-----------------------------------------------------------------------*/
    if (loaded_module->name)    DLIF_free(loaded_module->name);
    if (loaded_module->gsymtab) DLIF_free(loaded_module->gsymtab);
    loaded_module->gsymnum = 0;
    if (loaded_module->gstrtab) DLIF_free(loaded_module->gstrtab);
    loaded_module->gstrsz = 0;
    AL_destroy(&(loaded_module->loaded_segments));
    AL_destroy(&(loaded_module->dependencies));

    /*-----------------------------------------------------------------------*/
    /* Finally, free the host memory for the loaded module object, then NULL */
    /* the pointer that was passed in.                                       */
    /*-----------------------------------------------------------------------*/
    DLIF_free(loaded_module);
    *pplm = NULL;
}

/*****************************************************************************/
/* new_DLIMP_Dynamic_Module()                                                */
/*                                                                           */
/*   Allocate a dynamic module data structure from host memory and           */
/*   initialize its members to their default values.                         */
/*                                                                           */
/*****************************************************************************/
static DLIMP_Dynamic_Module *new_DLIMP_Dynamic_Module(LOADER_FILE_DESC *fd)
{
    /*-----------------------------------------------------------------------*/
    /* Allocate space for dynamic module data structure from host memory.    */
    /*-----------------------------------------------------------------------*/
    DLIMP_Dynamic_Module *dyn_module =
             (DLIMP_Dynamic_Module *)DLIF_malloc(sizeof(DLIMP_Dynamic_Module));

    if (!dyn_module)
        return NULL;

    /*-----------------------------------------------------------------------*/
    /* Initialize data members of the new dynamic module data structure.     */
    /*-----------------------------------------------------------------------*/
    dyn_module->name = NULL;
    dyn_module->fd = fd;
    dyn_module->phdr = NULL;
    dyn_module->phnum = 0;
    dyn_module->strtab = NULL;
    dyn_module->strsz = 0;
    dyn_module->dyntab = NULL;
    dyn_module->symtab = NULL;
    dyn_module->symnum = 0;
    dyn_module->gsymtab_offset = 0;
    dyn_module->gstrtab_offset = 0;
    dyn_module->c_args = NULL;
    dyn_module->argc = 0;
    dyn_module->argv = NULL;
    dyn_module->loaded_module = NULL;
    dyn_module->wrong_endian = 0;
    dyn_module->direct_dependent_only = TRUE;
    dyn_module->relocatable = FALSE;
    dyn_module->relocate_entry_point = TRUE;

    dyn_module->dsbt_size = 0;
    dyn_module->dsbt_index = DSBT_INDEX_INVALID;
    dyn_module->dsbt_base_tagidx = -1;

    dyn_module->preinit_array_idx = -1;
    dyn_module->preinit_arraysz = 0;
    dyn_module->init_idx = -1;
    dyn_module->init_array_idx = -1;
    dyn_module->init_arraysz = 0;

    return dyn_module;
}

/*****************************************************************************/
/* detach_loaded_module()                                                    */
/*                                                                           */
/*    Detach loaded module data structure from given dynamic module.  When   */
/*    an object file has been successfully loaded, the loader core will      */
/*    detach the loaded module data structure from the dynamic module data   */
/*    structure because the loaded module must continue to persist until is  */
/*    is actually unloaded from target memory.  If there is a problem with   */
/*    the load, then the host memory associated with the loaded module will  */
/*    be released as part of the destruction of the dynamic module.          */
/*                                                                           */
/*****************************************************************************/
static DLIMP_Loaded_Module *detach_loaded_module(DLIMP_Dynamic_Module *dyn_module)
{
    if (dyn_module && dyn_module->loaded_module)
    {
        DLIMP_Loaded_Module *loaded_module = dyn_module->loaded_module;
        dyn_module->loaded_module = NULL;
        return loaded_module;
    }

    return NULL;
}
/*****************************************************************************/
/* delete_DLIMP_Dynamic_Module()                                             */
/*                                                                           */
/*    Remove local copies of the string table, symbol table, program header  */
/*    table, and dynamic table.                                              */
/*                                                                           */
/*****************************************************************************/
static void delete_DLIMP_Dynamic_Module(DLOAD_HANDLE handle,
                                        DLIMP_Dynamic_Module **ppdm)
{
    DLIMP_Dynamic_Module *dyn_module = NULL;

    if (!ppdm || (*ppdm == NULL))
    {
        DLIF_error(DLET_MISC,
                   "Internal Error: invalid argument to dynamic module "
                   "destructor function; aborting loader\n");
        exit(1);
    }

    dyn_module = *ppdm;
    if (dyn_module->name)     DLIF_free(dyn_module->name);
    if (dyn_module->strtab)   DLIF_free(dyn_module->strtab);
    if (dyn_module->symtab)   DLIF_free(dyn_module->symtab);
    if (dyn_module->phdr)     DLIF_free(dyn_module->phdr);
    if (dyn_module->dyntab)   DLIF_free(dyn_module->dyntab);

    /*-----------------------------------------------------------------------*/
    /* If we left the loaded module attached to the dynamic module, then     */
    /* something must have gone wrong with the load.  Remove the loaded      */
    /* module from the queue of loaded modules, if it is there.  Then free   */
    /* the host memory allocated to the loaded module and its segments.      */
    /*-----------------------------------------------------------------------*/
    if (dyn_module->loaded_module != NULL)
        delete_DLIMP_Loaded_Module(handle, &(dyn_module->loaded_module));

    /*-----------------------------------------------------------------------*/
    /* Finally, free the host memory for this dynamic module object and NULL */
    /* the pointer to the object.                                            */
    /*-----------------------------------------------------------------------*/
    DLIF_free(dyn_module);
    *ppdm = NULL;
}

/*****************************************************************************/
/* file_header_magic_number_is_valid()                                       */
/*                                                                           */
/*    Given an object file header, check the magic number to ensure that it  */
/*    is an object file format that we recognize.  This implementation of    */
/*    the dynamic loader core will handle ELF object file format.            */
/*                                                                           */
/*****************************************************************************/
static BOOL file_header_magic_number_is_valid(struct Elf32_Ehdr* header)
{
    /*-----------------------------------------------------------------------*/
    /* Check for correct ELF magic numbers in file header.                   */
    /*-----------------------------------------------------------------------*/
    if (!header->e_ident[EI_MAG0] == ELFMAG0 ||
        !header->e_ident[EI_MAG1] == ELFMAG1 ||
        !header->e_ident[EI_MAG2] == ELFMAG2 ||
        !header->e_ident[EI_MAG3] == ELFMAG3)
    {
        DLIF_error(DLET_FILE, "Invalid ELF magic number.\n");
        return FALSE;
    }

    return TRUE;
}

/*****************************************************************************/
/* file_header_machine_is_valid()                                            */
/*                                                                           */
/*    Check if the machine specified in the file header is supported by the  */
/*    loader.  If the loader was compiled with support for all targets,      */
/*    the machine will be initially set to EM_NONE.  Once a module has been  */
/*    loaded, all remaining modules must have the same machine value.        */
/*****************************************************************************/
static BOOL file_header_machine_is_valid(DLOAD_HANDLE handle, Elf32_Half e_machine)
{
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;

    if (pHandle->DLOAD_TARGET_MACHINE == EM_NONE)
        pHandle->DLOAD_TARGET_MACHINE = e_machine;

    if (e_machine != pHandle->DLOAD_TARGET_MACHINE)
        return FALSE;

    return TRUE;
}

/*****************************************************************************/
/* is_valid_elf_object_file()                                                */
/*                                                                           */
/*    Check file size against anticipated end location of string table,      */
/*    symbol table, program header tables, etc.  If we anything untoward,    */
/*    then we declare that the ELF file is corrupt and the load is aborted.  */
/*                                                                           */
/*****************************************************************************/
static BOOL is_valid_elf_object_file(LOADER_FILE_DESC *fd,
                                     DLIMP_Dynamic_Module *dyn_module)
{
    uint32_t fsz;
    int i;

    /*-----------------------------------------------------------------------*/
    /* Get file size.                                                        */
    /*-----------------------------------------------------------------------*/
    DLIF_fseek(fd, 0, LOADER_SEEK_END);
    fsz = DLIF_ftell(fd);

    /*-----------------------------------------------------------------------*/
    /* Check for invalid table sizes (string table, symbol table, and        */
    /* program header tables).                                               */
    /*-----------------------------------------------------------------------*/
    if (!((dyn_module->strsz < fsz) &&
          (dyn_module->symnum < fsz) &&
          (dyn_module->phnum * sizeof(struct Elf32_Phdr)) < fsz))
    {
        DLIF_error(DLET_FILE, "Invalid ELF table bounds.\n");
        return FALSE;
    }

    /*-----------------------------------------------------------------------*/
    /* Check for null so_name string in file with dynamic information.       */
    /*-----------------------------------------------------------------------*/
    if (dyn_module->dyntab && !strcmp(dyn_module->name, ""))
    {
        DLIF_error(DLET_MISC, "Dynamic file lacks SO_NAME identifier.\n");
        return FALSE;
    }

    /*-----------------------------------------------------------------------*/
    /* Check for invalid program header information.                         */
    /*-----------------------------------------------------------------------*/
    for (i = 0; i < dyn_module->phnum; i++)
    {
        struct Elf32_Phdr* phdr = dyn_module->phdr + i;

        /*-------------------------------------------------------------------*/
        /* Sanity check for relative sizes of filesz and memsz.              */
        /*-------------------------------------------------------------------*/
        if (!(phdr->p_type != PT_LOAD || phdr->p_filesz <= phdr->p_memsz))
        {
             DLIF_error(DLET_MISC,
                        "Invalid file or memory size for segment %d.\n", i);
             return FALSE;
        }

        /*-------------------------------------------------------------------*/
        /* Check that segment file offset doesn't go off the end of the file.*/
        /*-------------------------------------------------------------------*/
        if (!(phdr->p_offset + phdr->p_filesz < fsz))
        {
             DLIF_error(DLET_FILE,
                 "File location of segment %d is past the end of file.\n", i);
             return FALSE;
        }
    }

    /*-----------------------------------------------------------------------*/
    /* Check that a ET_DYN-type file is relocatable.                         */
    /*-----------------------------------------------------------------------*/
    if (dyn_module->fhdr.e_type == ET_DYN && !dyn_module->symtab) return FALSE;

    /*-----------------------------------------------------------------------*/
    /* All checks passed.                                                    */
    /*-----------------------------------------------------------------------*/
    return TRUE;
}

/*****************************************************************************/
/* process_eiosabi()                                                         */
/*                                                                           */
/*   Check the EI_OSABI field to validate it and set any parameters based on */
/*   it.                                                                     */
/*****************************************************************************/
static BOOL process_eiosabi(DLIMP_Dynamic_Module* dyn_module)
{
#if ARM_TARGET
    if (is_arm_module(&dyn_module->fhdr))
        return DLDYN_arm_process_eiosabi(dyn_module);
#endif

#if C60_TARGET
    if (is_c60_module(&dyn_module->fhdr))
        return DLDYN_c60_process_eiosabi(dyn_module);
#endif

   return FALSE;
}
/*****************************************************************************/
/* dload_file_header()                                                       */
/*                                                                           */
/*    Read ELF file header.  Store critical information in the provided      */
/*    DLIMP_Dynamic_Module record.  Check file header for validity.          */
/*                                                                           */
/*****************************************************************************/
static BOOL dload_file_header(DLOAD_HANDLE handle, LOADER_FILE_DESC *fd,
                              DLIMP_Dynamic_Module *dyn_module)
{
    /*-----------------------------------------------------------------------*/
    /* Read ELF file header from given input file.                           */
    /*-----------------------------------------------------------------------*/
    DLIF_fread(&(dyn_module->fhdr), sizeof(struct Elf32_Ehdr), 1, fd);

    /*-----------------------------------------------------------------------*/
    /* Determine target vs. host endian-ness.  Does header data need to be   */
    /* byte swapped?                                                         */
    /*-----------------------------------------------------------------------*/
    dyn_module->wrong_endian =
                     (dyn_module->fhdr.e_ident[EI_DATA] != DLIMP_get_endian());

    /*-----------------------------------------------------------------------*/
    /* Swap file header structures, if needed.                               */
    /*-----------------------------------------------------------------------*/
    if (dyn_module->wrong_endian)
        DLIMP_change_ehdr_endian(&(dyn_module->fhdr));

#if LOADER_DEBUG
    if (debugging_on) {
        /*-------------------------------------------------------------------*/
        /* Write out magic ELF information for debug purposes.               */
        /*-------------------------------------------------------------------*/
        DLIF_trace("ELF: %c%c%c\n", dyn_module->fhdr.e_ident[1],
                                dyn_module->fhdr.e_ident[2],
                                dyn_module->fhdr.e_ident[3]);
        DLIF_trace("ELF file header entry point: %x\n",
                   dyn_module->fhdr.e_entry);
    }
#endif

    /*-----------------------------------------------------------------------*/
    /* Verify magic numbers in ELF file header.                              */
    /*-----------------------------------------------------------------------*/
    if (!file_header_magic_number_is_valid(&(dyn_module->fhdr)))
    {
        DLIF_error(DLET_FILE, "Invalid ELF file header magic number.\n");
        return FALSE;
    }

    if (!file_header_machine_is_valid(handle, dyn_module->fhdr.e_machine))
    {
        DLIF_error(DLET_FILE, "Invalid ELF file target machine.\n");
        DLIF_trace("dyn_module->fhdr.e_machine = 0x%x\n",
                   dyn_module->fhdr.e_machine);
        return FALSE;
    }

    /*-----------------------------------------------------------------------*/
    /* Verify file is an executable or dynamic shared object or library.     */
    /*-----------------------------------------------------------------------*/
    if ((dyn_module->fhdr.e_type != ET_EXEC) &&
        (dyn_module->fhdr.e_type != ET_DYN))
    {
        DLIF_error(DLET_FILE, "Invalid ELF file type.\n");
        return FALSE;
    }

#if LOADER_DEBUG || LOADER_PROFILE
    /*-----------------------------------------------------------------------*/
    /* Stop profiling clock when file header information has finished        */
    /* loading.  Re-start clock on initialization of symbol table, and       */
    /* dynamic table pointers.                                               */
    /*-----------------------------------------------------------------------*/
    if (debugging_on || profiling_on)
    {
        DLIF_trace("done.\n");
        if (profiling_on)
        {
            profile_stop_clock();
            DLIF_trace("Took %d cycles.\n", (int)profile_cycle_count());
            profile_start_clock();
        }
    }
#endif

    return TRUE;
}

/*****************************************************************************/
/* dload_program_header_table()                                              */
/*                                                                           */
/*    Make a local copy of the ELF object file's program header table in the */
/*    dynamic module data structure.                                         */
/*                                                                           */
/*****************************************************************************/
static void dload_program_header_table(LOADER_FILE_DESC *fd,
                                       DLIMP_Dynamic_Module *dyn_module)
{
    /*-----------------------------------------------------------------------*/
    /* Read the program header tables from the object file.                  */
    /*-----------------------------------------------------------------------*/
    struct Elf32_Ehdr *fhdr = &(dyn_module->fhdr);
    dyn_module->phdr = (struct Elf32_Phdr*)
                              (DLIF_malloc(fhdr->e_phnum * fhdr->e_phentsize));
    DLIF_fseek(fd, fhdr->e_phoff, LOADER_SEEK_SET);
    if(dyn_module->phdr) {
        DLIF_fread(dyn_module->phdr, fhdr->e_phentsize, fhdr->e_phnum,fd);
        dyn_module->phnum = fhdr->e_phnum;

        /*-------------------------------------------------------------------*/
        /* Byte swap the program header tables if the target endian-ness is  */
        /* not the same as the host endian-ness.                             */
        /*-------------------------------------------------------------------*/
        if (dyn_module->wrong_endian)
        {
            int i;
            for (i = 0; i < dyn_module->phnum; i++)
                DLIMP_change_phdr_endian(dyn_module->phdr + i);
        }
    }
}

/*****************************************************************************/
/* dload_headers()                                                           */
/*                                                                           */
/*    Read ELF object file header and program header table information into  */
/*    the given dynamic module data structure.  If the object file contains  */
/*    dynamic information, read in the dynamic tags, dynamic symbol table,   */
/*    and global string table.  Check to make sure that we are not already   */
/*    in the process of loading the module (circular dependencies), then     */
/*    perform some level of sanity checking on the content of the file to    */
/*    provide some assurance that the file is not corrupted.                 */
/*                                                                           */
/*****************************************************************************/
static BOOL dload_headers(DLOAD_HANDLE handle, LOADER_FILE_DESC *fd,
                          DLIMP_Dynamic_Module *dyn_module)
{
#if LOADER_DEBUG || LOADER_PROFILE
    /*-----------------------------------------------------------------------*/
    /* More progress information.  Start timing if profiling is enabled.     */
    /*-----------------------------------------------------------------------*/
    if (debugging_on || profiling_on)
    {
        DLIF_trace("\nReading file headers ...\n");
        if (profiling_on) profile_start_clock();
    }
#endif

    /*-----------------------------------------------------------------------*/
    /* Read file header information and check vs. expected ELF object file   */
    /* header content.                                                       */
    /*-----------------------------------------------------------------------*/
    if (!dload_file_header(handle, fd, dyn_module))
        return FALSE;

    /*-----------------------------------------------------------------------*/
    /* Read program header table information into the dynamic module object. */
    /*-----------------------------------------------------------------------*/
    dload_program_header_table(fd, dyn_module);

    return TRUE;
}

/*****************************************************************************/
/* find_dynamic_segment()                                                    */
/*                                                                           */
/*    Find the dynamic segment in the given ELF object file, if there is     */
/*    one.  If the segment is found, then the segment ID output parameter    */
/*    is set to the index of the dynamic segment in the program header       */
/*    table.  If the dynamic segment is not found, the dynamic module's      */
/*    relocatable flag is set to FALSE, and return FALSE.                    */
/*                                                                           */
/*****************************************************************************/
static BOOL find_dynamic_segment(DLIMP_Dynamic_Module *dyn_module,
                                 Elf32_Word *dyn_seg_idx)
{
    int i;

    /*-----------------------------------------------------------------------*/
    /* We should have a valid dynamic module pointer and somewhere to put the*/
    /* dynamic segment id, if we find one.  If either of these are missing,  */
    /* we should get an internal error and abort the loader.                 */
    /*-----------------------------------------------------------------------*/
    if ((dyn_module == NULL) || (dyn_seg_idx == NULL))
    {
        DLIF_error(DLET_MISC, "Internal error: find_dynamic_segment() needs "
                              "non-NULL arguments.\n");
        exit(1);
    }

    /*-----------------------------------------------------------------------*/
    /* Spin through segment program headers to find the dynamic segment.     */
    /*-----------------------------------------------------------------------*/
    dyn_module->relocatable = TRUE;
    for (i = 0; i < dyn_module->phnum; i++)
        if (dyn_module->phdr[i].p_type == PT_DYNAMIC)
            { *dyn_seg_idx = i; return TRUE; }

    /*-----------------------------------------------------------------------*/
    /* No dynamic segment found, mark the object module as not relocatable   */
    /* and warn the user.                                                    */
    /*-----------------------------------------------------------------------*/
    dyn_module->relocatable = FALSE;
#if LOADER_DEBUG
    DLIF_warning(DLWT_MISC, "'%s' does not have a dynamic segment; assuming "
                            "that it is a static executable and it cannot "
                            "be relocated.\n", dyn_module->name);
#endif
    return FALSE;
}

/*****************************************************************************/
/* copy_dynamic_table()                                                      */
/*                                                                           */
/*    Make a local copy of the dynamic table read from the dynamic segment   */
/*    in the ELF object file.                                                */
/*                                                                           */
/*****************************************************************************/
static void copy_dynamic_table(LOADER_FILE_DESC *fd,
                               DLIMP_Dynamic_Module *dyn_module,
                               Elf32_Word dyn_seg_idx)
{
    /*-----------------------------------------------------------------------*/
    /* Allocate space for the dynamic table from host memory and read its    */
    /* content from the ELF object file.                                     */
    /*-----------------------------------------------------------------------*/
    Elf32_Word num_elem;
    dyn_module->dyntab = DLIF_malloc(dyn_module->phdr[dyn_seg_idx].p_filesz);
    num_elem =
        dyn_module->phdr[dyn_seg_idx].p_filesz / sizeof(struct Elf32_Dyn);
    DLIF_fseek(fd, dyn_module->phdr[dyn_seg_idx].p_offset, LOADER_SEEK_SET);
    if(dyn_module->dyntab) {
        DLIF_fread(dyn_module->dyntab, sizeof(struct Elf32_Dyn), num_elem, fd);

        /*-------------------------------------------------------------------*/
        /* If necessary, byte swap each entry in the dynamic table.          */
        /*-------------------------------------------------------------------*/
        if (dyn_module->wrong_endian)
        {
            int i;
            for (i = 0; i < num_elem; i++)
                DLIMP_change_dynent_endian(&dyn_module->dyntab[i]);
        }
    }
}

/*****************************************************************************/
/* process_target_dynamic_tag()                                              */
/*                                                                           */
/* Process a target specific dynamic tag entry.  Returns TRUE if the tag     */
/* was handled and FALSE if it was not recognized.                           */
/*****************************************************************************/
static BOOL process_target_dynamic_tag(DLIMP_Dynamic_Module* dyn_module, int i)
{
#ifdef ARM_TARGET
    if (is_arm_module(&dyn_module->fhdr))
        return DLDYN_arm_process_dynamic_tag(dyn_module, i);
#endif

#ifdef C60_TARGET
    if (is_c60_module(&dyn_module->fhdr))
        return DLDYN_c60_process_dynamic_tag(dyn_module, i);
#endif

    return FALSE;
}

/*****************************************************************************/
/* process_dynamic_table()                                                   */
/*                                                                           */
/*    Process dynamic tag entries from the dynamic table.  At the conclusion */
/*    of this function, we should have made a copy of the global symbols     */
/*    and the global symbol names.                                           */
/*                                                                           */
/*****************************************************************************/
static BOOL process_dynamic_table(LOADER_FILE_DESC *fd,
                                  DLIMP_Dynamic_Module *dyn_module)
{
    int        i;
    BOOL       soname_found  = FALSE;
    Elf32_Addr soname_offset = 0;
    Elf32_Addr strtab_offset = 0;
    Elf32_Addr hash_offset   = 0;
    Elf32_Addr symtab_offset = 0;

    /*-----------------------------------------------------------------------*/
    /* Iterate over the dynamic table in order to process dynamic tags.      */
    /* See ELF TIS Specification for details on the meaning of each dynamic  */
    /* tag.  The C6000 ELF ABI Specification provides more details about the */
    /* TI specific C6000 ELF ABI tags.                                       */
    /*-----------------------------------------------------------------------*/
    for (i = 0; dyn_module->dyntab[i].d_tag != DT_NULL; i++)
    {
        switch(dyn_module->dyntab[i].d_tag)
        {
            /*---------------------------------------------------------------*/
            /* DT_SONAME: Contains name of dynamic object, used for          */
            /*            dependency comparisons.  Its value is an offset    */
            /*            from the start of the string table.  We need to    */
            /*            copy the string at this offset into dmodule->name. */
            /*------------------------------------------------------------- -*/
            case DT_SONAME:
#if LOADER_DEBUG
                if (debugging_on) DLIF_trace("Found SO_NAME.\n");
#endif
                /*-----------------------------------------------------------*/
                /* We store the offset of the so_name in the dynamic string  */
                /* table so that it doesn't matter which dynamic tag we see  */
                /* first (DT_SONAME actually is generated before DT_STRTAB). */
                /*-----------------------------------------------------------*/
                soname_found = TRUE;
                soname_offset = dyn_module->dyntab[i].d_un.d_ptr;
                break;

            /*---------------------------------------------------------------*/
            /* DT_STRSZ: Contains the size of the string table.              */
            /*---------------------------------------------------------------*/
            case DT_STRSZ:
                dyn_module->strsz = dyn_module->dyntab[i].d_un.d_val;

#if LOADER_DEBUG
                if (debugging_on)
                    DLIF_trace("Found string table Size: 0x%x\n",
                               dyn_module->strsz);
#endif
                break;

            /*---------------------------------------------------------------*/
            /* DT_STRTAB: Contains the file offset of the string table.  The */
            /*            tag directly after this is guaranteed to be        */
            /*            DT_STRSZ, containing the string table size.  We    */
            /*            need to allocate memory for the string table and   */
            /*            copy it from the file.                             */
            /*---------------------------------------------------------------*/
            case DT_STRTAB:
                strtab_offset = dyn_module->dyntab[i].d_un.d_ptr;
#if LOADER_DEBUG
                if (debugging_on)
                    DLIF_trace("Found string table: 0x%x\n", strtab_offset);
#endif
                break;

            /*---------------------------------------------------------------*/
            /* DT_HASH: Contains the file offset of the symbol hash table.   */
            /*---------------------------------------------------------------*/
            case DT_HASH:
                hash_offset = dyn_module->dyntab[i].d_un.d_ptr;
#if LOADER_DEBUG
                if (debugging_on)
                    DLIF_trace("Found symbol hash table: 0x%x\n", hash_offset);
#endif
                break;

            /*---------------------------------------------------------------*/
            /* DT_SYMTAB: Contains the file offset of the symbol table.      */
            /*---------------------------------------------------------------*/
            case DT_SYMTAB:
                symtab_offset = dyn_module->dyntab[i].d_un.d_ptr;
#if LOADER_DEBUG
                if (debugging_on)
                    DLIF_trace("Found symbol table: 0x%x\n", symtab_offset);
#endif
                break;

            /*---------------------------------------------------------------*/
            /* DSO Initialization / Termination Model Dynamic Tags           */
            /*---------------------------------------------------------------*/
            /* For initialization tags, we store indices and array sizes in  */
            /* the dyn_module. Termination works a little different, the     */
            /* indices into the local copy of the dynamic table are stored   */
            /* in dyn_module, but the DT_FINI_ARRAYSZ value is recorded with */
            /* the loaded module.                                            */
            /*---------------------------------------------------------------*/
            /* After placement is done, the DT_FINI and DT_FINI_ARRAY values */
            /* need to be copied from the local dynamic table into the       */
            /* loaded module object.                                         */
            /*---------------------------------------------------------------*/
            case DT_PREINIT_ARRAY:
                dyn_module->preinit_array_idx = i;
                break;

            case DT_PREINIT_ARRAYSZ:
                dyn_module->preinit_arraysz = dyn_module->dyntab[i].d_un.d_val;
                break;

            case DT_INIT:
                dyn_module->init_idx = i;
                break;

            case DT_INIT_ARRAY:
                dyn_module->init_array_idx = i;
                break;

            case DT_INIT_ARRAYSZ:
                dyn_module->init_arraysz = dyn_module->dyntab[i].d_un.d_val;
                break;

            /*---------------------------------------------------------------*/
            /* This information will be copied over to the loaded module     */
            /* object after placement has been completed and the information */
            /* in the dynamic table has been relocated.                      */
            /*---------------------------------------------------------------*/
            case DT_FINI_ARRAY:
            case DT_FINI_ARRAYSZ:
            case DT_FINI:
                break;

            /*---------------------------------------------------------------*/
            /* Unrecognized tag, may not be illegal, but is not explicitly   */
            /* handled by this function.  Should it be?                      */
            /*---------------------------------------------------------------*/
            default:
            {
                if (!process_target_dynamic_tag(dyn_module, i))
                {
#if LOADER_DEBUG
                    if (debugging_on)
                        DLIF_trace("Unrecognized dynamic tag: 0x%X\n",
                                   dyn_module->dyntab[i].d_tag);
#endif
                }

                break;
            }

        }
    }

    /*-----------------------------------------------------------------------*/
    /* If string table offset and size were found, read string table in from */
    /* the ELF object file.                                                  */
    /*-----------------------------------------------------------------------*/
    if (strtab_offset && dyn_module->strsz)
    {
        DLIF_fseek(fd, strtab_offset, LOADER_SEEK_SET);
        dyn_module->strtab = DLIF_malloc(dyn_module->strsz);
        if(dyn_module->strtab)
            DLIF_fread(dyn_module->strtab, sizeof(uint8_t), dyn_module->strsz,
                        fd);
        else
            return FALSE;
    }
    else
    {
        DLIF_warning(DLWT_MISC,
                     "Mandatory dynamic tag DT_STRTAB/DT_STRSZ not found!\n");
        return FALSE;
    }


    /*-----------------------------------------------------------------------*/
    /* If symbol hash table is found read-in the hash table.                 */
    /*-----------------------------------------------------------------------*/
    if (hash_offset)
    {
        /*-------------------------------------------------------------------*/
        /* Hash table has the following format. nchain equals the number of  */
        /* entries in the symbol table (symnum)                              */
        /*                                                                   */
        /*             +----------------------------+                        */
        /*             |          nbucket           |                        */
        /*             +----------------------------+                        */
        /*             |          nchain            |                        */
        /*             +----------------------------+                        */
        /*             |         bucket[0]          |                        */
        /*             |            ...             |                        */
        /*             |     bucket[nbucket-1]      |                        */
        /*             +----------------------------+                        */
        /*             |          chain[0]          |                        */
        /*             |            ...             |                        */
        /*             |       chain[nchain-1]      |                        */
        /*             +----------------------------+                        */
        /*-------------------------------------------------------------------*/
        Elf32_Word hash_nbucket;
        Elf32_Word hash_nchain;

        /*-------------------------------------------------------------------*/
        /* Seek to the hash offset and read first two words into nbucket and */
        /* symnum.                                                           */
        /*-------------------------------------------------------------------*/
        DLIF_fseek(fd, hash_offset, LOADER_SEEK_SET);
        DLIF_fread(&(hash_nbucket), sizeof(Elf32_Word), 1, fd);
        DLIF_fread(&(hash_nchain), sizeof(Elf32_Word), 1, fd);
        if (dyn_module->wrong_endian)
        {
         DLIMP_change_endian32((int32_t*)(&(hash_nbucket)));
         DLIMP_change_endian32((int32_t*)(&(hash_nchain)));
        }

        /*-------------------------------------------------------------------*/
        /* The number of entires in the dynamic symbol table is not encoded  */
        /* anywhere in the elf file. However, the nchain is guaranteed to be */
        /* the same as the number of symbols. Use nchain to set the symnum.  */
        /*-------------------------------------------------------------------*/
        dyn_module->symnum = hash_nchain;
#if LOADER_DEBUG
        if (debugging_on) DLIF_trace("symnum=%d\n", hash_nchain);
#endif
    }
    else
    {
        DLIF_warning(DLWT_MISC,
                     "Mandatory dynamic tag DT_HASH is not found!\n");
        return FALSE;
    }

    /*-----------------------------------------------------------------------*/
    /* Read dynamic symbol table.                                            */
    /*-----------------------------------------------------------------------*/
    if (symtab_offset)
    {
        int j = 0;
        DLIF_fseek(fd, symtab_offset, LOADER_SEEK_SET);
        dyn_module->symtab =
                   DLIF_malloc(dyn_module->symnum * sizeof(struct Elf32_Sym));
        if(dyn_module->symtab == NULL)
            return FALSE;
        DLIF_fread(dyn_module->symtab, sizeof(struct Elf32_Sym),
                   dyn_module->symnum, fd);
        if (dyn_module->wrong_endian)
        {
            for (j = 0; j < dyn_module->symnum; j++)
                DLIMP_change_sym_endian(dyn_module->symtab + j);
        }

        /*-------------------------------------------------------------------*/
        /* The st_name field of an Elf32_Sym entity is an offset into the    */
        /* string table. Convert it into a pointer to the string.            */
        /*-------------------------------------------------------------------*/
        if (strtab_offset)
            for (j = 0; j < dyn_module->symnum; j++) {
                dyn_module->symtab[j].st_name +=
                        (Elf32_Word) dyn_module->strtab;
#if LOADER_DEBUG
            if (debugging_on)
                DLIF_trace("Name of dynamic entry: %s\n",
                           dyn_module->symtab[j].st_name);
#endif
            }
    }
    else
    {
        DLIF_warning(DLWT_MISC,
                     "Mandatory dynamic tag DT_SYMTAB is not found!\n");
        return FALSE;
    }

    /*-----------------------------------------------------------------------*/
    /* Read the SONAME.                                                      */
    /*-----------------------------------------------------------------------*/
    if (!soname_found)
    {
        DLIF_warning(DLWT_MISC, "Dynamic tag DT_SONAME is not found!\n");
        dyn_module->name = DLIF_malloc(sizeof(char));
        if(dyn_module->name)
            *dyn_module->name = '\0';
        else
            return FALSE;
    }
    else
    {
        dyn_module->name =
                  DLIF_malloc(strlen(dyn_module->strtab + soname_offset) + 1);
        if(dyn_module->name) {
            strcpy(dyn_module->name, dyn_module->strtab + soname_offset);
#if LOADER_DEBUG
            if (debugging_on)
                DLIF_trace("Name of dynamic object: %s\n", dyn_module->name);
#endif
        }
        else {
            DLIF_error(DLET_MISC, "Error allocating memory %d.\n",__LINE__);
            return FALSE;
        }
    }

    return TRUE;
}


/*****************************************************************************/
/* dload_dynamic_information()                                               */
/*                                                                           */
/*    Given a dynamic module with a dynamic segment which is located via     */
/*    given dynamic segment index, make a local copy of the dynamic table    */
/*    in the dynamic module object, then process the dynamic tag entries in  */
/*    the table.                                                             */
/*                                                                           */
/*****************************************************************************/
static BOOL dload_dynamic_information(LOADER_FILE_DESC *fd,
                                      DLIMP_Dynamic_Module *dyn_module,
                                      Elf32_Word dyn_seg_idx)
{
    /*-----------------------------------------------------------------------*/
    /* Read a copy of the dynamic table into the dynamic module object.      */
    /*-----------------------------------------------------------------------*/
    copy_dynamic_table(fd, dyn_module, dyn_seg_idx);

    /*-----------------------------------------------------------------------*/
    /* Process dynamic entries in the dynamic table.  If any problems are    */
    /* encountered, the loader should emit an error or warning and return    */
    /* FALSE here.                                                           */
    /*-----------------------------------------------------------------------*/
    return process_dynamic_table(fd, dyn_module);
}

/*****************************************************************************/
/* check_circular_dependency()                                               */
/*                                                                           */
/*    Determine whether a dynamic module is already in the process of being  */
/*    loaded before we try to start loading it again.  If it is already      */
/*    being loaded, then the dynamic loader has detected a circular          */
/*    dependency.  An error will be emitted and the load will be aborted.    */
/*                                                                           */
/*****************************************************************************/
static BOOL check_circular_dependency(DLOAD_HANDLE handle,
                                      const char *dyn_mod_name)
{
    /*-----------------------------------------------------------------------*/
    /* Check the name of the given dependency module to be loaded against    */
    /* the list of modules that are currently in the process of being        */
    /* loaded.  Report an error if any circular dependencies are detected.   */
    /*-----------------------------------------------------------------------*/
    int i;
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;

    for (i = 0; i < pHandle->DLIMP_module_dependency_list.size; i++)
        if (!strcmp(dyn_mod_name,
            ((char**)(pHandle->DLIMP_module_dependency_list.buf))[i]))
        {
            DLIF_error(DLET_MISC,
                       "Circular dependency detected, '%s' is already in the "
                       "process of loading.\n", dyn_mod_name);
            return FALSE;
        }

    return TRUE;
}

/*****************************************************************************/
/* dload_dynamic_segment()                                                   */
/*                                                                           */
/*    Find the dynamic segment in the given ELF module, if there is one.     */
/*    If there is a dynamic segment, then make a local copy of the dynamic   */
/*    table in the dynamic module object provided, then process the dynamic  */
/*    tag entries in the table.                                              */
/*                                                                           */
/*    If there is no dynamic segment, then we return success from this       */
/*    function, marking the dynamic module as "not relocatable".             */
/*                                                                           */
/*****************************************************************************/
static BOOL dload_dynamic_segment(DLOAD_HANDLE handle,
                                  LOADER_FILE_DESC *fd,
                                  DLIMP_Dynamic_Module *dyn_module)
{
    /*-----------------------------------------------------------------------*/
    /* If we don't find dynamic segment, the relocatable flag will have been */
    /* set to false to indicate that the module is a static executable.  We  */
    /* still return TRUE from this function so that we can proceed with      */
    /* static loading.                                                       */
    /*-----------------------------------------------------------------------*/
    Elf32_Word dyn_seg_idx = 0;
    if (!find_dynamic_segment(dyn_module, &dyn_seg_idx))
        return TRUE;

    /*-----------------------------------------------------------------------*/
    /* Process the OSABI now, after we know if the module is relocatable.    */
    /*-----------------------------------------------------------------------*/
    if (!process_eiosabi(dyn_module))
    {
        DLIF_error(DLET_FILE, "Unsupported EI_OSABI value.\n");
        return FALSE;
    }

    /*-----------------------------------------------------------------------*/
    /* Read the dynamic table from the ELF file, then process the dynamic    */
    /* tags in the table.                                                    */
    /*-----------------------------------------------------------------------*/
    if (!dload_dynamic_information(fd, dyn_module, dyn_seg_idx))
        return FALSE;

    /*-----------------------------------------------------------------------*/
    /* Check to make sure that this module is not already being loaded.  If  */
    /* is, then it will cause a circular dependency to be introduced.        */
    /* Loader should detect circular dependencies and emit an error.         */
    /*-----------------------------------------------------------------------*/
    if (!check_circular_dependency(handle, dyn_module->name))
        return FALSE;

    return TRUE;
}

/*****************************************************************************/
/* COPY_SEGMENTS() -                                                         */
/*                                                                           */
/*   Copy all segments into host memory.                                     */
/*****************************************************************************/
static void copy_segments(DLOAD_HANDLE handle, LOADER_FILE_DESC* fp,
                          DLIMP_Dynamic_Module* dyn_module, int* data)
{
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;
    DLIMP_Loaded_Segment* seg =
       (DLIMP_Loaded_Segment*)(dyn_module->loaded_module->loaded_segments.buf);
    int s, seg_size = dyn_module->loaded_module->loaded_segments.size;
    void **va = DLIF_malloc(seg_size * sizeof(void*));

    if (!va) {
        DLIF_error(DLET_MISC, "Failed to allocate va in copy_segments.\n");
        return;
    }
    else
        *data = (int)va;

    for (s=0; s<seg_size; s++)
    {
        struct DLOAD_MEMORY_REQUEST targ_req;
        targ_req.fp = fp;
        targ_req.segment = seg[s].obj_desc;
        targ_req.offset = seg[s].phdr.p_offset;
        targ_req.flags = DLOAD_SF_relocatable;
        if (seg[s].phdr.p_flags & PF_X)
            seg[s].phdr.p_flags |= DLOAD_SF_executable;
        targ_req.align = seg[s].phdr.p_align;

        /*-------------------------------------------------------------------*/
        /* Copy segment data from the file into host buffer where it can     */
        /* be relocated.                                                     */
        /*-------------------------------------------------------------------*/
        DLIF_copy(pHandle->client_handle, &targ_req);

        va[s] = targ_req.host_address;

        /*-------------------------------------------------------------------*/
        /* Calculate offset for relocations.                                 */
        /*-------------------------------------------------------------------*/
        seg[s].reloc_offset = (int32_t)(targ_req.host_address) -
                              (int32_t)(seg[s].obj_desc->target_address);
    }
}

/*****************************************************************************/
/* WRITE_SEGMENTS() -                                                        */
/*                                                                           */
/*   Write all segments to target memory.                                    */
/*****************************************************************************/
static void write_segments(DLOAD_HANDLE handle, LOADER_FILE_DESC* fp,
                          DLIMP_Dynamic_Module* dyn_module, int* data)
{
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;
    DLIMP_Loaded_Segment* seg =
        (DLIMP_Loaded_Segment*)(dyn_module->loaded_module->loaded_segments.buf);
    int s, seg_size = dyn_module->loaded_module->loaded_segments.size;
    void **va = (void *)*data;

    if (!va) {
        DLIF_error(DLET_MISC, "Invalid host virtual address array passed into"
                   "write_segments.\n");
        return;
    }

    for (s=0; s<seg_size; s++)
    {
        struct DLOAD_MEMORY_REQUEST targ_req;
        targ_req.fp = fp;
        targ_req.segment = seg[s].obj_desc;
        targ_req.offset = seg[s].phdr.p_offset;
        targ_req.flags = DLOAD_SF_relocatable;
        if (seg[s].phdr.p_flags & PF_X)
            seg[s].phdr.p_flags |= DLOAD_SF_executable;
        targ_req.align = seg[s].phdr.p_align;
        targ_req.host_address = va[s];

        /*-------------------------------------------------------------------*/
        /* Copy segment data from the file into host buffer where it can     */
        /* be relocated.                                                     */
        /*-------------------------------------------------------------------*/
        DLIF_write(pHandle->client_handle, &targ_req);
    }

    DLIF_free(va);
}

/*****************************************************************************/
/* DLOAD_initialize()                                                        */
/*                                                                           */
/*    Construct and initialize data structures internal to the dynamic       */
/*    loader core.                                                           */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/*                                                                           */
/*    This implementation of DLOAD_initialize() will set up the list of      */
/*    dependency modules maintained by the loader core.  This list contains  */
/*    the names of the files that the loader is in the process of loading.   */
/*    The list is used to keep track of what objects are waiting on their    */
/*    dependents to be loaded so thath circular dependencies can be detected */
/*    and reported by the core loader.                                       */
/*                                                                           */
/*****************************************************************************/
void DLOAD_initialize(DLOAD_HANDLE handle)
{
     LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;

    /*-----------------------------------------------------------------------*/
    /* Set up initial objects_loading queue.                                 */
    /*-----------------------------------------------------------------------*/
    AL_initialize(&pHandle->DLIMP_module_dependency_list,
                  sizeof (const char*), 1);
}


/*****************************************************************************/
/* DLOAD_finalize()                                                          */
/*                                                                           */
/*    Destroy and finalize data structures internal to the dynamic           */
/*    loader core.                                                           */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/*                                                                           */
/*    This implementation of DLOAD_finalize() will destroy the list of       */
/*    dependency modules maintained by the loader core that is created       */
/*    during DLOAD_initialize().                                             */
/*                                                                           */
/*****************************************************************************/
void DLOAD_finalize(DLOAD_HANDLE handle)
{
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;

    /*-----------------------------------------------------------------------*/
    /* Destroy initial objects_loading queue.                                */
    /*-----------------------------------------------------------------------*/
    AL_destroy(&pHandle->DLIMP_module_dependency_list);
}


/*****************************************************************************/
/* dload_static_executable()                                                 */
/*                                                                           */
/*    Account for target memory allocated to static executable and wrap up   */
/*    loading.  No relocation is necessary.                                  */
/*                                                                           */
/*****************************************************************************/
static int32_t dload_static_executable(DLOAD_HANDLE handle,
                                       LOADER_FILE_DESC *fd,
                                       DLIMP_Dynamic_Module *dyn_module)
{
    int32_t local_file_handle = 0;

#if LOADER_DEBUG
    if (debugging_on) DLIF_trace("Starting dload_static_executable() ...\n");
#endif

    /*-----------------------------------------------------------------------*/
    /*-----------------------------------------------------------------------*/
    /* Set entry point for static executable and attempt to allocate target  */
    /* memory for the static executable.                                     */
    /*-----------------------------------------------------------------------*/
    dyn_module->loaded_module->entry_point = dyn_module->fhdr.e_entry;
    if (load_static_segment(handle, fd, dyn_module) &&
        load_object(fd, dyn_module))
    {
        /*-------------------------------------------------------------------*/
        /* If successful, we'll want to detach the loaded module object from */
        /* the dynamic module object that created it.  Take note of the file */
        /* handle.                                                           */
        /*-------------------------------------------------------------------*/
        DLIMP_Loaded_Module *loaded_module = detach_loaded_module(dyn_module);
        if (loaded_module)
            local_file_handle = loaded_module->file_handle;
        else {
            DLIF_error(DLET_MISC, "Failed to detach module.\n");
            delete_DLIMP_Dynamic_Module(handle, &dyn_module);
            return local_file_handle;
        }
   }

    /*-----------------------------------------------------------------------*/
    /* Static load failed.  Flag an error.                                   */
    /*-----------------------------------------------------------------------*/
    else
    {
        DLIF_trace("%s:%d EMEMORY\n",__func__,__LINE__);
        DLIF_error(DLET_MEMORY,
                   "Failed to allocate target memory for static executable.\n");
    }

    /*-----------------------------------------------------------------------*/
    /* Destruct dynamic module object.                                       */
    /*-----------------------------------------------------------------------*/
    delete_DLIMP_Dynamic_Module(handle, &dyn_module);

#if LOADER_DEBUG
    if (debugging_on) DLIF_trace("Finished dload_static_executable()\n");
#endif

    return local_file_handle;
}

/*****************************************************************************/
/* process_dynamic_module_relocations()                                      */
/*                                                                           */
/*    Make a host-accessible copy of all of the segments, process all        */
/*    relocation entries associated with the given module within that        */
/*    space, then write the updated segment buffers back out to target       */
/*    memory.                                                                */
/*                                                                           */
/*****************************************************************************/
static void process_dynamic_module_relocations(DLOAD_HANDLE handle,
                                               LOADER_FILE_DESC *fd,
                                               DLIMP_Dynamic_Module *dyn_module)
{
    int data = 0;

#if LOADER_DEBUG || LOADER_PROFILE
    if(debugging_on || profiling_on)
    {
        DLIF_trace("Running relocate()...\n");
        if (profiling_on) profile_start_clock();
    }
#endif

    /*-----------------------------------------------------------------------*/
    /* Copy segments from file to host memory                                */
    /*-----------------------------------------------------------------------*/
    copy_segments(handle, fd, dyn_module, &data);

   /*------------------------------------------------------------------------*/
   /* Process dynamic relocations.                                           */
   /*------------------------------------------------------------------------*/
#if ARM_TARGET
   if (is_arm_module(&dyn_module->fhdr))
      DLREL_relocate(handle, fd, dyn_module);
#endif

#if C60_TARGET
   if (is_c60_module(&dyn_module->fhdr))
      DLREL_relocate_c60(handle, fd, dyn_module);
#endif

    /*-----------------------------------------------------------------------*/
    /* Write segments from host memory to target memory                      */
    /*-----------------------------------------------------------------------*/
    write_segments(handle, fd, dyn_module, &data);

#if LOADER_DEBUG || LOADER_PROFILE
    /*-----------------------------------------------------------------------*/
    /* Report timing and progress information for relocation step.           */
    /*-----------------------------------------------------------------------*/
    if (debugging_on || profiling_on)
    {
        if (profiling_on)
        {
            profile_stop_clock();
            DLIF_trace("Took %d cycles.\n", (int)profile_cycle_count());
            DLIF_trace("Total reloc time: %d\n", (int)DLREL_total_reloc_time);
            DLIF_trace("Time per relocation: %d\n",
                       (DLREL_relocations ?
                        (int)(DLREL_total_reloc_time / DLREL_relocations) : 0));
        }

        DLIF_trace("Number of relocations: %d\n", DLREL_relocations);
        DLIF_trace("\nAbout to run load_object()...");
        DLREL_total_reloc_time = DLREL_relocations = 0;
        if (profiling_on) profile_start_clock();
    }
#endif
}

/*****************************************************************************/
/* execute_module_pre_initialization()                                       */
/*                                                                           */
/*    Given a dynamic module object, execute any pre-initialization          */
/*    functions for the specified dynamic executable module. Such functions  */
/*    must be provided by the user and their addresses written to the        */
/*    .preinit_array section. These functions should be executed in the      */
/*    order that they are specified in the array before the initialization   */
/*    process for this dynamic executable module begins.                     */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/*                                                                           */
/*    Note that dynamic shared objects (libraries) should not have a         */
/*    .preinit_array (should be caught during static link-time if the user   */
/*    attempts to link a .preinit_array section into a shared object.        */
/*                                                                           */
/*****************************************************************************/
static void execute_module_pre_initialization(DLOAD_HANDLE handle, DLIMP_Dynamic_Module *dyn_module)
{
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;

    /*-----------------------------------------------------------------------*/
    /* Check for presence of DT_PREINIT_ARRAY and DT_PREINIT_ARRAYSZ         */
    /* dynamic tags associated with this module. The dyn_module object will  */
    /* hold the relevant indices into the local copy of the dynamic table.   */
    /* The value of the DT_INIT_ARRAY tag will have been updated after       */
    /* placement of the  module was completed.                               */
    /*-----------------------------------------------------------------------*/
    if (dyn_module->preinit_arraysz != 0)
    {
        /*-------------------------------------------------------------------*/
        /* Retrieve the address of the .preinit_array section from the value */
        /* of the DT_PREINIT_ARRAY tag.                                      */
        /*-------------------------------------------------------------------*/
        TARGET_ADDRESS preinit_array_loc = (TARGET_ADDRESS)
                (dyn_module->dyntab[dyn_module->preinit_array_idx].d_un.d_ptr);

        /*-------------------------------------------------------------------*/
        /* Now make a loader-accessible copy of the .preinit_array section.  */
        /*-------------------------------------------------------------------*/
        int32_t i;
        int32_t num_preinit_fcns =
                            dyn_module->preinit_arraysz/sizeof(TARGET_ADDRESS);
        TARGET_ADDRESS *preinit_array_buf = (TARGET_ADDRESS *)
                                      DLIF_malloc(dyn_module->preinit_arraysz);
        if(preinit_array_buf) {
            DLIF_read(pHandle->client_handle, preinit_array_buf, 1,
                      dyn_module->preinit_arraysz,
                      (TARGET_ADDRESS)preinit_array_loc);

            /*---------------------------------------------------------------*/
            /* Call each function whose address occupies an entry in the     */
            /* array in the order that it appears in the array. The sizeof   */
            /* the array is provided by the preinit_arraysz field in the     */
            /* dynamic module (copied earlier when the dynamic table was     */
            /* read in). We need to divide the sizeof value down to get the  */
            /* number of actual entries in the array.                        */
            /*---------------------------------------------------------------*/
            for (i = 0; i < num_preinit_fcns; i++)
                DLIF_execute(pHandle->client_handle,
                             (TARGET_ADDRESS)(preinit_array_buf[i]));

            DLIF_free(preinit_array_buf);
        }
    }
}

/*****************************************************************************/
/* execute_module_initialization()                                           */
/*                                                                           */
/*    Given a dynamic module object, execute initialization function(s) for  */
/*    all global and static data objects that are defined in the module      */
/*    which require construction. The user may also provide a custom         */
/*    iniitialization function that needs to be executed before the compiler */
/*    generated static initialization functions are executed.                */
/*                                                                           */
/*****************************************************************************/
static void execute_module_initialization(DLOAD_HANDLE handle,
                                          DLIMP_Dynamic_Module *dyn_module)
{
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;

    /*-----------------------------------------------------------------------*/
    /* Check for presence of a DT_INIT dynamic tag associated with this      */
    /* module. The dynamic module will hold the index into the local copy of */
    /* the dynamic table. This entry in the dynamic table will have been     */
    /* updated after placement of the module is completed.                   */
    /*-----------------------------------------------------------------------*/
    if (dyn_module->init_idx != -1)
    {
        /*-------------------------------------------------------------------*/
        /* Retrieve the address of the initialization function from the      */
        /* value of the DT_INIT tag, and get the client to execute the       */
        /* function.                                                         */
        /*-------------------------------------------------------------------*/
        TARGET_ADDRESS init_fcn = (TARGET_ADDRESS)
                         (dyn_module->dyntab[dyn_module->init_idx].d_un.d_ptr);
        DLIF_execute(pHandle->client_handle, init_fcn);
    }

    /*-----------------------------------------------------------------------*/
    /* Check for presence of a DT_INIT_ARRAY and DT_INIT_ARRAYSZ dynamic     */
    /* tags associated with this module. The dyn_module object will hold the */
    /* relevant indices into the local copy of the dynamic table. The value  */
    /* of the DT_INIT_ARRAY tag will have been updated after placement of    */
    /* the module was completed.                                             */
    /*-----------------------------------------------------------------------*/
    if (dyn_module->init_arraysz != 0)
    {
        /*-------------------------------------------------------------------*/
        /* Retrieve the address of the .init_array section from the value of */
        /* DT_INIT_ARRAY tag.                                                */
        /*-------------------------------------------------------------------*/
        TARGET_ADDRESS init_array_loc = (TARGET_ADDRESS)
                   (dyn_module->dyntab[dyn_module->init_array_idx].d_un.d_ptr);

        /*-------------------------------------------------------------------*/
        /* Now make a loader-accessible copy of the .init_array section.     */
        /*-------------------------------------------------------------------*/
        int32_t i;
        int32_t num_init_fcns = dyn_module->init_arraysz/sizeof(TARGET_ADDRESS);
        TARGET_ADDRESS *init_array_buf = (TARGET_ADDRESS *)
                                         DLIF_malloc(dyn_module->init_arraysz);
        if(init_array_buf) {
            DLIF_read(pHandle->client_handle, init_array_buf, 1,
                      dyn_module->init_arraysz, (TARGET_ADDRESS)init_array_loc);

            /*---------------------------------------------------------------*/
            /* Call each function whose address occupies an entry in the     */
            /* array in the order that they appear in the array. The size of */
            /* the array is provided by the init_arraysz field in the        */
            /* dynamic module (copied earlier when the dynamic table was     */
            /* read in).                                                     */
            /*---------------------------------------------------------------*/
            for (i = 0; i < num_init_fcns; i++)
                DLIF_execute(pHandle->client_handle,
                             (TARGET_ADDRESS)(init_array_buf[i]));

            DLIF_free(init_array_buf);
        }
    }
}

/*****************************************************************************/
/* relocate_dependency_graph_modules()                                       */
/*                                                                           */
/*    For each dynamic module on the dependency stack, process dynamic       */
/*    relocation entries then perform initialization for all global and      */
/*    static objects that are defined in tha given module. The stack is      */
/*    emptied from the top (LIFO).  Each dynamic module object is popped     */
/*    off the top of the stack, the module gets relocated, its global and    */
/*    static objects that need to be constructed will be constructed, and    */
/*    then, after detaching the loaded module object from its dynamic        */
/*    module, the dynamic module object is destructed.                       */
/*                                                                           */
/*****************************************************************************/
static int32_t relocate_dependency_graph_modules(DLOAD_HANDLE handle,
                                               LOADER_FILE_DESC *fd,
                                               DLIMP_Dynamic_Module *dyn_module)
{
    /*-----------------------------------------------------------------------*/
    /* Processing of relocations will only be triggered when this function   */
    /* is called from the top-level object module (at the bottom of the      */
    /* dependency graph stack).                                              */
    /*-----------------------------------------------------------------------*/
    int32_t local_file_handle = dyn_module->loaded_module->file_handle;
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;
    dynamic_module_ptr_Stack_Node *ptr =
        pHandle->DLIMP_dependency_stack.bottom_ptr;
    if (ptr && (ptr->value != dyn_module)) {return local_file_handle;}

    /*-----------------------------------------------------------------------*/
    /* Assign DSBT indices.                                                  */
    /*-----------------------------------------------------------------------*/
    DLIF_assign_dsbt_indices();

    /*-----------------------------------------------------------------------*/
    /* Update the content of all DSBTs for any module that uses the DSBT     */
    /* model.                                                                */
    /*-----------------------------------------------------------------------*/
    DLIF_update_all_dsbts();

    /*-----------------------------------------------------------------------*/
    /* Ok, we are ready to process relocations. The relocation tables        */
    /* associated with dependent files will be processed first. Consume      */
    /* dynamic module objects from the dependency graph stack from dependents*/
    /* to the root of the dependency graph.                                  */
    /*-----------------------------------------------------------------------*/
    while (pHandle->DLIMP_dependency_stack.size > 0)
    {
        DLIMP_Dynamic_Module *dyn_mod_ptr =
                      dynamic_module_ptr_pop(&pHandle->DLIMP_dependency_stack);

        /*-------------------------------------------------------------------*/
        /* Process dynamic relocations associated with this module.          */
        /*-------------------------------------------------------------------*/
        process_dynamic_module_relocations(handle, dyn_mod_ptr->fd,
                                           dyn_mod_ptr);

        /*------------------------------------------------------------------ */
        /* __c_args__ points to the beginning of the .args section, if there */
        /* is one.  Record this pointer in the ELF file internal data object.*/
        /*-------------------------------------------------------------------*/
        DLSYM_lookup_local_symtab("__c_args__", dyn_mod_ptr->symtab,
                                  dyn_mod_ptr->symnum,
                                  (Elf32_Addr *)&dyn_mod_ptr->c_args);

        /*-------------------------------------------------------------------*/
        /* Pick up entry point address from ELF file header.                 */
        /*   We currently only support a single entry point into the ELF     */
        /*   file.  To support Braveheart notion of nodes, with multiple     */
        /*   entry points, we'll need to get the list of entry points        */
        /*   associated with a node, then add capability to the "execute"    */
        /*   command to select the entry point that we want to start         */
        /*   executing from.                                                 */
        /*-------------------------------------------------------------------*/
        dyn_mod_ptr->loaded_module->entry_point = dyn_mod_ptr->fhdr.e_entry;

        /*-------------------------------------------------------------------*/
        /* Copy command-line arguments into args section and deal with DSBT  */
        /* issues (copy DSBT to its run location).                           */
        /*-------------------------------------------------------------------*/
        load_object(dyn_mod_ptr->fd, dyn_mod_ptr);

        /*-------------------------------------------------------------------*/
        /* Perform initialization, if needed, for this module.               */
        /*-------------------------------------------------------------------*/
        execute_module_initialization(handle, dyn_mod_ptr);

        /*-------------------------------------------------------------------*/
        /* Free all dependent file pointers.                                 */
        /*-------------------------------------------------------------------*/
        if (dyn_mod_ptr->fd != fd)
        {
            DLIF_fclose(dyn_mod_ptr->fd);
            dyn_mod_ptr->fd = NULL;
        }

        /*-------------------------------------------------------------------*/
        /* Detach loaded module object from the dynamic module object that   */
        /* created it, then throw away the dynamic module object.            */
        /*-------------------------------------------------------------------*/
        detach_loaded_module(dyn_mod_ptr);
        delete_DLIMP_Dynamic_Module(handle, &dyn_mod_ptr);
    }
    return local_file_handle;
}

/*****************************************************************************/
/* DLOAD_load()                                                              */
/*                                                                           */
/*    Dynamically load the specified file and return a file handle for the   */
/*    loaded file.  If the load fails, this function will return a value of  */
/*    zero (0) for the file handle.                                          */
/*                                                                           */
/*    The core loader must have read access to the file pointed to by fd.    */
/*                                                                           */
/*****************************************************************************/
int32_t DLOAD_load(DLOAD_HANDLE handle, LOADER_FILE_DESC *fd, int argc,
                   char** argv)
{
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;
    DLIMP_Dynamic_Module *dyn_module = new_DLIMP_Dynamic_Module(fd);

    if (!dyn_module)
        return 0;

#if LOADER_DEBUG
    /*-----------------------------------------------------------------------*/
    /* Spit out some loader progress information when we begin loading an    */
    /* object.                                                               */
    /*-----------------------------------------------------------------------*/
    if (debugging_on) DLIF_trace("Loading file...\n");
#endif

    /*-----------------------------------------------------------------------*/
    /* If no access to a program was provided, there is nothing to do.       */
    /*-----------------------------------------------------------------------*/
    if (!fd)
    {
        DLIF_error(DLET_FILE, "Missing file specification.\n");
        delete_DLIMP_Dynamic_Module(handle, &dyn_module);
        return 0;
    }

    /*-----------------------------------------------------------------------*/
    /* Read file headers and dynamic information into dynamic module.        */
    /*-----------------------------------------------------------------------*/
    if (!dload_headers(handle, fd, dyn_module)) {
        delete_DLIMP_Dynamic_Module(handle, &dyn_module);
        return 0;
    }

    /*-----------------------------------------------------------------------*/
    /* Find the dynamic segment, if there is one, and read dynamic           */
    /* information from the ELF object file into the dynamic module data     */
    /* structure associated with this file.                                  */
    /*-----------------------------------------------------------------------*/
    if (!dload_dynamic_segment(handle, fd, dyn_module)) {
        delete_DLIMP_Dynamic_Module(handle, &dyn_module);
        return 0;
    }

    /*-----------------------------------------------------------------------*/
    /* ??? We currently don't have a way of finding the .args section.  So   */
    /*    we are hard-wiring the address of the .args section.  The proposed */
    /*    solution for this problem is to invent a special segment type or   */
    /*    dynamic tag(s) that identify the location and size of the .args    */
    /*    section for the dynamic loader.                                    */
    /*-----------------------------------------------------------------------*/
    /*HACK ---> */dyn_module->c_args = (uint8_t*)(0x02204000); /* <--- HACK*/

    /*-----------------------------------------------------------------------*/
    /* Record argc and argv pointers with the dynamic module record.         */
    /*-----------------------------------------------------------------------*/
    dyn_module->argc = argc;
    dyn_module->argv = argv;
    if (dyn_module->name == NULL) {
        dyn_module->name = DLIF_malloc(sizeof(char));
        if(dyn_module->name)
            *dyn_module->name = '\0';
        else {
            delete_DLIMP_Dynamic_Module(handle, &dyn_module);
            return 0;
        }
    }

    /*-----------------------------------------------------------------------*/
    /* Perform sanity checking on the read-in ELF file.                      */
    /*-----------------------------------------------------------------------*/
    if (!is_valid_elf_object_file(fd, dyn_module))
    {
        DLIF_error(DLET_FILE, "Attempt to load invalid ELF file, '%s'.\n",
                   dyn_module->name);
        delete_DLIMP_Dynamic_Module(handle, &dyn_module);
        return 0;
    }

#if LOADER_DEBUG || LOADER_PROFILE
    /*-----------------------------------------------------------------------*/
    /* Stop clock on initialization of ELF file information.  Start clock on */
    /* initialization of ELF module.                                         */
    /*-----------------------------------------------------------------------*/
    if (debugging_on || profiling_on)
    {
        DLIF_trace("Finished dload_dynamic_segment.\n");
        if (profiling_on)
        {
            profile_stop_clock();
            DLIF_trace("Took %d cycles.\n", (int)profile_cycle_count());
        }
    }
#endif

    /*-----------------------------------------------------------------------*/
    /* Initialize internal ELF module and segment structures.  Sets          */
    /* loaded_module in *dyn_module.  This also deals with assigning a file  */
    /* handle and bumping file handle counter.                               */
    /*-----------------------------------------------------------------------*/
    initialize_loaded_module(handle, dyn_module);

    /*-----------------------------------------------------------------------*/
    /* Append Module structure to loaded object list.                        */
    /*-----------------------------------------------------------------------*/
    loaded_module_ptr_enqueue(&pHandle->DLIMP_loaded_objects,
                              dyn_module->loaded_module);

    /*-----------------------------------------------------------------------*/
    /* Support static loading as special case.                               */
    /*-----------------------------------------------------------------------*/
    if (!dyn_module->relocatable)
        return dload_static_executable(handle, fd, dyn_module);

    /*-----------------------------------------------------------------------*/
    /* Get space & address for segments, and offset symbols and program      */
    /* header table to reflect the relocated address.  Also offset the       */
    /* addresses in the internal Segment structures used by the Module       */
    /* structure.  Note that this step needs to be performed prior and in    */
    /* addition to the relocation entry processing.                          */
    /*-----------------------------------------------------------------------*/
    if (!allocate_dynamic_segments_and_relocate_symbols(handle, fd, dyn_module)) {
        loaded_module_ptr_remove(&pHandle->DLIMP_loaded_objects,
                              dyn_module->loaded_module);
        delete_DLIMP_Dynamic_Module(handle, &dyn_module);
        return 0;
    }
    /*-----------------------------------------------------------------------*/
    /* Execute any user defined pre-initialization functions that may be     */
    /* associated with a dynamic executable module.                          */
    /*-----------------------------------------------------------------------*/
    if (dyn_module->fhdr.e_type == ET_EXEC)
        execute_module_pre_initialization(handle, dyn_module);

    /*-----------------------------------------------------------------------*/
    /* Append current ELF file to list of objects currently loading.         */
    /* This is used to detect circular dependencies while we are processing  */
    /* the dependents of this file.                                          */
    /*-----------------------------------------------------------------------*/
    AL_append(&pHandle->DLIMP_module_dependency_list, &dyn_module->name);

    /*-----------------------------------------------------------------------*/
    /* Push this dynamic module object onto the dependency stack.            */
    /* All of the modules on the stack will get relocated after all of the   */
    /* dependent files have been loaded and allocated.                       */
    /*-----------------------------------------------------------------------*/
    dynamic_module_ptr_push(&pHandle->DLIMP_dependency_stack, dyn_module);

    /*-----------------------------------------------------------------------*/
    /* If this object file uses the DSBT model, then register a DSBT index   */
    /* request with the client's DSBT support management.                    */
    /*-----------------------------------------------------------------------*/
    if (is_dsbt_module(dyn_module) &&
        !DLIF_register_dsbt_index_request(handle,
                                         dyn_module->name,
                                         dyn_module->loaded_module->file_handle,
                                         dyn_module->dsbt_index)) {
        dynamic_module_ptr_pop(&pHandle->DLIMP_dependency_stack);
        loaded_module_ptr_remove(&pHandle->DLIMP_loaded_objects,
                                  dyn_module->loaded_module);
        delete_DLIMP_Dynamic_Module(handle, &dyn_module);
        return 0;
    }

    /*-----------------------------------------------------------------------*/
    /* Load this ELF file's dependents (all files on its DT_NEEDED list).    */
    /* Do not process relocation entries for anyone in the dependency graph  */
    /* until all modules in the graph are loaded and allocated.              */
    /*-----------------------------------------------------------------------*/
    if (!dload_and_allocate_dependencies(handle, dyn_module)) {
        dynamic_module_ptr_pop(&pHandle->DLIMP_dependency_stack);
        loaded_module_ptr_remove(&pHandle->DLIMP_loaded_objects,
                                  dyn_module->loaded_module);
        delete_DLIMP_Dynamic_Module(handle, &dyn_module);
        return 0;
    }

    /*-----------------------------------------------------------------------*/
    /* Remove the current ELF file from the list of files that are in the    */
    /* process of loading.                                                   */
    /*-----------------------------------------------------------------------*/
    pHandle->DLIMP_module_dependency_list.size--;

    /*-----------------------------------------------------------------------*/
    /* __c_args__ points to the beginning of the .args section, if there is  */
    /* one.  Record this pointer in the ELF file internal data object.       */
    /*-----------------------------------------------------------------------*/
    DLSYM_lookup_local_symtab("__c_args__", dyn_module->symtab,
                              dyn_module->symnum,
                              (Elf32_Addr *)&dyn_module->c_args);


    return relocate_dependency_graph_modules(handle, fd, dyn_module);
}

BOOL DLOAD_get_entry_names_info(DLOAD_HANDLE handle,
                           uint32_t file_handle,
                           int32_t *entry_pt_cnt,
                           int32_t *entry_pt_max_name_len)
{
    /*-----------------------------------------------------------------------*/
    /* Spin through list of loaded files until we find the file handle we    */
    /* are looking for.  Then build a list of entry points from that file's  */
    /* symbol table.                                                         */
    /*-----------------------------------------------------------------------*/
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;
    loaded_module_ptr_Queue_Node* ptr;
    for (ptr = pHandle->DLIMP_loaded_objects.front_ptr;
         ptr != NULL;
         ptr = ptr->next_ptr)
    {
        if (ptr->value->file_handle == file_handle)
        {
            DLIMP_Loaded_Module *module = ptr->value;
            struct Elf32_Sym *symtab;
            int i;

            /*---------------------------------------------------------------*/
            /* Any symbol in our file's symbol table is considered a valid   */
            /* entry point.                                                  */
            /*---------------------------------------------------------------*/
            symtab = (struct Elf32_Sym*)module->gsymtab;
            *entry_pt_cnt = module->gsymnum;
            *entry_pt_max_name_len = 0;
            for (i = 0; i < module->gsymnum; i++)
            {
                const char *sym_name = (const char *)symtab[i].st_name;

                if ((strlen(sym_name) + 1) > *entry_pt_max_name_len)
                    *entry_pt_max_name_len = strlen(sym_name) + 1;
            }

            return TRUE;
        }
    }

    /*-----------------------------------------------------------------------*/
    /* We didn't find the file we were looking for, return false.            */
    /*-----------------------------------------------------------------------*/
    return FALSE;
}

/*****************************************************************************/
/* DLOAD_get_entry_names()                                                   */
/*                                                                           */
/*    Build a list of entry point names for a loaded object.  Currently,     */
/*    any global symbol in the module is considered a valid entry point      */
/*    regardless of whether it is defined in code or associated with a       */
/*    data object.  We would need to process the content of the symbol       */
/*    table entry or its debug information to determine whether it is a      */
/*    valid entry point or not.                                              */
/*                                                                           */
/*****************************************************************************/
BOOL DLOAD_get_entry_names(DLOAD_HANDLE handle,
                           uint32_t file_handle,
                           int32_t *entry_pt_cnt,
                           char ***entry_pt_names)
{
    /*-----------------------------------------------------------------------*/
    /* Spin through list of loaded files until we find the file handle we    */
    /* are looking for.  Then build a list of entry points from that file's  */
    /* symbol table.                                                         */
    /*-----------------------------------------------------------------------*/
    char **names;
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;
    loaded_module_ptr_Queue_Node* ptr;
    for (ptr = pHandle->DLIMP_loaded_objects.front_ptr;
         ptr != NULL;
         ptr = ptr->next_ptr)
    {
        if (ptr->value->file_handle == file_handle)
        {
            DLIMP_Loaded_Module *module = ptr->value;
            struct Elf32_Sym *symtab;
            int i;

            /*---------------------------------------------------------------*/
            /* Any symbol in our file's symbol table is considered a valid   */
            /* entry point.                                                  */
            /*---------------------------------------------------------------*/
            symtab = (struct Elf32_Sym*)module->gsymtab;
            if (*entry_pt_cnt < module->gsymnum) {
                DLIF_error(DLET_MEMORY, "There are %d entry points, "
                           "only %d spaces provided.\n",
                           module->gsymnum,*entry_pt_cnt);
                return FALSE;
            }

            names = *entry_pt_names;
            for (i = 0; i < module->gsymnum; i++)
            {
                const char *sym_name = (const char *)symtab[i].st_name;
                strcpy(names[i],sym_name);
            }

            return TRUE;
        }
    }

    /*-----------------------------------------------------------------------*/
    /* We didn't find the file we were looking for, return false.            */
    /*-----------------------------------------------------------------------*/
    return FALSE;
}

/*****************************************************************************/
/* DLOAD_get_entry_point()                                                   */
/*                                                                           */
/*    Given a file handle, return the entry point associated with that       */
/*    module in the *sym_val output parameter.                               */
/*                                                                           */
/*****************************************************************************/
BOOL DLOAD_get_entry_point(DLOAD_HANDLE handle, uint32_t file_handle,
                           TARGET_ADDRESS *sym_val)
{
    /*-----------------------------------------------------------------------*/
    /* Spin through list of loaded files until we find the file handle we    */
    /* are looking for.  Then return the entry point address associated with */
    /* that module.                                                          */
    /*-----------------------------------------------------------------------*/
    loaded_module_ptr_Queue_Node* ptr;
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;
    for (ptr = pHandle->DLIMP_loaded_objects.front_ptr;
         ptr != NULL;
         ptr = ptr->next_ptr)
        if (ptr->value->file_handle == file_handle)
        {
            *sym_val = (TARGET_ADDRESS)(ptr->value->entry_point);
            return TRUE;
        }

    /*-----------------------------------------------------------------------*/
    /* We didn't find the file we were looking for, return false.            */
    /*-----------------------------------------------------------------------*/
    return FALSE;
}

/*****************************************************************************/
/* DLOAD_query_symbol()                                                      */
/*                                                                           */
/*    Query the value of a global symbol from a specific file.  The value    */
/*    result will be written to *sym_val.  The function returns TRUE if the  */
/*    symbol was found, and FALSE if it wasn't.                              */
/*                                                                           */
/*****************************************************************************/
BOOL DLOAD_query_symbol(DLOAD_HANDLE handle,
                        uint32_t file_handle,
                        const char *sym_name,
                        TARGET_ADDRESS *sym_val)
{
    /*-----------------------------------------------------------------------*/
    /* Spin through list of loaded files until we find the file handle we    */
    /* are looking for.  Then return the value (target address) associated   */
    /* with the symbol we are looking for in that file.                      */
    /*-----------------------------------------------------------------------*/
    loaded_module_ptr_Queue_Node* ptr;
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;
    for (ptr = pHandle->DLIMP_loaded_objects.front_ptr;
         ptr != NULL;
         ptr = ptr->next_ptr)
    {
        if (ptr->value->file_handle == file_handle)
        {
            DLIMP_Loaded_Module *module = ptr->value;
            struct Elf32_Sym *symtab;
            int i;

            /*---------------------------------------------------------------*/
            /* Search through the symbol table by name.                      */
            /*---------------------------------------------------------------*/
            symtab = (struct Elf32_Sym*)module->gsymtab;
            for(i=0; i < module->gsymnum; i++)
            {
                if (!strcmp(sym_name, (const char *)symtab[i].st_name))
                {
                    *sym_val = (TARGET_ADDRESS) symtab[i].st_value;
                    return TRUE;
                }
            }
        }
    }

    /*-----------------------------------------------------------------------*/
    /* We didn't find the file we were looking for, return false.            */
    /*-----------------------------------------------------------------------*/
    return FALSE;
}



/*****************************************************************************/
/* unlink_loaded_module()                                                    */
/*                                                                           */
/*    Unlink a loaded module data object from the list of loaded objects,    */
/*    returning a pointer to the object so that it can be deconstructed.     */
/*                                                                           */
/*****************************************************************************/
static DLIMP_Loaded_Module *unlink_loaded_module(DLOAD_HANDLE handle,
                                         loaded_module_ptr_Queue_Node *back_ptr,
                                         loaded_module_ptr_Queue_Node *lm_node)
{
    DLIMP_Loaded_Module *loaded_module = lm_node->value;
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;
    loaded_module_ptr_remove(&pHandle->DLIMP_loaded_objects, lm_node->value);
    return loaded_module;
}

/*****************************************************************************/
/* execute_module_termination()                                              */
/*                                                                           */
/*    Execute termination functions associated with this loaded module.      */
/*    Termination functions are called in the reverse order as their         */
/*    corresponding initialization functions.                                */
/*                                                                           */
/*****************************************************************************/
static void execute_module_termination(DLOAD_HANDLE handle,
                                       DLIMP_Loaded_Module *loaded_module)
{
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;

    /*-----------------------------------------------------------------------*/
    /* If a DT_FINI_ARRAY dynamic tag was encountered for this module, spin  */
    /* through the array in reverse order, calling each function address     */
    /* stored in the array.                                                  */
    /*-----------------------------------------------------------------------*/
    if (loaded_module->fini_arraysz != 0)
    {
        /*-------------------------------------------------------------------*/
        /* Now make a loader-accessible copy of the .fini_array section.     */
        /*-------------------------------------------------------------------*/
        int32_t i;
        int32_t num_fini_fcns =
                            loaded_module->fini_arraysz/sizeof(TARGET_ADDRESS);
        TARGET_ADDRESS *fini_array_buf = (TARGET_ADDRESS *)
                                      DLIF_malloc(loaded_module->fini_arraysz);
        if(fini_array_buf) {
            DLIF_read(pHandle->client_handle, fini_array_buf, 1,
                      loaded_module->fini_arraysz,
                      (TARGET_ADDRESS)loaded_module->fini_array);

            /*---------------------------------------------------------------*/
            /* Now spin through the array in reverse order, executing each   */
            /* termination function whose address occupies an entry in the   */
            /* array.                                                        */
            /*---------------------------------------------------------------*/
            for (i = num_fini_fcns - 1; i >= 0; i--)
                DLIF_execute(pHandle->client_handle,
                             (TARGET_ADDRESS)(fini_array_buf[i]));

            DLIF_free(fini_array_buf);
        }
    }

    /*-----------------------------------------------------------------------*/
    /* If a DT_FINI dynamic tag was encountered for this module, call the    */
    /* function indicated by the tag's value to complete the termination     */
    /* process for this module.                                              */
    /*-----------------------------------------------------------------------*/
    if (loaded_module->fini != (Elf32_Addr)NULL)
        DLIF_execute(pHandle->client_handle,
                     (TARGET_ADDRESS)loaded_module->fini);
}

/*****************************************************************************/
/* remove_loaded_module()                                                    */
/*                                                                           */
/*    Find and unlink a loaded module data object from the list of loaded    */
/*    objects, then call its destructor to free the host memory associated   */
/*    with the loaded module and all of its loaded segments.                 */
/*                                                                           */
/*****************************************************************************/
static void remove_loaded_module(DLOAD_HANDLE handle,
                                 loaded_module_ptr_Queue_Node *lm_node)
{
     DLIMP_Loaded_Module *lm_object = NULL;
     loaded_module_ptr_Queue_Node *back_ptr = NULL;
     LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;

    if (lm_node != pHandle->DLIMP_loaded_objects.front_ptr)
        for (back_ptr = pHandle->DLIMP_loaded_objects.front_ptr;
             back_ptr->next_ptr != lm_node;
             back_ptr = back_ptr->next_ptr);

    lm_object = unlink_loaded_module(handle, back_ptr, lm_node);

    delete_DLIMP_Loaded_Module(handle, &lm_object);
}

/*****************************************************************************/
/* DLOAD_unload()                                                            */
/*                                                                           */
/*    Unload specified module (identified by its file handle) from target    */
/*    memory.  Free up any target memory that was allocated for the module's */
/*    segments and also any host heap memory that was allocated for the      */
/*    internal module and segment data structures.                           */
/*                                                                           */
/*    Return TRUE if program entry is actually destroyed.  This is a way of  */
/*    communicating to the client when it needs to actually remove debug     */
/*    information associated with this module (so that client does not have  */
/*    to maintain a use count that mirrors the program entry).               */
/*                                                                           */
/*****************************************************************************/
BOOL DLOAD_unload(DLOAD_HANDLE handle, uint32_t file_handle)
{
    loaded_module_ptr_Queue_Node* lm_node;
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;

    for (lm_node = pHandle->DLIMP_loaded_objects.front_ptr; lm_node != NULL;
         lm_node = lm_node->next_ptr)
    {
        if (lm_node->value->file_handle == file_handle)
        {
            --lm_node->value->use_count;
            if (lm_node->value->use_count == 0)
            {
                DLIMP_Loaded_Module *loaded_module =
                                    (DLIMP_Loaded_Module *)lm_node->value;
                int j;
                int *dep_file_handles;

                /*-----------------------------------------------------------*/
                /* Termination functions need to be executed in the reverse  */
                /* order as the corresponding initialization functions, so   */
                /* before we go unload this module's dependents, we need to  */
                /* perform the user/global/static termination functions      */
                /* associated with this module.                              */
                /*-----------------------------------------------------------*/
                execute_module_termination(handle, loaded_module);

                /*-----------------------------------------------------------*/
                /* Unload dependent modules via the client. Client needs to  */
                /* know when a dependent gets unloaded so that it can update */
                /* debug information.                                        */
                /*-----------------------------------------------------------*/
                dep_file_handles = (int*)(loaded_module->dependencies.buf);
                for (j = 0; j < loaded_module->dependencies.size; j++) {
                    DLIF_unload_dependent(pHandle->client_handle,
                                          dep_file_handles[j]);
                }

                /*-----------------------------------------------------------*/
                /* Find the predecessor node of the value we're deleting,    */
                /* because its next_ptr will need to be updated.             */
                /*                                                           */
                /* We can't keep a back pointer around because               */
                /* DLIF_unload_dependent() might free that node, making our  */
                /* pointer invalid.  Turn the Queue template into a doubly   */
                /* linked list if this overhead becomes a problem.           */
                /*-----------------------------------------------------------*/
                remove_loaded_module(handle, lm_node);

                /*-----------------------------------------------------------*/
                /* If all loaded objects have been unloaded (including the   */
                /* base image), then reset the machine to the default target */
                /* machine.  This only has an effect when multiple targets   */
                /* are supported, in which case the machine is set to        */
                /* EM_NONE.                                                  */
                /*-----------------------------------------------------------*/
                if (pHandle->DLIMP_loaded_objects.front_ptr == NULL)
                {
                    pHandle->DLOAD_TARGET_MACHINE = DLOAD_DEFAULT_TARGET_MACHINE;
                }

                return TRUE;
            }
        }
    }

    return FALSE;
}

/*****************************************************************************/
/* DLOAD_load_symbols()                                                      */
/*                                                                           */
/*    Load the symbols from the given file and make symbols available for    */
/*    global symbol linkage.                                                 */
/*                                                                           */
/*****************************************************************************/
int32_t DLOAD_load_symbols(DLOAD_HANDLE handle, LOADER_FILE_DESC *fd)
{
    DLIMP_Dynamic_Module *dyn_module = NULL;
    DLIMP_Loaded_Module *loaded_module = NULL;
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;

    /*-----------------------------------------------------------------------*/
    /* If no access to a program was provided, there is nothing to do.       */
    /*-----------------------------------------------------------------------*/
    if (!fd)
    {
        DLIF_error(DLET_FILE, "Missing file specification.\n");
        return 0;
    }

    dyn_module = new_DLIMP_Dynamic_Module(fd);

    /*-----------------------------------------------------------------------*/
    /* Ensure we have a valid dynamic module object from the constructor.    */
    /*-----------------------------------------------------------------------*/
    if (!dyn_module)
        return 0;

    /*-----------------------------------------------------------------------*/
    /* Record argc and argv pointers with the dynamic module record.         */
    /*-----------------------------------------------------------------------*/
    dyn_module->argc = 0;
    dyn_module->argv = NULL;

    /*-----------------------------------------------------------------------*/
    /* Read file headers and dynamic information into dynamic module.        */
    /*-----------------------------------------------------------------------*/
    if (!dload_headers(handle, fd, dyn_module)) {
        delete_DLIMP_Dynamic_Module(handle, &dyn_module);
        return 0;
    }

    /*-----------------------------------------------------------------------*/
    /* Find the dynamic segment, if there is one, and read dynamic           */
    /* information from the ELF object file into the dynamic module data     */
    /* structure associated with this file.                                  */
    /*-----------------------------------------------------------------------*/
    if (!dload_dynamic_segment(handle, fd, dyn_module)) {
        delete_DLIMP_Dynamic_Module(handle, &dyn_module);
        return 0;
    }

    /*-----------------------------------------------------------------------*/
    /* Perform sanity checking on the read-in ELF file.                      */
    /*-----------------------------------------------------------------------*/
    if (!is_valid_elf_object_file(fd, dyn_module))
    {
        DLIF_error(DLET_FILE, "Attempt to load invalid ELF file, '%s'.\n",
                   dyn_module->name);
        delete_DLIMP_Dynamic_Module(handle, &dyn_module);
        return 0;
    }

    /*-----------------------------------------------------------------------*/
    /* Initialize internal ELF module and segment structures.  Sets          */
    /* loaded_module in *dyn_module.  This also deals with assigning a file  */
    /* handle and bumping file handle counter.                               */
    /*-----------------------------------------------------------------------*/
    initialize_loaded_module(handle, dyn_module);

    /*-----------------------------------------------------------------------*/
    /* Add this module to the loaded module queue.                           */
    /* Detach the loaded module object from the dynamic module thath created */
    /* it. Ownership of the host memory allocated for the loaded module      */
    /* object now belongs to the DLIMP_loaded_objects list.                  */
    /*-----------------------------------------------------------------------*/
    loaded_module_ptr_enqueue(&pHandle->DLIMP_loaded_objects,
                              dyn_module->loaded_module);

    /*-----------------------------------------------------------------------*/
    /* Register a DSBT index request for this module and update its own copy */
    /* of the DSBT with the contents of the client's master DSBT.            */
    /*-----------------------------------------------------------------------*/
    if (is_dsbt_module(dyn_module))
    {
        dynamic_module_ptr_push(&pHandle->DLIMP_dependency_stack, dyn_module);
        DLIF_register_dsbt_index_request(handle,
                                         dyn_module->name,
                                         dyn_module->loaded_module->file_handle,
                                         dyn_module->dsbt_index);
        DLIF_assign_dsbt_indices();
        DLIF_update_all_dsbts();
        dynamic_module_ptr_pop(&pHandle->DLIMP_dependency_stack);
    }

    /*-----------------------------------------------------------------------*/
    /* Ownership of the host memory allocated for the loaded module object   */
    /* is transferred to the DLIMP_loaded_objects list. Free up the host     */
    /* memory for the dynamic module that created the loaded module object.  */
    /* Just call the destructor function for DLIMP_Dynamic_Module.           */
    /*-----------------------------------------------------------------------*/
    loaded_module = detach_loaded_module(dyn_module);
    if(loaded_module == NULL)
    {
        delete_DLIMP_Dynamic_Module(handle, &dyn_module);
        return 0;
    }
    delete_DLIMP_Dynamic_Module(handle, &dyn_module);

    /*-----------------------------------------------------------------------*/
    /* Return a file handle so that the client can match this file to an ID. */
    /*-----------------------------------------------------------------------*/
    return loaded_module->file_handle;
}

/*****************************************************************************/
/* DSBT Support Functions                                                    */
/*****************************************************************************/

/*****************************************************************************/
/* DLOAD_get_dsbt_size()                                                     */
/*                                                                           */
/*    Find the amount of space allocated for the specified module's DSBT.    */
/*    It must be big enough to hold a copy of the master DSBT or the client  */
/*    will flag an error. Those modules whose DSBT size is zero are assumed  */
/*    to not be using the DSBT model.                                        */
/*                                                                           */
/*****************************************************************************/
uint32_t DLOAD_get_dsbt_size(DLOAD_HANDLE handle, int32_t file_handle)
{
    dynamic_module_ptr_Stack_Node *ptr;
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;
    for (ptr = pHandle->DLIMP_dependency_stack.top_ptr;
         ptr != NULL;
         ptr = ptr->next_ptr)
    {
        DLIMP_Dynamic_Module *dmp = ptr->value;
        if (dmp->loaded_module->file_handle == file_handle)
            return dmp->dsbt_size;
    }

   return 0;
}

/*****************************************************************************/
/* DLOAD_get_static_base()                                                   */
/*                                                                           */
/*    Look up static base symbol associated with the specified module.       */
/*                                                                           */
/*****************************************************************************/
BOOL DLOAD_get_static_base(DLOAD_HANDLE handle, int32_t file_handle,
                           TARGET_ADDRESS *static_base)
{
    dynamic_module_ptr_Stack_Node *ptr;
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;
    for (ptr = pHandle->DLIMP_dependency_stack.top_ptr;
         ptr != NULL;
         ptr = ptr->next_ptr)
    {
        DLIMP_Dynamic_Module *dmp = ptr->value;
        if (dmp->loaded_module->file_handle == file_handle)
        {
            BOOL stat = DLSYM_lookup_local_symtab("__TI_STATIC_BASE",
                                                  dmp->symtab, dmp->symnum,
                                                  (Elf32_Addr *)static_base);
            return stat;
        }
    }

    return FALSE;
}

/*****************************************************************************/
/* DLOAD_get_dsbt_base()                                                     */
/*                                                                           */
/*    Look up address of DSBT for the specified module.                      */
/*                                                                           */
/*****************************************************************************/
BOOL DLOAD_get_dsbt_base(DLOAD_HANDLE handle, int32_t file_handle,
                         TARGET_ADDRESS *dsbt_base)
{
    dynamic_module_ptr_Stack_Node *ptr;
    LOADER_OBJECT *pHandle = (LOADER_OBJECT *)handle;
    for (ptr = pHandle->DLIMP_dependency_stack.top_ptr;
         ptr != NULL;
         ptr = ptr->next_ptr)
    {
        DLIMP_Dynamic_Module *dmp = ptr->value;
        if (dmp->loaded_module->file_handle == file_handle)
        {
            *dsbt_base =
                (TARGET_ADDRESS)dmp->dyntab[dmp->dsbt_base_tagidx].d_un.d_ptr;
            return TRUE;
        }
    }

    return FALSE;
}
