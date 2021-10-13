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

   g_dwStressDelay = GetOptionLong(&argc, argv, g_dwStressDelay, _T("delay"));
   g_bNoUnbind = GetOptionFlag(&argc, argv, _T("nounbind"));
   g_bFaultHandling = GetOptionFlag(&argc, argv, _T("fault"));
   g_bForceCancel = GetOptionFlag(&argc, argv, _T("cancel"));
   g_bNoStress = GetOptionFlag(&argc, argv, _T("nostress"));
   g_bFullMulti = GetOptionFlag(&argc, argv, _T("fullmulti"));
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

   hr = NDTBind(hAdapter, FALSE, g_ndisMedium, &g_ndisMedium);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailBind, hr);
      goto cleanUp;
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
   HANDLE hAdapter = NULL;

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
