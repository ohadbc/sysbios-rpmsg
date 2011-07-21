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
 *  ======== Settings.xdc ========
 *
 */


package ti.grcm;


/*!
 *  ======== Settings ========
 *  Global configuration settings for the ti.grcm package
 *
 *  This is a RTSC meta-only module. When building your executable with
 *  XDCtools, use this module to configure this package.
 *
 *  @a(See Also)
 *  @p(dlist)
 *  - {@link doxy(ti_grcm) RCM Overview}
 *  - {@link doxy(RcmClient.h) RcmClient File Reference}
 *  - {@link doxy(RcmServer.h) RcmServer File Reference}
 *  @p
 */

metaonly module Settings
{

    // -------- Module Constants --------

    // -------- Module Types --------

    /*!
     *  IPC Support enumeration type
     *
     *  The IPC support is provided by one of the following types.
     */
    enum IpcSupport {
        IpcSupport_ti_sdo_ipc,          //! DSP/BIOS IPC Support
        IpcSupport_ti_syslink_ipc       //! SysLink IPC Support
    };


    // -------- Module Parameters --------

    /*!
     *  Specifies which IPC support to link into the executable
     *
     *  This config param must be set in the application config script.
     *  It has no default value.
     */
    config IpcSupport ipc;

    /*!
     *  Controls the loading of string constants to the target
     *
     *  By default, all string constants are loaded to the target. If the
     *  program is using a logger which does not process the strings, then
     *  setting this config param to false will reduce the program's data
     *  footprint because the string constants will not be loaded.
     */
    config Bool loadStrings = true;
}
