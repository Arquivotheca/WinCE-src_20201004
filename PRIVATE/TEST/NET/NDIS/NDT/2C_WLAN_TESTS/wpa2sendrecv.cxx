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

HRESULT SendRecvPackets(HANDLE hTestAdapter, HANDLE hSupportAdapter)
{   
   ULONG ulSendPacketsCompleted=0;
   ULONG ulPacketsToSend = 500;
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

   NDIS_MEDIUM ndisMedium = g_ndisMedium;

   HRESULT hr = NDT_STATUS_SUCCESS;
   TCHAR   szSsidText[33];
   
   // Get some information about the media
   NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);
   dwReceiveDelay = NDTGetReceiveDelay(ndisMedium);

   //Test-> Support Directed
   ulMinDirectedPass = g_dwDirectedPasspercentage * ulPacketsToSend /100;
   NDTLogDbg(_T("Test -> Support Directed send"));
   hr = NDTWlanDirectedSend(hTestAdapter, hSupportAdapter, cbAddr, ndisMedium,
      ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
   if (NOT_SUCCEEDED(hr))
   {
      NDTLogErr(_T("DirectedSend failed %s Error:0x%x"),szSsidText,hr);
      hr = NDT_STATUS_FAILURE;            
      return hr;        
   }         

   if (ulPacketsReceived < ulMinDirectedPass)
   {
      NDTLogErr(_T("Test -> Support -Received less than the required amount "));
      NDTLogErr(_T("of directed packets with WLAN Directed send pass Percentage at %d"),g_dwDirectedPasspercentage);
      NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinDirectedPass);
      hr = NDT_STATUS_FAILURE;
   }
   else
      NDTLogDbg(_T("Directed Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);


      // Test -> Support Broadcast
     ulMinBroadcastPass = g_dwDirectedPasspercentage * ulPacketsToSend /100;
     NDTLogDbg(_T("Test -> Support Broadcast send"));
      ulPacketsSent = 0;
      ulPacketsReceived = 0;
      hr = NDTWlanBroadcastSend(hTestAdapter, hSupportAdapter, cbAddr, ndisMedium,
                       ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("BroadcastSend failed %s Error:0x%x"),szSsidText,hr);
         hr = NDT_STATUS_FAILURE;            
         return hr;        
      }         

      if (ulPacketsReceived < ulMinBroadcastPass)
      {
         NDTLogErr(_T("Test -> Support -Received less than the required amount of broadcast packets with"));
         NDTLogErr(_T("WLAN Broadcast send pass Percentage at %d%"),WLAN_PERCENT_TO_PASS_BROADCAST);
         NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinBroadcastPass);
         hr = NDT_STATUS_FAILURE;
      }
      else
      NDTLogDbg(_T("Broadcast Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);


    // Support -> Test Directed
    ulMinDirectedPass = g_dwDirectedPasspercentage * ulPacketsToSend /100;
    NDTLogDbg(_T("Support -> Test Directed send"));
    ulPacketsSent = 0;
    ulPacketsReceived = 0; 
    hr = NDTWlanDirectedSend(hSupportAdapter, hTestAdapter, cbAddr, ndisMedium,
         ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("DirectedSend failed %s Error:0x%x"),szSsidText,hr);
         hr = NDT_STATUS_FAILURE;            
         return hr;        
      }         

      if (ulPacketsReceived < ulMinDirectedPass)
      {
         NDTLogErr(_T("Support -> Test -Received less than the required amount "));
         NDTLogErr(_T("of directed packets with WLAN Directed send pass Percentage at %d"),g_dwDirectedPasspercentage);
         NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinDirectedPass);
         hr = NDT_STATUS_FAILURE;
      }
      else
         NDTLogDbg(_T("Directed Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);


         // Support  -> Test Broadcast
        ulMinBroadcastPass = g_dwDirectedPasspercentage * ulPacketsToSend /100;
        NDTLogDbg(_T("Support -> Test BRoadcast send"));
         ulPacketsSent = 0;
         ulPacketsReceived = 0;
         hr = NDTWlanBroadcastSend(hSupportAdapter, hTestAdapter, cbAddr, ndisMedium,
                          ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("BroadcastSend failed %s Error:0x%x"),szSsidText,hr);
            hr = NDT_STATUS_FAILURE;            
            return hr;        
         }         

         if (ulPacketsReceived < ulMinBroadcastPass)
         {
            NDTLogErr(_T("Support -> Test -Received less than the required amount of broadcast packets with"));
            NDTLogErr(_T("WLAN Broadcast send pass Percentage at %d%"),WLAN_PERCENT_TO_PASS_BROADCAST);
            NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinBroadcastPass);
            hr = NDT_STATUS_FAILURE;
         }
         else
         NDTLogDbg(_T("Broadcast Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);

        return hr;
}

//------------------------------------------------------------------------------

TEST_FUNCTION(TestWpa2Sendrecv)
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
   ULONG ulKeyLength = 0;
   ULONG ulTimeout;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;
   DWORD dwEncryption;

   BOOL bWPA2Supported;
   BOOL status;
   
   
   memset(ahAdapter, 0, sizeof(ahAdapter));

   NDTLogMsg(
      _T("Start 2c_wpa2_sendrecv test\n")
   );

   NDTLogMsg(_T("The test adapter is %s"), g_szTestAdapter);
   NDTLogMsg(_T("The support adapter is %s"), g_szHelpAdapter);

   
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
      NDTLogErr(_T("Failed to get IsWPASupported Error:0x%x\n"),hr);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }
   if (FALSE == bWPA2Supported)
   {
      NDTLogErr(_T("%s does not support WPA2, skipping test\n"), g_szTestAdapter);
      rc = TPR_SKIP;
      goto cleanUp;
   }
   

   TCHAR szSsidText[33];

   DWORD dwBufSize = sizeof(NDIS_802_11_CAPABILITY);
   PNDIS_802_11_CAPABILITY pnCapability = (PNDIS_802_11_CAPABILITY)malloc(dwBufSize);
   if (!pnCapability)
   {
      NDTLogErr(_T("Cannot allocate memory"));
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }   
   
   hr = NDTWlanGetCapability(ahAdapter[0], pnCapability, &dwBufSize);
   if (NOT_SUCCEEDED(hr) && NDT_STATUS_BUFFER_TOO_SHORT == hr)
   {
      PNDIS_802_11_CAPABILITY pnTemp = NULL;
      pnTemp = (PNDIS_802_11_CAPABILITY)realloc(pnCapability, dwBufSize);
      if (NULL == pnTemp)
      {
         NDTLogErr(_T("Cannot reallocate memory"));
         FlagError(ErrorSevere,&rc);
         goto cleanUp;
      }
      else
      {
         pnCapability = pnTemp;
      }
      hr = NDTWlanGetCapability(ahAdapter[0], pnCapability, &dwBufSize);      
   }         
   if (NOT_SUCCEEDED(hr))      
   {
      NDTLogErr(_T("Failed to query OID_802_11_CAPABILITY"));
      NDTLogErr(_T("WP2 drivers must support OID_802_11_CAPABILITY query request"));
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }    


   // First Variation
   NDTLogMsg(_T("Variation 1: Associate support device with WEP access point"));
   NDTLogMsg(_T("This test will associate the support device with the WEP access point so packets"));
   NDTLogMsg(_T("can be sent from the WPA\\WPA2 access points to the WEP access point"));

   do
   {
      NDIS_802_11_SSID WEPSsid;
      WEPSsid.SsidLength = strlen((char*)WLAN_WEP_AP1);
      memcpy(WEPSsid.Ssid, WLAN_WEP_AP1, WEPSsid.SsidLength);
      
      ulKeyLength = 10;
      dwEncryption = Ndis802_11WEPEnabled;
      ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
      hr = NDTWlanAssociate(ahAdapter[1], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
              ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, WEPSsid, FALSE ,&ulTimeout);         

      GetSsidText(szSsidText, WEPSsid);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Failed to associate with %s , Error :0x%x"),szSsidText,NDIS_FROM_HRESULT(hr));
         FlagError(ErrorSevere,&rc);            
         break;
      }
      else
          NDTLogDbg(_T("Succesfully associated support with %s"),szSsidText);         
      

     for(UINT iIndex = 0; iIndex < pnCapability->NoOfAuthEncryptPairsSupported; iIndex++)
     {   
         // WPA2\TKIP
         if (Ndis802_11AuthModeWPA2PSK == pnCapability->AuthenticationEncryptionSupported[iIndex].AuthModeSupported
            && Ndis802_11Encryption2Enabled == pnCapability->AuthenticationEncryptionSupported[iIndex].EncryptStatusSupported)
         {         
            // Second Variation
             NDTLogMsg(_T("Variation 2: Verify send\\receive with TKIP encrypted traffic"));
             NDTLogMsg(_T("This test will associate the test device with the WPA2-PSK access point"));
             NDTLogMsg(_T("and send and receive directed and broadcast packets using TKIP encryption"));

             NDTWlanReset(ahAdapter[0],TRUE);

             do
             {
                  NDIS_802_11_SSID WPA2Ssid;
                  WPA2Ssid.SsidLength = strlen((char*)WLAN_WPA2_AP1);
                  memcpy(WPA2Ssid.Ssid, WLAN_WPA2_AP1, WPA2Ssid.SsidLength);
                  
                  dwEncryption = Ndis802_11Encryption2Enabled;
                  ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
                  hr = NDTWPAAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeWPA2PSK, dwEncryption, 
                           NULL, WPA2Ssid, FALSE ,&ulTimeout);         

                  GetSsidText(szSsidText,WPA2Ssid);
                  if (NOT_SUCCEEDED(hr))
                  {
                     NDTLogErr(_T("Failed to associate test adapter with %s Error:0x%x\n"),szSsidText,hr);
                     FlagError(ErrorSevere,&rc);            
                     break;
                  }               
                  else
                     NDTLogDbg(_T("Succesfully associated test with %s"),szSsidText); 

                  hr = SendRecvPackets(ahAdapter[0],ahAdapter[1]);               
                  if (NOT_SUCCEEDED(hr))
                  {
                     NDTLogErr(_T("Failed send receive test using TKIP"));
                     FlagError(ErrorSevere,&rc);
                  }
                  else
                     NDTLogDbg(_T("PASS: Passed send-recv test for TKIP"));
                     
               }
               while(0);               
          }  

         // WPA2\AES
         if (Ndis802_11AuthModeWPA2PSK == pnCapability->AuthenticationEncryptionSupported[iIndex].AuthModeSupported
            && Ndis802_11Encryption3Enabled == pnCapability->AuthenticationEncryptionSupported[iIndex].EncryptStatusSupported)
         {         
            // Third Variation
             NDTLogMsg(_T("Variation 3: Verify send\\receive with AES encrypted traffic"));
             NDTLogMsg(_T("This test will associate the test device with the WPA2-PSK access point"));
             NDTLogMsg(_T("and send and receive directed and broadcast packets using AES encryption"));

             NDTWlanReset(ahAdapter[0],TRUE);

             do
             {
                  NDIS_802_11_SSID WPA2Ssid;
                  WPA2Ssid.SsidLength = strlen((char*)WLAN_WPA2_AP1);
                  memcpy(WPA2Ssid.Ssid, WLAN_WPA2_AP1, WPA2Ssid.SsidLength);
                  
                  dwEncryption = Ndis802_11Encryption3Enabled;
                  ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
                  hr = NDTWPAAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeWPA2PSK, dwEncryption, 
                           NULL, WPA2Ssid, FALSE ,&ulTimeout);         

                 GetSsidText(szSsidText, WPA2Ssid);                
                  if (NOT_SUCCEEDED(hr))
                  {
                     NDTLogErr(_T("Failed to associate with %s using AES,Error :0x%x"),szSsidText,NDIS_FROM_HRESULT(hr));
                     FlagError(ErrorSevere,&rc);            
                     break;
                  }         
                  else           
                     NDTLogDbg(_T("Succesfully associated test with %s"),szSsidText);



                  hr = SendRecvPackets(ahAdapter[0],ahAdapter[1]);               
                  if (NOT_SUCCEEDED(hr))
                  {
                     NDTLogErr(_T("Failed send receive test using AES"));
                     FlagError(ErrorSevere,&rc);
                  }
                  else
                     NDTLogDbg(_T("PASS: Passed send-recv test for AES"));
                  
               }
               while(0);               
          } 
     }
   }
   while(0);

 
   


cleanUp:

   if (pnCapability)
      free (pnCapability);
   
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

