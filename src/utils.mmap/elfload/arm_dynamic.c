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
/* arm_dynamic.c                                                             */
/*                                                                           */
/* ARM specific dynamic loader functionality.                                */
/*****************************************************************************/

#ifdef ARM_TARGET

#include "arm_elf32.h"
#include "dload.h"

/*****************************************************************************/
/* process_arm_dynamic_tag()                                                 */
/*                                                                           */
/* Process ARM specific dynamic tags                                         */
/*****************************************************************************/
BOOL DLDYN_arm_process_dynamic_tag(DLIMP_Dynamic_Module* dyn_module, int i)
{
    switch (dyn_module->dyntab[i].d_tag)
    {
        case DT_ARM_SYMTABSZ:
        {
            dyn_module->symnum = dyn_module->dyntab[i].d_un.d_val;
#if LOADER_DEBUG
            if (debugging_on)
                DLIF_trace("Found symbol table size: %d\n",
                           dyn_module->symnum);
#endif
            return TRUE;
        }
    }

    return FALSE;
}

/*****************************************************************************/
/* DLDYN_arm_relocate_dynamic_tag_info()                                     */
/*                                                                           */
/* Update any target specific dynamic tag values that are associated with    */
/* a section address. Return TRUE if the tag value is successfully           */
/* updated or if the tag is not associated with a section address, and       */
/* FALSE if we can't find the sectoin associated with the tag or if the      */
/* tag type is not recognized.                                               */
/*                                                                           */
/*****************************************************************************/
BOOL DLDYN_arm_relocate_dynamic_tag_info(DLIMP_Dynamic_Module *dyn_module,
                                         int32_t i)
{
    switch (dyn_module->dyntab[i].d_tag)
    {
        /*-------------------------------------------------------------------*/
        /* These tags do not point to sections.                              */
        /*-------------------------------------------------------------------*/
        case DT_ARM_RESERVED1:
        case DT_ARM_SYMTABSZ:
        case DT_ARM_PREEMPTMAP:
        case DT_ARM_RESERVED2:
            return TRUE;
    }

    DLIF_error(DLET_MISC, "Invalid dynamic tag encountered, %d\n",
                          (int)dyn_module->dyntab[i].d_tag);
    return FALSE;
}

/*****************************************************************************/
/* arm_process_eiosabi()                                                     */
/*                                                                           */
/* Process the EI_OSABI value. Verify that the OSABI is supported and set    */
/* any variables which depend on the OSABI.                                  */
/*****************************************************************************/
BOOL DLDYN_arm_process_eiosabi(DLIMP_Dynamic_Module* dyn_module)
{
    /*-----------------------------------------------------------------------*/
    /* For ARM, the EI_OSABI value must not be set.  The only ARM specific   */
    /* value is ELFOSABI_ARM_AEABI is for objects which contain symbol       */
    /* versioning, which we do not support for Baremetal ABI.                */
    /*-----------------------------------------------------------------------*/
    if (dyn_module->fhdr.e_ident[EI_OSABI] != ELFOSABI_NONE)
        return FALSE;

    return TRUE;
}

#endif /* ARM_TARGET */
