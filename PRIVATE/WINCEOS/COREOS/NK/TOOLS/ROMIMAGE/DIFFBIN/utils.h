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
// utils.h

HRESULT SafeRealloc(LPVOID *pPtr, UINT32 cBytes);

BOOL OnAssertionFail(
    eCodeType eType, // in - type of the assertion
    DWORD dwCode, // return value for what failed
    const TCHAR* pszFile, // in - filename
    unsigned long ulLine, // in - line number
    const TCHAR* pszMessage // in - message to include in assertion report
    );
