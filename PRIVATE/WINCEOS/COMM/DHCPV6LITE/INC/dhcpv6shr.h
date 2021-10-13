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

    dhcpv6shr.h

Abstract:

    Header file for DhcpV6 shared library.



    FrancisD

Environment:

    User Level: Windows / Kernel

Revision History:


--*/


#ifndef _DHCPV6SHR_
#define _DHCPV6SHR_

//
// BAIL Macros.
//

#define BAIL_ON_NTSTATUS_ERROR(ntStatus) \
    if (!NT_SUCCESS(ntStatus)) {         \
        goto error;                      \
    }

#define BAIL_ON_NTSTATUS_SUCCESS(ntStatus) \
    if (NT_SUCCESS(ntStatus)) {            \
        goto success;                      \
    }

#define BAIL_ON_NTSTATUS_ILERROR(ntStatus) \
    if (!NT_SUCCESS(ntStatus)) {           \
        goto il_error;                     \
    }

#define BAIL_ON_NTSTATUS_LKERROR(ntStatus) \
    if (!NT_SUCCESS(ntStatus)) {           \
        goto lock_error;                   \
    }


#define BAIL_ON_NTSTATUS_LKSUCCESS(ntStatus) \
    if (NT_SUCCESS(ntStatus)) {              \
        goto lock_success;                   \
    }

#define BAIL_ON_NDIS_ERROR(ndisStatus)          \
    if (ndisStatus != NDIS_STATUS_SUCCESS) {    \
        goto error;                             \
    }

#define BAIL_ON_NDIS_LKERROR(ndisStatus)        \
    if (ndisStatus != NDIS_STATUS_SUCCESS) {    \
        goto lock_error;                        \
    }

#define BAIL_ON_NDIS_LKSUCCESS(ndisStatus)      \
    if (ndisStatus == NDIS_STATUS_SUCCESS) {    \
        goto lock_success;                      \
    }


//
// Maximum and Minimum Macros.
//

#define SAP_MAX(x,y) ((x) < (y)) ? (y) : (x)

#define SAP_MIN(x,y) ((x) < (y)) ? (x) : (y)

#ifndef MIN
#define MIN SAP_MIN
#endif // MIN

#ifndef MAX
#define MAX SAP_MAX
#endif // MAX


#ifdef __cplusplus
extern "C" {
#endif


DWORD
ValidateTemplateDhcpV6Interface(
    PDHCPV6_INTERFACE pDhcpV6Interface
    );

DWORD
ValidateDhcpV6Interface(
    PDHCPV6_INTERFACE pDhcpV6Interface
    );



typedef struct _DHCPV6_RW_LOCK {
    CRITICAL_SECTION csExclusive;
    BOOL bInitExclusive;
    CRITICAL_SECTION csShared;
    BOOL bInitShared;
    LONG lReaders;
    HANDLE hReadDone;
    DWORD dwCurExclusiveOwnerThreadId;
} DHCPV6_RW_LOCK, * PDHCPV6_RW_LOCK;


DWORD
InitializeRWLock(
    PDHCPV6_RW_LOCK pDhcpV6RWLock
    );

VOID
DestroyRWLock(
    PDHCPV6_RW_LOCK pDhcpV6RWLock
    );

VOID
AcquireSharedLock(
    PDHCPV6_RW_LOCK pDhcpV6RWLock
    );

VOID
AcquireExclusiveLock(
    PDHCPV6_RW_LOCK pDhcpV6RWLock
    );

VOID
ReleaseSharedLock(
    PDHCPV6_RW_LOCK pDhcpV6RWLock
    );

VOID
ReleaseExclusiveLock(
    PDHCPV6_RW_LOCK pDhcpV6RWLock
    );


LPVOID
AllocDHCPV6Mem(
    DWORD cb
    );

BOOL
FreeDHCPV6Mem(
    LPVOID pMem
    );

LPVOID
ReallocDHCPV6Mem(
    LPVOID pOldMem,
    DWORD cbOld,
    DWORD cbNew
    );

LPWSTR
AllocDHCPV6Str(
    LPWSTR pStr
    );

BOOL
FreeDHCPV6Str(
    LPWSTR pStr
    );

BOOL
ReallocDHCPV6Str(
    LPWSTR *ppStr,
    LPWSTR pStr
    );

DWORD
AllocateDHCPV6Memory(
    DWORD cb,
    LPVOID * ppMem
    );

VOID
FreeDHCPV6Memory(
    LPVOID pMem
    );

DWORD
AllocateDHCPV6String(
    LPWSTR pszString,
    LPWSTR * ppszNewString
    );

VOID
FreeDHCPV6String(
    LPWSTR pszString
    );


#ifdef __cplusplus
}
#endif


#endif // _DHCPV6SHR_

