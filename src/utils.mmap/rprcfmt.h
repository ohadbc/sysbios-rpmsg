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
    RSC_CARVEOUT    = 0,
    RSC_DEVMEM      = 1,
    RSC_DEVICE      = 2,
    RSC_IRQ         = 3,
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
    u64 pa;
    u32 len;
    u32 flags;
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
