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

//
// testproc declarations
//


TESTPROCAPI Test_Booted_OK(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI Test_Phone_UI(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI Test_Phone_App(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI Test_Net_ipconfig(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI Test_Net_ping(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI Test_Net_ndisconfig(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI Test_Net_Media(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI Test_Net_Http(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);



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

void __keybd_event(DWORD _K1, DWORD _K2=0);


#endif // __TESTPROC_H__
