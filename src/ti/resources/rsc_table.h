/*
 * Copyright (c) 2011, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 *  ======== rsc_table.h ========
 *
 *  Include this table in each base image, which is read from remoteproc on
 *  host side.
 *
 *  These values are currently very OMAP4 specific!
 *
 */


#ifndef _RSC_TABLE_H_
#define _RSC_TABLE_H_



/* Ducati Memory Map: */
#define L4_44XX_BASE            0x4a000000

#define L4_PERIPHERAL_L4CFG     (L4_44XX_BASE)
#define IPU_PERIPHERAL_L4CFG    0xAA000000

#define L4_PERIPHERAL_L4PER     0x48000000
#define IPU_PERIPHERAL_L4PER    0xA8000000

#define L4_PERIPHERAL_L4EMU     0x54000000
#define IPU_PERIPHERAL_L4EMU    0xB4000000

#define L3_IVAHD_CONFIG         0x5A000000
#define IPU_IVAHD_CONFIG        0xBA000000

#define L3_IVAHD_SL2            0x5B000000
#define IPU_IVAHD_SL2           0xBB000000

#define L3_TILER_MODE_0_1       0x60000000
#define IPU_TILER_MODE_0_1      0x60000000

#define L3_TILER_MODE_2         0x70000000
#define IPU_TILER_MODE_2        0x70000000

#define L3_TILER_MODE_3         0x78000000
#define IPU_TILER_MODE_3        0x78000000

#define L3_IVAHD_CONFIG         0x5A000000
#define IPU_IVAHD_CONFIG        0xBA000000

#define L3_IVAHD_SL2            0x5B000000
#define IPU_IVAHD_SL2           0xBB000000

#define IPU_MEM_TEXT            0x0

#define IPU_MEM_DATA            0x80000000

#define IPU_MEM_IPC             0xA0000000


/* Size constants must match those used on host: include/asm-generic/sizes.h */
#define SZ_1M                           0x00100000
#define SZ_2M                           0x00200000
#define SZ_4M                           0x00400000
#define SZ_8M                           0x00800000
#define SZ_16M                          0x01000000
#define SZ_32M                          0x02000000
#define SZ_64M                          0x04000000
#define SZ_128M                         0x08000000
#define SZ_256M                         0x10000000
#define SZ_512M                         0x20000000


/* TODO:
 * Remove hardcoded RAM Addresses once we have the PA->VA lookup integrated.
 * IPC region should not be hard-coded. Text area is also not hard-coded since
 * VA to PA translation is not required. */
#ifndef IPU_MEM_DATA_ADDR
#  define IPU_MEM_DATA_ADDR           0xB9800000
#endif
#ifndef IPU_MEM_DATA_SIZE
#  define IPU_MEM_DATA_SIZE  (SZ_1M * 96)  /* OMX is a little piggy */
#endif

/* Resource info: Must match include/linux/remoteproc.h: */
#define TYPE_CARVEOUT    0
#define TYPE_DEVMEM      1
#define TYPE_DEVICE      2
#define TYPE_IRQ         3
#define TYPE_TRACE       4
#define TYPE_ENTRYPOINT  5

struct resource {
    u32 type;
    u32 da_low;       /* Device (Ducati virtual) Address */
    u32 da_high;
    u32 pa_low;       /* Physical Address */
    u32 pa_high;
    u32 size;
    u32 reserved;
    char name[48];
};

#pragma DATA_SECTION(resources, ".resource_table")
#pragma DATA_ALIGN(resources, 4096)
struct resource resources[] = {
    { TYPE_TRACE, 0, 0, 0, 0, 0x8000, 0, "0" },
    { TYPE_TRACE, 1, 0, 0, 0, 0x8000, 0, "1" },
    { TYPE_ENTRYPOINT, 0, 0, 0, 0, 0, 0, "0" },
    { TYPE_ENTRYPOINT, 1, 0, 0, 0, 0, 0, "1" },
    { TYPE_DEVMEM, IPU_TILER_MODE_0_1, 0, L3_TILER_MODE_0_1, 0, SZ_256M,
       0, "IPU_TILER_MODE_0_1" },
    { TYPE_DEVMEM, IPU_TILER_MODE_2, 0, L3_TILER_MODE_2, 0, SZ_128M,
       0, "IPU_TILER_MODE_2" },
    { TYPE_DEVMEM, IPU_TILER_MODE_3, 0, L3_TILER_MODE_3, 0, SZ_128M,
       0, "IPU_TILER_MODE_3" },
    { TYPE_DEVMEM, IPU_PERIPHERAL_L4CFG, 0, L4_PERIPHERAL_L4CFG, 0, SZ_16M,
       0, "IPU_PERIPHERAL_L4CFG" },
    { TYPE_DEVMEM, IPU_PERIPHERAL_L4PER, 0, L4_PERIPHERAL_L4PER, 0, SZ_16M,
       0,"IPU_PERIPHERAL_L4PER" },
    { TYPE_DEVMEM, IPU_IVAHD_CONFIG, 0, L3_IVAHD_CONFIG, 0, SZ_16M,
       0, "IPU_IVAHD_CONFIG" },
    { TYPE_DEVMEM, IPU_IVAHD_SL2, 0, L3_IVAHD_SL2, 0, SZ_16M,
       0, "IPU_IVAHD_SL2" },
    /* IPU_MEM_IPC needs to be first for dynamic carveout */
    { TYPE_CARVEOUT, IPU_MEM_IPC,  0, 0, 0, SZ_1M, 0, "IPU_MEM_IPC"  },
    { TYPE_CARVEOUT, IPU_MEM_TEXT, 0, 0, 0, SZ_4M, 0, "IPU_MEM_TEXT" },
    { TYPE_CARVEOUT, IPU_MEM_DATA, 0, IPU_MEM_DATA_ADDR, 0, IPU_MEM_DATA_SIZE,
       0, "IPU_MEM_DATA" },
};

#endif /* _RSC_TABLE_H_ */
