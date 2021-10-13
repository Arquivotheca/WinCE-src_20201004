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

TEST_FUNCTION(TestSendPackets)
{
   TEST_ENTRY;

   INT rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;

   UINT   chAdapter = 2;
   HANDLE ahAdapter[2];
   UINT   ixAdapter = 0;

   HANDLE hSend = NULL;
   ULONG  ulSendPacketsSent = 0;
   ULONG  ulSendPacketsCompleted = 0;
   ULONG  ulSendTime = 0;
   ULONG  ulSendBytesSent = 0;
   ULONG  ulSendBytesRecv = 0;
 
   HANDLE hRecv = NULL;
   ULONG  ulRecvPacketsReceived = 0;
   ULONG  ulRecvTime = 0;
   ULONG  ulRecvBytesSent = 0;
   ULONG  ulRecvBytesRecv = 0;

   BYTE*  pucRecvAddr = NULL;
   
   UINT   nPassesRequired = 0;
   UINT   ixPass = 0;
   UINT   ixFilter = 0;
   
   UINT   cbAddr = 0;
   UINT   cbHeader = 0;
   UINT   nDestAddrs = 8;
   UINT   ixDestAddr = 0;
   BYTE*  apucDestAddr[8];

   DWORD  dwReceiveDelay = 0;   

   UINT   uiPacketSize = 128;
   UINT   uiPacketsToSend = 300;
   UINT   uiMinimumPacketSize = 64;
   UINT   uiMaximumPacketSize = 0;
   BOOL   bFixedSize = FALSE;
   
   UINT   ui = 0;

   // Let start
   NDTLogMsg(_T("Start 2c_SendPackets test"));
   NDTLogMsg(_T("The test adapter is %s"), g_szTestAdapter);
   NDTLogMsg(_T("The support adapter is %s"), g_szHelpAdapter);

   // Zero local variables
   memset(apucDestAddr, 0, sizeof(apucDestAddr));

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
      goto cleanUp;
   }

   // Binding adapters
   NDTLogMsg(_T("Binding adapters"));
   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {
      hr = NDTBind(ahAdapter[ixAdapter], bForce30, ndisMedium);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailBind, hr);
         goto cleanUp;
      }
   }

   // Get basic information
   NDTLogMsg(_T("Get basic adapters info"));

   // Get a permanent address & other media specific adresses
   hr = NDTGetMaximumFrameSize(ahAdapter[0], &uiMaximumPacketSize);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetMaximumFrameSize, hr);
      goto cleanUp;
   }

   hr = NDTGetPermanentAddr(ahAdapter[1], ndisMedium, &pucRecvAddr);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetPermanentAddr, hr);
      goto cleanUp;
   }

   for (ixDestAddr = 0; ixDestAddr < nDestAddrs; ixDestAddr++) {
      apucDestAddr[ixDestAddr] = pucRecvAddr;
   }

   // Set filter for receiving
   NDTLogMsg(_T("Set adapter filters"));
   hr = NDTSetPacketFilter(ahAdapter[1], NDT_FILTER_DIRECTED);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailSetPacketFilter, hr);
      goto cleanUp;
   }

   // Send with different number of destination addresses
   for (ixDestAddr = 1; ixDestAddr <= nDestAddrs; ixDestAddr++) {

      uiPacketSize = uiMinimumPacketSize;
      uiPacketsToSend = 300;
      bFixedSize = FALSE;

      while (uiPacketSize <= uiMaximumPacketSize) {

         NDTLogMsg(
            _T("Sending %d packets with size %d bytes to %d addresses"),
            uiPacketsToSend, uiPacketSize, ixDestAddr
         );
         
         // Start receive
         hr = NDTReceive(ahAdapter[1], &hRecv);
         if (FAILED(hr)) {
            NDTLogErr(g_szFailReceive, hr);
            goto cleanUp;
         }
         
         // Send
         hr = NDTSend(
            ahAdapter[0], cbAddr, NULL, ixDestAddr, apucDestAddr, 
            NDT_RESPONSE_NONE, NDT_PACKET_TYPE_FIXED, uiPacketSize, 
            uiPacketsToSend, 0, 0, &hSend
         );
         if (FAILED(hr)) {
            NDTLogErr(g_szFailSend, hr);
            goto cleanUp;
         }


         // Wait till sending is done
         hr = NDTSendWait(
            ahAdapter[0], hSend, INFINITE, &ulSendPacketsSent, 
            &ulSendPacketsCompleted, NULL, NULL, NULL, &ulSendTime, 
            &ulSendBytesSent, NULL
         );
         if (FAILED(hr)) {
            NDTLogErr(g_szFailSendWait, hr);
            goto cleanUp;
         }

         NDTLogMsg(_T("Sent %d packets"), ulSendPacketsSent);
         
         // Wait for while
         Sleep(dwReceiveDelay);

         // Stop receiving and check results
         NDTLogMsg(_T("Stop receiving and checking results"));
         hr = NDTReceiveStop(
            ahAdapter[1], hRecv, &ulRecvPacketsReceived, NULL, NULL, 
            &ulRecvTime, &ulRecvBytesSent, &ulRecvBytesRecv
         );
         if (FAILED(hr)) {
            NDTLogErr(g_szFailReceiveStop, hr);
            goto cleanUp;
         }

         if (NotReceivedOK(ulRecvPacketsReceived,ulSendPacketsSent)) {
            hr = E_FAIL;
            NDTLogErr(
               _T("Should have received %d packets but get %d packets"), 
               ulSendPacketsSent, ulRecvPacketsReceived
            );
            goto cleanUp;
         }

         NDTLogMsg(_T("Received %d packets"), ulRecvPacketsReceived);

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

   // Unbinding adapters
   NDTLogMsg(_T("Unbinding adapters"));
   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {
      hr = NDTUnbind(ahAdapter[ixAdapter]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailUnbind, hr);
         goto cleanUp;
      }
   }

cleanUp:

   // We have deside about test pass/fail there
   rc = FAILED(hr) ? TPR_FAIL : TPR_PASS;

   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {
      hr = NDTClose(&ahAdapter[ixAdapter]);
      if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
   }
   delete pucRecvAddr;

   return rc;
}

//------------------------------------------------------------------------------
