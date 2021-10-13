//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef __UTILS_H
#define __UTILS_H

//------------------------------------------------------------------------------

#define COUNT(x) (sizeof(x)/sizeof(x[0]))

BOOL GetOptionFlag(int *pargc, LPTSTR argv[], LPCTSTR szOption);
LPTSTR GetOptionString(int *pargc, LPTSTR argv[], LPCTSTR szOption);
LONG GetOptionLong(int *pargc, LPTSTR argv[], LONG lDefault, LPCTSTR szOption);
BOOL GetOptionMAC(int *pargc, LPTSTR argv[], UCHAR aMAC[], LPCTSTR szOption);
void CommandLineToArgs(TCHAR szCommandLine[], int *argc, LPTSTR argv[]);
TCHAR * MultiSzToSz(OUT TCHAR * tsz,IN TCHAR *tszMulti);
BOOL AddSecondsToFileTime(LPFILETIME lpFileTime, DWORD dwSeconds);

//------------------------------------------------------------------------------

#endif
