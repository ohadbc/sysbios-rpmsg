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
/** ============================================================================
 *  @file       IpcPower.h
 *  ============================================================================
 */

#ifndef ti_pm_IpcPower__include
#define ti_pm_IpcPower__include

#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Structures & Definitions
 * =============================================================================
 */

/*!
 *  @def    IpcPower_S_SUCCESS
 *  @brief  Operation is successful.
 */
#define IpcPower_S_SUCCESS               0

/*!
 *  @def    IpcPower_E_FAIL
 *  @brief  Operation is not successful.
 */
#define IpcPower_E_FAIL                  -1

/*!
 *  @def    IpcPower_E_MEMORY
 *  @brief  Operation resulted in memory failure.
 */
#define IpcPower_E_MEMORY                -2


/* =============================================================================
 *  IpcPower Functions:
 * =============================================================================
 */

/*!
 *  @brief      Initialize the IpcPower module
 *
 *  @sa         IpcPower_exit
 */
Void IpcPower_init();

/*!
 *  @brief      Finalize the IpcPower module
 *
 *  @sa         IpcPower_init
 */
Void IpcPower_exit();

/*!
 *  @brief      Initiate the suspend procedure
 */
Void IpcPower_suspend();


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* ti_ipc_IpcPower__include */
