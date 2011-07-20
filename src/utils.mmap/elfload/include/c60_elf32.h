/*
 *  c60_elf32.h
 *
 *  C6x-specific data structures for 32-bit ELF object format files.
 *
 *  Copyright (C) 2009 Texas Instruments Incorporated
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
 *     documentation and/or other materials provided with the
 *     distribution.
 *
 *  *  Neither the name of Texas Instruments Incorporated nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef C60_ELF32_H
#define C60_ELF32_H

#include "elf32.h"

/*---------------------------------------------------------------------------*/
/* C6x specific EI_OSABI values                                              */
/*---------------------------------------------------------------------------*/
enum
{
   ELFOSABI_C6000_ELFABI = 64,  /* C6X Baremetal OSABI                */
   ELFOSABI_C6000_LINUX  = 65   /* C6X Linux OSABI                    */
};

/*---------------------------------------------------------------------------*/
/* File Header Flags (value of "e_flags")                                    */
/*---------------------------------------------------------------------------*/
enum
{
   EF_C6000_REL = 0x01        /* Contains static relocations. A ET_EXEC or  */
                              /* ET_DYN file w/ this flag set can be        */
                              /* treated as ET_REL during static linking.   */
};

/*---------------------------------------------------------------------------*/
/* Segment Types (value of "p_type")                                         */
/*---------------------------------------------------------------------------*/
enum
{
   PT_C6000_PHATTRS    = 0x70000000     /* Extended Program Header Attributes*/
};

/*---------------------------------------------------------------------------*/
/* C6x specific section types                                                */
/*---------------------------------------------------------------------------*/
enum
{

   /*------------------------------------------------------------------------*/
   /* Section types defined by the C6x ELFABI.                               */
   /* Note: ABI defined section type should be named SHT_C6000_xxx           */
   /*------------------------------------------------------------------------*/
   SHT_C6000_UNWIND     = 0x70000001,   /* Exception Index Table             */
   SHT_C6000_PREEMPTMAP = 0x70000002,   /* Pre-emption Map                   */

   SHT_C6000_ATTRIBUTES = 0x70000003,   /* Obj File Compatability Attributes */

   /*------------------------------------------------------------------------*/
   /* The following section types are not part of C6x ABI. As per the ABI,   */
   /* the processor specific values not defined in the ABI are reserved for  */
   /* future use. Here we reserve the range 0x7F000000 through 0x7FFFFFFFF   */
   /* for the TI specific processor section types.                           */
   /* Note: TI specific section type should be named SHT_TI_xxx              */
   /*------------------------------------------------------------------------*/
   SHT_TI_ICODE         = 0x7F000000,   /* ICODE representation              */
   SHT_TI_XREF          = 0x7F000001,   /* Symbol cross reference            */
   SHT_TI_HANDLER       = 0x7F000002,   /* Handler function table            */
   SHT_TI_INITINFO      = 0x7F000003,   /* Info for C auto-init of variables */
   SHT_TI_PHATTRS       = 0x7F000004    /* Extended Program Header Attributes*/
};

/*****************************************************************************/
/* C6x-Specific Dynamic Array Tags (C6x ELF ABI Section ??? - AEGUPD)        */
/* NOTE:                                                                     */
/*     As per GABI a tag whose value is even number indicates a dynamic tag  */
/*     that uses d_ptr. Odd number indicates the use of d_val or doesn't use */
/*     neither d_val nor d_ptr.                                              */
/*****************************************************************************/
enum
{
   /*------------------------------------------------------------------------*/
   /* OSABI specific tags:                                                   */
   /*          From 0x6000000D thru 0x6FFFF000                               */
   /*------------------------------------------------------------------------*/
   DT_C6000_GSYM_OFFSET   = 0x6000000D,  /* d_val    -- OSABI Specific    -- */
   DT_C6000_GSTR_OFFSET   = 0x6000000F,  /* d_val    -- OSABI Specific    -- */

   /*------------------------------------------------------------------------*/
   /* Processor specific tags:                                               */
   /*          From 0x70000000 thru 0x7FFFFFFF                               */
   /*------------------------------------------------------------------------*/
   DT_C6000_DSBT_BASE     = 0x70000000,  /* d_ptr    -- Platform Specific -- */
   DT_C6000_DSBT_SIZE     = 0x70000001,  /* d_val    -- Platform Specific -- */
   DT_C6000_PREEMPTMAP    = 0x70000002,  /* d_ptr    -- Platform Specific -- */
   DT_C6000_DSBT_INDEX    = 0x70000003   /* d_val    -- Platform Specific -- */
};

/*---------------------------------------------------------------------------*/
/* C6x Dynamic Relocation Types                                              */
/*---------------------------------------------------------------------------*/
typedef enum
{
   R_C6000_NONE           = 0,
   R_C6000_ABS32          = 1,
   R_C6000_ABS16          = 2,
   R_C6000_ABS8           = 3,
   R_C6000_PCR_S21        = 4,
   R_C6000_PCR_S12        = 5,
   R_C6000_PCR_S10        = 6,
   R_C6000_PCR_S7         = 7,
   R_C6000_ABS_S16        = 8,
   R_C6000_ABS_L16        = 9,
   R_C6000_ABS_H16        = 10,
   R_C6000_SBR_U15_B      = 11,
   R_C6000_SBR_U15_H      = 12,
   R_C6000_SBR_U15_W      = 13,
   R_C6000_SBR_S16        = 14,
   R_C6000_SBR_L16_B      = 15,
   R_C6000_SBR_L16_H      = 16,
   R_C6000_SBR_L16_W      = 17,
   R_C6000_SBR_H16_B      = 18,
   R_C6000_SBR_H16_H      = 19,
   R_C6000_SBR_H16_W      = 20,
   R_C6000_SBR_GOT_U15_W  = 21,
   R_C6000_SBR_GOT_L16_W  = 22,
   R_C6000_SBR_GOT_H16_W  = 23,
   R_C6000_DSBT_INDEX     = 24,
   R_C6000_PREL31         = 25,
   R_C6000_COPY           = 26
}C60_RELOC_TYPE;

#endif /* C60_ELF32_H */
