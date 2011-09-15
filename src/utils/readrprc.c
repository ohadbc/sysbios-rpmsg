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
/*
 *  ======== dbg-loader.c ========
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include "rprcfmt.h"

static int dump_image(void * data, int size);

/*
 *  ======== main ========
 */
int main(int argc, char * argv[])
{
    FILE * fp;
    struct stat st;
    char * filename;
    void * data;
    int size;
    int status;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
        exit(1);
    }

    filename = argv[1];
    if ((fp = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "%s: could not open: %s\n", argv[0], filename);
        exit(2);
    }

    fstat(fileno(fp), &st);
    size = st.st_size;
    data = malloc(size);
    fread(data, 1, size, fp);
    fclose(fp);

    status = dump_image(data, size);

    free(data);

    return status;
}

/*
 *  ======== dump_resources ========
 */
static void dump_resources(struct rproc_fw_section * s)
{
    struct rproc_fw_resource * res = (struct rproc_fw_resource * )s->content;
    int i;

    printf("resource table: %d\n", sizeof(struct rproc_fw_resource));
    for (i = 0; i < s->len / sizeof(struct rproc_fw_resource); i++) {
        printf("resource: %d, da: 0x%8llx, pa: 0x%8llx, len: 0x%8x, name: %s\n",
               res[i].type, res[i].da, res[i].pa, res[i].len, res[i].name);
    }
    printf("\n");
}

/*
 *  ======== dump_image ========
 */
static int dump_image(void * data, int size)
{
    struct rproc_fw_header *hdr;
    struct rproc_fw_section *s;

    hdr = (struct rproc_fw_header *)data;

    /* check that magic is what we expect */
    if (memcmp(hdr->magic, RPROC_FW_MAGIC, sizeof(hdr->magic))) {
        fprintf(stderr, "invalid magic number: %.4s\n", hdr->magic);
        return -1;
    }

    /* baseimage information */
    printf("magic number %.4s\n", hdr->magic);
    printf("header version %d\n", hdr->version);
    printf("header size %d\n", hdr->header_len);
    printf("header data\n%s\n", hdr->header);

    /* get the first section */
    s = (struct rproc_fw_section *)(hdr->header + hdr->header_len);

    while ((u8 *)s < (u8 *)(data + size)) {
        printf("section: %d, address: 0x%8llx, size: 0x%8x\n", s->type,
               s->da, s->len);
        if (s->type == FW_RESOURCE) {
            dump_resources(s);
        }

        s = ((void *)s->content) + s->len;
    }

    return 0;
}
