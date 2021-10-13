/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@doc LIBRARY

Copyright (c) 1999-2000 Microsoft Corporation

Module Name:
    NetInit.h

Abstract:
    Definition of a helper functions to initialize the system

Revision History:
     2-Feb-2000		Created

-------------------------------------------------------------------*/
#ifndef _NET_INIT_H_
#define _NET_INIT_H_

#include <windows.h>
#include <tchar.h>
#include "netmain.h"  // Include NetMain.h to get the flag definitions.


#ifdef __cplusplus
extern "C" {
#endif

BOOL WINAPI NetInitStartLogWrap(HANDLE h);
BOOL WINAPI NetInit(DWORD param);

#ifdef __cplusplus
}
#endif

#endif
