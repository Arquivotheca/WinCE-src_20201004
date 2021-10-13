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
#include "NDTLog.h"
#include "NDTLib.h"
#include "NDTLibEx.h"
#include "ndt_2c.h"
#include "utils.h"

//------------------------------------------------------------------------------

TCHAR * GetFilterSettingInStr(ULONG ulFilter, TCHAR * tszFilter);

TEST_FUNCTION(TestFilterReceive)
{
   TEST_ENTRY;

   INT rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;

   HANDLE hSendAdapter = NULL;
   HANDLE hSend = NULL;
   ULONG  ulSendPacketsSent = 0;
   ULONG  ulSendPacketsCompleted = 0;
   ULONG  ulSendTime = 0;
   ULONG  ulSendBytesSent = 0;

   UINT   chRecvAdapter = 8;
   HANDLE ahRecvAdapter[8];
   HANDLE ahReceive[8];
   ULONG  aulRecvPacketsReceived[8];
   ULONG  aulRecvTime[8];
   ULONG  aulRecvBytesRecv[8];

   UINT   cbAddr = 0;
   UINT   cbHeader = 0;
   DWORD  dwReceiveDelay = 0;   
   UINT   uiMinimumPacketSize = 64;
   UINT   uiMaximumPacketSize = 0;

   UINT   cpucDestAddr = 8;
   UINT   ixDestAddr = 0;
   
   typedef struct {
	   TCHAR szDestAddrDesc[64];
	   UCHAR* apucDestAddr;
   } tsDestAddr;

   tsDestAddr DestAddr[8];

   UINT   nFilters = 0;
   UINT   nFiltersCombinations = 0;
   UINT   uiAvaiableFilters = 0;
   ULONG  aulFilter[8];                         // Filter value
   ULONG  aulActiveFilter[8];                   // Filter active on binding
   ULONG  aulReceiveFilter[8];                  // What should pass for address
   ULONG  ulFilter = 0;

   UCHAR* pucPermanentAddr = NULL;
   UCHAR* pucRandomAddr = NULL;
   UCHAR* pucBroadcastAddr = NULL;
   UCHAR* pucMulticastAddr = NULL;
   UCHAR* pucBadMulticastAddr = NULL;
   UCHAR* pucGroupAddr = NULL;

   UINT   uiPacketSize = 128;
   UINT   uiPacketsToSend = 300;
   BOOL   bFixedSize = FALSE;

   UINT   nPassesRequired = 0;
   UINT   ixPass = 0;
   UINT   ixFilter = 0;
   UINT   ixAdapter = 0;
   UINT   ix = 0;


   // Intro
   NDTLogMsg(_T("Start 2c_FilterReceive test"));
   NDTLogMsg(_T("The test adapter is %s"), g_szTestAdapter);
   NDTLogMsg(_T("The support adapter is %s"), g_szHelpAdapter);

   // Zero local variables
   memset(ahRecvAdapter, 0, sizeof(ahRecvAdapter));
   memset(ahReceive, 0, sizeof(ahReceive));
   memset(DestAddr, 0, sizeof(DestAddr));

   // Get some information about a media
   NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);
   dwReceiveDelay = NDTGetReceiveDelay(ndisMedium);

   pucRandomAddr = NDTGetRandomAddr(ndisMedium);
   pucBroadcastAddr = NDTGetBroadcastAddr(ndisMedium);
   pucMulticastAddr = NDTGetMulticastAddr(ndisMedium);
   pucBadMulticastAddr = NDTGetBadMulticastAddr(ndisMedium);
   pucGroupAddr = NDTGetGroupAddr(ndisMedium);

   // Open adapters
   NDTLogMsg(_T("Opening adapters"));
   
   // Test
   for (ixAdapter = 0; ixAdapter < chRecvAdapter; ixAdapter++) {
      hr = NDTOpen(g_szTestAdapter, &ahRecvAdapter[ixAdapter]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
         rc = TPR_FAIL;
         goto cleanUp;
      }
   }
   
   // Support
   hr = NDTOpen(g_szHelpAdapter, &hSendAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailOpen, g_szHelpAdapter, hr);
      rc = TPR_FAIL;
      goto cleanUp;
   }

   // Binding adapters
   NDTLogMsg(_T("Binding adapters"));
   for (ixAdapter = 0; ixAdapter < chRecvAdapter; ixAdapter++) {
      hr = NDTBind(ahRecvAdapter[ixAdapter], bForce30, ndisMedium);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailBind, hr);
         rc = TPR_FAIL;
         goto cleanUp;
      }
   }
   hr = NDTBind(hSendAdapter, bForce30, ndisMedium);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailBind, hr);
      rc = TPR_FAIL;
      goto cleanUp;
   }
   

   // Get basic information
   NDTLogMsg(_T("Get basic adapters info"));

   hr = NDTGetMaximumFrameSize(hSendAdapter, &uiMaximumPacketSize);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetMaximumFrameSize, hr);
      rc = TPR_FAIL;
      goto cleanUp;
   }

   hr = NDTGetPermanentAddr(ahRecvAdapter[0], ndisMedium, &pucPermanentAddr);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetPermanentAddr, hr);
      rc = TPR_FAIL;
      goto cleanUp;
   }

   // Get avaiable filters
   hr = NDTGetFilters(ahRecvAdapter[0], ndisMedium, &uiAvaiableFilters);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetFilters, hr);
      rc = TPR_FAIL;
      goto cleanUp;
   }

   NDTLogMsg(_T("Possible filter settings:"));
   NDTLogMsg(_T("  - Directed"));
   NDTLogMsg(_T("  - Broadcast"));
   aulFilter[0] = NDT_FILTER_DIRECTED;
   aulFilter[1] = NDT_FILTER_BROADCAST;
   nFilters = 2;
   nFiltersCombinations = 4;

   DestAddr[0].apucDestAddr = pucPermanentAddr;
   _tcscpy(DestAddr[0].szDestAddrDesc,_T("Test NIC's directed Addr"));
   aulReceiveFilter[0]  = NDT_FILTER_DIRECTED;
   aulReceiveFilter[0] |= NDT_FILTER_PROMISCUOUS;
   
   DestAddr[1].apucDestAddr = pucBroadcastAddr;
   _tcscpy(DestAddr[1].szDestAddrDesc,_T("Broadcast Addr"));
   aulReceiveFilter[1]  = NDT_FILTER_BROADCAST;
   aulReceiveFilter[1] |= NDT_FILTER_PROMISCUOUS;
   cpucDestAddr = 2;

   // If promiscuous mode is supported extend filters, adresses etc.
   if ((uiAvaiableFilters & NDT_FILTER_PROMISCUOUS) != 0) {
      NDTLogMsg(_T("  - Promiscuous"));
      aulFilter[nFilters++] = NDT_FILTER_PROMISCUOUS;
      nFiltersCombinations *= 2;

	  DestAddr[cpucDestAddr].apucDestAddr = pucRandomAddr;
	  _tcscpy(DestAddr[cpucDestAddr].szDestAddrDesc,_T("Any directed Random Addr"));
	  cpucDestAddr++;

      aulReceiveFilter[2] = NDT_FILTER_PROMISCUOUS;
   }

   if ((uiAvaiableFilters & NDT_FILTER_MULTICAST) != 0) {
      NDTLogMsg(_T("  - Multicast"));
      aulFilter[nFilters++] = NDT_FILTER_MULTICAST;
      nFiltersCombinations *= 2;
      if ((uiAvaiableFilters & NDT_FILTER_ALL_MULTICAST) != 0) {
         NDTLogMsg(_T("  - All Multicast"));
         aulFilter[nFilters++] = NDT_FILTER_ALL_MULTICAST;
         nFiltersCombinations *= 2;
      }

	  DestAddr[cpucDestAddr].apucDestAddr = pucMulticastAddr;
	  _tcscpy(DestAddr[cpucDestAddr].szDestAddrDesc,_T("Multicast Addr"));
      aulReceiveFilter[cpucDestAddr] = NDT_FILTER_MULTICAST;
      aulReceiveFilter[cpucDestAddr] |= NDT_FILTER_ALL_MULTICAST;
      aulReceiveFilter[cpucDestAddr] |= NDT_FILTER_PROMISCUOUS;
      cpucDestAddr++;
      
	  DestAddr[cpucDestAddr].apucDestAddr = pucBadMulticastAddr;
	  _tcscpy(DestAddr[cpucDestAddr].szDestAddrDesc,_T("Bad Multicast Addr"));
      aulReceiveFilter[cpucDestAddr] = NDT_FILTER_ALL_MULTICAST;
      aulReceiveFilter[cpucDestAddr] |= NDT_FILTER_PROMISCUOUS;
      cpucDestAddr++;
   }

   nPassesRequired = (nFiltersCombinations + chRecvAdapter - 1)/chRecvAdapter;

   // Repeat for all dest addresses         
   for (ixDestAddr = 0; ixDestAddr < cpucDestAddr; ixDestAddr++) {

      // We need so much passed for each dest address
      for (ixPass = 0; ixPass < nPassesRequired; ixPass++) {

         NDTLogMsg(_T("Setting receiving filters"));
         for (ixAdapter = 0; ixAdapter < chRecvAdapter; ixAdapter++) {

			TCHAR szFilter[256];

            // Create a filter
            ulFilter = 0;
            ix = (ixPass * chRecvAdapter + ixAdapter) % nFiltersCombinations;
            for (ixFilter = 0; ixFilter < 8; ixFilter++) {
               if ((ix & (0x01 << ixFilter)) != 0) {
                  ulFilter |= aulFilter[ixFilter];
               }
            }

            // Set it
            hr = NDTSetPacketFilter(ahRecvAdapter[ixAdapter], ulFilter);
            if (FAILED(hr)) {
               NDTLogErr(g_szFailSetPacketFilter, hr);
               rc = TPR_FAIL;
               goto cleanUp;
            }

            // Set filter parametes
            if (ulFilter & NDT_FILTER_MULTICAST) {
               hr = NDTAddMulticastAddr(
                  ahRecvAdapter[ixAdapter], ndisMedium, pucMulticastAddr
               );
               if (FAILED(hr)) {
                  NDTLogErr(g_szFailAddMulticastAddr, hr);
                  rc = TPR_FAIL;
                  goto cleanUp;
               }
            }

            // TODO: Other filter types addresses
            
            // Save it for later use
            aulActiveFilter[ixAdapter] = ulFilter;

            // Log
            NDTLogMsg(
               _T("Set filter %s for instance %d "), GetFilterSettingInStr(ulFilter,szFilter), ixAdapter
            );

         }

         // Start receive
         NDTLogMsg(_T("Start receiving"));
         for (ixAdapter = 0; ixAdapter < chRecvAdapter; ixAdapter++) {
            hr = NDTReceive(ahRecvAdapter[ixAdapter], &ahReceive[ixAdapter]);
            if (FAILED(hr)) {
               NDTLogErr(g_szFailReceive, hr);
               rc = TPR_FAIL;
               goto cleanUp;
            }
         }

         // Start sendings
         NDTLogMsg(
            _T("Sending %d packets with size %d bytes to %s"), 
            uiPacketsToSend, uiPacketSize,DestAddr[ixDestAddr].szDestAddrDesc
         );
         hr = NDTSend(
            hSendAdapter, cbAddr, NULL, 1, &(DestAddr[ixDestAddr].apucDestAddr), 
            NDT_RESPONSE_NONE, NDT_PACKET_TYPE_FIXED, uiPacketSize, 
            uiPacketsToSend, 0, 0, &hSend
         );
         if (FAILED(hr)) {
            NDTLogErr(g_szFailSend, hr);
            rc = TPR_FAIL;
            goto cleanUp;
         }

         // Wait till sending is done
         hr = NDTSendWait(
            hSendAdapter, hSend, INFINITE, &ulSendPacketsSent, 
            &ulSendPacketsCompleted, NULL, NULL, NULL, &ulSendTime, 
            &ulSendBytesSent, NULL
         );
         if (FAILED(hr)) {
            NDTLogErr(g_szFailSendWait, hr);
            rc = TPR_FAIL;
            goto cleanUp;
         }

         // Wait for while
         Sleep(dwReceiveDelay);

         // Stop receiving and check results
         NDTLogMsg(_T("Stop receiving and checking results"));
         for (ixAdapter = 0; ixAdapter < chRecvAdapter; ixAdapter++) {
            // Stop receiving
            hr = NDTReceiveStop(
               ahRecvAdapter[ixAdapter], ahReceive[ixAdapter],
               &aulRecvPacketsReceived[ixAdapter], NULL, NULL, 
               &aulRecvTime[ixAdapter], NULL, &aulRecvBytesRecv[ixAdapter]
            );
            if (FAILED(hr)) {
               NDTLogErr(g_szFailReceiveStop, hr);
               rc = TPR_FAIL;
               goto cleanUp;
            }

            if ((aulActiveFilter[ixAdapter] & aulReceiveFilter[ixDestAddr])) {
               if (NotReceivedOK(aulRecvPacketsReceived[ixAdapter],uiPacketsToSend)) {
                  NDTLogErr(
                     _T("Instance %d received %d packets from %d expected"),
                     ixAdapter, aulRecvPacketsReceived[ixAdapter], 
                     uiPacketsToSend
                  );
                  rc = TPR_FAIL;
               } else {
                  NDTLogMsg(
                     _T("Instance %d received expected %d packets"),
                     ixAdapter, aulRecvPacketsReceived[ixAdapter]
                  );
               }
            } else {
               if (aulRecvPacketsReceived[ixAdapter] != 0) {
                  NDTLogErr(
                     _T("Instance %d improperly received %d packets"),
                     ixAdapter, aulRecvPacketsReceived[ixAdapter]
                  );
                  rc = TPR_FAIL;
               } else {
                  NDTLogMsg(
                     _T("Instance %d properly didn't receive packets"),
                     ixAdapter
                  );
               }
            }
         }

         // Reset filters
         for (ixAdapter = 0; ixAdapter < chRecvAdapter; ixAdapter++) {
            // Get set filter
            ulFilter = aulActiveFilter[ixAdapter];
            // Remove filter parametes
            if (ulFilter & NDT_FILTER_MULTICAST) {
               hr = NDTDeleteMulticastAddr(
                  ahRecvAdapter[ixAdapter], ndisMedium, pucMulticastAddr
               );
               if (FAILED(hr)) {
                  NDTLogErr(g_szFailDeleteMulticastAddr, hr);
                  rc = TPR_FAIL;
                  goto cleanUp;
               }
            }
            // Clear filter itself
            hr = NDTSetPacketFilter(ahRecvAdapter[ixAdapter], 0);
            if (FAILED(hr)) {
               NDTLogErr(g_szFailSetPacketFilter, hr);
               rc = TPR_FAIL;
               goto cleanUp;
            }
            aulActiveFilter[ixAdapter] = 0;
         }
         
      }
      
   }

   // Unbinding adapters
   NDTLogMsg(_T("Unbinding adapters"));
   for (ixAdapter = 0; ixAdapter < chRecvAdapter; ixAdapter++) {
      hr = NDTUnbind(ahRecvAdapter[ixAdapter]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailUnbind, hr);
         rc = TPR_FAIL;
         goto cleanUp;
      }
   }
   hr = NDTUnbind(hSendAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailUnbind, hr);
      rc = TPR_FAIL;
      goto cleanUp;
   }
   

cleanUp:
   for (ixAdapter = 0; ixAdapter < chRecvAdapter; ixAdapter++) {
      hr = NDTClose(&ahRecvAdapter[ixAdapter]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailClose, hr);
         rc = TPR_FAIL;
      }
   }
   hr = NDTClose(&hSendAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailClose, hr);
      rc = TPR_FAIL;
   }      

   delete pucPermanentAddr;
   delete pucRandomAddr;
   delete pucBroadcastAddr;
   delete pucMulticastAddr;
   delete pucBadMulticastAddr;
   delete pucGroupAddr;
   
   return rc;
}

TCHAR * GetFilterSettingInStr(ULONG ulFilter, TCHAR * tszFilter)
{
	if (!tszFilter)
		return tszFilter;

	tszFilter[0] = _T('\0');

	if (ulFilter & NDT_FILTER_DIRECTED)
		_tcscat(tszFilter, _T("FILTER_DIRECTED+"));
	if (ulFilter & NDT_FILTER_MULTICAST)
		_tcscat(tszFilter, _T("FILTER_MULTICAST+"));
	if (ulFilter & NDT_FILTER_ALL_MULTICAST)
		_tcscat(tszFilter, _T("FILTER_ALL_MULTICAST+"));
	if (ulFilter & NDT_FILTER_BROADCAST)
		_tcscat(tszFilter, _T("FILTER_BROADCAST+"));
	if (ulFilter & NDT_FILTER_SOURCE_ROUTING)
		_tcscat(tszFilter, _T("FILTER_SOURCE_ROUTING+"));
	if (ulFilter & NDT_FILTER_PROMISCUOUS)
		_tcscat(tszFilter, _T("FILTER_PROMISCUOUS+"));
	if (ulFilter & NDT_FILTER_SMT)
		_tcscat(tszFilter, _T("FILTER_SMT+"));
	if (ulFilter & NDT_FILTER_GROUP_PKT)
		_tcscat(tszFilter, _T("FILTER_GROUP_PKT+"));
	if (ulFilter & NDT_FILTER_ALL_FUNCTIONAL)
		_tcscat(tszFilter, _T("FILTER_ALL_FUNCTIONAL+"));
	if (ulFilter & NDT_FILTER_FUNCTIONAL)
		_tcscat(tszFilter, _T("FILTER_FUNCTIONAL+"));
	if (ulFilter & NDT_FILTER_MAC_FRAME)
		_tcscat(tszFilter, _T("FILTER_MAC_FRAME+"));

	if (  tszFilter[0] == _T('\0')  )
		_tcscat(tszFilter, _T("FILTER_NONE"));

	return tszFilter;
}

//------------------------------------------------------------------------------
