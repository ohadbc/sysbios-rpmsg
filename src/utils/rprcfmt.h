/*
 *  Copyright (c) 2011, Texas Instruments Incorporated
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

#ifndef _RPROCFMT_H_
#define _RPROCFMT_H_

typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned char u8;
#define __packed __attribute__ ((packed))

/**
 * The following enums and structures define the binary format of the images
 * we load and run the remote processors with.
 *
 * The binary format is as follows:
 *
 * struct {
 *     char magic[4] = { 'R', 'P', 'R', 'C' };
 *     u32 version;
 *     u32 header_len;
 *     char header[...] = { header_len bytes of unformatted, textual header };
 *     struct sections {
 *         u32 type;
 *         u64 da;
 *         u32 len;
 *         u8 content[...] = { len bytes of binary data };
 *     } [ no limit on number of sections ];
 * } __packed;
 */

#define RPROC_FW_MAGIC  "RPRC"

enum rproc_fw_resource_type {
    RSC_MEMORY      = 0,
    RSC_DEVICE      = 1,
    RSC_IRQ         = 2,
    RSC_SERVICE     = 3,
    RSC_TRACE       = 4,
    RSC_BOOTADDR    = 5,
};

enum rproc_fw_section_type {
    FW_RESOURCE    = 0,
    FW_TEXT        = 1,
    FW_DATA        = 2,
};

struct rproc_fw_resource {
    u32 type;
    u64 da;
    u32 len;
    u32 reserved;
    u8 name[48];
} __packed;

struct rproc_fw_section {
    u32 type;
    u64 da;
    u32 len;
    char content[0];
} __packed;

struct rproc_fw_header {
    char magic[4];
    u32 version;
    u32 header_len;
    char header[0];
} __packed;

#endif
