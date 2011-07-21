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
/*
 *  ======== service_mgr.c ========
 *  Simple server that handles requests to create threads (services) by name
 *  and provide an endpoint to the new thread.
 *
 */

#include <xdc/std.h>

#include <ti/grcm/RcmServer.h>

/* Max number of known service types: */
#define ServiceMgr_NUMSERVICETYPES         16

/*!
 *  @brief Service instance object handle
 */
typedef RcmServer_Handle  Service_Handle;

/*
 *  ======== ServiceMgr_init ========
 */
/*!
 *  @brief Initialize the ServiceMgr.
 *
 *  Currently, this simply initializes the RcmServer module.a
 *
 */
Void ServiceMgr_init();

/*
 *  ======== ServiceMgr_register ========
 */
/*!
 *  @brief Register a service with its static function table.
 *
 *  This function stores the RcmServerParams structure with the name key, which
 *  the ServiceMgr will find on a subsequent connect call to instantiate an
 *  RcmServer of this name.
 *
 *  @param[in] name    The name of the server that messages will be sent to for
 *  executing commands. The name must be a system-wide unique name.
 *
 *  @param[in] rcmServerParams  The RcmServer_create params used to initialize
 *                              the service instance.
 *
 *  @sa RcmServer_create
 */
Bool ServiceMgr_register(String name, RcmServer_Params  *rcmServerParams);


/*
 *  ======== ServiceMgr_send ========
 */
/*!
 *  @brief Send an asynchrounous message to a service's client endpoint.
 *
 *  This function uses the reply endpoint registered with the given service
 *  by the ServiceMgr, to send a message to the client.  It is meant to
 *  be used for asynchronous callbacks from service functions, where the
 *  callbacks do not expect a reply (hence, no ServiceMgr_recv is defined).
 *
 *  @param[in]  srvc    Handle to a service, passed into every service function.
 *  @param[in]  data    Data payload to be copied and sent. First structure
 *                      must be an omx_hdr
 *  @param[in]  len     Amount of data to be copied.
 *
 */
Void ServiceMgr_send(Service_Handle srvc, Ptr data, UInt16 len);
