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
#ifndef __TAPIINFO_H__
#define __TAPIINFO_H__

#ifdef __cplusplus
extern "C" {
#endif

LPTSTR FormatLineCallback(LPTSTR szOutputBuffer,
  DWORD dwDevice, DWORD dwMsg, DWORD dwCallbackInstance, 
  DWORD dwParam1, DWORD dwParam2, DWORD dwParam3);
LPTSTR FormatLineError(long lLineError, LPTSTR lpszOutputBuffer);
VOID PrintAndAppend(LPTSTR lpszMsg, LPCTSTR lpszFormat, ...);
#ifdef __cplusplus
}
#endif

#endif
