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
/******************************************************************************/
/* arm_reloc.c                                                                */
/*                                                                            */
/* Process ARM-specific dynamic relocations for core dynamic loader.          */
/******************************************************************************/

#include <limits.h>
#include <stdlib.h>
#include "relocate.h"
#include "dload_api.h"
#include "util.h"
#include "dload_endian.h"
#include "symtab.h"
#include "arm_elf32.h"

#define EXTRACT(field, lsb_offset, field_size) \
   ( (field >> lsb_offset) & ((1U << field_size) - 1) )
#define OPND_S(symval) (symval & ~0x1)
#define OPND_T(symval) (symval & 0x1)
#define IS_BLX(field) ((EXTRACT(field, 24, 8) & ~0x1) == 0xFA)
#define SIGN_EXTEND(field, field_size) (field |= -(field & (1<<(field_size-1))))

/*****************************************************************************/
/* Enumeration to represent the relocation container size.                   */
/*                                                                           */
/* ARM_RELOC      - one 32 bit relocation field                              */
/* THUMB32_RELOC  - two 16 bit relocation fields                             */
/* THUMB16_RELOC  - one 16 bit relocation field                              */
/* ARM8_RELOC     - one 8 bit relocation field                               */
/*****************************************************************************/
typedef enum
{
    ARM_RELOC,
    THUMB32_RELOC,
    THUMB16_RELOC,
    ARM8_RELOC
}ARM_RELOC_SIZE;

/*****************************************************************************/
/* OBJ_IS_BE8() - Returns TRUE if the object file contains BE8 code.         */
/*****************************************************************************/
static inline int obj_is_be8(struct Elf32_Ehdr* fhdr)
{
    return (fhdr->e_flags & EF_ARM_BE8);
}

/*****************************************************************************/
/* IS_DATA_RELOCATION() - Returns TRUE if the relocation pertains to data    */
/*****************************************************************************/
static BOOL is_data_relocation(ARM_RELOC_TYPE r_type)
{
    switch (r_type)
    {
        case R_ARM_ABS32:
        case R_ARM_ABS16:
        case R_ARM_ABS8:
        case R_ARM_PREL31:
        case R_ARM_REL32:
        case R_ARM_ABS32_NOI:
        case R_ARM_REL32_NOI:
            return TRUE;
        default:
            return FALSE;
    }
}

/*****************************************************************************/
/* REL_SWAP_ENDIAN() - Return TRUE if we should change the endianness of a   */
/*                     relocation field.  Due to BE-8 encoding, we cannot    */
/*                     simply rely on the wrong_endian member of elf_addrs.  */
/*****************************************************************************/
static BOOL rel_swap_endian(DLIMP_Dynamic_Module* dyn_module,
                            ARM_RELOC_TYPE r_type)
{
    /*-----------------------------------------------------------------------*/
    /* LE -> BE8 - swap data relocations only                                */
    /*-----------------------------------------------------------------------*/
    if (dyn_module->wrong_endian && obj_is_be8(&dyn_module->fhdr) &&
        is_data_relocation(r_type))
        return TRUE;
    /*-----------------------------------------------------------------------*/
    /* BE -> BE8 - swap instruction relocations                              */
    /*-----------------------------------------------------------------------*/
    else if (!dyn_module->wrong_endian &&
             obj_is_be8(&dyn_module->fhdr) &&
             !is_data_relocation(r_type))
        return TRUE;
    /*-----------------------------------------------------------------------*/
    /* LE -> BE32, BE8-> LE, BE32 -> LE - swap all relocations               */
    /*-----------------------------------------------------------------------*/
    else if (dyn_module->wrong_endian)
        return TRUE;

    /*-----------------------------------------------------------------------*/
    /* BE32 -> BE32, LE -> LE, BE8 -> BE32 - swap nothing                    */
    /*-----------------------------------------------------------------------*/
    return FALSE;
}

/*****************************************************************************/
/* REL_CONCLUDES_GROUP() - Returns true if this relocation type concludes a  */
/*                         group relocation sequence                         */
/*****************************************************************************/
static BOOL rel_concludes_group(ARM_RELOC_TYPE r_type)
{
    switch (r_type)
    {
        case R_ARM_ALU_PC_G0:
        case R_ARM_ALU_PC_G1:
        case R_ARM_ALU_PC_G2:
        case R_ARM_LDC_PC_G0:
        case R_ARM_LDC_PC_G1:
        case R_ARM_LDC_PC_G2:
        case R_ARM_LDR_PC_G0:
        case R_ARM_LDR_PC_G1:
        case R_ARM_LDR_PC_G2:
        case R_ARM_LDRS_PC_G0:
        case R_ARM_LDRS_PC_G1:
        case R_ARM_LDRS_PC_G2:
            return TRUE;
        default:
            return FALSE;
    }
}

/*****************************************************************************/
/* REL_GROUP_NUM() - Returns group number of this relocation type.           */
/*****************************************************************************/
static int rel_group_num(ARM_RELOC_TYPE r_type)
{
    switch (r_type)
    {
        case R_ARM_ALU_PC_G0:
        case R_ARM_ALU_PC_G0_NC:
        case R_ARM_LDC_PC_G0:
        case R_ARM_LDR_PC_G0:
        case R_ARM_LDRS_PC_G0:
            return 0;

        case R_ARM_ALU_PC_G1:
        case R_ARM_ALU_PC_G1_NC:
        case R_ARM_LDC_PC_G1:
        case R_ARM_LDR_PC_G1:
        case R_ARM_LDRS_PC_G1:
            return 1;

        case R_ARM_ALU_PC_G2:
        case R_ARM_LDC_PC_G2:
        case R_ARM_LDR_PC_G2:
        case R_ARM_LDRS_PC_G2:
            return 2;

        default:
            return 0;
    }
}

/*****************************************************************************/
/* REL_ALU_MASK_OFFSET() - Calculate the offset of an 8-bit mask for an      */
/*                         ALU immediate value.                              */
/*****************************************************************************/
static uint32_t rel_alu_mask_offset(int32_t residual, int bit_align)
{
    uint32_t mask_offset;

    for (mask_offset = 31; mask_offset > 7; mask_offset--)
        if (residual & (0x1 << mask_offset)) break;
    mask_offset -= 7;

    if (bit_align == 0) bit_align = 1;

    if ((mask_offset & bit_align) != 0)
        mask_offset += (bit_align - (mask_offset % bit_align));

    return mask_offset;
}

/*****************************************************************************/
/* REL_MASK_FOR_GROUP() - Mask off the appropriate bits from reloc_val,      */
/*                        depending on the group relocation type.            */
/*                        See Section 4.6.1.4 Group relocations in AAELF     */
/*                        for more details.                                  */
/*****************************************************************************/
static void rel_mask_for_group(ARM_RELOC_TYPE r_type, int32_t* reloc_val)
{
    int32_t curr_residual = *reloc_val;
    int num_alu_groups = rel_group_num(r_type) + 1;
    int n;
    if (rel_concludes_group(r_type)) num_alu_groups--;

    for (n = 0; n < num_alu_groups; n++)
    {
        uint32_t mask_offset = rel_alu_mask_offset(curr_residual, 2);
        uint32_t cres_mask   = 0xFF << mask_offset;

        *reloc_val = curr_residual & cres_mask;

        curr_residual = curr_residual & ~cres_mask;
    }

    if (rel_concludes_group(r_type)) *reloc_val = curr_residual;

}

/*****************************************************************************/
/* GET_RELOC_SIZE() - Return the container size for this relocation type.    */
/*****************************************************************************/
static ARM_RELOC_SIZE get_reloc_size(ARM_RELOC_TYPE r_type)
{
    switch (r_type)
    {
        case R_ARM_THM_ABS5:
        case R_ARM_THM_PC8:
        case R_ARM_THM_JUMP6:
        case R_ARM_THM_JUMP11:
        case R_ARM_THM_JUMP8:
            return THUMB16_RELOC;

        case R_ARM_THM_CALL:
        case R_ARM_THM_JUMP24:
        case R_ARM_THM_MOVW_ABS_NC:
        case R_ARM_THM_MOVT_ABS:
        case R_ARM_THM_MOVW_PREL_NC:
        case R_ARM_THM_MOVT_PREL:
        case R_ARM_THM_JUMP19:
        case R_ARM_THM_ALU_PREL_11_0:
        case R_ARM_THM_PC12:
        case R_ARM_THM_MOVW_BREL_NC:
        case R_ARM_THM_MOVT_BREL:
        case R_ARM_THM_MOVW_BREL:
            return THUMB32_RELOC;

        case R_ARM_ABS8:
            return ARM8_RELOC;

        default:
            return ARM_RELOC;
    }
}

/*****************************************************************************/
/* REL_CHANGE_ENDIAN() - Changes the endianess of the relocation field       */
/*                       located at address.  The size of the field depends  */
/*                       on the relocation type.                             */
/*****************************************************************************/
static void rel_change_endian(ARM_RELOC_TYPE r_type, uint8_t* address)
{
    ARM_RELOC_SIZE reloc_size = get_reloc_size(r_type);

    switch (reloc_size)
    {
        case ARM_RELOC:
        {
            DLIMP_change_endian32((int32_t*)address);
            break;
        }

        case THUMB32_RELOC:
        {
            int16_t* ins1_ptr = (int16_t*)address;
            DLIMP_change_endian16(ins1_ptr);
            DLIMP_change_endian16(ins1_ptr + 1);
            break;
        }

        case THUMB16_RELOC:
        {
            DLIMP_change_endian16((int16_t*)address);
            break;
        }

        default:
            break;
    }
}

/*****************************************************************************/
/* WRITE_RELOC_R() - Write a relocation value to address rel_field_ptr.      */
/*                   It is assumed that all values have been properly packed */
/*                   and masked.                                             */
/*****************************************************************************/
static void write_reloc_r(uint8_t* rel_field_ptr,
                          ARM_RELOC_TYPE r_type, int32_t reloc_val,
                          uint32_t symval)
{
#if LOADER_DEBUG
    /*-----------------------------------------------------------------------*/
    /* Print some details about the relocation we are about to process.      */
    /*-----------------------------------------------------------------------*/
    if(debugging_on)
    {
        DLIF_trace("RWRT: rel_field_ptr: 0x%x\n", (uint32_t)rel_field_ptr);
        DLIF_trace("RWRT: result: 0x%x\n", reloc_val);
    }
#endif


    /*-----------------------------------------------------------------------*/
    /* Given the relocation type, carry out relocation into a 4 byte packet  */
    /* within the buffered segment.                                          */
    /*-----------------------------------------------------------------------*/
    switch(r_type)
    {
        case R_ARM_ABS32:
        case R_ARM_REL32:
        case R_ARM_REL32_NOI:
        case R_ARM_ABS32_NOI:
        {
            *((uint32_t*)rel_field_ptr) = reloc_val;
            break;
        }

        case R_ARM_PREL31:
        {
            *((uint32_t*)rel_field_ptr) |= reloc_val;
            break;
        }

        case R_ARM_PC24:
        case R_ARM_CALL:
        case R_ARM_PLT32:
        {
            /*---------------------------------------------------------------*/
            /* ARM BL/BLX                                                    */
            /* reloc_val has already been packed down to 25 bits. If the     */
            /* callee is a thumb function, we convert to a BLX. After        */
            /* conversion, the field is packed down to 24 bits.              */
            /*---------------------------------------------------------------*/
            uint32_t rel_field = *((uint32_t*)rel_field_ptr);

            /*---------------------------------------------------------------*/
            /* Check to see if callee is a thumb function.  If so convert to */
            /* BLX                                                           */
            /*---------------------------------------------------------------*/
            if (OPND_T(symval))
                rel_field |= 0xF0000000;

            /*---------------------------------------------------------------*/
            /* Clear imm24 bits.  This must be done for both BL and BLX      */
            /*---------------------------------------------------------------*/
            rel_field &= ~0xFFFFFF;

            if (IS_BLX(rel_field))
            {
                uint8_t Hval = reloc_val & 0x1;
                /*-----------------------------------------------------------*/
                /* For BLX clear bit 24 (the H bit)                          */
                /*-----------------------------------------------------------*/
                rel_field &= ~0x01000000;
                rel_field |= Hval << 24;
            }

            /*---------------------------------------------------------------*/
            /* Pack reloc_val down to 24 bits.                               */
            /*---------------------------------------------------------------*/
            rel_field |= (reloc_val >> 1);
            *((uint32_t*)rel_field_ptr) = rel_field;
            break;
        }

        case R_ARM_JUMP24:
        {
            *((uint32_t*)rel_field_ptr) &= ~0xFFFFFF;
            *((uint32_t*)rel_field_ptr) |= reloc_val;
            break;
        }

        case R_ARM_THM_CALL:
        case R_ARM_THM_JUMP24:
        {
            /*---------------------------------------------------------------*/
            /* THUMB B.W/BL/BLX                                              */
            /*---------------------------------------------------------------*/
            uint16_t* rel_field_16_ptr = (uint16_t*) rel_field_ptr;

            uint8_t Sval;
            uint8_t J1;
            uint8_t J2;
            uint16_t imm10;
            uint16_t imm11;

            /*---------------------------------------------------------------*/
            /* If callee is a thumb function, convert BL to BLX              */
            /*---------------------------------------------------------------*/
            if (!OPND_T(symval) && r_type == R_ARM_THM_CALL)
            {
                if (reloc_val & 0x1) reloc_val++;
                *(rel_field_16_ptr + 1) &= 0xEFFF;
            }

            /*---------------------------------------------------------------*/
            /* reloc_val = S:I1:I2:imm10:imm11                               */
            /*---------------------------------------------------------------*/
            Sval  = (reloc_val >> 23)    & 0x1;
            J1    = ((reloc_val >> 22) ^ (!Sval)) & 0x1;
            J2    = ((reloc_val >> 21) ^ (!Sval)) & 0x1;
            imm10 = (reloc_val >> 11)  & 0x3FF;
            imm11 = reloc_val & 0x7FF;

            *rel_field_16_ptr &= 0xF800;
            *rel_field_16_ptr |= (Sval << 10);
            *rel_field_16_ptr |= imm10;

            rel_field_16_ptr++;
            *rel_field_16_ptr &= 0xD000;

            *rel_field_16_ptr |= (J1 << 13);
            *rel_field_16_ptr |= (J2 << 11);
            *rel_field_16_ptr |= imm11;

            break;
        }
        case R_ARM_THM_JUMP19:
        {
            /*---------------------------------------------------------------*/
            /* THUMB B<c>.W                                                  */
            /* reloc_val = S:J2:J1:imm6:imm11:'0'                            */
            /*---------------------------------------------------------------*/
            uint16_t* rel_field_16_ptr = (uint16_t*) rel_field_ptr;
            uint8_t S;
            uint8_t J2;
            uint8_t J1;
            uint8_t imm6;
            uint16_t imm11;

            S =     (reloc_val >> 19) & 0x1;
            J2 =    (reloc_val >> 18) & 0x1;
            J1 =    (reloc_val >> 17) & 0x1;
            imm6 =  (reloc_val >> 11) & 0x3F;
            imm11 = (reloc_val      ) & 0x7FF;

            /*---------------------------------------------------------------*/
            /* Clear S and imm6 fields in first part of instruction          */
            /*---------------------------------------------------------------*/
            *rel_field_16_ptr &= 0xFBC0;
            *rel_field_16_ptr |= (S << 10);
            *rel_field_16_ptr |= imm6;

            rel_field_16_ptr++;

            *rel_field_16_ptr &= 0xD800;
            *rel_field_16_ptr |= (J2 << 13);
            *rel_field_16_ptr |= (J1 << 11);
            *rel_field_16_ptr |= imm11;
            break;
        }
        case R_ARM_THM_JUMP11:
        {
            /*---------------------------------------------------------------*/
            /* THUMB B (unconditional)                                       */
            /*---------------------------------------------------------------*/
            *((uint16_t*)rel_field_ptr) &= ~0x7FF;
            *((uint16_t*)rel_field_ptr) |= reloc_val;
            break;
        }

        case R_ARM_THM_JUMP8:
        {
            /*---------------------------------------------------------------*/
            /* THUMB B<c>                                                    */
            /*---------------------------------------------------------------*/
            *((uint16_t*)rel_field_ptr) &= ~0xFF;
            *((uint16_t*)rel_field_ptr) |= reloc_val;
            break;
        }

        case R_ARM_THM_ABS5:
        {
            /*---------------------------------------------------------------*/
            /* THUMB LDR<c> <Rt>, [<Rn>(,#imm}]                              */
            /*---------------------------------------------------------------*/
            *((uint16_t*)rel_field_ptr) &= 0xF83F;
            *((uint16_t*)rel_field_ptr) |= (reloc_val << 6);
            break;
        }

        case R_ARM_THM_PC8:
        {
            /*---------------------------------------------------------------*/
            /* THUMB LDR<c> <Rt>,[SP{,#imm}]                                 */
            /*---------------------------------------------------------------*/
            *((uint16_t*)rel_field_ptr) &= 0xFF00;
            *((uint16_t*)rel_field_ptr) |= reloc_val;
            break;
        }

        case R_ARM_THM_JUMP6:
        {
            /*---------------------------------------------------------------*/
            /* THUMB CBZ,CBNZ                                                */
            /* reloc_field = Ival:imm5                                       */
            /*---------------------------------------------------------------*/
            uint8_t Ival;
            uint8_t imm5;

            Ival = (reloc_val >> 5) & 0x1;
            imm5 = (reloc_val & 0x1F);
            *((uint16_t*)rel_field_ptr) &= 0xFD07;
            *((uint16_t*)rel_field_ptr) |= (Ival << 9);
            *((uint16_t*)rel_field_ptr) |= (imm5 << 3);
            break;
        }

        case R_ARM_ABS16:
        {
            /*---------------------------------------------------------------*/
            /* 16 bit data relocation                                        */
            /*---------------------------------------------------------------*/
            *((uint16_t*)rel_field_ptr) = reloc_val;
            break;
        }

        case R_ARM_ABS8:
        {
            /*---------------------------------------------------------------*/
            /* 8 bit data relocation                                         */
            /*---------------------------------------------------------------*/
            *((uint8_t*)rel_field_ptr) = reloc_val;
            break;
        }

        case R_ARM_MOVW_ABS_NC:
        case R_ARM_MOVT_ABS:
        case R_ARM_MOVW_PREL_NC:
        case R_ARM_MOVT_PREL:
        {
            /*---------------------------------------------------------------*/
            /* MOVW/MOVT                                                     */
            /*---------------------------------------------------------------*/
           uint8_t imm4 = reloc_val >> 12;
           uint16_t imm12 = reloc_val & 0xFFF;
           *((uint32_t*)rel_field_ptr) &= 0xFFF0F000;
           *((uint32_t*)rel_field_ptr) |= imm12;
           *((uint32_t*)rel_field_ptr) |= (imm4 << 16);
           break;
       }

       case R_ARM_THM_MOVW_ABS_NC:
       case R_ARM_THM_MOVT_ABS:
       case R_ARM_THM_MOVW_PREL_NC:
       case R_ARM_THM_MOVT_PREL:
       {
           /*----------------------------------------------------------------*/
           /* THUMB2 MOVW/MOVT                                               */
           /*----------------------------------------------------------------*/
           uint16_t* rel_field_16_ptr = (uint16_t*) rel_field_ptr;

           uint8_t imm4   = (reloc_val >> 12);
           uint8_t i      = (reloc_val >> 11) & 0x1;
           uint8_t imm3   = (reloc_val >> 8 ) & 0x7;
           uint8_t imm8   = (reloc_val      ) & 0xFF;

           *rel_field_16_ptr &= 0xFBF0;
           *rel_field_16_ptr |= i << 10;
           *rel_field_16_ptr |= imm4;

           rel_field_16_ptr++;

           *rel_field_16_ptr &= 0x8F00;
           *rel_field_16_ptr |= imm3 << 12;
           *rel_field_16_ptr |= imm8;

           break;
       }

       case R_ARM_ABS12:
       {
           /*----------------------------------------------------------------*/
           /* LDR immediate                                                  */
           /*                                                                */
           /* We need to know the sign of the relocated value, so we wait to */
           /* to mask off any bits until now.                                */
           /*----------------------------------------------------------------*/
           uint8_t S = !((reloc_val >> 31) & 0x1);
           reloc_val = abs(reloc_val) & 0xFFF;

           *((uint32_t*)rel_field_ptr) &= 0xFF7FF000;
           *((uint32_t*)rel_field_ptr) |= reloc_val;
           *((uint32_t*)rel_field_ptr) |= (S << 23);
           break;
       }

       case R_ARM_THM_PC12:
       {
           /*----------------------------------------------------------------*/
           /* LDR<,B,SB,H,SH> Rd, [PC, #imm] (literal)                       */
           /*                                                                */
           /* We need to know the sign of the relocated value, so we wait to */
           /* to mask off any bits until now.                                */
           /*----------------------------------------------------------------*/
           uint16_t* rel_field_16_ptr = (uint16_t*) rel_field_ptr;
           uint8_t U = (reloc_val < 0) ? 0 : 1;
           reloc_val = abs(reloc_val) & 0xFFF;

           *rel_field_16_ptr &= 0xFF7F;
           *rel_field_16_ptr |= U << 7;

           rel_field_16_ptr++;

           *rel_field_16_ptr &= 0xF000;
           *rel_field_16_ptr |= reloc_val;

           break;
       }

       /*--------------------------------------------------------------------*/
       /* ARM GROUP RELOCATIONS.  See Section 4.6.1.4 in AAELF for more info */
       /*                                                                    */
       /* For these relocations, the reloc_val is calculated by              */
       /* rel_mask_for_group().  We cannot check for overflow until after    */
       /* this has been called, so overflow checking has been delayed until  */
       /* now.                                                               */
       /*--------------------------------------------------------------------*/

       case R_ARM_ALU_PC_G0_NC:
       case R_ARM_ALU_PC_G0:
       case R_ARM_ALU_PC_G1_NC:
       case R_ARM_ALU_PC_G2:
       {
           /*----------------------------------------------------------------*/
           /* ARM ALU (ADD/SUB)                                              */
           /*----------------------------------------------------------------*/
           uint32_t *rel_field_32_ptr = (uint32_t*) rel_field_ptr;
           uint32_t mask_offset;
           uint8_t Rval = 0;
           uint8_t Ival = 0;

           *rel_field_32_ptr &= 0xFF3FF000;

           /******************************************************************/
           /* Change the instruction to ADD or SUB, depending on the sign    */
           /******************************************************************/
           if ((int32_t)reloc_val >= 0)
               *rel_field_32_ptr |= 0x1 << 23;
           else
               *rel_field_32_ptr |= 0x1 << 22;

           reloc_val = abs(reloc_val);
           rel_mask_for_group(r_type, &reloc_val);

           mask_offset = rel_alu_mask_offset(reloc_val, 2);

           Rval = 32 - mask_offset;
           Ival = (reloc_val >> mask_offset) & 0xFF;

           if (reloc_val & ~(0xFF << mask_offset))
               DLIF_error(DLET_RELOC, "relocation overflow\n");

           *rel_field_32_ptr |= ((Rval >> 1) & 0xF) << 8;
           *rel_field_32_ptr |= (Ival & 0xFF);
       }
       break;

       case R_ARM_LDR_PC_G0:
       case R_ARM_LDR_PC_G1:
       case R_ARM_LDR_PC_G2:
       {
           /*----------------------------------------------------------------*/
           /* ARM LDR/STR/LDRB/STRB                                          */
           /*----------------------------------------------------------------*/
           uint8_t Uval = (reloc_val < 0) ? 0 : 1;
           uint32_t Lval = 0;

           reloc_val = abs(reloc_val);

           rel_mask_for_group(r_type, &reloc_val);

           if (abs(reloc_val) >= 0x1000)
               DLIF_error(DLET_RELOC, "relocation overflow!\n");

           Lval = reloc_val & 0xFFF;

           *((uint32_t*) rel_field_ptr) &= 0xFF7FF000;
           *((uint32_t*) rel_field_ptr) |= Uval << 23;
           *((uint32_t*) rel_field_ptr) |= Lval;
       }
       break;

       case R_ARM_LDRS_PC_G0:
       case R_ARM_LDRS_PC_G1:
       case R_ARM_LDRS_PC_G2:
       {
           /*----------------------------------------------------------------*/
           /* ARM LDRD/STRD/LDRH/STRH/LDRSH/LDRSB                            */
           /*----------------------------------------------------------------*/
           uint8_t Uval = (reloc_val < 0) ? 0 : 1;
           uint8_t Hval;
           uint8_t Lval;

           reloc_val = abs(reloc_val);

           rel_mask_for_group(r_type, &reloc_val);

           if (abs(reloc_val) >= 0x100)
               DLIF_error(DLET_RELOC, "relocation overflow!\n");

           Hval = (reloc_val >> 4) & 0xF;
           Lval = (reloc_val     ) & 0xF;

           *((uint32_t*) rel_field_ptr) &= 0xFF7FF0F0;
           *((uint32_t*) rel_field_ptr) |= (Uval << 23);
           *((uint32_t*) rel_field_ptr) |= (Hval << 8);
           *((uint32_t*) rel_field_ptr) |= Lval;
       }
       break;

       case R_ARM_LDC_PC_G0:
       case R_ARM_LDC_PC_G1:
       case R_ARM_LDC_PC_G2:
       {
           /*----------------------------------------------------------------*/
           /* ARM LDC/STC                                                    */
           /*----------------------------------------------------------------*/
           uint8_t Uval = (reloc_val < 0) ? 0 : 1;
           uint8_t Lval;

           reloc_val = abs(reloc_val);

           rel_mask_for_group(r_type, &reloc_val);

           if (abs(reloc_val) >= 0x1000)
               DLIF_error(DLET_RELOC, "relocation overflow!\n");

           Lval = reloc_val & 0xFF;

           *((uint32_t*) rel_field_ptr) &= 0xFF7FFF00;
           *((uint32_t*) rel_field_ptr) |= (Uval << 23);
           *((uint32_t*) rel_field_ptr) |= Lval;
       }
       break;

       /*--------------------------------------------------------------------*/
       /* THUMB2 ALU RELOCATION.  This is a big block of code, most of which */
       /* was taken from the linker.  The problem is that there are a lot of */
       /* special formats.                                                   */
       /*--------------------------------------------------------------------*/
       case R_ARM_THM_ALU_PREL_11_0:
       {
           /*----------------------------------------------------------------*/
           /* THUMB2 ADR.W                                                   */
           /*                                                                */
           /* Encode the immediate value as specified in section 4.2 of the  */
           /* Thumb2 supplement:  "Immediate constants in data-processing    */
           /* instructions"                                                  */
           /*----------------------------------------------------------------*/
           uint16_t* rel_field_16_ptr = (uint16_t*) rel_field_ptr;
           uint16_t imm12;
           uint8_t  bits_7_0, bits_15_8, bits_23_16, bits_31_24, Ival,
                    imm3, imm8;

           /*----------------------------------------------------------------*/
           /* Convert to ADD/SUB depending on the sign.                      */
           /* Clear out the 'i' bit while clearing the ADD/SUB bits          */
           /* If SUB, set bits 7 and 5 to 1, else should be 0                */
           /*----------------------------------------------------------------*/
           *rel_field_16_ptr &= 0xFB5F;
           if (reloc_val < 0)
               *rel_field_16_ptr |= 0x00A0;

           reloc_val = abs(reloc_val);

           /*----------------------------------------------------------------*/
           /* Extract out bits after we take the absolute value.             */
           /*----------------------------------------------------------------*/
           bits_7_0   = (reloc_val)       & 0xFF;
           bits_15_8  = (reloc_val >> 8)  & 0xFF;
           bits_23_16 = (reloc_val >> 16) & 0xFF;
           bits_31_24 = (reloc_val >> 24) & 0xFF;

           /*----------------------------------------------------------------*/
           /* Does the value match the pattern 0x00XY00XY?                   */
           /*----------------------------------------------------------------*/
           if (bits_23_16 == bits_7_0 && bits_31_24 == 0 && bits_15_8 == 0)
               imm12 = (0x1 << 8) | bits_7_0;

           /*----------------------------------------------------------------*/
           /* Does the value match the pattern 0xXY00XY00?                   */
           /*----------------------------------------------------------------*/
           else if (bits_31_24 == bits_15_8 && bits_23_16 == 0 && bits_7_0 == 0)
               imm12 = (0x2 << 8) | bits_7_0;

           /*-----------------------------------------------------------------*/
           /* Does the value match the pattern 0xXYXYXYXY?                    */
           /*-----------------------------------------------------------------*/
           else if (bits_31_24 == bits_23_16 && bits_31_24 == bits_15_8 &&
                    bits_31_24 == bits_7_0)
               imm12 = (0x3 << 8) | bits_7_0;

           /*-----------------------------------------------------------------*/
           /* Finally, check to see if we can encode this immediate as an     */
           /* 8-bit shifted value.                                            */
           /*-----------------------------------------------------------------*/
           else if (reloc_val >= 0 && reloc_val <= 255)
               imm12 = reloc_val;
           else
           {
               /*-------------------------------------------------------------*/
               /* Calculate the mask for this immediate, then find the        */
               /* appropriate rotate and 8-bit immediate value                */
               /* (Rval and Sval).                                            */
               /*-------------------------------------------------------------*/
               uint32_t mask_offset = rel_alu_mask_offset(
                   (uint32_t)reloc_val, 1);
               uint8_t Rval         = 32 - mask_offset;
               uint8_t Sval         = (reloc_val >> mask_offset) & 0x7F;

               imm12 = (Rval << 7) | Sval;

               /*-------------------------------------------------------------*/
               /* If overflow occurred, then finding an 8-bit shiftable value */
               /* failed.                                                     */
               /*-------------------------------------------------------------*/
               if (reloc_val & ~(0xFF << mask_offset))
                   DLIF_error(DLET_RELOC, "relocation overflow!\n");
           }

           Ival = (imm12 >> 11) & 0x1;
           imm3 = (imm12 >> 8)  & 0x7;
           imm8 = (imm12)       & 0xFF;

           *rel_field_16_ptr |= Ival << 10;
           rel_field_16_ptr++;
           *rel_field_16_ptr |= imm3 << 12;
           *rel_field_16_ptr |= imm8;
           break;
       }

       default:
           DLIF_error(DLET_RELOC,
                      "write_reloc_r called with invalid relocation type\n");
   }

}

/*****************************************************************************/
/* PACK_RESULT() - Pack the result of a relocation calculation for storage   */
/*      in the relocation field.                                             */
/*****************************************************************************/
static int32_t pack_result(int32_t unpacked_result, int r_type)
{
    switch (r_type)
    {
        case R_ARM_ABS32:
        case R_ARM_REL32:
        case R_ARM_ABS16:
        case R_ARM_ABS8:
        case R_ARM_PREL31:
        case R_ARM_MOVW_ABS_NC:
        case R_ARM_MOVW_PREL_NC:
        case R_ARM_THM_MOVW_ABS_NC:
        case R_ARM_THM_MOVW_PREL_NC:
        case R_ARM_ABS32_NOI:
        case R_ARM_REL32_NOI:
        case R_ARM_ABS12:
        case R_ARM_ALU_PC_G0_NC:
        case R_ARM_ALU_PC_G0:
        case R_ARM_ALU_PC_G1_NC:
        case R_ARM_ALU_PC_G1:
        case R_ARM_ALU_PC_G2:
        case R_ARM_LDR_PC_G0:
        case R_ARM_LDR_PC_G1:
        case R_ARM_LDR_PC_G2:
        case R_ARM_LDRS_PC_G0:
        case R_ARM_LDRS_PC_G1:
        case R_ARM_LDRS_PC_G2:
        case R_ARM_LDC_PC_G0:
        case R_ARM_LDC_PC_G1:
        case R_ARM_LDC_PC_G2:
        case R_ARM_THM_PC12:
        case R_ARM_THM_ALU_PREL_11_0:
            return unpacked_result;

        case R_ARM_PC24:
        case R_ARM_CALL:
        case R_ARM_PLT32:
        case R_ARM_THM_CALL:
        case R_ARM_THM_JUMP24:
        case R_ARM_THM_JUMP11:
        case R_ARM_THM_JUMP8:
        case R_ARM_THM_JUMP6:
        case R_ARM_THM_JUMP19:
            return unpacked_result >> 1;

        case R_ARM_JUMP24:
        case R_ARM_THM_ABS5:
        case R_ARM_THM_PC8:
            return unpacked_result >> 2;

        case R_ARM_MOVT_ABS:
        case R_ARM_MOVT_PREL:
        case R_ARM_THM_MOVT_ABS:
        case R_ARM_THM_MOVT_PREL:
            return unpacked_result >> 16;

        default:
            DLIF_error(DLET_RELOC,
                       "pack_result called with invalid relocation type!\n");
            return 0;
    }

}

/*****************************************************************************/
/* MASK_RESULT() - Mask the result of a relocation calculation so that it    */
/*      fits the size of the relocation type's field.                        */
/*****************************************************************************/
static int32_t mask_result(int32_t unmasked_result, int r_type)
{
    switch (r_type)
    {
        case R_ARM_ABS8:
        case R_ARM_THM_JUMP8:
        case R_ARM_THM_PC8:
            return unmasked_result & 0xFF;

        case R_ARM_ABS16:
        case R_ARM_MOVW_ABS_NC:
        case R_ARM_MOVT_ABS:
        case R_ARM_MOVW_PREL_NC:
        case R_ARM_MOVT_PREL:
        case R_ARM_THM_MOVW_ABS_NC:
        case R_ARM_THM_MOVT_ABS:
        case R_ARM_THM_MOVW_PREL_NC:
        case R_ARM_THM_MOVT_PREL:
            return unmasked_result & 0xFFFF;

        case R_ARM_ABS32:
        case R_ARM_REL32:
        case R_ARM_ABS32_NOI:
        case R_ARM_REL32_NOI:
        case R_ARM_ABS12:
        case R_ARM_ALU_PC_G0_NC:
        case R_ARM_ALU_PC_G0:
        case R_ARM_ALU_PC_G1_NC:
        case R_ARM_ALU_PC_G1:
        case R_ARM_ALU_PC_G2:
        case R_ARM_LDR_PC_G0:
        case R_ARM_LDR_PC_G1:
        case R_ARM_LDR_PC_G2:
        case R_ARM_LDRS_PC_G0:
        case R_ARM_LDRS_PC_G1:
        case R_ARM_LDRS_PC_G2:
        case R_ARM_LDC_PC_G0:
        case R_ARM_LDC_PC_G1:
        case R_ARM_LDC_PC_G2:
        case R_ARM_THM_PC12:
        case R_ARM_THM_ALU_PREL_11_0:
            return unmasked_result;

        case R_ARM_PREL31:
            return unmasked_result & 0x7FFFFFFF;

        case R_ARM_CALL:
        case R_ARM_PC24:
        case R_ARM_THM_CALL:
        case R_ARM_THM_JUMP24:
            return unmasked_result & 0x01FFFFFF;

        case R_ARM_JUMP24:
            return unmasked_result & 0x00FFFFFF;

        case R_ARM_THM_JUMP11:
            return unmasked_result & 0x7FF;

        case R_ARM_THM_ABS5:
            return unmasked_result & 0x1F;

        case R_ARM_THM_JUMP6:
            return unmasked_result & 0x3F;

        case R_ARM_THM_JUMP19:
            return unmasked_result & 0xFFFFF;

        default:
            DLIF_error(DLET_RELOC,
                       "mask_result invalid relocation type!\n");
            return 0;
    }

}


/*****************************************************************************/
/* OVERFLOW() - Check relocation value against the range associated with a   */
/*      given relocation type field size and signedness.                     */
/*****************************************************************************/
static BOOL rel_overflow(ARM_RELOC_TYPE r_type, int32_t reloc_value)
{
    switch(r_type)
    {
        uint32_t sbits;
        case R_ARM_JUMP24:
        case R_ARM_CALL:
        case R_ARM_PC24:
        {
            sbits = reloc_value >> 25;
            return (sbits != 0 && sbits != -1UL);
        }
        case R_ARM_THM_CALL:
        case R_ARM_THM_JUMP24:
        {
            sbits = reloc_value >> 24;
            return (sbits != 0 && sbits != -1UL);
        }
        case R_ARM_THM_JUMP19:
        {
            sbits = reloc_value >> 20;
            return (sbits != 0 && sbits != -1UL);
        }
        case R_ARM_ABS12:
        case R_ARM_THM_PC12:
        {
            return abs(reloc_value) >= 0x1000;
        }
        case R_ARM_PREL31:
        {
            sbits = reloc_value >> 30;
            return (sbits != 0 && sbits != -1UL);
        }

        case R_ARM_ABS16:
        {
            return ((reloc_value > 65535) ||
                        (reloc_value < -32768));
        }
        case R_ARM_ABS8:
        {
            return ((reloc_value > 255) ||
                         (reloc_value < -128));
        }

        default:
            return FALSE;
    }
}

/*****************************************************************************/
/* RELOC_DO() - Process a single relocation entry.                           */
/*****************************************************************************/
static void reloc_do(ARM_RELOC_TYPE r_type, uint8_t* address,
              uint32_t addend, uint32_t symval, uint32_t pc,
              uint32_t base_pointer)
{
    int32_t reloc_value = 0;

#if LOADER_DEBUG || LOADER_PROFILE
    /*-----------------------------------------------------------------------*/
    /* In debug mode, keep a count of the number of relocations processed.   */
    /* In profile mode, start the clock on a given relocation.               */
    /*-----------------------------------------------------------------------*/
    int start_time;
    if (debugging_on || profiling_on)
    {
        DLREL_relocations++;
        if (profiling_on) start_time = clock();
    }
#endif

    /*-----------------------------------------------------------------------*/
    /* Calculate the relocation value according to the rules associated with */
    /* the given relocation type.                                            */
    /*-----------------------------------------------------------------------*/
    switch(r_type)
    {
        case R_ARM_NONE:
            reloc_value = addend;
            break;

        /*-------------------------------------------------------------------*/
        /* S + A                                                             */
        /*-------------------------------------------------------------------*/
        case R_ARM_ABS16:
        case R_ARM_ABS12:
        case R_ARM_THM_ABS5:
        case R_ARM_ABS8:
        case R_ARM_MOVT_ABS:
        case R_ARM_THM_MOVT_ABS:
        case R_ARM_ABS32_NOI:
        case R_ARM_PLT32_ABS:
            reloc_value = OPND_S(symval) + addend;
            break;

        /*-------------------------------------------------------------------*/
        /* (S + A) | T                                                       */
        /*-------------------------------------------------------------------*/
        case R_ARM_ABS32:
        case R_ARM_MOVW_ABS_NC:
        case R_ARM_THM_MOVW_ABS_NC:
            reloc_value = (OPND_S(symval) + addend) | OPND_T(symval);
            break;

        /*-------------------------------------------------------------------*/
        /* (S + A) - P                                                       */
        /*-------------------------------------------------------------------*/
        case R_ARM_LDR_PC_G0:
        case R_ARM_LDR_PC_G1:
        case R_ARM_LDR_PC_G2:
        case R_ARM_MOVT_PREL:
        case R_ARM_THM_MOVT_PREL:
        case R_ARM_REL32_NOI:
        case R_ARM_THM_JUMP6:
        case R_ARM_THM_JUMP11:
        case R_ARM_THM_JUMP8:
        case R_ARM_LDRS_PC_G0:
        case R_ARM_LDRS_PC_G1:
        case R_ARM_LDRS_PC_G2:
        case R_ARM_LDC_PC_G0:
        case R_ARM_LDC_PC_G1:
        case R_ARM_LDC_PC_G2:
            reloc_value = (OPND_S(symval) + addend) - pc;
            break;

        /*-------------------------------------------------------------------*/
        /* (S + A) - Pa                                                      */
        /*-------------------------------------------------------------------*/
        case R_ARM_THM_PC8:
        case R_ARM_THM_PC12:
            reloc_value = (OPND_S(symval) + addend) - (pc & 0xFFFFFFFC);
            break;

        /*-------------------------------------------------------------------*/
        /* ((S + A) | T) - P                                                 */
        /*-------------------------------------------------------------------*/
        case R_ARM_REL32:
        case R_ARM_PC24:
        case R_ARM_THM_CALL:
        case R_ARM_PLT32:
        case R_ARM_CALL:
        case R_ARM_JUMP24:
        case R_ARM_THM_JUMP24:
        case R_ARM_PREL31:
        case R_ARM_MOVW_PREL_NC:
        case R_ARM_THM_MOVW_PREL_NC:
        case R_ARM_THM_JUMP19:
        case R_ARM_THM_ALU_PREL_11_0:
        case R_ARM_ALU_PC_G0_NC:
        case R_ARM_ALU_PC_G0:
        case R_ARM_ALU_PC_G1_NC:
        case R_ARM_ALU_PC_G1:
        case R_ARM_ALU_PC_G2:
            reloc_value = ((OPND_S(symval) + addend) | OPND_T(symval)) - pc;
            break;

       /*--------------------------------------------------------------------*/
       /* Unrecognized relocation type.                                      */
       /*--------------------------------------------------------------------*/
       default:
           DLIF_error(DLET_RELOC,"invalid relocation type!\n");
           break;

    }

    if (rel_overflow(r_type, reloc_value))
        DLIF_error(DLET_RELOC, "relocation overflow!\n");

    /*-----------------------------------------------------------------------*/
    /* Move relocation value to appropriate offset for relocation field's    */
    /* location.                                                             */
    /*-----------------------------------------------------------------------*/
    reloc_value = pack_result(reloc_value, r_type);

    /*-----------------------------------------------------------------------*/
    /* Mask packed result to the size of the relocation field.               */
    /*-----------------------------------------------------------------------*/
    reloc_value = mask_result(reloc_value, r_type);

    /*-----------------------------------------------------------------------*/
    /* Write the relocated 4-byte packet back to the segment buffer.         */
    /*-----------------------------------------------------------------------*/
    write_reloc_r(address, r_type, reloc_value, symval);

#if LOADER_DEBUG || LOADER_PROFILE
    /*-----------------------------------------------------------------------*/
    /* In profile mode, add elapsed time for this relocation to total time   */
    /* spent doing relocations.                                              */
    /*-----------------------------------------------------------------------*/
    if (profiling_on)
        DLREL_total_reloc_time += (clock() - start_time);
    if (debugging_on)
        DLIF_trace("reloc_value = 0x%x\n", reloc_value);
#endif
}

/*****************************************************************************/
/* REL_UNPACK_ADDEND() - Unpacks the addend from the relocation field        */
/*****************************************************************************/
static void rel_unpack_addend(ARM_RELOC_TYPE r_type,
                              uint8_t* address,
                              uint32_t* addend)
{
    switch (r_type)
    {
        case R_ARM_ABS32:
        case R_ARM_REL32:
        case R_ARM_ABS32_NOI:
        case R_ARM_REL32_NOI:
        {
            DLIF_trace("rel_unpack_addend %d\n", __LINE__);
            *addend = *((uint32_t*)address);
            DLIF_trace("rel_unpack_addend %d\n", __LINE__);
        }
        break;

        case R_ARM_ABS16:
        {
            *addend = *((uint16_t*)address);
            *addend = SIGN_EXTEND(*addend, 16);
        }
        break;

        case R_ARM_ABS8:
        {
            *addend = *address;
            *addend = SIGN_EXTEND(*addend, 8);
        }
        break;

        case R_ARM_CALL:
        case R_ARM_PC24:
        case R_ARM_PLT32:
        case R_ARM_JUMP24:
        {
            uint32_t rel_field = *((uint32_t*)address);
            uint32_t imm24;
            uint8_t Hval;
            if (IS_BLX(rel_field))
                Hval = EXTRACT(rel_field, 24, 1);
            else
                Hval = 0x0;

            imm24 = EXTRACT(rel_field, 0, 24);
            *addend = (((imm24 << 1) + Hval) << 1);
            SIGN_EXTEND(*addend, 26);
        }
        break;

        case R_ARM_THM_JUMP11:
        {
            *addend = EXTRACT(*((uint16_t*)address), 0, 11);
            *addend = *addend << 1;
            SIGN_EXTEND(*addend, 12);
            break;
        }

        case R_ARM_THM_JUMP8:
        {
            *addend = EXTRACT(*((uint16_t*)address), 0, 8);
            *addend = *addend << 1;
            SIGN_EXTEND(*addend, 9);
            break;
        }

        case R_ARM_THM_CALL:
        case R_ARM_THM_JUMP24:
        {
            uint16_t* rel_field_ptr = (uint16_t *) address;

            uint8_t Sval;
            uint16_t imm10;
            uint8_t I1;
            uint8_t I2;
            uint16_t imm11;

            Sval = EXTRACT(*rel_field_ptr, 10, 1);
            imm10 = EXTRACT(*rel_field_ptr, 0, 10);
            rel_field_ptr++;
            I1 = !(EXTRACT(*rel_field_ptr, 13, 1) ^ Sval);
            I2 = !(EXTRACT(*rel_field_ptr, 11, 1) ^ Sval);
            imm11 = EXTRACT(*rel_field_ptr, 0, 11);

            *addend = (Sval << 23) | (I1 << 22) | (I2 << 21) |
                (imm10 << 11) | imm11;
            *addend = *addend << 1;
            SIGN_EXTEND(*addend, 25);
            break;
        }

        case R_ARM_THM_JUMP19:
        {
            uint16_t* rel_field_ptr = (uint16_t *) address;

            uint8_t Sval;
            uint8_t imm6;
            uint8_t J1;
            uint8_t J2;
            uint16_t imm11;

            Sval = EXTRACT(*rel_field_ptr, 10, 1);
            imm6 = EXTRACT(*rel_field_ptr, 0, 6);
            rel_field_ptr++;
            J1 = EXTRACT(*rel_field_ptr, 13, 1);
            J2 = EXTRACT(*rel_field_ptr, 11, 1);
            imm11 = EXTRACT(*rel_field_ptr, 0, 11);

            *addend = (Sval << 19) | (J2 << 18) | (J1 << 17) |
                (imm6 << 11) | imm11;
            *addend = *addend << 1;
            SIGN_EXTEND(*addend, 21);
        }
        break;

        case R_ARM_LDR_PC_G0:
        case R_ARM_LDR_PC_G1:
        case R_ARM_LDR_PC_G2:
        case R_ARM_ABS12:
        {
            uint8_t Uval;
            uint16_t imm12;

            uint32_t* rel_field_ptr = (uint32_t*) address;

            Uval  = EXTRACT(*rel_field_ptr, 23, 1);
            imm12 = EXTRACT(*rel_field_ptr, 0, 12);

            *addend = (Uval == 1) ? imm12 : -((int32_t)(imm12));
        }
        break;

        case R_ARM_THM_PC12:
        {
            uint8_t Uval;
            uint16_t imm12;
            uint16_t* rel_field_ptr = (uint16_t*) address;

            Uval = EXTRACT(*rel_field_ptr, 7, 1);
            rel_field_ptr++;
            imm12 = EXTRACT(*rel_field_ptr, 0, 12);

            *addend = (Uval == 1) ? imm12 : -((int32_t)imm12);
            break;
        }

        case R_ARM_LDRS_PC_G0:
        case R_ARM_LDRS_PC_G1:
        case R_ARM_LDRS_PC_G2:
        {
            uint8_t Uval;
            uint8_t imm4H;
            uint8_t imm4L;
            uint32_t* rel_field_ptr = (uint32_t*) address;

            Uval = EXTRACT(*rel_field_ptr, 23, 1);
            imm4H = EXTRACT(*rel_field_ptr, 8, 4);
            imm4L = EXTRACT(*rel_field_ptr, 0, 4);

            *addend = (imm4H << 4) | imm4L;
            if (Uval == 0) *addend = -(*addend);
        }
        break;

        case R_ARM_LDC_PC_G0:
        case R_ARM_LDC_PC_G1:
        case R_ARM_LDC_PC_G2:
        {
            uint8_t Uval;
            uint16_t imm8;
            uint32_t* rel_field_ptr = (uint32_t*) address;

            Uval = EXTRACT(*rel_field_ptr, 23, 1);
            imm8 = EXTRACT(*rel_field_ptr, 0, 8);

            *addend = (Uval == 1) ? imm8 : -((int32_t)imm8);
        }
        break;

        case R_ARM_ALU_PC_G0_NC:
        case R_ARM_ALU_PC_G0:
        case R_ARM_ALU_PC_G1_NC:
        case R_ARM_ALU_PC_G1:
        case R_ARM_ALU_PC_G2:
        {
            uint8_t Rval;
            uint8_t Ival;
            uint32_t rel_field = *((uint32_t*)address);

            Rval = EXTRACT(rel_field, 8, 4);
            Ival = EXTRACT(rel_field, 0, 8);

            *addend = (Ival >> Rval) | (Ival << (32 - Rval));

            if (EXTRACT(rel_field, 22, 2) == 1)
                *addend = -(*addend);
        }
        break;

        case R_ARM_MOVW_ABS_NC:
        case R_ARM_MOVT_ABS:
        case R_ARM_MOVW_PREL_NC:
        case R_ARM_MOVT_PREL:
        {
            uint8_t Ival;
            uint16_t Jval;
            uint32_t rel_field = *((uint32_t*)address);
            Ival = EXTRACT(rel_field, 16, 4);
            Jval = EXTRACT(rel_field, 0, 12);

            *addend = (Ival << 12) | Jval;
            SIGN_EXTEND(*addend, 16);
        }
        break;

        case R_ARM_THM_MOVW_ABS_NC:
        case R_ARM_THM_MOVT_ABS:
        case R_ARM_THM_MOVW_PREL_NC:
        case R_ARM_THM_MOVT_PREL:
        {
            uint8_t Ival;
            uint8_t Jval;
            uint8_t Kval;
            uint8_t Lval;

            uint16_t* rel_field_ptr = (uint16_t*)address;
            uint16_t rel_field = *rel_field_ptr;

            Ival = EXTRACT(rel_field, 0, 4);
            Jval = EXTRACT(rel_field, 10, 1);

            rel_field = *(rel_field_ptr + 1);
            Kval = EXTRACT(rel_field, 12, 3);
            Lval = EXTRACT(rel_field, 0, 8);

            *addend = (Ival << 12) | (Jval << 11) | (Kval << 8) | Lval;
            SIGN_EXTEND(*addend, 16);
        }
        break;

        case R_ARM_THM_ALU_PREL_11_0:
        {
            uint8_t Ival;
            uint8_t Jval;
            uint8_t Kval;
            uint16_t imm12;
            uint16_t* rel_field_ptr = (uint16_t*)address;
            uint16_t rel_field = *rel_field_ptr;
            uint8_t bits_11_10;

            Ival = EXTRACT(rel_field, 12, 3);

            rel_field = *(rel_field_ptr + 1);
            Jval = EXTRACT(rel_field, 12, 3);
            Kval = EXTRACT(rel_field, 0 , 8);

            imm12 = (Ival << 11) | (Jval << 8) | Kval;

            /*---------------------------------------------------------------*/
            /* Now that we've unpacked the immediate value, we need to       */
            /* decode it according to section 4.2 in the Thumb2 supplement:  */
            /* "Immediate constants in data-processing instructions"         */
            /*---------------------------------------------------------------*/
            bits_11_10 = (imm12 >> 10) & 0x3;

            if (bits_11_10 == 0x0)
            {
                uint8_t bits_9_8   = (imm12 >> 8)  & 0x3;
                uint8_t bits_7_0   = (imm12)       & 0xFF;

                switch (bits_9_8)
                {
                    case 0x0:  *addend = bits_7_0;
                    break;
                    case 0x1:  *addend = (bits_7_0 << 16) | (bits_7_0);
                    break;
                    case 0x2:  *addend = (bits_7_0 << 24) | (bits_7_0 << 8);
                    break;
                    case 0x3:  *addend= (bits_7_0 << 24) | (bits_7_0 << 16) |
                                   (bits_7_0 << 8)  | (bits_7_0);
                    break;
                }
            }
            else
            {
                uint8_t bits_6_0  = (imm12)      & 0x7F;
                uint8_t bits_11_7 = (imm12 >> 7) & 0x1F;

                uint8_t byte = 0x80 | bits_6_0;
                *addend = (byte >> bits_11_7) | (byte << (32 - bits_11_7));
            }

            rel_field = *rel_field_ptr;

            if (EXTRACT(rel_field, 7, 1) == 1 &&
                EXTRACT(rel_field, 5, 1) == 1)
                *addend = -(*addend);
        }
        break;

        case R_ARM_THM_JUMP6:
        {
            uint8_t Ival;
            uint8_t Jval;

            uint16_t rel_field = *((uint16_t*)address);

            Ival = EXTRACT(rel_field, 9, 1);
            Jval = EXTRACT(rel_field, 3, 5);

            *addend = ((Ival << 5) | Jval) << 1;
            *addend = ((*addend + 4) & 0x7F) - 4;

        }
        break;

        case R_ARM_THM_ABS5:
        {
            uint16_t rel_field = *((uint16_t*)address);
            *addend = EXTRACT(rel_field, 6, 5);
            *addend = *addend << 2;
            break;
        }

        case R_ARM_THM_PC8:
        {
            uint16_t rel_field = *((uint16_t*)address);
            *addend = EXTRACT(rel_field, 0, 8);
            *addend = *addend << 2;
            *addend = ((*addend + 4) & 0x3FF) - 4;

            break;
        }

        case R_ARM_PREL31:
        {
            uint32_t rel_field = *((uint32_t*)address);
            *addend = EXTRACT(rel_field, 0, 31);
            SIGN_EXTEND(*addend, 31);
            break;
        }

        default:
        DLIF_error(DLET_RELOC,
                   "ERROR: Cannot unpack addend for relocation type %d\n",
                   r_type);
    }
}

/*****************************************************************************/
/* PROCESS_REL_TABLE() - Process REL type relocation table.                  */
/*****************************************************************************/
static BOOL process_rel_table(DLOAD_HANDLE handle,
                              DLIMP_Loaded_Segment* seg,
                              struct Elf32_Rel* rel_table,
                              uint32_t relnum,
                              int32_t *start_rid,
                              DLIMP_Dynamic_Module* dyn_module)
{
    Elf32_Addr seg_start_addr = seg->input_vaddr;
    Elf32_Addr seg_end_addr   = seg_start_addr + seg->phdr.p_memsz;
    BOOL found = FALSE;
    int32_t rid = *start_rid;

    if (rid >= relnum) rid = 0;

    for ( ; rid < relnum; rid++)
    {
        /*---------------------------------------------------------------*/
        /* If the relocation offset falls within the segment, process it */
        /*---------------------------------------------------------------*/
        if (rel_table[rid].r_offset >= seg_start_addr &&
            rel_table[rid].r_offset < seg_end_addr)
        {
            Elf32_Addr r_symval;
            ARM_RELOC_TYPE r_type = ELF32_R_TYPE(rel_table[rid].r_info);
            int32_t r_symid = ELF32_R_SYM(rel_table[rid].r_info);
            uint8_t* reloc_address;
            uint32_t pc;
            uint32_t addend;
            BOOL change_endian;

            found = TRUE;

            /*---------------------------------------------------------*/
            /* If symbol definition is not found don't do the          */
            /* relocation. An error is generated by the lookup         */
            /* function.                                               */
            /*---------------------------------------------------------*/
            if (!DLSYM_canonical_lookup(handle, r_symid, dyn_module, &r_symval))
                continue;

            reloc_address =
                (((uint8_t*)(seg->phdr.p_vaddr) + seg->reloc_offset) +
                 rel_table[rid].r_offset - seg->input_vaddr);
            pc = (uint32_t) reloc_address;
            change_endian = rel_swap_endian(dyn_module, r_type);
            if (change_endian)
                rel_change_endian(r_type, reloc_address);

            rel_unpack_addend(ELF32_R_TYPE(rel_table[rid].r_info),
                              reloc_address,
                              &addend);

#if LOADER_DEBUG || LOADER_PROFILE
            if (debugging_on)
            {
                char *r_symname = (char*) dyn_module->symtab[r_symid].st_name;
                DLIF_trace("r_type=%d, "
                           "pc=0x%x, "
                           "addend=0x%x, "
                           "symnm=%s, "
                           "symval=0x%x\n",
                           r_type,
                           pc,
                           addend,
                           r_symname,
                           r_symval);
            }
#endif
            /*----------------------------------------------------------*/
            /* Perform actual relocation.  This is a really wide        */
            /* function interface and could do with some encapsulation. */
            /*----------------------------------------------------------*/
            reloc_do(r_type,
                     reloc_address,
                     addend,
                     r_symval,
                     pc,
                     0 /* static base, not yet supported */);


            if (change_endian)
                rel_change_endian(r_type, reloc_address);

        }
        else if (found)
            break;
    }

    *start_rid = rid;
    return found;
}

static BOOL process_rela_table(DLOAD_HANDLE handle,
                               DLIMP_Loaded_Segment* seg,
                               struct Elf32_Rela* rela_table,
                               uint32_t relanum,
                               int32_t* start_rid,
                               DLIMP_Dynamic_Module* dyn_module)
{
    Elf32_Addr seg_start_addr = seg->input_vaddr;
    Elf32_Addr seg_end_addr   = seg_start_addr + seg->phdr.p_memsz;
    BOOL found = FALSE;
    int32_t rid = *start_rid;

    if (rid >= relanum) rid = 0;

    for ( ; rid < relanum; rid++)
    {
        /*---------------------------------------------------------------*/
        /* If the relocation offset falls within the segment, process it */
        /*---------------------------------------------------------------*/
        if (rela_table[rid].r_offset >= seg_start_addr &&
            rela_table[rid].r_offset < seg_end_addr)
        {
            Elf32_Addr r_symval;
            ARM_RELOC_TYPE r_type = ELF32_R_TYPE(rela_table[rid].r_info);
            int32_t r_symid = ELF32_R_SYM(rela_table[rid].r_info);
            uint8_t* reloc_address;
            uint32_t pc;
            uint32_t addend;
            BOOL change_endian;

            found = TRUE;

            /*---------------------------------------------------------*/
            /* If symbol definition is not found don't do the          */
            /* relocation. An error is generated by the lookup         */
            /* function.                                               */
            /*---------------------------------------------------------*/
            if (!DLSYM_canonical_lookup(handle, r_symid, dyn_module, &r_symval))
                continue;

            reloc_address = (((uint8_t*)(seg->phdr.p_vaddr) + seg->reloc_offset) +
                             rela_table[rid].r_offset - seg->input_vaddr);
            pc = (uint32_t) reloc_address;
            addend = rela_table[rid].r_addend;

            change_endian = rel_swap_endian(dyn_module, r_type);
            if (change_endian)
                rel_change_endian(r_type, reloc_address);

#if LOADER_DEBUG || LOADER_PROFILE
            if (debugging_on)
            {
                char *r_symname = (char*) dyn_module->symtab[r_symid].st_name;
                DLIF_trace("r_type=%d, "
                           "pc=0x%x, "
                           "addend=0x%x, "
                           "symnm=%s, "
                           "symval=0x%x\n",
                           r_type,
                           pc,
                           addend,
                           r_symname,
                           r_symval);
            }
#endif

            /*----------------------------------------------------------*/
            /* Perform actual relocation.  This is a really wide        */
            /* function interface and could do with some encapsulation. */
            /*----------------------------------------------------------*/

            reloc_do(ELF32_R_TYPE(rela_table[rid].r_info),
                     reloc_address,
                     addend,
                     r_symval,
                     pc,
                     0);


            if (change_endian)
                rel_change_endian(r_type, reloc_address);
        }
        else if (found)
            break;
    }

    *start_rid = rid;
    return found;
}

/*****************************************************************************/
/* READ_REL_TABLE() -                                                        */
/*                                                                           */
/*   Read in a REL type relocation table.  This function allocates host      */
/*   memory for the table.                                                   */
/*****************************************************************************/
static void read_rel_table(struct Elf32_Rel** rel_table,
                           int32_t table_offset,
                           uint32_t relnum, uint32_t relent,
                           LOADER_FILE_DESC* elf_file,
                           BOOL wrong_endian)
{
    int i;
    *rel_table = (struct Elf32_Rel*) DLIF_malloc(relnum*relent);
    if (NULL == *rel_table) {
        DLIF_error(DLET_MEMORY,"Failed to Allocate read_rel_table\n");
        return;
    }
    DLIF_fseek(elf_file, table_offset, LOADER_SEEK_SET);
    DLIF_fread(*rel_table, relnum, relent, elf_file);

    if (wrong_endian)
        for (i=0; i<relnum; i++)
            DLIMP_change_rel_endian(*rel_table + i);
}

/*****************************************************************************/
/* READ_RELA_TABLE() -                                                       */
/*                                                                           */
/*   Read in a RELA type relocation table.  This function allocates host     */
/*   memory for the table.                                                   */
/*****************************************************************************/
static void read_rela_table(struct Elf32_Rela** rela_table,
                            int32_t table_offset,
                            uint32_t relanum, uint32_t relaent,
                            LOADER_FILE_DESC* elf_file,
                            BOOL wrong_endian)
{
    int i;
    *rela_table = DLIF_malloc(relanum*relaent);
    if (NULL == *rela_table) {
        DLIF_error(DLET_MEMORY,"Failed to Allocate read_rela_table\n");
        return;
    }
    DLIF_fseek(elf_file, table_offset, LOADER_SEEK_SET);
    DLIF_fread(*rela_table, relanum, relaent, elf_file);

    if (wrong_endian)
        for (i=0; i<relanum; i++)
            DLIMP_change_rela_endian(*rela_table + i);
}

/*****************************************************************************/
/* PROCESS_GOT_RELOCS() -                                                    */
/*                                                                           */
/*   Process all GOT relocations. It is possible to have both REL and RELA   */
/*   relocations in the same file, so we handle them both.                   */
/*****************************************************************************/
static void process_got_relocs(DLOAD_HANDLE handle,
                               struct Elf32_Rel* rel_table, uint32_t relnum,
                               struct Elf32_Rela* rela_table, uint32_t relanum,
                               DLIMP_Dynamic_Module* dyn_module)
{
    DLIMP_Loaded_Segment* seg =
      (DLIMP_Loaded_Segment*)(dyn_module->loaded_module->loaded_segments.buf);
    int seg_size = dyn_module->loaded_module->loaded_segments.size;
    int32_t rel_rid = 0;
    int32_t rela_rid = 0;
    int s;

    for (s=0; s<seg_size; s++)
    {
        /*-------------------------------------------------------------------*/
        /* Relocations into the BSS should not occur.                        */
        /*-------------------------------------------------------------------*/
        if(!seg[s].phdr.p_filesz) continue;

#if LOADER_DEBUG || LOADER_PROFILE
        /*-------------------------------------------------------------------*/
        /* More details about the progress on this relocation entry.         */
        /*-------------------------------------------------------------------*/
        if (debugging_on)
        {
            DLIF_trace("Reloc segment %d:\n", s);
            DLIF_trace("addr=0x%x, old_addr=0x%x, r_offset=0x%x\n",
                seg[s].phdr.p_vaddr, seg[s].input_vaddr, seg[s].reloc_offset);
        }
#endif

        if (rela_table)
            process_rela_table(handle, (seg + s), rela_table, relanum,
                               &rela_rid, dyn_module);

        if (rel_table)
            process_rel_table(handle, (seg + s), rel_table, relnum, &rel_rid,
                              dyn_module);
    }
}

/*****************************************************************************/
/* PROCESS_PLTGOT_RELOCS                                                     */
/*                                                                           */
/*  Proceses all PLTGOT relocation entries.  The PLTGOT relocation table can */
/*  be either REL or RELA type. All PLTGOT relocations are guaranteed to     */
/*  belong to the same segment.                                              */
/*****************************************************************************/
static void process_pltgot_relocs(DLOAD_HANDLE handle,
                                  void* plt_reloc_table, int reltype,
                                  uint32_t pltnum,
                                  DLIMP_Dynamic_Module* dyn_module)
{
    Elf32_Addr r_offset = (reltype == DT_REL) ?
        ((struct Elf32_Rel*) plt_reloc_table)->r_offset :
        ((struct Elf32_Rela*) plt_reloc_table)->r_offset;
    DLIMP_Loaded_Segment* seg =
       (DLIMP_Loaded_Segment*)(dyn_module->loaded_module->loaded_segments.buf);
    int seg_size = dyn_module->loaded_module->loaded_segments.size;
    int32_t plt_rid = 0;
    int s;

    /*-----------------------------------------------------------------------*/
    /* For each segment s, check if the relocation falls within s. If so,    */
    /* then all other relocations are guaranteed to fall with s. Process     */
    /* all relocations and then return.                                      */
    /*-----------------------------------------------------------------------*/

    for (s=0; s<seg_size; s++)
    {
        Elf32_Addr seg_start_addr = seg[s].input_vaddr;
        Elf32_Addr seg_end_addr   = seg_start_addr + seg[s].phdr.p_memsz;

        /*-------------------------------------------------------------------*/
        /* Relocations into the BSS should not occur.                        */
        /*-------------------------------------------------------------------*/
        if(!seg[s].phdr.p_filesz) continue;

        if (r_offset >= seg_start_addr &&
            r_offset < seg_end_addr)
        {
            if (reltype == DT_REL)
                process_rel_table(handle, (seg + s),
                                  (struct Elf32_Rel*) plt_reloc_table,
                                  pltnum, &plt_rid,
                                  dyn_module);
            else
                process_rela_table(handle, (seg + s),
                                   (struct Elf32_Rela*) plt_reloc_table,
                                   pltnum, &plt_rid,
                                   dyn_module);

            break;
        }
    }
}

/*****************************************************************************/
/* RELOCATE() - Perform RELA and REL type relocations for given ELF object   */
/*      file that we are in the process of loading and relocating.           */
/*****************************************************************************/
void DLREL_relocate(DLOAD_HANDLE handle,
                    LOADER_FILE_DESC* elf_file,
                    DLIMP_Dynamic_Module* dyn_module)

{
    struct Elf32_Dyn* dyn_nugget = dyn_module->dyntab;
    struct Elf32_Rela* rela_table = NULL;
    struct Elf32_Rel*  rel_table = NULL;
    void*              plt_table = NULL;

    /*-----------------------------------------------------------------------*/
    /* Read the size of the relocation table (DT_RELASZ) and the size per    */
    /* relocation (DT_RELAENT) from the dynamic segment.                     */
    /*-----------------------------------------------------------------------*/
    uint32_t relasz = DLIMP_get_first_dyntag(DT_RELASZ, dyn_nugget);
    uint32_t relaent = DLIMP_get_first_dyntag(DT_RELAENT, dyn_nugget);
    uint32_t relanum = 0;

    /*-----------------------------------------------------------------------*/
    /* Read the size of the relocation table (DT_RELSZ) and the size per     */
    /* relocation (DT_RELENT) from the dynamic segment.                      */
    /*-----------------------------------------------------------------------*/
    uint32_t relsz = DLIMP_get_first_dyntag(DT_RELSZ, dyn_nugget);
    uint32_t relent = DLIMP_get_first_dyntag(DT_RELENT, dyn_nugget);
    uint32_t relnum = 0;

    /*-----------------------------------------------------------------------*/
    /* Read the size of the relocation table (DT_PLTRELSZ) and the type of   */
    /* of the PLTGOT relocation table (DT_PLTREL): one of DT_REL or DT_RELA  */
    /*-----------------------------------------------------------------------*/
    uint32_t pltrelsz = DLIMP_get_first_dyntag(DT_PLTRELSZ, dyn_nugget);
    int      pltreltype = DLIMP_get_first_dyntag(DT_PLTREL, dyn_nugget);
    uint32_t pltnum = 0;

    /*-----------------------------------------------------------------------*/
    /* Read the PLTGOT relocation table from the file                        */
    /* The PLTGOT table is a subsection at the end of either the DT_REL or   */
    /* DT_RELA table.  The size of the table it belongs to DT_REL(A)SZ       */
    /* includes the size of the PLTGOT table.  So it must be adjusted so that*/
    /* the GOT relocation tables only contain actual GOT relocations.        */
    /*-----------------------------------------------------------------------*/
    if (pltrelsz != INT_MAX)
    {
        if (pltreltype == DT_REL)
        {
            pltnum = pltrelsz/relent;
            relsz -= pltrelsz;
            read_rel_table(((struct Elf32_Rel**) &plt_table),
                           DLIMP_get_first_dyntag(DT_JMPREL, dyn_nugget),
                           pltnum, relent, elf_file,
                           dyn_module->wrong_endian);
        }
        else if (pltreltype == DT_RELA)
        {
            pltnum = pltrelsz/relaent;
            relasz -= pltrelsz;
            read_rela_table(((struct Elf32_Rela**) &plt_table),
                            DLIMP_get_first_dyntag(DT_JMPREL, dyn_nugget),
                            pltnum, relaent, elf_file,
                            dyn_module->wrong_endian);
        }
        else
        {
            DLIF_error(DLET_RELOC,
                       "DT_PLTREL is invalid: must be either %d or %d\n",
                       DT_REL, DT_RELA);
        }
    }

    /*-----------------------------------------------------------------------*/
    /* Read the DT_RELA GOT relocation table from the file                   */
    /*-----------------------------------------------------------------------*/
    if (relasz != INT_MAX)
    {
        relanum = relasz/relaent;
        read_rela_table(&rela_table,
                        DLIMP_get_first_dyntag(DT_RELA, dyn_nugget),
                        relanum, relaent, elf_file, dyn_module->wrong_endian);
    }

    /*-----------------------------------------------------------------------*/
    /* Read the DT_REL GOT relocation table from the file                    */
    /*-----------------------------------------------------------------------*/
    if (relsz != INT_MAX)
    {
        relnum = relsz/relent;
        read_rel_table(&rel_table, DLIMP_get_first_dyntag(DT_REL, dyn_nugget),
                       relnum, relent, elf_file, dyn_module->wrong_endian);
    }

   /*------------------------------------------------------------------------*/
   /* Process the PLTGOT relocations                                         */
   /*------------------------------------------------------------------------*/
   if (plt_table)
      process_pltgot_relocs(handle, plt_table, pltreltype, pltnum, dyn_module);

    /*-----------------------------------------------------------------------*/
    /* Process the GOT relocations                                           */
    /*-----------------------------------------------------------------------*/
    if (rel_table || rela_table)
        process_got_relocs(handle, rel_table, relnum, rela_table, relanum,
                           dyn_module);

    /*------------------------------------------------------------------------*/
    /* Free memory used for ELF relocation table copies.                      */
    /*------------------------------------------------------------------------*/
    if (rela_table) DLIF_free(rela_table);
    if (rel_table)  DLIF_free(rel_table);
    if (plt_table)  DLIF_free(plt_table);
}

/*****************************************************************************/
/* UNIT TESTING INTERFACE.                                                   */
/*****************************************************************************/
#if UNIT_TEST
void unit_arm_reloc_do(ARM_RELOC_TYPE r_type,
                       uint8_t* address_space,
                       uint32_t addend, uint32_t symval, uint32_t pc,
                       uint32_t static_base, int wrong_endian)
{
    reloc_do(r_type, address_space, addend, symval, pc, static_base);
}

void unit_arm_rel_unpack_addend(ARM_RELOC_TYPE r_type,
                                uint8_t* address,
                                uint32_t* addend)
{
    rel_unpack_addend(r_type, address, addend);
}

BOOL unit_arm_rel_overflow(ARM_RELOC_TYPE r_type, int32_t reloc_value)
{
    return rel_overflow(r_type, reloc_value);
}

void unit_arm_rel_mask_for_group(ARM_RELOC_TYPE r_type, int32_t* reloc_val)
{
    rel_mask_for_group(r_type, reloc_val);
}
#endif
