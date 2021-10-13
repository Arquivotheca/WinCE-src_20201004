//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef __DOT11_MACROS_H__
#define __DOT11_MACROS_H__

#define DHCPV6_INCREMENT(value) \
    InterlockedIncrement(&(value))

#define DHCPV6_DECREMENT(value) \
    InterlockedDecrement(&(value))



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

#define BAIL_ON_NDIS_SUCCESS(ndisStatus)        \
    if (ndisStatus == NDIS_STATUS_SUCCESS) {    \
        goto success;                           \
    }

#define BAIL_ON_NDIS_LKERROR(ndisStatus)        \
    if (ndisStatus != NDIS_STATUS_SUCCESS) {    \
        goto lock_error;                        \
    }

#define BAIL_ON_NDIS_LKSUCCESS(ndisStatus)      \
    if (ndisStatus == NDIS_STATUS_SUCCESS) {    \
        goto lock_success;                      \
    }


#endif // __DOT11_MACROS_H__

