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
#include "ndt_2c.h"
#include "utils.h"
//------------------------------------------------------------------------------

TEST_FUNCTION(TestStressReceive)
{
   TEST_ENTRY;

   // Skip test if required
   if (g_bNoStress) return TPR_SKIP;

   INT rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;

   UINT   chAdapter = 2;
   HANDLE ahAdapter[2];
   HANDLE ahSend[2];
   HANDLE ahReceive[2];
   
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
   
   UINT  uiDifference = 0;
   UINT  uiTreshold = 0;
   UINT  ui = 0;


   // Let start
   NDTLogMsg(_T("Start 2c_StessReceive test"));
   NDTLogMsg(_T("The test adapter is %s"), g_szTestAdapter);
   NDTLogMsg(_T("The support adapter is %s"), g_szHelpAdapter);

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
   
   
   // Get some information about a media
   NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);
   dwReceiveDelay = NDTGetReceiveDelay(ndisMedium);

   // Open adapters
   NDTLogMsg(_T("Opening adapters"));
   
   // Test
   hr = NDTOpen(g_szTestAdapter, &ahAdapter[0]);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
      goto cleanUp;
   }

   // Support
   hr = NDTOpen(g_szHelpAdapter, &ahAdapter[1]);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailOpen, g_szHelpAdapter, hr);
      rc = TPR_FAIL;
      goto cleanUp;
   }

   // Binding adapters
   NDTLogMsg(_T("Binding adapters"));
   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {
      hr = NDTBind(ahAdapter[ixAdapter], bForce30, ndisMedium);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailBind, hr);
         rc = TPR_FAIL;
         goto cleanUp;
      }
   }

   // Get basic information
   NDTLogMsg(_T("Get basic adapters info"));

   // Get maximum frame size
   hr = NDTGetMaximumFrameSize(ahAdapter[0], &uiMaximumPacketSize);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetMaximumFrameSize, hr);
      rc = TPR_FAIL;
      goto cleanUp;
   }

   hr = NDTGetPermanentAddr(ahAdapter[0], ndisMedium, &pucPermanentAddr);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetPermanentAddr, hr);
      rc = TPR_FAIL;
      goto cleanUp;
   }

   // Set direct filters
   NDTLogMsg(_T("Seting direct receive filters"));
   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {
      hr = NDTSetPacketFilter(ahAdapter[ixAdapter], NDT_FILTER_DIRECTED);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSetPacketFilter, hr);
         rc = TPR_FAIL;
         goto cleanUp;
      }
   }
   
   // Prepare test iteration paramters
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


   // For each test
   for (ixTest = 0; ixTest < 10; ixTest++) {

      NDTLogMsg(_T("Test Pass #%d"), ixTest);
      NDTLogMsg(_T("Start sending %d packets"), auiPacketsToSend[ixTest]);

      // Start stress by running receiving on second adapter
      hr = NDTReceive(ahAdapter[0], &ahReceive[0]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailReceive, hr);
         rc = TPR_FAIL;
         goto cleanUp;
      }

      // Send on first instance
      hr = NDTSend(
         ahAdapter[1], cbAddr, NULL, 1, &pucPermanentAddr, 
         aucReponseMode[ixTest], aucPacketSizeMode[ixTest], 
         auiPacketSize[ixTest], auiPacketsToSend[ixTest], auiBeatDelay[ixTest],
         auiBeatGroup[ixTest], &ahSend[1]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSend, hr);
         rc = TPR_FAIL;
         goto cleanUp;
      }

      // Wait for send end
      hr = NDTSendWait(
         ahAdapter[1], ahSend[1], INFINITE, &aulPacketsSent[1], 
         &aulPacketsCompleted[1], &aulPacketsCanceled[1], 
         &aulPacketsUncanceled[1], &aulPacketsReplied[1], &aulTime[1], 
         &aulBytesSent[1], &aulBytesReceived[1]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSendWait, hr);
         rc = TPR_FAIL;
         goto cleanUp;
      }

      // Log info
      NDTLogMsg(_T("Sent %d packets"), aulPacketsSent[1]);
      
      // Wait for while
      Sleep(dwReceiveDelay);

      // Stop receiving
      NDTLogMsg(_T("Stop receiving and checking results"));
      hr = NDTReceiveStop(
         ahAdapter[0], ahReceive[0], &aulPacketsReceived[0], 
         &aulPacketsReplied[0], &aulPacketsCompleted[0], &aulTime[0], 
         &aulBytesSent[0], &aulBytesReceived[0]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailReceiveStop, hr);
         rc = TPR_FAIL;
         goto cleanUp;
      }

      NDTLogMsg(
         _T("Support: Sent %d packets with total size %d bytes in %d ms"),
         aulPacketsSent[1], aulBytesSent[1], aulTime[1]
      );
      NDTLogMsg(
         _T("Support: Recv %d packets with total size %d bytes in %d ms"),
         aulPacketsReplied[1], aulBytesReceived[1], aulTime[1]
      );
      NDTLogMsg(
         _T("Test: Recv %d packets with total size %d bytes in %d ms"),
         aulPacketsReceived[0], aulBytesReceived[0], aulTime[0]
      );
      NDTLogMsg(
         _T("Test: Sent %d packets with total size %d bytes in %d ms"),
         aulPacketsReplied[0], aulBytesSent[0], aulTime[0]
      );

      // Check result
      if (NotReceivedOK(aulPacketsReceived[0],aulPacketsSent[1])) {
		 rc = TPR_FAIL;
         NDTLogErr(
            _T("Received %d packets but %d was expected"), 
            aulPacketsReceived[0], aulPacketsSent[1]
         );
	  }
   }

   NDTLogMsg(_T("Unbinding adapters"));
   for (ixAdapter = 0; ixAdapter < 2; ixAdapter++) {
      hr = NDTUnbind(ahAdapter[ixAdapter]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailUnbind, hr);
         goto cleanUp;
      }
   }

cleanUp:

   NDTLogMsg(_T("Closing adapters"));
   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {
      hr = NDTClose(&ahAdapter[ixAdapter]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailClose, hr);
         rc = TPR_FAIL;
      }
   }

   delete pucPermanentAddr;
   return rc;
}

//------------------------------------------------------------------------------
