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

TEST_FUNCTION(TestSend)
{
   TEST_ENTRY;

   INT rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;

   UINT   nBindings = 2;
   HANDLE ahAdapter[2];
   HANDLE ahSend[2];
   UINT   ixBinding = 0;
   
   ULONG aulPacketsSent[2];
   ULONG aulPacketsCompleted[2];
   ULONG aulPacketsCanceled[2]; 
   ULONG aulPacketsUncanceled[2];
   ULONG aulPacketsReplied[2];
   ULONG aulTime[2];
   ULONG aulBytesSend[2];
   ULONG aulBytesRecv[2];

   UINT  cbAddr = 0;
   UINT  cbHeader = 0;
   BYTE* pucRandomAddr = NULL;
   BYTE* pucBroadcastAddr = NULL;
   BYTE* pucMulticastAddr = NULL;
   BYTE* pucGroupAddr = NULL;

   ULONG nDestAddrs = 0;
   BYTE* apucDestAddr[8];

   UINT  uiMaximumFrameSize = 0;
   UINT  uiMaximumTotalSize = 0;
   UINT  uiMinimumPacketSize = 64;
   UINT  uiMaximumPacketSize = 0;

   UINT  uiPacketSize = 64;
   UINT  uiPacketsToSend = 100;

   BOOL  bFixedSize = FALSE;
   UINT  ui = 0;

   // Zero structures
   memset(ahAdapter, 0, sizeof(ahAdapter));
   memset(ahSend, 0, sizeof(ahSend));
   memset(aulPacketsSent, 0, sizeof(aulPacketsSent));
   memset(aulPacketsCompleted, 0, sizeof(aulPacketsCompleted));
   memset(aulPacketsCanceled, 0, sizeof(aulPacketsCanceled));
   memset(aulPacketsUncanceled, 0, sizeof(aulPacketsUncanceled));
   memset(aulPacketsReplied, 0, sizeof(aulPacketsReplied));
   memset(aulTime, 0, sizeof(aulTime));
   memset(aulBytesRecv, 0, sizeof(aulBytesRecv));
   memset(aulBytesSend, 0, sizeof(aulBytesSend));
   memset(apucDestAddr, 0, sizeof(apucDestAddr));

   // Log intro
   NDTLogMsg(_T("Start 1c_Send test on the adapter %s"), g_szTestAdapter);

   for (ixBinding = 0; ixBinding < nBindings; ixBinding++) {
      hr = NDTOpen(g_szTestAdapter, &ahAdapter[ixBinding]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
         goto cleanUp;
      }
      if (FAILED(hr)) goto cleanUp;
   }

   NDTLogMsg(_T("Getting basic info about the adapter"));

   // Get some information about a media
   NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);

   // Bind
   hr = NDTBind(ahAdapter[0], bForce30, ndisMedium);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailBind, hr);
      goto cleanUp;
   }

   // Get maximum frame size
   hr = NDTGetMaximumFrameSize(ahAdapter[0], &uiMaximumFrameSize);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetMaximumFrameSize, hr);
      goto cleanUp;
   }

   // Get maximum total size
   hr = NDTGetMaximumTotalSize(ahAdapter[0], &uiMaximumTotalSize);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetMaximumTotalSize, hr);
      goto cleanUp;
   }
   
   NDTLogMsg(_T("Get OID_GEN_MAXIMUM_TOTAL_SIZE = %d"), uiMaximumTotalSize);
   NDTLogMsg(_T("Get OID_GEN_MAXIMUM_FRAME_SIZE = %d"), uiMaximumFrameSize);
   
   if (uiMaximumTotalSize - uiMaximumFrameSize != cbHeader) {
      NDTLogErr(
         _T("Difference between values above should be %d"), cbHeader
      );
      goto cleanUp;
   }

   uiMaximumPacketSize = uiMaximumFrameSize;
   
   // Unbind
   hr = NDTUnbind(ahAdapter[0]);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailUnbind, hr);
      goto cleanUp;
   }

   // Generate address used later
   pucRandomAddr = NDTGetRandomAddr(ndisMedium);
   pucBroadcastAddr = NDTGetBroadcastAddr(ndisMedium);
   pucMulticastAddr = NDTGetMulticastAddr(ndisMedium);
   pucGroupAddr = NDTGetGroupAddr(ndisMedium);

   // Bind adapters
   for (ixBinding = 0; ixBinding < nBindings; ixBinding++) {
      hr = NDTBind(ahAdapter[ixBinding], bForce30, ndisMedium);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailBind, hr);
         goto cleanUp;
      }
   }
   
   NDTLogMsg(_T("Testing sending to a (random) directed address"));
   
   for (ui = 0; ui < 4; ui++) {

      NDTLogMsg(_T("Sending: Send"));

      switch (ui) {
      case 0:
         apucDestAddr[0] = pucRandomAddr;
         NDTLogMsg(_T("Sending to a random address"));
         break;
      case 1:
         apucDestAddr[0] = pucBroadcastAddr;
         NDTLogMsg(_T("Sending to a broadcast address"));
         break;
      case 2:
         apucDestAddr[0] = pucMulticastAddr;
         NDTLogMsg(_T("Sending to a multicast/functional address"));
         break;
      case 3:
         if (pucGroupAddr == NULL) continue;
         apucDestAddr[0] = pucGroupAddr;
         NDTLogMsg(_T("Sending to a group address"));
         break;
      }

      uiPacketSize = uiMinimumPacketSize;
      bFixedSize = FALSE;

      while (uiPacketSize <= uiMaximumPacketSize) {
         
         // Send on first instance
         hr = NDTSend(
            ahAdapter[0], cbAddr, NULL, 1, apucDestAddr, NDT_RESPONSE_NONE, 
            NDT_PACKET_TYPE_FIXED, uiPacketSize, uiPacketsToSend, 0, 0, 
            &ahSend[0]
         );
         if (FAILED(hr)) {
            NDTLogErr(g_szFailSend, hr);
            goto cleanUp;
         }

         // Send on second instance
         hr = NDTSend(
            ahAdapter[1], cbAddr, NULL, 1, apucDestAddr, NDT_RESPONSE_NONE, 
            NDT_PACKET_TYPE_FIXED, uiPacketSize, uiPacketsToSend, 0, 0, 
            &ahSend[1]
         );
         if (FAILED(hr)) {
            NDTLogErr(g_szFailSend, hr);
            goto cleanUp;
         }

         // Wait to finish on first
         hr = NDTSendWait(
            ahAdapter[0], ahSend[0], INFINITE, &aulPacketsSent[0],
            &aulPacketsCompleted[0], &aulPacketsCanceled[0], 
            &aulPacketsUncanceled[0], &aulPacketsReplied[0], &aulTime[0], 
            &aulBytesSend[0], &aulBytesRecv[0]
         );
         if (FAILED(hr)) {
            NDTLogErr(g_szFailSendWait, hr);
            goto cleanUp;
         }

         // Wait to finish on second
         hr = NDTSendWait(
            ahAdapter[1], ahSend[1], INFINITE, &aulPacketsSent[1],
            &aulPacketsCompleted[1], &aulPacketsCanceled[1], 
            &aulPacketsUncanceled[1], &aulPacketsReplied[1], &aulTime[1], 
            &aulBytesSend[1], &aulBytesRecv[1]
         );
         if (FAILED(hr)) {
            NDTLogErr(g_szFailSendWait, hr);
            goto cleanUp;
         }

         // Update the packet size
         if (bFixedSize) {
            uiPacketSize--;
            bFixedSize = FALSE;
         }
         uiPacketSize += (uiMaximumPacketSize - uiMinimumPacketSize)/2;
         if (uiPacketSize < uiMaximumPacketSize && (uiPacketSize & 0x1) == 0) {
            bFixedSize = TRUE;
            uiPacketSize++;
         }
      }
      
   }

   // Unbind
   for (ixBinding = 0; ixBinding < nBindings; ixBinding++) {
      hr = NDTUnbind(ahAdapter[ixBinding]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailUnbind, hr);
         goto cleanUp;
      }
   }
   
   NDTLogMsg(_T("Testing sending to a group of addresses"));

   apucDestAddr[0] = pucRandomAddr;
   apucDestAddr[1] = pucRandomAddr;
   apucDestAddr[2] = pucBroadcastAddr;
   apucDestAddr[3] = pucBroadcastAddr;
   nDestAddrs = 4;

   if (ndisMedium == NdisMedium802_5) {
      apucDestAddr[4] = pucMulticastAddr;
      apucDestAddr[5] = pucMulticastAddr;
      apucDestAddr[6] = pucGroupAddr;
      apucDestAddr[7] = pucGroupAddr;
      nDestAddrs = 8;
   }

   // Bind adapters
   for (ixBinding = 0; ixBinding < nBindings; ixBinding++) {
      hr = NDTBind(ahAdapter[ixBinding], bForce30, ndisMedium);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailBind, hr);
         goto cleanUp;
      }
   }

   uiPacketSize = uiMinimumPacketSize;
   bFixedSize = FALSE;

   while (uiPacketSize <= uiMaximumPacketSize) {

      // Send on first instance
      hr = NDTSend(
         ahAdapter[0], cbAddr, NULL, nDestAddrs, apucDestAddr, 
         NDT_RESPONSE_NONE, /*NDT_PACKET_FLAG_GROUP |*/ NDT_PACKET_TYPE_FIXED, 
         uiPacketSize, uiPacketsToSend, 0, 0, &ahSend[0]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSend, hr);
         goto cleanUp;
      }

      // Send on second instance
      hr = NDTSend(
         ahAdapter[1], cbAddr, NULL, nDestAddrs, apucDestAddr, 
         NDT_RESPONSE_NONE, /*NDT_PACKET_FLAG_GROUP |*/ NDT_PACKET_TYPE_FIXED, 
         uiPacketSize, uiPacketsToSend, 0, 0, &ahSend[1]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSend, hr);
         goto cleanUp;
      }

      // Wait to finish on first
      hr = NDTSendWait(
         ahAdapter[0], ahSend[0], INFINITE, &aulPacketsSent[0],
         &aulPacketsCompleted[0], &aulPacketsCanceled[0], 
         &aulPacketsUncanceled[0], &aulPacketsReplied[0], &aulTime[0], 
         &aulBytesSend[0], &aulBytesRecv[0]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSendWait, hr);
         goto cleanUp;
      }

      // Wait to finish on second
      hr = NDTSendWait(
         ahAdapter[1], ahSend[1], INFINITE, &aulPacketsSent[1],
         &aulPacketsCompleted[1], &aulPacketsCanceled[1], 
         &aulPacketsUncanceled[1], &aulPacketsReplied[1], &aulTime[1], 
         &aulBytesSend[1], &aulBytesRecv[1]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSendWait, hr);
         goto cleanUp;
      }

      // Update the packet size
     if (bFixedSize) {
         uiPacketSize--;
         bFixedSize = FALSE;
     }
     uiPacketSize += (uiMaximumPacketSize - uiMinimumPacketSize) / 2;
     if (uiPacketSize < uiMaximumPacketSize && (uiPacketSize & 0x1) == 0) {
         bFixedSize = TRUE;
         uiPacketSize++;
     }

   }

   // Unbind
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
   
   for (ixBinding = 0; ixBinding < nBindings; ixBinding++) {
      hr = NDTClose(&ahAdapter[ixBinding]);
      if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
   }
   delete pucRandomAddr;
   delete pucBroadcastAddr;
   delete pucMulticastAddr;
   delete pucGroupAddr;
   return rc;
}

//------------------------------------------------------------------------------
