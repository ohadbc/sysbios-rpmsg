#
#  Copyright (c) 2011, Texas Instruments Incorporated
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#
#  *  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#
#  *  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
#  *  Neither the name of Texas Instruments Incorporated nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
#  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
#  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
#  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
#  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
#  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
#  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
#  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

CFLAGS = -Wall -m32 -I./include -DARM_TARGET -DC60_TARGET

SRC = arm_dynamic.c arm_reloc.c ArrayList.c c60_dynamic.c c60_reloc.c dload.c dload_endian.c dlw_client.c dlw_debug.c dlw_dsbt.c dlw_trgmem.c elf32.c genbase.c symtab.c
OBJ = arm_dynamic.o arm_reloc.o ArrayList.o c60_dynamic.o c60_reloc.o dload.o dload_endian.o dlw_client.o dlw_debug.o dlw_dsbt.o dlw_trgmem.o elf32.o genbase.o symtab.o
OBJ_PA = arm_dynamic.o arm_reloc.o ArrayList.o c60_dynamic.o c60_reloc.o dload.o dload_endian.o dlw_client_pa.o dlw_debug.o dlw_dsbt.o dlw_trgmem.o elf32.o genbase.o symtab.o

genbase: $(OBJ)
	gcc $(CFLAGS) -o $@ $(OBJ)

genbase_pa: $(OBJ_PA)
	gcc $(CFLAGS) -o $@ $(OBJ_PA)

dlw_client_pa.o: dlw_client.c
	cc -DTRANSLATE -Wall -I./include -DARM_TARGET -c -o $@ dlw_client.c

clean:
	@rm -f genbase genbase_pa dlw_client_pa.o $(OBJ) $(OBJ_PA)
