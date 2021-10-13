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

TEST_FUNCTION(TestWlanReloaddefaults)
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
   
   memset(ahAdapter, 0, sizeof(ahAdapter));

   NDTLogMsg(
      _T("Start 2c_wlan_reloaddefaults test\n")
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
         NDTLogErr(_T("Failed to initialize %s Wlan adapter Error0x%x"),((ixAdapter==0)? _T("Test"):_T("Support")), hr);
         FlagError(ErrorSevere,&rc);
         goto cleanUp;
      }
   }

    hr = NDTWlanGetDeviceType(ahAdapter[0],&WlanDeviceType);
    if (NOT_SUCCEEDED(hr))
      goto cleanUp;
   
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
         // First variation
          NDTLogMsg(_T("Variation 1 : Verify setting OID_802_11_RELOAD_DEFAULTS does not cause disconnect"));
		 NDTLogMsg(_T("This test will set OID_802_11_RELOAD_DEFAULTS and verify it does not cause"));
		 NDTLogMsg(_T("the device to disconnect"));

  	    NDTLogMsg(_T("Initializing %s WLAN adapter"), g_szTestAdapter);

         NDTWlanReset(ahAdapter[0], TRUE);

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
            NDTLogDbg(_T("Succesfully associated with %s"), szSsidText);

         hr = NDTWlanSetReloadDefaults(ahAdapter[0], Ndis802_11ReloadWEPKeys);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_RELOAD_DEFAULTS"));
            NDTLogErr(_T("Driver must support setting OID_802_11_RELOAD_DEFAULTS Error:0x%x"), NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("Succesfully set oid reload defaults"));


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

         if (ulConnectStatus == NdisMediaStateDisconnected)
        {
            hr=NDT_STATUS_FAILURE;
            NDTLogErr(_T("Media connect status should have been disconnect. "));
            NDTLogErr(_T("Driver must set the media connect status to disconnect when the device is not associated"));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("PASS: Media state disconnected"));

      }
      while(0);

     	// Second Variation
   
      NDTLogMsg(_T("Variation 2: Verify setting OID_802_11_RELOAD_DEFAULTS does not change the authentication mode\n"));
      NDTLogMsg(_T(" This test will set OID_802_11_RELOAD_DEFAULTS and verify the authentication mode does not change"));

      do
      {
         DWORD dwAuthenticationMode;
         
         NDTWlanReset(ahAdapter[0], TRUE);

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
            NDTLogDbg(_T("Succesfully associated with %s"), szSsidText);
 

         hr = NDTWlanSetReloadDefaults(ahAdapter[0], Ndis802_11ReloadWEPKeys);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_RELOAD_DEFAULTS"));
            NDTLogErr(_T("Driver must support setting OID_802_11_RELOAD_DEFAULTS Error:0x%x"), NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }
        else
            NDTLogDbg(_T("Succesfully set oid reload defaults"));

         hr = NDTWlanGetAuthenticationMode(ahAdapter[0], &dwAuthenticationMode);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to query OID_802_11_AUTHENTICATION_MODE"));
            FlagError(ErrorSevere,&rc);
            break;
         }

         if (Ndis802_11AuthModeOpen != dwAuthenticationMode)
         {
            NDTLogErr(_T("OID_802_11_AUTHENTICATION_MODE should have returned Ndis802_11AuthModeOpen"));
            NDTLogErr(_T("Driver must not change the authentication mode after OID_802_11_RELOAD_DEFAULTS has been set"));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("PASS: Oid authentication mode returned Ndis802_11AuthModeOpen"));
         
      }
      while(0);

      // Third Variation
      
      NDTLogMsg(_T("Variation 3: Verify setting OID_802_11_RELOAD_DEFAULTS changes encryption status\n"));
      NDTLogMsg(_T(" This test will set OID_802_11_RELOAD_DEFAULTS and verify the encription status is changed to Ndis802_11Encryption1KeyAbsent"));

      do
      {
         DWORD dwEncryptionStatus;
         
         NDTWlanReset(ahAdapter[0],TRUE);

         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);         
         if (NOT_SUCCEEDED(hr))
         {
            GetSsidText(szSsidText, ssidList[ix]);
            NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }

         hr = NDTWlanSetReloadDefaults(ahAdapter[0], Ndis802_11ReloadWEPKeys);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_RELOAD_DEFAULTS"));
            NDTLogErr(_T("Driver must support setting OID_802_11_RELOAD_DEFAULTS Error:0x%x"), NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("Succesfully set oid reload defaults"));
       

         hr = NDTWlanGetEncryptionStatus(ahAdapter[0], &dwEncryptionStatus);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to query OID_802_11_ENCRYPTION_STATUS Error:0x%x"), NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }

          if (Ndis802_11Encryption1KeyAbsent == dwEncryptionStatus)
         {
            NDTLogErr(_T("OID_802_11_ENCRYPTION_STATUS should have returned Ndis802_11Encryption1KeyAbsent\n"));
            NDTLogErr(_T("Driver should have changed the encryption status to Ndis802_11Encryption1KeyAbsent unless"));
            NDTLogErr(_T("WEP keys were loaded from the card"));
            FlagError(ErrorSevere,&rc);
            break;
         }
          else
            NDTLogDbg(_T("PASS: Encryption status returned Ndis802_11Encryption1KeyAbsent"));
      }
      while(0);

     	// Fourth Variation
      
      NDTLogMsg(_T("Variation 4: Verify setting OID_802_11_RELOAD_DEFAULTS removes all default WEP keys\n"));
      NDTLogMsg(_T("This test will send\recv using default WEP keys then set \n"));
      NDTLogMsg(_T("OID_802_11_RELOAD_DEFAULTS and verify packets can no longer be received "));

      do
      {
         ULONG ulPacketsToSend = 50;
         ULONG ulPacketsSent = 0;
         ULONG ulPacketsReceived = 0;
         ULONG ulMinDirectedPass = g_dwDirectedPasspercentage * ulPacketsToSend /100;
         
          NDTWlanReset(ahAdapter[0],TRUE);


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

         NDTLogMsg(_T("Setting Reload Defaults"));
         
         hr = NDTWlanSetReloadDefaults(ahAdapter[0], Ndis802_11ReloadWEPKeys);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_RELOAD_DEFAULTS"));
            NDTLogErr(_T("Driver must support setting OID_802_11_RELOAD_DEFAULTS Error:0x%x"), NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("Succesfully set oid reload defaults"));

   
         hr = NDTWlanDirectedSend(ahAdapter[0], ahAdapter[1], cbAddr, ndisMedium,
            ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
         if (FAILED(hr))
         {
            NDTLogErr(_T("DirectedSend failed %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;        
         }

         if (0 != ulPacketsReceived )
         {
            NDTLogErr(_T("No directed packets should have been received (Received: %d, Expected: 0)\n"),ulPacketsReceived);
            NDTLogErr(_T("Driver should have removed all default WEP keys and restored any keys from permanent storage,\n"));
            NDTLogErr(_T("since the stored keys are not the same as the keys used to send in this test or there are no keys available\n"));
            NDTLogErr(_T("the packets should not have been received"));
            
            FlagError(ErrorSevere,&rc);
         }   
         else
            NDTLogDbg(_T("PASS: No directed packets received"));
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

