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
#ifndef _UDFSHELPER_H_
#define _UDFSHELPER_H_

BOOL UDFSMatchesWildcard(DWORD len, LPWSTR lpWild, DWORD len2, LPSTR lpFile, DWORD dwCharSize);

extern HANDLE g_hHeap;

inline LPVOID UDFSAlloc(HANDLE hHeap, DWORD dwSize)
{
    LPVOID ptr = HeapAlloc( hHeap, HEAP_ZERO_MEMORY, dwSize);
    DEBUGMSG( ZONE_ALLOC, (TEXT("UDFSAlloc size = %ld Returning ptr = %08X\r\n"), dwSize, ptr));
    return ptr;
}

inline LPVOID UDFSReAlloc(HANDLE hHeap, LPVOID ptr, DWORD dwSize)
{
    LPVOID newptr = HeapReAlloc( hHeap, HEAP_ZERO_MEMORY, ptr, dwSize); // Realloc should only zero out the additional bytes
    DEBUGMSG( ZONE_ALLOC, (TEXT("UDFSReAlloc ptr = %08X dwSize = %ld Returning ptr=%08X\r\n"), ptr, dwSize, newptr));
    return newptr;
}

inline BOOL UDFSFree(HANDLE hHeap, LPVOID ptr)
{
    DEBUGMSG(ZONE_ALLOC, (TEXT("UDFSFree ptr = %08X\r\n"), ptr));
    return HeapFree( hHeap, 0, ptr);
}


#endif //_UDFSHELPER_H_
