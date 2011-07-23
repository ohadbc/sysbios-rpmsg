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
/* dlw_trgmem.c                                                              */
/*                                                                           */
/* This source file contains the implementation of the client side's target  */
/* memory allocator.  The RIDL version of the dynamic loader will manage     */
/* target memory from a massive data object called "trg_mem_pool".  This     */
/* memory manager is derived from the RTS version of malloc()/free().        */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/*                                                                           */
/*  This module contains the functions which implement the dynamic memory    */
/*  management routines. The following assumptions/rules apply:              */
/*                                                                           */
/*   1) Packets are allocated from host memory so they have no impact on     */
/*   the target memory heap.                                                 */
/*   2) The size of the heap is assumed to be 0x02000000, the size of the    */
/*   .blob section in the ELF dynamic loader executable file.                */
/*   3) The heap can be reset at any time by calling trg_minit()             */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/* These functions constitute the target memory manager interface with the   */
/* rest of the dynamic loader:                                               */
/*                                                                           */
/*   DLTMM_malloc() : Allocate a chunk of target memory per request.         */
/*   DLTMM_free()   : Release target memory allocated to specified address.  */
/*   DLTMM_fwrite() : Write content of target memory to dump file.           */
/*   DLTMM_fread()  : Read core file into target memory area.                */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/* These functions manage the target memory packet list and help the         */
/* interface functions carry out a target memory allocation:                 */
/*                                                                           */
/*   trg_minit()     : Initialize target memory packet list.                 */
/*   trg_align()     : Find next address within packet that is aligned.      */
/*   trg_free_pkt()  : Free a used packet and merge it with free neighbors.  */
/*   trg_alloc_pkt() : Allocate chunk of free packet and slit packet into    */
/*                     used and available pieces.                            */
/*   trg_malloc()    : Serve DLTMM_malloc(), set actual request size to      */
/*                     multiple of MIN_BLOCK and find a free packet to       */
/*                     allocate from.                                        */
/*                                                                           */
/*****************************************************************************/
#include "ArrayList.h"
#include "dload_api.h"
#include "dload4430.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "dlw_trgmem.h"

/*---------------------------------------------------------------------------*/
/* Function declarations                                                     */
/*---------------------------------------------------------------------------*/
static void       trg_minit(void* client_handle, uint32_t dyn_seg,
                            uint32_t size);
static void       trg_mdeinit(void* client_handle);
static uint32_t   trg_align(uint32_t orig_addr, int alignment);
static void       trg_free_pkt(void* client_handle, TRG_PACKET *);
static uint32_t   trg_alloc_pkt(TRG_PACKET *, size_t, int, uint32_t);
static BOOL       trg_malloc(void* client_handle, uint32_t *req_addr,
                             size_t size, int alignment);

/*****************************************************************************/
/* TRG_MINIT() - Initialize target memory management data structures.        */
/*   Set up initial free list.                                               */
/*****************************************************************************/
static void trg_minit(void* client_handle, uint32_t dyn_seg, uint32_t size)
{
    DLoad4430_Object *clientObj = (DLoad4430_Object *)client_handle;

    clientObj->trg_mem_head = (TRG_PACKET *)DLIF_malloc(sizeof(TRG_PACKET));
    if (clientObj->trg_mem_head) {
        clientObj->trg_mem_head->packet_addr = dyn_seg;
        clientObj->trg_mem_head->packet_size = size;
        clientObj->trg_mem_head->prev_packet = NULL;
        clientObj->trg_mem_head->next_packet = NULL;
        clientObj->trg_mem_head->used_packet = FALSE;
    }
}

/*****************************************************************************/
/* TRG_MDEINIT() - De-Initialize target memory management data structures.   */
/*   Free up initial free list.                                              */
/*****************************************************************************/
static void trg_mdeinit(void* client_handle)
{
    DLoad4430_Object *clientObj = (DLoad4430_Object *)client_handle;

    DLIF_free(clientObj->trg_mem_head);
}

/*****************************************************************************/
/* TRG_ALIGN() - If given origin address is aligned, return it.  Otherwise,  */
/*   find next aligned address and return it.                                */
/*****************************************************************************/
static uint32_t trg_align(uint32_t orig_addr, int alignment)
{
    if (alignment <= 1) return orig_addr;
    return ((orig_addr + (alignment - 1)) & ~(alignment - 1));
}

/*****************************************************************************/
/* TRG_FREE_PKT() - Move packet from used state to free state and merge it   */
/*   with any free neighbors on the target memory packet list.               */
/*****************************************************************************/
static void trg_free_pkt(void* client_handle, TRG_PACKET *ptr)
{
    DLoad4430_Object *clientObj = (DLoad4430_Object *)client_handle;

    if (ptr)
    {
        TRG_PACKET *prev_pkt = ptr->prev_packet;
        TRG_PACKET *next_pkt = ptr->next_packet;

        if (prev_pkt && !prev_pkt->used_packet)
        {
            ptr->packet_addr  = prev_pkt->packet_addr;
            ptr->packet_size += prev_pkt->packet_size;
            ptr->prev_packet  = prev_pkt->prev_packet;
            if (prev_pkt->prev_packet)
                prev_pkt->prev_packet->next_packet = ptr;
            DLIF_free(prev_pkt);
        }

        if (next_pkt && !next_pkt->used_packet)
        {
            ptr->packet_size += next_pkt->packet_size;
            ptr->next_packet  = next_pkt->next_packet;
            if (next_pkt->next_packet)
                next_pkt->next_packet->prev_packet = ptr;
            DLIF_free(next_pkt);
        }

        if (!ptr->prev_packet) clientObj->trg_mem_head = ptr;

        ptr->used_packet = FALSE;
    }
}

/*****************************************************************************/
/* TRG_ALLOC_PKT() - Allocate size bytes into given free packet at next      */
/*   aligned address in the packet.  Split packet into used and free         */
/*   pieces, updating the target memory list along the way.                  */
/*****************************************************************************/
static uint32_t trg_alloc_pkt(TRG_PACKET *ptr, size_t size, int alignment,
                              uint32_t req_addr)
{
    uint32_t align_addr;
    uint32_t align_pad;

    /*-----------------------------------------------------------------------*/
    /* Split given packet into used and unused pieces.                       */
    /*-----------------------------------------------------------------------*/
    TRG_PACKET *used_pkt = ptr;
    size_t      orig_sz  = 0;
    TRG_PACKET *free_pkt = NULL;

    /*-----------------------------------------------------------------------*/
    /* If the requested address is not equal to the packet address, we need  */
    /* to break the packet in two by inserting a free packet before the      */
    /* the used packet.  This assumes that the requested address has already */
    /* been verified to lie within the packet.                               */
    /*-----------------------------------------------------------------------*/
    if (req_addr > used_pkt->packet_addr)
    {
        free_pkt = (TRG_PACKET *)DLIF_malloc(sizeof(TRG_PACKET));
        if(!free_pkt) {
            return 0;
        }
        free_pkt->next_packet = used_pkt;
        free_pkt->prev_packet = used_pkt->prev_packet;
        used_pkt->prev_packet = free_pkt;

        free_pkt->packet_size = req_addr - used_pkt->packet_addr;
        free_pkt->packet_addr = used_pkt->packet_addr;
        free_pkt->used_packet = FALSE;

        used_pkt->packet_addr = req_addr;
    }

    /*-----------------------------------------------------------------------*/
    /* Compute aligned address within given free packet where we want to     */
    /* allocate.  Any alignment padding at the front will become a separate  */
    /* free packet on the target memory list.                                */
    /*-----------------------------------------------------------------------*/
    align_addr = trg_align(used_pkt->packet_addr, alignment);
    align_pad = align_addr - used_pkt->packet_addr;

    /*-----------------------------------------------------------------------*/
    /* If there is any padding at the front of the packet, then we'll build  */
    /* a new packet to represent our allocated space.                        */
    /*-----------------------------------------------------------------------*/
    if (align_pad)
    {
        /*-------------------------------------------------------------------*/
        /* If free_pkt is NULL, then we did not split ptr into two packets.  */
        /* If this is the case we need to allocate a new packet for the      */
        /* padding.  If we did split ptr into two packets, just merge the    */
        /* padding into the free packet.                                     */
        /*-------------------------------------------------------------------*/
        if (!free_pkt)
        {
            free_pkt = (TRG_PACKET *)DLIF_malloc(sizeof(TRG_PACKET));
            if(!free_pkt) {
                return 0;
            }
            free_pkt->next_packet = used_pkt;
            free_pkt->prev_packet = used_pkt->prev_packet;
            used_pkt->prev_packet = free_pkt;
            free_pkt->packet_addr = used_pkt->packet_addr;
            free_pkt->packet_size = 0;
        }

        free_pkt->packet_size += align_pad;
        used_pkt->packet_size -= align_pad;
        used_pkt->packet_addr = align_addr;

    }

    /*-----------------------------------------------------------------------*/
    /* Preserve original size of used packet so that we can compute size of  */
    /* unused space when we split the used packet.  Then set size of used    */
    /* packet.                                                               */
    /*-----------------------------------------------------------------------*/
    orig_sz = used_pkt->packet_size;
    used_pkt->packet_size = size;
    used_pkt->used_packet = TRUE;

    /*-----------------------------------------------------------------------*/
    /* If there is unused space at the end of our allocated packet, then     */
    /* we'll build up a new packet to represent this free space and at it    */
    /* into the target memory list.                                          */
    /*-----------------------------------------------------------------------*/
    if (orig_sz > size)
    {
        free_pkt = (TRG_PACKET *)DLIF_malloc(sizeof(TRG_PACKET));
        if(!free_pkt) {
            return 0;
        }
        free_pkt->next_packet = used_pkt->next_packet;
        free_pkt->prev_packet = used_pkt;
        used_pkt->next_packet = free_pkt;

        free_pkt->packet_size = orig_sz - size;

        free_pkt->packet_addr = used_pkt->packet_addr + size;

        free_pkt->used_packet = FALSE;
    }

    return (used_pkt->packet_addr);
}

/*****************************************************************************/
/* TRG_MALLOC() - Allocate target memory from the free list (which manages   */
/*   available target memory in .blob section).  The free list keeps         */
/*   track of available and used target memory.                              */
/*****************************************************************************/
static BOOL trg_malloc(void* client_handle, uint32_t *req_addr, size_t size,
                       int alignment)
{
    DLoad4430_Object *clientObj = (DLoad4430_Object *)client_handle;

    TRG_PACKET   *current  = NULL;
    TRG_PACKET   *best_fit = NULL;

    if (size <= 0) return FALSE;

    /*-----------------------------------------------------------------------*/
    /* If we did not get a request for a specific target address from the    */
    /* client, then find the best fit available packet, incorporating any    */
    /* alignment constraints imposed by the client.                          */
    /*-----------------------------------------------------------------------*/
    if (*req_addr == (uint32_t)-1)
    {
        /*-------------------------------------------------------------------*/
        /* Find best free packet on the target memory list.                  */
        /*-------------------------------------------------------------------*/
        for (current = clientObj->trg_mem_head;
             current;
             current = current->next_packet)
        {
            /*---------------------------------------------------------------*/
            /* Account for alignment constraint on current packet.           */
            /*---------------------------------------------------------------*/
            uint32_t align_addr = trg_align(current->packet_addr, alignment);
            uint32_t align_pad = align_addr - current->packet_addr;

            /*---------------------------------------------------------------*/
            /* Skip over any packets that are already allocated.             */
            /*---------------------------------------------------------------*/
            if (current->used_packet) continue;

            /*---------------------------------------------------------------*/
            /* Best fit will be smallest free packet that is >= size.        */
            /*---------------------------------------------------------------*/
            if ((current->packet_size > align_pad) &&
                ((current->packet_size - align_pad) >= size))
            {
                if (best_fit && (current->packet_size >= best_fit->packet_size))
                    continue;
                best_fit = current;
            }
        }

        if (!best_fit) return FALSE;

        *req_addr = trg_alloc_pkt(best_fit, size, alignment,
                                   best_fit->packet_addr);

        return TRUE;
    }

    /*-----------------------------------------------------------------------*/
    /* Otherwise, we do have a request for a specific target address from the*/
    /* client.  So we need to find the free packet that contains that target */
    /* address.                                                              */
    /*-----------------------------------------------------------------------*/
    else
    {
        /*-------------------------------------------------------------------*/
        /* Find the free packet that contains the requested address.         */
        /*-------------------------------------------------------------------*/
        for (current = clientObj->trg_mem_head;
             current;
             current = current->next_packet)
        {
            /*---------------------------------------------------------------*/
            /* If we have a requested address, we must make sure that the    */
            /* requested address falls on an alignment boundary, if it does  */
            /* not report an error.                                          */
            /* --------------------------------------------------------------*/
            uint32_t align_addr = trg_align(*req_addr, alignment);
            if (align_addr != *req_addr)
            {
                DLIF_error(DLET_TRGMEM, "requested address is not aligned\n");
                return FALSE;
            }

            /*---------------------------------------------------------------*/
            /* Does the requested address fall inside the packet?            */
            /*---------------------------------------------------------------*/
            if (*req_addr < current->packet_addr ||
                ((current->packet_addr + current->packet_size) <= *req_addr))
                continue;

            /*---------------------------------------------------------------*/
            /* Is the current packet big enough for the request?             */
            /*---------------------------------------------------------------*/
            if ((current->packet_size) >= size)
            {
                uint32_t alloc_addr = trg_alloc_pkt(current, size, alignment,
                                                    *req_addr);
                if (alloc_addr != *req_addr)
                {
                    DLIF_error(DLET_TRGMEM, "Problem with trg_alloc_pkt\n");
                    exit(1);
                }

                return TRUE;
            }

            break;
        }
    }

    return FALSE;
}

/*****************************************************************************/
/* DLTMM_INIT() - Externally accessible interface to initialize the target   */
/*   memory allocator.                                                       */
/*****************************************************************************/
BOOL DLTMM_init(void* client_handle, uint32_t dynMemAddr, uint32_t size)
{
    DLoad4430_Object *clientObj = (DLoad4430_Object *)client_handle;

    if (dynMemAddr == 0)
        return FALSE;

    if (!clientObj->trg_minit)
    {
        trg_minit(client_handle, dynMemAddr, size);
        clientObj->trg_minit = TRUE;
    }

    return TRUE;
}

/*****************************************************************************/
/* DLTMM_DEINIT() - Externally accessible interface to de-initialize the     */
/*   target memory allocator.                                                */
/*****************************************************************************/
BOOL DLTMM_deinit(void* client_handle)
{
    DLoad4430_Object *clientObj = (DLoad4430_Object *)client_handle;

    if (clientObj->trg_minit)
    {
        trg_mdeinit(client_handle);
        clientObj->trg_minit = FALSE;
    }

    return TRUE;
}

/*****************************************************************************/
/* DLTMM_MALLOC() - Externally accessible interface into target memory       */
/*   allocator.  This function will use the core target memory manager       */
/*   to find available space per requested and update target memory list     */
/*   to reflect new status of used and free target memory.                   */
/*****************************************************************************/
BOOL DLTMM_malloc(void* client_handle,
                  struct DLOAD_MEMORY_REQUEST *targ_req,
                  struct DLOAD_MEMORY_SEGMENT *obj_desc)
{
    /*-----------------------------------------------------------------------*/
    /* Set up alignment constraint and request for specific address, if      */
    /* there is one.                                                         */
    /*-----------------------------------------------------------------------*/
    int      alignment = (targ_req->align) ? targ_req->align : 1;
    uint32_t req_addr  = (targ_req->flags & DLOAD_SF_relocatable) ?
        (uint32_t)-1 : (uint32_t)obj_desc->target_address;

    /*-----------------------------------------------------------------------*/
    /* Ask for free space from the target memory allocator.                  */
    /*-----------------------------------------------------------------------*/
    if (trg_malloc(client_handle,
                   &req_addr,
                   obj_desc->memsz_in_bytes,
                   alignment))
    {
        obj_desc->target_address = targ_req->host_address = (void *)req_addr;
        return TRUE;
    }

    /*-----------------------------------------------------------------------*/
    /* Something went wrong.  Target memory allocation failed.               */
    /*-----------------------------------------------------------------------*/
    return FALSE;
}

/*****************************************************************************/
/* DLTMM_FREE() - Find packet in target memory list associated with given    */
/*   target address and change its state from used to free.               */
/*****************************************************************************/
void DLTMM_free(void* client_handle, TARGET_ADDRESS ptr)
{
    DLoad4430_Object *clientObj = (DLoad4430_Object *)client_handle;

    uint32_t      pkt_addr = (uint32_t)ptr;
    TRG_PACKET   *prev     = NULL;
    TRG_PACKET   *current  = NULL;

    /*-----------------------------------------------------------------------*/
    /* Find free packet on the target memory list that contains the specified*/
    /* address that we are trying to free.                                   */
    /*-----------------------------------------------------------------------*/
    for (current = clientObj->trg_mem_head;
         current;
         current = current->next_packet)
    {
        /*-------------------------------------------------------------------*/
        /* Skip over any packets that are already free.                      */
        /*-------------------------------------------------------------------*/
        if (!current->used_packet) continue;

        /*-------------------------------------------------------------------*/
        /* Find used packet associated with given address.                   */
        /*-------------------------------------------------------------------*/
        if (current->packet_addr <= pkt_addr) prev = current;
        else break;
    }

    if (prev) trg_free_pkt(client_handle, prev);

    else
    {
        DLIF_error(DLET_TRGMEM,
                   "Did not find free packet associated with given target "
                   "address, 0x%lx\n", pkt_addr);
        exit(1);
    }
}

/*****************************************************************************/
/* DLTMM_FWRITE_TRG_MEM() - Write content of target memory area to a file.   */
/*****************************************************************************/
void DLTMM_fwrite_trg_mem(FILE *fp)
{
    /*if (fp) fwrite(trg_mem_pool, trg_mem_pool_sz, 1, fp);*/
}

/*****************************************************************************/
/* DLTMM_FREAD_TRG_MEM() - Read file content into target memory area.        */
/*****************************************************************************/
void DLTMM_fread_trg_mem(FILE *fp)
{
    /*if (fp) fread(trg_mem_pool, trg_mem_pool_sz, 1, fp);*/
}

/*****************************************************************************/
/* DLTMM_DUMP_TRG_MEM() - Dump the contents of target memory into a file.    */
/*****************************************************************************/
void DLTMM_dump_trg_mem(uint32_t offset, uint32_t nbytes,
                        FILE* fp)
{
#if 0
    uint8_t* ptr = trg_mem_pool + offset;

    if (!fp)
    {
        DLIF_error(DLET_TRGMEM, "NULL file pointer given to dump_trgmem\n");
        return;
    }

    /*-----------------------------------------------------------------------*/
    /* Make sure the request is in range.                                    */
    /*-----------------------------------------------------------------------*/
    if ((ptr + nbytes) > (trg_mem_pool + trg_mem_pool_sz ))
    {
        DLIF_error(DLET_TRGMEM, "Invalid range given to dump_trgmem\n");
        return;
    }

    fwrite(ptr, 1, nbytes, fp);
#endif
}
