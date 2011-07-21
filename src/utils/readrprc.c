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
        printf("type: %d, addr: 0x%llx, len: %d, name: %s\n", res[i].type,
               res[i].da, res[i].len, res[i].name);
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
        printf("type: %d, address: 0x%llx, size: 0x%x\n", s->type,
               s->da, s->len);
        if (s->type == FW_RESOURCE) {
            dump_resources(s);
        }

        s = ((void *)s->content) + s->len;
    }

    return 0;
}
