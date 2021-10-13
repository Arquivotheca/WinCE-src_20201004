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
#include "NDTLog.h"
#include "NDTLib.h"
#include "NDTLibEx.h"
#include "ndt_2c.h"
#include "utils.h"
#include "NDTError.h"

extern UINT  g_uiTestPhyMedium;
extern DWORD g_dwSendWaitTimeout;

//------------------------------------------------------------------------------

TEST_FUNCTION(TestReceivePackets)
{
   TEST_ENTRY;

   INT rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;

   UINT   chAdapter = 2;
   HANDLE ahAdapter[2] = {NULL, NULL};
   UINT   ixAdapter = 0;

   UINT   uiMinimumPacketSize = 64;
   UINT   uiMaximumPacketSize = 0;
   DWORD  dwReceiveDelay = 0;   

   HANDLE hSend = NULL;
   ULONG  ulSendPacketsSent = 0;
   ULONG  ulSendPacketsCompleted = 0;
   ULONG ulConversationId = 0;
#ifndef NDT6
   ULONG  ulSendTime = 0;
#else
   ULONGLONG  ulSendTime = 0;
#endif
   ULONG  ulSendBytesSent = 0;

   HANDLE hReceive = NULL;
   ULONG  ulRecvPacketsReceived = 0;
#ifndef NDT6   
   ULONG  ulRecvTime = 0;
#else
   ULONGLONG  ulRecvTime = 0;
#endif
   ULONG  ulRecvBytesRecv = 0;

   UINT   cbAddr = 0;
   UINT   cbHeader = 0;

   UINT   cpucDestAddr = 8;
   UCHAR* apucDestAddr[8];
   UINT   ixDestAddr = 0;
   
   UCHAR* pucPermanentAddr = NULL;

   UINT   uiPacketSize = 128;
   UINT   uiPacketsToSend = 300;
   BOOL   bFixedSize = FALSE;


   // Intro
   NDTLogMsg(_T("Start 2c_ReceivePackets test"));
   NDTLogMsg(_T("The test adapter is %s"), g_szTestAdapter);
   NDTLogMsg(_T("The support adapter is %s"), g_szHelpAdapter);

   if (rand_s((unsigned int*)&ulConversationId) != 0)   {
      NDTLogErr(g_szFailGetConversationId, 0);
      goto cleanUp;
   }

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

   hr = NDTGetMaximumFrameSize(ahAdapter[0], &uiMaximumPacketSize);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetMaximumFrameSize, hr);
      goto cleanUp;
   }

   hr = NDTGetPermanentAddr(ahAdapter[0], ndisMedium, &pucPermanentAddr);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetPermanentAddr, hr);
      goto cleanUp;
   }

   for (ixDestAddr = 0; ixDestAddr < cpucDestAddr; ixDestAddr++) {
      apucDestAddr[ixDestAddr] = pucPermanentAddr;
   }
   
   NDTLogMsg(_T("Set adapter filters"));
   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {
      hr = NDTSetPacketFilter(ahAdapter[ixAdapter], NDT_FILTER_DIRECTED);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSetPacketFilter, hr);
         goto cleanUp;
      }
   }

   uiPacketSize = uiMinimumPacketSize;
   uiPacketsToSend = 300;
   bFixedSize = FALSE;
   
   while (uiPacketSize <= uiMaximumPacketSize) {

      NDTLogMsg(
         _T("Sending %d packets with size %d bytes to 1 address"),
         (uiPacketsToSend*cpucDestAddr), uiPacketSize
      );
         
      // Start receive
      hr = NDTReceive(ahAdapter[0], ulConversationId, &hReceive);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailReceive, hr);
         goto cleanUp;
      }
         
      // Send
      hr = NDTSend(
         ahAdapter[1], cbAddr, NULL, cpucDestAddr, apucDestAddr, 
         NDT_RESPONSE_NONE, NDT_PACKET_FLAG_GROUP | NDT_PACKET_TYPE_FIXED, 
         uiPacketSize, uiPacketsToSend, ulConversationId, 0, 0, &hSend
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSend, hr);
         goto cleanUp;
      }

      // Wait till sending is done
      hr = NDTSendWait(
         ahAdapter[1], hSend, g_dwSendWaitTimeout, &ulSendPacketsSent, 
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
         ahAdapter[0], hReceive, &ulRecvPacketsReceived, NULL, NULL, 
         &ulRecvTime, NULL, &ulRecvBytesRecv
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailReceiveStop, hr);
         goto cleanUp;
      }

      if (NotReceivedOK(ulRecvPacketsReceived,ulSendPacketsSent,g_uiTestPhyMedium,NDIS_PACKET_TYPE_DIRECTED)) {
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

   delete pucPermanentAddr;
   return rc;
}

//------------------------------------------------------------------------------
