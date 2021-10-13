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
#pragma once
#ifndef __UPNP_MEM_H__INCLUDED__ 
#define __UPNP_MEM_H__INCLUDED__

#include <windows.h>

extern HANDLE   g_hHeap;


BOOL WINAPI UpnpHeapCreate();
VOID WINAPI UpnpHeapDestroy();

inline void* __cdecl UpnpAlloc(size_t size)
{
    if(g_hHeap)
    {
        return HeapAlloc(g_hHeap, 0, size);
    }
    else
    {
        return LocalAlloc(LPTR, size);
    }
}
inline void __cdecl UpnpFree(void *p)
{
    if(g_hHeap)
    {
        HeapFree(g_hHeap, 0, p);
    }
    else
    {
        LocalFree(p);
    }
}

inline void* __cdecl operator new ( size_t size )
{
    return UpnpAlloc(size);
}

inline void* __cdecl  operator new [] ( size_t size )
{
    return UpnpAlloc(size);
}


// we need these 2 versions of operator delete to keep compiler happy
// in cases where exceptions are generated in constructor
inline void __cdecl operator delete ( void *p )
{
    if ( p )
    {
        UpnpFree( p );
    }
}

inline void __cdecl operator delete [] ( void *p )
{
    if ( p )
    {
        UpnpFree( p );
    }
}

#endif // __UPNP_MEM_H__INCLUDED__

