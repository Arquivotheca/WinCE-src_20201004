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
#include <ndis.h>

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

   UCHAR** ppucMulticastAddrs = NULL;
   UCHAR*  pucMulticastAddrs = NULL;

   //Max number Mcast addresses supported by NIC.
   UINT  uiMaxListSize = 0;

   //Corrected Max number of Mcast addresses supported by NIC. Since Test restricts testing multicast addrs to 128,
   //if NIC supports more than that then uiMaxListSize gets truncated to 128.
   UINT  uiMaxListSize2 = 0;

   //Actual number of Mcast addresses Test could set. If -nounbind is used then TCP/IP, TCP/IP6, NDISUIO would have been
   //bound to NIC & actual Mcast addresses test could use would be less than max.
   UINT  uiMaxListSizeSet = 0;
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
      hr = E_FAIL;
      NDTLogErr(_T("List size may not be equal to zero"));
      goto cleanUp;
   }

   if (uiMaxListSize < 32) {
      NDTLogWrn(_T("Driver must support at least 32 multicast addresses"));
   }

   NDTLogMsg(_T("Driver support %d address in multicast list"), uiMaxListSize);

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

   uiMaxListSize2 = uiMaxListSizeSet = uiMaxListSize;
   if (uiMaxListSize > 128 && !g_bFullMulti)
       uiMaxListSize2 = uiMaxListSizeSet = 128;

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
      hr = E_FAIL;
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
         hr = E_FAIL; 
         NDTLogErr(_T("Multicast list should only have 1 entry"));
         goto cleanUp;
      }

      if (memcmp(ppucMulticastAddrs[0], pucMulticastAddrs, cbAddr) != 0) {
         hr = E_FAIL;
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
         hr = E_FAIL;
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
          //if run with -nounbind then TCP/IP, TCPIP6 must have set their own Mcast addresses.
          //if we get NDIS_STATUS_MULTICAST_FULL then its OK.
          if ((ix) && (g_bNoUnbind) && (NDIS_FROM_HRESULT(hr) == NDIS_STATUS_MULTICAST_FULL))
          {
              uiMaxListSizeSet = ix;
              NDTLogMsg(_T("NDIS_STATUS_MULTICAST_FULL while adding %d Multicast addr."), (uiMaxListSizeSet+1));
              break;
          }
          else
          {
              NDTLogErr(g_szFailAddMulticastAddr, hr);
              goto cleanUp;
          }
      }

      hr = NDTQueryInfo(
         ahAdapter[0], oidList, pucMulticastAddrs, uiMaxListSizeSet * cbAddr, 
         &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed != (ix + 1) * cbAddr) {
         hr = E_FAIL;
         NDTLogErr(_T("But adapter report back %d addresses"), uiUsed/cbAddr);
         goto cleanUp;
      }
   }

   NDTLogMsg(_T("Delete all these added multicast addresses"));
   for (ix = 0; ix < uiMaxListSizeSet; ix++) {
      hr = NDTDeleteMulticastAddr(
         ahAdapter[0], ndisMedium, ppucMulticastAddrs[ix]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailDeleteMulticastAddr, hr);
         goto cleanUp;
      }
      hr = NDTQueryInfo(
         ahAdapter[0], oidList, pucMulticastAddrs, uiMaxListSizeSet * cbAddr, 
         &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed != (uiMaxListSizeSet - ix - 1) * cbAddr) {
         hr = E_FAIL;
         NDTLogErr(
            _T("Multicast list should have %d entries"), uiMaxListSizeSet - ix - 1
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
         hr = E_FAIL;
         NDTLogErr(_T("Multicast list should only have 1 entry"));
         goto cleanUp;
      }
      if (memcmp(ppucMulticastAddrs[0], pucMulticastAddrs, cbAddr) != 0) {
         hr = E_FAIL;
         NDTLogErr(_T("Incorrect multicast address returned"));
         goto cleanUp;
      }
      hr = NDTQueryInfo(
         ahAdapter[1], oidList, pucMulticastAddrs, uiMaxListSize * cbAddr, 
         &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed != cbAddr) {
         hr = E_FAIL;
         NDTLogErr(_T("Multicast list should only have 1 entry"));
         goto cleanUp;
      }
      if (memcmp(ppucMulticastAddrs[1], pucMulticastAddrs, cbAddr) != 0) {
         hr = E_FAIL;
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
         hr = E_FAIL;
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
         hr = E_FAIL;
         NDTLogErr(_T("Multicast list should be empty"));
         goto cleanUp;
      }
      
   }

   NDTLogMsg(
      _T("Add half of the maximum allowed list size multicast address ")
      _T("to each open instance")
   );
   for (ix = 0; ix < uiMaxListSizeSet/2; ix++) {
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
         hr = E_FAIL;
         NDTLogErr(_T("Multicast list should have %d entries"), 2*(ix + 1));
         goto cleanUp;
      }

      hr = NDTQueryInfo(
         ahAdapter[(ix + 1) % 2], oidList, pucMulticastAddrs, 
         uiMaxListSize * cbAddr, &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed != (ix + 1)*cbAddr) {
         hr = E_FAIL;
         NDTLogErr(_T("Multicast list should have %d entries"), 2*(ix + 1));
         goto cleanUp;
      }
   }

   NDTLogMsg(_T("Delete all these added multicast addresses"));
   for (ix = 0; ix < uiMaxListSizeSet/2; ix++) {

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

      if (uiUsed != (uiMaxListSizeSet/2 - (ix + 1)) * cbAddr) {
         hr = E_FAIL;
         NDTLogErr(
            _T("Multicast list should have %d entries"), 
            uiMaxListSizeSet/2 - (ix + 1)
         );
         goto cleanUp;
      }
      hr = NDTQueryInfo(
         ahAdapter[(ix + 1)%2], oidList, pucMulticastAddrs, 
         uiMaxListSize * cbAddr, &uiUsed, NULL
      );
      if (FAILED(hr)) goto cleanUp;

      if (uiUsed != (uiMaxListSizeSet/2 - (ix + 1)) * cbAddr) {
         hr = E_FAIL;
         NDTLogErr(
            _T("Multicast list should have %d entries"), 
            uiMaxListSizeSet/2 - 2*(ix + 1)
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
