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

// This function is used to place a device in infrastructre mode before we move to adhoc
void ResetDevice(HANDLE hAdapter)
{
   NDIS_802_11_SSID ssid;
   ULONG ulKeyLength = 10;
   ULONG ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
   DWORD dwEncryption = Ndis802_11WEPEnabled;
   HRESULT hr;
   
   ssid.SsidLength = strlen((char *)WLAN_WEP_AP1);
   memcpy(ssid.Ssid,WLAN_WEP_AP1,ssid.SsidLength);
      
   hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
           ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssid, FALSE ,&ulTimeout);         
   if (NOT_SUCCEEDED(hr))
   {
      TCHAR szSsidText[33];
      GetSsidText(szSsidText, ssid);
      NDTLogErr(_T("Inside Reset before AdHoc -Failed to associate with %s Error:0x%x"),szSsidText,hr);      
   }   
}

//------------------------------------------------------------------------------

TEST_FUNCTION(TestWlanAdhoc)
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
   DWORD dwEncryption;
   ULONG ulTimeout;
   ULONG ulConnectStatus;
   UINT cbUsed = 0;
   UINT cbRequired = 0;

   UINT   cbAddr = 0;
   UINT   cbHeader = 0;   
   DWORD  dwReceiveDelay = 0;   

   UINT   uiMinimumPacketSize = 64;   
   ULONG ulPacketsToSend = 50;
   ULONG ulPacketsSent = 0;
   ULONG ulPacketsReceived = 0;
   
   memset(ahAdapter, 0, sizeof(ahAdapter));

   NDTLogMsg(
      _T("Start 2c_wlan_adhoc test\n")
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
   
   // Initialize Wlan adapters
   NDTLogMsg(_T("Initializing adapters for Wlan test"));
   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {
      hr = NDTWlanInitializeTest(ahAdapter[ixAdapter], ixAdapter);
      if (FAILED(hr)) 
      {
		  NDTLogErr(_T("Failed to initialize %s Wlan adapter Error0x%x"),((ixAdapter==0)? _T("Test"):_T("Support")), hr);
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
         ULONG ulMinDirectedPass = g_dwDirectedPasspercentage * ulPacketsToSend /100;
         // First  Variation        
         NDTLogMsg(_T("Variation 1: Verify directed send\recv in IBSS mode\n"));
         NDTLogMsg(_T("This test will create and joing an IBSS cell then verify directed packets can be sent\received\n"));

         ResetDevice(ahAdapter[0]);
         ResetDevice(ahAdapter[1]);

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
            
         if (ulPacketsReceived < ulMinDirectedPass)
         {
            NDTLogErr(_T("Received less than the required amount of directed packets with WLAN Directed send pass"));
            NDTLogErr(_T("Percentage at %d (Received: %d, Expected: %d)"),g_dwDirectedPasspercentage,ulPacketsReceived, ulMinDirectedPass);
            FlagError(ErrorSevere,&rc);
         }
         else
            NDTLogDbg(_T("PASS: Directed send %d packets sent %d packets received"),ulPacketsSent,ulPacketsReceived);

         // Support to Test device
         NDTLogDbg(_T("Support -> Test Directed send"));
          hr = NDTWlanDirectedSend(ahAdapter[1], ahAdapter[0], cbAddr, ndisMedium,
            ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
         if (FAILED(hr))
         {
            NDTLogErr(_T("DirectedSend from support to test failed %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;        
         }

         if (ulPacketsReceived < ulMinDirectedPass)
         {
            NDTLogErr(_T("Received less than the required amount of directed packets with WLAN Directed send pass"));
            NDTLogErr(_T("Percentage at %d (Received: %d, Expected: %d)"),g_dwDirectedPasspercentage,ulPacketsReceived, ulMinDirectedPass);
            FlagError(ErrorSevere,&rc);
         } 
         else
            NDTLogDbg(_T("PASS: Directed send %d packets sent %d packets received"),ulPacketsSent,ulPacketsReceived);            
            
      }
      while(0);

      // Second Variation      
      NDTLogMsg(_T("Variation 2: Verify broadcast send\recv in IBSS mode\n"));
      NDTLogMsg(_T("This test will create and join an IBSS and verify broadcast packets can be sent and received\n "));

      do
      {
         ulPacketsToSend = 50;
         ulPacketsSent = 0;
         ulPacketsReceived = 0;
         ULONG ulMinBroadcastPass = WLAN_PERCENT_TO_PASS_BROADCAST* ulPacketsToSend /100;

         ResetDevice(ahAdapter[0]);
         ResetDevice(ahAdapter[1]);

      
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
   
         // Test to support
         NDTLogDbg(_T("Test -> Support Broadcast send"));
         hr = NDTWlanBroadcastSend(ahAdapter[0], ahAdapter[1], cbAddr, ndisMedium,
                          ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("BroadcastSend failed Test to support %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;        
         }         
         
         if (ulPacketsReceived < ulMinBroadcastPass)
         {
            NDTLogErr(_T("Test -> Support -Received less than the required amount of broadcast packets with WLAN Broadcast"));
            NDTLogErr(_T("send pass Percentage at %d (Received: %d, Expected: %d)"),WLAN_PERCENT_TO_PASS_BROADCAST,ulPacketsReceived, ulMinBroadcastPass);
            FlagError(ErrorSevere,&rc);
         }
         else
            NDTLogDbg(_T("PASS: Broadcast send %d packets sent %d packets received"),ulPacketsSent,ulPacketsReceived);            
                   

         // Support to test
         NDTLogDbg(_T("Support -> Test Broadcast send"));
         hr = NDTWlanBroadcastSend(ahAdapter[1], ahAdapter[0], cbAddr, ndisMedium,
                          ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("BroadcastSend failed Test to support %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;        
         }         
         
         if (ulPacketsReceived < ulMinBroadcastPass)
         {
            NDTLogErr(_T("Support -> test -Received less than the required amount of broadcast packets with WLAN Broadcast"));
            NDTLogErr(_T("send pass Percentage at %d (Received: %d, Expected: %d)"),WLAN_PERCENT_TO_PASS_BROADCAST,ulPacketsReceived, ulMinBroadcastPass);
            FlagError(ErrorSevere,&rc);
         }      
         else
             NDTLogDbg(_T("PASS: Broadcast send %d packets sent %d packets received"),ulPacketsSent,ulPacketsReceived);            
       
      }
      while(0);


      // Third Variation
      
      NDTLogMsg(_T("Variation 3: Verify media connect events are not indicated when creating first cell\n"));
      NDTLogMsg(_T("This test will create an IBSS cell and verify a media connect event is not indicated\n "));         
      NDTLogMsg(_T("and verify the driver does not change to media connect status to connected\n"));

      do
      {         
         ResetDevice(ahAdapter[0]);
         ResetDevice(ahAdapter[1]);
         ULONG ulConnects;

         ulKeyLength = 10;
         hr = NDTWlanCreateIBSS(ahAdapter[0],  Ndis802_11AuthModeOpen, Ndis802_11WEPEnabled, ulKeyLength,
               0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to Create IBSS Error:0x%x"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("Support adapter Successfully created IBSS"));

         hr = NDTGetCounter(ahAdapter[0], NDT_COUNTER_STATUS_MEDIA_CONNECT, &ulConnects);
         if (FAILED(hr)) {
            NDTLogErr(g_szFailGetCounter, hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

         if (0 != ulConnects)
         {
              NDTLogErr(_T("No media connect event should have been indicated (Received: %d)"), ulConnects);
              NDTLogErr(_T("Driver must not indicate a media connect event when creating the first cell in an IBSS\n"));
              FlagError(ErrorSevere,&rc);
         }
         else
            NDTLogDbg(_T("PASS: No media connect indicated"));

         ulConnectStatus = 0;
         cbUsed = 0;
         cbRequired = 0;
         hr = NDTQueryInfo(ahAdapter[0], OID_GEN_MEDIA_CONNECT_STATUS, &ulConnectStatus,
                  sizeof(ulConnectStatus),&cbUsed,&cbRequired);
         if (NOT_SUCCEEDED(hr)) 
         {
            NDTLogErr(_T("Unable to query OID_GEN_MEDIA_CONNECT_STATUS with hr=0x%08x\n"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

         if (ulConnectStatus != NdisMediaStateDisconnected)
        {
            hr=NDT_STATUS_FAILURE;
            NDTLogErr(_T("Media connect status should have been disconnect. \n"));
            NDTLogErr(_T("Driver must not set change to media connect status connected when its the first cell in an IBSS\n"));
            FlagError(ErrorSevere,&rc);
            break;
         }            
         else
            NDTLogDbg(_T("PASS: media disconnected"));

      }
      while(0);


	// Fourth Variation
      
      NDTLogMsg(_T("Variation 4: Verify a media connect event is indicated by both IBSS nodes after joining\n"));
      NDTLogMsg(_T("This test will verify that both test and support device indicate a media connect and are "));
      NDTLogMsg(_T("media status connected after a node joins the cell\n "));

      do
      {
         ULONG ulConnects = 0;
         
         ResetDevice(ahAdapter[0]);
         ResetDevice(ahAdapter[1]);

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
       
         hr = NDTGetCounter(ahAdapter[0], NDT_COUNTER_STATUS_MEDIA_CONNECT, &ulConnects);
         if (FAILED(hr)) {
            NDTLogErr(g_szFailGetCounter, hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

         if (0 == ulConnects)
         {
              NDTLogErr(_T("Test device failed to indicate a connect event\b"));
              NDTLogErr(_T("Driver must indicate a media connect event after joining an IBSS cell\n"));
              FlagError(ErrorSevere,&rc);
         }     
         else
            if(1 != ulConnects)
            {
                    NDTLogErr(_T("Test device indicated an invalid number of connect events (Received %d)\n"), ulConnects);
                    NDTLogErr(_T("Driver must indicate asingle connect event after joining an IBSS cell\n"));
                    FlagError(ErrorSevere,&rc);
             }   
            else
               NDTLogDbg(_T("PASS: 1 Test adapter media connect event generated"));

          Sleep(WLAN_ASSOCIATE_TIMEOUT);
          
         hr = NDTGetCounter(ahAdapter[1], NDT_COUNTER_STATUS_MEDIA_CONNECT, &ulConnects);
         if (FAILED(hr)) {
            NDTLogErr(g_szFailGetCounter, hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

         if (0 == ulConnects)
         {
              NDTLogErr(_T("Support device failed to indicate a connect event\b"));
              NDTLogErr(_T("Driver must indicate a media connect event after joining an IBSS cell\n"));
              FlagError(ErrorSevere,&rc);
         }     
         else
            if(1 != ulConnects)
            {
                    NDTLogErr(_T("Support device indicated an invalid number of connect events (Received %d connects Expected 1)\n"), ulConnects);
                    NDTLogErr(_T("Driver must indicate a single media connect event after joining an IBSS cell\n"));
                    FlagError(ErrorSevere,&rc);
             }   
            else
                 NDTLogDbg(_T("PASS: 1 Support adapter media connect event generated"));
   

         ulConnectStatus = 0;
         cbUsed = 0;
         cbRequired = 0;
         hr = NDTQueryInfo(ahAdapter[1], OID_GEN_MEDIA_CONNECT_STATUS, &ulConnectStatus,
                  sizeof(ulConnectStatus),&cbUsed,&cbRequired);
         if (NOT_SUCCEEDED(hr)) 
         {
            NDTLogErr(_T("Unable to query OID_GEN_MEDIA_CONNECT_STATUS with hr=0x%08x\n"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

         if (ulConnectStatus != NdisMediaStateConnected)
        {
            hr=NDT_STATUS_FAILURE;
            NDTLogErr(_T("Support device media connect status should have been connected \n"));
            NDTLogErr(_T("Driver must change to media connect status connected when a node had joined\n"));
            FlagError(ErrorSevere,&rc);
            break;
         } 
         else
            NDTLogDbg(_T("PASS: Media state connected"));
          
      }
      while(0);

      // Fifth Variation      
      NDTLogMsg(_T("Variation 5: Verify remaining cell indicates a disconnect after all nodes leave\n"));
      NDTLogMsg(_T("This test will create an IBSS cell on the test device then join the support device to it\n "));
      NDTLogMsg(_T("it will then associate the support device with an access point and verify the test device indicates a disconnect\n"));

      do
      {
         ULONG ulDisconnects = 0;
         ResetDevice(ahAdapter[0]);
         ResetDevice(ahAdapter[1]);
      
         ulKeyLength = 10;
         hr = NDTWlanCreateIBSS(ahAdapter[0],  Ndis802_11AuthModeOpen, Ndis802_11WEPEnabled, ulKeyLength,
               0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to Create IBSS Error:0x%x"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("Test adapter Successfully created IBSS"));

         ulKeyLength = 10;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanJoinIBSS(ahAdapter[1], Ndis802_11AuthModeOpen, Ndis802_11WEPEnabled , 
               ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], &ulTimeout);
        if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to Join IBSS Error:0x%x"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }          
         else
            NDTLogDbg(_T("Support adapter Succesfully joined IBSS"));

         // Now associate the test device with an AP so that it leaves the IBSS cell
         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         NDIS_802_11_SSID ssid;
         ssid.SsidLength = strlen((char *)WLAN_WEP_AP1);
         memcpy(ssid.Ssid, WLAN_WEP_AP1, ssid.SsidLength);
         
         hr = NDTWlanAssociate(ahAdapter[1], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssid, FALSE ,&ulTimeout);         

         GetSsidText(szSsidText, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("Succesfully associated with %s"),szSsidText);

         //Give a little more than a 10 second hystorisis before checking for the disconnect
         Sleep(12000);
         
         hr = NDTGetCounter(ahAdapter[0], NDT_COUNTER_STATUS_MEDIA_DISCONNECT, &ulDisconnects);
         if (FAILED(hr)) {
            NDTLogErr(g_szFailGetCounter, hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

         if (1 != ulDisconnects)
         {
              NDTLogErr(_T("Test device should have indicated a disconnect event after the support device disjoined (Received: %d)"), ulDisconnects);
              NDTLogErr(_T("Driver must indicate a disconnect event after all STA's have disconnected from it's IBSS cell\n"));
              FlagError(ErrorSevere,&rc);
         }     
         else
            NDTLogDbg(_T("PASS One disconnect event indicated"));

         ulConnectStatus = 0;
         cbUsed = 0;
         cbRequired = 0;
         hr = NDTQueryInfo(ahAdapter[0], OID_GEN_MEDIA_CONNECT_STATUS, &ulConnectStatus,
                  sizeof(ulConnectStatus),&cbUsed,&cbRequired);
         if (NOT_SUCCEEDED(hr)) 
         {
            NDTLogErr(_T("Unable to query OID_GEN_MEDIA_CONNECT_STATUS with hr=0x%08x\n"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

         if (ulConnectStatus != NdisMediaStateDisconnected)
        {
            NDTLogErr(_T("Test device media connect status should have been disconnected. Driver must change"));
            NDTLogErr(_T("it's media connect status to disconnected after all STA's have disconnected from it's IBSS cell\n"));
            FlagError(ErrorSevere,&rc);
            break;
         }      
            NDTLogDbg(_T("PASS Media state disconnected"));
      }
      while(0);


	// Sixth Variation      
	
      NDTLogMsg(_T("Variation 6: Verify association with IBSS in 2 seconds or less\n"));
      NDTLogMsg(_T("This test will verify the device can associate with an AP in 2 seconds or less "));

      do
      {          
         DWORD dwAssocTime;
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

         hStatus = NULL;
         hr = NDTStatus(ahAdapter[0], NDIS_STATUS_MEDIA_CONNECT, &hStatus);
         if (FAILED(hr))
         {
            NDTLogErr(_T("Starting wait for connect on %s failed Error:0x%x\n"),g_szTestAdapter, hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

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
         
         dwAssocTime = GetTickCount();
         
         // Expect MEDIA_CONNECT within 2 seconds
         hr = NDTStatusWait(ahAdapter[0], hStatus, 2000);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to receive NDIS_STATUS_MEDIA_CONNECT Error : 0x%x\n"), NDIS_FROM_HRESULT(hr));
            NDTLogErr(_T("Driver must be able to associate with an IBSS in 2 seconds or less\n"));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
         {
            dwAssocTime = GetTickCount() - dwAssocTime;
            NDTLogDbg(_T("PASS: Associated succesfully in time %d millisec\n"), dwAssocTime);
         }   
         
      }
      while(0);     
      
   }
   
cleanUp:
  
   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {   

	  NDTWlanCleanupTest(ahAdapter[ixAdapter]); 
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

