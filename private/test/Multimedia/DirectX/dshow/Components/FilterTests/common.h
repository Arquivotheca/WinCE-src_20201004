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
#ifndef _COMMON_H
#define _COMMON_H

#include <windows.h>
#include <kato.h>

#define countof(x) (sizeof(x)/sizeof(x[0]))

#define CHECK(hr)	if (FAILED(hr)){LOG(TEXT("error code: 0x%08x"),hr);goto cleanup;}

#define MAKELONGLONG(low,high) ((LONGLONG)(((DWORD)(low)) | ((LONGLONG)((DWORD)(high))) << 32))

#define HANDLE_TPM_EXECUTE()                \
    if ( uMsg != TPM_EXECUTE )                \
    {                                        \
        return TPR_NOT_HANDLED;                \
    }


//common functions


void Debug(LPCTSTR szFormat, ...);
void LOG(LPWSTR szFmt, ...);

#endif
