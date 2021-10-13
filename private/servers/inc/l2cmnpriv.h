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

Copyright (c) 2004 Microsoft Corporation

Module Name:

    l2cmnpriv.h

Abstract:

    Definitions and data structures common private layer 2 

Environment:

    User mode only

Revision History:


--*/

#ifndef _L2CMNPRIV_H
#define _L2CMNPRIV_H

#include "l2cmn.h"

#ifdef __cplusplus
extern "C" {
#endif



// the types of notification
// This code signifies that the Notification is a private notification all private enums should start with this .
#define L2_NOTIFICATION_CODE_PRIVATE_BEGIN         0X10000
#define IsPrivateNotif(notif) \
            ((notif&L2_NOTIFICATION_CODE_PRIVATE_BEGIN) == L2_NOTIFICATION_CODE_PRIVATE_BEGIN)

#define L2_UI_REQUEST_FLAG_EXPIRE_TIME      0x0000000000000001

typedef struct _L2_UI_REQUEST {
    ULONGLONG Flags;
    ULONGLONG ExpireTime; // Absolute time of expiry
    GUID InterfaceGuid;
    GUID NetworkGuid; 
    DWORD dwSessionId;  
    GUID UIPageClsid;	
    DWORD dwDataSize;
    DWORD fIsOneXExtUIReq;
    // !!! the following field is used to align the data blob.
    // !!! please do not add anything between it and DataBlob
    __int64 Padding;
#ifdef __midl
    [unique, size_is(dwDataSize)] BYTE DataBlob[*];
#else
    BYTE DataBlob[1];
#endif
} L2_UI_REQUEST, *PL2_UI_REQUEST;

typedef struct _L2_UI_RESPONSE {
    GUID InterfaceGuid;
    GUID UIRequestId;
    DWORD dwDataSize;
    // !!! the following field is used to align the data blob.
    // !!! please do not add anything between it and DataBlob
    __int64 Padding;
#ifdef __midl
    [unique, size_is(dwDataSize)] BYTE DataBlob[*];
#else
    BYTE DataBlob[1];
#endif
} L2_UI_RESPONSE, *PL2_UI_RESPONSE;

#ifdef __cplusplus
}
#endif

#endif  // _L2CMNPRIV_H
