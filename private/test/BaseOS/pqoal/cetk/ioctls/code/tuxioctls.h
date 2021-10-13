//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//


#ifndef __TUX_IOCTLS_H__
#define __TUX_IOCTLS_H__

#include <windows.h>
#include <tux.h>

/* common utils */
#include "..\..\..\common\commonUtils.h"


/*
 * This file includes TESTPROCs for testing the Ioctls. Anything 
 * needed by the tux function table must be included here. This file 
 * is included by the file with the tux function table.
 *
 * Please keep non-tux related stuff (stuff local to Ioctls) out of 
 * this file to keep from polluting the name space.
 */


/**************************************************************************
 * 
 *                           Ioctl Test Functions
 *
 **************************************************************************/
 
/****************************** Usage Message *****************************/
TESTPROCAPI IoctlsTestUsageMessage(
                UINT uMsg,
                TPPARAM tpParam,
            LPFUNCTION_TABLE_ENTRY lpFTE);


/************************** Device Info IOCTL ******************************/
TESTPROCAPI testIoctlDevInfoOutputValue(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI testIoctlDevInfoInParam(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI testIoctlDevInfoOutParam(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI testIoctlDevInfoInBufferAlignment(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI testIoctlDevInfoOutBufferAlignment(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);

 

/************************ PROCESSOR INFORMATION IOCTL **********************/
TESTPROCAPI testIoctlProcInfoOutputValue(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);

 TESTPROCAPI testIoctlProcInfoInParam(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);

 TESTPROCAPI testIoctlProcInfoOutParam(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);

 TESTPROCAPI testIoctlProcInfoOutBufferAlignment(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);



/****************************** ILTIMING IOCTL *****************************/

TESTPROCAPI IltimingIoctlUsageMessage(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);
              
TESTPROCAPI testIfIltimingIoctlSupported(
               UINT uMsg,  
               TPPARAM tpParam,
               LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI testIltimingEnableDisable(
               UINT uMsg,  
               TPPARAM tpParam,
               LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI testIltimingIoctlInParam(
                     UINT uMsg,  
                     TPPARAM tpParam,
                     LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI testIltimingIoctlOutParam(
                     UINT uMsg,  
                     TPPARAM tpParam,
                     LPFUNCTION_TABLE_ENTRY lpFTE);


/************************ IOCTL_HAL_GET_RANDOM_SEED **********************/

TESTPROCAPI testRandomSeedIoctlSupported(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI testRandomSeedIoctlInParam(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI testRandomSeedIoctlOutParam(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);


/************************ IOCTL_HAL_GET_WAKE_SOURCE **********************/

TESTPROCAPI testGetWakeSource(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI testGetWakeSourceInParam(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI testGetWakeSourceOutParam(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);


/************************ IOCTL_HAL_GET_POWER_DISPOSITION **********************/

TESTPROCAPI testGetPowerDisposition(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI testGetPowerDispositionInParam(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI testGetPowerDispositionOutParam(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);

#endif // __TUX_IOCTLS_H__
