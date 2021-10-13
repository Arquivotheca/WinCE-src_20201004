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

//------------------------------------------------------------------------------

#define WIFI_ONLY_SUPPORT

TEST_FUNCTION(SimpleWEPXfer)
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
   UINT cbUsed = 0;
   UINT cbRequired = 0;

   UINT   cbAddr = 0;
   UINT   cbHeader = 0;   
   DWORD  dwReceiveDelay = 0;   

   ULONG ulPacketsToSend = 100;
   ULONG ulPacketsSent = 0;
   ULONG ulPacketsReceived = 0;
   ULONG ulMinDirectedPass = g_dwDirectedPasspercentage * ulPacketsToSend /100;
   ULONG ulMinBroadcastPass = WLAN_PERCENT_TO_PASS_BROADCAST* ulPacketsToSend /100;
      
   UINT   uiMinimumPacketSize = 64;   
   
   memset(ahAdapter, 0, sizeof(ahAdapter));

   NDTLogMsg(
      _T("Start 2c_wlan_SimpleWEPXfer test\n")
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
#ifdef   WIFI_ONLY_SUPPORT
   NDTLogMsg(_T("Initializing adapters for Wlan test"));
   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++) {
      hr = NDTWlanInitializeTest(ahAdapter[ixAdapter],ixAdapter);
      if (FAILED(hr)) 
      {
         NDTLogErr(_T("Failed to initialize %s Wlan adapter Error0x%x"),((ixAdapter==0)? _T("Test"):_T("Support")), hr);
         FlagError(ErrorSevere,&rc);
         goto cleanUp;
      }
   }
#else
  hr = NDTWlanInitializeTest(ahAdapter[0],0);
      if (FAILED(hr)) 
      {
         NDTLogErr(_T("Failed to initialize %s Wlan adapter Error0x%x"),((ixAdapter==0)? _T("Test"):_T("Support")), hr);
         FlagError(ErrorSevere,&rc);
         goto cleanUp;
      }
#endif
      
    hr = NDTWlanGetDeviceType(ahAdapter[0],&WlanDeviceType);
    if (NOT_SUCCEEDED(hr))
      goto cleanUp;
   
    //Test first AP irrespective of Network Type
   dwSsidCount = 1;
   ssidList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WEP_AP1);
   memcpy((ssidList[dwSsidCount -1].Ssid), WLAN_WEP_AP1,ssidList[dwSsidCount -1].SsidLength);

     
   // Execute tests by ssid
   for (ix= 0; ix<dwSsidCount; ix++)
   {
      TCHAR szSsidText[33];
      GetSsidText(szSsidText, ssidList[ix]);
    
      NDTLogMsg(_T("This test will send\recv using default WEP keys"));

      do
      {

#ifdef WIFI_ONLY_SUPPORT    
         // associating support adapter if the User does no configuration
         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[1], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);         
         if (NOT_SUCCEEDED(hr))
         {
            GetSsidText(szSsidText, ssidList[ix]);
            NDTLogErr(_T("Failed to associate support adapter with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
#endif
          // associating test adapter
         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);         
         if (NOT_SUCCEEDED(hr))
         {
            GetSsidText(szSsidText, ssidList[ix]);
            NDTLogErr(_T("Failed to associatetest adapter with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }

        //  Test -> Support
          NDTLogMsg(_T("Test -> Support Directed send"));
         hr = NDTWlanDirectedSend(ahAdapter[0], ahAdapter[1], cbAddr, ndisMedium,
            ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
         if (FAILED(hr))
         {
            NDTLogErr(_T("DirectedSend failed %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;        
         }         
         NDTLogMsg(_T("ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d\n"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);
            
         if (ulPacketsReceived < ulMinDirectedPass)
         {
            NDTLogErr(_T("Received less than the required amount of directed packets with WLAN Directed"));
            NDTLogErr(_T("send pass Percentage at %d (Received: %d, Expected: %d)"),g_dwDirectedPasspercentage,ulPacketsReceived, ulMinDirectedPass);
            FlagError(ErrorSevere,&rc);
         }
         else
            NDTLogMsg(_T("DirectedSend succeeded : Received %d packets out of %d sent"),ulPacketsReceived, ulPacketsSent);


         // Support -> Test
          NDTLogMsg(_T("Support -> Test Directed send"));

         ulPacketsSent = 0;
         ulPacketsReceived = 0;

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
            NDTLogErr(_T("Support -> Test -Received less than the required amount of directed packets with WLAN Directed \n"));
            NDTLogErr(_T("send pass Percentage at %d (Received: %d, Expected: %d)"),g_dwDirectedPasspercentage,ulPacketsReceived, ulMinDirectedPass);
            FlagError(ErrorSevere,&rc);
         }         
         else
            NDTLogMsg(_T("DirectedSend succeeded : Received %d packets out of %d sent"),ulPacketsReceived, ulPacketsSent);


      }
      while(0);     

      

      do
      {
       
            // Test -> Support
                   NDTLogMsg(_T("Test -> Support Broadcast send"));
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
            NDTLogErr(_T("Test -> Support -Received less than the required amount of broadcast packets with WLAN Broadcast"));
            NDTLogErr(_T("send pass Percentage at %d (Received: %d, Expected: %d)"),WLAN_PERCENT_TO_PASS_BROADCAST,ulPacketsReceived, ulMinBroadcastPass);
            FlagError(ErrorSevere,&rc);
         }
		 else
			 NDTLogMsg(_T("Broadcast Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);

          // Support -> Test
         NDTLogMsg(_T("Support -> Test Broadcast Send"));
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
            NDTLogErr(_T("Support -> Test -Received less than the required amount of broadcast packets with WLAN Broadcast"));
            NDTLogErr(_T("send pass Percentage at %d (Received: %d, Expected: %d)"),WLAN_PERCENT_TO_PASS_BROADCAST,ulPacketsReceived, ulMinBroadcastPass);
            FlagError(ErrorSevere,&rc);
         }
		 else
			 NDTLogMsg(_T("Broadcast Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);
      }
      while(0);

    
      
   }
   
cleanUp:

   for (ixAdapter = 0; ixAdapter < chAdapter; ixAdapter++)    
   {
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

