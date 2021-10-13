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
/*
 * tuxIoctls.h
 */

#ifndef __TUX_IOCTLS_H__
#define __TUX_IOCTLS_H__

#include <windows.h>
#include <tux.h>


/*
 * This file includes TESTPROCs for testing the Ioctls. Anything 
 * needed by the tux function table must be included here. This file 
 * is included by the file with the tux function table.
 *
 * Please keep non-tux related stuff (stuff local to Ioctls) out of 
 * this file to keep from polluting the name space.
 */


/**************************** IOCTL HAL REBOOT  ***************************/

TESTPROCAPI IoctlHalRebootUsageMessage(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE);
              
TESTPROCAPI testIoctlHalReboot(
               UINT uMsg,  
               TPPARAM tpParam,
               LPFUNCTION_TABLE_ENTRY lpFTE);




#endif // __TUX_IOCTLS_H__
