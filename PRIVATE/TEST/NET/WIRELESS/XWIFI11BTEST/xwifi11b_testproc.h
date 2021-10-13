//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
