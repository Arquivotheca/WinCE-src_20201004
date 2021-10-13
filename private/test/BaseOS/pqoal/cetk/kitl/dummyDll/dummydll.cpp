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
/*
 * dummyDll.cpp
 */

#include <windows.h>

/*
 * This file provides an empty dll which the other tests can use.
 * This way the tests are always guaranteed to have a dll to work with.
 */

BOOL WINAPI DllMain(HANDLE /*hInstance*/, DWORD /*dwReason*/, LPVOID /*lpReserved*/)
{
  return (TRUE);
}

