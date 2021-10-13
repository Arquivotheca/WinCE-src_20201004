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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#ifndef __TESTPROC_H__
#define __TESTPROC_H__

#include <winsock2.h>

// see xwifi-auto-test-cases.exl
TESTPROCAPI IF1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI F1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI F2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI F3(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);   // stability
TESTPROCAPI F_Stress(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

#define STANDARD_BUF_SIZE 256

extern wchar_t argv0[STANDARD_BUF_SIZE];
extern int     argc;
extern wchar_t **argv;


int
WasOption
// look for tux argument then find -c option.
// this -c is our module-specific command line option.
// get this string, then find given option in this string.
// returns option index
// returns index of argv[] found, -1 if not found
(
    IN int argc,       // number of args
    IN WCHAR* argv[],  // arg array
    IN WCHAR* szOption // to find ('t')
);



int
GetOption
// look for argument like '-t 100' or '/t 100'.
// returns index of '100' if option ('t') is found
// returns -1 if not found
(
    int argc,                // number of args
    IN WCHAR* argv[],        // arg array
    IN WCHAR* pszOption,     // to find ('n')
    OUT WCHAR** ppszArgument // option value ('100')
);






BOOL Prepare_TestSSIDSet();
BOOL IsDialogWindowPoppedUp(WCHAR *szTitle);


// general functions
BOOL DoNdisIOControl(
IN DWORD   dwCommand,
IN LPVOID  pInBuffer,
IN DWORD   cbInBuffer,
IN LPVOID  pOutBuffer,
IN DWORD   *pcbOutBuffer OPTIONAL
);
BOOL IsAdapterInstalled(WCHAR *szAdapter);
BOOL _NdisConfigAdapterBind(WCHAR *szAdapter1, DWORD dwCommand);
BOOL _CardInsertNdisMiniportAdapter(TCHAR* szAdapter);
int CardInsertNdisMiniportAdapter(TCHAR* szAdapter, TCHAR* szIpAddress);
int CardEjectNdisMiniportAdapter(WCHAR *szAdapter1);
WCHAR *NextOption(WCHAR *p, WCHAR *pOption);
DWORD GenerateRandomInterval(DWORD dwInterval);

BOOL AdapterHasNoneZeroIpAddress(WCHAR *szAdapter);
BOOL WaitForLoosingNetworkConnection(WCHAR *szAdapter);
BOOL WaitForGainingNetworkConnection(WCHAR *szAdapter);
BOOL AdapterHasIpAddress(WCHAR *szAdapter1, WCHAR *szIpAddr);

void
AddSsidToThePreferredList
(
WCHAR* szDeviceName, // [in] wireless network card device name
WCHAR* szPreferredSSID_withOption
);

void ResetPreferredList(WCHAR* szDeviceName);
void FindWirelessNetworkDevice();


#endif // __TESTPROC_H__
