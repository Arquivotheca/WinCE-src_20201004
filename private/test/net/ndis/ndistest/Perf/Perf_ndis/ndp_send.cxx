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
#include <windows.h>
#include <tchar.h>
#include "utils.h"
#include "log.h"
#include "ndp_protocol.h"
#include "ndp_lib.h"
#include "globals.h"

/////////////////////////////Externs

extern BOOL  g_receivePeer;
extern HANDLE g_hAdapter;
extern DWORD g_packetsSend;
extern DWORD g_minSize;
extern DWORD g_maxSize;
extern DWORD g_sizeStep;
extern DWORD g_poolSize;
extern teNDPTestType g_eTestType;
extern BOOL  g_RunAsPerfWinsock;
extern DWORD g_AddendumHdrSize;
extern BOOL g_fUseNdisSendOnly;
//------------------------------------------------------------------------------

BOOL ConnectToPeer(HANDLE hAdapter, UCHAR peerAddr[])
{
   BOOL ok = FALSE;
   BOOL bListened = FALSE;
   static UCHAR bcastAddr[ETH_ADDR_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
   UCHAR srcAddr[ETH_ADDR_SIZE];
   DWORD packetType = NDP_PACKET_NONE;
   LONG i;

   // Start listen for offer
   if (!Listen(hAdapter, 8, TRUE, FALSE)) {
      LogErr(_T("Failed start listen for direct packets"));
      goto cleanUp;
   }

   bListened = TRUE;

   // Try get offer from peer 8 times
   for (i = 0; i < 8; i++) {

      // Send LookFor packet
      if (!SendPacket(hAdapter, bcastAddr, NDP_PACKET_LOOKFOR, NULL, 0)) {
         LogErr(_T("Failed send 'LOOKFOR' packet"));
         goto cleanUp;
      }

      LogVbs(_T("Sent 'LOOKFOR' packet"));
      LogVbs(_T("Wait for 'OFFER' packet"));

      // Get offer
      if (!ReceivePacket(
         hAdapter, NDP_TIMEOUT_SHORT, peerAddr, &packetType, NULL, NULL
      )) {
         LogWrn(_T("Timeout on 'OFFER' packet"));
         continue;
      }
      
      // We get an offer
      if (packetType == NDP_PACKET_OFFER) break;
   }

   // Stop listen
   if (!Listen(hAdapter, 0, FALSE, FALSE)) {
      LogErr(_T("Failed stop listen for direct packets"));
      goto cleanUp;
   }

   bListened = FALSE;

   // We don't get offer
   if (packetType != NDP_PACKET_OFFER) goto cleanUp;

   LogVbs(
      _T("Get 'OFFER' packet from %02x:%02x:%02x:%02x:%02x:%02x"),
      peerAddr[0], peerAddr[1], peerAddr[2], peerAddr[3], peerAddr[4],
      peerAddr[5], peerAddr[6]
   );

   // Start listen for direct packets
   if (!Listen(hAdapter, 8, TRUE, FALSE)) {
      LogErr(_T("Failed start listen for direct packets"));
      goto cleanUp;
   }

   bListened = TRUE;
   
   // Send Accept packet
   if (!SendPacket(hAdapter, peerAddr, NDP_PACKET_ACCEPT, NULL, 0)) {
      LogErr(_T("Failed send 'ACCEPT' packet"));
      goto cleanUp;
   }

   LogVbs(_T("Sent 'ACCEPT' packet"));
   LogVbs(_T("Wait for 'OK' packet"));

   do {
      if (!ReceivePacket(
         hAdapter, NDP_TIMEOUT_SHORT, srcAddr, &packetType, NULL, NULL
      )) {
         LogErr(_T("Timeout waiting for 'OK' packet"));
      }
   } while (
      packetType != NDP_PACKET_OK || memcmp(peerAddr, srcAddr, 6) != 0
   );

   LogVbs(_T("Received 'OK' packet"));
   
   ok = TRUE;

cleanUp:
   if (bListened)
   {
	   bListened = FALSE;
	   // Stop listen for direct packets
	   if (!Listen(hAdapter, 0, FALSE, FALSE))
		   LogErr(_T("Failed stop listen for direct packets"));
   }

   return ok;
}

//------------------------------------------------------------------------------

BOOL ContinueWithPeer(HANDLE hAdapter, UCHAR peerAddr[])
{
   BOOL ok = FALSE; 
   UCHAR srcAddr[ETH_ADDR_SIZE];
   DWORD packetType = NDP_PACKET_NONE;
   BOOL bListened = FALSE;

   // Start listen for direct packets
   if (!Listen(hAdapter, 8, TRUE, FALSE)) {
      LogErr(_T("Failed start listen for direct packets"));
      goto cleanUp;
   }
   
   bListened = TRUE;

   // Send NEXT packet
   if (!SendPacket(hAdapter, peerAddr, NDP_PACKET_NEXT, NULL, 0)) {
      LogErr(_T("Failed send 'NEXT' packet"));
      goto cleanUp;
   }

   LogVbs(_T("Sent 'NEXT' packet"));
   LogVbs(_T("Wait for 'OK' packet"));

   // Wait for OK packet
   do {
      if (!ReceivePacket(
         hAdapter, NDP_TIMEOUT_SHORT, srcAddr, &packetType, NULL, NULL
      )) {
         LogErr(_T("Timeout waiting for 'OK' packet"));
      }
   } while (
      packetType != NDP_PACKET_OK || memcmp(peerAddr, srcAddr, 6) != 0
   );

   LogVbs(_T("Received 'OK' packet"));

   ok = TRUE;
   
cleanUp:
   if (bListened)
   {
	   bListened = FALSE;
	   // Stop listen for direct packets
	   if (!Listen(hAdapter, 0, FALSE, FALSE)) 
		   LogErr(_T("Failed start listen for direct packets"));
   }

   return ok;
}

//------------------------------------------------------------------------------

BOOL DisconnectFromPeer(HANDLE hAdapter, UCHAR peerAddr[])
{
   BOOL ok = FALSE; 
   
   // Send Accept packet
   if (!SendPacket(hAdapter, peerAddr, NDP_PACKET_EXIT, NULL, 0)) {
      LogErr(_T("Failed send 'EXIT' packet"));
      goto cleanUp;
   }

   LogVbs(_T("Sent 'EXIT' packet"));

   ok = TRUE;
   
cleanUp:
   return ok;
}

//------------------------------------------------------------------------------
#ifdef NDIS60
BOOL SendIterationWithPeer(
   HANDLE hAdapter, PtsNDPParams pNDPParams, UCHAR peerAddr[], DWORD packetSize, 
   DWORD packetsSend, LONGLONG *pSendTime, LONGLONG *pSendIdleTime,
   DWORD *pPacketsSent, DWORD *pBytesSent, LONGLONG *pRecvTime, 
   LONGLONG *pRecvIdleTime, DWORD *pPacketsReceived, DWORD *pBytesReceived
) 
#else
BOOL SendIterationWithPeer(
   HANDLE hAdapter, PtsNDPParams pNDPParams, UCHAR peerAddr[], DWORD packetSize, 
   DWORD packetsSend, DWORD *pSendTime, DWORD *pSendIdleTime,
   DWORD *pPacketsSent, DWORD *pBytesSent, DWORD *pRecvTime, 
   DWORD *pRecvIdleTime, DWORD *pPacketsReceived, DWORD *pBytesReceived
) 
#endif
{
   BOOL ok = FALSE;
   DWORD packetType = 0;
   UCHAR srcAddr[6];
   NDP_STRESS_RESULT receiveResult;
   DWORD receiveResultSize;
   BOOL bListened = FALSE;

#ifdef NDIS60
   LONGLONG *pSendRecvTime, *pSendRecvIdleTime;
#else
   DWORD *pSendRecvTime, *pSendRecvIdleTime;
#endif

   DWORD *pPacketsSentRecv ,*pBytesSentRecv;

   // Start listen for READY
   if (!Listen(hAdapter, 8, TRUE, FALSE)) {
      LogErr(_T("Failed start listen for direct packets"));
      goto cleanUp;
   }

   bListened = TRUE;
   
   // Send PARAMS packet (poolSize for now)
   if (!SendPacket(
      hAdapter, peerAddr, NDP_PACKET_PARAMS, (UCHAR*)pNDPParams, sizeof(*pNDPParams)
   )) {
      LogErr(_T("Failed send 'PARAMS' packet"));
      goto cleanUp;
   }

   LogVbs(_T("Sent 'PARAMS' packet"));
   LogVbs(_T("Wait for 'READY' packet"));
   
   // Get results
   do {
      if (!ReceivePacket(
         hAdapter, NDP_TIMEOUT_SHORT, srcAddr, &packetType, NULL, NULL
      )) {
         LogErr(_T("Timeout waiting for 'READY' packet"));
      }
   } while (
      packetType != NDP_PACKET_OK || 
      memcmp(peerAddr, srcAddr, ETH_ADDR_SIZE) != 0
   );

   LogVbs(_T("Got 'OK' packet"));

   switch(pNDPParams->dwNDPTestType)
   {
   case SEND_THROUGHPUT:

	   //Wait for ready packet.
	   do {
		   if (!ReceivePacket(
			   hAdapter, NDP_TIMEOUT_SHORT, srcAddr, &packetType, NULL, NULL
			   )) {
			   LogErr(_T("Timeout waiting for 'READY' packet"));
		   }
	   } while (
		   packetType != NDP_PACKET_READY || 
		   memcmp(peerAddr, srcAddr, ETH_ADDR_SIZE) != 0
		   );
	   
	   if (!Listen(hAdapter, 0, FALSE, FALSE)) {
		   LogErr(_T("Failed stop listen for direct packets"));
		   goto cleanUp;
	   }

	   bListened = FALSE;

	   LogVbs(_T("Got 'READY' packet"));

	   Sleep(NDP_TIMEOUT_DELAY);
	   
	   LogVbs(_T("Start send 'STRESS' packets"));
	   
	   // Send packets   
	   if (!StressSend(
		   hAdapter, g_fUseNdisSendOnly, peerAddr, NDP_PACKET_STRESS, pNDPParams->dwPoolSize, packetSize, 
		   packetsSend, 0,0,0,pSendTime, pSendIdleTime, pPacketsSent, pBytesSent
		   )) {
		   LogErr(_T("Stress send failed"));
		   goto cleanUp;
	   }
	   
	   LogVbs(_T("Sent 'STRESS' packets"));
	   
	   // Wait a while before we end stress iteration
	   Sleep(NDP_TIMEOUT_SHORT);

	   // Start listen for results
	   if (!Listen(hAdapter, 8, TRUE, FALSE)) {
		   LogErr(_T("Failed start listen for direct packets"));
		   goto cleanUp;
	   }

	   bListened = TRUE;

	   // Send DONE packet
	   if (!SendPacket(g_hAdapter, peerAddr, NDP_PACKET_DONE, NULL, 0)) {
		   LogErr(_T("Failed send 'DONE' packet"));
		   goto cleanUp;
	   }
	   
	   LogVbs(_T("Sent 'DONE' packet"));

	   //Wait for Done packet from peer.
	   do {
		   if (!ReceivePacket(
			   hAdapter, NDP_TIMEOUT_SHORT, srcAddr, &packetType, NULL, NULL
			   )) {
			   LogErr(_T("Timeout waiting for 'DONE' packet"));
		   }
	   } while (
		   packetType != NDP_PACKET_DONE || 
		   memcmp(peerAddr, srcAddr, ETH_ADDR_SIZE) != 0
		   );

	   // Wait a while before we end stress iteration
	   //Sleep(NDP_TIMEOUT_DELAY);
	   
	   pSendRecvTime = pRecvTime;
	   pSendRecvIdleTime = pRecvIdleTime;
	   pPacketsSentRecv = pPacketsReceived;
	   pBytesSentRecv = pBytesReceived;

	   ok = TRUE;
	   
	   break;
   case RECV_THROUGHPUT:

	   if (!Listen(hAdapter, 0, FALSE, FALSE)) {
		   LogErr(_T("Failed stop listen for direct packets"));
		   goto cleanUp;
	   }

	   bListened = FALSE;

	   // Send 'READY' packet
	   if (!SendPacket(hAdapter, peerAddr, NDP_PACKET_READY, NULL, 0)) {
		   LogErr(_T("Failed send 'OK' packet"));
		   goto cleanUp;
	   }
	   
	   LogVbs(_T("Sent 'READY' packet"));

	   LogVbs(_T("Wait for 'STRESS' packets"));
	   
	   // Start stress receive
	   if (!StressReceive(
		   hAdapter, pNDPParams->dwPoolSize, NDP_PACKET_STRESS, peerAddr, &packetType, NULL,
		   NULL, pRecvTime, pRecvIdleTime, pPacketsReceived, pBytesReceived
		   )) {
		   LogErr(_T("Failed start stress receive"));
		   goto cleanUp;
	   }
	   
	   LogVbs(_T("Done with 'STRESS' packets"));

	   // Start listen for results
	   if (!Listen(hAdapter, 8, TRUE, FALSE)) {
		   LogErr(_T("Failed start listen for direct packets"));
		   goto cleanUp;
	   }

	   bListened = TRUE;

	   // Send DONE packet
	   if (!SendPacket(g_hAdapter, peerAddr, NDP_PACKET_DONE, NULL, 0)) {
		   LogErr(_T("Failed send 'DONE' packet"));
		   goto cleanUp;
	   }
	   
	   pSendRecvTime = pSendTime;
	   pSendRecvIdleTime = pSendIdleTime;
	   pPacketsSentRecv = pPacketsSent;
	   pBytesSentRecv = pBytesSent;

	   ok = TRUE;
	   break;

   case PING_THROUGHPUT:
	   break;
   }

   // Send GET ME RESULT packet
   if (!SendPacket(g_hAdapter, peerAddr, NDP_PACKET_RESULT, NULL, 0)) {
	   LogErr(_T("Failed send 'DONE' packet"));
	   goto cleanUp;
   }
   
   LogVbs(_T("Sent 'RESULT' packet"));
   
   // Wait a while before we end stress iteration
   Sleep(NDP_TIMEOUT_DELAY);
   
   LogVbs(_T("Wait for 'RESULT' packet"));
   
   // Get results
   do {
	   receiveResultSize = sizeof(receiveResult);
	   if (!ReceivePacket(
		   hAdapter, NDP_TIMEOUT_SHORT, srcAddr, &packetType, 
		   (UCHAR*)&receiveResult, &receiveResultSize
		   )) {
		   LogErr(_T("Timeout waiting for 'RESULT' packet"));
	   }
   } while (
	   packetType != NDP_PACKET_RESULT || memcmp(peerAddr, srcAddr, 6) != 0
	   );
   
   if (receiveResultSize < sizeof(receiveResult)) {
	   LogErr(_T("Received 'RESULT' packet has improper size"));
	   goto cleanUp;
   }
   
   LogVbs(_T("Get 'RESULT' packet"));
   
   *pSendRecvTime = receiveResult.time;
   *pSendRecvIdleTime = receiveResult.idleTime;
   *pPacketsSentRecv = receiveResult.packetsReceived;
   *pBytesSentRecv = receiveResult.bytesReceived;
   
   ok = TRUE;
   
cleanUp:
   if (bListened)
   {
	   bListened = FALSE;
	   if (!Listen(hAdapter, 0, FALSE, FALSE))
		   LogErr(_T("Failed stop listen for direct packets"));
   }

   return ok;
}

//------------------------------------------------------------------------------
#ifdef NDIS60
BOOL SendIteration(
   HANDLE hAdapter, DWORD poolSize, DWORD packetSize, DWORD packetsSend, 
   LONGLONG *pSendTime, LONGLONG *pSendIdleTime, DWORD *pPacketsSent, 
   DWORD *pBytesSent
) 
#else
BOOL SendIteration(
   HANDLE hAdapter, DWORD poolSize, DWORD packetSize, DWORD packetsSend, 
   DWORD *pSendTime, DWORD *pSendIdleTime, DWORD *pPacketsSent, 
   DWORD *pBytesSent
) 
#endif
{
   BOOL ok = FALSE;
   static UCHAR dstAddr[] = {0x01, 0x02, 0x4, 0x06, 0x08, 0x0A};


   LogVbs(_T("Start send 'STRESS' packets"));

   // Send packets   
   if (!StressSend(
      hAdapter, g_fUseNdisSendOnly, dstAddr, NDP_PACKET_STRESS, poolSize, packetSize, 
      packetsSend, 0,0,0, pSendTime, pSendIdleTime, pPacketsSent, pBytesSent
    )) {
      LogErr(_T("Stress send failed"));
      goto cleanUp;
   }

   LogVbs(_T("Sent 'STRESS' packets"));

   ok = TRUE;

cleanUp:
   return ok;
}

//------------------------------------------------------------------------------

int SendPerf()
{
   UCHAR peerAddr[ETH_ADDR_SIZE];
   DWORD packetSize;
   #ifdef NDIS60
   LONGLONG sendTime, sendIdleTime;
   LONGLONG receiveTime, receiveIdleTime;
   #else
   DWORD sendTime, sendIdleTime;
   DWORD receiveTime, receiveIdleTime;
   #endif
   LONG lSendTime, lSendIdleTime;
   DWORD packetsSent, bytesSent;
   LONG lReceiveTime, lReceiveIdleTime;
   DWORD packetsReceived, bytesReceived;
   float fBytesSent, fBytesReceived;
   
   int rc = TPR_FAIL;
   tsNDPParams sNDPParams = {g_poolSize,g_eTestType};

   float fMaxPeerSendSpeed = 0;
   float fRatio = 0;
   DWORD dwPercent = 100;
   BOOL bFindMaxPeerSendSpeed = FALSE;
   BOOL bFlagRecvTPDone = FALSE;
   
   // Connect to receive peer
   if (g_receivePeer) {
      if (!ConnectToPeer(g_hAdapter, peerAddr)) goto cleanUp;
   }

   packetSize = g_minSize;
   while (packetSize <= g_maxSize) {

	   if (g_receivePeer) {

		   if (sNDPParams.dwNDPTestType == RECV_THROUGHPUT)
		   {
			   dwPercent = 100;
			   bFindMaxPeerSendSpeed = FALSE;
			   bFlagRecvTPDone = FALSE;

			   sNDPParams.dwPacketSize = packetSize;
			   sNDPParams.dwNosOfPacketsToSend = g_packetsSend;
			   sNDPParams.dwFlagBurstControl = 0;
			   sNDPParams.dwDelayInABurst = 0;
			   sNDPParams.dwPacketsInABurst = 0;
		   }
		   else
			   bFlagRecvTPDone = TRUE;

		   do
		   {
			   if (!SendIterationWithPeer(
				   g_hAdapter, &sNDPParams, peerAddr, packetSize, g_packetsSend, 
				   &sendTime, &sendIdleTime, &packetsSent, &bytesSent, &receiveTime, 
				   &receiveIdleTime, &packetsReceived, &bytesReceived
				   )) goto cleanUp;

			   lSendTime = (long) sendTime;
			   lSendIdleTime = (long) sendIdleTime;
			   lReceiveTime = (long) receiveTime;
			   lReceiveIdleTime = (long) receiveIdleTime;
			   fBytesSent = (float) bytesSent;
			   fBytesReceived = (float) bytesReceived;
			   
			   LogMsg(
				   _T("Send %6d packets : size %4d bytes : pool %d packets : ")
				   _T("%5d ms : %5.1f MB : %4.1f Mbps : %8.1f pps : %4.1f idle"), 
				   packetsSent, packetSize, g_poolSize, lSendTime, fBytesSent/1048576.0,
				   (fBytesSent * 8.0 * 1000.0)/(lSendTime*1048576.0), 
				   packetsSent * 1000.0/lSendTime, (100.0 * lSendIdleTime)/lSendTime
				   );
			   LogMsg(
				   _T("Recv %6d packets : size %4d bytes : pool %d packets : ")
				   _T("%5d ms : %5.1f MB : %4.1f Mbps : %8.1f pps : %4.1f idle : ")
				   _T("%4.1f lost"), 
				   packetsReceived, packetSize, g_poolSize, lReceiveTime, 
				   fBytesReceived/1048576.0, 
				   (fBytesReceived * 8000.0)/(lReceiveTime*1048576.0), 
				   packetsReceived * 1000.0/lReceiveTime, 
				   (100.0*lReceiveIdleTime)/lReceiveTime, 
				   100.0 - (fBytesReceived*100.0) /fBytesSent
				   );
			   
			   if (sNDPParams.dwNDPTestType == RECV_THROUGHPUT)
			   {
				   if (!bFindMaxPeerSendSpeed)
				   {
					   bFindMaxPeerSendSpeed = TRUE;
					   //Speed in Bytes/second
					   fMaxPeerSendSpeed = float(bytesSent * 1000.0)/(sendTime);
					   sNDPParams.dwFlagBurstControl = 1;
					   LogMsg(_T("\t\t Max Send throughput by daemon = %8.2f bytes/sec"),fMaxPeerSendSpeed);
				   }
				   
				   LogMsg(_T("\t\t Packets Sent = %d, Packets Received = %d"),packetsSent,packetsReceived);
				   
				   if (packetsReceived < packetsSent)
				   {
					   dwPercent = dwPercent - 5;
					   if (dwPercent <= 0)
					   {
						   bFlagRecvTPDone = TRUE;
						   break;
					   }
					   
					   fRatio = float(sNDPParams.dwPacketSize) * float(1000.0) * ((float(100.0)/float(dwPercent)) -1)/fMaxPeerSendSpeed;
					   LogMsg(_T("\t\t Offsetting daemon's  Send throughput to %d percent of MaxSpeed, delay/packets = %8.2f"),
						   dwPercent,fRatio);
					   
					   if (fRatio >= 1.0)
					   {
						   sNDPParams.dwDelayInABurst = DWORD (fRatio * 10.0);
						   sNDPParams.dwPacketsInABurst = 10;
					   }
					   else
					   {
						   sNDPParams.dwPacketsInABurst = DWORD (10.0/fRatio);
						   sNDPParams.dwDelayInABurst = 10;
					   }
					   
					   LogMsg(_T("\t\t Packets in a burst = %d, delay in a burst = %d"),
						   sNDPParams.dwPacketsInABurst,sNDPParams.dwDelayInABurst);

					   if (!ContinueWithPeer(g_hAdapter, peerAddr)) 
						   goto cleanUp;
				   }
				   else
					   bFlagRecvTPDone = TRUE;
			   }
			   else
				   bFlagRecvTPDone = TRUE;
			   
		   } while (!bFlagRecvTPDone);
			   
	   } else {
		   
		   if (!SendIteration(
			   g_hAdapter, g_poolSize, packetSize, g_packetsSend, &sendTime, 
			   &sendIdleTime, &packetsSent, &bytesSent
			   )) goto cleanUp;
		   
		   LogMsg(
			   _T("Send %6d packets : size %4d bytes : pool %d packets : ")
			   _T("%5d ms : %4.1f Mbps : %8.1f pps : %4.1f idle"), 
			   packetsSent, packetSize, g_poolSize, (long) sendTime, 
			   (bytesSent * 8.0 * 1000.0)/(((long) sendTime)*1048576.0), 
			   packetsSent * 1000.0/((long) sendTime), (100.0 * ((long) sendIdleTime))/((long) sendTime)
			   );
		   
		   }

	 if (packetSize >= g_maxSize) break;	  
	  
	  if (!g_RunAsPerfWinsock)
	  {
		  packetSize += g_sizeStep;
	  }
	  else
	  {
		  g_sizeStep = g_sizeStep << 1;
		  packetSize = g_sizeStep + g_AddendumHdrSize;
	  }

      if (packetSize > ETH_MAX_FRAME_SIZE) packetSize = ETH_MAX_FRAME_SIZE;

      if (packetSize <= g_maxSize && g_receivePeer) {
         if (!ContinueWithPeer(g_hAdapter, peerAddr)) goto cleanUp;
      }
   }

   // Disconnect from peer
   if (g_receivePeer) {
      if (!DisconnectFromPeer(g_hAdapter, peerAddr)) goto cleanUp;
   }

   rc = TPR_PASS;
cleanUp:
   return rc;
}

//------------------------------------------------------------------------------
