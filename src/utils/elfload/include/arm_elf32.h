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
/* arm_elf32.h                                                               */
/*                                                                           */
/* ARM-Specific Data Structures for 32-bit ELF Object Files.                 */
/*                                                                           */
/* The data structures in this file come primarily from this specification:  */
/*                                                                           */
/* ELF for the ARM Architecture (AAELF)                                      */
/* Document Number: ARM IHI 0044C, current through ABI release 2.07          */
/* Date of Issue  : 10th October, 2008                                       */
/*****************************************************************************/
#ifndef ARM_ELF32_H
#define ARM_ELF32_H

#include "elf32.h"

/*---------------------------------------------------------------------------*/
/* ARM-specific e_flags                                                      */
/*---------------------------------------------------------------------------*/
enum
{
    EF_ARM_EABIMASK = 0xFF000000,
    EF_ARM_BE8      = 0x00800000
};

/*---------------------------------------------------------------------------*/
/* ARM-specific EI_OSABI values                                              */
/*---------------------------------------------------------------------------*/
enum
{
   ELFOSABI_ARM_AEABI  = 64  /* ARM AEABI with symbol versioning   */
};

/*---------------------------------------------------------------------------*/
/* ARM-specific section types                                                */
/*---------------------------------------------------------------------------*/
enum
{
    SHT_ARM_EXIDX          = 0x70000001,
    SHT_ARM_PREEMPTMAP     = 0x70000002,
    SHT_ARM_ATTRIBUTES     = 0x70000003,
    SHT_ARM_DEBUGOVERLAY   = 0x70000004,
    SHT_ARM_OVERLAYSECTION = 0x70000005
};

/*---------------------------------------------------------------------------*/
/* ARM-specific segment types                                                */
/*---------------------------------------------------------------------------*/
enum
{
    PT_ARM_ARCHEXT = 0x70000000,
    PT_ARM_EXIDX   = 0x70000001
};

/*---------------------------------------------------------------------------*/
/* Common architecture compatibility information                             */
/*---------------------------------------------------------------------------*/
enum
{
    PT_ARM_ARCHEXT_FMTMSK =0xFF000000,
    PT_ARM_ARCHEXT_PROFMASK = 0x00FF0000,
    PT_ARM_ARCHEXT_ARCHMSK  = 0x000000FF
};

/*---------------------------------------------------------------------------*/
/* Architecture compatibility formats                                        */
/*---------------------------------------------------------------------------*/
enum
{
    PT_ARM_ARCHEXT_FMT_OS = 0x0
};

/*---------------------------------------------------------------------------*/
/* Architecture profile compatibility                                        */
/*---------------------------------------------------------------------------*/
enum
{
    PT_ARM_ARCHEXT_PROF_NONE = 0x0,
    PT_ARM_ARCHEXT_PROF_ARM  = 0x00410000,
    PT_ARM_ARCHEXT_PROF_RT   = 0x00520000,
    PT_ARM_ARCHEXT_PROF_MC   = 0x004D0000
};

/*---------------------------------------------------------------------------*/
/* Architecture compatibility                                                */
/*---------------------------------------------------------------------------*/
enum
{
    PT_ARM_ARCHEXT_ARCH_UNKN = 0x0,
    PT_ARM_ARCHEXT_ARCHv4    = 0x1,
    PT_ARM_ARCHEXT_ARCHv4T   = 0x2,
    PT_ARM_ARCHEXT_ARCHv5T   = 0x3,
    PT_ARM_ARCHEXT_ARCHv5TE  = 0x4,
    PT_ARM_ARCHEXT_ARCHv5TEJ = 0x5,
    PT_ARM_ARCHEXT_ARCHv6    = 0x6,
    PT_ARM_ARCHEXT_ARCHv6KZ  = 0x7,
    PT_ARM_ARCHEXT_ARCHv6T2  = 0x8,
    PT_ARM_ARCHEXT_ARCHv6K   = 0x9,
    PT_ARM_ARCHEXT_ARCHv7    = 0xA
};


/*---------------------------------------------------------------------------*/
/* ARM-specific dynamic array tags                                           */
/*---------------------------------------------------------------------------*/
enum
{
    DT_ARM_RESERVED1  = 0x70000000,
    DT_ARM_SYMTABSZ   = 0x70000001,
    DT_ARM_PREEMPTMAP = 0x70000002,
    DT_ARM_RESERVED2  = 0x70000003
};

/*---------------------------------------------------------------------------*/
/* ARM Relocation Types                                                      */
/*---------------------------------------------------------------------------*/
typedef enum
{
  R_ARM_NONE                 =       0,
  R_ARM_PC24                 =       1,
  R_ARM_ABS32                =       2,
  R_ARM_REL32                =       3,
  R_ARM_LDR_PC_G0            =       4,
  R_ARM_ABS16                =       5,
  R_ARM_ABS12                =       6,
  R_ARM_THM_ABS5             =       7,
  R_ARM_ABS8                 =       8,
  R_ARM_SBREL32              =       9,
  R_ARM_THM_CALL             =      10,
  R_ARM_THM_PC8              =      11,
  R_ARM_BREL_ADJ             =      12,
  R_ARM_SWI24                =      13,
  R_ARM_THM_SWI8             =      14,
  R_ARM_XPC25                =      15,
  R_ARM_THM_XPC22            =      16,
  R_ARM_TLS_DTPMOD32         =      17,
  R_ARM_TLS_DTPOFF32         =      18,
  R_ARM_TLS_TPOFF32          =      19,
  R_ARM_COPY                 =      20,
  R_ARM_GLOB_DAT             =      21,
  R_ARM_JUMP_SLOT            =      22,
  R_ARM_RELATIVE             =      23,
  R_ARM_GOTOFF32             =      24,
  R_ARM_BASE_PREL            =      25,
  R_ARM_GOT_BREL             =      26,
  R_ARM_PLT32                =      27,
  R_ARM_CALL                 =      28,
  R_ARM_JUMP24               =      29,
  R_ARM_THM_JUMP24           =      30,
  R_ARM_BASE_ABS             =      31,
  R_ARM_ALU_PCREL_7_0        =      32,
  R_ARM_ALU_PCREL_15_8       =      33,
  R_ARM_ALU_PCREL_23_15      =      34,
  R_ARM_LDR_SBREL_11_0_NC    =      35,
  R_ARM_ALU_SBREL_19_12_NC   =      36,
  R_ARM_ALU_SBREL_27_20_CK   =      37,
  R_ARM_TARGET1              =      38,
  R_ARM_SBREL31              =      39,
  R_ARM_V4BX                 =      40,
  R_ARM_TARGET2              =      41,
  R_ARM_PREL31               =      42,
  R_ARM_MOVW_ABS_NC          =      43,
  R_ARM_MOVT_ABS             =      44,
  R_ARM_MOVW_PREL_NC         =      45,
  R_ARM_MOVT_PREL            =      46,
  R_ARM_THM_MOVW_ABS_NC      =      47,
  R_ARM_THM_MOVT_ABS         =      48,
  R_ARM_THM_MOVW_PREL_NC     =      49,
  R_ARM_THM_MOVT_PREL        =      50,
  R_ARM_THM_JUMP19           =      51,
  R_ARM_THM_JUMP6            =      52,
  R_ARM_THM_ALU_PREL_11_0    =      53,
  R_ARM_THM_PC12             =      54,
  R_ARM_ABS32_NOI            =      55,
  R_ARM_REL32_NOI            =      56,
  R_ARM_ALU_PC_G0_NC         =      57,
  R_ARM_ALU_PC_G0            =      58,
  R_ARM_ALU_PC_G1_NC         =      59,
  R_ARM_ALU_PC_G1            =      60,
  R_ARM_ALU_PC_G2            =      61,
  R_ARM_LDR_PC_G1            =      62,
  R_ARM_LDR_PC_G2            =      63,
  R_ARM_LDRS_PC_G0           =      64,
  R_ARM_LDRS_PC_G1           =      65,
  R_ARM_LDRS_PC_G2           =      66,
  R_ARM_LDC_PC_G0            =      67,
  R_ARM_LDC_PC_G1            =      68,
  R_ARM_LDC_PC_G2            =      69,
  R_ARM_ALU_SB_G0_NC         =      70,
  R_ARM_ALU_SB_G0            =      71,
  R_ARM_ALU_SB_G1_NC         =      72,
  R_ARM_ALU_SB_G1            =      73,
  R_ARM_ALU_SB_G2            =      74,
  R_ARM_LDR_SB_G0            =      75,
  R_ARM_LDR_SB_G1            =      76,
  R_ARM_LDR_SB_G2            =      77,
  R_ARM_LDRS_SB_G0           =      78,
  R_ARM_LDRS_SB_G1           =      79,
  R_ARM_LDRS_SB_G2           =      80,
  R_ARM_LDC_SB_G0            =      81,
  R_ARM_LDC_SB_G1            =      82,
  R_ARM_LDC_SB_G2            =      83,
  R_ARM_MOVW_BREL_NC         =      84,
  R_ARM_MOVT_BREL            =      85,
  R_ARM_MOVW_BREL            =      86,
  R_ARM_THM_MOVW_BREL_NC     =      87,
  R_ARM_THM_MOVT_BREL        =      88,
  R_ARM_THM_MOVW_BREL        =      89,
  R_ARM_PLT32_ABS            =      94,
  R_ARM_GOT_ABS              =      95,
  R_ARM_GOT_PREL             =      96,
  R_ARM_GOT_BREL12           =      97,
  R_ARM_GOTOFF12             =      98,
  R_ARM_GOTRELAX             =      99,
  R_ARM_GNU_VTENTRY          =     100,
  R_ARM_GNU_VTINHERIT        =     101,
  R_ARM_THM_JUMP11           =     102,
  R_ARM_THM_JUMP8            =     103,
  R_ARM_TLS_GD32             =     104,
  R_ARM_TLS_LDM32            =     105,
  R_ARM_TLS_LDO32            =     106,
  R_ARM_TLS_IE32             =     107,
  R_ARM_TLS_LE32             =     108,
  R_ARM_TLS_LDO12            =     109,
  R_ARM_TLS_LE12             =     110,
  R_ARM_TLS_IE12GP           =     111,
  R_ARM_PRIVATE_0            =     112,
  R_ARM_PRIVATE_1            =     113,
  R_ARM_PRIVATE_2            =     114,
  R_ARM_PRIVATE_3            =     115,
  R_ARM_PRIVATE_4            =     116,
  R_ARM_PRIVATE_5            =     117,
  R_ARM_PRIVATE_6            =     118,
  R_ARM_PRIVATE_7            =     119,
  R_ARM_PRIVATE_8            =     120,
  R_ARM_PRIVATE_9            =     121,
  R_ARM_PRIVATE_10           =     122,
  R_ARM_PRIVATE_11           =     123,
  R_ARM_PRIVATE_12           =     124,
  R_ARM_PRIVATE_13           =     125,
  R_ARM_PRIVATE_14           =     126,
  R_ARM_PRIVATE_15           =     127,
  R_ARM_ME_TOO               =     128,
  R_ARM_THM_TLS_DESCSEQ16    =     129,
  R_ARM_THM_TLS_DESCSEQ32    =     130
}ARM_RELOC_TYPE;

#endif /* ARM_ELF32_H */
