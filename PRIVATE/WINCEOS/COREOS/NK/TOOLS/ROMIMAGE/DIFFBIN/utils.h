//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
