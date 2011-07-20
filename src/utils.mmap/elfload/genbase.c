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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dload.h>


/* global for simple access in dlw_client.c */
FILE * out_file;

#define MAXTAGS 128
unsigned int tag_addr[MAXTAGS];
char *tag_name[MAXTAGS];
int num_tags;

int main(int argc, char * argv[])
{
    DLOAD_HANDLE loader;
    FILE * in_file;
    int i, j, k;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s input-file output-file\n", argv[0]);
        exit(1);
    }

    if ((in_file = fopen(argv[1], "rb")) == NULL) {
        fprintf(stderr, "%s: could not open file %s for reading\n", argv[0],
                argv[1]);
        exit(2);
    }

    if ((out_file = fopen(argv[2], "w+b")) == NULL) {
        fprintf(stderr, "%s: could not open file %s for writing\n", argv[0],
                argv[2]);
        exit(3);
    }

    num_tags = 0;

    for (j = 0, i = 3; i < argc; i++, j++) {
        num_tags++;
        k = 0;
        tag_name[j] = argv[i];
        while (argv[i][k] != ':') {
            k++;
        }
        argv[i][k] = '\0';
        tag_addr[j] = strtoll(&argv[i][k+1], NULL, 16);
        printf("found tag %d: name '%s' addr 0x%x\n", j, tag_name[j],
                tag_addr[j]);
    }

    loader = DLOAD_create(NULL);
    DLOAD_load(loader, in_file, 0, NULL);
    DLOAD_destroy(loader);

    fclose(out_file);
    fclose(in_file);

    return 0;
}
