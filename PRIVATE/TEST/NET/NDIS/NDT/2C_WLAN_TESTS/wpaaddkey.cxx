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

TEST_FUNCTION(TestWpaAddkey)
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
      _T("Start 2c_wpa_addkey test\n")
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
      goto cleanUp;
   }

   ulMinDirectedPass = g_dwDirectedPasspercentage* ulPacketsToSend /100;
   ulMinBroadcastPass = WLAN_PERCENT_TO_PASS_BROADCAST* ulPacketsToSend /100;

   // Execute test by ssid
   for (ix= 0; ix<dwSsidCount; ix++)
   {
      TCHAR szSsidText[33];         
      DWORD dwSize = FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial[strlen((char *)WLAN_KEY_TKIP)]);
      PNDIS_802_11_KEY pnKey = (PNDIS_802_11_KEY)malloc(dwSize);
      pnKey->Length = dwSize;
      pnKey->KeyLength = 64;      
      pnKey->KeyIndex = 0xC0000000;
      memcpy(pnKey->KeyMaterial, WLAN_KEY_TKIP, 64); 
      memcpy(pnKey->BSSID, pucSuppMac, 6);      

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
         NDTLogDbg(_T("PASS Associated support device with WEP AP %s"),szSsidText);

      // Second Variation 
      NDTLogMsg(_T("Variation 2: Set OID_802_11_ADD_KEY with a pairwise key that has bits other than 31 and 30 set"));
      NDTLogMsg(_T("This test will set OID_802_11_ADD_KEY with a pairwise with with bits other than 30 and 31 set"));
      NDTLogMsg(_T("and verify the request fails"));

      pnKey->Length = dwSize;
      pnKey->KeyLength = 64;      
      memcpy(pnKey->KeyMaterial, WLAN_KEY_TKIP, 64);         
      memcpy(pnKey->BSSID, pucSuppMac, 6);      
      pnKey->KeyIndex = 0xFFFFFFFF;
      dwSize = FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial[pnKey->Length]);
      
      hr = NDTWlanSetAddKey(ahAdapter[0], pnKey, dwSize);
      if (!NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Should have returned NDIS_STATUS_INVALID_DATA, returned success"));
         NDTLogErr(_T("Driver must return NDIS_STATUS_INVALID_DATA when a pairwise key is set"));
         NDTLogErr(_T("with key index bits other than 30 and 31 set"));
         FlagError(ErrorSevere,&rc);            
      }
      else
         NDTLogDbg(_T("PASS: AddKey Returned error as expected"));      

      // Third Variation 
      NDTLogMsg(_T("Variation 3: Set OID_802_11_ADD_KEY with a pairwise key that does not have the transmit bit set"));
      NDTLogMsg(_T("This test will set OID_802_11_ADD_KEY with a pairwise key that"));
      NDTLogMsg(_T("does not have the transmit bit set and verify the request fails"));

      pnKey->Length = dwSize;
      pnKey->KeyLength = 64;      
      memcpy(pnKey->KeyMaterial, WLAN_KEY_TKIP, 64);          
      memcpy(pnKey->BSSID, pucSuppMac, 6);      
      pnKey->KeyIndex = 0x40000000;
      dwSize = FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial[pnKey->Length]);
      
      hr = NDTWlanSetAddKey(ahAdapter[0], pnKey, dwSize);
      if (!NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Should have returned NDIS_STATUS_INVALID_DATA, returned success"));
         NDTLogErr(_T("Driver must return NDIS_STATUS_INVALID_DATA when a pairwise key is set"));
         NDTLogErr(_T("that does not have the tramit bit in the key index set"));
         FlagError(ErrorSevere,&rc);            
      }  
      else
         NDTLogDbg(_T("PASS: AddKey Returned error as expected"));    

      // Fourth Variation 
      NDTLogMsg(_T("Variation 4: Set OID_802_11_ADD_KEY with a pairwise key with BSSID equal to 0xffffffffffff"));
      NDTLogMsg(_T("This test will set OID_802_11_ADD_KEY with a pairwise key that"));
      NDTLogMsg(_T("has the BSSID set to FF:FF:FF:FF:FF:FF and verify the request fails"));

      UCHAR DummyDestAddr[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
     
      pnKey->Length = dwSize;
      pnKey->KeyLength = 64;      
      memcpy(pnKey->KeyMaterial, WLAN_KEY_TKIP, 64);          
      memcpy(pnKey->BSSID, DummyDestAddr, 6);      
      pnKey->KeyIndex = 0x40000000;
      dwSize = FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial[pnKey->Length]);
      
      hr = NDTWlanSetAddKey(ahAdapter[0], pnKey, dwSize);
      if (!NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Should have returned NDIS_STATUS_INVALID_DATA, returned success"));
         NDTLogErr(_T("Driver must return NDIS_STATUS_INVALID_DATA when a pairwise key is set"));
         NDTLogErr(_T("with a BSSID equal to FF:FF:FF:FF:FF:FF when key mapping keys are supported"));
         FlagError(ErrorSevere,&rc);            
      }      
      else
         NDTLogDbg(_T("PASS: AddKey Returned error as expected"));    


      // Fifth Variation 
      NDTLogMsg(_T("Variation 5: Set OID_802_11_ADD_KEY with a TKIP pairwise key that is less than required length"));
      NDTLogMsg(_T("This test will set OID_802_11_ADD_KEY with a TKIP pairwise key"));
      NDTLogMsg(_T("that is less than the required length and verify the request fails"));


      // As we are adding an invalid key length, Add key function will not convert it for us
      // So we have to convert it here, But the test itself doesnot care what value it is
      // So we assume its the final hex bits
      pnKey->Length = dwSize - 64 + 24; // Size will include 24 bits for the key itself
      pnKey->KeyLength = 24 ; // Keylength is 24      
      memcpy(pnKey->KeyMaterial, WLAN_KEY_TKIP, 24);          
      pnKey->KeyIndex = 0xC0000000;
      dwSize = FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial[pnKey->Length]);
      
      hr = NDTWlanSetAddKey(ahAdapter[0], pnKey, dwSize);
      if (!NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Should have returned NDIS_STATUS_INVALID_DATA, returned success"));
         NDTLogErr(_T("Driver must return NDIS_STATUS_INVALID_DATA when a TKIP key"));
         NDTLogErr(_T(" is added that is less than the required length"));
         FlagError(ErrorSevere,&rc);            
      }      
      else
         NDTLogDbg(_T("PASS: AddKey Returned error as expected"));    


      // Sixth Variation 
      NDTLogMsg(_T("Variation 6: Set OID_802_11_ADD_KEY with a TKIP pairwise key that is less than required length"));
      NDTLogMsg(_T("This test will set OID_802_11_ADD_KEY with a TKIP pairwise key"));
      NDTLogMsg(_T("that is larger than the required length and verify the request fails"));


      // As we are adding an invalid key length, Add key function will not convert it for us
      // So we have to convert it here, But the test itself doesnot care what value it is
      // So we assume its the final hex bits
      pnKey->Length = dwSize - 64 + 40; // Size will include 24 bits for the key itself
      pnKey->KeyLength = 40 ; // Keylength is 24      
      memcpy(pnKey->KeyMaterial, WLAN_KEY_TKIP, 40);          
      pnKey->KeyIndex = 0xC0000000;
      dwSize = FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial[pnKey->Length]);
      
      hr = NDTWlanSetAddKey(ahAdapter[0], pnKey, dwSize);
      if (!NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Should have returned NDIS_STATUS_INVALID_DATA, returned success"));
         NDTLogErr(_T("Driver must return NDIS_STATUS_INVALID_DATA when a TKIP key"));
         NDTLogErr(_T(" is added that is larger than the required length"));
         FlagError(ErrorSevere,&rc);            
      }       
      else
         NDTLogDbg(_T("PASS: AddKey Returned error as expected"));    


      // Seventh Variation 
      NDTLogMsg(_T("Variation 7: Verify last pairwise key added overwrites previous pairwise key"));
      NDTLogMsg(_T("This test will associate in WPA-PSK and send\recv, then it will overwite"));
      NDTLogMsg(_T("the paiwise key and verify the send\recv fails"));

      do
      {
         NDTWlanReset(ahAdapter[0],TRUE);

         dwEncryption = Ndis802_11Encryption2Enabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWPAAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeWPAPSK, dwEncryption, 
                  NULL, ssidTestList[ix], FALSE ,&ulTimeout);         
         if (NOT_SUCCEEDED(hr))
         {
            GetSsidText(szSsidText, ssidTestList[ix]);
            NDTLogErr(_T("Failed to associate test adapter with %s Error:0x%x\n"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }         

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


         NDIS_802_11_MAC_ADDRESS  abTestMac;
         hr = NDTWlanGetBssid(ahAdapter[0],&abTestMac);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to get Bssid of test adapter Error:0x%x"), NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }
       
         pnKey->Length = dwSize;
         pnKey->KeyLength = 64;      
         memcpy(pnKey->KeyMaterial, WLAN_KEY_TKIP, 64);         
         memcpy(pnKey->BSSID, abTestMac, 6);      
         pnKey->KeyIndex = 0xC0000000;
         dwSize = FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial[pnKey->Length]);
      
         hr = NDTWlanSetAddKey(ahAdapter[0], pnKey, dwSize);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_ADD_KEY, Error:0x%x"), NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("Successfully overwrote pairwise key with a different value"));
        
         
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
            NDTLogDbg(_T("PASS: Directed Send failed to send packets through as expected"));
         
      }
      while(0);


      // Eight Variation
      NDTLogMsg(_T("Variation 8: Verify last group key added overwrites previous group key"));
      NDTLogMsg(_T("This test will associate in WPA-PSK and send\recv, then it will overwite"));
      NDTLogMsg(_T("the group key and verify the send\recv fails"));

      do
      {
         NDTWlanReset(ahAdapter[0],TRUE);

         dwEncryption = Ndis802_11Encryption2Enabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWPAAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeWPAPSK, dwEncryption, 
                  NULL, ssidTestList[ix], FALSE ,&ulTimeout);         
         if (NOT_SUCCEEDED(hr))
         {
            GetSsidText(szSsidText, ssidTestList[ix]);
            NDTLogErr(_T("Failed to associate test adapter with %s Error:0x%x\n"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }         

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
		 NDTLogDbg(_T("Broadcast Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);

     
         NDIS_802_11_MAC_ADDRESS  abTestMac;
         hr = NDTWlanGetBssid(ahAdapter[0],&abTestMac);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to get Bssid of test adapter Error:0x%x"), NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }
       
         pnKey->Length = dwSize;
         pnKey->KeyLength = 64;
         g_pSupplicant->get_GroupKeyIndex((LONG*)&(pnKey->KeyIndex));      
         memcpy(pnKey->KeyMaterial, WLAN_KEY_TKIP, 64);         
         memcpy(pnKey->BSSID, DummyDestAddr, 6);      
         dwSize = FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial[pnKey->Length]);
      
         hr = NDTWlanSetAddKey(ahAdapter[0], pnKey, dwSize);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_ADD_KEY, Error:0x%x"), NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("Successfully overwrote group key with a different value"));
        
        
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
            NDTLogDbg(_T("PASS: Broadcast send failed to get packets through as expected"));
        
         
      }
      while(0);

      // Ninth Variation
      NDTLogMsg(_T("Variation 9: Verify broadcast packets are not received with pairwise keys"));
      NDTLogMsg(_T("This test will associate in WPA-PSK but will not add the"));
      NDTLogMsg(_T("group key, it will then send\recv and verify send\recv fails"));

      do
      {
         NDTWlanReset(ahAdapter[0], TRUE);
         
         dwEncryption = Ndis802_11Encryption2Enabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWPAAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeWPAPSK, dwEncryption, 
                  NULL, ssidTestList[ix], FALSE ,&ulTimeout);         
         if (NOT_SUCCEEDED(hr))
         {
            GetSsidText(szSsidText, ssidTestList[ix]);
            NDTLogErr(_T("Failed to associate test adapter with %s Error:0x%x\n"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }         
        
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
            NDTLogDbg(_T("PASS: Broadcast send failed to get packets through as expected"));
        
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

