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

TEST_FUNCTION(TestReset)
{
   TEST_ENTRY;

   INT rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;

   UINT   nAdapters = 2;
   HANDLE ahAdapter[2];
   HANDLE ahSend[2];
   UINT   ixAdapter = 0;
   
   ULONG aulPacketsSent[2];
   ULONG aulPacketsCompleted[2];
   ULONG aulPacketsCanceled[2]; 
   ULONG aulPacketsUncanceled[2];
   ULONG aulPacketsReplied[2];
   ULONG aulTime[2];
   ULONG aulBytesSent[2];
   ULONG aulBytesReceived[2];

   UINT  cbAddr = 0;
   UINT  cbHeader = 0;
   BYTE* pucRandomAddr = NULL;
   BYTE* pucBroadcastAddr = NULL;

   UINT  nDestAddrs = 8;
   BYTE* apucDestAddr[8];
   UINT  lxDestAddr = 0;

   UINT  uiMinimumPacketSize = 64;
   UINT  uiMaximumPacketSize = 0;
   UINT  uiPacketSize = 64;
   UINT  uiPacketsToSend = 100;
   UINT  uiBeatDelay = 100;
   UINT  uiNumberOfResets = 1;
   DWORD dwResetSleep = 1000;

   UINT  uiConnectStatus = 0;
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
   memset(aulBytesReceived, 0, sizeof(aulBytesReceived));
   memset(aulBytesSent, 0, sizeof(aulBytesSent));
   memset(apucDestAddr, 0, sizeof(apucDestAddr));

   // Log intro
   NDTLogMsg(_T("Start 1c_Reset test on the adapter %s"), g_szTestAdapter);

   // Bind adapters
   NDTLogMsg(_T("Opening adapters"));
   for (ixAdapter = 0; ixAdapter < nAdapters; ixAdapter++) {
      hr = NDTOpen(g_szTestAdapter, &ahAdapter[ixAdapter]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
         goto cleanUp;
      }
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
   hr = NDTGetMaximumFrameSize(ahAdapter[0], &uiMaximumPacketSize);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetMaximumFrameSize, hr);
      goto cleanUp;
   }

   // Unbind
   hr = NDTUnbind(ahAdapter[0]);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailUnbind, hr);
      goto cleanUp;
   }

   // Generate address used later
   pucRandomAddr = NDTGetRandomAddr(ndisMedium);
   pucBroadcastAddr = NDTGetBroadcastAddr(ndisMedium);

   apucDestAddr[0] = pucRandomAddr;
   apucDestAddr[1] = pucRandomAddr;
   apucDestAddr[2] = pucBroadcastAddr;
   apucDestAddr[3] = pucBroadcastAddr;
   apucDestAddr[4] = pucRandomAddr;
   apucDestAddr[5] = pucRandomAddr;
   apucDestAddr[6] = pucBroadcastAddr;
   apucDestAddr[7] = pucBroadcastAddr;

   switch (ndisMedium) {
   case NdisMedium802_3:
      uiNumberOfResets = 70;
      dwResetSleep = 500;
      break;
   case NdisMedium802_5:
   case NdisMediumFddi:
      uiNumberOfResets = 30;
      dwResetSleep = 1000;
      break;
   }

   uiPacketsToSend = 600;
   uiBeatDelay = 100;
   
   // Open adapters
   NDTLogMsg(_T("Binding adapters"));
   for (ixAdapter = 0; ixAdapter < nAdapters; ixAdapter++) {
      hr = NDTBind(ahAdapter[ixAdapter], bForce30, ndisMedium);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailBind, hr);
         rc = TPR_FAIL;
         goto cleanUp;
      }
   }

   NDTLogMsg(_T("Sending packets"));
   for (ixAdapter = 0; ixAdapter < nAdapters; ixAdapter++) {
      hr = NDTSend(
         ahAdapter[ixAdapter], cbAddr, NULL, nDestAddrs, apucDestAddr, 
         NDT_RESPONSE_NONE, NDT_PACKET_FLAG_GROUP | NDT_PACKET_TYPE_FIXED, 
         uiPacketSize, uiPacketsToSend, uiBeatDelay, 1, &ahSend[ixAdapter]
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSend, hr);
         rc = TPR_FAIL;
         goto cleanUp;
      }
   }
   
   for (ui = 0; ui < uiNumberOfResets; ui++) {

      Sleep(dwResetSleep);

      NDTLogMsg(_T("Reset"));
      hr = NDTReset(ahAdapter[0]);
      if (FAILED(hr)) {
         rc = TPR_FAIL;
         goto cleanUp;
      }

      NDTLogMsg(_T("Querying connect status"));
      hr = NDTQueryInfo(
         ahAdapter[0], OID_GEN_MEDIA_CONNECT_STATUS, &uiConnectStatus,
         sizeof(UINT), NULL, NULL
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailQueryInfo, hr);
         rc = TPR_FAIL;
      } else if (uiConnectStatus != NdisMediaStateConnected) {
         NDTLogErr(_T("Link is not established when reset is complete"));
         rc = TPR_FAIL;
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
         rc = TPR_FAIL;
         goto cleanUp;
      }
   }

   NDTLogMsg(_T("Unbinding adapters"));
   for (ixAdapter = 0; ixAdapter < nAdapters; ixAdapter++) {
      hr = NDTUnbind(ahAdapter[ixAdapter]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailUnbind, hr);
         rc = TPR_FAIL;
         goto cleanUp;
      }
   }

cleanUp:
  
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
