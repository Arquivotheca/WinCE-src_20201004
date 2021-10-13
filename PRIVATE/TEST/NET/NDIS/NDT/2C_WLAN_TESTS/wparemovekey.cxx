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

TEST_FUNCTION(TestWpaRemovekey)
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
   NDIS_802_11_SSID ssidTestList[3];
   NDIS_802_11_SSID ssidSuppList[3];
   DWORD dwSsidCount;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;
   DWORD dwEncryption;

   BOOL bWPASupported;
   BOOL status;

   ULONG ulSendPacketsCompleted=0;
   ULONG ulPacketsToSend = 50;
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

   UCHAR* pucSuppMac;

   
   memset(ahAdapter, 0, sizeof(ahAdapter));

   NDTLogMsg(
      _T("Start 2c_wpa_removekey test\n")
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


   
   dwSsidCount = 0;
   // Test APs depending on Network Type
   if (WlanDeviceType == WLAN_DEVICE_TYPE_802_11B || WlanDeviceType == WLAN_DEVICE_TYPE_802_11A)
   {
      dwSsidCount++;
      ssidTestList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WPA_AP1);
      memcpy((ssidTestList[dwSsidCount -1].Ssid), WLAN_WPA_AP1,ssidTestList[dwSsidCount -1].SsidLength);

      ssidSuppList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WEP_AP1);
      memcpy((ssidSuppList[dwSsidCount -1].Ssid), WLAN_WEP_AP1,ssidSuppList[dwSsidCount -1].SsidLength);      
   }
   else
      if (WlanDeviceType == WLAN_DEVICE_TYPE_802_11G || WlanDeviceType == WLAN_DEVICE_TYPE_802_11AB)
   {
      dwSsidCount++;
      ssidTestList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WPA_AP1);
      memcpy((ssidTestList[dwSsidCount -1].Ssid), WLAN_WPA_AP1,ssidTestList[dwSsidCount -1].SsidLength);

      ssidSuppList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WEP_AP2);
      memcpy((ssidSuppList[dwSsidCount -1].Ssid), WLAN_WEP_AP2,ssidSuppList[dwSsidCount -1].SsidLength);      
   }
   else
      if (WlanDeviceType == WLAN_DEVICE_TYPE_802_11AG)
   {
      dwSsidCount++;
      ssidTestList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WPA_AP1);
      memcpy((ssidTestList[dwSsidCount -1].Ssid), WLAN_WPA_AP1,ssidTestList[dwSsidCount -1].SsidLength);

      ssidSuppList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WEP_AP3);
      memcpy((ssidSuppList[dwSsidCount -1].Ssid), WLAN_WEP_AP3,ssidSuppList[dwSsidCount -1].SsidLength);
   } 
       

   hr = NDTGetPermanentAddr(ahAdapter[1], ndisMedium, &pucSuppMac);
   if (NOT_SUCCEEDED(hr)) 
   {
      NDTLogErr(_T("Failed to get permanent address Error:0x%x\n"),NDIS_FROM_HRESULT(hr));         
      if (NULL != pucSuppMac)
         delete pucSuppMac;
      FlagError(ErrorSevere,&rc);
   }

   ulMinDirectedPass = g_dwDirectedPasspercentage* ulPacketsToSend /100;
   ulMinBroadcastPass = WLAN_PERCENT_TO_PASS_BROADCAST* ulPacketsToSend /100;
      
   UCHAR DummyDestAddr[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
   
   // Execute test by ssid
   for (ix= 0; ix<dwSsidCount; ix++)
   {
      TCHAR szSsidText[33];         
      DWORD dwSize = FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial[strlen((char *)WLAN_KEY_TKIP)]);
      PNDIS_802_11_KEY pnAddKey = (PNDIS_802_11_KEY)malloc(dwSize);
      NDIS_802_11_REMOVE_KEY nRemoveKey;
      
      
      // First Variation
      NDTLogMsg(_T("Variation 1: Associate support device with WEP AP"));
   
      ulKeyLength = 10;
      dwEncryption = Ndis802_11WEPEnabled;
      ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
      hr = NDTWlanAssociate(ahAdapter[1], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
              ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidSuppList[ix], FALSE ,&ulTimeout);         
     GetSsidText(szSsidText, ssidSuppList[ix]);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
         FlagError(ErrorSevere,&rc);            
         break;
      }
      else
         NDTLogDbg(_T("Succesfully associated suppport with %s"),szSsidText);


      // Second Variation 
      NDTLogMsg(_T("Variation 2: Remove group key that has the transmit bit set"));
      NDTLogMsg(_T("This test will remove a group key that has the transmit bit set and verify the request fails"));

      do
      {         
         pnAddKey->Length = dwSize;
         pnAddKey->KeyLength = 64;      
         memcpy(pnAddKey->KeyMaterial, WLAN_KEY_TKIP, 64);         
         memcpy(pnAddKey->BSSID, DummyDestAddr, 6);      
         pnAddKey->KeyIndex = 0x80000001;
         dwSize = FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial[pnAddKey->Length]);


         hr = NDTWlanSetAddKey(ahAdapter[0], pnAddKey, dwSize);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_ADD_KEY Error:0x%x"), NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);            
         } 

         nRemoveKey.KeyIndex = 0x80000001;
         memcpy(nRemoveKey.BSSID, DummyDestAddr, 6);      
         nRemoveKey.Length = sizeof(NDIS_802_11_REMOVE_KEY);

         hr = NDTWlanSetRemoveKey(ahAdapter[0], &nRemoveKey);
         if (!NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Should have returned NDIS_STATUS_INVALID_DATA, Returned success"));
            NDTLogErr(_T("Driver must return NDIS_STATUS_INVALID_DATA when a group key is being removed"));
            NDTLogErr(_T("that has the transmit bit set"));
            FlagError(ErrorSevere,&rc);
         }
         else
            NDTLogDbg(_T("PASS: Set remove key returned ndis status invalid data as expected"));
       
      }
      while(0);


      // Third Variation
      NDTLogMsg(_T("Variation 3: Remove pairwise key that has the transmit bit set"));
      NDTLogMsg(_T("This test will remove a pairwise key that has the transmit bit set and verify the request fails"));

      do
      {   
         pnAddKey->Length = dwSize;
         pnAddKey->KeyLength = 64;      
         memcpy(pnAddKey->KeyMaterial, WLAN_KEY_TKIP, 64);         
         memcpy(pnAddKey->BSSID, pucSuppMac, 6);      
         pnAddKey->KeyIndex = 0xC0000000;
         dwSize = FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial[pnAddKey->Length]);

         hr = NDTWlanSetAddKey(ahAdapter[0], pnAddKey, dwSize);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_ADD_KEY Error:0x%x"), NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);            
         } 

         nRemoveKey.KeyIndex = 0xC0000000;
         memcpy(nRemoveKey.BSSID, pucSuppMac, 6);      
         nRemoveKey.Length = sizeof(NDIS_802_11_REMOVE_KEY);

         hr = NDTWlanSetRemoveKey(ahAdapter[0], &nRemoveKey);
         if (!NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Should have returned NDIS_STATUS_INVALID_DATA, Returned success"));
            NDTLogErr(_T("Driver must return NDIS_STATUS_INVALID_DATA when a pairwise key is being removed"));
            NDTLogErr(_T("that has the transmit bit set"));
            FlagError(ErrorSevere,&rc);
         }
         else
            NDTLogDbg(_T("PASS: Set remove key returned ndis status invalid data as expected"));
       
      }
      while(0);

      
      // Fourth Variation
      NDTLogMsg(_T("Variation 4: Remove key with an invalid KeyIndex"));
      NDTLogMsg(_T("This test will remove a key with an invalid key index and verify the request fails"));

      do
      {
         nRemoveKey.KeyIndex = 0xFFFFFFFF;
         memcpy(nRemoveKey.BSSID, DummyDestAddr, 6);      
         nRemoveKey.Length = sizeof(NDIS_802_11_REMOVE_KEY);

         hr = NDTWlanSetRemoveKey(ahAdapter[0], &nRemoveKey);
         if (!NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Should have returned NDIS_STATUS_INVALID_DATA, Returned success"));
            NDTLogErr(_T("Driver must return NDIS_STATUS_INVALID_DATA when OID_802_11_REMOVE_KEY "));
            NDTLogErr(_T("is set with an invalid key index"));
            FlagError(ErrorSevere,&rc);
         }
         else
            NDTLogDbg(_T("PASS: Set remove key returned ndis status invalid data as expected"));

      }
      while(0);


      // Fifth Variation
      NDTLogMsg(_T("Verify group keys can be removed"));
      NDTLogMsg(_T("This test will associate with the WPA AP and then remove the group key and"));
      NDTLogMsg(_T("verify broadcast packets are not received"));      
      do
      {
         NDTWlanReset(ahAdapter[0], TRUE);

         dwEncryption = Ndis802_11Encryption2Enabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWPAAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeWPAPSK, dwEncryption, 
                  NULL, ssidTestList[ix], FALSE ,&ulTimeout);         

         GetSsidText(szSsidText, ssidTestList[ix]);         
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate test adapter with %s Error:0x%x\n"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }            
         else
            NDTLogDbg(_T("Succesfully associated test with %s"),szSsidText);

         NDTLogDbg(_T("Support -> Test Directed send"));
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
            NDTLogErr(_T("Support -> Test -Received less than the required amount "));
            NDTLogErr(_T("of directed packets with WLAN Directed send pass Percentage at %d"),g_dwDirectedPasspercentage);
            NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinDirectedPass);
            FlagError(ErrorSevere,&rc);
         }
         else
		 NDTLogDbg(_T("Directed Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);

        NDTLogDbg(_T("Support -> Test BRoadcast send"));
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
            NDTLogErr(_T("Support -> Test -Received less than the required amount "));
            NDTLogErr(_T("of broadcast packets with WLAN Broadcast send pass Percentage at %d"),WLAN_PERCENT_TO_PASS_BROADCAST);
            NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinBroadcastPass);
            FlagError(ErrorSevere,&rc);        
         }
		 else
			 NDTLogDbg(_T("Broadcast Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);

         LONG lTestGroupIndex;
         g_pSupplicant->get_GroupKeyIndex(&lTestGroupIndex);
         NDTLogDbg(_T("Group Index is 0x%x"),lTestGroupIndex);
         
         nRemoveKey.KeyIndex = 0x0000003 & lTestGroupIndex;
         memcpy(nRemoveKey.BSSID, DummyDestAddr, 6);      
         nRemoveKey.Length = sizeof(NDIS_802_11_REMOVE_KEY);

         NDTLogDbg(_T("Removing key with index 0x%x"),nRemoveKey.KeyIndex );
         hr = NDTWlanSetRemoveKey(ahAdapter[0], &nRemoveKey);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_REMOVE_KEY Error:0x%x"),NDIS_FROM_HRESULT(hr));
         }   
         else
            NDTLogDbg(_T("Succesfully removed key"));

        NDTLogDbg(_T("Support -> Test Directed send"));
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
            NDTLogErr(_T("Support -> Test -Received less than the required amount "));
            NDTLogErr(_T("of directed packets with WLAN Directed send pass Percentage at %d"),g_dwDirectedPasspercentage);
            NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinDirectedPass);
            FlagError(ErrorSevere,&rc);
         }
         else
		 NDTLogDbg(_T("Directed Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);

        NDTLogDbg(_T("Support -> Test BRoadcast send"));
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

         if (ulPacketsReceived != 0)
         {
           NDTLogErr(_T("Should not have received any broadcast packets Received: %d"),ulPacketsReceived);
            FlagError(ErrorSevere,&rc);
         }              
         else
            NDTLogDbg(_T("PASS: No broadcast packets can be received as expected as group key was removed"));
         
      }
      while(0);


      // Sixth Variation
      NDTLogMsg(_T("Variation 6: Verify pairwise keys can be removed"));
      NDTLogMsg(_T("This test will associate with the WPA AP and then remove the pairwise key and"));
      NDTLogMsg(_T("verify directed packets are not received"));      
      do
      {
         NDTWlanReset(ahAdapter[0], TRUE);

         dwEncryption = Ndis802_11Encryption2Enabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWPAAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeWPAPSK, dwEncryption, 
                  NULL, ssidTestList[ix], FALSE ,&ulTimeout);         
         GetSsidText(szSsidText, ssidTestList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate test adapter with %s Error:0x%x\n"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }        
         else
            NDTLogDbg(_T("Succesfully associated test with %s"),szSsidText);

        NDTLogDbg(_T("Support -> Test Directed send"));
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
            NDTLogErr(_T("Support -> Test -Received less than the required amount "));
            NDTLogErr(_T("of directed packets with WLAN Directed send pass Percentage at %d"),g_dwDirectedPasspercentage);
            NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinDirectedPass);
            FlagError(ErrorSevere,&rc);
         }
         else
		 NDTLogDbg(_T("Directed Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);

        NDTLogDbg(_T("Support -> Test BRoadcast send"));
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
            NDTLogErr(_T("Support -> Test -Received less than the required amount "));
            NDTLogErr(_T("of broadcast packets with WLAN Broadcast send pass Percentage at %d"),WLAN_PERCENT_TO_PASS_BROADCAST);
            NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinBroadcastPass);
            FlagError(ErrorSevere,&rc);
         
         }
		 else
			 NDTLogDbg(_T("Broadcast Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);


         NDIS_802_11_MAC_ADDRESS  abTestMac;
         hr = NDTWlanGetBssid(ahAdapter[0],&abTestMac);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to get Bssid of test adapter Error:0x%x"), NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }
         
         nRemoveKey.KeyIndex = 0x40000000;
         memcpy(nRemoveKey.BSSID, abTestMac, 6);      
         nRemoveKey.Length = sizeof(NDIS_802_11_REMOVE_KEY);

         NDTLogDbg(_T("Removing key with index 0x%x"),nRemoveKey.KeyIndex );
         hr = NDTWlanSetRemoveKey(ahAdapter[0], &nRemoveKey);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_REMOVE_KEY Error:0x%x"),NDIS_FROM_HRESULT(hr));
         }   
         else
            NDTLogDbg(_T("Succesfully removed key"));
   
        NDTLogDbg(_T("Support -> Test Directed send"));
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

         if (ulPacketsReceived != 0)
         {
            NDTLogErr(_T("Should not have received any directed packets Received: %d"),ulPacketsReceived);
            FlagError(ErrorSevere,&rc);
         }                          
         else
            NDTLogDbg(_T("PASS: No directed packets could be received as expected"));

        NDTLogDbg(_T("Support -> Test BRoadcast send"));
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
            NDTLogErr(_T("Support -> Test -Received less than the required amount "));
            NDTLogErr(_T("of broadcast packets with WLAN Broadcast send pass Percentage at %d"),WLAN_PERCENT_TO_PASS_BROADCAST);
            NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinBroadcastPass);
            FlagError(ErrorSevere,&rc);
         
         }
 	    else
		 NDTLogDbg(_T("Broadcast Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);
         
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

