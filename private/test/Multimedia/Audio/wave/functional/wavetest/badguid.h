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
#ifndef __BadGUID_H__
#define __BadGUID_H__

#include "TUXMAIN.H"

#define INVALID_GUID 1
#define VALID_GUID   0

TESTPROCAPI
    BadGUID( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

// Helper Functions for use by the tests
DWORD ProcessWaveOutPropFunctionResults(
    MMRESULT MMResult, DWORD dwGUID_Index, const TCHAR *szID );

#endif