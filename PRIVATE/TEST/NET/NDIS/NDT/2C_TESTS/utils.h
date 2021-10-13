//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __UTILS_H
#define __UTILS_H

//------------------------------------------------------------------------------

#define COUNT(x) (sizeof(x)/sizeof(x[0]))

BOOL GetOptionFlag(int *pargc, LPTSTR argv[], LPCTSTR szOption);
LPTSTR GetOptionString(int *pargc, LPTSTR argv[], LPCTSTR szOption);
LONG GetOptionLong(int *pargc, LPTSTR argv[], LONG lDefault, LPCTSTR szOption);
void CommandLineToArgs(TCHAR szCommandLine[], int *argc, LPTSTR argv[]);
BOOL NotReceivedOK(ULONG ulPacketsReceived, ULONG ulPacketsSent);

//------------------------------------------------------------------------------

#endif
