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

TEST_FUNCTION(TestSetMulticast)
{
   TEST_ENTRY;

   INT rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;

   UINT   nBindings = 2;
   HANDLE ahAdapter[2] = {NULL, NULL};
   UINT   ixBinding = 0;

   NDIS_OID oidSize = 0;
   NDIS_OID oidList = 0;
   
   UINT  cbAddr = 0;
   UINT  cbHeader = 0;

   UCHAR*  pucPermanentAddr = NULL;
   UCHAR** ppucMulticastAddrs = NULL;
   UCHAR*  pucMulticastAddrs = NULL;

   UINT  uiMaxListSize = 0;
   UINT  uiMaxListSize2 = 0;
   UINT  uiUsed = 0;
   UINT  ix = 0;

   // Let start
   NDTLogMsg(_T("Start 1c_SetMulticast test"));
   NDTLogMsg(_T("The test adapter is %s"), g_szTestAdapter);

   // Get some information about a media
   NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);
   switch (ndisMedium) {
   case NdisMedium802_3:
      oidSize = OID_802_3_MAXIMUM_LIST_SIZE;
      oidList = OID_802_3_MULTICAST_LIST;
      break;
   case NdisMediumFddi:
      oidSize = OID_FDDI_LONG_MAX_LIST_SIZE;
      oidList = OID_FDDI_LONG_MULTICAST_LIST;
      break;
   default:
      NDTLogMsg(_T("Test make no sense on this type of medium"));
      goto cleanUp;
   }

   // Open adapters
   NDTLogMsg(_T("Opening adapters"));
   for (ixBinding = 0; ixBinding < nBindings; ixBinding++) {
      hr = NDTOpen(g_szTestAdapter, &ahAdapter[ixBinding]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
         goto cleanUp;
      }
   }
      
   // Open adapters
   NDTLogMsg(_T("Binding to adapters"));
   for (ixBinding = 0; ixBinding < 2; ixBinding++) {
      hr = NDTBind(ahAdapter[ixBinding], bForce30, ndisMedium);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailBind, hr);
         goto cleanUp;
      }
   }

   NDTLogMsg(_T("GetMaxListSize"));
   hr = NDTQueryInfo(
      ahAdapter[0], oidSize, &uiMaxListSize, sizeof(UINT), NULL, NULL
   );
   if (FAILED(hr)) goto cleanUp;

   if (uiMaxListSize == 0) {
      NDTLogErr(_T("List size may not be equal to zero"));
      goto cleanUp;
   }

   if (uiMaxListSize < 32) {
      NDTLogWrn(_T("Driver must support at least 32 multicast addresses"));
   }

   NDTLogMsg(_T("Driver support %d address in multicast list"), uiMaxListSize);

   if (g_bNoUnbind) {
      NDTLogWrn(_T("Other protocols are not unbind the test can fail"));
      uiMaxListSize -= 1;
   }
   
   // Allocate a memory
   ppucMulticastAddrs = new PUCHAR[uiMaxListSize];
   if (ppucMulticastAddrs == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }

   pucMulticastAddrs = new UCHAR[uiMaxListSize * cbAddr];
   if (pucMulticastAddrs == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }

   uiMaxListSize2 = uiMaxListSize;
   if (uiMaxListSize > 128 && !g_bFullMulti) uiMaxListSize2 = 128;

   NDTLogMsg(_T("%d multicast addresses will be tested"), uiMaxListSize2); 

   // Create multicast addresses
   for (ix = 0; ix < uiMaxListSize2; ix++) {
      ppucMulticastAddrs[ix] = NDTGetMulticastAddrEx(ndisMedium, ix);
   }
   
   // Get a multicast addresses
   hr = NDTQueryInfo(
      ahAdapter[0], oidList, pucMulticastAddrs, uiMaxListSize * cbAddr, 
      &uiUsed, NULL
   );
   if (FAILED(hr)) goto cleanUp;

   if (uiUsed > 0) {
      NDTLogErr(_T("Multicast list should be empty"));
      goto cleanUp;
   }

   NDTLogMsg(_T("Add/Delete a multicast address multiple times"));
   for (ix = 0; ix < 24; ix++) {

      NDTLogMsg(_T("Add/Query Multicast"));
      hr = NDTAddMulticastAddr(ahAdapter[0], ndisMedium, ppucMulticastAddrs[0]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailAddMulticastAddr, hr);
         goto cleanUp;
      }

      hr = NDTQueryInfo(
         ahAdapter[0], oidList, pucMulticastAddrs, uiMaxListSize * cbAddr, 
         &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed != cbAddr) {
         NDTLogErr(_T("Multicast list should only have 1 entry"));
         goto cleanUp;
      }

      if (memcmp(ppucMulticastAddrs[0], pucMulticastAddrs, cbAddr) != 0) {
         NDTLogErr(_T("Incorrect multicast address returned"));
         goto cleanUp;
      }
      
      NDTLogMsg(_T("Delete/Query Multicast"));
      hr = NDTDeleteMulticastAddr(
         ahAdapter[0], ndisMedium, ppucMulticastAddrs[0]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailDeleteMulticastAddr, hr);
         goto cleanUp;
      }

      hr = NDTQueryInfo(
         ahAdapter[0], oidList, pucMulticastAddrs, uiMaxListSize * cbAddr, 
         &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed > 0) {
         NDTLogErr(_T("Multicast list should be empty"));
         goto cleanUp;
      }
   }

   NDTLogMsg(_T("Add as many multicast addresses as driver says it supports"));
   for (ix = 0; ix < uiMaxListSize2; ix++) {

      NDTLogMsg(_T("Adding multicast address %d"), ix + 1);

      hr = NDTAddMulticastAddr(
         ahAdapter[0], ndisMedium, ppucMulticastAddrs[ix]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailAddMulticastAddr, hr);
         goto cleanUp;
      }

      hr = NDTQueryInfo(
         ahAdapter[0], oidList, pucMulticastAddrs, uiMaxListSize * cbAddr, 
         &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed != (ix + 1) * cbAddr) {
         NDTLogErr(_T("But adapter report back %d addresses"), uiUsed/cbAddr);
         goto cleanUp;
      }
   }

   NDTLogMsg(_T("Delete all these added multicast addresses"));
   for (ix = 0; ix < uiMaxListSize2; ix++) {
      hr = NDTDeleteMulticastAddr(
         ahAdapter[0], ndisMedium, ppucMulticastAddrs[ix]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailDeleteMulticastAddr, hr);
         goto cleanUp;
      }
      hr = NDTQueryInfo(
         ahAdapter[0], oidList, pucMulticastAddrs, uiMaxListSize * cbAddr, 
         &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed != (uiMaxListSize2 - ix - 1) * cbAddr) {
         NDTLogErr(
            _T("Multicast list should have %d entries"), uiMaxListSize2 - ix - 1
         );
         goto cleanUp;
      }
   }

   NDTLogMsg(
      _T("Add/Delete a multicast address multiple times on two instances")
   );
   for (ix = 0; ix < 24; ix++) {

      NDTLogMsg(_T("Add/Query Multicast"));
      hr = NDTAddMulticastAddr(ahAdapter[0], ndisMedium, ppucMulticastAddrs[0]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailAddMulticastAddr, hr);
         goto cleanUp;
      }
      hr = NDTAddMulticastAddr(ahAdapter[1], ndisMedium, ppucMulticastAddrs[1]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailAddMulticastAddr, hr);
         goto cleanUp;
      }
      hr = NDTQueryInfo(
         ahAdapter[0], oidList, pucMulticastAddrs, uiMaxListSize * cbAddr, 
         &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed != cbAddr) {
         NDTLogErr(_T("Multicast list should only have 1 entry"));
         goto cleanUp;
      }
      if (memcmp(ppucMulticastAddrs[0], pucMulticastAddrs, cbAddr) != 0) {
         NDTLogErr(_T("Incorrect multicast address returned"));
         goto cleanUp;
      }
      hr = NDTQueryInfo(
         ahAdapter[1], oidList, pucMulticastAddrs, uiMaxListSize * cbAddr, 
         &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed != cbAddr) {
         NDTLogErr(_T("Multicast list should only have 1 entry"));
         goto cleanUp;
      }
      if (memcmp(ppucMulticastAddrs[1], pucMulticastAddrs, cbAddr) != 0) {
         NDTLogErr(_T("Incorrect multicast address returned"));
         goto cleanUp;
      }
      
      NDTLogMsg(_T("Delete/Query Multicast"));
      hr = NDTDeleteMulticastAddr(
         ahAdapter[ix % 2], ndisMedium, ppucMulticastAddrs[ix % 2]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailDeleteMulticastAddr, hr);
         goto cleanUp;
      }
      hr = NDTQueryInfo(
         ahAdapter[ix % 2], oidList, pucMulticastAddrs, 
         uiMaxListSize * cbAddr, &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed > 0) {
         NDTLogErr(_T("Multicast list should be empty"));
         goto cleanUp;
      }
      hr = NDTDeleteMulticastAddr(
         ahAdapter[(ix + 1) % 2], ndisMedium, ppucMulticastAddrs[(ix + 1) % 2]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailDeleteMulticastAddr, hr);
         goto cleanUp;
      }
      hr = NDTQueryInfo(
         ahAdapter[(ix + 1) % 2], oidList, pucMulticastAddrs, 
         uiMaxListSize * cbAddr, &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed > 0) {
         NDTLogErr(_T("Multicast list should be empty"));
         goto cleanUp;
      }
      
   }

   NDTLogMsg(
      _T("Add half of the maximum list size multicast address ")
      _T("to each open instance")
   );
   for (ix = 0; ix < uiMaxListSize2/2; ix++) {
      hr = NDTAddMulticastAddr(
         ahAdapter[ix % 2], ndisMedium, ppucMulticastAddrs[ix*2]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailAddMulticastAddr, hr);
         goto cleanUp;
      }
      hr = NDTAddMulticastAddr(
         ahAdapter[(ix + 1) % 2], ndisMedium, ppucMulticastAddrs[ix*2 + 1]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailAddMulticastAddr, hr);
         goto cleanUp;
      }
      hr = NDTQueryInfo(
         ahAdapter[ix % 2], oidList, pucMulticastAddrs, 
         uiMaxListSize * cbAddr, &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed != (ix + 1)*cbAddr) {
         NDTLogErr(_T("Multicast list should have %d entries"), 2*(ix + 1));
         goto cleanUp;
      }

      hr = NDTQueryInfo(
         ahAdapter[(ix + 1) % 2], oidList, pucMulticastAddrs, 
         uiMaxListSize * cbAddr, &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed != (ix + 1)*cbAddr) {
         NDTLogErr(_T("Multicast list should have %d entries"), 2*(ix + 1));
         goto cleanUp;
      }
   }

   NDTLogMsg(_T("Delete all these added multicast addresses"));
   for (ix = 0; ix < uiMaxListSize2/2; ix++) {

      hr = NDTDeleteMulticastAddr(
         ahAdapter[ix%2], ndisMedium, ppucMulticastAddrs[ix*2]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailDeleteMulticastAddr, hr);
         goto cleanUp;
      }

      hr = NDTDeleteMulticastAddr(
         ahAdapter[(ix + 1)%2], ndisMedium, ppucMulticastAddrs[ix*2 + 1]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailDeleteMulticastAddr, hr);
         goto cleanUp;
      }

      hr = NDTQueryInfo(
         ahAdapter[ix%2], oidList, pucMulticastAddrs, 
         uiMaxListSize * cbAddr, &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed != (uiMaxListSize2/2 - (ix + 1)) * cbAddr) {
         NDTLogErr(
            _T("Multicast list should have %d entries"), 
            uiMaxListSize2/2 - (ix + 1)
         );
         goto cleanUp;
      }
      hr = NDTQueryInfo(
         ahAdapter[(ix + 1)%2], oidList, pucMulticastAddrs, 
         uiMaxListSize * cbAddr, &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed != (uiMaxListSize2/2 - (ix + 1)) * cbAddr) {
         NDTLogErr(
            _T("Multicast list should have %d entries"), 
            uiMaxListSize2/2 - 2*(ix + 1)
         );
         goto cleanUp;
      }
   }

   NDTLogMsg(_T("Unbinding adapters"));
   for (ixBinding = 0; ixBinding < nBindings; ixBinding++) {
      hr = NDTUnbind(ahAdapter[ixBinding]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailUnbind, hr);
         goto cleanUp;
      }
   }

cleanUp:
   // We have deside about test pass/fail there
   rc = FAILED(hr) ? TPR_FAIL : TPR_PASS;

   NDTLogMsg(_T("Closing adapters"));
   for (ixBinding = 0; ixBinding < nBindings; ixBinding++) {
      hr = NDTClose(&ahAdapter[ixBinding]);
      if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
   }
   delete [] pucMulticastAddrs;
   for (ix = 0; ix < uiMaxListSize2; ix++) delete ppucMulticastAddrs[ix];
   delete [] ppucMulticastAddrs;

   return rc;
}

//------------------------------------------------------------------------------
