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
/* dload4430.h                                                               */
/*                                                                           */
/* Define internal data structures used by core dynamic loader.              */
/*****************************************************************************/
#ifndef DLOAD4430_H
#define DLOAD4430_H

#include "ArrayList.h"
#include "Queue.h"
#include "Stack.h"
#include "elf32.h"
#include "dload_api.h"
#include "dlw_debug.h"
#include "dlw_trgmem.h"
#include "util.h"
#include "Std.h"

/*!
 *  @def    DLOAD_MODULEID
 *  @brief  Module ID for DLoad.
 */
#define DLOAD_MODULEID           (UInt16) 0xfade

/* =============================================================================
 *  All success and failure codes for the module
 * =============================================================================
 */

/*!
 *  @def    DLOAD_STATUSCODEBASE
 *  @brief  Error code base for DLoad.
 */
#define DLOAD_STATUSCODEBASE     (DLOAD_MODULEID << 12u)

/*!
 *  @def    DLOAD_MAKE_FAILURE
 *  @brief  Macro to make failure code.
 */
#define DLOAD_MAKE_FAILURE(x)    ((Int)(  0x80000000                          \
                                      | (DLOAD_STATUSCODEBASE + (x))))

/*!
 *  @def    DLOAD_MAKE_SUCCESS
 *  @brief  Macro to make success code.
 */
#define DLOAD_MAKE_SUCCESS(x)    (DLOAD_STATUSCODEBASE + (x))

/*!
 *  @def    DLOAD_E_INVALIDARG
 *  @brief  Argument passed to a function is invalid.
 */
#define DLOAD_E_INVALIDARG       DLOAD_MAKE_FAILURE(1)

/*!
 *  @def    DLOAD_E_MEMORY
 *  @brief  Memory allocation failed.
 */
#define DLOAD_E_MEMORY           DLOAD_MAKE_FAILURE(2)

/*!
 *  @def    DLOAD_E_FAIL
 *  @brief  Generic failure.
 */
#define DLOAD_E_FAIL             DLOAD_MAKE_FAILURE(3)

/*!
 *  @def    DLOAD_E_INVALIDSTATE
 *  @brief  Module is in invalid state.
 */
#define DLOAD_E_INVALIDSTATE     DLOAD_MAKE_FAILURE(4)

/*!
 *  @def    DLOAD_E_HANDLE
 *  @brief  Invalid object handle specified
 */
#define DLOAD_E_HANDLE           DLOAD_MAKE_FAILURE(5)

/*!
 *  @def    DLOAD_E_OSFAILURE
 *  @brief  Failure in an OS-specific operation.
 */
#define DLOAD_E_OSFAILURE        DLOAD_MAKE_FAILURE(6)

/*!
 *  @def    DLOAD_E_ACCESSDENIED
 *  @brief  The operation is not permitted in this process.
 */
#define DLOAD_E_ACCESSDENIED     DLOAD_MAKE_FAILURE(7)

/*!
 *  @def    DLOAD_E_TRANSLATE
 *  @brief  An address translation error occurred.
 */
#define DLOAD_E_TRANSLATE        DLOAD_MAKE_FAILURE(8)

/*!
 *  @def    DLOAD_SUCCESS
 *  @brief  Operation successful.
 */
#define DLOAD_SUCCESS            DLOAD_MAKE_SUCCESS(0)

/*!
 *  @def    DLOAD_S_ALREADYSETUP
 *  @brief  The DLoad module has already been setup in this process.
 */
#define DLOAD_S_ALREADYSETUP     DLOAD_MAKE_SUCCESS(1)

/*!
 *  @def    DLOAD_S_OPENHANDLE
 *  @brief  Other DLoad clients have still setup the DLoad module.
 */
#define DLOAD_S_SETUP            DLOAD_MAKE_SUCCESS(2)

/*!
 *  @def    DLOAD_S_OPENHANDLE
 *  @brief  Other DLoad handles are still open in this process.
 */
#define DLOAD_S_OPENHANDLE       DLOAD_MAKE_SUCCESS(3)

/*!
 *  @def    DLOAD_S_ALREADYEXISTS
 *  @brief  The DLoad instance has already been created/opened in this process
 */
#define DLOAD_S_ALREADYEXISTS    DLOAD_MAKE_SUCCESS(4)

/*!
 *  @brief  DLoad instance object
 */
typedef struct DLoad4430_Object_tag {
    UInt32                 openRefCount;
    /*!< Reference count for number of times open/close were called in this
         process. */
    Bool                   created;
    /*!< Indicates whether the object was created in this process. */
    UInt16                 procId;
    /*!< Processor ID */

    UInt32                 fileId;
    /*!< Baseimage fileId */

    void *                 dynLoadMem;
    UInt32                 dynLoadMemSize;
    /*!< Baseimage DynLoad Mem Section */
    TRG_PACKET            *trg_mem_head;
    BOOL                   trg_minit;

    BOOL                   DLL_debug;
    /*!< Flag to indicate if DLL_debug is supported. */
    TARGET_ADDRESS         DLModules_loc;
    /*!< Address on the target of the DLModules. */
    mirror_debug_ptr_Queue mirror_debug_list;
    dl_debug_Stack         dl_debug_stk;

    DLOAD_HANDLE     loaderHandle;
    /*!< Handle to loader-instance specific info used by dyn loader lib. */
} DLoad4430_Object;


/*!
 *  @brief  Defines DLoad object handle
 */
typedef struct DLoad4430_Object * DLoad4430_Handle;

/*!
 *  @brief  Module configuration structure.
 */
typedef struct DLoad_Config {
    UInt32 reserved;
    /*!< Reserved value. */
} DLoad_Config;


typedef enum {
    DLOAD_MPU = 0,
    DLOAD_TESLA = 1,
    DLOAD_SYSM3 = 2,
    DLOAD_APPM3 = 3,
    DLOAD_END = 4
} DLoad_ProcId;

#define DLOAD_INVALIDFILEID = 0xFFFFFFFF;

/*!
 *  @brief  Configuration parameters specific to the slave ProcMgr instance.
 */
typedef struct DLoad_Params_tag {
    UInt32 reserved;
    /*!< Reserved value. */
} DLoad_Params ;


/* =============================================================================
 *  APIs
 * =============================================================================
 */

/* Function to get the default configuration for the DLoad module. */
Void DLoad4430_getConfig (DLoad_Config * cfg);

/* Function to setup the DLoad module. */
Int DLoad4430_setup (DLoad_Config * cfg);

/* Function to destroy the DLoad module. */
Int DLoad4430_destroy (Void);

/* Function to create the DLoad instance. */
DLoad4430_Handle DLoad4430_create (UInt16 procId, const DLoad_Params * params);

/* Function to delete the DLoad instance. */
Int DLoad4430_delete (DLoad4430_Handle * handlePtr);

/* Function to load a file to the specified instance. */
Int
DLoad4430_load (DLoad4430_Handle handle,
                String           imagePath,
                UInt32           argc,
                String *         argv,
                UInt32 *         entry_point,
                UInt32 *         fileId);

/* Function to unload a file from the specified instance. */
Int
DLoad4430_unload (DLoad4430_Handle handle, UInt32 fileId);

/* Function to get the information needed to call getEntryNames. */
Int
DLoad4430_getEntryNamesInfo (DLoad4430_Handle handle,
                             UInt32           fileId,
                             Int32 *          entry_pt_cnt,
                             Int32 *          entry_pt_max_name_len);

/* Function to get the entry names for a specified instance and file. */
Int
DLoad4430_getEntryNames (DLoad4430_Handle handle,
                         UInt32           fileId,
                         Int32 *          entry_pt_cnt,
                         Char ***         entry_pt_names);

/* Function to get the symbol address of a specified symbol. */
Int
DLoad4430_getSymbolAddress (DLoad4430_Handle handle,
                            UInt32           fileId,
                            String           symbolName,
                            UInt32 *         symValue);


#endif
