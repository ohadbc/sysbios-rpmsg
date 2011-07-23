/*
 *  c60_reloc.c
 *
 *  Process C6x-specific dynamic relocations for core dynamic loader.
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

#if defined (__KERNEL__)
#include <linux/limits.h>
#else
#include <limits.h>
#endif
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "relocate.h"
#include "c60_elf32.h"
#include "dload_api.h"
#include "util.h"
#include "dload_endian.h"
#include "symtab.h"

#define MASK(n,s) (((1 << n) - 1) << s)

/*---------------------------------------------------------------------------*/
/* C6x Relocations Supported                                                 */
/*                                                                           */
/* See the C6000 ELF ABI Specification for more details.                     */
/*                                                                           */
/* R_C6000_ABS32          |   .field    X,32                                 */
/* R_C6000_ABS16          |   .field    X,16                                 */
/* R_C6000_ABS8           |   .field    X,8                                  */
/* R_C6000_PCR_S21        |   B         foo                                  */
/*                            CALLP     foo, B3                              */
/* R_C6000_PCR_S12        |   BNOP      foo                                  */
/* R_C6000_PCR_S10        |   BPOS      foo, A10                             */
/*                            BDEC      foo, A1                              */
/* R_C6000_PCR_S7         |   ADDKPC    foo, B3, 4                           */
/* R_C6000_ABS_S16        |   MVK       sym, A0                              */
/* R_C6000_ABS_L16        |   MVKL      sym, A0                              */
/*                            MVKLH     sym, A0                              */
/* R_C6000_ABS_H16        |   MVKH      sym, A0                              */
/* R_C6000_SBR_U15_B      |   LDB   *+B14(sym), A1                           */
/*                            ADDAB   B14, sym, A1                           */
/* R_C6000_SBR_U15_H      |   LDH   *+B14(sym), A1                           */
/*                            ADDAH   B14, sym, A1                           */
/* R_C6000_SBR_U15_W      |   LDW   *+B14(sym), A1                           */
/*                            ADDAW   B14, sym, A1                           */
/* R_C6000_SBR_S16        |   MVK     sym-$bss, A0                           */
/* R_C6000_SBR_L16_B      |   MVKL (sym-$bss),  A0                           */
/* R_C6000_SBR_L16_H      |   MVKL (sym-$bss)/2,A0                           */
/* R_C6000_SBR_L16_W      |   MVKL (sym-$bss)/4,A0                           */
/* R_C6000_SBR_H16_B      |   MVKH (sym-$bss),  A0                           */
/* R_C6000_SBR_H16_H      |   MVKH (sym-$bss)/2,A0                           */
/* R_C6000_SBR_H16_W      |   MVKH (sym-$bss)/4,A0                           */
/* R_C6000_SBR_GOT_U15_W  |   LDW *+B14[GOT(sym)],A0                         */
/* R_C6000_SBR_GOT_L16_W  |   MVKL $DPR_GOT(sym), A0                         */
/* R_C6000_SBR_GOT_H16_W  |   MVKH $DPR_GOT(sym), A0                         */
/* R_C6000_DSBT_INDEX     |   LDW *+B14[$DSBT_index()], DP                   */
/*                                                                           */
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* WRITE_RELOC_R() - Perform a relocation into a buffered segment.           */
/*****************************************************************************/
static void write_reloc_r(uint8_t* buffered_segment,
                          uint32_t segment_offset,
                          int r_type, uint32_t r)
{
   uint32_t* rel_field_ptr = (uint32_t*)(buffered_segment + segment_offset);

#if LOADER_DEBUG
   /*------------------------------------------------------------------------*/
   /* Print some details about the relocation we are about to process.       */
   /*------------------------------------------------------------------------*/
   if(debugging_on)
   {
          DLIF_trace("RWRT: segment_offset: %d\n", segment_offset);
          DLIF_trace("RWRT: buffered_segment: 0x%x\n", buffered_segment);
          DLIF_trace("RWRT: rel_field_ptr: 0x%x\n", rel_field_ptr);
          DLIF_trace("RWRT: result: 0x%x\n", r);
   }
#endif


   /*------------------------------------------------------------------------*/
   /* Given the relocation type, carry out relocation into a 4 byte packet   */
   /* within the buffered segment.                                           */
   /*------------------------------------------------------------------------*/
   switch(r_type)
   {
      case R_C6000_ABS32:
          *rel_field_ptr = r;
          break;
      case R_C6000_PREL31:
          *rel_field_ptr = (*rel_field_ptr & ~MASK(30,0)) | r;
          break;
      case R_C6000_ABS16:
          *((uint16_t*)(buffered_segment + segment_offset)) = r;
          break;
      case R_C6000_ABS8:
          *((uint8_t*)(buffered_segment + segment_offset)) = r;
          break;
     case R_C6000_PCR_S21:
          *rel_field_ptr = (*rel_field_ptr & ~MASK(21,7)) | (r << 7);
          break;
      case R_C6000_PCR_S12:
          *rel_field_ptr = (*rel_field_ptr & ~MASK(12,16)) | (r << 16);
          break;
      case R_C6000_PCR_S10:
          *rel_field_ptr = (*rel_field_ptr & ~MASK(10,13)) | (r << 13);
          break;
      case R_C6000_PCR_S7:
          *rel_field_ptr = (*rel_field_ptr & ~MASK(7,16)) | (r << 16);
          break;

      case R_C6000_ABS_S16:
          *rel_field_ptr = (*rel_field_ptr & ~MASK(16,7)) | (r << 7);
          break;
      case R_C6000_ABS_L16:
          *rel_field_ptr = (*rel_field_ptr & ~MASK(16,7)) | (r << 7);
          break;
      case R_C6000_ABS_H16:
          *rel_field_ptr = (*rel_field_ptr & ~MASK(16,7)) | (r << 7);
          break;

      case R_C6000_SBR_U15_B:
          *rel_field_ptr = (*rel_field_ptr & ~MASK(15,8)) | (r << 8);
          break;
      case R_C6000_SBR_U15_H:
          *rel_field_ptr = (*rel_field_ptr & ~MASK(15,8)) | (r << 8);
          break;
      case R_C6000_SBR_U15_W:
      case R_C6000_DSBT_INDEX:
          *rel_field_ptr = (*rel_field_ptr & ~MASK(15,8)) | (r << 8);
          break;

      case R_C6000_SBR_S16:
      case R_C6000_SBR_L16_B:
      case R_C6000_SBR_L16_H:
      case R_C6000_SBR_L16_W:
      case R_C6000_SBR_H16_B:
      case R_C6000_SBR_H16_H:
      case R_C6000_SBR_H16_W:
          *rel_field_ptr = (*rel_field_ptr & ~MASK(16,7)) | (r << 7);
          break;

      /*---------------------------------------------------------------------*/
      /* Linux "import-as-own" copy relocations are not yet supported.       */
      /*---------------------------------------------------------------------*/
      case R_C6000_COPY:

      default:
          DLIF_error(DLET_RELOC,
                     "write_reloc_r called with invalid relocation type!\n");
  }

#if LOADER_DEBUG
  if (debugging_on)
     DLIF_trace("reloc_field 0x%x\n", *rel_field_ptr);
#endif
}

/*****************************************************************************/
/* PACK_RESULT() - Pack the result of a relocation calculation for storage   */
/*      in the relocation field.                                             */
/*****************************************************************************/
static int32_t pack_result(int32_t unpacked_result, int r_type)
{
   switch(r_type)
   {
      case R_C6000_ABS32:
      case R_C6000_ABS16:
      case R_C6000_ABS8:
      case R_C6000_ABS_S16:
      case R_C6000_ABS_L16:
      case R_C6000_SBR_U15_B:
      case R_C6000_SBR_S16:
      case R_C6000_SBR_L16_B:
          return unpacked_result;

      case R_C6000_SBR_U15_H:
      case R_C6000_SBR_L16_H:
      case R_C6000_PREL31:
          return unpacked_result >> 1;

      case R_C6000_PCR_S21:
      case R_C6000_PCR_S12:
      case R_C6000_PCR_S10:
      case R_C6000_PCR_S7:
      case R_C6000_SBR_U15_W:
      case R_C6000_SBR_L16_W:
      case R_C6000_DSBT_INDEX:
          return unpacked_result >> 2;

      case R_C6000_ABS_H16:
      case R_C6000_SBR_H16_B:
          return unpacked_result >> 16;

      case R_C6000_SBR_H16_H:
          return unpacked_result >> 17;

      case R_C6000_SBR_H16_W:
          return unpacked_result >> 18;

      /*---------------------------------------------------------------------*/
      /* Linux "import-as-own" copy relocations are not yet supported.       */
      /*---------------------------------------------------------------------*/
      case R_C6000_COPY:

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
   switch(r_type)
   {
      case R_C6000_ABS8:
         return unmasked_result & 0xFF;

      case R_C6000_ABS32:
         return unmasked_result;

      case R_C6000_ABS16:
      case R_C6000_ABS_S16:
      case R_C6000_ABS_L16:
      case R_C6000_ABS_H16:
      case R_C6000_SBR_S16:
      case R_C6000_SBR_L16_B:
      case R_C6000_SBR_L16_H:
      case R_C6000_SBR_L16_W:
      case R_C6000_SBR_H16_B:
      case R_C6000_SBR_H16_H:
      case R_C6000_SBR_H16_W:
          return unmasked_result & 0xFFFF;

      case R_C6000_PCR_S21:
          return unmasked_result & 0x1FFFFF;

      case R_C6000_PCR_S12:
          return unmasked_result & 0xFFF;

      case R_C6000_PCR_S10:
          return unmasked_result & 0x3FF;

      case R_C6000_PCR_S7:
          return unmasked_result & 0x7F;

      case R_C6000_SBR_U15_B:
      case R_C6000_SBR_U15_H:
      case R_C6000_SBR_U15_W:
      case R_C6000_DSBT_INDEX:
          return unmasked_result & 0x7FFF;

      case R_C6000_PREL31:
          return unmasked_result & 0x7FFFFFFF;

      /*---------------------------------------------------------------------*/
      /* Linux "import-as-own" copy relocations are not yet supported.       */
      /*---------------------------------------------------------------------*/
      case R_C6000_COPY:

      default:
         DLIF_error(DLET_RELOC,
                    "mask_result called with invalid relocation type!\n");
         return 0;
   }
}

/*****************************************************************************/
/* REL_OVERFLOW()                                                            */
/*                                                                           */
/*    Check relocation value against the range associated with a given       */
/*    relocation type field size and signedness.                             */
/*                                                                           */
/*****************************************************************************/
static BOOL rel_overflow(C60_RELOC_TYPE r_type, int32_t reloc_value)
{
   /*------------------------------------------------------------------------*/
   /* Select appropriate range check based on relocation type.               */
   /*------------------------------------------------------------------------*/
   switch(r_type)
   {
      case R_C6000_ABS16:       return ((reloc_value > 65535) ||
                                        (reloc_value < -32768));
      case R_C6000_ABS8:        return ((reloc_value > 255) ||
                                        (reloc_value < -128));
      case R_C6000_PCR_S21:     return ((reloc_value >= 0x400000) ||
                                        (reloc_value < -0x400000));
      case R_C6000_PCR_S12:     return ((reloc_value >= 0x2000) ||
                                        (reloc_value < -0x2000));
      case R_C6000_PCR_S10:     return ((reloc_value >= 0x800) ||
                                        (reloc_value < -0x800));
      case R_C6000_PCR_S7:      return ((reloc_value >= 0x100) ||
                                        (reloc_value < -0x100));
      case R_C6000_SBR_S16:
      case R_C6000_ABS_S16:     return ((reloc_value >= 0x8000) ||
                                        (reloc_value < -0x8000));
      case R_C6000_SBR_U15_B:   return (((uint32_t)reloc_value) >= 0x8000);
      case R_C6000_SBR_U15_H:   return (((uint32_t)reloc_value) >= 0xFFFF);
      case R_C6000_DSBT_INDEX:
      case R_C6000_SBR_U15_W:   return (((uint32_t)reloc_value) >= 0x1FFFD);


      /*---------------------------------------------------------------------*/
      /* Some relocation types suppress overflow checking at link-time.      */
      /*---------------------------------------------------------------------*/
      case R_C6000_ABS_L16:
      case R_C6000_ABS_H16:
      case R_C6000_SBR_L16_B:
      case R_C6000_SBR_L16_H:
      case R_C6000_SBR_L16_W:
      case R_C6000_SBR_H16_B:
      case R_C6000_SBR_H16_H:
      case R_C6000_SBR_H16_W:
         return 0;

      /*---------------------------------------------------------------------*/
      /* 32-bit relocation field values are not checked for overflow.        */
      /*---------------------------------------------------------------------*/
      case R_C6000_ABS32:
      case R_C6000_PREL31:
         return 0;

      /*----------------------------------------------------------------------*/
      /* If relocation type did not appear in the above cases, then we didn't */
      /* expect to see it.                                                    */
      /*----------------------------------------------------------------------*/
      default:
        DLIF_error(DLET_RELOC,
                 "rel_overflow called with invalid relocation type!\n");
        return 1;
        }
}

/*****************************************************************************/
/* RELOC_DO() - Process a single relocation entry.                           */
/*****************************************************************************/
static void reloc_do(C60_RELOC_TYPE r_type,
                     uint8_t *segment_address,
                     uint32_t addend,
                     uint32_t symval,
                     uint32_t spc,
                     int      wrong_endian,
                     uint32_t base_pointer,
                     int32_t  dsbt_index)
{
   int32_t reloc_value = 0;

#if LOADER_DEBUG && LOADER_PROFILE
   /*------------------------------------------------------------------------*/
   /* In debug mode, keep a count of the number of relocations processed.    */
   /* In profile mode, start the clock on a given relocation.                */
   /*------------------------------------------------------------------------*/
   int start_time;
   if (debugging_on || profiling_on)
   {
      DLREL_relocations++;
      if (profiling_on) start_time = clock();
   }
#endif

   /*------------------------------------------------------------------------*/
   /* Calculate the relocation value according to the rules associated with  */
   /* the given relocation type.                                             */
   /*------------------------------------------------------------------------*/
   switch(r_type)
   {
      /*---------------------------------------------------------------------*/
      /* Straight-Up Address relocations (address references).               */
      /*---------------------------------------------------------------------*/
      case R_C6000_ABS32:
      case R_C6000_ABS16:
      case R_C6000_ABS8:
      case R_C6000_ABS_S16:
      case R_C6000_ABS_L16:
      case R_C6000_ABS_H16:
         reloc_value = symval + addend;
         break;

      /*---------------------------------------------------------------------*/
      /* PC-Relative relocations (calls and branches).                       */
      /*---------------------------------------------------------------------*/
      case R_C6000_PCR_S21:
      case R_C6000_PCR_S12:
      case R_C6000_PCR_S10:
      case R_C6000_PCR_S7:
      {
         /*------------------------------------------------------------------*/
         /* Add SPC to segment address to get the PC. Mask for exec-packet   */
         /* boundary.                                                        */
         /*------------------------------------------------------------------*/
         int32_t opnd_p = (spc + (int32_t)segment_address) & 0xffffffe0;
         reloc_value = symval + addend - opnd_p;
         break;
      }

      /*---------------------------------------------------------------------*/
      /* "Place"-relative relocations (TDEH).                                */
      /*---------------------------------------------------------------------*/
      /* These relocations occur in data and refer to a label that occurs    */
      /* at some signed 32-bit offset from the place where the relocation    */
      /* occurs.                                                             */
      /*---------------------------------------------------------------------*/
      case R_C6000_PREL31:
      {
         /*------------------------------------------------------------------*/
         /* Compute location of relocation entry and subtract it from the    */
         /* address of the location being referenced (it is computed very    */
         /* much like a PC-relative relocation, but it occurs in data and    */
         /* is called a "place"-relative relocation).                        */
         /*------------------------------------------------------------------*/
         /* If this is an Elf32_Rel type relocation, then addend is assumed  */
         /* to have been scaled when it was unpacked (field << 1).           */
         /*------------------------------------------------------------------*/
         /* For Elf32_Rela type relocations the addend is assumed to be a    */
         /* signed 32-bit integer value.                                     */
         /*------------------------------------------------------------------*/
         /* Offset is not fetch-packet relative; doesn't need to be masked.  */
         /*------------------------------------------------------------------*/
         int32_t opnd_p = (spc + (int32_t)segment_address);
         reloc_value = symval + addend - opnd_p;
         break;
      }

      /*---------------------------------------------------------------------*/
      /* Static-Base Relative relocations (near-DP).                         */
      /*---------------------------------------------------------------------*/
      case R_C6000_SBR_U15_B:
      case R_C6000_SBR_U15_H:
      case R_C6000_SBR_U15_W:
      case R_C6000_SBR_S16:
      case R_C6000_SBR_L16_B:
      case R_C6000_SBR_L16_H:
      case R_C6000_SBR_L16_W:
      case R_C6000_SBR_H16_B:
      case R_C6000_SBR_H16_H:
      case R_C6000_SBR_H16_W:
         reloc_value = symval + addend - base_pointer;
         break;

      /*---------------------------------------------------------------------*/
      /* R_C6000_DSBT_INDEX - uses value assigned by the dynamic loader to   */
      /*    be the DSBT index for this module as a scaled offset when        */
      /*    referencing the DSBT. The DSBT base address is in symval and the */
      /*    static base is in base_pointer. DP-relative offset to slot in    */
      /*    DSBT is the offset of the DSBT relative to the DP plus the       */
      /*    scaled DSBT index into the DSBT.                                 */
      /*---------------------------------------------------------------------*/
      case R_C6000_DSBT_INDEX:
         reloc_value = ((symval + addend) - base_pointer) + (dsbt_index << 2);
         break;

      /*---------------------------------------------------------------------*/
      /* Linux "import-as-own" copy relocation: after DSO initialization,    */
      /* copy the named object from the DSO into the executable's BSS        */
      /*---------------------------------------------------------------------*/
      /* Linux "import-as-own" copy relocations are not yet supported.       */
      /*---------------------------------------------------------------------*/
      case R_C6000_COPY:

      /*---------------------------------------------------------------------*/
      /* Unrecognized relocation type.                                       */
      /*---------------------------------------------------------------------*/
      default:
         DLIF_error(DLET_RELOC,
                    "reloc_do called with invalid relocation type!\n");
         break;
   }

   /*------------------------------------------------------------------------*/
   /* Overflow checking.  Is relocation value out of range for the size and  */
   /* type of the current relocation?                                        */
   /*------------------------------------------------------------------------*/
   if (rel_overflow(r_type, reloc_value))
      DLIF_error(DLET_RELOC, "relocation overflow!\n");

   /*------------------------------------------------------------------------*/
   /* Move relocation value to appropriate offset for relocation field's     */
   /* location.                                                              */
   /*------------------------------------------------------------------------*/
   reloc_value = pack_result(reloc_value, r_type);

   /*------------------------------------------------------------------------*/
   /* Mask packed result to the size of the relocation field.                */
   /*------------------------------------------------------------------------*/
   reloc_value = mask_result(reloc_value, r_type);

   /*------------------------------------------------------------------------*/
   /* If necessary, Swap endianness of data at relocation address.           */
   /*------------------------------------------------------------------------*/
   if (wrong_endian)
      DLIMP_change_endian32((int32_t*)(segment_address + spc));

   /*------------------------------------------------------------------------*/
   /* Write the relocated 4-byte packet back to the segment buffer.          */
   /*------------------------------------------------------------------------*/
   write_reloc_r(segment_address, spc, r_type, reloc_value);

   /*------------------------------------------------------------------------*/
   /* Change endianness of segment address back to original.                 */
   /*------------------------------------------------------------------------*/
   if (wrong_endian)
      DLIMP_change_endian32((int32_t*)(segment_address + spc));

#if LOADER_DEBUG && LOADER_PROFILE
   /*------------------------------------------------------------------------*/
   /* In profile mode, add elapsed time for this relocation to total time    */
   /* spent doing relocations.                                               */
   /*------------------------------------------------------------------------*/
   if (profiling_on)
      DLREL_total_reloc_time += (clock() - start_time);
   if (debugging_on)
      DLIF_trace("reloc_value = 0x%x\n", reloc_value);
#endif

}

/*****************************************************************************/
/* REL_UNPACK_ADDEND()                                                       */
/*                                                                           */
/*    Unpack addend value from the relocation field.                         */
/*                                                                           */
/*****************************************************************************/
static void rel_unpack_addend(C60_RELOC_TYPE r_type,
                              uint8_t *address,
                              uint32_t *addend)
{
   /*------------------------------------------------------------------------*/
   /* C6000 does not support Elf32_Rel type relocations in the dynamic       */
   /* loader core.  We will emit an internal error and abort until this      */
   /* support is added.  I abort here because this is necessarily a target-  */
   /* specific part of the relocation infrastructure.                        */
   /*------------------------------------------------------------------------*/
   *addend = 0;

   DLIF_error(DLET_RELOC,
              "Internal Error: unpacking addend values from the relocation "
              "field is not supported in the C6000 dynamic loader at this "
              "time; aborting\n");
#if !defined (__KERNEL__)
   exit(1);
#endif
}

/*****************************************************************************/
/* REL_SWAP_ENDIAN()                                                         */
/*                                                                           */
/*    Return TRUE if we should change the endianness of a relocation field.  */
/*                                                                           */
/*****************************************************************************/
static BOOL rel_swap_endian(DLIMP_Dynamic_Module *dyn_module,
                            C60_RELOC_TYPE r_type)
{
   if (dyn_module->wrong_endian) return TRUE;

   return FALSE;
}

/*****************************************************************************/
/* REL_CHANGE_ENDIAN()                                                       */
/*                                                                           */
/*    Change the endianness of the relocation field at the specified address */
/*    in the segment's data.                                                 */
/*                                                                           */
/*****************************************************************************/
static void rel_change_endian(C60_RELOC_TYPE r_type, uint8_t *address)
{
   /*------------------------------------------------------------------------*/
   /* On C6000, all instructions are 32-bits wide.                           */
   /*------------------------------------------------------------------------*/
   DLIMP_change_endian32((int32_t *)address);
}

/*****************************************************************************/
/* READ_REL_TABLE()                                                          */
/*                                                                           */
/*    Read in an Elf32_Rel type relocation table.  This function allocates   */
/*    host memory for the table.                                             */
/*                                                                           */
/*****************************************************************************/
static void read_rel_table(struct Elf32_Rel **rel_table,
                           int32_t table_offset,
                           uint32_t relnum, uint32_t relent,
                           LOADER_FILE_DESC *fd, BOOL wrong_endian)
{
   *rel_table = (struct Elf32_Rel *)DLIF_malloc(relnum * relent);
   if (NULL == *rel_table) {
       DLIF_error(DLET_MEMORY, "Failed to Allocate read_rel_table\n");
       return;
   }
   DLIF_fseek(fd, table_offset, LOADER_SEEK_SET);
   DLIF_fread(*rel_table, relnum, relent, fd);

   if (wrong_endian)
   {
      int i;
      for (i = 0; i < relnum; i++)
         DLIMP_change_rel_endian(*rel_table + i);
   }
}

/*****************************************************************************/
/* PROCESS_REL_TABLE()                                                       */
/*                                                                           */
/*    Process table of Elf32_Rel type relocations.                           */
/*                                                                           */
/*****************************************************************************/
static void process_rel_table(DLOAD_HANDLE handle, DLIMP_Loaded_Segment* seg,
                              struct Elf32_Rel *rel_table,
                              uint32_t relnum,
                              int32_t *start_relidx,
                              uint32_t ti_static_base,
                              DLIMP_Dynamic_Module* dyn_module)
{
   Elf32_Addr seg_start_addr = seg->input_vaddr;
   Elf32_Addr seg_end_addr   = seg_start_addr + seg->phdr.p_memsz;
   BOOL found = FALSE;
   int32_t relidx = *start_relidx;

   /*------------------------------------------------------------------------*/
   /* If the given start reloc index is out of range, then start from the    */
   /* beginning of the given table.                                          */
   /*------------------------------------------------------------------------*/
   if (relidx >= relnum) relidx = 0;

   /*------------------------------------------------------------------------*/
   /* Spin through Elf32_Rel type relocation table.                          */
   /*------------------------------------------------------------------------*/
   for ( ; relidx < relnum; relidx++)
   {
      /*---------------------------------------------------------------------*/
      /* If the relocation offset falls within the segment, process it.      */
      /*---------------------------------------------------------------------*/
      if (rel_table[relidx].r_offset >= seg_start_addr &&
          rel_table[relidx].r_offset < seg_end_addr)
      {
         Elf32_Addr     r_symval = 0;
         C60_RELOC_TYPE r_type  =
                       (C60_RELOC_TYPE)ELF32_R_TYPE(rel_table[relidx].r_info);
         int32_t        r_symid = ELF32_R_SYM(rel_table[relidx].r_info);

         uint8_t *reloc_address = NULL;
         uint32_t pc     = 0;
         uint32_t addend = 0;

         BOOL     change_endian = FALSE;

         found = TRUE;

         /*------------------------------------------------------------------*/
         /* If symbol definition is not found, don't do the relocation.      */
         /* An error is generated by the lookup function.                    */
         /*------------------------------------------------------------------*/
         if (!DLSYM_canonical_lookup(handle, r_symid, dyn_module, &r_symval))
            continue;

         /*------------------------------------------------------------------*/
         /* Addend value is stored in the relocation field.                  */
         /* We'll need to unpack it from the data for the segment that is    */
         /* currently being relocated.                                       */
         /*------------------------------------------------------------------*/
         reloc_address =
                       (((uint8_t *)(seg->phdr.p_vaddr) + seg->reloc_offset) +
                        rel_table[relidx].r_offset - seg->input_vaddr);
         pc = (uint32_t)reloc_address;

         change_endian = rel_swap_endian(dyn_module, r_type);
         if (change_endian)
            rel_change_endian(r_type, reloc_address);

         rel_unpack_addend(
                        (C60_RELOC_TYPE)ELF32_R_TYPE(rel_table[relidx].r_info),
                                                       reloc_address, &addend);

         /*------------------------------------------------------------------*/
         /* Perform actual relocation.  This is a really wide function       */
         /* interface and could do with some encapsulation.                  */
         /*------------------------------------------------------------------*/
         reloc_do(r_type,
                  reloc_address,
                  addend,
                  r_symval,
                  pc,
                  dyn_module->wrong_endian,
                  ti_static_base,
                  dyn_module->dsbt_index);

      }

      else if (found)
         break;
   }
}

/*****************************************************************************/
/* READ_RELA_TABLE()                                                         */
/*                                                                           */
/*    Read in an Elf32_Rela type relocation table.  This function allocates  */
/*    host memory for the table.                                             */
/*                                                                           */
/*****************************************************************************/
static void read_rela_table(struct Elf32_Rela **rela_table,
                            int32_t table_offset,
                            uint32_t relanum, uint32_t relaent,
                            LOADER_FILE_DESC *fd, BOOL wrong_endian)
{
   *rela_table = (struct Elf32_Rela *)DLIF_malloc(relanum * relaent);
   if (NULL == *rela_table) {
       DLIF_error(DLET_MEMORY, "Failed to Allocate read_rela_table\n");
       return;
   }
   DLIF_fseek(fd, table_offset, LOADER_SEEK_SET);
   DLIF_fread(*rela_table, relanum, relaent, fd);

   if (wrong_endian)
   {
      int i;
      for (i = 0; i < relanum; i++)
         DLIMP_change_rela_endian(*rela_table + i);
   }
}

/*****************************************************************************/
/* PROCESS_RELA_TABLE()                                                      */
/*                                                                           */
/*    Process a table of Elf32_Rela type relocations.                        */
/*                                                                           */
/*****************************************************************************/
static void process_rela_table(DLOAD_HANDLE handle, DLIMP_Loaded_Segment *seg,
                               struct Elf32_Rela *rela_table,
                               uint32_t relanum,
                               int32_t *start_relidx,
                               uint32_t ti_static_base,
                               DLIMP_Dynamic_Module *dyn_module)
{
    Elf32_Addr seg_start_addr = seg->input_vaddr;
    Elf32_Addr seg_end_addr   = seg_start_addr + seg->phdr.p_memsz;
    BOOL       found          = FALSE;
    int32_t    relidx         = *start_relidx;

    /*-----------------------------------------------------------------------*/
    /* If the given start reloc index is out of range, then start from       */
    /* the beginning of the given table.                                     */
    /*-----------------------------------------------------------------------*/
    if (relidx > relanum) relidx = 0;

    /*-----------------------------------------------------------------------*/
    /* Spin through RELA relocation table.                                   */
    /*-----------------------------------------------------------------------*/
    for ( ; relidx < relanum; relidx++)
    {
        /*-------------------------------------------------------------------*/
        /* If the relocation offset falls within the segment, process it.    */
        /*-------------------------------------------------------------------*/
        if (rela_table[relidx].r_offset >= seg_start_addr &&
            rela_table[relidx].r_offset < seg_end_addr)
        {
            Elf32_Addr     r_symval;
            C60_RELOC_TYPE r_type  =
                      (C60_RELOC_TYPE)ELF32_R_TYPE(rela_table[relidx].r_info);
            int32_t        r_symid = ELF32_R_SYM(rela_table[relidx].r_info);

            found = TRUE;

            /*---------------------------------------------------------------*/
            /* If symbol definition is not found, don't do the relocation.   */
            /* An error is generated by the lookup function.                 */
            /*---------------------------------------------------------------*/
            if (!DLSYM_canonical_lookup(handle, r_symid, dyn_module, &r_symval))
                continue;

            /*---------------------------------------------------------------*/
            /* Perform actual relocation.  This is a really wide function    */
            /* interface and could do with some encapsulation.               */
            /*---------------------------------------------------------------*/
            reloc_do(r_type,
                     (uint8_t*)(seg->phdr.p_vaddr) + seg->reloc_offset,
                     rela_table[relidx].r_addend,
                     r_symval,
                     rela_table[relidx].r_offset - seg->input_vaddr,
                     dyn_module->wrong_endian,
                     ti_static_base,
                     dyn_module->dsbt_index);
        }

        else if (found)
            break;
    }
}

/*****************************************************************************/
/* PROCESS_GOT_RELOCS()                                                      */
/*                                                                           */
/*    Process all GOT relocations.  It is possible to have both Elf32_Rel    */
/*    and Elf32_Rela type relocations in the same file, so we handle tham    */
/*    both.                                                                  */
/*                                                                           */
/*****************************************************************************/
static void process_got_relocs(DLOAD_HANDLE handle,
                               struct Elf32_Rel* rel_table, uint32_t relnum,
                               struct Elf32_Rela* rela_table, uint32_t relanum,
                               DLIMP_Dynamic_Module* dyn_module)
{
   DLIMP_Loaded_Segment *seg =
       (DLIMP_Loaded_Segment*)(dyn_module->loaded_module->loaded_segments.buf);
   uint32_t num_segs = dyn_module->loaded_module->loaded_segments.size;
   int32_t  rel_relidx = 0;
   int32_t  rela_relidx = 0;
   uint32_t seg_idx = 0;
   uint32_t ti_static_base = 0;

   /*------------------------------------------------------------------------*/
   /* Get the value of the static base (__TI_STATIC_BASE) which will be      */
   /* passed into the relocation table processing functions.                 */
   /*------------------------------------------------------------------------*/
   if (!DLSYM_lookup_local_symtab("__TI_STATIC_BASE", dyn_module->symtab,
                             dyn_module->symnum, &ti_static_base))
      DLIF_error(DLET_RELOC, "Could not resolve value of __TI_STATIC_BASE\n");

   /*------------------------------------------------------------------------*/
   /* Process relocations segment by segment.                                */
   /*------------------------------------------------------------------------*/
   for (seg_idx = 0; seg_idx < num_segs; seg_idx++)
   {
      /*---------------------------------------------------------------------*/
      /* Relocations should not occur in uninitialized segments.             */
      /*---------------------------------------------------------------------*/
      if (!seg[seg_idx].phdr.p_filesz) continue;

      if (rela_table)
         process_rela_table(handle, (seg + seg_idx),
                            rela_table, relanum, &rela_relidx,
                            ti_static_base, dyn_module);

      if (rel_table)
         process_rel_table(handle, (seg + seg_idx),
                            rel_table, relnum, &rel_relidx,
                            ti_static_base, dyn_module);
   }
}

/*****************************************************************************/
/* PROCESS_PLTGOT_RELOCS()                                                   */
/*                                                                           */
/*    Process all PLTGOT relocation entries.  The PLTGOT relocation table    */
/*    can be either Elf32_Rel or Elf32_Rela type.  All PLTGOT relocations    */
/*    ar guaranteed to belong to the same segment.                           */
/*                                                                           */
/*****************************************************************************/
static void process_pltgot_relocs(DLOAD_HANDLE handle, void* plt_reloc_table,
                                  int reltype,
                                  uint32_t pltnum,
                                  DLIMP_Dynamic_Module* dyn_module)
{
   Elf32_Addr r_offset = (reltype == DT_REL) ?
                             ((struct Elf32_Rel *)plt_reloc_table)->r_offset :
                             ((struct Elf32_Rela *)plt_reloc_table)->r_offset;

   DLIMP_Loaded_Segment* seg =
      (DLIMP_Loaded_Segment*)(dyn_module->loaded_module->loaded_segments.buf);

   uint32_t num_segs = dyn_module->loaded_module->loaded_segments.size;
   int32_t  plt_relidx = 0;
   uint32_t seg_idx = 0;
   uint32_t ti_static_base = 0;

   /*------------------------------------------------------------------------*/
   /* Get the value of the static base (__TI_STATIC_BASE) which will be      */
   /* passed into the relocation table processing functions.                 */
   /*------------------------------------------------------------------------*/
   if (!DLSYM_lookup_local_symtab("__TI_STATIC_BASE", dyn_module->symtab,
                             dyn_module->symnum, &ti_static_base))
      DLIF_error(DLET_RELOC, "Could not resolve value of __TI_STATIC_BASE\n");

   /*------------------------------------------------------------------------*/
   /* For each segment s, check if the relocation falls within s. If so,     */
   /* then all other relocations are guaranteed to fall with s. Process      */
   /* all relocations and then return.                                       */
   /*------------------------------------------------------------------------*/
   for (seg_idx = 0; seg_idx < num_segs; seg_idx++)
   {
      Elf32_Addr seg_start_addr = seg[seg_idx].input_vaddr;
      Elf32_Addr seg_end_addr   = seg_start_addr + seg[seg_idx].phdr.p_memsz;

      /*---------------------------------------------------------------------*/
      /* Relocations should not occur in uninitialized segments.             */
      /*---------------------------------------------------------------------*/
      if(!seg[seg_idx].phdr.p_filesz) continue;

      if (r_offset >= seg_start_addr &&
          r_offset < seg_end_addr)
      {
         if (reltype == DT_REL)
            process_rel_table(handle, (seg + seg_idx),
                              (struct Elf32_Rel *)plt_reloc_table,
                              pltnum, &plt_relidx,
                              ti_static_base, dyn_module);
         else
            process_rela_table(handle, (seg + seg_idx),
                               (struct Elf32_Rela *)plt_reloc_table,
                               pltnum, &plt_relidx,
                               ti_static_base, dyn_module);

         break;
      }
   }
}

/*****************************************************************************/
/* RELOCATE() - Perform RELA and REL type relocations for given ELF object   */
/*      file that we are in the process of loading and relocating.           */
/*****************************************************************************/
void DLREL_relocate_c60(DLOAD_HANDLE handle, LOADER_FILE_DESC *fd,
                        DLIMP_Dynamic_Module *dyn_module)
{
   struct Elf32_Dyn  *dyn_nugget = dyn_module->dyntab;
   struct Elf32_Rela *rela_table = NULL;
   struct Elf32_Rel  *rel_table  = NULL;
   void              *plt_table  = NULL;

   /*------------------------------------------------------------------------*/
   /* Read the size of the relocation table (DT_RELASZ) and the size per     */
   /* relocation (DT_RELAENT) from the dynamic segment.                      */
   /*------------------------------------------------------------------------*/
   uint32_t relasz  = DLIMP_get_first_dyntag(DT_RELASZ, dyn_nugget);
   uint32_t relaent = DLIMP_get_first_dyntag(DT_RELAENT, dyn_nugget);
   uint32_t relanum = 0;

   /*------------------------------------------------------------------------*/
   /* Read the size of the relocation table (DT_RELSZ) and the size per      */
   /* relocation (DT_RELENT) from the dynamic segment.                       */
   /*------------------------------------------------------------------------*/
   uint32_t relsz  = DLIMP_get_first_dyntag(DT_RELSZ, dyn_nugget);
   uint32_t relent = DLIMP_get_first_dyntag(DT_RELENT, dyn_nugget);
   uint32_t relnum = 0;

   /*------------------------------------------------------------------------*/
   /* Read the size of the relocation table (DT_PLTRELSZ) and the type of    */
   /* of the PLTGOT relocation table (DT_PLTREL): one of DT_REL or DT_RELA   */
   /*------------------------------------------------------------------------*/
   uint32_t pltrelsz  = DLIMP_get_first_dyntag(DT_PLTRELSZ, dyn_nugget);
   int      pltreltyp = DLIMP_get_first_dyntag(DT_PLTREL, dyn_nugget);
   uint32_t pltnum    = 0;

   /*------------------------------------------------------------------------*/
   /* Find/record DSBT index associated with this module.                    */
   /*------------------------------------------------------------------------*/
   if (is_dsbt_module(dyn_module) &&
       (dyn_module->dsbt_index == DSBT_INDEX_INVALID))
      dyn_module->dsbt_index =
                   DLIF_get_dsbt_index(dyn_module->loaded_module->file_handle);

   /*------------------------------------------------------------------------*/
   /* Read the PLTGOT relocation table from the file                         */
   /* The PLTGOT table is a subsection at the end of either the DT_REL or    */
   /* DT_RELA table.  The size of the table it belongs to DT_REL(A)SZ        */
   /* includes the size of the PLTGOT table.  So it must be adjusted so that */
   /* the GOT relocation tables only contain actual GOT relocations.         */
   /*------------------------------------------------------------------------*/
   if (pltrelsz != INT_MAX)
   {
      if (pltreltyp == DT_REL)
      {
         pltnum = pltrelsz/relent;
         relsz -= pltrelsz;
         read_rel_table(((struct Elf32_Rel**) &plt_table),
                        DLIMP_get_first_dyntag(DT_JMPREL, dyn_nugget),
                        pltnum, relent, fd, dyn_module->wrong_endian);
      }

      else if (pltreltyp == DT_RELA)
      {
         pltnum = pltrelsz/relaent;
         relasz -= pltrelsz;
         read_rela_table(((struct Elf32_Rela**) &plt_table),
                         DLIMP_get_first_dyntag(DT_JMPREL, dyn_nugget),
                         pltnum, relaent, fd, dyn_module->wrong_endian);
      }

      else
      {
         DLIF_error(DLET_RELOC,
                    "DT_PLTREL is invalid: must be either %d or %d\n",
                    DT_REL, DT_RELA);
      }
   }

   /*------------------------------------------------------------------------*/
   /* Read the DT_RELA GOT relocation table from the file                    */
   /*------------------------------------------------------------------------*/
   if (relasz != INT_MAX)
   {
      relanum = relasz/relaent;
      read_rela_table(&rela_table, DLIMP_get_first_dyntag(DT_RELA, dyn_nugget),
                      relanum, relaent, fd, dyn_module->wrong_endian);
   }

   /*------------------------------------------------------------------------*/
   /* Read the DT_REL GOT relocation table from the file                     */
   /*------------------------------------------------------------------------*/
   if (relsz != INT_MAX)
   {
      relnum = relsz/relent;
      read_rel_table(&rel_table, DLIMP_get_first_dyntag(DT_REL, dyn_nugget),
                     relnum, relent, fd, dyn_module->wrong_endian);
   }

   /*------------------------------------------------------------------------*/
   /* Process the PLTGOT relocations                                         */
   /*------------------------------------------------------------------------*/
   if (plt_table)
      process_pltgot_relocs(handle, plt_table, pltreltyp, pltnum, dyn_module);

   /*------------------------------------------------------------------------*/
   /* Process the GOT relocations                                            */
   /*------------------------------------------------------------------------*/
   if (rel_table || rela_table)
      process_got_relocs(handle, rel_table, relnum, rela_table, relanum, dyn_module);

   /*-------------------------------------------------------------------------*/
   /* Free memory used for ELF relocation table copies.                       */
   /*-------------------------------------------------------------------------*/
   if (rela_table) DLIF_free(rela_table);
   if (rel_table)  DLIF_free(rel_table);
   if (plt_table)  DLIF_free(plt_table);
}

/*****************************************************************************/
/* UNIT TESTING INTERFACE                                                    */
/*****************************************************************************/
#ifdef UNIT_TEST
void unit_c60_reloc_do(C60_RELOC_TYPE r_type,
                       uint8_t *address_space,
                       uint32_t addend, uint32_t symval, uint32_t pc,
                       uint32_t static_base, int wrong_endian,
                       int32_t dsbt_index)
{
    reloc_do(r_type, address_space, addend, symval, pc, FALSE, static_base, dsbt_index);
}

#if 0 /* RELA TYPE RELOCATIONS HAVE ADDEND IN RELOCATION ENTRY */
void unit_c60_rel_unpack_addend(C60_RELOC_TYPE r_type,
                                uint8_t* address,
                                uint32_t* addend)
{
    rel_unpack_addend(r_type, address, addend);
}
#endif

BOOL unit_c60_rel_overflow(C60_RELOC_TYPE r_type, int32_t reloc_value)
{
   return rel_overflow(r_type, reloc_value);
}
#endif
