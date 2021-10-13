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

TEST_FUNCTION(TestWpaAdhoc)
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
   ULONG ulKeyLength;
   NDIS_802_11_SSID ssidList[3];
   DWORD dwSsidCount;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;

   BOOL bWPASupported;
   BOOL bWPAAdhocSupported;
   BOOL status;
   
   UINT   cbAddr = 0;
   UINT   cbHeader = 0;   
   DWORD  dwReceiveDelay = 0;   

   UINT   uiMinimumPacketSize = 64;   
   ULONG ulPacketsToSend = 50;
   ULONG ulPacketsSent = 0;
   ULONG ulPacketsReceived = 0;
   
   memset(ahAdapter, 0, sizeof(ahAdapter));

   NDTLogMsg(
      _T("Start 2c_wpa_adhoc test\n")
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

   hr = NDTWlanIsWPASupported(ahAdapter[0], &bWPASupported);
   if (FAILED(hr))
   {
      NDTLogErr(_T("Failed to get IsWPASupported Error:0x%x\n"),hr);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }
   if (FALSE == bWPASupported)
   {
      NDTLogErr(_T("%s does not support WPA, skipping test\n"), g_szTestAdapter);
      rc = TPR_SKIP;
      goto cleanUp;
   }

   hr = NDTWlanIsWPAAdhocSupported(ahAdapter[0], &bWPAAdhocSupported);
   if (NOT_SUCCEEDED(hr))
   {
      NDTLogErr(_T("Failed to call NDTWlanIsWPAAdhocSupported Error0x:%x"),NDIS_FROM_HRESULT(hr));
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }   
   if (FALSE == bWPAAdhocSupported)
   {
      NDTLogMsg(_T("Card does not support WPA Adhoc mode, skipping test"));
      rc = TPR_SKIP;
      goto cleanUp;
   }
   
   //Test against WPA AP 
   dwSsidCount = 1;
   ssidList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_IBSS);
   memcpy((ssidList[dwSsidCount -1].Ssid), WLAN_IBSS,ssidList[dwSsidCount -1].SsidLength);

   DWORD aAuthenticationModes[]={Ndis802_11AuthModeOpen, Ndis802_11AuthModeShared, Ndis802_11AuthModeWPA, 
         Ndis802_11AuthModeWPAPSK, Ndis802_11AuthModeWPANone};
        
   // Execute test by ssid
   for (ix= 0; ix<dwSsidCount; ix++)
   {
      TCHAR szSsidText[33];
         
      GetSsidText(szSsidText, ssidList[ix]);

      // First Variation
      NDTLogMsg(_T("Variation 1: Set OID_802_11_AUTHENTICATION_MODE with invalid value"));
      NDTLogMsg(_T("This test will set OID_802_11_AUTHENTICATION_MODE with an invalid value and verify the request fails"));

      do
      {
         DWORD dwInvalidAuthMode = 255;
         hr = NDTWlanSetAuthenticationMode(ahAdapter[0], dwInvalidAuthMode);
         if (!NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Should have returned NDIS_STATUS_INVALID_DATA"));
            NDTLogErr(_T("Returned %d instead"),NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
         }
         else
            NDTLogDbg(_T("PASS: Returned Ndis status invalid data as expected"));
      }
      while(0);
      
      // Second Variation      
      NDTLogMsg(_T("Variation 2: Verify directed send\recv in IBSS mode\n"));
      NDTLogMsg(_T("This test will create and join an IBSS and verify directed packets can be sent and received\n "));

      do
      {
         ulPacketsToSend = 50;
         ulPacketsSent = 0;
         ulPacketsReceived = 0;
         ULONG ulMinDirectedPass = g_dwDirectedPasspercentage * ulPacketsToSend /100;
   
         ulKeyLength = 64;
         hr = NDTWlanCreateIBSS(ahAdapter[1],  Ndis802_11AuthModeWPANone, Ndis802_11Encryption2Enabled, ulKeyLength,
               0x80000003, (PBYTE)WLAN_KEY_TKIP, (NDIS_802_11_MAC_ADDRESS*)WLAN_KEYTYPE_DEFAULT, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to Create IBSS Error:0x%x"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("Support adapter Successfully created IBSS"));

         ulKeyLength = 64;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanJoinIBSS(ahAdapter[0], Ndis802_11AuthModeWPANone, Ndis802_11Encryption2Enabled, ulKeyLength, 
               0x80000003, (PBYTE)WLAN_KEY_TKIP, (NDIS_802_11_MAC_ADDRESS*)WLAN_KEYTYPE_DEFAULT, ssidList[ix], &ulTimeout);
        if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to Join IBSS Error:0x%x"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }          
        else
            NDTLogDbg(_T("Test adapter Succesfully joined IBSS"));

         // Test to support
         NDTLogDbg(_T("Test -> Support Directed send"));
         hr = NDTWlanDirectedSend(ahAdapter[0], ahAdapter[1], cbAddr, ndisMedium,
                          ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("DirectedSend failed Test to support %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;        
         }         
         
         if (ulPacketsReceived < ulMinDirectedPass)
         {
            NDTLogErr(_T("Test -> Support -Received less than the required amount of Directed packets with WLAN Directed"));
            NDTLogErr(_T("send pass Percentage at %d (Received: %d, Expected: %d)"),g_dwDirectedPasspercentage,ulPacketsReceived, ulMinDirectedPass);
            FlagError(ErrorSevere,&rc);
         }
         else
              NDTLogDbg(_T("PASS: Directed send %d packets sent %d packets received"),ulPacketsSent,ulPacketsReceived);            
       

         // Support to test
         NDTLogDbg(_T("Support -> Test Directed send"));
         hr = NDTWlanDirectedSend(ahAdapter[1], ahAdapter[0], cbAddr, ndisMedium,
                          ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("DirectedSend failed Test to support %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;        
         }                 
         
         if (ulPacketsReceived < ulMinDirectedPass)
         {
            NDTLogErr(_T("Support -> test -Received less than the required amount of Directed packets with WLAN Directed"));
            NDTLogErr(_T("send pass Percentage at %d (Received: %d, Expected: %d)"),g_dwDirectedPasspercentage,ulPacketsReceived, ulMinDirectedPass);
            FlagError(ErrorSevere,&rc);
         } 
         else
            NDTLogDbg(_T("PASS: Directed send %d packets sent %d packets received"),ulPacketsSent,ulPacketsReceived);            

      }
      while(0);

      // Third Variation      
      NDTLogMsg(_T("Variation 3: Verify broadcast send\recv in IBSS mode\n"));
      NDTLogMsg(_T("This test will create and join an IBSS and verify broadcast packets can be sent and received\n "));

      do
      {
         ulPacketsToSend = 50;
         ulPacketsSent = 0;
         ulPacketsReceived = 0;
         ULONG ulMinBroadcastPass = WLAN_PERCENT_TO_PASS_BROADCAST* ulPacketsToSend /100;

      
         ulKeyLength = 64;
         hr = NDTWlanCreateIBSS(ahAdapter[1],  Ndis802_11AuthModeWPANone, Ndis802_11Encryption2Enabled, ulKeyLength,
               0x80000003, (PBYTE)WLAN_KEY_TKIP, (NDIS_802_11_MAC_ADDRESS*)WLAN_KEYTYPE_DEFAULT, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to Create IBSS Error:0x%x"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("Support adapter Successfully created IBSS"));

         ulKeyLength = 64;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanJoinIBSS(ahAdapter[0], Ndis802_11AuthModeWPANone, Ndis802_11Encryption2Enabled, ulKeyLength, 
               0x80000003, (PBYTE)WLAN_KEY_TKIP, (NDIS_802_11_MAC_ADDRESS*)WLAN_KEYTYPE_DEFAULT, ssidList[ix], &ulTimeout);
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
            NDTLogErr(_T("Test -> Support -Received less than the required amount of broadcast packets with WLAN Broadcast "));
            NDTLogErr(_T("send pass Percentage at %d (Received: %d, Expected: %d)"),WLAN_PERCENT_TO_PASS_BROADCAST,ulPacketsReceived, ulMinBroadcastPass);
            FlagError(ErrorSevere,&rc);
         }
         else
            NDTLogDbg(_T("PASS: Broadcast send %d packets sent %d packets received"),ulPacketsSent,ulPacketsReceived);            
       

         // Support to test
         NDTLogDbg(_T("Support -> Test BRoadcast send"));
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
            NDTLogErr(_T(" send pass Percentage at %d (Received: %d, Expected: %d)"),WLAN_PERCENT_TO_PASS_BROADCAST,ulPacketsReceived, ulMinBroadcastPass);
            FlagError(ErrorSevere,&rc);
         }      
         else
             NDTLogDbg(_T("PASS: Broadcast send %d packets sent %d packets received"),ulPacketsSent,ulPacketsReceived);            
       
      }
      while(0);
      
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

