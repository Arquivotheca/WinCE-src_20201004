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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*++



Module Name:

    memory.c

Abstract:

    This module provides all the memory management functions for
    all DHCPv6 components.

Author:

    FrancisD

Revision History:

--*/

#include "dhcpv6p.h"
//#include "precomp.h"


LPVOID
AllocDHCPV6Mem(
    DWORD cb
    )
{
    LPVOID pMem = NULL;

    pMem = LocalAlloc(LPTR, cb);

    return (pMem);
}


BOOL
FreeDHCPV6Mem(
    LPVOID pMem
    )
{
    return(LocalFree(pMem) == NULL);
}


LPVOID
ReallocDHCPV6Mem(
    LPVOID pOldMem,
    DWORD cbOld,
    DWORD cbNew
    )
{
    LPVOID pNewMem;

    pNewMem=AllocDHCPV6Mem(cbNew);

    if (pOldMem && pNewMem) {
        memcpy(pNewMem, pOldMem, min(cbNew, cbOld));
        FreeDHCPV6Mem(pOldMem);
    }

    return pNewMem;
}


LPWSTR
AllocDHCPV6Str(
    LPWSTR pStr
    )
{
   LPWSTR pMem;

   if (!pStr)
      return 0;

   if (pMem = (LPWSTR)AllocDHCPV6Mem( wcslen(pStr)*sizeof(WCHAR) + sizeof(WCHAR) ))
      wcscpy(pMem, pStr);

   return pMem;
}


BOOL
FreeDHCPV6Str(
    LPWSTR pStr
    )
{
   return pStr ? FreeDHCPV6Mem(pStr)
               : FALSE;
}


BOOL
ReallocDHCPV6Str(
    LPWSTR *ppStr,
    LPWSTR pStr
    )
{
   FreeDHCPV6Str(*ppStr);
   *ppStr=AllocDHCPV6Str(pStr);

   return TRUE;
}


DWORD
AllocateDHCPV6Memory(
    DWORD cb,
    LPVOID * ppMem
    )
{
    DWORD dwError = 0;

    LPBYTE pMem = NULL;

    pMem = AllocDHCPV6Mem(cb);

    if (!pMem) {
        dwError = GetLastError();
    }

    *ppMem = pMem;

    return(dwError);
}


VOID
FreeDHCPV6Memory(
    LPVOID pMem
    )
{
    if (pMem) {
        FreeDHCPV6Mem(pMem);
    }

    return;
}


DWORD
AllocateDHCPV6String(
    LPWSTR pszString,
    LPWSTR * ppszNewString
    )
{
    LPWSTR pszNewString = NULL;
    DWORD dwError = 0;

    pszNewString = AllocDHCPV6Str(pszString);

    if (!pszNewString) {
        dwError = GetLastError();
    }

    *ppszNewString = pszNewString;

    return(dwError);
}


VOID
FreeDHCPV6String(
    LPWSTR pszString
    )
{
    if (pszString) {
        FreeDHCPV6Str(pszString);
    }

    return;
}

