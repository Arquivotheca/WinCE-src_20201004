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

TEST_FUNCTION(TestWlanWep)
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

   ULONG ulSendPacketsCompleted=0;
   ULONG ulPacketsToSend ;
   ULONG ulPacketsSent = 0;
   ULONG ulPacketsReceived = 0;
   ULONG ulMinDirectedPass;
   ULONG ulMinBroadcastPass;
   ULONG ulSendTime = 0;
   ULONG ulSendBytesSent = 0;
   ULONG ulRecvTime=0;
   ULONG ulRecvBytesReceived =0;
   UINT   uiMinimumPacketSize = 64;
        
   memset(ahAdapter, 0, sizeof(ahAdapter));

   NDTLogMsg(
      _T("Start 2c_wlan_wep test\n")
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

    hr = NDTWlanGetDeviceType(ahAdapter[0],&WlanDeviceType);
    if (NOT_SUCCEEDED(hr))
    {
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
    }
   
    //Test first AP irrespective of Network Type
   dwSsidCount = 1;
   ssidList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WEP_AP1);
   memcpy((ssidList[dwSsidCount -1].Ssid), WLAN_WEP_AP1,ssidList[dwSsidCount -1].SsidLength);

   // Test more APs depending on Network Type
   if (WlanDeviceType == WLAN_DEVICE_TYPE_802_11G || WlanDeviceType == WLAN_DEVICE_TYPE_802_11AB)
   {
      dwSsidCount++;
      ssidList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WEP_AP2);
      memcpy((ssidList[dwSsidCount -1].Ssid), WLAN_WEP_AP2,ssidList[dwSsidCount -1].SsidLength);
   }
   else
      if (WlanDeviceType == WLAN_DEVICE_TYPE_802_11AG)
   {
      dwSsidCount++;
      ssidList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WEP_AP2);
      memcpy((ssidList[dwSsidCount -1].Ssid), WLAN_WEP_AP2,ssidList[dwSsidCount -1].SsidLength);
      dwSsidCount++;
      ssidList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WEP_AP3);
      memcpy((ssidList[dwSsidCount -1].Ssid), WLAN_WEP_AP3,ssidList[dwSsidCount -1].SsidLength);
   }  
      
     
   // Execute tests by ssid
   for (ix= 0; ix<dwSsidCount; ix++)
   {
      TCHAR szSsidText[33];
      GetSsidText(szSsidText, ssidList[ix]);

      do
      {
         DWORD dwEncryptionStatus;
         // First variation
         NDTLogMsg(_T("Variation 1: Verify OID_802_11_ENCRYPTION_STATUS can be set with no key present\n"));
         NDTLogMsg(_T("This test will set the OID_802_11_ENCRYPTION_STATUS while there are no key present "));
         NDTLogMsg(_T("and verify the request succeeds"));

         NDTWlanReset(ahAdapter[0], TRUE);

         hr = NDTWlanSetEncryptionStatus(ahAdapter[0], Ndis802_11Encryption1Enabled);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed set OID_802_11_ENCRYPTION_STATUS Error0x:%x\n"),NDIS_FROM_HRESULT(hr));
            NDTLogErr(_T("Driver must allow OID_802_11_ENCRYPTION_STATUS to be set when there are no key available\n"));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("PASS: Succesfully set oid encryption status"));

         hr = NDTWlanGetEncryptionStatus(ahAdapter[0], &dwEncryptionStatus);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to query OID_802_11_ENCRYPTION_STATUS Error0x:%x\n"),NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }         
      }
      while(0);


	// Second Variation
      
      NDTLogMsg(_T("Variation 2: Verify directed packets can be sent and received using WEP\n"));
      NDTLogMsg(_T("This test will send and receive directed packets using WEP keys."));
      NDTLogMsg(_T("This test will send and receive directed packets using WEP keys\n"));

      do
      {
         ulPacketsToSend = 50;
         ulMinDirectedPass = g_dwDirectedPasspercentage * ulPacketsToSend /100;

           // associating support adapter if the User does no configuration
         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[1], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);         

         GetSsidText(szSsidText, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("Succesfully associated support with %s"),szSsidText);

         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);         

         GetSsidText(szSsidText, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
            NDTLogDbg(_T("Succesfully associated test with %s"),szSsidText);

         NDTLogDbg(_T("Test -> Support Directed send"));
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
            NDTLogErr(_T("Test -> Support -Received less than the required amount of directed packets with WLAN Directed \n"));
            NDTLogErr(_T("send pass Percentage at %d (Received: %d, Expected: %d)"),g_dwDirectedPasspercentage,ulPacketsReceived, ulMinDirectedPass);
            FlagError(ErrorSevere,&rc);
         }
	    else
		 NDTLogDbg(_T("Directed Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);
      
         ulPacketsSent = 0;
         ulPacketsReceived = 0;

         NDTLogDbg(_T("Support -> Test Directed send"));
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
		 NDTLogDbg(_T("Directed Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);
         
      }
      while(0);

     	// THird Variation      
      NDTLogMsg(_T("Variation 3: Verify broadcast packets can be sent and received using WEP \n"));
      NDTLogMsg(_T("This test will send and receive broadcast packets using WEP keys and"));
      NDTLogMsg(_T("verify the minimium number of packets are received\n"));

      do
      {
         ulPacketsToSend = 50;
         ulPacketsSent = 0;
         ulPacketsReceived = 0;
         ulMinBroadcastPass = WLAN_PERCENT_TO_PASS_BROADCAST* ulPacketsToSend /100;
         
         // Assuming support adapter is associated
         // associating support adapter if the User does no configuration
         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[1], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);         

         GetSsidText(szSsidText, ssidList[ix]);     
         if (NOT_SUCCEEDED(hr))
         {
             NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("Succesfully associated support with %s"),szSsidText);



         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);         
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("Succesfully associated test with %s"),szSsidText);


         // Test -> Support
        NDTLogDbg(_T("Test -> Support Broadcast send"));         
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
		 NDTLogDbg(_T("Broadcast Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);

         // Support -> Test
        NDTLogDbg(_T("Support -> Test BRoadcast send"));
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
		 NDTLogDbg(_T("Broadcast Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);
         }
      while(0);


      // Fourth Variation
      
      NDTLogMsg(_T("Variation 4:Verify broadcast and directed packets can be sent using each default WEP key"));
	  NDTLogMsg(_T("This test will send and receive broadcast and directed packets rotating"));
	  NDTLogMsg(_T("through the 4 default key locations, and verify the packets are received"));

      do
      {
         ulPacketsToSend = 50;
         ulMinDirectedPass = g_dwDirectedPasspercentage * ulPacketsToSend /100;
         ulMinBroadcastPass = WLAN_PERCENT_TO_PASS_BROADCAST* ulPacketsToSend /100;
         
         UCHAR puDefaultKeys[4][10];
         ULONG piDefaultIndex[4] = {0x80000000,0x80000001,0x80000002,0x80000003};         

         memcpy((void *)puDefaultKeys[0], WLAN_DEFAULT_KEY1,10);
         memcpy((void *)puDefaultKeys[1], WLAN_DEFAULT_KEY2 ,10);
         memcpy((void *)puDefaultKeys[2], WLAN_DEFAULT_KEY3,10);
         memcpy((void *)puDefaultKeys[3], WLAN_DEFAULT_KEY4,10);
         
           // associating support adapter if the User does no configuration
         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[1], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);         
         if (NOT_SUCCEEDED(hr))
         {
            GetSsidText(szSsidText, ssidList[ix]);
            NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }

         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);         
        GetSsidText(szSsidText, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("Succesfully associated test with %s"),szSsidText);


         for (UINT iKeyIndex=0; iKeyIndex<4;iKeyIndex++)
         {
            PNDIS_802_11_WEP pnWepKey = (PNDIS_802_11_WEP)malloc(
                  FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial[sizeof(puDefaultKeys[iKeyIndex])]) );
            
            pnWepKey->KeyIndex = piDefaultIndex[iKeyIndex];
            pnWepKey->KeyLength = sizeof(puDefaultKeys[iKeyIndex]);
            memcpy(pnWepKey->KeyMaterial, puDefaultKeys[iKeyIndex], sizeof(puDefaultKeys[iKeyIndex]));

			DWORD dwSize = FIELD_OFFSET(NDIS_802_11_WEP, KeyMaterial[pnWepKey->KeyLength]);

            hr = NDTWlanSetAddWep(ahAdapter[0], pnWepKey, dwSize);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed SetAddWep to Test adapter Error0x:%x"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }
            else
               NDTLogDbg(_T("Succesfully added WEP key to test adapter"));

            //pnWepKey->KeyIndex = piDefaultIndex[iKeyIndex] ^ 0x80000000;

            hr = NDTWlanSetAddWep(ahAdapter[1], pnWepKey, dwSize);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed SetAddWep to Support adapter Error0x:%x"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }
            else
               NDTLogDbg(_T("Succesfully added WEP key to support adapter"));


           NDTLogMsg(_T("Sending packets using default WEP key %d\n"), iKeyIndex);

           NDTLogDbg(_T("Test -> Support Directed send"));
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
               NDTLogErr(_T("Test -> Support -Received less than the required amount of directed packets with WLAN Directed"));
               NDTLogErr(_T(" send pass Percentage at %d (Received: %d, Expected: %d)"),g_dwDirectedPasspercentage,ulPacketsReceived, ulMinDirectedPass);
               FlagError(ErrorSevere,&rc);
            }
			else
				NDTLogDbg(_T("Directed Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);

            NDTLogDbg(_T("Test -> Support Broadcast send"));
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
               NDTLogErr(_T("Test -> Support -Received less than the required amount of broadcast packets with WLAN send \n"));
               NDTLogErr(_T("broadcast pass Percentage at %d(Received: %d, Expected: %d)"),WLAN_PERCENT_TO_PASS_BROADCAST,ulPacketsReceived, ulMinBroadcastPass);
               
               FlagError(ErrorSevere,&rc);
            }
			else
				NDTLogDbg(_T("Broadcast Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);
            
            if (pnWepKey)
               free(pnWepKey);
         }         
      }
      while(0);

   	// Fifth Variation
         
      NDTLogMsg(_T("Variation 5: Verify packets are not received after WEP key is removed\n"));
      NDTLogMsg(_T("This test will send and using WEP then remove the WEP key and verify packet can no longer be received\n "));

      do
      {
         ulPacketsToSend = 50;
         ulMinDirectedPass = g_dwDirectedPasspercentage * ulPacketsToSend /100;

           // associating support adapter if the User does no configuration
         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[1], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);         

         GetSsidText(szSsidText, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         } 
         else
            NDTLogDbg(_T("Succesfully associated support with %s"),szSsidText);
         

         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);         

        GetSsidText(szSsidText, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("Succesfully associated test with %s"),szSsidText);
         

         NDTLogDbg(_T("Test -> Support Directed send"));
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
            NDTLogErr(_T("Test -> Support -Received less than the required amount of directed packets with WLAN Directed"));
            NDTLogErr(_T(" send pass Percentage at %d (Received: %d, Expected: %d)"),g_dwDirectedPasspercentage,ulPacketsReceived, ulMinDirectedPass);
            FlagError(ErrorSevere,&rc);
         }
         else
      		NDTLogDbg(_T("Directed Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);


         hr = NDTWlanSetRemoveWep(ahAdapter[0], 0);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_REMOVE_WEP Error0x:%x"),NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("Succesfully removed WEP key index 0 from test adapter"));

         NDTLogDbg(_T("Test -> Support Directed send"));
         hr = NDTWlanDirectedSend(ahAdapter[0], ahAdapter[1], cbAddr, ndisMedium,
            ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("DirectedSend failed %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;        
         }         
         
         if (ulPacketsReceived != 0)
         {
            NDTLogErr(_T("Should not have received any directed packets\n"));
            NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinDirectedPass);
            FlagError(ErrorSevere,&rc);
         }      
         else
            NDTLogDbg(_T("PASS: No directed packets could be received as expected"));
         
      }
      while(0);

     	// Sixth Variation
      
      NDTLogMsg(_T("Variation 6: Verify device does not disassociate after the WEP key is removed\n"));
      NDTLogMsg(_T("This test will send and using WEP then remove and verify the device does not disassociate\n"));

      do
      {
         ulPacketsToSend = 50;
         ulMinDirectedPass = g_dwDirectedPasspercentage * ulPacketsToSend /100;

           // associating support adapter if the User does no configuration
         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[1], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);         

         GetSsidText(szSsidText, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }         
         else
            NDTLogDbg(_T("Succesfully support associated with %s"),szSsidText);

         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);         

         GetSsidText(szSsidText, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("Succesfully associated test with %s"),szSsidText);

         NDTLogDbg(_T("Test -> Support Directed send"));
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
            NDTLogErr(_T("Test -> Support -Received less than the required amount of directed packets with WLAN Directed "));
            NDTLogErr(_T("send pass Percentage at %d (Received: %d, Expected: %d)"),g_dwDirectedPasspercentage,ulPacketsReceived, ulMinDirectedPass);
            FlagError(ErrorSevere,&rc);
         }         
         else
             NDTLogDbg(_T("PASS: Directed send %d packets sent %d packets received"),ulPacketsSent,ulPacketsReceived);            
       

         hr = NDTWlanSetRemoveWep(ahAdapter[0], 0);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T(" Failed to set OID_802_11_REMOVE_WEP Error0x:%x"),NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }

         Sleep(10000);

         ulConnectStatus = 0;
         cbUsed = 0;
         cbRequired = 0;
         hr = NDTQueryInfo(ahAdapter[0], OID_GEN_MEDIA_CONNECT_STATUS, &ulConnectStatus,
                  sizeof(ulConnectStatus),&cbUsed,&cbRequired);
         if (NOT_SUCCEEDED(hr)) 
         {
            NDTLogErr(_T("Unable to query OID_GEN_MEDIA_CONNECT_STATUS with hr=0x%08x"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

         if (ulConnectStatus != NdisMediaStateConnected)
        {
            hr=NDT_STATUS_FAILURE;
            NDTLogErr(_T("Media connect status should not have disconnected after key was removed"));
            FlagError(ErrorSevere,&rc);
            break;
         }        
         else
            NDTLogDbg(_T("PASS: Media state is still connected"));
      }
      while(0);

    	// Seventh Variation
      
      NDTLogMsg(_T("Variation 7: Run stress while adding\removing keys\n"));
      NDTLogMsg(_T("This test will run stress on the test device while adding and removing WEP keys\n"));

      do
      {
           // associating support adapter if the User does no configuration
         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[1], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);         

         GetSsidText(szSsidText, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("Succesfully associated support with %s"),szSsidText);

         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);         

         GetSsidText(szSsidText, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("Succesfully associated test with %s"),szSsidText);
         
         hr = NDTSetPacketFilter(ahAdapter[1], NDT_FILTER_DIRECTED);   
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("vbSetPacketFilter() failed Error0x:%x"),NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }

         HANDLE hReceive;
         HANDLE hSend;
         UCHAR *pucPermanentAddr;

         ulPacketsToSend = 1500;
         ulMinDirectedPass = g_dwDirectedPasspercentage * ulPacketsToSend /100;
         
         hr = NDTReceive(ahAdapter[1], &hReceive);
         if (FAILED(hr))
         {
            NDTLogErr(_T("Failed to start receive Error:0x%x\n"),NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("Succesfully started Receive on support"));

         hr = NDTGetPermanentAddr(ahAdapter[1], ndisMedium, &pucPermanentAddr);
         if (NOT_SUCCEEDED(hr)) 
         {
            NDTLogErr(_T("Failed to get permanent address Error:0x%x\n"),NDIS_FROM_HRESULT(hr));         
            if (NULL != pucPermanentAddr)
               delete pucPermanentAddr;
            FlagError(ErrorSevere,&rc);
            break;
         }

         NDTLogDbg(_T("Starting send on the test adapter"));
         hr = NDTSend(ahAdapter[0], cbAddr, NULL, 1,&pucPermanentAddr, NDT_RESPONSE_NONE, NDT_PACKET_TYPE_FIXED,
            1024, ulPacketsToSend, 0, 0, &hSend);


         DWORD dwSize = FIELD_OFFSET(NDIS_802_11_WEP,KeyMaterial[10]);
         PNDIS_802_11_WEP pnWepKey = (PNDIS_802_11_WEP )malloc(dwSize);

         pnWepKey->KeyIndex = 0x3;
		 pnWepKey->KeyLength = 10;
         memcpy(pnWepKey->KeyMaterial,WLAN_KEY_WEP,10);

         BOOL bErrorInLoop = FALSE;
         for(UINT iy=20; iy >0; iy--)
         {
            hr = NDTWlanSetAddWep(ahAdapter[0], pnWepKey, dwSize);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("SetAddWEp failed Error0x:%x"),NDIS_FROM_HRESULT(hr));
               bErrorInLoop = TRUE;
               FlagError(ErrorSevere,&rc);
               break;
            }
            else
               NDTLogDbg(_T("Succesfully added WEP key while sending receiving data"));

            hr = NDTWlanSetRemoveWep(ahAdapter[0], pnWepKey->KeyIndex);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("SetAddWEp failed Error0x:%x"),NDIS_FROM_HRESULT(hr));
               bErrorInLoop = TRUE;
               FlagError(ErrorSevere,&rc);
               break;
            }
            else
               NDTLogDbg(_T("Succesfully removed WEP key while sending receiving data"));

         }

         if (bErrorInLoop)
            break;

          // Wait till sending is done
         hr = NDTSendWait(ahAdapter[0], hSend,INFINITE, &ulPacketsSent , &ulSendPacketsCompleted,
               NULL, NULL, NULL, &ulSendTime, &ulSendBytesSent, NULL);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("FAiled in SendWait Error:0x%x"),NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }
         
         // Stop receiving and check results
         hr = NDTReceiveStop(ahAdapter[1], hReceive, &ulPacketsReceived, NULL, NULL, 
            &ulRecvTime, NULL, &ulRecvBytesReceived);

         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("FAiled in RecvStop Error:0x%x"),NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }

         if (ulPacketsReceived < ulMinDirectedPass)
         {
            NDTLogErr(_T("Test -> Support -Received less than the required amount of directed packets with WLAN Directed "));
            NDTLogErr(_T("send pass Percentage at %d (Received: %d, Expected: %d)"),g_dwDirectedPasspercentage,ulPacketsReceived, ulMinDirectedPass);
            FlagError(ErrorSevere,&rc);
         }         
         else
             NDTLogDbg(_T("PASS: Directed send %d packets sent %d packets received"),ulPacketsSent,ulPacketsReceived);            
       

         if (pnWepKey)
            free(pnWepKey);
      }
      while(0);

	// Eight Variation
      
      NDTLogMsg(_T("Variation 8: Verify OID_802_11_ADD_WEP fails when setting an invalid key index\n"));
      NDTLogMsg(_T("This test will OID_802_11_ADD_WEP with an invalid WEP key index and verify the request fails \n"));

      do
      {
         DWORD dwSize;
         PNDIS_802_11_WEP pnWepKey = (PNDIS_802_11_WEP )malloc(FIELD_OFFSET(NDIS_802_11_WEP,KeyMaterial[10]));
         pnWepKey->KeyIndex = 0x8000000;
         memcpy(pnWepKey->KeyMaterial,WLAN_KEY_WEP,10);
         dwSize = sizeof(NDIS_802_11_WEP) +  10 -1;
   
         hr = NDTWlanSetAddWep(ahAdapter[0], pnWepKey, dwSize);
         if (!NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Should not have return NDIS_STATUS_SUCCESS Error:0x%x"),NDIS_FROM_HRESULT(hr));
            NDTLogErr(_T("Driver must return NDIS_STATUS_INVALID_DATA when OID_802_11_ADD_WEP is set"));
            NDTLogErr(_T("with an invalid key index 0x80000000"));
            FlagError(ErrorSevere,&rc);
            break;            
         }
         else
            NDTLogDbg(_T("PASS: Adding key with invalid key index 0x80000000 failed as expected"));

         pnWepKey->KeyIndex = 0xFFFFFFFF;
         dwSize = sizeof(NDIS_802_11_WEP) +  10 -1;
         memcpy(pnWepKey->KeyMaterial,WLAN_KEY_WEP,10);

        hr = NDTWlanSetAddWep(ahAdapter[0], pnWepKey, dwSize);
         if (!NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Should not have return NDIS_STATUS_SUCCESS Error:0x%x"),NDIS_FROM_HRESULT(hr));
            NDTLogErr(_T("Driver must return NDIS_STATUS_INVALID_DATA when OID_802_11_ADD_WEP is set"));
            NDTLogErr(_T("with an invalid key index 0xFFFFFFFFF"));
            FlagError(ErrorSevere,&rc);
            break;            
         }
         else
            NDTLogDbg(_T("PASS: Adding key with invalid key index 0xFFFFFFFF failed as expected"));

         if (pnWepKey)
            free(pnWepKey);
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


