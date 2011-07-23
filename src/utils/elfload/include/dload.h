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
/* dload.h                                                                   */
/*                                                                           */
/* Define internal data structures used by core dynamic loader.              */
/*****************************************************************************/
#ifndef DLOAD_H
#define DLOAD_H

#include "ArrayList.h"
#include "Queue.h"
#include "Stack.h"
#include "elf32.h"
#include "dload_api.h"
#include "util.h"
#include "Std.h"


/*---------------------------------------------------------------------------*/
/* DLIMP_Loaded_Segment                                                      */
/*                                                                           */
/*    This structure represents a segment loaded on memory.                  */
/*                                                                           */
/*    This data structure should be created using host memory when a module  */
/*    is being loaded into target memory.  The data structure should persist */
/*    as long as the module stays resident in target memory.  It should be   */
/*    removed when the last use of the module is unloaded from the target.   */
/*---------------------------------------------------------------------------*/
typedef struct
{
   struct Elf32_Phdr            phdr;
   Elf32_Addr                   input_vaddr;  /* original segment load addr  */
   BOOL                         modified;
   int32_t                      reloc_offset; /* used by load_program        */
   struct DLOAD_MEMORY_SEGMENT *obj_desc;
} DLIMP_Loaded_Segment;

/*---------------------------------------------------------------------------*/
/* DLIMP_Loaded_Module                                                       */
/*                                                                           */
/*    This structure contains all the information the dynamic loader needs   */
/*    to retain after loading an object file's segments into target memory.  */
/*    The data structure is created while the object file is being loaded,   */
/*    and should persist until the last use of the module is unloaded from   */
/*    target memory.                                                         */
/*                                                                           */
/*    The information contained here is used by the dynamic loader to        */
/*    perform dynamic symbol resolution, to track the use count, and to      */
/*    finally deallocate the module's segments when the module is unloaded.  */
/*---------------------------------------------------------------------------*/
typedef struct
{
   char                *name;            /* Local copy of so_name           */
   int32_t              file_handle;
   int32_t              use_count;
   Elf32_Addr           entry_point;     /* Entry point address into module */
   struct Elf32_Sym    *gsymtab;         /* Module's global symbol table    */
   Elf32_Word           gsymnum;         /* # global symbols                */
   char                *gstrtab;         /* Module's global symbol names    */
   Elf32_Word           gstrsz;          /* Size of global string table     */
   Array_List           loaded_segments; /* List of DLIMP_Loaded_Segment(s) */
   Array_List           dependencies;    /* List of dependent file handles  */
   BOOL                 direct_dependent_only;

   Elf32_Addr           fini;            /* .fini function/section address  */
   Elf32_Addr           fini_array;      /* .fini_array term fcn ary addr   */
   int32_t              fini_arraysz;    /* sizeof .fini_array              */

} DLIMP_Loaded_Module;

/*---------------------------------------------------------------------------*/
/* DLIMP_loaded_objects                                                      */
/*                                                                           */
/*    A list of loaded module objects (DLIMP_Loaded_Module *) that the       */
/*    loader has placed into target memory.                                  */
/*---------------------------------------------------------------------------*/
TYPE_QUEUE_DEFINITION(DLIMP_Loaded_Module*, loaded_module_ptr)

/*---------------------------------------------------------------------------*/
/* DLIMP_Dynamic_Module                                                      */
/*                                                                           */
/*   This structure represents a dynamic module to be loaded by the dynamic  */
/*   loader. It contains all the information necessary to load and relocate  */
/*   the module. It actually contains most of the headers, dynamic info,     */
/*   dynamic symbol table, string table etc.                                 */
/*                                                                           */
/*   This structure is allocated in host memory while an ELF object file is  */
/*   being loaded and will be destructed after the file has been             */
/*   successfully loaded.  To simplify loading and relocation of the object  */
/*   file's segments, this data structure maintains a link to the loaded     */
/*   module.  This link is severed when the load is successfully completed.  */
/*   The loaded module data structure will persist until the module is       */
/*   actually unloaded from target memory, but this data structure will be   */
/*   freed.                                                                  */
/*                                                                           */
/*   If the load of the object file is not successful for any reason, then   */
/*   the loaded module will not be detached from the dynamic module.  In     */
/*   such case, the destructor for the dynamic module will assume            */
/*   responsibility for freeing any host memory associated with the loaded   */
/*   module and its segments.                                                */
/*---------------------------------------------------------------------------*/
typedef struct
{
   char                *name;          /* Local copy of so_name              */
   LOADER_FILE_DESC    *fd;            /* Access to ELF object file          */
   struct Elf32_Ehdr    fhdr;          /* ELF Object File Header             */
   struct Elf32_Phdr   *phdr;          /* ELF Program Header Table           */
   Elf32_Word           phnum;         /* # entries in program header table  */
   char*                strtab;        /* String Table                       */
   Elf32_Word           strsz;         /* String Table size in bytes         */
   struct Elf32_Dyn    *dyntab;        /* Elf Dynamic Table (.dynamic scn)   */
                                       /* This contains a list of dynamic    */
                                       /* tags which is terminated by a NULL */
                                       /* record.                            */
   struct Elf32_Sym    *symtab;        /* Elf Dynamic Symbol Table           */
   Elf32_Word           symnum;        /* # symbols in dynamic symbol table  */
   Elf32_Word           gsymtab_offset;/* Offset into symbol table where     */
                                       /* global symbols start.              */
   Elf32_Word           gstrtab_offset;/* Offset into string table where     */
                                       /* global symbol names start.         */

   uint8_t             *c_args;
   int32_t              argc;
   char               **argv;
   DLIMP_Loaded_Module *loaded_module;
   int32_t              wrong_endian;
   BOOL                 direct_dependent_only;
   BOOL                 relocatable;   /* TRUE if module can be relocated    */
                                       /* at load-time.  FALSE if module is  */
                                       /* a static executable.               */
   BOOL                 relocate_entry_point; /* TRUE if the entry point has */
                                              /* not been relocated          */

   int32_t              dsbt_index;      /* DSBT index requested/assigned    */
   uint32_t             dsbt_size;       /* DSBT size for this module        */
   int32_t              dsbt_base_tagidx;/* Location of DSBT base dyn tag    */

   int32_t              preinit_array_idx; /* DT_PREINIT_ARRAY dyn tag loc   */
   int32_t              preinit_arraysz;   /* sizeof pre-init array          */
   int32_t              init_idx;          /* DT_INIT dynamic tag location   */
   int32_t              init_array_idx;    /* DT_INIT_ARRAY dyn tag location */
   int32_t              init_arraysz;      /* sizeof init array              */

} DLIMP_Dynamic_Module;

/*---------------------------------------------------------------------------*/
/* DLIMP_dependency_stack                                                    */
/*                                                                           */
/*    A LIFO stack of dynamic module objects (DLIMP_Dynamic_Module *) that   */
/*    is retained while dependent files are being loaded and allocated.  It  */
/*    is used to guide which dynamic modules need to be relocated after all  */
/*    items in the dependency graph have been allocated. The stack is only   */
/*    used when the client asks the core loader to load a dynamic executable */
/*    or library. When relocation is completed, this stack should be empty.  */
/*---------------------------------------------------------------------------*/
TYPE_STACK_DEFINITION(DLIMP_Dynamic_Module*, dynamic_module_ptr)

/*---------------------------------------------------------------------------*/
/* Private Loader Object instance.                                           */
/*---------------------------------------------------------------------------*/
typedef struct
{
    /*-----------------------------------------------------------------------*/
    /* Contains filenames (type const char*) the system is in the process of */
    /* loading.  Used to detect cycles in incorrectly compiled ELF binaries. */
    /*-----------------------------------------------------------------------*/
    Array_List               DLIMP_module_dependency_list;

    /*-----------------------------------------------------------------------*/
    /* Contains objects (type DLIMP_Loaded_Module) that the system has loaded*/
    /* into target memory.                                                   */
    /*-----------------------------------------------------------------------*/
    loaded_module_ptr_Queue  DLIMP_loaded_objects;

    /*-----------------------------------------------------------------------*/
    /* Dependency Graph Queue - FIFO queue of dynamic modules that are loaded*/
    /* when client asks to load a dynamic executable or library. Note that   */
    /* dependents that have already been loaded with another module will not */
    /* appear on this queue.                                                 */
    /*-----------------------------------------------------------------------*/
    dynamic_module_ptr_Stack DLIMP_dependency_stack;

    /*-----------------------------------------------------------------------*/
    /* Counter for generating unique IDs for file handles.                   */
    /*   NOTE: File handle is assigned sequencially but is never reclaimed   */
    /*         when the modules are unloaded. It is conceivable that a loader*/
    /*         running for a long time and loading and unloading modules     */
    /*         could wrap-around. The loader generates error in this case.   */
    /*-----------------------------------------------------------------------*/
    int32_t                  file_handle;

    /*-----------------------------------------------------------------------*/
    /* Identify target supported by this implementation of the core loader.  */
    /*-----------------------------------------------------------------------*/
    int                      DLOAD_TARGET_MACHINE;

    /*-----------------------------------------------------------------------*/
    /* Client token, passed in via DLOAD_create()                            */
    /*-----------------------------------------------------------------------*/
    void *                   client_handle;
} LOADER_OBJECT;


/*****************************************************************************/
/* is_DSBT_module()                                                          */
/*                                                                           */
/*   return true if the module uses DSBT model                               */
/*****************************************************************************/
static inline BOOL is_dsbt_module(DLIMP_Dynamic_Module *dyn_module)
{
    return (dyn_module->dsbt_size != 0);
}

/*****************************************************************************/
/* is_arm_module()                                                           */
/*                                                                           */
/*    return true if the module being processed is for ARM                   */
/*****************************************************************************/
static inline BOOL is_arm_module(struct Elf32_Ehdr* fhdr)
{
    return fhdr->e_machine == EM_ARM;
}

/*****************************************************************************/
/* is_c60_module()                                                           */
/*                                                                           */
/*   return true if the module being processed is for C60                    */
/*****************************************************************************/
static inline BOOL is_c60_module(struct Elf32_Ehdr* fhdr)
{
    return fhdr->e_machine == EM_TI_C6000;
}

/*---------------------------------------------------------------------------*/
/* Identifies the current supported target.  This is reset by the default    */
/* value when all loaded objects have been unloaded.                         */
/*---------------------------------------------------------------------------*/
extern int DLOAD_TARGET_MACHINE;

uint32_t DLIMP_get_first_dyntag(int tag, struct Elf32_Dyn* dyn_table);
/*---------------------------------------------------------------------------*/
/* Identify target which is supported by the core loader.  If set to EM_NONE */
/* the target will be determined by the first loaded module.                 */
/*---------------------------------------------------------------------------*/
#if defined(ARM_TARGET) && defined(C60_TARGET)
#define DLOAD_DEFAULT_TARGET_MACHINE  (EM_NONE)
#elif defined(ARM_TARGET)
#define DLOAD_DEFAULT_TARGET_MACHINE  (EM_ARM)
#elif defined(C60_TARGET)
#define DLOAD_DEFAULT_TARGET_MACHINE  (EM_TI_C6000)
#else
#error "ARM_TARGET and/or C60_TARGET must be defined"
#endif

/* =============================================================================
 *  APIs
 * =============================================================================
 */

/*---------------------------------------------------------------------------*/
/* DLIMP_update_dyntag_section_address()                                     */
/*                                                                           */
/*    Given the index of a dynamic tag which we happen to know points to a   */
/*    section address, find the program header table entry associated with   */
/*    the specified address and update the tag value with the real address   */
/*    of the section.                                                        */
/*                                                                           */
/*---------------------------------------------------------------------------*/
extern BOOL DLIMP_update_dyntag_section_address(
                                               DLIMP_Dynamic_Module *dyn_module,
                                               int32_t i);

/*---------------------------------------------------------------------------*/
/* Global flags to help manage internal debug and profiling efforts.         */
/*---------------------------------------------------------------------------*/
#ifndef __TI_COMPILER_VERSION__
#define LOADER_DEBUG 1
#else
#define LOADER_DEBUG 0
#endif

#undef LOADER_DEBUG
#define LOADER_DEBUG 0
#define LOADER_PROFILE 0

#if LOADER_DEBUG
extern Bool debugging_on;
#endif

#if LOADER_DEBUG || LOADER_PROFILE
extern Bool  profiling_on;
#endif

#endif
