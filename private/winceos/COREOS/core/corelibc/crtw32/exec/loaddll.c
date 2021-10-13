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
*loaddll.c - load or free a Dynamic Link Library
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _loaddll() and _unloaddll() - load and unload DLL
*
*Revision History:
*       08-21-91  BWM   Wrote module.
*       09-30-93  GJF   Resurrected for compatibility with NT SDK.
*       02-06-98  GJF   Changes for Win64: changed return type to intptr_t.
*       02-16-04  SJ    VSW#243523 - moved the lines preventing deprecation from
*                       the header to the sources.
*       03-13-04  MSL   Added new system DLL load function for prefast
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <stdlib.h>
#include <awint.h>
#include <internal.h>
#include <errno.h>

#define _CRT_ENABLE_OBSOLETE_LOADLIBRARY_FUNCTIONS

#include <process.h>

/***
*int _loaddll(filename) - Load a dll
*
*Purpose:
*       Load a DLL into memory
*
*Entry:
*       char *filename - file to load
*
*Exit:
*       returns a unique DLL (module) handle if succeeds
*       returns 0 if fails
*
*Exceptions:
*
*******************************************************************************/

intptr_t __cdecl _loaddll(char * szName)
{
        if (__crtIsPackagedApp())
            return 0;

        return ((intptr_t)LoadLibraryExA(szName, NULL, 0));
}

/***
*int _unloaddll(handle) - Unload a dll
*
*Purpose:
*       Unloads a DLL. The resources of the DLL will be freed if no other
*       processes are using it.
*
*Entry:
*       int handle - handle from _loaddll
*
*Exit:
*       returns 0 if succeeds
*       returns DOS error if fails
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _unloaddll(intptr_t hMod)
{
        if (!FreeLibrary((HANDLE)hMod)) {
            return ((int)GetLastError());
        }
        return (0);
}
