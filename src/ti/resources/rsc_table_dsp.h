/*
 * Copyright (c) 2012, Texas Instruments Incorporated
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
 *  ======== rsc_table_dsp.h ========
 *
 *  Include this table in each base image, which is read from remoteproc on
 *  host side.
 *
 *  These values are currently very OMAP4 specific!
 *
 */

#ifndef _RSC_TABLE_DSP_H_
#define _RSC_TABLE_DSP_H_

/* Ducati Memory Map: */
#define L4_44XX_BASE            0x4a000000

#define L4_PERIPHERAL_L4CFG     (L4_44XX_BASE)
#define DSP_PERIPHERAL_L4CFG    0x4A000000

#define L4_PERIPHERAL_L4PER     0x48000000
#define DSP_PERIPHERAL_L4PER    0x48000000

#define L4_PERIPHERAL_L4EMU     0x54000000
#define DSP_PERIPHERAL_L4EMU    0x54000000

#define L3_TILER_MODE_0_1       0x60000000
#define DSP_TILER_MODE_0_1      0x60000000

#define L3_TILER_MODE_2         0x70000000
#define DSP_TILER_MODE_2        0x70000000

#define L3_TILER_MODE_3         0x78000000
#define DSP_TILER_MODE_3        0x78000000

#define TEXT_DA                 0x20000000
#define DATA_DA                 0x90000000
#define HEAP_DA                 0x90100000

#define IPC_DA                  0xA0000000
#define IPC_PA                  0xA8800000

#define RPMSG_VRING0_DA               0xA0000000
#define RPMSG_VRING1_DA               0xA0004000

#define CONSOLE_VRING0_DA               0xA0008000
#define CONSOLE_VRING1_DA               0xA000C000

#define BUFS0_DA                0xA0040000
#define BUFS1_DA                0xA0080000

/*
 * sizes of the virtqueues (expressed in number of buffers supported,
 * and must be power of 2)
 */
#define RPMSG_VQ0_SIZE                256
#define RPMSG_VQ1_SIZE                256

#define CONSOLE_VQ0_SIZE                256
#define CONSOLE_VQ1_SIZE                256

/* Size constants must match those used on host: include/asm-generic/sizes.h */
#define SZ_64K                          0x00010000
#define SZ_128K                         0x00020000
#define SZ_256K                         0x00040000
#define SZ_512K                         0x00080000
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

#ifndef DATA_SIZE
#  define DATA_SIZE  (SZ_512K)  /* data */
#endif

#  define HEAP_SIZE  (SZ_2M + SZ_128K) /* heap +pm_data*/

#  define TEXT_SIZE  (SZ_512K)

/* virtio ids: keep in sync with the linux "include/linux/virtio_ids.h" */
#define VIRTIO_ID_CONSOLE	3 /* virtio console */
#define VIRTIO_ID_RPMSG		7 /* virtio remote processor messaging */

/* Indices of rpmsg virtio features we support */
#define VIRTIO_RPMSG_F_NS	0 /* RP supports name service notifications */

/* flip up bits whose indices represent features we support */
#define RPMSG_DSP_C0_FEATURES         1

/* Resource info: Must match include/linux/remoteproc.h: */
#define TYPE_CARVEOUT    0
#define TYPE_DEVMEM      1
#define TYPE_TRACE       2
#define TYPE_VDEV  3

struct fw_rsc_carveout {
	u32 type;
	u32 da;
	u32 pa;
	u32 len;
	u32 flags;
	u32 reserved;
	char name[32];
};

struct fw_rsc_devmem {
	u32 type;
	u32 da;
	u32 pa;
	u32 len;
	u32 flags;
	u32 reserved;
	char name[32];
};

struct fw_rsc_trace {
	u32 type;
	u32 da;
	u32 len;
	u32 reserved;
	char name[32];
};

struct fw_rsc_vdev_vring {
	u32 da; /* device address */
	u32 align;
	u32 num;
	u32 notifyid;
	u32 reserved;
};

struct fw_rsc_vdev {
	u32 type;
	u32 id;
	u32 notifyid;
	u32 dfeatures;
	u32 gfeatures;
	u32 config_len;
	char status;
	char num_of_vrings;
	char reserved[2];
};

struct resource_table {
	u32 version;
	u32 num;
	u32 reserved[2];
	u32 offset[13];

	/* rpmsg vdev entry */
	struct fw_rsc_vdev rpmsg_vdev;
	struct fw_rsc_vdev_vring rpmsg_vring0;
	struct fw_rsc_vdev_vring rpmsg_vring1;

	/* console vdev entry */
	struct fw_rsc_vdev console_vdev;
	struct fw_rsc_vdev_vring console_vring0;
	struct fw_rsc_vdev_vring console_vring1;

	/* data carveout entry */
	struct fw_rsc_carveout data_cout;

	/* heap carveout entry */
	struct fw_rsc_carveout heap_cout;

	/* text carveout entry */
	struct fw_rsc_carveout text_cout;

	/* trace entry */
	struct fw_rsc_trace trace;

	/* devmem entry */
	struct fw_rsc_devmem devmem0;

	/* devmem entry */
	struct fw_rsc_devmem devmem1;

	/* devmem entry */
	struct fw_rsc_devmem devmem2;

	/* devmem entry */
	struct fw_rsc_devmem devmem3;

	/* devmem entry */
	struct fw_rsc_devmem devmem4;

	/* devmem entry */
	struct fw_rsc_devmem devmem5;
};

extern char * xdc_runtime_SysMin_Module_State_0_outbuf__A;
#define TRACEBUFADDR (u32)&xdc_runtime_SysMin_Module_State_0_outbuf__A

#pragma DATA_SECTION(resources, ".resource_table")
#pragma DATA_ALIGN(resources, 4096)

struct resource_table resources = {
	1, /* we're the first version that implements this */
	12, /* number of entries in the table */
	0, 0, /* reserved, must be zero */
	/* offsets to entries */
	{
		offsetof(struct resource_table, rpmsg_vdev),
		offsetof(struct resource_table, console_vdev),
		offsetof(struct resource_table, data_cout),
		offsetof(struct resource_table, heap_cout),
		offsetof(struct resource_table, text_cout),
		offsetof(struct resource_table, trace),
		offsetof(struct resource_table, devmem0),
		offsetof(struct resource_table, devmem1),
		offsetof(struct resource_table, devmem2),
		offsetof(struct resource_table, devmem3),
		offsetof(struct resource_table, devmem4),
		offsetof(struct resource_table, devmem5),
	},

	/* rpmsg vdev entry */
	{
		TYPE_VDEV, VIRTIO_ID_RPMSG, 0,
		RPMSG_DSP_C0_FEATURES, 0, 0, 0, 2, { 0, 0 },
		/* no config data */
	},
	/* the two vrings */
	{ RPMSG_VRING0_DA, 4096, RPMSG_VQ0_SIZE, 1, 0 },
	{ RPMSG_VRING1_DA, 4096, RPMSG_VQ1_SIZE, 2, 0 },

	/* console vdev entry */
	{
		TYPE_VDEV, VIRTIO_ID_CONSOLE, 3,
		0, 0, 0, 0, 2, { 0, 0 },
		/* no config data */
	},
	/* the two vrings */
	{ CONSOLE_VRING0_DA, 4096, CONSOLE_VQ0_SIZE, 4, 0 },
	{ CONSOLE_VRING1_DA, 4096, CONSOLE_VQ1_SIZE, 5, 0 },

	{
		TYPE_CARVEOUT, DATA_DA, 0, DATA_SIZE, 0, 0, "DSP_MEM_DATA",
	},

	{
		TYPE_CARVEOUT, HEAP_DA, 0, HEAP_SIZE, 0, 0, "DSP_MEM_HEAP",
	},

	{
		TYPE_CARVEOUT, TEXT_DA, 0, TEXT_SIZE, 0, 0, "DSP_MEM_TEXT",
	},

	{
		TYPE_TRACE, TRACEBUFADDR, 0x8000, 0, "trace:dsp",
	},

	{
		TYPE_DEVMEM, IPC_DA, IPC_PA, SZ_1M, 0, 0, "DSP_MEM_IPC",
	},

	{
		TYPE_DEVMEM,
		DSP_TILER_MODE_0_1, L3_TILER_MODE_0_1,
		SZ_256M, 0, 0, "DSP_TILER_MODE_0_1",
	},

	{
		TYPE_DEVMEM,
		DSP_TILER_MODE_2, L3_TILER_MODE_2,
		SZ_128M, 0, 0, "DSP_TILER_MODE_2",
	},

	{
		TYPE_DEVMEM,
		DSP_TILER_MODE_3, L3_TILER_MODE_3,
		SZ_128M, 0, 0, "DSP_TILER_MODE_3",
	},

	{
		TYPE_DEVMEM,
		DSP_PERIPHERAL_L4CFG, L4_PERIPHERAL_L4CFG,
		SZ_16M, 0, 0, "DSP_PERIPHERAL_L4CFG",
	},

	{
		TYPE_DEVMEM,
		DSP_PERIPHERAL_L4PER, L4_PERIPHERAL_L4PER,
		SZ_16M, 0, 0, "DSP_PERIPHERAL_L4PER",
	},
};

#endif /* _RSC_TABLE_DSP_H_ */