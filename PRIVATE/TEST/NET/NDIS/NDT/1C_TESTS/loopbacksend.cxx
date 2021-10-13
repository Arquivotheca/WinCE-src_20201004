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

TEST_FUNCTION(TestLoopbackSend)
{
   TEST_ENTRY;

   INT rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bOk = TRUE;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;

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
   UCHAR* apucDestAddr[8];

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
   NDTLogMsg(_T("Start 1c_Loopback test"));
   NDTLogMsg(_T("The test adapter is %s"), g_szTestAdapter);

   // Zero local variables
   memset(ahRecvAdapter, 0, sizeof(ahRecvAdapter));
   memset(ahReceive, 0, sizeof(ahReceive));
   memset(apucDestAddr, 0, sizeof(apucDestAddr));

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
         bOk = FALSE;
         NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
         goto cleanUp;
      }
   }
   
   // Binding adapters
   NDTLogMsg(_T("Binding adapters"));
   for (ixAdapter = 0; ixAdapter < chRecvAdapter; ixAdapter++) {
      hr = NDTBind(ahRecvAdapter[ixAdapter], bForce30, ndisMedium);
      if (FAILED(hr)) {
         bOk = FALSE;
         NDTLogErr(g_szFailBind, hr);
          goto cleanUp;
      }
   }

   // Get basic information
   NDTLogMsg(_T("Get basic adapter info"));

   hr = NDTGetMaximumFrameSize(ahRecvAdapter[0], &uiMaximumPacketSize);
   if (FAILED(hr)) {
      bOk = FALSE;
      NDTLogErr(g_szFailGetMaximumFrameSize, hr);
      goto cleanUp;
   }

   hr = NDTGetPermanentAddr(ahRecvAdapter[0], ndisMedium, &pucPermanentAddr);
   if (FAILED(hr)) {
      bOk = FALSE;
      NDTLogErr(g_szFailGetPermanentAddr, hr);
      goto cleanUp;
   }

   // Get avaiable filters
   hr = NDTGetFilters(ahRecvAdapter[0], ndisMedium, &uiAvaiableFilters);
   if (FAILED(hr)) {
      bOk = FALSE;
      NDTLogErr(g_szFailGetFilters, hr);
      goto cleanUp;
   }

   NDTLogMsg(_T("Possible filter settings:"));
   NDTLogMsg(_T("  - Directed"));
   NDTLogMsg(_T("  - Broadcast"));
   aulFilter[0] = NDT_FILTER_DIRECTED;
   aulFilter[1] = NDT_FILTER_BROADCAST;
   nFilters = 2;
   nFiltersCombinations = 4;
   apucDestAddr[0] = pucPermanentAddr;
   aulReceiveFilter[0]  = NDT_FILTER_DIRECTED;
   aulReceiveFilter[0] |= NDT_FILTER_PROMISCUOUS;
   apucDestAddr[1] = pucBroadcastAddr;
   aulReceiveFilter[1]  = NDT_FILTER_BROADCAST;
   aulReceiveFilter[1] |= NDT_FILTER_PROMISCUOUS;
   cpucDestAddr = 2;

   // If promiscuous mode is supported extend filters, adresses etc.
   if ((uiAvaiableFilters & NDT_FILTER_PROMISCUOUS) != 0) {
      NDTLogMsg(_T("  - Promiscuous"));
      aulFilter[nFilters++] = NDT_FILTER_PROMISCUOUS;
      nFiltersCombinations *= 2;
      apucDestAddr[cpucDestAddr++] = pucRandomAddr;
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
      apucDestAddr[cpucDestAddr] = pucMulticastAddr;
      aulReceiveFilter[cpucDestAddr] = NDT_FILTER_MULTICAST;
      aulReceiveFilter[cpucDestAddr] |= NDT_FILTER_ALL_MULTICAST;
      aulReceiveFilter[cpucDestAddr] |= NDT_FILTER_PROMISCUOUS;
      cpucDestAddr++;
      apucDestAddr[cpucDestAddr] = pucBadMulticastAddr;
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
               bOk = FALSE;
               NDTLogErr(g_szFailSetPacketFilter, hr);
               goto cleanUp;
            }

            // Set filter parametes
            if (ulFilter & NDT_FILTER_MULTICAST) {
               hr = NDTAddMulticastAddr(
                  ahRecvAdapter[ixAdapter], ndisMedium, pucMulticastAddr
               );
               if (FAILED(hr)) {
                  bOk = FALSE;
                  NDTLogErr(g_szFailAddMulticastAddr, hr);
                  goto cleanUp;
               }
            }

            // TODO: Other filter types addresses
            
            // Save it for later use
            aulActiveFilter[ixAdapter] = ulFilter;

            // Log
            NDTLogMsg(
               _T("Set filter 0x%08x for instance %d "), ulFilter, ixAdapter
            );

         }

         // Start receive
         NDTLogMsg(_T("Start receiving"));
         for (ixAdapter = 0; ixAdapter < chRecvAdapter; ixAdapter++) {
            hr = NDTReceive(ahRecvAdapter[ixAdapter], &ahReceive[ixAdapter]);
            if (FAILED(hr)) {
               bOk = FALSE;
               NDTLogErr(g_szFailReceive, hr);
               goto cleanUp;
            }
         }

         // Start sendings
         NDTLogMsg(
            _T("Sending %d packets with size %d bytes"), 
            uiPacketsToSend, uiPacketSize
         );
         hr = NDTSend(
            ahRecvAdapter[0], cbAddr, NULL, 1, &apucDestAddr[ixDestAddr], 
            NDT_RESPONSE_NONE, NDT_PACKET_TYPE_FIXED, uiPacketSize, 
            uiPacketsToSend, 0, 0, &hSend
         );
         if (FAILED(hr)) {
            bOk = FALSE;
            NDTLogErr(g_szFailSend, hr);
            goto cleanUp;
         }

         // Wait till sending is done
         hr = NDTSendWait(
            ahRecvAdapter[0], hSend, INFINITE, &ulSendPacketsSent, 
            &ulSendPacketsCompleted, NULL, NULL, NULL, &ulSendTime, 
            &ulSendBytesSent, NULL
         );
         if (FAILED(hr)) {
            bOk = FALSE;
            NDTLogErr(g_szFailSendWait, hr);
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
               bOk = FALSE;
               NDTLogErr(g_szFailReceiveStop, hr);
               goto cleanUp;
            }

            if ((aulActiveFilter[ixAdapter] & aulReceiveFilter[ixDestAddr])) {
               if (aulRecvPacketsReceived[ixAdapter] != uiPacketsToSend) {
                  bOk = FALSE;
                  NDTLogErr(
                     _T("Instance %d received %d packets from %d expected"),
                     ixAdapter, aulRecvPacketsReceived[ixAdapter], 
                     uiPacketsToSend
                  );
               } else {
                  NDTLogMsg(
                     _T("Instance %d received expected %d packets"),
                     ixAdapter, aulRecvPacketsReceived[ixAdapter]
                  );
               }
            } else {
               if (aulRecvPacketsReceived[ixAdapter] != 0) {
                  bOk = FALSE;
                  NDTLogErr(
                     _T("Instance %d improperly received %d packets"),
                     ixAdapter, aulRecvPacketsReceived[ixAdapter]
                  );
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
                  bOk = FALSE;
                  NDTLogErr(g_szFailDeleteMulticastAddr, hr);
                  goto cleanUp;
               }
            }
            // Clear filter itself
            hr = NDTSetPacketFilter(ahRecvAdapter[ixAdapter], 0);
            if (FAILED(hr)) {
               bOk = FALSE;
               NDTLogErr(g_szFailSetPacketFilter, hr);
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
         bOk = FALSE;
         NDTLogErr(g_szFailUnbind, hr);
         goto cleanUp;
      }
   }

cleanUp:
   // We have deside about test pass/fail there
   rc = !bOk ? TPR_FAIL : TPR_PASS;

   for (ixAdapter = 0; ixAdapter < chRecvAdapter; ixAdapter++) {
      hr = NDTClose(&ahRecvAdapter[ixAdapter]);
      if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
   }

   delete pucPermanentAddr;
   delete pucRandomAddr;
   delete pucBroadcastAddr;
   delete pucMulticastAddr;
   delete pucBadMulticastAddr;
   delete pucGroupAddr;
   
   return rc;
}

//------------------------------------------------------------------------------
