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

#include <windows.h>
#include <auto_xxx.hxx>
#include <bt_api.h>
#include <bt_buffer.h>
#include <bt_ddi.h>
#include <bt_avctp.h>

#define BD_ADDRESS_STRING_LEN   (12+1)  // 12 character string + null

// This API blocks for the specified amount of time or until the specified
// initialized event occurs.  The API returns TRUE if the stack is up, or false
// if a timeout or error occured.
inline BOOL
WaitForBthEvent(
    __in DWORD dwMilliseconds,
    __in LPCWSTR szEventName)
{
    BOOL result = TRUE;

    const ce::auto_handle h = OpenEvent(EVENT_ALL_ACCESS, FALSE, szEventName);
    if (!h.valid())
    {
        // Not installed or other random failure
        ASSERT(0);
        result = FALSE;
        goto exit;
    }

    const DWORD dwWait = WaitForSingleObject(h, dwMilliseconds);
    if (dwWait != WAIT_OBJECT_0)
    {
        // Timeout or other random failure
        result = FALSE;
        goto exit;
    }

exit:
    return result;
}

// Wait for the stack to be ready
inline BOOL 
WaitForBthStackReady(
    __in DWORD dwMilliseconds)
{
    return WaitForBthEvent(dwMilliseconds, BTH_NAMEDEVENT_STACK_INITED);
}

// Wait for AVDTP to be ready
inline BOOL 
WaitForBthAvdtpReady(
    __in DWORD dwMilliseconds)
{
    return WaitForBthEvent(dwMilliseconds, BTH_NAMEDEVENT_AVDTP_INITED);
}

// Wait for AVCTP to be ready
inline BOOL 
WaitForBthAvctpReady(
    __in DWORD dwMilliseconds)
{
    return WaitForBthEvent(dwMilliseconds, BTH_NAMEDEVENT_AVCTP_INITED);
}


// This API converts a BT address in string representation to a BD_ADDR.
inline BOOL 
GetBD(
    __in WCHAR *pString, 
    __out BD_ADDR *pba)
{
    pba->NAP = 0;

    for (int i = 0 ; i < 4 ; ++i, ++pString) 
    {
        if (! iswxdigit (*pString))        
            return FALSE;
        
        int c = *pString;
        if (c >= 'a')
            c = c - 'a' + 0xa;
        else if (c >= 'A')
            c = c - 'A' + 0xa;
        else c = c - '0';

        if ((c < 0) || (c > 16))
            return FALSE;

        pba->NAP = (unsigned short)(pba->NAP * 16 + c);
    }

    pba->SAP = 0;

    for (i = 0 ; i < 8 ; ++i, ++pString) 
    {
        if (! iswxdigit (*pString))
            return FALSE;

        int c = *pString;
        if (c >= 'a')
            c = c - 'a' + 0xa;
        else if (c >= 'A')
            c = c - 'A' + 0xa;
        else c = c - '0';

        if ((c < 0) || (c > 16))
            return FALSE;

        pba->SAP = pba->SAP * 16 + c;
    }

    if (*pString != '\0')
        return FALSE;

    return TRUE;
}
