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
#include "ShellProc.h"
#include "NDTNdis.h"
#include "NDTMsgs.h"
#include "NDTError.h"
#include "NDTLog.h"
#include "NDTLib.h"
#include "NDTLibEx.h"
#include "ndt_1c.h"

//------------------------------------------------------------------------------

TEST_FUNCTION(TestOpenClose)
{
   TEST_ENTRY;
   
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;
   HANDLE hAdapter = NULL;
   HANDLE ahAdapter[128];
   UINT  nAdapters = 128;
   UINT  ixAdapter = 0;
   UINT  ix = 0;
   DWORD nValue = 0;

   memset(ahAdapter, 0, sizeof(ahAdapter));

   NDTLogMsg(
      _T("Start 1c_OpenClose test on the adapter %s"), g_szTestAdapter
   );

   NDTLogMsg(_T("Opening adapter"));
   hr = NDTOpen(g_szTestAdapter, &hAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
      goto cleanUp;
   }

   NDTLogMsg(_T("Repeatedly binding and unbinding the adapter"));

   for (ix = 0; ix < 16; ix++) {
      // Open 
      hr = NDTBind(hAdapter, bForce30, ndisMedium);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailBind, hr);
         goto cleanUp;
      }
      // Get events counter
      hr = NDTGetCounter(hAdapter, NDT_COUNTER_UNEXPECTED_EVENTS, &nValue);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailGetCounter, hr);
         goto cleanUp;
      }
      if (nValue != 0) {
         NDTLogErr(_T("Unexpected events occured - see a debug log output"));
         goto cleanUp;
      }
      hr = NDTUnbind(hAdapter);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailUnbind, hr);
         goto cleanUp;
      }
   }
      
   hr = NDTClose(&hAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailClose, hr);
      goto cleanUp;
   }

   NDTLogMsg(
      _T("Open and bind the adapter multiple times, ")
      _T("then close each open instance")
   );

   for (ix = 0; ix < 16; ix++) {

      NDTLogMsg(
         _T("Opening %d adapter instances (pass #%d)"), nAdapters, ix
      );

      // Open all adapters
      for (ixAdapter = 0; ixAdapter < nAdapters; ixAdapter++) {
          NDTLogMsg(
         _T("Opening instance %d (pass #%d)"), ixAdapter, ix
      );
         // Open
         hr = NDTOpen(g_szTestAdapter, &ahAdapter[ixAdapter]);
         if (FAILED(hr)) {
            NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
            goto cleanUp;
         }
         // Bind
         hr = NDTBind(ahAdapter[ixAdapter], bForce30, ndisMedium);
         if (FAILED(hr)) {
            NDTLogErr(g_szFailBind, hr);
            goto cleanUp;
         }
      }         

      NDTLogMsg(_T("Closing all adapter instances"));
      for (ixAdapter = 0; ixAdapter < nAdapters; ixAdapter++) {
          NDTLogMsg(
         _T("Closing instance %d (pass #%d)"), ixAdapter, ix
      );
         // Get events counter
         hr = NDTGetCounter(
            ahAdapter[ixAdapter], NDT_COUNTER_UNEXPECTED_EVENTS, &nValue
         );
         if (FAILED(hr)) {
            NDTLogErr(g_szFailGetCounter, hr);
            goto cleanUp;
         }
         if (nValue != 0) {
            NDTLogErr(_T("Unexpected events occure - see a debug log output"));
            goto cleanUp;
         }

         // Unbind
         hr = NDTUnbind(ahAdapter[ixAdapter]);
         if (FAILED(hr)) {
            NDTLogErr(g_szFailUnbind, hr);
            goto cleanUp;
         }

         // Close
         hr = NDTClose(&ahAdapter[ixAdapter]);
         if (FAILED(hr)) {
            NDTLogErr(g_szFailClose, hr);
            goto cleanUp;
         }
      }      
   }

   NDTLogMsg(
      _T("Test 1c_OpenClose on the adapter %s PASS"), g_szTestAdapter
   );
   return TPR_PASS;

cleanUp:
   if (hAdapter != NULL) {
      hr = NDTClose(&hAdapter);
      if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
   }   
   for (ixAdapter = 0; ixAdapter < nAdapters; ixAdapter++) {
      if (ahAdapter[ixAdapter] != NULL) {
         hr = NDTClose(&ahAdapter[ixAdapter]);
         if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
      }
   }
   NDTLogErr(
      _T("Test 1c_OpenClose on the adapter %s FAILED"), g_szTestAdapter
   );
   return TPR_FAIL;
}

//------------------------------------------------------------------------------
