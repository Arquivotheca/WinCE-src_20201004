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
#include "wpadata.h"
#include "supplicant.h"

extern CSupplicant* g_pSupplicant;

//------------------------------------------------------------------------------

TEST_FUNCTION(SimpleWPA2Xfer)
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
   ULONG ulTimeout;
   NDIS_802_11_SSID ssidTestList[3];
   NDIS_802_11_SSID ssidSuppList[3];
   DWORD dwSsidCount;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;
   DWORD dwEncryption;

   BOOL bWPA2Supported;
   BOOL status;

   ULONG ulSendPacketsCompleted=0;
   ULONG ulPacketsToSend = 100;
   ULONG ulPacketsSent = 0;
   ULONG ulPacketsReceived = 0;
   ULONG ulMinDirectedPass;
   ULONG ulMinBroadcastPass;
   ULONG ulSendTime = 0;
   ULONG ulSendBytesSent = 0;
   ULONG ulRecvTime=0;
   ULONG ulRecvBytesReceived =0;
   UINT   uiMinimumPacketSize = 64;
   
   
   UINT   cbAddr = 0;
   UINT   cbHeader = 0;   
   DWORD  dwReceiveDelay = 0;   

   
   memset(ahAdapter, 0, sizeof(ahAdapter));

   NDTLogMsg(
      _T("Start 2c_simple_WPA2xfer test\n")
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
      hr = NDTWlanInitializeTest(ahAdapter[ixAdapter],ixAdapter);
      if (FAILED(hr)) 
      {
         NDTLogErr(_T("Failed to initialize Wlan %s adapter Error0x%x"), (ixAdapter ? _T("Support") : _T("Test")) ,hr);
         FlagError(ErrorSevere,&rc);
         goto cleanUp;
      }
   }

   g_pSupplicant = new CSupplicant();
   if (!g_pSupplicant)
   {
      NDTLogErr(_T("Failed to create supplicant"));
      FlagError(ErrorSevere,&rc);      
      goto cleanUp;
   }
      
   status = g_pSupplicant->OpenSupplicant(g_szTestAdapter);
   if (FALSE == status)
   {
      NDTLogErr(_T("Failed to open supplicant"));
      FlagError(ErrorSevere,&rc);            
      goto cleanUp;
   }

   hr = NDTWlanGetDeviceType(ahAdapter[0],&WlanDeviceType);
    if (NOT_SUCCEEDED(hr))
    {      
      FlagError(ErrorSevere,&rc);      
      goto cleanUp;
    }

   hr = NDTWlanIsWPA2Supported(ahAdapter[0], &bWPA2Supported);
   if (FAILED(hr))
   {
      NDTLogErr(_T("Failed to get IsWPA2Supported Error:0x%x\n"),hr);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }
   if (FALSE == bWPA2Supported)
   {
      NDTLogErr(_T("%s does not support WPA2, skipping test\n"), g_szTestAdapter);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }

    hr = NDTWlanIsWPA2Supported(ahAdapter[1], &bWPA2Supported);
   if (FAILED(hr))
   {
      NDTLogErr(_T("Failed to get IsWPA2Supported Error:0x%x\n"),hr);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }
   if (FALSE == bWPA2Supported)
   {
      NDTLogErr(_T("%s does not support WPA2, skipping test\n"), g_szHelpAdapter);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }
   
   
      dwSsidCount = 1;
      ssidTestList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WPA2_AP1);
      memcpy((ssidTestList[dwSsidCount -1].Ssid), WLAN_WPA2_AP1,ssidTestList[dwSsidCount -1].SsidLength);

      ssidSuppList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WPA2_AP1);
      memcpy((ssidSuppList[dwSsidCount -1].Ssid), WLAN_WPA2_AP1,ssidSuppList[dwSsidCount -1].SsidLength);      
       
   ulMinDirectedPass = g_dwDirectedPasspercentage* ulPacketsToSend /100;
   ulMinBroadcastPass = WLAN_PERCENT_TO_PASS_BROADCAST* ulPacketsToSend /100;
      
   // Execute test by ssid
   for (ix= 0; ix<dwSsidCount; ix++)
   {
      TCHAR szSsidText[33];
         
       // First Variation
      NDTLogMsg(_T("Variation 1: Associate test device with WPA2 AP"));    

      dwEncryption = Ndis802_11Encryption3Enabled;
      ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
      hr = NDTWPAAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeWPA2PSK, dwEncryption, 
               NULL, ssidTestList[ix], FALSE ,&ulTimeout);         
      if (NOT_SUCCEEDED(hr))
      {
         GetSsidText(szSsidText, ssidTestList[ix]);
         NDTLogErr(_T("Failed to associate test adapter with %s Error:0x%x\n"),szSsidText,hr);
         FlagError(ErrorSevere,&rc);            
         break;
      }         

/*
      NDTLogMsg(_T("Variation 1: Associate Support device with WPA2 AP"));    

      dwEncryption = Ndis802_11Encryption3Enabled;
      ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
      hr = NDTWPAAssociate(ahAdapter[1], Ndis802_11Infrastructure, Ndis802_11AuthModeWPA2PSK, dwEncryption, 
               NULL, ssidTestList[ix], FALSE ,&ulTimeout);         
      if (NOT_SUCCEEDED(hr))
      {
         GetSsidText(szSsidText, ssidTestList[ix]);
         NDTLogErr(_T("Failed to associate support adapter with %s Error:0x%x\n"),szSsidText,hr);
         FlagError(ErrorSevere,&rc);            
         break;
      }         
*/
      

      // Second Variation
      NDTLogMsg(_T("Variation 2: Verifying directed send\\receive"));
      NDTLogMsg(_T("This test will verify directed packets can be sent"));

       ulMinDirectedPass = g_dwDirectedPasspercentage * ulPacketsToSend /100;
   
      NDTLogMsg(_T("Support ->  Test"));
      
      hr = NDTWlanDirectedSend(ahAdapter[1], ahAdapter[0], cbAddr, ndisMedium,
         ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("DirectedSend failed %s Error:0x%x"),szSsidText,hr);
         FlagError(ErrorSevere,&rc);            
         break;        
      }         

      if (ulPacketsReceived < ulMinDirectedPass)
      {
         NDTLogErr(_T("Support -> Test -Received less than the required amount "));
         NDTLogErr(_T("of directed packets with WLAN Directed send pass Percentage at %d"),g_dwDirectedPasspercentage);
         NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinDirectedPass);
         FlagError(ErrorSevere,&rc);
      }
      else
	 NDTLogMsg(_T("Support -> Test Directed Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);
   

      NDTLogMsg(_T("Test-> Support"));
      
      hr = NDTWlanDirectedSend(ahAdapter[0], ahAdapter[1], cbAddr, ndisMedium,
         ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("DirectedSend failed %s Error:0x%x"),szSsidText,hr);
         FlagError(ErrorSevere,&rc);            
         break;        
      }         

      if (ulPacketsReceived < ulMinDirectedPass)
      {
         NDTLogErr(_T("Test -> Support -Received less than the required amount "));
         NDTLogErr(_T("of directed packets with WLAN Directed send pass Percentage at %d"),g_dwDirectedPasspercentage);
         NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinDirectedPass);
         FlagError(ErrorSevere,&rc);
      }
      else
	 NDTLogMsg(_T("Test -> Support Directed Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);
   


      // Third Variation
      NDTLogMsg(_T("Variation 2: Verifying broadcast send\\receive"));
      NDTLogMsg(_T("This test will verify broadcastpackets can be sent"));

      ulMinBroadcastPass = WLAN_PERCENT_TO_PASS_BROADCAST* ulPacketsToSend /100;

      NDTLogMsg(_T(" Support -> Test"));      
      ulPacketsSent = 0;
      ulPacketsReceived = 0;
      hr = NDTWlanBroadcastSend(ahAdapter[1], ahAdapter[0], cbAddr, ndisMedium,
                       ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("BroadcastSend failed %s Error:0x%x"),szSsidText,hr);
         FlagError(ErrorSevere,&rc);            
         break;        
      }         

      if (ulPacketsReceived < ulMinBroadcastPass)
      {
         NDTLogErr(_T("Support -> Test -Received less than the required amount of broadcast packets with"));
         NDTLogErr(_T("WLAN Broadcast send pass Percentage at %d%"),WLAN_PERCENT_TO_PASS_BROADCAST);
         NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinBroadcastPass);
         FlagError(ErrorSevere,&rc);
      }
      else
      NDTLogMsg(_T("Support -> Test Broadcast Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);


      NDTLogMsg(_T("Test -> Support"));   
      ulPacketsSent = 0;
      ulPacketsReceived = 0;
      hr = NDTWlanBroadcastSend(ahAdapter[0], ahAdapter[1], cbAddr, ndisMedium,
                       ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("BroadcastSend failed %s Error:0x%x"),szSsidText,hr);
         FlagError(ErrorSevere,&rc);            
         break;        
      }         

      if (ulPacketsReceived < ulMinBroadcastPass)
      {
         NDTLogErr(_T("Test -> Support -Received less than the required amount of broadcast packets with"));
         NDTLogErr(_T("WLAN Broadcast send pass Percentage at %d%"),WLAN_PERCENT_TO_PASS_BROADCAST);
         NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinBroadcastPass);
         FlagError(ErrorSevere,&rc);
      }
      else
      NDTLogMsg(_T("Test -> Support Broadcast Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);



   }   

cleanUp:

   if (g_pSupplicant)
   {
      status = g_pSupplicant->CloseSupplicant();
      delete g_pSupplicant;
      g_pSupplicant = NULL;
   }


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

