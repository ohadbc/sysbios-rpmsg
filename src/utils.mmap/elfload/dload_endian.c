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
/* dload_endian.c                                                            */
/*                                                                           */
/* Simple helper functions to assist core loader with endian-ness issues     */
/* when the host endian-ness may be opposite the endian-ness of the target.  */
/*****************************************************************************/
#include "dload_endian.h"

/*****************************************************************************/
/* DLIMP_GET_ENDIAN() - Determine endianness of the host.  Uses ELF          */
/*      endianness constants.                                                */
/*****************************************************************************/
int DLIMP_get_endian()
{
    int32_t x = 0x1;

    if (*((int16_t*)(&x))) return ELFDATA2LSB;

    return ELFDATA2MSB;
}

/*****************************************************************************/
/* DLIMP_CHANGE_ENDIAN32() - Swap endianness of a 32-bit integer.            */
/*****************************************************************************/
void DLIMP_change_endian32(int32_t* to_change)
{
    int32_t temp = 0;
    temp += (*to_change & 0x000000FF) << 24;
    temp += (*to_change & 0x0000FF00) << 8;
    temp += (*to_change & 0x00FF0000) >> 8;
    temp += (*to_change & 0xFF000000) >> 24;
    *to_change = temp;
}

/*****************************************************************************/
/* DLIMP_CHANGE_ENDIAN16() - Swap endianness of a 16-bit integer.            */
/*****************************************************************************/
void DLIMP_change_endian16(int16_t* to_change)
{
    int16_t temp = 0;
    temp += (*to_change & 0x00FF) << 8;
    temp += (*to_change & 0xFF00) >> 8;
    *to_change = temp;
}

/*****************************************************************************/
/* DLIMP_CHANGE_EHDR_ENDIAN() - Swap endianness of an ELF file header.       */
/*****************************************************************************/
void DLIMP_change_ehdr_endian(struct Elf32_Ehdr* ehdr)
{
    DLIMP_change_endian16((int16_t*)(&ehdr->e_type));
    DLIMP_change_endian16((int16_t*)(&ehdr->e_machine));
    DLIMP_change_endian32((int32_t*)(&ehdr->e_version));
    DLIMP_change_endian32((int32_t*)(&ehdr->e_entry));
    DLIMP_change_endian32((int32_t*)(&ehdr->e_phoff));
    DLIMP_change_endian32((int32_t*)(&ehdr->e_shoff));
    DLIMP_change_endian32((int32_t*)(&ehdr->e_flags));
    DLIMP_change_endian16((int16_t*)(&ehdr->e_ehsize));
    DLIMP_change_endian16((int16_t*)(&ehdr->e_phentsize));
    DLIMP_change_endian16((int16_t*)(&ehdr->e_phnum));
    DLIMP_change_endian16((int16_t*)(&ehdr->e_shentsize));
    DLIMP_change_endian16((int16_t*)(&ehdr->e_shnum));
    DLIMP_change_endian16((int16_t*)(&ehdr->e_shstrndx));
}

/*****************************************************************************/
/* DLIMP_CHANGE_PHDR_ENDIAN() - Swap endianness of an ELF program header.    */
/*****************************************************************************/
void DLIMP_change_phdr_endian(struct Elf32_Phdr* phdr)
{
    DLIMP_change_endian32((int32_t*)(&phdr->p_type));
    DLIMP_change_endian32((int32_t*)(&phdr->p_offset));
    DLIMP_change_endian32((int32_t*)(&phdr->p_vaddr));
    DLIMP_change_endian32((int32_t*)(&phdr->p_paddr));
    DLIMP_change_endian32((int32_t*)(&phdr->p_filesz));
    DLIMP_change_endian32((int32_t*)(&phdr->p_memsz));
    DLIMP_change_endian32((int32_t*)(&phdr->p_flags));
    DLIMP_change_endian32((int32_t*)(&phdr->p_align));
}

/*****************************************************************************/
/* DLIMP_CHANGE_DYNENT_ENDIAN() - Swap endianness of a dynamic table entry.  */
/*****************************************************************************/
void DLIMP_change_dynent_endian(struct Elf32_Dyn* dyn)
{
    DLIMP_change_endian32((int32_t*)(&dyn->d_tag));
    DLIMP_change_endian32((int32_t*)(&dyn->d_un.d_val));
}

/*****************************************************************************/
/* DLIMP_CHANGE_SYM_ENDIAN() - Swap endianness of an ELF symbol table entry. */
/*****************************************************************************/
void DLIMP_change_sym_endian(struct Elf32_Sym* sym)
{
    DLIMP_change_endian32((int32_t*)(&sym->st_name));
    DLIMP_change_endian32((int32_t*)(&sym->st_value));
    DLIMP_change_endian32((int32_t*)(&sym->st_size));
    DLIMP_change_endian16((int16_t*)(&sym->st_shndx));
}

/*****************************************************************************/
/* DLIMP_CHANGE_RELA_ENDIAN() - Swap endianness of a RELA-type relocation.   */
/*****************************************************************************/
void DLIMP_change_rela_endian(struct Elf32_Rela* ra)
{
    DLIMP_change_endian32((int32_t*)(&ra->r_offset));
    DLIMP_change_endian32((int32_t*)(&ra->r_info));
    DLIMP_change_endian32((int32_t*)(&ra->r_addend));
}

/*****************************************************************************/
/* DLIMP_CHANGE_REL_ENDIAN() - Swap endianness of a REL-type relocation.     */
/*****************************************************************************/
void DLIMP_change_rel_endian(struct Elf32_Rel* r)
{
    DLIMP_change_endian32((int32_t*)(&r->r_offset));
    DLIMP_change_endian32((int32_t*)(&r->r_info));
}
