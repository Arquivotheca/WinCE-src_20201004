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
/*++


Module Name:
    mqformat.h

Abstract:
    Convert a QUEUE_FORMAT struc to FORMAT_NAME string

--*/

#ifndef __MQFORMAT_H
#define __MQFORMAT_H

#include <fntoken.h>

#if !defined(NTSTATUS) && !defined(_NTDEF_)
#define NTSTATUS HRESULT
#endif


inline
ULONG
MQpPublicToFormatName(
    const QUEUE_FORMAT* pqf,
    LPCWSTR pSuffix,
    ULONG buff_size,
    LPWSTR pfn
    )
{
    ASSERT(pqf->GetType() == QUEUE_FORMAT_TYPE_PUBLIC);

    const GUID* pg = &pqf->PublicID();
    _snwprintf(
        pfn,
        buff_size,
        FN_PUBLIC_TOKEN     // "PUBLIC"
            FN_EQUAL_SIGN   // "="
            GUID_FORMAT     // "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
            SUFFIX_FORMAT,  // "%s"
        GUID_ELEMENTS(pg),
        pSuffix
        );

    //
    //  return format name buffer length *not* including suffix length
    //  "PUBLIC=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\0"
    //
    return (
        FN_PUBLIC_TOKEN_LEN + 1 +
        GUID_STR_LENGTH +  1
        );
}


inline
ULONG
MQpPrivateToFormatName(
    const QUEUE_FORMAT* pqf,
    LPCWSTR pSuffix,
    ULONG buff_size,
    LPWSTR pfn
    )
{
    ASSERT(pqf->GetType() == QUEUE_FORMAT_TYPE_PRIVATE);

    const GUID* pg = &pqf->PrivateID().Lineage;
    _snwprintf(
        pfn,
        buff_size,
        FN_PRIVATE_TOKEN            // "PRIVATE"
            FN_EQUAL_SIGN           // "="
            GUID_FORMAT             // "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
            FN_PRIVATE_SEPERATOR    // "\\"
            PRIVATE_ID_FORMAT      // "xxxxxxxx"
            SUFFIX_FORMAT,          // "%s"
        GUID_ELEMENTS(pg),
        pqf->PrivateID().Uniquifier,
        pSuffix
        );

    //
    //  return format name buffer length *not* including suffix length
    //  "PRIVATE=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\\xxxxxxxx\0"
    //
    return (
        FN_PRIVATE_TOKEN_LEN + 1 +
        GUID_STR_LENGTH + 1 + 8 + 1
        );
}


inline
ULONG
MQpDirectToFormatName(
    const QUEUE_FORMAT* pqf,
    LPCWSTR pSuffix,
    ULONG buff_size,
    LPWSTR pfn
    )
{
    ASSERT(pqf->GetType() == QUEUE_FORMAT_TYPE_DIRECT);

    _snwprintf(
        pfn,
        buff_size,
        FN_DIRECT_TOKEN     // "DIRECT"
            FN_EQUAL_SIGN   // "="
            L"%s"           // "OS:bla-bla"
            SUFFIX_FORMAT,  // "%s"
        pqf->DirectID(),
        pSuffix
        );

    //
    //  return format name buffer length *not* including suffix length
    //  "DIRECT=OS:bla-bla\0"
    //
    return (
        FN_DIRECT_TOKEN_LEN + 1 +
        wcslen(pqf->DirectID()) + 1
        );
}


inline
ULONG
MQpMachineToFormatName(
    const QUEUE_FORMAT* pqf,
    LPCWSTR pSuffix,
    ULONG buff_size,
    LPWSTR pfn
    )
{
    ASSERT(pqf->GetType() == QUEUE_FORMAT_TYPE_MACHINE);

    const GUID* pg = &pqf->MachineID();
    _snwprintf(
        pfn,
        buff_size,
        FN_MACHINE_TOKEN    // "MACHINE"
            FN_EQUAL_SIGN   // "="
            GUID_FORMAT     // "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
            SUFFIX_FORMAT,  // "%s"
        GUID_ELEMENTS(pg),
        pSuffix
        );

    //
    //  return format name buffer length *not* including suffix length
    //  "MACHINE=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\0"
    //
    return (
        FN_MACHINE_TOKEN_LEN + 1 +
        GUID_STR_LENGTH + 1
        );
}


inline
ULONG
MQpConnectorToFormatName(
    const QUEUE_FORMAT* pqf,
    LPCWSTR pSuffix,
    ULONG buff_size,
    LPWSTR pfn
    )
{
    ASSERT(pqf->GetType() == QUEUE_FORMAT_TYPE_CONNECTOR);

    const GUID* pg = &pqf->ConnectorID();
    _snwprintf(
        pfn,
        buff_size,
        FN_CONNECTOR_TOKEN  // "CONNECTOR"
            FN_EQUAL_SIGN   // "="
            GUID_FORMAT     // "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
            SUFFIX_FORMAT,  // "%s"
        GUID_ELEMENTS(pg),
        pSuffix
        );

    //
    //  return format name buffer length *not* including suffix length
    //  "CONNECTOR=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\0"
    //
    return (
        FN_CONNECTOR_TOKEN_LEN + 1 +
        GUID_STR_LENGTH + 1
        );
}


//
// Convert a QUEUE_FORMAT union to a format name string.
//
inline
NTSTATUS
MQpQueueFormatToFormatName(
    const QUEUE_FORMAT* pqf,    // queue format to translate
    LPWSTR pfn,                 // lpwcsFormatName format name buffer
    ULONG buff_size,            // format name buffer size
    PULONG pulFormatNameLength  // required buffer length of format name
    )
{
#ifdef _DEBUG
    //
    //  avoid crt assert in debug builds
    //
    if(pfn == 0)
    {
        pfn = (LPWSTR)1;
    }
#endif // _DEBUG

    //
    //  Sanity check
    //
    pqf->Validate();

    const LPCWSTR suffixes[] = {
        FN_NONE_SUFFIX,
        FN_JOURNAL_SUFFIX,
        FN_DEADLETTER_SUFFIX,
        FN_DEADXACT_SUFFIX,
        FN_XACTONLY_SUFFIX,
    };

    const ULONG suffixes_len[] = {
        FN_NONE_SUFFIX_LEN,
        FN_JOURNAL_SUFFIX_LEN,
        FN_DEADLETTER_SUFFIX_LEN,
        FN_DEADXACT_SUFFIX_LEN,
        FN_XACTONLY_SUFFIX_LEN,
    };


    ULONG fn_size = suffixes_len[pqf->Suffix()];
    LPCWSTR pSuffix = suffixes[pqf->Suffix()];

    switch(pqf->GetType())
    {
        case QUEUE_FORMAT_TYPE_PUBLIC:
            fn_size += MQpPublicToFormatName(pqf, pSuffix, buff_size, pfn);
            break;

        case QUEUE_FORMAT_TYPE_PRIVATE:
            fn_size += MQpPrivateToFormatName(pqf, pSuffix, buff_size, pfn);
            break;

        case QUEUE_FORMAT_TYPE_DIRECT:
            fn_size += MQpDirectToFormatName(pqf, pSuffix, buff_size, pfn);
            break;

        case QUEUE_FORMAT_TYPE_MACHINE:
            fn_size += MQpMachineToFormatName(pqf, pSuffix, buff_size, pfn);
            break;

        case QUEUE_FORMAT_TYPE_CONNECTOR:
            fn_size += MQpConnectorToFormatName(pqf, pSuffix, buff_size, pfn);
            break;

        default:
            //
            //  ASSERT(0) with no level 4 warning
            //
            ASSERT(pqf->GetType() == QUEUE_FORMAT_TYPE_DIRECT);
    }

    *pulFormatNameLength = fn_size;
    if(buff_size < fn_size)
    {
        //
        //  put a null terminator, and indicate buffer too small
        //
        if(buff_size > 0)
        {
            pfn[buff_size - 1] = 0;
        }
        return MQ_ERROR_FORMATNAME_BUFFER_TOO_SMALL;
    }

    return MQ_OK;
}

#endif //  __MQFORMAT_H
