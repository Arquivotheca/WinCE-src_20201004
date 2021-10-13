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

TEST_FUNCTION(TestLoopbackStress)
{
   TEST_ENTRY;

   // Skip test if required
   if (g_bNoStress) return TPR_SKIP;

   INT rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;

   UINT   nAdapters = 8;
   HANDLE ahAdapter[8];
   HANDLE ahSend[8];
   HANDLE ahReceive[8];
   
   ULONG aulPacketsSent[8];
   ULONG aulPacketsReceived[8];
   ULONG aulPacketsCompleted[8];
   ULONG aulPacketsCanceled[8]; 
   ULONG aulPacketsUncanceled[8];
   ULONG aulPacketsReplied[8];
   ULONG aulTime[8];
   ULONG aulBytesSent[8];
   ULONG aulBytesReceived[8];

   UINT  ixAdapter = 0;
   ULONG ulFilter = 0;
   
   UINT  cbAddr = 0;
   UINT  cbHeader = 0;

   UINT  uiMaximumPacketSize = 0;
   DWORD dwReceiveDelay = 0;   

   BYTE* pucPermanentAddr = NULL;
   
   UINT  auiPacketsToSend[16];
   BYTE  aucReponseMode[16];
   BYTE  aucPacketSizeMode[16];
   UINT  auiPacketSize[16];
   UINT  auiBeatDelay[16];
   UINT  auiBeatGroup[16];
   UINT  ixTest = 0;
   
   UINT  ui = 0;

   // Zero local variables
   memset(ahAdapter, 0, sizeof(ahAdapter));
   memset(ahSend, 0, sizeof(ahSend));
   memset(ahReceive, 0, sizeof(ahReceive));
   memset(aulPacketsSent, 0, sizeof(aulPacketsSent));
   memset(aulPacketsReceived, 0, sizeof(aulPacketsReceived));
   memset(aulPacketsCompleted, 0, sizeof(aulPacketsCompleted));
   memset(aulPacketsCanceled, 0, sizeof(aulPacketsCanceled));
   memset(aulPacketsUncanceled, 0, sizeof(aulPacketsUncanceled));
   memset(aulPacketsReplied, 0, sizeof(aulPacketsReplied));
   memset(aulTime, 0, sizeof(aulTime));
   memset(aulBytesReceived, 0, sizeof(aulBytesReceived));
   memset(aulBytesSent, 0, sizeof(aulBytesSent));
   memset(auiPacketsToSend, 0, sizeof(auiPacketsToSend));
   memset(aucReponseMode, 0, sizeof(aucReponseMode));
   memset(aucPacketSizeMode, 0, sizeof(aucPacketSizeMode));
   memset(auiPacketSize, 0, sizeof(auiPacketSize));
   memset(auiBeatDelay, 0, sizeof(auiBeatDelay));
   memset(auiBeatGroup, 0, sizeof(auiBeatGroup));
   

   // Let start
   NDTLogMsg(
      _T("Start 1c_LoopbackStess test on the adapter %s"), g_szTestAdapter
   );

   
   // Getting informations about an adapter
   NDTLogMsg(_T("Getting informations about the adapter"));

   // Get some information about a media
   NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);

   // Open 
   NDTLogMsg(_T("Opening adapters"));
   for (ixAdapter = 0; ixAdapter < nAdapters; ixAdapter++) {
      hr = NDTOpen(g_szTestAdapter, &ahAdapter[ixAdapter]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
         goto cleanUp;
      }
   }

   // Bind
   hr = NDTBind(ahAdapter[0], bForce30, ndisMedium);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailBind, hr);
      goto cleanUp;
   }

   // Get maximum frame size
   hr = NDTGetMaximumFrameSize(ahAdapter[0], &uiMaximumPacketSize);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetMaximumFrameSize, hr);
      goto cleanUp;
   }

   // Get a permanent address & other media specific adresses
   hr = NDTGetPermanentAddr(ahAdapter[0], ndisMedium, &pucPermanentAddr);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetPermanentAddr, hr);
      goto cleanUp;
   }
   dwReceiveDelay = NDTGetReceiveDelay(ndisMedium);
   
   // Close
   hr = NDTUnbind(ahAdapter[0]);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailUnbind, hr);
      goto cleanUp;
   }

   //
   // Prepare test iteration paramters
   //
   auiPacketsToSend[0]   = 10000;
   aucReponseMode[0]     = NDT_RESPONSE_ACK | NDT_RESPONSE_FLAG_WINDOW;
   aucPacketSizeMode[0]  = NDT_PACKET_TYPE_RANDOM | NDT_PACKET_BUFFERS_ZEROS;
   aucPacketSizeMode[0] |= NDT_PACKET_FLAG_GROUP;
   auiPacketSize[0]      = 0;
   auiBeatDelay[0]       = g_dwStressDelay; 
   auiBeatGroup[0]       = 8;
   
   auiPacketsToSend[1]   = 10000;
   aucReponseMode[1]     = NDT_RESPONSE_ACK | NDT_RESPONSE_FLAG_WINDOW;
   aucPacketSizeMode[1]  = NDT_PACKET_TYPE_RANDOM | NDT_PACKET_BUFFERS_ONES;
   aucPacketSizeMode[1] |= NDT_PACKET_FLAG_GROUP;
   auiPacketSize[1]      = 0;
   auiBeatDelay[1]       = g_dwStressDelay; 
   auiBeatGroup[1]       = 8;

   auiPacketsToSend[2]   = 10000;
   aucReponseMode[2]     = NDT_RESPONSE_ACK | NDT_RESPONSE_FLAG_WINDOW;
   aucPacketSizeMode[2]  = NDT_PACKET_TYPE_RANDOM | NDT_PACKET_BUFFERS_SMALL;
   aucPacketSizeMode[2] |= NDT_PACKET_FLAG_GROUP;
   auiPacketSize[2]      = 0;
   auiBeatDelay[2]       = g_dwStressDelay;
   auiBeatGroup[2]       = 8;

   auiPacketsToSend[3]   = 100;
   aucReponseMode[3]     = NDT_RESPONSE_ACK | NDT_RESPONSE_FLAG_WINDOW;
   aucPacketSizeMode[3]  = NDT_PACKET_TYPE_CYCLICAL | NDT_PACKET_BUFFERS_RANDOM;
   aucPacketSizeMode[3] |= NDT_PACKET_FLAG_GROUP;
   auiPacketSize[3]      = 0;
   auiBeatDelay[3]       = g_dwStressDelay;
   auiBeatGroup[3]       = 8;

   auiPacketsToSend[4]   = 80000/uiMaximumPacketSize;
   aucReponseMode[4]     = NDT_RESPONSE_FULL | NDT_RESPONSE_FLAG_WINDOW;
   aucPacketSizeMode[4]  = NDT_PACKET_TYPE_CYCLICAL | NDT_PACKET_BUFFERS_RANDOM;
   aucPacketSizeMode[4] |= NDT_PACKET_FLAG_GROUP;
   auiPacketSize[4]      = 0;
   auiBeatDelay[4]       = g_dwStressDelay;
   auiBeatGroup[4]       = 8;

   auiPacketsToSend[5]   = 10000;
   aucReponseMode[5]     = NDT_RESPONSE_ACK | NDT_RESPONSE_FLAG_WINDOW;
   aucPacketSizeMode[5]  = NDT_PACKET_TYPE_FIXED | NDT_PACKET_BUFFERS_SMALL;
   aucPacketSizeMode[5] |= NDT_PACKET_FLAG_GROUP;
   auiPacketSize[5]      = 100;
   auiBeatDelay[5]       = g_dwStressDelay;
   auiBeatGroup[5]       = 8;

   auiPacketsToSend[6]   = 10000;
   aucReponseMode[6]     = NDT_RESPONSE_ACK10;
   aucPacketSizeMode[6]  = NDT_PACKET_TYPE_RANDOM | NDT_PACKET_BUFFERS_SMALL;
   aucPacketSizeMode[6] |= NDT_PACKET_FLAG_GROUP;
   auiPacketSize[6]      = 0;
   auiBeatDelay[6]       = g_dwStressDelay; 
   auiBeatGroup[6]       = 8;

   auiPacketsToSend[7]   = 10000;
   aucReponseMode[7]     = NDT_RESPONSE_NONE;
   aucPacketSizeMode[7]  = NDT_PACKET_TYPE_RANDOM | NDT_PACKET_BUFFERS_RANDOM;
   aucPacketSizeMode[7] |= NDT_PACKET_FLAG_GROUP;
   auiPacketSize[7]      = 0;
   auiBeatDelay[7]       = g_dwStressDelay; 
   auiBeatGroup[7]       = 8;
   
   auiPacketsToSend[8]   = 10000;
   aucReponseMode[8]     = NDT_RESPONSE_FULL;
   aucPacketSizeMode[8]  = NDT_PACKET_TYPE_RANDOM | NDT_PACKET_BUFFERS_RANDOM;
   aucPacketSizeMode[8] |= NDT_PACKET_FLAG_GROUP;
   auiPacketSize[8]      = 0;
   auiBeatDelay[8]       = g_dwStressDelay;
   auiBeatGroup[8]       = 8;

   auiPacketsToSend[9]   = 10000;
   aucReponseMode[9]     = NDT_RESPONSE_FULL;
   aucPacketSizeMode[9]  = NDT_PACKET_TYPE_FIXED | NDT_PACKET_BUFFERS_RANDOM;
   aucPacketSizeMode[9] |= NDT_PACKET_FLAG_GROUP;
   auiPacketSize[9]      = 60;
   auiBeatDelay[9]       = g_dwStressDelay;
   auiBeatGroup[9]       = 8;


   // Intro
   NDTLogMsg(_T("Start Stress Tests"));

   // For each test
   for (ixTest = 0; ixTest < 10; ixTest++) {

      NDTLogMsg(_T("Test #%d"), ixTest);

      // Open adapters
      NDTLogMsg(_T("Binding adapters"));
      for (ixAdapter = 0; ixAdapter < 2; ixAdapter++) {
         // Open 
         hr = NDTBind(ahAdapter[ixAdapter], bForce30, ndisMedium);
         if (FAILED(hr)) {
            NDTLogErr(g_szFailBind, hr);
            goto cleanUp;
         }
         // Set directed filters
         hr = NDTSetPacketFilter(ahAdapter[ixAdapter], NDT_FILTER_DIRECTED);
         if (FAILED(hr)) {
            NDTLogErr(g_szFailSetPacketFilter, hr);
            goto cleanUp;
         }
      }

      // Start stress by running receiving on second adapter
      NDTLogMsg(_T("Start receiving"));
      hr = NDTReceive(ahAdapter[1], &ahReceive[1]);
      if (FAILED(hr)) {
          NDTLogErr(g_szFailReceive, hr);
          goto cleanUp;
      }

      // Send on first instance
      NDTLogMsg(_T("Start sending"));
      hr = NDTSend(
         ahAdapter[0], cbAddr, NULL, 1, &pucPermanentAddr, 
         aucReponseMode[ixTest], aucPacketSizeMode[ixTest], 
         auiPacketSize[ixTest], auiPacketsToSend[ixTest], auiBeatDelay[ixTest],
         auiBeatGroup[ixTest], &ahSend[0]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSend, hr);
         goto cleanUp;
      }

      // Wait for send end
      NDTLogMsg(_T("Wait to finish with sending"));
      hr = NDTSendWait(
         ahAdapter[0], ahSend[0], INFINITE, &aulPacketsSent[0], 
         &aulPacketsCompleted[0], &aulPacketsCanceled[0], 
         &aulPacketsUncanceled[0], &aulPacketsReplied[0], &aulTime[0], 
         &aulBytesSent[0], &aulBytesReceived[0]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSendWait, hr);
         goto cleanUp;
      }

      // Wait for while
      Sleep(dwReceiveDelay);

      NDTLogMsg(_T("Stop listening"));
      hr = NDTReceiveStop(
         ahAdapter[1], ahReceive[1], &aulPacketsReceived[1], 
         &aulPacketsReplied[1], &aulPacketsCompleted[1], &aulTime[1], 
         &aulBytesSent[1], &aulBytesReceived[1]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailReceiveStop, hr);
         goto cleanUp;
      }

      NDTLogMsg(
         _T("Bind #0: Sent %d packets with total size %d bytes in %d ms"),
         aulPacketsSent[0], aulBytesSent[0], aulTime[0]
      );
      NDTLogMsg(
         _T("Bind #0: Recv %d packets with total size %d bytes in %d ms"),
         aulPacketsReplied[0], aulBytesReceived[0], aulTime[0]
      );
      NDTLogMsg(
         _T("Bind #1: Recv %d packets with total size %d bytes in %d ms"),
         aulPacketsReceived[1], aulBytesReceived[1], aulTime[1]
      );
      NDTLogMsg(
         _T("Bind #1: Sent %d packets with total size %d bytes in %d ms"),
         aulPacketsReplied[1], aulBytesSent[1], aulTime[1]
      );
      
      NDTLogMsg(_T("Unbinding adapters"));
      for (ixAdapter = 0; ixAdapter < 2; ixAdapter++) {
         hr = NDTUnbind(ahAdapter[ixAdapter]);
         if (FAILED(hr)) {
            NDTLogErr(g_szFailUnbind, hr);
            goto cleanUp;
         }
      }
   }

   // Intro
   NDTLogMsg(_T("Start simultaneous stress tests"));

   // We will run test #4 with lower amount of packets
   auiPacketsToSend[4] = auiPacketsToSend[4]/10 + 1;
   
   // Open adapters
   NDTLogMsg(_T("Binding to adapters"));
   for (ixAdapter = 0; ixAdapter < 8; ixAdapter++) {
      // Open 
      hr = NDTBind(ahAdapter[ixAdapter], bForce30, ndisMedium);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailBind, hr);
         goto cleanUp;
      }
      // Set directed filters
      hr = NDTSetPacketFilter(ahAdapter[ixAdapter], NDT_FILTER_DIRECTED);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSetPacketFilter, hr);
         goto cleanUp;
      }
      if (ixAdapter < 4) {
         hr = NDTSetId(ahAdapter[ixAdapter], ixAdapter, ixAdapter + 4);
      } else {
         hr = NDTSetId(ahAdapter[ixAdapter], ixAdapter, ixAdapter - 4);
      }
      if (FAILED(hr)) goto cleanUp;
   }

   
   // Start listening on 4 adapters
   NDTLogMsg(_T("Start receiving"));
   for (ixAdapter = 4; ixAdapter < 8; ixAdapter++) {
      hr = NDTReceive(ahAdapter[ixAdapter], &ahReceive[ixAdapter]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailReceive, hr);
         goto cleanUp;
      }
   }

   // Sending on 4 adapters
   NDTLogMsg(_T("Start sending"));
   for (ixAdapter = 0; ixAdapter < 4; ixAdapter++) {
      hr = NDTSend(
         ahAdapter[ixAdapter], cbAddr, NULL, 1, &pucPermanentAddr, 
         aucReponseMode[4], aucPacketSizeMode[4], 
         auiPacketSize[4], auiPacketsToSend[4], auiBeatDelay[4],
         auiBeatGroup[4], &ahSend[ixAdapter]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSend, hr);
         goto cleanUp;
      }
   }

   // Wait
   NDTLogMsg(_T("Wait to finish with sending"));
   for (ixAdapter = 0; ixAdapter < 4; ixAdapter++) {
      hr = NDTSendWait(
         ahAdapter[ixAdapter], ahSend[ixAdapter], INFINITE, 
         &aulPacketsSent[ixAdapter], &aulPacketsCompleted[ixAdapter], 
         &aulPacketsCanceled[ixAdapter], &aulPacketsUncanceled[ixAdapter],
         &aulPacketsReplied[ixAdapter], &aulTime[ixAdapter], 
         &aulBytesSent[ixAdapter], &aulBytesReceived[ixAdapter]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSendWait, hr);
         goto cleanUp;
      }
   }

   // Wait for while
   Sleep(dwReceiveDelay);

   NDTLogMsg(_T("Stop listening"));
   for (ixAdapter = 4; ixAdapter < 8; ixAdapter++) {
      hr = NDTReceiveStop(
         ahAdapter[ixAdapter], ahReceive[ixAdapter], 
         &aulPacketsReceived[ixAdapter], &aulPacketsReplied[ixAdapter],
         &aulPacketsCompleted[ixAdapter], &aulTime[ixAdapter], 
         &aulBytesSent[ixAdapter], &aulBytesReceived[ixAdapter]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailReceiveStop, hr);
         goto cleanUp;
      }
   }

   for (ixAdapter = 0; ixAdapter < 4; ixAdapter++) {
      NDTLogMsg(
         _T("Bind #%d: Sent %d packets with total size %d bytes in %d ms"),
         ixAdapter, aulPacketsSent[ixAdapter], aulBytesSent[ixAdapter], 
         aulTime[ixAdapter]
      );
      NDTLogMsg(
         _T("Bind #%d: Recv %d packets with total size %d bytes in %d ms"),
         ixAdapter, aulPacketsReplied[ixAdapter], aulBytesReceived[ixAdapter],
         aulTime[ixAdapter]
      );
   }

   for (ixAdapter = 4; ixAdapter < 8; ixAdapter++) {
      NDTLogMsg(
         _T("Bind #%d: Recv %d packets with total size %d bytes in %d ms"),
         ixAdapter, aulPacketsReceived[ixAdapter], aulBytesReceived[ixAdapter],
         aulTime[ixAdapter]
      );
      NDTLogMsg(
         _T("Bind #%d: Sent %d packets with total size %d bytes in %d ms"),
         ixAdapter, aulPacketsReplied[ixAdapter], aulBytesSent[ixAdapter], 
         aulTime[ixAdapter]
      );
   }

   NDTLogMsg(_T("Unbinding adapters"));
   for (ixAdapter = 0; ixAdapter < nAdapters; ixAdapter++) {
      hr = NDTUnbind(ahAdapter[ixAdapter]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailUnbind, hr);
         goto cleanUp;
      }
   }

cleanUp:
   // We have deside about test pass/fail there
   rc = FAILED(hr) ? TPR_FAIL : TPR_PASS;

   NDTLogMsg(_T("Closing adapters"));
   for (ixAdapter = 0; ixAdapter < nAdapters; ixAdapter++) {
      hr = NDTClose(&ahAdapter[ixAdapter]);
      if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
   }

   delete pucPermanentAddr;
   return rc;
}

//------------------------------------------------------------------------------
