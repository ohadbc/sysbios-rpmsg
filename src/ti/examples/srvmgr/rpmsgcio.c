/*
 * Copyright (c) 2011, Texas Instruments Incorporated
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

#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/Diags.h>

#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/ipc/MultiProc.h>

#include <stdio.h>
#include <file.h>
#include <stdlib.h>
#include <string.h>

#include "../../ipc/rpmsg/VirtQueue.h"

#define EARLYBUF_SZ 4096

struct RPCIO_Transport  {
	Swi_Handle       swiHandle;
	VirtQueue_Handle virtQueue_toHost;
	VirtQueue_Handle virtQueue_fromHost;
};

static int started = 0;

static char earlybuf[EARLYBUF_SZ];
static int earlyindex;

static struct RPCIO_Transport transport;

/* it's just a stub for now, that assumes only one file will be opened */
int RPCIO_open(const char *path, unsigned flags, int llv_fd)
{
	return 0;
}

int RPCIO_close(int dev_fd)
{
	return 0;
}

int RPCIO_read(int dev_fd, char *buf, unsigned count)
{
	return 0;
}

int RPCIO_write(int dev_fd, const char *buf, unsigned count)
{
	Int16             token = 0;
	char *vbuf;
	int vbuflen;

	/* Host's vring isn't set up yet ? */
	if (!started) {
		int left = EARLYBUF_SZ - earlyindex;
		count = left < count ? left : count;
		memcpy(earlybuf + earlyindex, buf, count);
		earlyindex += count;
		goto out;
	}

	/* FIXME: protect vrings */
	token = VirtQueue_getAvailBuf(transport.virtQueue_toHost,
			(void **)&vbuf, &vbuflen);
	if (token < 0) {
		Log_print0(Diags_STATUS, "RPCIO: getAvailBuf failed!");
		goto out;
	}

	vbuflen = vbuflen < count ? vbuflen : count;

	/* Copy the payload and set message header: */
	memcpy(vbuf, buf, vbuflen);

	VirtQueue_addUsedBuf(transport.virtQueue_toHost, token, vbuflen);
	VirtQueue_kick(transport.virtQueue_toHost);

out:
	return count;
}

fpos_t RPCIO_lseek(int dev_fd, fpos_t offset, int origin)
{
	return 0;
}

int RPCIO_unlink(const char *path)
{
	return 0;
}

int RPCIO_rename(const char *old_name, const char *new_name)
{
	return 0;
}

static Void callback_availBufReady(VirtQueue_Handle vq)
{
	if (vq == transport.virtQueue_fromHost)  {
		/* Post a SWI to process all incoming messages */
		Log_print0(Diags_INFO, "RPCIO: virtQueue_fromHost kicked");
		Swi_post(transport.swiHandle);
	}
	else if (vq == transport.virtQueue_toHost) {
		/*
		 * Note: We post nothing for transport.virtQueue_toHost,
		 * as we assume the
		 * host has already made all buffers available for sending.
		 */
		Log_print0(Diags_INFO, "RPCIO: virtQueue_toHost kicked");
		started = 1;
	}
}

#define FXNN "RPCIO_swiFxn"
static Void RPCIO_swiFxn(UArg arg0, UArg arg1)
{
	Int16             token;
	char *msg;
	Bool              usedBufAdded = FALSE;
	int len;
	int msglen;
	int dump = 0;

	Log_print0(Diags_ENTRY, "--> "FXNN);

	/* Process all available buffers: */
	while ((token = VirtQueue_getAvailBuf(transport.virtQueue_fromHost,
					(Void **)&msg, &len)) >= 0) {
		if (len >= 4 && !memcmp(msg, "dump", 4))
			dump = 1;

		VirtQueue_addUsedBuf(transport.virtQueue_fromHost, token, len);
		usedBufAdded = TRUE;
	}

	if (usedBufAdded)  {
		/* Tell host we've processed the buffers: */
		VirtQueue_kick(transport.virtQueue_fromHost);
	}

	if (dump && earlyindex) {
		dump = 0;
		token = VirtQueue_getAvailBuf(transport.virtQueue_toHost,
				(Void **)&msg, &len);

		msglen = len < earlyindex ? len : earlyindex;
		memcpy(msg, earlybuf, msglen);

		VirtQueue_addUsedBuf(transport.virtQueue_toHost, token, msglen);
		earlyindex = 0;
		/* Tell host we've processed the buffers: */
		VirtQueue_kick(transport.virtQueue_fromHost);
	}

	Log_print0(Diags_EXIT, "<-- "FXNN);
}
#undef FXNN

void initRpmsgCio()
{
	UInt16 remoteProcId = MultiProc_getId("HOST");

	transport.swiHandle = Swi_create(RPCIO_swiFxn, NULL, NULL);
	if (transport.swiHandle == NULL) {
		System_printf("Swi_create failed\n");
		return;
	}

	/*
	 * Create a pair VirtQueues (one for sending, one for receiving).
	 */
	transport.virtQueue_toHost   = VirtQueue_create(callback_availBufReady,
			remoteProcId,
			CONSOLE_SELF_TO_A9);
	if (transport.virtQueue_toHost == NULL) {
		System_printf("VirtQueue_create failed\n");
		return;
	}

	transport.virtQueue_fromHost = VirtQueue_create(callback_availBufReady,
			remoteProcId,
			CONSOLE_A9_TO_SELF);
	if (transport.virtQueue_fromHost == NULL) {
		System_printf("VirtQueue_create failed\n");
		return;
	}

	add_device("rpcio", _MSA, RPCIO_open, RPCIO_close, RPCIO_read,
			RPCIO_write, RPCIO_lseek, RPCIO_unlink, RPCIO_rename);
	freopen("rpcio:2", "w", stdout);
	freopen("rpcio:2", "r", stdin);
	setvbuf(stdout, NULL, _IOLBF, 128);
	setvbuf(stdin, NULL, _IONBF, 80);

	System_printf("Hello from SYS/BIOS\n");
	printf("M3 Core0 init...\n");
	printf("Hello from SYS/BIOS\n");
}
