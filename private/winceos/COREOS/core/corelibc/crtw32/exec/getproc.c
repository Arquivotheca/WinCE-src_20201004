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
*getproc.c - Get the address of a procedure in a DLL.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _getdllprocadd() - gets a procedure address by name or
*       ordinal
*
*Revision History:
*       08-21-91  BWM   Wrote module.
*       09-30-93  GJF   Resurrected for compatiblity with NT SDK.
*       02-06-98  GJF   Changes for Win64: changed return type to intptr_t.
*       02-10-98  GJF   Changes for Win64: changed 3rd arg type intptr_t.
*       10-25-02  SJ    Fixed /clr /W4 Warnings VSWhidbey-2445
*       02-16-04  SJ    VSW#243523 - moved the lines preventing deprecation from
*                       the header to the sources.
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>

#define _CRT_ENABLE_OBSOLETE_LOADLIBRARY_FUNCTIONS

#include <process.h>

/***
*int (*)() _getdllprocaddr(handle, name, ordinal) - Get the address of a
*       DLL procedure specified by name or ordinal
*
*Purpose:
*
*Entry:
*       int handle - a DLL handle from _loaddll
*       char * name - Name of the procedure, or NULL to get by ordinal
*       int ordinal - Ordinal of the procedure, or -1 to get by name
*
*
*Exit:
*       returns a pointer to the procedure if found
*       returns NULL if not found
*
*Exceptions:
*
*******************************************************************************/

int (__cdecl * __cdecl _getdllprocaddr(
        intptr_t hMod,
        char * szProcName,
        intptr_t iOrdinal))(void)
{
        typedef int (__cdecl * PFN)(void);

        if (szProcName == NULL) {
            if (iOrdinal <= 65535) {
#ifdef _WIN32_WCE
                return ((PFN)GetProcAddressA((HANDLE)hMod, (LPSTR)iOrdinal));
#else
                return ((PFN)GetProcAddress((HANDLE)hMod, (LPSTR)iOrdinal));
#endif // _WIN32_WCE
            }
        }
        else {
            if (iOrdinal == (intptr_t)(-1)) {
#ifdef _WIN32_WCE
                return ((PFN)GetProcAddressA((HANDLE)hMod, szProcName));
#else
                return ((PFN)GetProcAddress((HANDLE)hMod, szProcName));
#endif // _WIN32_WCE
            }
        }

        return (NULL);

}
