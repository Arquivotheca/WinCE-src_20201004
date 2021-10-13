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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "StdAfx.h"
#include "ShellProc.h"
#include "NDTNdis.h"
#include "NDTMsgs.h"
#include "NDTError.h"
#include "NDTLog.h"
#include "NDTLib.h"
#include "NDTLibEx.h"
#include "ndtlibwlan.h"
#include "ndt_2c_wlan.h"
#include <ndis.h>

unsigned long Delta(LARGE_INTEGER a, LARGE_INTEGER b)
{

   unsigned long res = a.HighPart - b.HighPart;
   if (0 == res)
      res = a.LowPart - b.LowPart;
   else
      res = MAXLONG;
   
   return res;
}

//------------------------------------------------------------------------------

TEST_FUNCTION(TestWlanStatistics)
{
   TEST_ENTRY;

   int rc = TPR_PASS;   
   HRESULT hr = S_OK;

   UINT   chAdapter = 2;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;

   HANDLE ahAdapter[128];
   UINT  nAdapters = 1;
   UINT  ixAdapter = 0;
   UINT  ix = 0;
   DWORD nValue = 0;

   HANDLE hStatus = NULL;
   ULONG ulKeyLength;
   NDIS_802_11_SSID ssidList[3];
   DWORD dwSsidCount;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;
   ULONG ulTimeout;
   UINT cbUsed = 0;
   UINT cbRequired = 0;
   DWORD dwSize = 0;

   NDIS_802_11_STATISTICS nBase,nCurrent;
        
   UINT   cbAddr = 0;
   UINT   cbHeader = 0;   
   DWORD  dwReceiveDelay = 0;   

   UINT   uiMinimumPacketSize = 64;   
   ULONG ulPacketsToSend = 200;
   ULONG ulPacketsSent = 0;
   ULONG ulPacketsReceived = 0;

   ULONG ulSendPacketsCompleted = 0;
   ULONG ulSendTime = 0;
   ULONG ulSendBytesSent = 0;
     
   memset(ahAdapter, 0, sizeof(ahAdapter));

   NDTLogMsg(
      _T("Start 2c_wlan_statistics test\n")
   );

   NDTLogMsg(_T("The test adapter is %s"), g_szTestAdapter);
   NDTLogMsg(_T("The support adapter is %s"), g_szHelpAdapter);



   // Get some information about the media
   NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);
   dwReceiveDelay = NDTGetReceiveDelay(ndisMedium);
  
   
    // Open adapters
   NDTLogMsg(_T("Opening adapters"));
   
   // Test
   hr = NDTOpen(g_szTestAdapter, &ahAdapter[0]);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }

   // Support
   hr = NDTOpen(g_szHelpAdapter, &ahAdapter[1]);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailOpen, g_szHelpAdapter, hr);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }

   // Binding adapters
   NDTLogMsg(_T("Binding adapters"));
   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {
      hr = NDTBind(ahAdapter[ixAdapter], bForce30, ndisMedium);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailBind, hr);
         FlagError(ErrorSevere,&rc);
         goto cleanUp;
      }
   }
   
   NDTLogMsg(_T("Initializing adapters for Wlan test"));
   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {
      hr = NDTWlanInitializeTest(ahAdapter[ixAdapter],ixAdapter);
      if (FAILED(hr)) 
      {
         NDTLogErr(_T("Failed to initialize Wlan %s adapter Error0x%x"),(ixAdapter?_T("Support"):_T("Test")),hr);
         FlagError(ErrorSevere,&rc);
         goto cleanUp;
      }
   }

    hr = NDTWlanGetDeviceType(ahAdapter[0],&WlanDeviceType);
    if (NOT_SUCCEEDED(hr))
    {
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
    }
   
    //Test first AP irrespective of Network Type
   dwSsidCount = 1;
   ssidList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_IBSS);
   memcpy((ssidList[dwSsidCount -1].Ssid), WLAN_IBSS,ssidList[dwSsidCount -1].SsidLength);

     
   // Execute tests by ssid
   for (ix= 0; ix<dwSsidCount; ix++)
   {
      TCHAR szSsidText[33];
      GetSsidText(szSsidText, ssidList[ix]);

      do
      {
         NDTLogMsg(_T("Variation 1: Verify OID_802_11_STATISTICS is supported"));
         dwSize = sizeof(NDIS_802_11_STATISTICS);
         NDIS_802_11_STATISTICS nStats;
         hr = NDTWlanGetStatistics(ahAdapter[0], &nStats, &dwSize);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogWrn(_T("Failed to query OID_802_11_STATISTICS, OID may not be supported Error:0x%x"),NDIS_FROM_HRESULT(hr));
            NDTLogWrn(_T("This is an optional OID but support for it is recommended"));
            break;
         }
         else
            NDTLogDbg(_T("PASS: Succesfully queried OID_802_11_STATISTICS"));
    

         NDTLogMsg(_T("Variation 2: Create adhoc cell on support device and join test device"));
         do
         {       
            ulKeyLength = 10;
            hr = NDTWlanCreateIBSS(ahAdapter[1],  Ndis802_11AuthModeOpen, Ndis802_11WEPEnabled, ulKeyLength,
                  0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix]);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to Create IBSS Error:0x%x"),hr);
               FlagError(ErrorSevere,&rc);
               break;
            }
            else
               NDTLogDbg(_T("Support adapter Successfully created IBSS"));

            ulKeyLength = 10;
            ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
            hr = NDTWlanJoinIBSS(ahAdapter[0], Ndis802_11AuthModeOpen, Ndis802_11WEPEnabled , 
                  ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], &ulTimeout);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to Join IBSS Error:0x%x"),hr);
               FlagError(ErrorSevere,&rc);
               break;
            }      
            else
               NDTLogDbg(_T("Test adapter Succesfully joined IBSS"));
         }
         while(0);

         NDTLogMsg(_T("Variation 3: Verify TransmittedFragmentCount & ReceivedFragmentCount counters are updated"));
         do
         {
            dwSize = sizeof(NDIS_802_11_STATISTICS);
            hr = NDTWlanGetStatistics(ahAdapter[0], &nBase, &dwSize);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to query OID_802_11_STATISTICS, Error:0x%x"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }
            else
               NDTLogDbg(_T("Successfully queried OID_802_11_STATISTICS"));

            // Test to support device
            NDTLogDbg(_T("Test -> Support Directed send"));
            hr = NDTWlanDirectedSend(ahAdapter[0], ahAdapter[1], cbAddr, ndisMedium,
               ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
            if (FAILED(hr))
            {
               NDTLogErr(_T("DirectedSend from test to support failed %s Error:0x%x"),szSsidText,hr);
               FlagError(ErrorSevere,&rc);            
               break;        
            }         

            NDTLogDbg(_T("Directed Send - Packets to send %d Packets sent %d Packets received %d"),ulPacketsToSend, ulPacketsSent, ulPacketsReceived);

            dwSize = sizeof(NDIS_802_11_STATISTICS);
            hr = NDTWlanGetStatistics(ahAdapter[0], &nCurrent, &dwSize);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to query OID_802_11_STATISTICS, Error:0x%x"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }

            if (Delta(nBase.TransmittedFragmentCount,nCurrent.TransmittedFragmentCount) < ulPacketsSent)
               NDTLogWrn(_T("TransmittedFragmentCount was not correctly updated, Current %d  Base %d ulPacketsSent %d"), 
                     nCurrent.TransmittedFragmentCount, nBase.TransmittedFragmentCount, ulPacketsSent);

            dwSize = sizeof(NDIS_802_11_STATISTICS);
            hr = NDTWlanGetStatistics(ahAdapter[0], &nBase, &dwSize);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to query OID_802_11_STATISTICS, Error:0x%x"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }

            // Support to test device
            NDTLogDbg(_T("Support -> Test Directed send"));
            hr = NDTWlanDirectedSend(ahAdapter[1], ahAdapter[0], cbAddr, ndisMedium,
               ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
            if (FAILED(hr))
            {
               NDTLogErr(_T("DirectedSend from support to test failed %s Error:0x%x"),szSsidText,hr);
               FlagError(ErrorSevere,&rc);            
               break;        
            }         

            NDTLogDbg(_T("Directed Send - Packets to send %d Packets sent %d Packets received %d"),ulPacketsToSend, ulPacketsSent, ulPacketsReceived);

            dwSize = sizeof(NDIS_802_11_STATISTICS);
            hr = NDTWlanGetStatistics(ahAdapter[0], &nCurrent, &dwSize);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to query OID_802_11_STATISTICS, Error:0x%x"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }

            if (Delta(nBase.ReceivedFragmentCount,nCurrent.ReceivedFragmentCount) < ulPacketsReceived)
            {
               NDTLogErr(_T("ReceivedFragmentCount was not correctly updated, Current %d Base %d (Expected difference = Packets received %d)"), 
                     nCurrent.ReceivedFragmentCount, nBase.ReceivedFragmentCount, ulPacketsReceived);         
               FlagError(ErrorSevere,&rc);
            }
            else
               NDTLogDbg(_T("PASS Received Fragment count correctly updated"));
         }
         while(0);

         NDTLogMsg(_T("Variation 4: Verify MulticastTransmittedFrameCount & MulticastReceivedFrameCount counters are updated"));
         do
        {       
            dwSize = sizeof(NDIS_802_11_STATISTICS);
            hr = NDTWlanGetStatistics(ahAdapter[0], &nBase, &dwSize);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to query OID_802_11_STATISTICS, Error:0x%x"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }

             // Test to support device
            NDTLogDbg(_T("Test -> Support Broadcast send"));
            hr = NDTWlanBroadcastSend(ahAdapter[0], ahAdapter[1], cbAddr, ndisMedium,
               ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
            if (FAILED(hr))
            {
               NDTLogErr(_T("BroadcastSend from test to support failed %s Error:0x%x"),szSsidText,hr);
               FlagError(ErrorSevere,&rc);            
               break;        
            }         

            NDTLogDbg(_T("BroadcastSend - Packets to send %d Packets sent %d Packets received %d"),ulPacketsToSend, ulPacketsSent, ulPacketsReceived);

            dwSize = sizeof(NDIS_802_11_STATISTICS);
            hr = NDTWlanGetStatistics(ahAdapter[0], &nCurrent, &dwSize);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to query OID_802_11_STATISTICS, Error:0x%x"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }

            if (Delta(nBase.MulticastTransmittedFrameCount,nCurrent.MulticastTransmittedFrameCount) < ulPacketsSent)
               NDTLogWrn(_T("MulticastTransmittedFragmentCount was not correctly updated, Current %d smaller than Base %d"), 
                     nCurrent.MulticastTransmittedFrameCount, nBase.MulticastTransmittedFrameCount);


            dwSize = sizeof(NDIS_802_11_STATISTICS);
            hr = NDTWlanGetStatistics(ahAdapter[0], &nBase, &dwSize);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to query OID_802_11_STATISTICS, Error:0x%x"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }
            
            // Support to test device
            NDTLogDbg(_T("Support -> Test BRoadcast send"));
            hr = NDTWlanBroadcastSend(ahAdapter[1], ahAdapter[0], cbAddr, ndisMedium,
               ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
            if (FAILED(hr))
            {
               NDTLogErr(_T("BroadcastSend from support to test failed %s Error:0x%x"),szSsidText,hr);
               FlagError(ErrorSevere,&rc);            
               break;        
            }         

            NDTLogDbg(_T("BroadcastSend - Packets to send %d Packets sent %d Packets received %d"),ulPacketsToSend, ulPacketsSent, ulPacketsReceived);

            dwSize = sizeof(NDIS_802_11_STATISTICS);
            hr = NDTWlanGetStatistics(ahAdapter[0], &nCurrent, &dwSize);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to query OID_802_11_STATISTICS, Error:0x%x"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }

            if (Delta(nBase.MulticastReceivedFrameCount,nCurrent.MulticastReceivedFrameCount) < ulPacketsReceived)
            {
               NDTLogErr(_T("MulticastReceivedFragmentCount was not correctly updated, Current %d Base %d (Expected difference = Packets received %d)"), 
                     nCurrent.MulticastReceivedFrameCount, nBase.MulticastReceivedFrameCount, ulPacketsReceived);      
               FlagError(ErrorSevere,&rc);
            }
            else
                 NDTLogDbg(_T("PASS Multicast Received fragment count correctly updated"));
         }
         while(0);

         NDTLogMsg(_T("Variation 5: Verify Failedcount is updated"));
         do         
         {       
            UCHAR DummyDestAddr[6] = {0x0,0x1,0x2,0x3,0x4,0x5};
            UCHAR* pucDestAddr[1] = {DummyDestAddr};
               
            HANDLE hSend;
            
            dwSize = sizeof(NDIS_802_11_STATISTICS);
            hr = NDTWlanGetStatistics(ahAdapter[0], &nBase, &dwSize);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to query OID_802_11_STATISTICS, Error:0x%x"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            } 

            NDTLogMsg(_T("Ignore actual send errors for this variation as we are sending to a bogus address"));
            //Send packets to a bogus address to force retries         
            hr = NDTSend(ahAdapter[0], cbAddr, NULL, 1,pucDestAddr, NDT_RESPONSE_NONE, NDT_PACKET_TYPE_FIXED,
               250, ulPacketsToSend, 0, 0, &hSend);

            hr = NDTSendWait(ahAdapter[0], hSend,INFINITE, &ulPacketsSent , &ulSendPacketsCompleted,
                  NULL, NULL, NULL, &ulSendTime, &ulSendBytesSent, NULL);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("SendWait failed Error:0x%x"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }

            dwSize = sizeof(NDIS_802_11_STATISTICS);
            hr = NDTWlanGetStatistics(ahAdapter[0], &nCurrent, &dwSize);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to query OID_802_11_STATISTICS, Error:0x%x"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }

            if (Delta(nBase.FailedCount,nCurrent.FailedCount) < ulPacketsToSend)
            {
               NDTLogErr(_T("FailedCount was not correctly updated Current %d Base %d (Expected atleast ulPacketsSent %d)"), nCurrent.FailedCount, 
                  nBase.FailedCount, ulPacketsToSend);
               FlagError(ErrorSevere,&rc);
               break;
            }
            else
               NDTLogDbg(_T("PASS Failure count was correctly updated"));
         }
         while(0);

         NDTLogMsg(_T("Variation 6: Verify RetryCount and MultipleRetryCount"));
         do
         {
            dwSize = sizeof(NDIS_802_11_STATISTICS);
            hr = NDTWlanGetStatistics(ahAdapter[0], &nBase, &dwSize);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to query OID_802_11_STATISTICS, Error:0x%x"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            } 

            // Test to support device
            hr = NDTWlanDirectedSend(ahAdapter[0], ahAdapter[1], cbAddr, ndisMedium,
               5000, &ulPacketsSent, &ulPacketsReceived);
            if (FAILED(hr))
            {
               NDTLogErr(_T("DirectedSend from test to support failed %s Error:0x%x"),szSsidText,hr);
               FlagError(ErrorSevere,&rc);            
               break;        
            }         

            NDTLogDbg(_T("Directed Send - Packets to send %d Packets sent %d Packets received %d"),ulPacketsToSend, ulPacketsSent, ulPacketsReceived);

            dwSize = sizeof(NDIS_802_11_STATISTICS);
            hr = NDTWlanGetStatistics(ahAdapter[0], &nCurrent, &dwSize);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to query OID_802_11_STATISTICS, Error:0x%x"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }

            if (Delta(nBase.RetryCount,nCurrent.RetryCount) <= 0)
            {
               NDTLogWrn(_T("Retry count was not updated Current %d Base %d"),nCurrent.RetryCount, nBase.RetryCount);
               NDTLogWrn(_T("Driver must update RetryCount after successfully retransmitting one more retransmissions, "));
               NDTLogWrn(_T("it is possible that no retransmission occured during this test, so this is just a warning to"));
               NDTLogWrn(_T("help point out a potential issue, if the driver does implement this counter and no retransmission occured"));
               NDTLogWrn(_T("during this test then the warning can be ignored"));
            }         
            else
                  NDTLogDbg(_T("PASS Retry count correctly updated"));

            if (Delta(nBase.MultipleRetryCount,nCurrent.MultipleRetryCount) <= 0)
            {
               NDTLogWrn(_T("MultipleRetry count was not updated Current %d Base %d"),nCurrent.MultipleRetryCount, nBase.MultipleRetryCount);
               NDTLogWrn(_T("Driver must update MultipleRetryCount after successfully retransmitting more than one retransmissions, "));
               NDTLogWrn(_T("it is possible that no retransmission occured during this test, so this is just a warning to"));
               NDTLogWrn(_T("help point out a potential issue, if the driver does implement this counter and no retransmission occured"));
               NDTLogWrn(_T("during this test then the warning can be ignored"));
            }         
            else
               NDTLogDbg(_T("PASS Multiple Retry count correctly updated"));
         }
         while(0);      

      }
      while(0);

   }   
cleanUp:
 
   NDTWlanCleanupTest(ahAdapter[0]); 
   NDTWlanCleanupTest(ahAdapter[1]); 
  
   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {   
      hr = NDTUnbind(ahAdapter[ixAdapter]);
      if (FAILED(hr)) {
         NDTLogErr(g_szFailUnbind, hr);
      }

      if (ahAdapter[ixAdapter] != NULL) {
      hr = NDTClose(&ahAdapter[ixAdapter]);
      if (FAILED(hr))
         NDTLogErr(g_szFailClose, hr);
      }   
   }   

   return rc;  
}

