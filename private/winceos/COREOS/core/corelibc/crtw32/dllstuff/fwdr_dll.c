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
/***
*Fwdr_DLL.c - CRTL Forwarder DLL initialization and termination routine (Win32)
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This module contains initialization entry point for the CRTL forwarder DLL
*       in the Win32 environment.  This DLL doesn't do anything except forward its
*       imports to the newer CRTL DLL.
*
*Revision History:
*       05-13-96  SKS   Initial version.
*       11-08-06  PMB   Remove support for Win9x and _osplatform and relatives.
*                       DDB#11325
*
*******************************************************************************/

#ifdef  CRTDLL

#include <windows.h>

/*
 * The following variable is exported just so that the forwarder DLL can import it.
 * The forwarder DLL needs to have at least one import from this DLL to ensure that
 * this DLL will be fully initialized.
 */

extern __declspec(dllimport) int __dummy_export;

int __dummy_import;


/***
*BOOL _FWDR_CRTDLL_INIT(hDllHandle, dwReason, lpreserved) - C DLL initialization.
*
*Purpose:
*       This routine does the C runtime initialization.  It disables Thread
*       Attach/Detach notifications for this DLL since they are not used.
*
*Entry:
*
*Exit:
*
*******************************************************************************/

BOOL WINAPI _FWDR_CRTDLL_INIT(
        HANDLE  hDllHandle,
        DWORD   dwReason,
        LPVOID  lpreserved
        )
{
        if ( dwReason == DLL_PROCESS_ATTACH )
        {
                __dummy_import = __dummy_export;

                DisableThreadLibraryCalls(hDllHandle);
        }

        return TRUE ;
}

#endif /* CRTDLL */
