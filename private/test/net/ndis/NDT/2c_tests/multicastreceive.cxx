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
#include "ndt_2c.h"
#include "utils.h"
#include <ndis.h>

extern UINT  g_uiTestPhyMedium;
extern DWORD g_dwSendWaitTimeout;
extern BOOL g_bNoUnbind;

//------------------------------------------------------------------------------

TEST_FUNCTION(TestMulticastReceive)
{
   TEST_ENTRY;

   INT rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bOk = TRUE;
   BOOL bForce30 = FALSE;
   //This bFilterWarning is used to indicate that miniport driver + its hardware are indicating unwanted multicast packets.
   //Note that it would not be treated as error but warning.
   BOOL bFilterWarning = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;

   UINT   chAdapter = 2;
   HANDLE ahAdapter[2] = {NULL, NULL};
   UINT   ixAdapter = 0;

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
   UINT   uiMaximumPacketSize = 0;
   DWORD  dwReceiveDelay = 0;   

   NDIS_OID oidSize = 0;
   NDIS_OID oidList = 0;

   UINT   cpucMulticastAddr = 256;
   UCHAR* apucMulticastAddr[256];
   UINT   cpucUnusedAddr = 8;
   UCHAR* apucUnusedAddr[264];

   UINT   uiPacketSize = 128;
   UINT   uiPacketsToSend = 10;
   UINT   uiAddressBurst = 0;
   UINT   uiMaxListSize = 0;
   UINT   ix = 0;
   UINT   ixSlot = 0;

   ULONG  ulTotalPacketSent = 0;

   // This test should be skipped for some media types
   if (ndisMedium != NdisMedium802_3 && ndisMedium != NdisMediumFddi) {
      return TPR_SKIP;
   }
   
   // Intro
   NDTLogMsg(_T("Start 1c_MulticastReceive test"));
   NDTLogMsg(_T("The test adapter is %s"), g_szTestAdapter);
   NDTLogMsg(_T("The support adapter is %s"), g_szHelpAdapter);

   if (rand_s((unsigned int*)&ulConversationId) != 0)   {
      NDTLogErr(g_szFailGetConversationId, 0);
      goto cleanUp;
   }

   
   // Zero local variables
   memset(ahAdapter, 0, sizeof(ahAdapter));
   memset(apucMulticastAddr, 0, sizeof(apucMulticastAddr));
   memset(apucUnusedAddr, 0, sizeof(apucUnusedAddr));

   // Get some information about a media
   NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);
   dwReceiveDelay = NDTGetReceiveDelay(ndisMedium);

   switch (ndisMedium) {
   case NdisMedium802_3:
      oidSize = OID_802_3_MAXIMUM_LIST_SIZE;
      oidList = OID_802_3_MULTICAST_LIST;
      break;
   case NdisMediumFddi:
      oidSize = OID_FDDI_LONG_MAX_LIST_SIZE;
      oidList = OID_FDDI_LONG_MULTICAST_LIST;
      break;
   }

   // Open adapters
   NDTLogMsg(_T("Opening adapters"));
   
   // Test
   hr = NDTOpen(g_szTestAdapter, &ahAdapter[0]);
   if (FAILED(hr)) {
      bOk = FALSE;
      NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
      goto cleanUp;
   }

   // Support
   hr = NDTOpen(g_szHelpAdapter, &ahAdapter[1]);
   if (FAILED(hr)) {
      bOk = FALSE;
      NDTLogErr(g_szFailOpen, g_szHelpAdapter, hr);
      goto cleanUp;
   }

   // Binding adapters
   NDTLogMsg(_T("Binding adapters"));
   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {
      hr = NDTBind(ahAdapter[ixAdapter], bForce30, ndisMedium);
      if (FAILED(hr)) {
         bOk = FALSE;
         NDTLogErr(g_szFailBind, hr);
         goto cleanUp;
      }
   }

   // Get basic information
   NDTLogMsg(_T("Get basic adapters info"));

   // Get maximum frame size
   hr = NDTGetMaximumFrameSize(ahAdapter[0], &uiMaximumPacketSize);
   if (FAILED(hr)) {
      bOk = FALSE;
      NDTLogErr(g_szFailGetMaximumFrameSize, hr);
      goto cleanUp;
   }

   hr = NDTQueryInfo(
      ahAdapter[0], oidSize, &uiMaxListSize, sizeof(UINT), NULL, NULL
   );
   if (FAILED(hr)) {
      bOk = FALSE;
      NDTLogErr(g_szFailQueryInfo, hr);
      goto cleanUp;
   }

   if (uiMaxListSize == 0) {
      bOk = FALSE;
      NDTLogErr(_T("List size may not be equal to zero"));
      goto cleanUp;
   }

   NDTLogMsg(_T("%s driver supports %d multicast addresses"), g_szTestAdapter, uiMaxListSize);

   if (uiMaxListSize < 32) {
      NDTLogWrn(_T("Driver must support at least 32 multicast addresses"));
   }

   cpucMulticastAddr = uiMaxListSize;

   if (uiMaxListSize > 256) {
      cpucMulticastAddr = 256;
      NDTLogMsg(
         _T("Driver support %d addresses in list - only %d will be tested"), 
         uiMaxListSize, cpucMulticastAddr
      );
   }

   for (ix = 0; ix < cpucUnusedAddr; ix++) {
      apucUnusedAddr[ix] = NDTGetMulticastAddrEx(ndisMedium, 0x0600 + ix);
   }

   NDTLogMsg(_T("Add multicast addresses"));
   for (ix = 0; ix < cpucMulticastAddr; ix++) {
      // Create multicast addresses
      apucMulticastAddr[ix] = NDTGetMulticastAddrEx(ndisMedium, ix);

      hr = NDTAddMulticastAddr(ahAdapter[0], ndisMedium, apucMulticastAddr[ix]);
      if (FAILED(hr)) {
          //if run with -nounbind then TCP/IP, TCPIP6 must have set their own Mcast addresses.
          //if we get NDIS_STATUS_MULTICAST_FULL then its OK.
          if ((ix) && (g_bNoUnbind) && (NDIS_FROM_HRESULT(hr) == NDIS_STATUS_MULTICAST_FULL))
          {
              delete apucMulticastAddr[ix];
              cpucMulticastAddr = ix;
              NDTLogMsg(_T("NDIS_STATUS_MULTICAST_FULL while adding %d Multicast addr."), (cpucMulticastAddr+1));
              NDTLogMsg(_T("Indicates that %d multicast addresses are already set by other Protocol drivers."), 
                  uiMaxListSize - cpucMulticastAddr);
              break;
          }
          else
          {
              bOk = FALSE;
              NDTLogErr(g_szFailAddMulticastAddr, hr);
              goto cleanUp;
          }
      }
   }      

   NDTLogMsg(_T("Set multicast filter"));
   hr = NDTSetPacketFilter(ahAdapter[0], NDT_FILTER_MULTICAST);
   if (FAILED(hr)) {
      bOk = FALSE;
      NDTLogErr(g_szFailSetPacketFilter, hr);
      goto cleanUp;
   }
   
   while (cpucMulticastAddr >= 0) {

      NDTLogMsg(
         _T("Sending %d packets on %d unused multicast addresses"), 
         uiPacketsToSend, cpucUnusedAddr
      );

      // Start receive
      hr = NDTReceive(ahAdapter[0], ulConversationId, &hReceive);
      if (FAILED(hr)) {
         bOk = FALSE;
         NDTLogErr(g_szFailReceive, hr);
         goto cleanUp;
      }

      ulTotalPacketSent = 0;
      for (ix = 0; ix < cpucUnusedAddr; ix += 8) {
         
         uiAddressBurst = cpucUnusedAddr - ix;
         if (uiAddressBurst > 8) uiAddressBurst = 8;
         hr = NDTSend(
            ahAdapter[1], cbAddr, NULL, uiAddressBurst, apucUnusedAddr + ix, 
            NDT_RESPONSE_NONE, NDT_PACKET_FLAG_GROUP | NDT_PACKET_TYPE_FIXED, 
            uiPacketSize, uiPacketsToSend, ulConversationId, 0, 0, &hSend
         );
         if (FAILED(hr)) {
            bOk = FALSE;
            NDTLogErr(g_szFailSend, hr);
            goto cleanUp;
         }

         // Wait till sending is done
         hr = NDTSendWait(
            ahAdapter[1], hSend, g_dwSendWaitTimeout, &ulSendPacketsSent, 
            &ulSendPacketsCompleted, NULL, NULL, NULL, &ulSendTime, 
            &ulSendBytesSent,  NULL
         );
         if (FAILED(hr)) {
            bOk = FALSE;
            NDTLogErr(g_szFailSendWait, hr);
            goto cleanUp;
         }

         ulTotalPacketSent += ulSendPacketsSent;
      }

      NDTLogMsg(_T("Sent %d packets"), ulTotalPacketSent);
         
      // Wait for while
      Sleep(dwReceiveDelay);

      // Stop receiving and check results
      NDTLogMsg(_T("Stop receiving and checking results"));
      hr = NDTReceiveStop(
         ahAdapter[0], hReceive, &ulRecvPacketsReceived, NULL, NULL, 
         &ulRecvTime,  NULL, &ulRecvBytesRecv
      );
      if (FAILED(hr)) {
         bOk = FALSE;
         NDTLogErr(g_szFailReceiveStop, hr);
         goto cleanUp;
      }

      if (ulRecvPacketsReceived != 0) {
         //it is acceptable if NIC hardware and miniport driver indicates the multicast packets up the stack 
         //those were sent to multicast addresses not set in the multicast address list of the miniport driver. 
         //If NIC hardware and its miniport driver fail to filter out unwanted multicast packets then 
         //the protocol layers like TCP/IP will filter them out. 
         //so don't call it out as error, just do warning.
         //bOk = FALSE;
         NDTLogWrn(_T("Improperly received %d packets"), ulRecvPacketsReceived);
         bFilterWarning = TRUE;
      } else {
         NDTLogMsg(_T("Properly didn't receive packets"));
      }
      
      // There is nothing to send
      if (cpucMulticastAddr == 0) break;
      
      NDTLogMsg(
         _T("Sending %d packets on %d multicast addresses"), uiPacketsToSend,
         cpucMulticastAddr
      );

      // Start receive
      hr = NDTReceive(ahAdapter[0], ulConversationId, &hReceive);
      if (FAILED(hr)) {
         bOk = FALSE;
         NDTLogErr(g_szFailReceive, hr);
         goto cleanUp;
      }

      ulTotalPacketSent = 0;
      for (ix = 0; ix < cpucMulticastAddr; ix += 8) {
         
         uiAddressBurst = cpucMulticastAddr - ix;
         if (uiAddressBurst > 8) uiAddressBurst = 8;
         hr = NDTSend(
            ahAdapter[1], cbAddr, NULL, uiAddressBurst, apucMulticastAddr + ix, 
            NDT_RESPONSE_NONE, NDT_PACKET_FLAG_GROUP | NDT_PACKET_TYPE_FIXED, 
            uiPacketSize, uiPacketsToSend, ulConversationId, 0, 0, &hSend
         );
         if (FAILED(hr)) {
            bOk = FALSE;
            NDTLogErr(g_szFailSend, hr);
            goto cleanUp;
         }

         // Wait till sending is done
         hr = NDTSendWait(
            ahAdapter[1], hSend, g_dwSendWaitTimeout, &ulSendPacketsSent, 
            &ulSendPacketsCompleted, NULL, NULL, NULL, &ulSendTime, 
            &ulSendBytesSent,  NULL
         );
         if (FAILED(hr)) {
            bOk = FALSE;
            NDTLogErr(g_szFailSendWait, hr);
            goto cleanUp;
         }

         ulTotalPacketSent += ulSendPacketsSent;
      }

      Sleep(dwReceiveDelay);

      // Stop receiving and check results
      NDTLogMsg(_T("Stop receiving and checking results"));
      hr = NDTReceiveStop(
         ahAdapter[0], hReceive, &ulRecvPacketsReceived, NULL, NULL, &ulRecvTime, 
         NULL, &ulRecvBytesRecv
      );
      if (FAILED(hr)) {
         bOk = FALSE;
         NDTLogErr(g_szFailReceiveStop, hr);
         goto cleanUp;
      }

      if (NotReceivedOK(ulRecvPacketsReceived,cpucMulticastAddr * uiPacketsToSend,
          g_uiTestPhyMedium,NDIS_PACKET_TYPE_MULTICAST)) {
         bOk = FALSE;
         NDTLogErr(
            _T("Received %d packets but %d was expected"), 
            ulRecvPacketsReceived, cpucMulticastAddr * uiPacketsToSend
         );
      } else {
         NDTLogMsg(_T("Properly received %d packets"), ulRecvPacketsReceived);
      }

      if (cpucMulticastAddr > 128) {
         ix = 64;
      } else if (cpucMulticastAddr > 64) {
         ix = 16;
      } else if (cpucMulticastAddr > 32) {
         ix = 8;
      } else if (cpucMulticastAddr > 16) {
         ix = 4;
      } else {
         ix = 1;
      }

      NDTLogMsg(_T("Removing randomly %d multicast addresses"), ix);
      while (ix > 0) {
         ixSlot = NDTGetRandom(0, cpucMulticastAddr - 1);
         hr = NDTDeleteMulticastAddr(
            ahAdapter[0], ndisMedium, apucMulticastAddr[ixSlot]
         );
         if (FAILED(hr)) {
            bOk = FALSE;
            NDTLogErr(g_szFailDeleteMulticastAddr, hr);
            goto cleanUp;
         }
         apucUnusedAddr[cpucUnusedAddr++] = apucMulticastAddr[ixSlot++];
         while (ixSlot < cpucMulticastAddr) {
            apucMulticastAddr[ixSlot - 1] = apucMulticastAddr[ixSlot];
            ixSlot++;
         }
         cpucMulticastAddr--;
         ix--;
      }

   }

cleanUp:
   // We have deside about test pass/fail there
   rc = !bOk ? TPR_FAIL : TPR_PASS;

   if ((bOk) && (bFilterWarning))
   {
       NDTLogWrn(_T("WARNING: Adapter %s indicates unwanted multicast packets up the stack"), g_szTestAdapter); 
       NDTLogWrn(_T("It indicates packets sent to multicast addresses not set in its multicast address list"));
   }

   NDTLogMsg(_T("Closing adapters"));
   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {
      hr = NDTClose(&ahAdapter[ixAdapter]);
      if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
   }

   for (ix = 0; ix < cpucMulticastAddr; ix++) delete apucMulticastAddr[ix];
   for (ix = 0; ix < cpucUnusedAddr; ix++) delete apucUnusedAddr[ix];

   return rc;
}

//------------------------------------------------------------------------------
