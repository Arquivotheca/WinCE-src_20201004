//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "Utils.h"
#include "ShellProc.h"
#include "NDTMsgs.h"
#include "NDTLog.h"
#include "NDTLib.h"
#include "ndt_2c.h"

//------------------------------------------------------------------------------

#define BASE         0x00000001

//------------------------------------------------------------------------------

LPCTSTR g_szTestGroupName = _T("ndt_2c");

//------------------------------------------------------------------------------

extern FUNCTION_TABLE_ENTRY g_lpFTE[] = {
   _T("2c_SendPackets"            ), 0, 0, BASE + 0, TestSendPackets,
   _T("2c_ReceivePackets"         ), 0, 0, BASE + 1, TestReceivePackets,
   _T("2c_FilterReceive"          ), 0, 0, BASE + 2, TestFilterReceive,
   _T("2c_MulticastReceive"       ), 0, 0, BASE + 3, TestMulticastReceive,
   _T("2c_StressSend"             ), 0, 0, BASE + 4, TestStressSend,
   _T("2c_StressReceive"          ), 0, 0, BASE + 5, TestStressReceive,
   NULL, 0, 0, 0, NULL 
};

//------------------------------------------------------------------------------

HANDLE g_hTestDevice = NULL;
TCHAR g_szTestAdapter[256] = _T("");
TCHAR g_szHelpAdapter[256] = _T("");
NDIS_MEDIUM g_ndisMedium = (NDIS_MEDIUM)-1;
DWORD g_dwStressDelay = 10;
BOOL  g_bNoUnbind = FALSE;
BOOL  g_bNoStress = FALSE;
DWORD g_dwBeatDelay = 0;
BOOL  g_bLogPackets = FALSE;

//------------------------------------------------------------------------------

void ListAdapters(LPCTSTR szSystem)
{
   HRESULT hr = S_OK;
   TCHAR mszAdapters[1024];
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

   hr = NDTQueryAdapters(szSystem, mszAdapters, sizeof(mszAdapters));
   if (FAILED(hr)) {
      NDTLogErr(g_szFailQueryAdapters, szSystem);
      goto cleanUp;
   }

   NDTLogMsg(_T("Miniport NDIS Adapters"));
   NDTLogMsg(_T(""));
   NDTLogMsg(_T("Name                 Medium"));
   NDTLogMsg(_T("======================================"));
   
   szAdapter = mszAdapters;
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
   NDTLogMsg(_T("tux -o -d ndt_2c -c \"-s support_adapter -t test_adapter ...\""));
   NDTLogMsg(_T(""));
   NDTLogMsg(_T("Parameters:"));
   NDTLogMsg(_T(""));
   NDTLogMsg(_T("-t adapter : Adapter to test (e.g. NE20001)"));
   NDTLogMsg(_T("-s adapter : Support adapter (e.g. NE20001@KARELD01)"));
   NDTLogMsg(_T("-nounbind  : Do not unbind protocols from the test adapter"));
   NDTLogMsg(_T("-nostress  : Skip stress test (default false)"));
   NDTLogMsg(_T(""));
   NDTLogMsg(_T("Adapter name is in form 'adapter@IP' for remote adapter"));
   NDTLogMsg(_T("and in form 'adapter' for local one. E.g. NE20001@196.23.3.3"));
   NDTLogMsg(_T(""));
   NDTLogMsg(_T(""));
   ListAdapters(NULL);
}

//------------------------------------------------------------------------------

BOOL ParseCommands(INT argc, LPTSTR argv[])
{
   BOOL bResult = FALSE;
   LPTSTR szOption = NULL;

   szOption = GetOptionString(&argc, argv, _T("t"));
   if (szOption == NULL) goto cleanUp;
   lstrcpy(g_szTestAdapter, szOption);

   szOption = GetOptionString(&argc, argv, _T("s"));
   if (szOption == NULL) goto cleanUp;
   lstrcpy(g_szHelpAdapter, szOption);

   g_dwStressDelay = GetOptionLong(&argc, argv, g_dwStressDelay, _T("delay"));
   g_bNoUnbind = GetOptionFlag(&argc, argv, _T("nounbind"));
   g_bNoStress = GetOptionFlag(&argc, argv, _T("nostress"));
   g_dwBeatDelay = GetOptionLong(&argc, argv, g_dwBeatDelay, _T("beat"));
   g_bLogPackets = GetOptionFlag(&argc, argv, _T("packets"));

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

   // Set option and try bind/unbind from adapter to get prefered medium
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

   hr = NDTUnbind(hTestAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailUnbind, hr);
      goto cleanUp;
   }
   
   // Initialize test adapter
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
      
   // Set option and try bind/unbind from adapter
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

   hr = NDTUnbind(hHelpAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailUnbind, hr);
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

   // Test adapter
   if (!g_bNoUnbind) {
      hr = NDTBindProtocol(g_szTestAdapter, NULL);
      if (FAILED(hr)) NDTLogErr(g_szFailBindProtocol, g_szTestAdapter, hr);
      hr = NDTBindProtocol(g_szHelpAdapter, NULL);
      if (FAILED(hr)) NDTLogErr(g_szFailBindProtocol, g_szHelpAdapter, hr);
   }
   
   hr = NDTCleanup(NDTSystem(g_szHelpAdapter));
   if (FAILED(hr)) NDTLogErr(g_szFailCleanup, g_szHelpAdapter, hr);
   hr = NDTCleanup(NDTSystem(g_szTestAdapter));
   if (FAILED(hr)) NDTLogErr(g_szFailCleanup, g_szTestAdapter, hr);
   
   return TRUE;
}

//------------------------------------------------------------------------------
