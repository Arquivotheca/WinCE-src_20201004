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

TEST_FUNCTION(TestCancelSend)
{
   TEST_ENTRY;

   INT rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;

   UINT   nAdapters = 1;
   HANDLE ahAdapter[1];
   HANDLE ahSend[1];
   UINT   ixAdapter = 0;
   
   ULONG aulPacketsSent[1];
   ULONG aulPacketsCompleted[1];
   ULONG aulPacketsCanceled[1]; 
   ULONG aulPacketsUncanceled[1];
   ULONG aulPacketsReplied[1];
   ULONG ulConversationId = 0;
#ifndef NDT6
   ULONG aulTime[1];
#else
   ULONGLONG aulTime[1];
#endif
   ULONG aulBytesSent[1];
   ULONG aulBytesReceived[1];

   UINT  cbAddr = 0;
   UINT  cbHeader = 0;
   BYTE* pucRandomAddr = NULL;
   BYTE* pucBroadcastAddr = NULL;

   UINT  uiMaximumPacketSize = 0;
   UINT  uiPacketSize = 64;
   UINT  uiPacketsToSend = 100;
   UINT  uiBeatDelay = 100;
   UINT  uiBeatGroup = 8;

   // Zero structures
   memset(ahAdapter, 0, sizeof(ahAdapter));
   memset(ahSend, 0, sizeof(ahSend));
   memset(aulPacketsSent, 0, sizeof(aulPacketsSent));
   memset(aulPacketsCompleted, 0, sizeof(aulPacketsCompleted));
   memset(aulPacketsCanceled, 0, sizeof(aulPacketsCanceled));
   memset(aulPacketsUncanceled, 0, sizeof(aulPacketsUncanceled));
   memset(aulPacketsReplied, 0, sizeof(aulPacketsReplied));
   memset(aulTime, 0, sizeof(aulTime));
   memset(aulBytesReceived, 0, sizeof(aulBytesReceived));
   memset(aulBytesSent, 0, sizeof(aulBytesSent));

   // Log intro
   NDTLogMsg(
      _T("Start 1c_CancelSend test on the adapter %s"), g_szTestAdapter
   );

   // Open adapters
   NDTLogMsg(_T("Opening adapters"));
   for (ixAdapter = 0; ixAdapter < nAdapters; ixAdapter++) {
      hr = NDTOpen(g_szTestAdapter, &ahAdapter[ixAdapter]);
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

   // Query OID_GEN_MAXIMUM_FRAME_SIZE
   hr = NDTGetMaximumFrameSize(ahAdapter[0], &uiMaximumPacketSize);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetMaximumFrameSize, hr);
      goto cleanUp;
   }

   // Close
   hr = NDTUnbind(ahAdapter[0]);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailUnbind, hr);
      goto cleanUp;
   }

   // Generate address used later
   pucRandomAddr = NDTGetRandomAddr(ndisMedium);
   pucBroadcastAddr = NDTGetBroadcastAddr(ndisMedium);

   uiPacketsToSend = 1000;
   uiBeatDelay = 50;
   uiBeatGroup = 8;

   if (rand_s((unsigned int*)&ulConversationId) != 0)   {
      NDTLogErr(g_szFailGetConversationId, 0);
      goto cleanUp;
   }
   
   // Open adapters
   NDTLogMsg(_T("Binding adapters"));
   for (ixAdapter = 0; ixAdapter < nAdapters; ixAdapter++) {
      hr = NDTBind(ahAdapter[ixAdapter], bForce30, ndisMedium);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailBind, hr);
         goto cleanUp;
      }
   }

   NDTLogMsg(_T("Sending and cancel packets"));
   for (ixAdapter = 0; ixAdapter < nAdapters; ixAdapter++) {
      hr = NDTSend(
         ahAdapter[ixAdapter], cbAddr, NULL, 1, &pucRandomAddr, 
         NDT_RESPONSE_NONE, 
         NDT_PACKET_TYPE_FIXED | NDT_PACKET_FLAG_GROUP | NDT_PACKET_FLAG_CANCEL, 
         uiPacketSize, uiPacketsToSend, ulConversationId, uiBeatDelay, uiBeatGroup, 
         &ahSend[ixAdapter]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSend, hr);
         goto cleanUp;
      }
   }
   
   for (ixAdapter = 0; ixAdapter < nAdapters; ixAdapter++) {
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

   NDTLogMsg(_T("Unbind adapters"));
   for (ixAdapter = 0; ixAdapter < nAdapters; ixAdapter++) {
      hr = NDTUnbind(ahAdapter[ixAdapter]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailUnbind, hr);
         goto cleanUp;
      }
   }

   NDTLogMsg(
      _T("%d packets was sent. %d packet wasn't canceled. %d packet was."),
      aulPacketsSent[0], aulPacketsUncanceled[0], aulPacketsCanceled[0]
   );
   if (aulPacketsCanceled[0] == 0 && g_bForceCancel) hr = E_FAIL;

cleanUp:
   // We have deside about test pass/fail there
   rc = FAILED(hr) ? TPR_FAIL : TPR_PASS;
   
   NDTLogMsg(_T("Closing adapters"));
   for (ixAdapter = 0; ixAdapter < nAdapters; ixAdapter++) {
      hr = NDTClose(&ahAdapter[ixAdapter]);
      if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
   }
   delete pucRandomAddr;
   delete pucBroadcastAddr;
   return rc;
}

//------------------------------------------------------------------------------
