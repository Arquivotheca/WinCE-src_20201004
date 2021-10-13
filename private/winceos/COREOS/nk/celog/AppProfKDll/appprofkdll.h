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


#define KDLL_NAME        TEXT("AppProfKDll.dll")

// IOCTLs supported by KDLL
typedef enum {
    // Query the ID of the thread that ran most recently before the current
    // thread.  If the CPU was idle before the current thread, returns 0.
    // Logging must be enabled.
    //
    // lpInBuf:     ignored
    // nInBufSize:  ignored
    // lpOutBuf:    Buffer to receive a DWORD with the thread ID.
    // nOutBufSize: Must be at least sizeof(DWORD).
    IOCTL_KDLL_GET_PREV_THREAD   = 1,

} KDLL_IOCTL_ID;
