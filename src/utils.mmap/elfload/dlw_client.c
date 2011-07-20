/*
 *  Syslink-IPC for TI OMAP Processors
 *
 *  Copyright (c) 2008-2011, Texas Instruments Incorporated
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
/* dlw_client.c                                                              */
/*                                                                           */
/* DLW implementation of client functions required by dynamic loader API.    */
/* Please see list of client-required API functions in dload_api.h.          */
/*                                                                           */
/* DLW is expected to run on the DSP.  It uses C6x RTS functions for file    */
/* I/O and memory management (both host and target memory).                  */
/*                                                                           */
/* A loader that runs on a GPP for the purposes of loading C6x code onto a   */
/* DSP will likely need to re-write all of the functions contained in this   */
/* module.                                                                   */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dload_api.h>

#include "../rprcfmt.h"

extern FILE * out_file;
extern unsigned int tag_addr[];
extern char *tag_name[];
extern int num_tags;

/*
 * insert data at the beginning of an open, existing file
 * note that file needs to be opened with "w+b"
 */
static int finsert(unsigned char * newData, int numBytes, FILE * fp)
{
    struct stat statBuf;
    off_t size;
    char * data;
    int n;

    /* get size of current data in file and alloc buffer to hold it */
    fseek(fp, 0, SEEK_SET);
    fstat(fileno(fp), &statBuf);
    size = statBuf.st_size;

    data = malloc(size);
    fread(data, 1, size, fp);

    /* back to the beginning of file */
    fseek(fp, 0, SEEK_SET);

    /* write the new data */
    n = fwrite(newData, 1, numBytes, fp);
    if (!n || ferror(fp)) {
        perror("first fwrite: ");
    }

    /* write the original data */
    n = fwrite(data, 1, size, fp);
    if (!n || ferror(fp)) {
        perror("second fwrite: ");
    }

    fseek(fp, 0, SEEK_END);

    free(data);

    return size + numBytes;
}

static void patchup_resources(struct rproc_fw_resource *res,
                              unsigned int res_size)
{
    int i;
    struct rproc_fw_resource *res_end;

    printf("resource size: %d\n", sizeof(struct rproc_fw_resource));

    res_end = (struct rproc_fw_resource *)((unsigned int)res + res_size);

    while (res < res_end) {
        printf("resource: %d, da: %llx, pa: %llx, len: %d, name: %s\n",
                res->type, res->da, res->pa, res->len, res->name);
        switch (res->type) {
        case RSC_TRACE:
            printf("found TRACE resource, looking for corresponding tag...\n");

            for (i = 0; i < num_tags; i++) {
                if (!strncmp(tag_name[i], "trace", 5)) {
                    if (res->da == atoi(&tag_name[i][5])) {
                        printf("...found tag %s\n", tag_name[i]);
                        printf("patching address 0x%x\n", tag_addr[i]);
                        res->da = tag_addr[i];
                        break;
                    }
                }
            }

            if (i == num_tags) {
                printf("...no tag found, ignoring resource\n");
            }
            break;

        case RSC_BOOTADDR:
            printf("found ENTRYPOINT resource, looking for corresponding tag...\n");

            for (i = 0; i < num_tags; i++) {
                if (!strncmp(tag_name[i], "entry", 5)) {
                    if (res->da == atoi(&tag_name[i][5])) {
                        printf("...found tag %s\n", tag_name[i]);
                        printf("patching address 0x%x\n", tag_addr[i]);
                        res->da = tag_addr[i];
                        break;
                    }
                }
            }

            if (i == num_tags) {
                printf("...no tag found, ignoring resource\n");
            }
            break;

        case RSC_DEVMEM:
        case RSC_IRQ:
        case RSC_DEVICE:
            printf("found M/I/D/S resource, nothing to do\n");
            break;

        default:
            fprintf(stderr, "unknown resource type %d\n", res->type);
            break;
        }
        res++;
    }
}

/*---------------------------------------------------------------------------*/
/* Target Memory Access / Write Services                                     */
/*                                                                           */
/*    The client side's target memory allocator infrastructure communicates  */
/*    with the core loader through the DLOAD_MEMORY_REQUEST and              */
/*    DLOAD_MEMORY_SEGMENT data structures defined above.  To complete the   */
/*    loading of an object segment, the segment may need to be relocated     */
/*    before it is actually written to target memory in the space that was   */
/*    allocated for it by DLIF_allocate().                                   */
/*                                                                           */
/*    The client side of the dynamic loader provides two functions to help   */
/*    complete the process of loading an object segment, DLIF_copy() and     */
/*    DLIF_write().                                                          */
/*                                                                           */
/*    These functions help to make the core loader truly independent of      */
/*    whether it is running on the host or target architecture and how the   */
/*    client provides for reading/writing from/to target memory.             */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/* DLIF_copy()                                                               */
/*                                                                           */
/*    Copy segment data from the object file described in the 'fp' and       */
/*    'offset' of the DLOAD_MEMORY_REQUEST into host accessible memory so    */
/*    that it can relocated or otherwise manipulated by the core loader.     */
/*                                                                           */
/*---------------------------------------------------------------------------*/
BOOL     DLIF_copy(void* client_handle, struct DLOAD_MEMORY_REQUEST* targ_req)
{
    struct DLOAD_MEMORY_SEGMENT* obj_desc = targ_req->segment;
    FILE * f = targ_req->fp;
    unsigned int dst_addr = 0;
    struct rproc_fw_section * fwsect;
    void * buf;
    void * fwbuf;
    unsigned int buf_size;
    unsigned int fwbuf_size;
    unsigned int type;
    int i;

    if (obj_desc->objsz_in_bytes == 0) {
        printf("skipping empty section at %p\n", obj_desc->target_address);
        return TRUE;
    }

    dst_addr = (unsigned int)obj_desc->target_address;

    /* insure all sections are word-multiples for easier loading */
    buf_size = (obj_desc->objsz_in_bytes + 3) & ~0x3;
    fwbuf_size = buf_size + 16;
    fwbuf = calloc(4, fwbuf_size / 4);
    buf = fwbuf + 16;
    fseek(f, targ_req->offset, SEEK_SET);
    fread(buf, obj_desc->objsz_in_bytes, 1, f);

    type = FW_DATA;        // default
    for (i = 0; i < num_tags; i++) {
        type = FW_DATA;        // default
        if (dst_addr == tag_addr[i]) {
            printf("  matched destination addr w/ specified tag addr\n");
            if (!strcmp(tag_name[i], "restab")) {
                printf("  found 'restab' section\n");
                type = FW_RESOURCE;
                patchup_resources((struct rproc_fw_resource *)buf, buf_size);
                break;
            }
            else if (!strcmp(tag_name[i], "text")) {
                printf("  found 'text' section\n");
                type = FW_TEXT;
                break;
            }
            else if (!strcmp(tag_name[i], "data")) {
                printf("  found 'data' section\n");
                type = FW_DATA;
                break;
            }
        }
    }

    fwsect = (struct rproc_fw_section *)fwbuf;

    fwsect->type = type;
    fwsect->da = dst_addr;
    fwsect->len = buf_size;
    if (type == FW_RESOURCE) {
        finsert(fwbuf, fwbuf_size, out_file);
    }
    else {
        fwrite(fwbuf, 1, fwbuf_size, out_file);
    }

    free(fwbuf);

    return 1;
}

/*---------------------------------------------------------------------------*/
/* File I/O                                                                  */
/*                                                                           */
/*    The client side of the dynamic loader must provide basic file I/O      */
/*    capabilities so that the core loader has random access into any        */
/*    object file that it is asked to load.                                  */
/*                                                                           */
/*    The client side of the dynamic loader must provide a definition of     */
/*    the LOADER_FILE_DESC in dload_filedefs.h.  This allows the core loader */
/*    to be independent of how the client accesses raw data in an object     */
/*    file.                                                                  */
/*                                                                           */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* DLIF_fseek()                                                              */
/*                                                                           */
/*    Seek to a position in a file (accessed via 'stream') based on the      */
/*    values for offset and origin.                                          */
/*                                                                           */
/*---------------------------------------------------------------------------*/
int      DLIF_fseek(LOADER_FILE_DESC *stream, int32_t offset, int origin)
{
    return fseek(stream, offset, origin);
}

/*---------------------------------------------------------------------------*/
/* DLIF_ftell()                                                              */
/*                                                                           */
/*    Return the current file position in the file identified in the         */
/*    LOADER_FILE_DESC pointed to by 'stream'.                               */
/*                                                                           */
/*---------------------------------------------------------------------------*/
int32_t  DLIF_ftell(LOADER_FILE_DESC *stream)
{
    return ftell(stream);
}

/*---------------------------------------------------------------------------*/
/* DLIF_fread()                                                              */
/*                                                                           */
/*    Read 'size' * 'nmemb' bytes of data from the file identified in the    */
/*    LOADER_FILE_DESC object pointed to by 'stream', and write that data    */
/*    into the memory accessed via 'ptr'.                                    */
/*                                                                           */
/*---------------------------------------------------------------------------*/
size_t   DLIF_fread(void *ptr, size_t size, size_t nmemb,
                    LOADER_FILE_DESC *stream)
{
    return fread(ptr, size, nmemb, stream);
}

/*---------------------------------------------------------------------------*/
/* DLIF_fclose()                                                             */
/*                                                                           */
/*    Close a file that was opened on behalf of the core loader. Ownership   */
/*    of the file pointer in question belongs to the core loader, but the    */
/*    client has exclusive access to the file system.                        */
/*                                                                           */
/*---------------------------------------------------------------------------*/
int      DLIF_fclose(LOADER_FILE_DESC *fd)
{
    return fclose(fd);
}

/*---------------------------------------------------------------------------*/
/* Host Memory Management                                                    */
/*                                                                           */
/*    Allocate and free host memory as needed for the dynamic loader's       */
/*    internal data structures.  If the dynamic loader resides on the        */
/*    target architecture, then this memory is allocated from a target       */
/*    memory heap that must be managed separately from memory that is        */
/*    allocated for a dynamically loaded object file.                        */
/*                                                                           */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* DLIF_malloc()                                                             */
/*                                                                           */
/*    Allocate 'size' bytes of memory space that is usable as scratch space  */
/*    (appropriate for the loader's internal data structures) by the dynamic */
/*    loader.                                                                */
/*                                                                           */
/*---------------------------------------------------------------------------*/
void*    DLIF_malloc(size_t size)
{
    return malloc(size);
}

/*---------------------------------------------------------------------------*/
/* DLIF_free()                                                               */
/*                                                                           */
/*    Free memory space that was previously allocated by DLIF_malloc().      */
/*                                                                           */
/*---------------------------------------------------------------------------*/
void     DLIF_free(void* ptr)
{
    free(ptr);
}

/*****************************************************************************/
/* DLIF_LOAD_DEPENDENT() - Perform whatever maintenance is needed in the     */
/*      client when loading of a dependent file is initiated by the core     */
/*      loader.  Open the dependent file on behalf of the core loader,       */
/*      then invoke the core loader to get it into target memory. The core   */
/*      loader assumes ownership of the dependent file pointer and must ask  */
/*      the client to close the file when it is no longer needed.            */
/*                                                                           */
/*      If debug support is needed under the Braveheart model, then create   */
/*      a host version of the debug module record for this object.  This     */
/*      version will get updated each time we allocate target memory for a   */
/*      segment that belongs to this module.  When the load returns, the     */
/*      client will allocate memory for the debug module from target memory  */
/*      and write the host version of the debug module into target memory    */
/*      at the appropriate location.  After this takes place the new debug   */
/*      module needs to be added to the debug module list.  The client will  */
/*      need to update the tail of the DLModules list to link the new debug  */
/*      module onto the end of the list.                                     */
/*                                                                           */
/*****************************************************************************/
int DLIF_load_dependent(void* client_handle, const char* so_name)
{
    printf("%s: unexpected call\n", __func__);
    return 0;
}

/*****************************************************************************/
/* DLIF_UNLOAD_DEPENDENT() - Perform whatever maintenance is needed in the   */
/*      client when unloading of a dependent file is initiated by the core   */
/*      loader.  Invoke the DLOAD_unload() function to get the core loader   */
/*      to release any target memory that is associated with the dependent   */
/*      file's segments.                                                     */
/*****************************************************************************/
void DLIF_unload_dependent(void* client_handle, uint32_t file_handle)
{
    printf("%s: unexpected call\n", __func__);
}


/*****************************************************************************/
/* DLIF_WARNING() - Write out a warning message from the core loader.        */
/*****************************************************************************/
void DLIF_warning(LOADER_WARNING_TYPE wtype, const char *fmt, ...)
{
    va_list ap;
    va_start(ap,fmt);
    printf("<< D L O A D >> WARNING: ");
    vprintf(fmt,ap);
    va_end(ap);
}

/*****************************************************************************/
/* DLIF_ERROR() - Write out an error message from the core loader.           */
/*****************************************************************************/
void DLIF_error(LOADER_ERROR_TYPE etype, const char *fmt, ...)
{
    va_list ap;
    va_start(ap,fmt);
    printf("<< D L O A D >> ERROR: ");
    vprintf(fmt,ap);
    va_end(ap);
}

/*****************************************************************************/
/* DLIF_trace() - Write out a trace from the core loader.                    */
/*****************************************************************************/
void DLIF_trace(const char *fmt, ...)
{
    va_list ap;
    va_start(ap,fmt);
    vprintf(fmt,ap);
    va_end(ap);
}

/*---------------------------------------------------------------------------*/
/* DLIF_allocate()                                                           */
/*                                                                           */
/*    Given a DLOAD_MEMORY_REQUEST created by the core loader, allocate      */
/*    target memory to fulfill the request using the target memory           */
/*    management infrastrucutre on the client side of the dynamic loader.    */
/*    The contents of the DLOAD_MEMORY_REQUEST will be updated per the       */
/*    details of a successful allocation.  The allocated page and address    */
/*    can be found in the DLOAD_MEMORY_SEGMENT attached to the request.      */
/*    The boolean return value reflects whether the allocation was           */
/*    successful or not.                                                     */
/*                                                                           */
/*---------------------------------------------------------------------------*/
BOOL     DLIF_allocate(void * client_handle, struct DLOAD_MEMORY_REQUEST *req)
{
//    printf("%s: %p, %p\n", __func__, client_handle, req);

    return 1;
}

/*---------------------------------------------------------------------------*/
/* DLIF_release()                                                            */
/*                                                                           */
/*    Given a DLOAD_MEMORY_SEGMENT description, free the target memory       */
/*    associated with the segment using the target memory management         */
/*    infrastructure on the client side of the dynamic loader.               */
/*                                                                           */
/*---------------------------------------------------------------------------*/
BOOL     DLIF_release(void* client_handle, struct DLOAD_MEMORY_SEGMENT* ptr)
{
    printf("%s: %p, %p\n", __func__, client_handle, ptr);

    return 1;
}

/*---------------------------------------------------------------------------*/
/* DLIF_write()                                                              */
/*                                                                           */
/*    Once the segment data described in the DLOAD_MEMORY_REQUEST is ready   */
/*    (relocated, if needed), write the segment contents to the target       */
/*    memory identified in the DLOAD_MEMORY_SEGMENT attached to the request. */
/*                                                                           */
/*    After the segment contents have been written to target memory, the     */
/*    core loader should discard the DLOAD_MEMORY_REQUEST object, but retain */
/*    the DLOAD_MEMORY_SEGMENT object so that the target memory associated   */
/*    with the segment can be releases when the segment is unloaded.         */
/*                                                                           */
/*---------------------------------------------------------------------------*/
BOOL     DLIF_write(void* client_handle, struct DLOAD_MEMORY_REQUEST* req)
{
//    printf("%s: %p, %p\n", __func__, client_handle, req);

    return 1;
}

/*---------------------------------------------------------------------------*/
/* DLIF_read()                                                               */
/*                                                                           */
/*    Given a host accessible buffer, read content of indicated target       */
/*    memory address into the buffer.                                        */
/*---------------------------------------------------------------------------*/
BOOL     DLIF_read(void* client_handle, void *ptr, size_t size, size_t nmemb,
                   TARGET_ADDRESS src)
{
    printf("%s: %p, %p, %d, %d, %p\n", __func__, client_handle, ptr, size, nmemb, src);

    return 1;
}

/*---------------------------------------------------------------------------*/
/* DLIF_execute()                                                            */
/*                                                                           */
/*    Start execution on the target architecture from given 'exec_addr'.     */
/*    If the dynamic loader is running on the target architecture, this can  */
/*    be effected as a simple function call.                                 */
/*                                                                           */
/*---------------------------------------------------------------------------*/
int32_t  DLIF_execute(void* client_handle, TARGET_ADDRESS exec_addr)
{
    printf("%s: %p, %p\n", __func__, client_handle, exec_addr);

    return 1;
}
