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

