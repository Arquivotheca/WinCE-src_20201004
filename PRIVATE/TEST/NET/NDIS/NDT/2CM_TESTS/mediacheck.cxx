//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

/*********************************************************************************************************************************************************
This test verifies the ability to detect media connects/disconnects.
(This is a manual test that requires the user to disconnect and connect the networking cable.)
It verifies the reconnect is detected properly with long (>30 seconds)
disconnects and that communication with the server is restored.
There are 3 major passes completed in this test:
  Pass 0 = short disconnect event
  Pass 1 = long disconnect event (>30 seconds)
  Pass 2 = disconnect/unload/load/connect
*********************************************************************************************************************************************************/

#include "StdAfx.h"
#include "ShellProc.h"
#include "NDTNdis.h"
#include "NDTMsgs.h"
#include "NDTLog.h"
#include "NDTLib.h"
#include "NDTLibEx.h"
#include "ndt_2cm.h"
#include "utils.h"
#include <ndis.h>
#include "ndterror.h"

//60000 msec
#define _1_min_			(60000)
extern DWORD g_dwMseWaitInMin;

extern UINT  g_uiTestPhyMedium;

TEST_FUNCTION(TestMediaCheck)
{
   TEST_ENTRY;

   INT rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;

   UINT   chAdapter = 1;
   HANDLE ahAdapter[2] = {NULL, NULL};
   HANDLE ahSend[2];
   HANDLE ahReceive[2];

   UINT   ixAdapter = 0;

   UINT   uiMinimumPacketSize = 64;
   UINT   uiMaximumPacketSize = 0;
   DWORD  dwReceiveDelay = 0;   

   HANDLE hSend = NULL;
   ULONG  ulTestPacketsSent = 0;
   ULONG  ulTestPacketsCompleted = 0;
   ULONG  ulTestPacketsCanceled = 0;
   ULONG  ulTestPacketsUnCanceled = 0;
   ULONG  ulTestPacketsReplied = 0;
   ULONG  ulTestSendTime = 0;
   ULONG  ulTestBytesSent = 0;
   ULONG  ulTestBytesReceived = 0;

   HANDLE hReceive = NULL;
   ULONG  ulSuppPacketsReceived = 0;
   ULONG  ulSuppPacketsReplied = 0;
   ULONG  ulSuppPacketsCompleted = 0;
   ULONG  ulSuppRecvTime = 0;
   ULONG  ulSuppBytesSent = 0;
   ULONG  ulSuppBytesReceived = 0;

   UINT   cbAddr = 0;
   UINT   cbHeader = 0;

   UINT   cpucDestAddr = 8;
   UCHAR* apucDestAddr[8];
   UINT   ixDestAddr = 0;
   
   UCHAR* pucPermanentAddr = NULL;

   UINT   uiPacketSize = 128;
   UINT   uiPacketsToSend = 300;
   BOOL   bFixedSize = FALSE;

   UINT   uiBeatDelay = 10;
   UINT   uiBeatGroup = 8;

   UINT cbUsed = 0;
   UINT cbRequired = 0;
   ULONG ulConnectStatus = 0;

   HANDLE hStatus = NULL;

   ULONG ulArrStatus[4];
   ULONG ulStatusIndecies[] = { NDT_COUNTER_STATUS_RESET_START, NDT_COUNTER_STATUS_RESET_END,
	   NDT_COUNTER_STATUS_MEDIA_CONNECT, NDT_COUNTER_STATUS_MEDIA_DISCONNECT };

#define MAX_MEDIACHECK_TEST_PASS (2)
   ULONG ulTestPass = 0;

   // Intro
   NDTLogMsg(_T("Start 2c_MediaCheck test"));
   NDTLogMsg(_T("The test adapter is %s"), g_szTestAdapter);
   NDTLogMsg(_T("The support adapter is %s"), g_szHelpAdapter);

   // Zero local variables
   memset(apucDestAddr, 0, sizeof(apucDestAddr));

   // Get some information about a media
   NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);
   dwReceiveDelay = NDTGetReceiveDelay(ndisMedium);

   // Open adapters
   NDTLogMsg(_T("Opening test adapter"));
   
   // Test
   hr = NDTOpen(g_szTestAdapter, &ahAdapter[0]);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
      goto cleanUp;
   }

   // Binding adapters
   NDTLogMsg(_T("Binding to test adapter"));
   hr = NDTBind(ahAdapter[0], bForce30, ndisMedium);
   if (FAILED(hr)) {
	   NDTLogErr(g_szFailBind, hr);
	   goto cleanUp;
   }

   do 
   {
	   ++ulTestPass;

	   //First start the waiting for event notification.
	   hr = NDTStatus(ahAdapter[0], NDIS_STATUS_MEDIA_DISCONNECT, &hStatus);
	   
	   if (FAILED(hr)) {
		   NDTLogErr(_T("Failed to start receiving notifications on %s Error : 0x%x"), g_szTestAdapter,NDIS_FROM_HRESULT(hr));
		   goto cleanUp;
	   }
	   
	   NDTLogMsg(_T("Started waiting to receive notification on the miniport adapter %s"), g_szTestAdapter);
	   NDTLogMsg(_T("Waiting %d min for disconnect event on %s....."), g_dwMseWaitInMin, g_szTestAdapter);
	   NDTLogMsg(_T("** ATTENTION **: Please ==> ** DISCONNECT ** <== the miniport adapter %s from network"), g_szTestAdapter);
	   
	   hr = NDTStatusWait(ahAdapter[0], hStatus, _1_min_ * g_dwMseWaitInMin);
	   
	   if ( FAILED(hr) || (hr == NDT_STATUS_PENDING) ) {
		   NDTLogErr(_T("Failed to receive NDIS_STATUS_MEDIA_DISCONNECT Error : 0x%x"), NDIS_FROM_HRESULT(hr));
		   NDTLogMsg(_T("Either miniport adapter %s did not indicate it"), g_szTestAdapter);
		   NDTLogMsg(_T("Or user of this test failed to disconnect adapter from network"));
		   hr=NDT_STATUS_FAILURE;
		   goto cleanUp;
	   }
	   
	   // Now Query media status information
	   hr = NDTQueryInfo(ahAdapter[0], OID_GEN_MEDIA_CONNECT_STATUS, &ulConnectStatus,
		   sizeof(ulConnectStatus),&cbUsed,&cbRequired);
	   
	   if (FAILED(hr)) 
	   {
		   NDTLogErr(_T("Unable to query OID_GEN_MEDIA_CONNECT_STATUS with hr=0x%08x"),NDIS_FROM_HRESULT(hr));
		   goto cleanUp;
	   }
	   
	   if (ulConnectStatus == NdisMediaStateConnected)
	   {
		   hr=NDT_STATUS_FAILURE;
		   NDTLogErr(_T("Media status detected as connected."));
		   goto cleanUp;
	   }
	   else if (ulConnectStatus != NdisMediaStateDisconnected)
	   {
		   hr=NDT_STATUS_FAILURE;
		   NDTLogErr(_T("Invalid media status returned from %s"), g_szTestAdapter);
		   goto cleanUp;
	   }
	   else
		   NDTLogMsg(_T("miniport adapter %s notified NdisMediaStateDisconnected properly."), g_szTestAdapter);
	   
	   if (ulTestPass == 2)
	   {
		   NDTLogMsg(_T("This time Waiting for 30 seconds to simulate long disconnect event...."));
		   Sleep(30000);
	   }

	   //First start the waiting for event notification.
	   hr = NDTStatus(ahAdapter[0], NDIS_STATUS_MEDIA_CONNECT, &hStatus);
	   
	   if (FAILED(hr)) {
		   NDTLogErr(_T("Failed to start receiving notifications on %s Error : 0x%x"), g_szTestAdapter,NDIS_FROM_HRESULT(hr));
		   goto cleanUp;
	   }
	   NDTLogMsg(_T("Started waiting to receive notification on the miniport adapter %s"), g_szTestAdapter);
	   NDTLogMsg(_T("Waiting %d min for connect event on %s....."), g_dwMseWaitInMin, g_szTestAdapter);
	   NDTLogMsg(_T("** ATTENTION **: Please ==> ** CONNECT ** <== the miniport adapter %s back to network"), g_szTestAdapter);
	   
	   hr = NDTStatusWait(ahAdapter[0], hStatus, _1_min_ * g_dwMseWaitInMin);
	   
	   if ( FAILED(hr) || (hr == NDT_STATUS_PENDING) ) {
		   NDTLogErr(_T("Failed to receive NDIS_STATUS_MEDIA_CONNECT Error : 0x%x"), NDIS_FROM_HRESULT(hr));
		   NDTLogMsg(_T("Either miniport adapter %s did not indicate it"), g_szTestAdapter);
		   NDTLogMsg(_T("Or user of this test failed to connect adapter back to network"));
		   hr=NDT_STATUS_FAILURE;
		   goto cleanUp;
	   }

	   if (FAILED(hr)) {
		   goto cleanUp;
	   }
	   
	   // Now Query media status information
	   hr = NDTQueryInfo(ahAdapter[0], OID_GEN_MEDIA_CONNECT_STATUS, &ulConnectStatus,
		   sizeof(ulConnectStatus),&cbUsed,&cbRequired);
	   
	   if (FAILED(hr)) 
	   {
		   NDTLogErr(_T("Unable to query OID_GEN_MEDIA_CONNECT_STATUS with hr=0x%08x"),NDIS_FROM_HRESULT(hr));
		   goto cleanUp;
	   }
	   
	   if (ulConnectStatus == NdisMediaStateDisconnected)
	   {
		   hr=NDT_STATUS_FAILURE;
		   NDTLogErr(_T("Media status detected as Disconnected."));
		   goto cleanUp;
	   }
	   else if (ulConnectStatus != NdisMediaStateConnected)
	   {
		   hr=NDT_STATUS_FAILURE;
		   NDTLogErr(_T("Invalid media status returned from %s"), g_szTestAdapter);
		   goto cleanUp;
	   }
	   else
		   NDTLogMsg(_T("miniport adapter %s notified NdisMediaStateConnected properly."), g_szTestAdapter);

   } while (ulTestPass < MAX_MEDIACHECK_TEST_PASS);

   // Get events counter
   NDTLogMsg(_T("Reading Status/Event counters for miniport adapter %s."), g_szTestAdapter);
   for (UINT i=0;i<4;++i)
   {
	   hr = NDTGetCounter(ahAdapter[0], ulStatusIndecies[i], &ulArrStatus[i]);
	   if (FAILED(hr)) {
		   NDTLogErr(g_szFailGetCounter, hr);
		   goto cleanUp;
	   }
   }

   // We are expecting 1 Connect & 1 Disconnect & normally no RESET for 1 connect & disconnect test pass.
   if (ulArrStatus[0] != 0)
	   NDTLogWrn(_T("** WARNING ** miniport adapter %s notified %d RESETs."), g_szTestAdapter, ulArrStatus[0]);
   else
	   NDTLogMsg(_T("miniport adapter %s properly notified NO RESETs."), g_szTestAdapter, ulArrStatus[0]);

   if (ulArrStatus[2] != ulTestPass)
	   NDTLogWrn(_T("** WARNING ** miniport adapter %s notified %d MEDIA_CONNECTs instead of %d."), g_szTestAdapter, ulArrStatus[2], ulTestPass);
   else
	   NDTLogMsg(_T("miniport adapter %s properly notified %d MEDIA_CONNECTs."), g_szTestAdapter, ulArrStatus[2]);

   if (ulArrStatus[3] != ulTestPass)
	   NDTLogWrn(_T("** WARNING ** miniport adapter %s notified %d MEDIA_DISCONNECTs instead of %d."), g_szTestAdapter, ulArrStatus[3], ulTestPass);
   else
	   NDTLogMsg(_T("miniport adapter %s properly notified %d MEDIA_DISCONNECTs."), g_szTestAdapter, ulArrStatus[3]);

   NDTLogMsg(_T(""));
   NDTLogMsg(_T("Verify that test adapter %s communicate to support adapter %s."), g_szTestAdapter, g_szHelpAdapter);

   // Support
   NDTLogMsg(_T("Opening support adapter"));
   hr = NDTOpen(g_szHelpAdapter, &ahAdapter[1]);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailOpen, g_szHelpAdapter, hr);
      goto cleanUp;
   }

   // Binding adapters
   NDTLogMsg(_T("Binding to support adapter"));
   hr = NDTBind(ahAdapter[1], bForce30, ndisMedium);
   if (FAILED(hr)) {
	   NDTLogErr(g_szFailBind, hr);
	   goto cleanUp;
   }

   NDTLogMsg(_T("Checking media status on Support adapter"));

   //Letting support adapter to catch up
   //Query media status information on support adapter
   ulConnectStatus = NdisMediaStateDisconnected;
   for (INT j=0; ( (j<2) && (ulConnectStatus != NdisMediaStateConnected) ) ;j++)
   {
	   Sleep(5000+j*25000);

	   hr = NDTQueryInfo(ahAdapter[1], OID_GEN_MEDIA_CONNECT_STATUS, &ulConnectStatus,
		   sizeof(ulConnectStatus),&cbUsed,&cbRequired);
	   
	   if (FAILED(hr)) 
	   {
		   NDTLogErr(_T("Unable to query OID_GEN_MEDIA_CONNECT_STATUS on Support adapter hr=0x%08x"),NDIS_FROM_HRESULT(hr));
		   goto cleanUp;
	   }
   }

   if (ulConnectStatus != NdisMediaStateConnected)
   {
	   hr=NDT_STATUS_FAILURE;
	   NDTLogErr(_T("Media status detected as ** NOT Connected ** on Support Card"));
	   goto cleanUp;
   }
   else
	   NDTLogMsg(_T("Media status detected as *Connected* on Support Card"));

   // Get basic information
   NDTLogMsg(_T("Get basic adapters info"));

   hr = NDTGetMaximumFrameSize(ahAdapter[0], &uiMaximumPacketSize);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetMaximumFrameSize, hr);
      goto cleanUp;
   }

   hr = NDTGetPermanentAddr(ahAdapter[1], ndisMedium, &pucPermanentAddr);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetPermanentAddr, hr);
      goto cleanUp;
   }

   chAdapter = 2;

   NDTLogMsg(_T("Seting direct receive filters on both adapters"));
   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {
      hr = NDTSetPacketFilter(ahAdapter[ixAdapter], NDT_FILTER_DIRECTED);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailSetPacketFilter, hr);
         goto cleanUp;
      }
   }

   NDTLogMsg(_T("Start sending %d packets at test adapter"), uiPacketsToSend);
   
   // Start stress by running receiving on second adapter
   hr = NDTReceive(ahAdapter[1], &ahReceive[1]);
   if (FAILED(hr)) {
	   NDTLogErr(g_szFailReceive, hr);
	   goto cleanUp;
   }
   
   // Send on first instance
   hr = NDTSend(
	   ahAdapter[0], cbAddr, NULL, 1, &pucPermanentAddr, 
	   NDT_RESPONSE_FULL, (NDT_PACKET_TYPE_FIXED | NDT_PACKET_BUFFERS_RANDOM), 
	   uiPacketSize, uiPacketsToSend, uiBeatDelay,
	   uiBeatGroup, &ahSend[0]
	   );
   if (FAILED(hr)) {
	   NDTLogErr(g_szFailSend, hr);
	   goto cleanUp;
   }
   
   // Wait for send end
   hr = NDTSendWait(
	   ahAdapter[0], ahSend[0], 60000, &ulTestPacketsSent, 
	   &ulTestPacketsCompleted, &ulTestPacketsCanceled, 
	   &ulTestPacketsUnCanceled, &ulTestPacketsReplied, &ulTestSendTime, 
	   &ulTestBytesSent, &ulTestBytesReceived
	   );
   if (FAILED(hr)) {
	   NDTLogErr(g_szFailSendWait, hr);
	   goto cleanUp;
   }
   
   // Log info
   NDTLogMsg(_T("Sent %d packets"), ulTestPacketsSent);
   
   // Wait for while
   Sleep(dwReceiveDelay);
   
   // Stop receiving
   NDTLogMsg(_T("Stop receiving and checking results"));
   hr = NDTReceiveStop(
	   ahAdapter[1], ahReceive[1], &ulSuppPacketsReceived, 
	   &ulSuppPacketsReplied, &ulSuppPacketsCompleted, &ulSuppRecvTime, 
	   &ulSuppBytesSent, &ulSuppBytesReceived
	   );
   if (FAILED(hr)) {
	   NDTLogErr(g_szFailReceiveStop, hr);
	   goto cleanUp;
   }
   
   NDTLogMsg(
	   _T("Test: Sent %d packets with total size %d bytes in %d ms"),
	   ulTestPacketsSent, ulTestBytesSent, ulTestSendTime
	   );
   NDTLogMsg(
	   _T("Test: Recv %d packets with total size %d bytes in %d ms"),
	   ulTestPacketsReplied, ulTestBytesReceived, ulTestSendTime
	   );
   NDTLogMsg(
	   _T("Support: Recv %d packets with total size %d bytes in %d ms"),
	   ulSuppPacketsReceived, ulSuppBytesReceived, ulSuppRecvTime
	   );
   NDTLogMsg(
	   _T("Support: Sent %d packets with total size %d bytes in %d ms"),
	   ulSuppPacketsReplied, ulSuppBytesSent, ulSuppRecvTime
	   );
   
   // Check result
   if (NotReceivedOK(ulSuppPacketsReceived,ulTestPacketsSent,
	   g_uiTestPhyMedium,NDIS_PACKET_TYPE_DIRECTED)) {
	   hr=NDT_STATUS_FAILURE;
	   NDTLogErr(
		   _T("Received %d packets at support adapter but %d was expected"), 
		   ulSuppPacketsReceived, ulTestPacketsSent
		   );
	  }

cleanUp:

   // We have deside about test pass/fail there
   rc = FAILED(hr) ? TPR_FAIL : TPR_PASS;

   // Unbinding adapters
   NDTLogMsg(_T("Unbinding adapters"));
   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {
      hr = NDTUnbind(ahAdapter[ixAdapter]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailUnbind, hr);
      }
   }

   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {
      hr = NDTClose(&ahAdapter[ixAdapter]);
      if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
   }

   delete pucPermanentAddr;
   return rc;
}
