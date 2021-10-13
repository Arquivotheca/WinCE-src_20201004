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
#include "StdAfx.h"
#include "Utils.h"
#include "ShellProc.h"
#include "NDTMsgs.h"
#include "NDTLog.h"
#include "NDTLib.h"
#include "NDTLibEx.h"
#include "ndtlibwlan.h"
#include "ndt_2c_wlan.h"

//------------------------------------------------------------------------------

#define BASE 0x00000001

//------------------------------------------------------------------------------

extern LPCTSTR g_szTestGroupName = _T("ndt_2c_wlan");

//------------------------------------------------------------------------------

extern FUNCTION_TABLE_ENTRY g_lpFTE[] = {

   _T("2c_wlan_adhoc.wsf"           ), 0, 0, BASE + 0, TestWlanAdhoc,
   _T("2c_wlan_mediastream.wsf"                   ), 0, 0, BASE + 1, TestWlanMediastream,
   _T("2c_wlan_reloaddefaults.wsf"               ), 0, 0, BASE + 2, TestWlanReloaddefaults,
   _T("2c_wlan_statistics.wsf"           ), 0, 0, BASE + 3, TestWlanStatistics,
   _T("2c_wlan_wep.wsf"            ), 0, 0, BASE + 4, TestWlanWep,
   
   _T("2c_wpa_addkey"	), 0, 0, BASE + 10, TestWpaAddkey,
   _T("2c_wpa_adhoc"	), 0, 0, BASE + 11, TestWpaAdhoc ,
   _T("2c_wpa_authentication"	), 0, 0, BASE + 12, TestWpaAuthentication,
   _T("2c_wpa_bssidlist"	), 0, 0, BASE + 13, TestWpaBssidlist,
   _T("2c_wpa_infrastructure"	), 0, 0, BASE + 14, TestWpaInfrastructure,
   _T("2c_wpa_removekey"	), 0, 0, BASE + 15, TestWpaRemovekey,
   _T("2c_wpa_sendcheck"	), 0, 0, BASE + 16, TestWpaSendCheck,
   _T("2c_wpa_sendrecv_aes"	), 0, 0, BASE + 17, TestWpaSendrecvAes ,
   
   _T("2c_wpa2_misc"	), 0, 0, BASE + 20, TestWpa2Misc,
   _T("2c_wpa2_sendcheck"	), 0, 0, BASE + 21, TestWpa2Sendcheck,
   _T("2c_wpa2_sendrecv"	), 0, 0, BASE + 22, TestWpa2Sendrecv,  

   _T("2c_simple_WEP_Xfer"	), 0, 0, BASE + 100, SimpleWEPXfer,
   _T("2c_simple_WPA_Xfer"	), 0, 0, BASE + 101, SimpleWPAXfer,
   _T("2c_simple_WPAWEP_Xfer"	), 0, 0, BASE + 102, SimpleWPAWEPXfer,
   _T("2c_simple_WPA2_Xfer"	), 0, 0, BASE + 103, SimpleWPA2Xfer,
   _T("2c_simple_WPA2WEP_Xfer"	), 0, 0, BASE + 104, SimpleWPA2WEPXfer,
   NULL, 0, 0, 0, NULL 
};

//------------------------------------------------------------------------------

HANDLE g_hTestDevice = NULL;
TCHAR  g_szTestAdapter[256] = _T("");
TCHAR g_szHelpAdapter[256] = _T("");
NDIS_MEDIUM g_ndisMedium = (NDIS_MEDIUM)-1;
BOOL  g_bNoUnbind = FALSE;
BOOL  g_bDisplayBssidList = FALSE;
DWORD g_dwBeatDelay = 0;
BOOL  g_bLogPackets = FALSE;
BOOL  g_bWirelessMiniport = FALSE;
DWORD g_dwStrictness = MildStrict;
DWORD g_dwDirectedPasspercentage = WLAN_PERCENT_TO_PASS_DIRECTED ;
DWORD g_dwBroadcastPasspercentage = WLAN_PERCENT_TO_PASS_BROADCAST;

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
      
      lstrcpy(szAdapterFull, szAdapter);
      if (szSystem != NULL) {
         lstrcat(szAdapterFull, _T("@"));
         lstrcat(szAdapterFull, szSystem);
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
   NDTLogMsg(_T("tux -o -d ndt_2c_wlan -c \"-s support_adapter -t test_adapter ...\""));
   NDTLogMsg(_T(""));
   NDTLogMsg(_T("Parameters:"));
   NDTLogMsg(_T(""));
   NDTLogMsg(_T("-t adapter : Adapter to test (e.g. CISCO1)"));
   NDTLogMsg(_T("-s adapter : Support adapter (e.g. ISLP21@192.168.0.102) "));
   NDTLogMsg(_T("         The support adapter should be wireless lan for adhoc tests to work"));
   NDTLogMsg(_T("-nounbind  : Do not unbind protocols from the test adapter"));
   NDTLogMsg(_T("-displaybssidlist    : Displays before the start of the test, a list of APs seen by the test network card"));
   NDTLogMsg(_T("-strictness <1 /5> : Control whether test strictly follow the ndis guide (1: Default) or make it work with wzcsvc (5)"));
   NDTLogMsg(_T(""));
   NDTLogMsg(_T("Example usage: tux -o -d ndt_2c_wlan -x 1-23 -c\"-t CISCO1 -s PCI\\AR52111@192.168.1.101 -nounbind\" "));
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
	   lstrcpy(g_szTestAdapter, szOption);

    	   szOption = GetOptionString(&argc, argv, _T("s"));
	   if (szOption == NULL) goto cleanUp;
	   lstrcpy(g_szHelpAdapter, szOption);
   }
   else
   {
        // Pass number of characters that can be stored in g_szTestAdapter
	   if (!GetTestAdapterAutomatically(NULL, g_szTestAdapter, sizeof(g_szTestAdapter)/sizeof(g_szTestAdapter[0])))
	   {
		   NDTLogMsg(_T("Error: Failed to locate miniport adapter *automatically* to which NDIS test can bind"));
		   goto cleanUp;
	   }
   }

   g_bNoUnbind = GetOptionFlag(&argc, argv, _T("nounbind"));
   g_bDisplayBssidList = GetOptionFlag(&argc, argv, _T("displaybssidlist"));
   g_dwBeatDelay = GetOptionLong(&argc, argv, g_dwBeatDelay, _T("beat"));
   g_bLogPackets = GetOptionFlag(&argc, argv, _T("packets"));
   g_dwStrictness = GetOptionLong(&argc, argv, g_dwStrictness, _T("strictness"));

   
   bResult = TRUE;
 
cleanUp:
   if (!bResult) PrintUsage();
   return bResult;
}

//------------------------------------------------------------------------------

BOOL PrepareToRun()
{
   BOOL bOk = FALSE;
   DWORD rc = ERROR_SUCCESS;
   HRESULT hr = S_OK;
   HANDLE hTestAdapter = NULL;
   HANDLE hHelpAdapter = NULL;

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
   hr = NDTOpen(g_szTestAdapter, &hTestAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
      goto cleanUp;
   }
   
   hr = NDTSetOptions(hTestAdapter, dwLogLevel, g_dwBeatDelay);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailSetOptions, g_szTestAdapter, hr);
      goto cleanUp;
   }

   hr = NDTBind(hTestAdapter, FALSE, g_ndisMedium, &g_ndisMedium);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailBind, hr);
      goto cleanUp;
   }

   // Lets find out if test miniport driver is wireless or not.
   NDTLogMsg(_T("Querying if test miniport is wireless"));
   UINT uiPhyMeduim = NdisPhysicalMediumUnspecified;

   hr = NDTQueryInfo(
	   hTestAdapter, OID_GEN_PHYSICAL_MEDIUM, &uiPhyMeduim,
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

         if (g_bDisplayBssidList)
            NDTWlanPrintBssidList(hTestAdapter);
   }
   else 
   {
	   NDTLogMsg(_T("Miniport %s is ** NOT Wireless-LAN **"), g_szTestAdapter);
   }
   
   hr = NDTUnbind(hTestAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailUnbind, hr);
      goto cleanUp;
   }


   // Initialize help adapter
   hr = NDTStartup(NDTSystem(g_szHelpAdapter));
   if (FAILED(hr)) {
      NDTLogErr(g_szFailStartup, g_szHelpAdapter, hr);
      goto cleanUp;
   }

   if (!g_bNoUnbind) {
      hr = NDTUnbindProtocol(g_szHelpAdapter, NULL);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailUnbindProtocol, g_szHelpAdapter, hr);
         goto cleanUp;
      }
   }

   // Set options and try bind/unbind adapter
   hr = NDTOpen(g_szHelpAdapter, &hHelpAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailOpen, g_szHelpAdapter, hr);
      goto cleanUp;
   }
   
   hr = NDTSetOptions(hHelpAdapter, dwLogLevel, g_dwBeatDelay);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailSetOptions, g_szHelpAdapter, hr);
      goto cleanUp;
   }

   hr = NDTBind(hHelpAdapter, FALSE, g_ndisMedium, &g_ndisMedium);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailBind, hr);
      goto cleanUp;
   }
   
   bOk = TRUE;
   
cleanUp:
   if (hHelpAdapter != NULL) {
      hr = NDTClose(&hHelpAdapter);
      if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
   }   

   if (hTestAdapter != NULL) {
      hr = NDTClose(&hTestAdapter);
      if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
   }
   if (!bOk) FinishRun();
   return bOk;
}

//------------------------------------------------------------------------------

BOOL FinishRun()
{
   HRESULT hr = S_OK;

   // Test Adapter   
   if (!g_bNoUnbind) {
      hr = NDTBindProtocol(g_szTestAdapter, NULL);
      if (FAILED(hr)) NDTLogErr(g_szFailBindProtocol, g_szTestAdapter, hr);
   }
   
   // Support Adapter
   if (!g_bNoUnbind) {
      hr = NDTBindProtocol(g_szHelpAdapter, NULL);
      if (FAILED(hr)) NDTLogErr(g_szFailBindProtocol, g_szHelpAdapter, hr);      
   }

   hr = NDTCleanup(NDTSystem(g_szTestAdapter));
   if (FAILED(hr)) NDTLogErr(g_szFailCleanup, g_szTestAdapter, hr);
   hr = NDTCleanup(NDTSystem(g_szHelpAdapter));
   if (FAILED(hr)) NDTLogErr(g_szFailCleanup, g_szHelpAdapter, hr);
   return TRUE;
}

//------------------------------------------------------------------------------

// This function returns the first miniport adapter that supports NdisMedium802_3
// szSystem: In: Indicates local or remote machine. Use NULL for local & IP address of remote machine
//               machine for remote.           
// pszAdapter: In: Pointer to buffer OUT:will have name of adapter
// DWORD cbSize: In: size of buffer in bytes.
BOOL GetTestAdapterAutomatically(LPCTSTR szSystem, PTCHAR pszAdapter, DWORD cbSize)
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
      
      lstrcpy(szAdapterFull, szAdapter);
      if (szSystem != NULL) {
         lstrcat(szAdapterFull, _T("@"));
         lstrcat(szAdapterFull, szSystem);
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
		  if ( (_tcslen(szAdapterFull)+1) <= cbSize)
		  {
			  _tcscpy(pszAdapter, szAdapterFull);
			  fRet=TRUE;
		  }
		  else
			  cbSize = _tcslen(szAdapterFull)+1;

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

