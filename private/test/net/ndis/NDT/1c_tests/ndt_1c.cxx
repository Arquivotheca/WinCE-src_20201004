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
#include "StdAfx.h"
#include "Utils.h"
#include "ShellProc.h"
#include "NDTMsgs.h"
#include "NDTLog.h"
#include "NDTLib.h"
#include "ndt_1c.h"

//------------------------------------------------------------------------------

#define BASE 0x00000001

//------------------------------------------------------------------------------

extern LPCTSTR g_szTestGroupName = _T("ndt_1c");

//------------------------------------------------------------------------------

extern FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    _T("1c_OpenClose"              ), 0, 0, BASE + 0, TestOpenClose,
    _T("1c_Send"                   ), 0, 0, BASE + 1, TestSend,
    _T("1c_LoopbackSend"           ), 0, 0, BASE + 2, TestLoopbackSend,
    _T("1c_LoopbackStress"         ), 0, 0, BASE + 3, TestLoopbackStress,
    _T("1c_SetMulticast"           ), 0, 0, BASE + 4, TestSetMulticast,
    _T("1c_Reset"                  ), 0, 0, BASE + 5, TestReset,
    _T("1c_CancelSend"             ), 0, 0, BASE + 6, TestCancelSend,
    _T("1c_FaultHandling"          ), 0, 0, BASE + 7, TestFaultHandling,
    _T("1c_OIDs"                   ), 0, 0, BASE + 8, TestOids,
    _T("1c_64BitOIDs"              ), 0, 0, BASE + 9, Test64BitOids,
    _T("1c_SuspendResume"          ), 0, 0, BASE +10, TestSusRes,
    _T("1c_StressSusRes"           ), 0, 0, BASE +11, TestStressSusRes,
    _T("1c_ResetOnResume"          ), 0, 0, BASE +12, TestResetOnResume,
    NULL, 0, 0, 0, NULL 
};

//------------------------------------------------------------------------------

HANDLE g_hTestDevice = NULL;
TCHAR  g_szTestAdapter[256] = _T("");
NDIS_MEDIUM g_ndisMedium = (NDIS_MEDIUM)-1;
DWORD g_dwStressDelay = 10;
BOOL  g_bNoUnbind = FALSE;
BOOL  g_bFaultHandling = FALSE;
BOOL  g_bForceCancel = FALSE;
BOOL  g_bNoStress = FALSE;
BOOL  g_bFullMulti = FALSE;
DWORD g_dwBeatDelay = 0;
BOOL  g_bLogPackets = FALSE;
BOOL  g_bWirelessMiniport = FALSE;
BOOL  g_bForceTestNicWiFi = FALSE;

// Auto mode per request from MDPG folks.
BOOL  g_bAutoMode=FALSE;

//------------------------------------------------------------------------------

BOOL GetTestAdapterAutomatically(LPCTSTR szSystem, PTCHAR pszAdapter, DWORD cbSize);

//------------------------------------------------------------------------------

void ListAdapters(LPCTSTR szSystem)
{
    HRESULT hr = S_OK;
    BYTE mszAdapters[2048];
    TCHAR szAdapterFull[256];
    LPTSTR szAdapter = NULL;
    HANDLE hAdapter = NULL;
    NDIS_MEDIUM ndisMedium = (NDIS_MEDIUM)-1;


    // Initialize system
    hr = NDTStartup(szSystem);
    if (FAILED(hr)) {
        NDTLogErr(g_szFailStartup, szSystem, hr);
        goto cleanUp;
    }

    hr = NDTQueryAdapters(szSystem, LPTSTR(mszAdapters), sizeof(mszAdapters));
    if (FAILED(hr)) {
        NDTLogErr(g_szFailQueryAdapters, szSystem);
        goto cleanUp;
    }

    NDTLogMsg(_T("Miniport NDIS Adapters"));
    NDTLogMsg(_T(""));
    NDTLogMsg(_T("Name                 Medium"));
    NDTLogMsg(_T("======================================"));

    szAdapter = LPTSTR(mszAdapters);
    while (*szAdapter != _T('\0')) {

        // Open and bind adapter to get a medium
        ndisMedium = (NDIS_MEDIUM)-1;

        StringCchCopy(szAdapterFull, _countof(szAdapterFull), szAdapter);
        if (szSystem != NULL) {
            StringCchCat(szAdapterFull, _countof(szAdapterFull),_T("@"));
            StringCchCat(szAdapterFull, _countof(szAdapterFull), szSystem);
        }

        // Set option and try bind/unbind from adapter to get prefered medium
        hr = NDTOpen(szAdapterFull, &hAdapter);
        if (FAILED(hr)) {
            NDTLogErr(g_szFailOpen, szAdapterFull, hr);
            goto cleanUp;
        }

        hr = NDTBind(hAdapter, FALSE, ndisMedium, &ndisMedium);
        if (FAILED(hr)) {
            NDTLogErr(g_szFailBind, hr);
            goto cleanUp;
        }

        hr = NDTUnbind(hAdapter);
        if (FAILED(hr)) {
            NDTLogErr(g_szFailUnbind, hr);
            goto cleanUp;
        }

        hr = NDTClose(&hAdapter);
        if (FAILED(hr)) {
            NDTLogErr(g_szFailClose, hr);
            goto cleanUp;
        }

        switch (ndisMedium) {
         case NdisMedium802_3:
             NDTLogMsg(_T("%-20s Ethernet 802.3"), szAdapter);
             break;
         case NdisMedium802_5:
             NDTLogMsg(_T("%-20s Token Ring 802.5"), szAdapter);
             break;
         default:
             NDTLogMsg(_T("%-20s ***Unsupported***"), szAdapter);
        }

        szAdapter += lstrlen(szAdapter) + 1;
    }

cleanUp:
    if (hAdapter != NULL) {
        NDTClose(&hAdapter);
        if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
    }      
    hr = NDTCleanup(szSystem);
    if (FAILED(hr)) NDTLogErr(g_szFailCleanup, szSystem, hr);
}

//------------------------------------------------------------------------------

void PrintUsage()
{
    NDTLogMsg(_T("tux -o -d ndt_1c -c \"-t test_adapter ...\""));
    NDTLogMsg(_T(""));
    NDTLogMsg(_T("Parameters:"));
    NDTLogMsg(_T(""));
    NDTLogMsg(_T("-t adapter : Adapter to test (e.g. NE20001)"));
    NDTLogMsg(_T("-nounbind  : Do not unbind protocols from the test adapter"));
    NDTLogMsg(_T("-fault     : Run 1c_FaultHandling test (default false)"));
    NDTLogMsg(_T("-cancel    : Check result for 1c_CancelSend (default false)"));
    NDTLogMsg(_T("-nostress  : Skip stress tests (default false)"));
    NDTLogMsg(_T("-fullmulti : Run 1c_SetMulticast with maximal number of addresses"));
    NDTLogMsg(_T("-forcetestnicwifi  : Make the test NIC appear to be wireless, even if NDIS detects it to be wired"));
    NDTLogMsg(_T(""));
    NDTLogMsg(_T(""));
    ListAdapters(NULL);
}

//------------------------------------------------------------------------------

BOOL ParseCommands(INT argc, LPTSTR argv[])
{
    BOOL bResult = FALSE;
    LPTSTR szOption = NULL;

    g_bAutoMode = GetOptionFlag(&argc, argv, _T("auto"));

    if (!g_bAutoMode)
    {
        szOption = GetOptionString(&argc, argv, _T("t"));
        if (szOption == NULL) goto cleanUp;
        StringCchCopy(g_szTestAdapter, _countof(g_szTestAdapter), szOption);
    }
    else
    {
        if (!GetTestAdapterAutomatically(NULL, g_szTestAdapter, _countof(g_szTestAdapter)))
        {
            NDTLogMsg(_T("Error: Failed to locate miniport adapter *automatically* to which NDIS test can bind"));
            goto cleanUp;
        }
    }

    g_dwStressDelay = GetOptionLong(&argc, argv, g_dwStressDelay, _T("delay"));
    g_bNoUnbind = GetOptionFlag(&argc, argv, _T("nounbind"));
    g_bFaultHandling = GetOptionFlag(&argc, argv, _T("fault"));
    g_bForceCancel = GetOptionFlag(&argc, argv, _T("cancel"));
    g_bNoStress = GetOptionFlag(&argc, argv, _T("nostress"));
    g_bFullMulti = GetOptionFlag(&argc, argv, _T("fullmulti"));
    g_dwBeatDelay = GetOptionLong(&argc, argv, g_dwBeatDelay, _T("beat"));
    g_bLogPackets = GetOptionFlag(&argc, argv, _T("packets"));
    g_bForceTestNicWiFi = GetOptionFlag(&argc, argv, _T("forcetestnicwifi"));

    bResult = TRUE;

cleanUp:
    if (!bResult) PrintUsage();
    return bResult;
}

//------------------------------------------------------------------------------

BOOL PrepareToRun()
{
    BOOL bOk = FALSE;
    HRESULT hr = S_OK;
    HANDLE hAdapter = NULL;
    DWORD dwLogLevel = 0;

    // Get log level value
    dwLogLevel = NDTLogGetLevel();
    if (g_bLogPackets) dwLogLevel |= NDT_LOG_PACKETS;

    // Initialize test adapter
    hr = NDTStartup(NDTSystem(g_szTestAdapter));
    if (FAILED(hr)) {
        NDTLogErr(g_szFailStartup, g_szTestAdapter, hr);
        goto cleanUp;
    }

    if (!g_bNoUnbind) {
        hr = NDTUnbindProtocol(g_szTestAdapter, NULL);
        if (FAILED(hr)) {
            NDTLogErr(g_szFailUnbindProtocol, g_szTestAdapter, hr);
            goto cleanUp;
        }
    }

   // Set options and try bind/unbind adapter
   hr = NDTOpen(g_szTestAdapter, &hAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
      goto cleanUp;
   }
   
   hr = NDTSetOptions(hAdapter, dwLogLevel, g_dwBeatDelay);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailSetOptions, g_szTestAdapter, hr);
      goto cleanUp;
   }

    hr = NDTSetOptions(hAdapter, dwLogLevel, g_dwBeatDelay);
    if (FAILED(hr)) {
        NDTLogErr(g_szFailSetOptions, g_szTestAdapter, hr);
        goto cleanUp;
    }

    hr = NDTBind(hAdapter, FALSE, g_ndisMedium, &g_ndisMedium);
    if (FAILED(hr)) {
        NDTLogErr(g_szFailBind, hr);
        goto cleanUp;
    }

    if (!g_bForceTestNicWiFi) {
        // Lets find out if miniport driver is wireless or not.
        NDTLogMsg(_T("Querying if miniport is wireless"));
        UINT uiPhyMeduim = NdisPhysicalMediumUnspecified;

        hr = NDTQueryInfo(
            hAdapter, OID_GEN_PHYSICAL_MEDIUM, &uiPhyMeduim,
            sizeof(UINT), NULL, NULL
            );

        if (  (SUCCEEDED(hr))  && 
            ( (uiPhyMeduim == NdisPhysicalMediumWirelessLan) || (uiPhyMeduim == NdisPhysicalMediumNative802_11) )
            )
        {
            g_bWirelessMiniport = TRUE;
            if (uiPhyMeduim == NdisPhysicalMediumWirelessLan)
                NDTLogMsg(_T("Miniport %s is ** Wireless-LAN **"), g_szTestAdapter);
            else
                NDTLogMsg(_T("Miniport %s is ** Native WiFi-LAN **"), g_szTestAdapter);
        }
        else 
        {
            NDTLogMsg(_T("Miniport %s is ** NOT Wireless-LAN **"), g_szTestAdapter);
        }
    }
    else
    {
        g_bWirelessMiniport = TRUE;
        NDTLogMsg(_T("Forcing miniport to appear to be wireless."));
    }

    hr = NDTUnbind(hAdapter);
    if (FAILED(hr)) {
        NDTLogErr(g_szFailUnbind, hr);
        goto cleanUp;
    }

    bOk = TRUE;

cleanUp:
    if (hAdapter != NULL) {
        hr = NDTClose(&hAdapter);
        if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
    }
    if (!bOk) FinishRun();
    return bOk;
}

//------------------------------------------------------------------------------

BOOL FinishRun()
{
    HRESULT hr = S_OK;
    
    // Rebind protocol
    if (!g_bNoUnbind) {
        hr = NDTBindProtocol(g_szTestAdapter, NULL);
        if (FAILED(hr)) NDTLogErr(g_szFailBindProtocol, g_szTestAdapter, hr);
    }

    hr = NDTCleanup(NDTSystem(g_szTestAdapter));
    if (FAILED(hr)) NDTLogErr(g_szFailCleanup, g_szTestAdapter, hr);
    return TRUE;
}

//------------------------------------------------------------------------------

// This function returns the first miniport adapter that supports NdisMedium802_3
// szSystem: In: Indicates local or remote machine. Use NULL for local & IP address of remote machine
//               machine for remote.           
// pszAdapter: In: Pointer to buffer OUT:will have name of adapter
// DWORD cbSize: In: size of buffer in bytes.
BOOL GetTestAdapterAutomatically(LPCTSTR szSystem, PTCHAR pszAdapter, DWORD cchSize)
{
    HRESULT hr = S_OK;
    BYTE mszAdapters[2048];
    TCHAR szAdapterFull[256];
    LPTSTR szAdapter = NULL;
    HANDLE hAdapter = NULL;
    NDIS_MEDIUM ndisMedium = (NDIS_MEDIUM)-1;
    BOOL fRet = FALSE;

    if (!pszAdapter) return FALSE;

    // Initialize system
    hr = NDTStartup(szSystem);
    if (FAILED(hr)) {
        NDTLogErr(g_szFailStartup, szSystem, hr);
        goto cleanUp;
    }

    hr = NDTQueryAdapters(szSystem, LPTSTR(mszAdapters), sizeof(mszAdapters));
    if (FAILED(hr)) {
        NDTLogErr(g_szFailQueryAdapters, szSystem);
        goto cleanUp;
    }

    szAdapter = LPTSTR(mszAdapters);

    while (*szAdapter != _T('\0')) {

        // Open and bind adapter to get a medium
        ndisMedium = (NDIS_MEDIUM)-1;

        StringCchCopy(szAdapterFull, _countof(szAdapterFull), szAdapter);
        if (szSystem != NULL) {
            StringCchCat(szAdapterFull, _countof(szAdapterFull),_T("@"));
            StringCchCat(szAdapterFull, _countof(szAdapterFull), szSystem);
        }

        // Set option and try bind/unbind from adapter to get prefered medium
        hr = NDTOpen(szAdapterFull, &hAdapter);
        if (FAILED(hr)) {
            NDTLogErr(g_szFailOpen, szAdapterFull, hr);
            goto cleanUp;
        }

        hr = NDTBind(hAdapter, FALSE, ndisMedium, &ndisMedium);
        if (FAILED(hr)) {
            NDTLogErr(g_szFailBind, hr);
            goto cleanUp;
        }

        hr = NDTUnbind(hAdapter);
        if (FAILED(hr)) {
            NDTLogErr(g_szFailUnbind, hr);
            goto cleanUp;
        }

        hr = NDTClose(&hAdapter);
        if (FAILED(hr)) {
            NDTLogErr(g_szFailClose, hr);
            goto cleanUp;
        }

        if (ndisMedium == NdisMedium802_3)
        {
            if ( (_tcslen(szAdapterFull)+1) <= cchSize)
            {
                StringCchCopy(pszAdapter, cchSize, szAdapterFull);
                fRet=TRUE;
            }
            else
                cchSize = _tcslen(szAdapterFull)+1;

            break;
        }

        szAdapter += lstrlen(szAdapter) + 1;
    }

cleanUp:
    if (hAdapter != NULL) {
        NDTClose(&hAdapter);
        if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
    }      
    hr = NDTCleanup(szSystem);
    if (FAILED(hr)) NDTLogErr(g_szFailCleanup, szSystem, hr);

    return fRet;
}

