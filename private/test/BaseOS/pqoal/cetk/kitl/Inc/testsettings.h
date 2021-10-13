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

#ifdef UNDER_CE
#define KITL_STREAM_ID         "KITLTest"
#define KITL_TEST_SETTINGS_ID  "KITLTestSettings"
#else if
#define KITL_STREAM_ID         L"KITLTest"
#define KITL_TEST_SETTINGS_ID  L"KITLTestSettings"
#endif

#define KITL_RCV_TIMEOUT        300
#define KITL_RCV_ITERATIONS     1
#define KITL_SEND_ITERATIONS    1
#define KITL_PAYLOAD_DATA       '0'



struct TEST_SETTINGS
{
    // Stress test settings
    DWORD   dwSettingsSize;
    DWORD   dwThreadCount;
    DWORD   dwMinPayLoadSize;
    DWORD   dwMaxPayLoadSize;
    UCHAR   ucFlags;
    UCHAR   ucWindowSize;
    DWORD   dwRcvIterationsCount;
    DWORD   dwRcvTimeout;
    BOOL    fVerifyRcv;
    DWORD   dwSendIterationsCount;
    CHAR    cData;
    DWORD   dwDelay;
};