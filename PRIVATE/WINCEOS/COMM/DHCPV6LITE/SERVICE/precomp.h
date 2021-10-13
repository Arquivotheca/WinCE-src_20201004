//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++



Module Name:

    precomp.h

Abstract:

    Precompiled header for wlclient.dll.



    FrancisD

Environment:

    User Level: Windows

Revision History:


--*/


#ifdef __cplusplus
extern "C" {
#endif

#ifdef UNDER_CE
#include <wce.h>
#include <windows.h>
#include <types.h>
#include <memory.h>
#include <wdm.h>
#else
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <stddef.h>
#include <ntddrdr.h>
#endif

#ifdef __cplusplus
}
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rpc.h>
#include <windows.h>
//#include <imagehlp.h>
#include <tchar.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <dsgetdc.h>

#ifdef __cplusplus
}
#endif

#include "winioctl.h"
#include <winsock2.h>
#include "winsock.h"
#include <ws2tcpip.h>
#include <mswsock.h>
//#include <userenv.h>
#include <wchar.h>
#include <winldap.h>
#include "ipexport.h"
#include <iphlpapi.h>
#include <ndis.h>
#include <tdikrnl.h>
//#include <nhapi.h>
//#include <seopaque.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NT
#include <msaudite.h>
#include <ntlsa.h>
#include <lsarpc.h>
#include <ntsam.h>
#include <lsaisrv.h>
#endif

#ifdef __cplusplus
}
#endif

/*
#include <esent.h>
#include <aclapi.h>
#include <process.h>
#include <dbt.h>
#include <ndisguid.h>
*/

#ifdef UNDER_CE
#include <wcetimer.h>
#endif

//#include "dhcpv6_s.h"
#include "dhcpv6l.h"
#include "dhcpv6.h"
#include "dhcpv6shr.h"
#include "dhcpv6hdr.h"
#include "dhcpv6def.h"
//#include "debug.h"
#include "dhcpv6dbg.h"

#include "structs.h"
#include "apiutils.h"
#ifndef UNDER_CE
#include "loopmgr.h"
#include "rpcserv.h"
#endif
#include "dhcpv6svc.h"
#include "security.h"
#include "init.h"

#include "eventmgr.h"
#include "optionmgr.h"
#include "adapters.h"
#include "messagemgr.h"
#include "timer.h"
#include "replymgr.h"
#include "apis.h"

#include "rand.h"
#include "wmi.h"

#include "macros.h"
#include "externs.h"

#include "mngprfx.h"

#ifdef BAIL_ON_WIN32_ERROR
#undef BAIL_ON_WIN32_ERROR
#endif

#ifdef BAIL_ON_LOCK_ERROR
#undef BAIL_ON_LOCK_ERROR
#endif


#define BAIL_ON_WIN32_ERROR(dwError)                \
    if (dwError) {                                  \
        goto error;                                 \
    }

#define BAIL_ON_LOCK_ERROR(dwError)                 \
    if (dwError) {                                  \
        goto lock;                                  \
    }

#define BAIL_ON_WIN32_SUCCESS(dwError) \
    if (!dwError) {                    \
        goto success;                  \
    }

#define BAIL_ON_LOCK_SUCCESS(dwError)  \
    if (!dwError) {                    \
        goto lock_success;             \
    }

#define ENTER_DHCPV6_SECTION()             \
    EnterCriticalSection(&gcDHCPV6Section) \

#define LEAVE_DHCPV6_SECTION()             \
    LeaveCriticalSection(&gcDHCPV6Section) \

