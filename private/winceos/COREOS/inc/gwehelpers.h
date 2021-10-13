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

#include <guts.h>
#include <ehm.h>


// extra handler to ensure that Win32 errors always return error values
__inline HRESULT ERR_FROM_WIN32(DWORD code) { return (0==code?E_FAIL:HRESULT_FROM_WIN32(code)); }

#define CBRAGLEx(exp,code) CBRAEx(exp,ERR_FROM_WIN32(code))
#define CBRGLEx(exp,code)  CBREx(exp,ERR_FROM_WIN32(code))
#define CBRAGLE(exp)       CBRAGLEx(exp,GetLastError())
#define CBRGLE(exp)        CBRGLEx(exp,GetLastError())
