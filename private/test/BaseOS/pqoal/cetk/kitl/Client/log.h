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

#define countof(a) ((sizeof(a))/(sizeof(a[0])))

void FAILLOGA(LPCTSTR szFmt, ...);
void FAILLOG(LPCTSTR szFmt, ...);
void LOG(LPCTSTR szFmt, ...);
VOID Debug(LPCTSTR szFormat, ...);
#define FAILLOGV() {FAILLOG(L"Error hit in %S::%S::%d", __FUNCTION__,  __FILE__, __LINE__);ASSERT(0);}
