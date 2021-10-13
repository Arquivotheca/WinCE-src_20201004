//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "ShellProc.h"
#include "NDTNdis.h"
#include "NDTMsgs.h"
#include "NDTError.h"
#include "NDTLog.h"
#include "NDTLib.h"
#include "NDTLibEx.h"
#include "ndt_1c.h"

//------------------------------------------------------------------------------

TEST_FUNCTION(TestFaultHandling)
{
   TEST_ENTRY;

   // Run this test only when flag says so - PC cards don't support driver
   // start/stop through NDIS commands
   if (!g_bFaultHandling) return TPR_SKIP;

   INT rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;
   HANDLE hAdapter = NULL;
   UINT  cbAddr = 0;
   UINT  cbHeader = 0;
   UINT  uiMaximumPacketSize = 0;
   DWORD adwVerifyFlag[8];
   UINT  ix = 0;
   LPTSTR szInfo = NULL;
   
   memset(adwVerifyFlag, 0, sizeof(adwVerifyFlag));

   // Let start
   NDTLogMsg(
      _T("Start 1c_FaultHandling test on the adapter %s"), g_szTestAdapter
   );

   // Open adapter
   NDTLogMsg(_T("Open adapter"));
   hr = NDTOpen(g_szTestAdapter, &hAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
      goto cleanUp;
   }
 
   adwVerifyFlag[0] = 0x0001;
   adwVerifyFlag[1] = 0x0002;
   adwVerifyFlag[2] = 0x0004;
   adwVerifyFlag[3] = 0x0010;
   adwVerifyFlag[4] = 0x0020;
   adwVerifyFlag[5] = 0x0040;
   adwVerifyFlag[6] = 0x0080;
   adwVerifyFlag[7] = 0x0100;

   NDTLogMsg(_T("StopDriver"));
   hr = NDTUnloadMiniport(hAdapter);
   if (FAILED(hr)) goto cleanUp;

   for (ix = 0; ix < 8; ix++) {

      hr = NDTWriteVerifyFlag(hAdapter, adwVerifyFlag[ix]);
      if (FAILED(hr)) goto cleanUp;

      NDTLogMsg(_T("StartDriver"));
      switch (ix) {
      case 0: szInfo = _T("NdisMallocateMapRegisters"); break;
      case 1: szInfo = _T("NdisMRegisterInterrupt"); break;
      case 2: szInfo = _T("NdisMAllocateSharedMemory"); break;
      case 3: szInfo = _T("NdisMMapIoSpace"); break;
      case 4: szInfo = _T("NdisMRegisterIoPortRange"); break;
      case 5: szInfo = _T("Read NdisGetSetBusConfigSpace"); break;
      case 6: szInfo = _T("Write NdisGetSetBusConfigSpace"); break;
      case 7: szInfo = _T("NdisMInitializeScatterGatherDma"); break;
      }
      NDTLogMsg(_T("Ndis will fail %s"), szInfo);

      hr = NDTLoadMiniport(hAdapter);
      if (FAILED(hr)) {
         NDTLogMsg(
            _T("Driver failed to load. This is expected because of ")
            _T("the failed NDIS call.")
         );
         NDTLogMsg(
            _T("The driver will now be loaded without any values written ")
            _T("into the registry, just to make sure it still works.")
         );

         hr = NDTDeleteVerifyFlag(hAdapter);
         if (FAILED(hr)) goto cleanUp;

         NDTLogMsg(_T("Restarting the driver"));
         hr = NDTLoadMiniport(hAdapter);
         if (FAILED(hr)) {
            NDTLogErr(
               _T("Driver failed load after the verify flag was removed.")
            );
            goto cleanUp;
         }
         
      } else {
         NDTLogMsg(
            _T("Driver loaded. It must not require the NDIS call that failed ")
            _T("in this loop of the test.")
         );
      }

      NDTLogMsg(_T("StopDriver"));
      hr = NDTUnloadMiniport(hAdapter);
      if (FAILED(hr)) goto cleanUp;
      
   }

cleanUp:
   // We have deside about test pass/fail there
   rc = FAILED(hr) ? TPR_FAIL : TPR_PASS;

   hr = NDTDeleteVerifyFlag(hAdapter);
   if (FAILED(hr)) NDTLogErr(g_szFailDeleteVerifyFlag, hr);
   hr = NDTLoadMiniport(hAdapter);
   if (FAILED(hr)) NDTLogErr(g_szFailLoadMiniport, hr);
   hr = NDTClose(&hAdapter);
   if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);

   return rc;
}

//------------------------------------------------------------------------------
