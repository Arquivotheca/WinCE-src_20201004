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
TESTPROCAPI Test1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI Test_CheckWiFiCard(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI Test_ResetPreferredList(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI Test_CheckUiWhenNoPreferredSsid(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI Test_CheckUiWhenNoPreferredSsid_100(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI Test_ActivateDeactivate100(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI Test_ConnectDisconnect100(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);


TESTPROCAPI Test_Adhoc_Ssid(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );


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
