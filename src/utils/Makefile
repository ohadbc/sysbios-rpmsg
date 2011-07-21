#
# Copyright (c) 2011, Texas Instruments Incorporated
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# *  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# *  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# *  Neither the name of Texas Instruments Incorporated nor the names of
#    its contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

all: readrprc wrints genbase ducati-m3.bin

ducati-m3.bin: ../ti/examples/srvmgr/ti_platform_omap4430_core0/debug/test_omx_sysm3.xem3 ../ti/examples/srvmgr/ti_platform_omap4430_core1/debug/test_omx_appm3.xem3 wrints genbase
	#
	# ensure that ducati's core0 program is listed before ducati's core1
	# program, the Linux FW loader expects such.
	#
	./genrprc --t0=SysMin_Module_State_0_outbuf__A ../ti/examples/srvmgr/ti_platform_omap4430_core0/debug/test_omx_sysm3.xem3  --t1=SysMin_Module_State_0_outbuf__A ../ti/examples/srvmgr/ti_platform_omap4430_core1/debug/test_omx_appm3.xem3 $@

CFLAGS = -Wall -m32
RPRCOBJ = readrprc.o
WRNTOBJ = wrints.o

readrprc: $(RPRCOBJ)
	gcc $(CFLAGS) -o $@ $(RPRCOBJ)

readrprc.o: readrprc.c
	gcc $(CFLAGS) -c -o $@ $<

wrints: $(WRNTOBJ)
	gcc $(CFLAGS) -o $@ $(WRNTOBJ)

wrints.o: wrints.c
	gcc $(CFLAGS) -c -o $@ $<

genbase: elfload/genbase
	cp $^ .

elfload/genbase:
	cd elfload; make

clean:
	@rm -f genbase readrprc wrints *.o ducati-m3.bin
	cd elfload; make clean
