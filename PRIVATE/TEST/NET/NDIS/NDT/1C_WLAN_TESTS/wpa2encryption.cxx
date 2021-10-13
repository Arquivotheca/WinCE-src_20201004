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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
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
#include "ndt_1c_wlan.h"
#include "wpadata.h"
#include "supplicant.h"

extern CSupplicant* g_pSupplicant;

//------------------------------------------------------------------------------

TEST_FUNCTION(TestWpa2Encryption)
{
   TEST_ENTRY;

   int rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;
   HANDLE hAdapter = NULL;

   UINT  nAdapters = 1;
   UINT  ixAdapter = 0;
   UINT  ix = 0;
   DWORD nValue = 0;

   HANDLE hStatus = NULL;
   ULONG ulTimeout;
   NDIS_802_11_SSID ssidList[3];
   DWORD dwSsidCount;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;
   DWORD dwEncryption;

   BOOL bWPA2Supported;
   
   

   NDTLogMsg(
      _T("Start 1c_wpa2_encryption on the adapter %s"), g_szTestAdapter
   );

   NDTLogMsg(_T("Opening adapter"));
   hr = NDTOpen(g_szTestAdapter, &hAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }

   hr = NDTBind(hAdapter, bForce30, ndisMedium);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailBind, hr);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }

   hr = NDTWlanInitializeTest(hAdapter);
   if (FAILED(hr))
   {
      NDTLogErr(_T("Failed to initialize Wlan test Error0x%x"),hr);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }

   g_pSupplicant = new CSupplicant();
   if (!g_pSupplicant)
   {
      NDTLogErr(_T("Failed to create supplicant"));
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }
      
   if (FALSE == (g_pSupplicant->OpenSupplicant(g_szTestAdapter)))
   {
      NDTLogErr(_T("Failed to open supplicant"));
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }

   hr = NDTWlanGetDeviceType(hAdapter,&WlanDeviceType);
    if (NOT_SUCCEEDED(hr))
    {
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
    }

   hr = NDTWlanIsWPA2Supported(hAdapter, &bWPA2Supported);
   if (FAILED(hr))
   {
      NDTLogErr(_T("Failed to get IsWPA2Supported Error:0x%x"),hr);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }
   if (FALSE == bWPA2Supported)
   {
      NDTLogErr(_T("%s does not support WPA2, skipping test"), g_szTestAdapter);
      rc = TPR_SKIP;
      goto cleanUp;
   }
   
   //Test against WPA AP 
   dwSsidCount = 1;
   ssidList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WPA2_AP1);
   memcpy((ssidList[dwSsidCount -1].Ssid), WLAN_WPA2_AP1,ssidList[dwSsidCount -1].SsidLength);
   
   // Execute test by ssid
   for (ix= 0; ix<dwSsidCount; ix++)
   {
      TCHAR szSsidText[33];
         
      GetSsidText(szSsidText, ssidList[ix]);

      // First Variation
      NDTLogMsg(_T("Variation 1: Verify Ndis802_11Encryption3Enabled is return when associated with a WPA2-PSK AP %s"), szSsidText);
      NDTLogMsg(_T("This test will associate with a WPA2-PSK AP, query OID_802_11_ENCRYPTION_STATUS"));
      NDTLogMsg(_T("and verify Ndis802_11Encryption3Enabled is returned"));

      do 
      {
         DWORD dwEncryptionStatus;
                  
         dwEncryption = Ndis802_11Encryption3Enabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWPAAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeWPA2PSK, dwEncryption, 
                  NULL, ssidList[ix], FALSE ,&ulTimeout);         

         GetSsidText(szSsidText, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("Succesfully associated with %s"),szSsidText);



         hr = NDTWlanGetEncryptionStatus(hAdapter, &dwEncryptionStatus);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to query Encryption status Error:0x%x\n"),hr);
            NDTLogErr(_T("Driver must support querying OID_802_11_ENCRYPTION_STATUS while associated"));
            FlagError(ErrorSevere,&rc);            
            break;
         }

         if (Ndis802_11Encryption3Enabled != dwEncryptionStatus)
         {
            TCHAR szEncryptionText[100];
            GetEncryptionText(szEncryptionText,dwEncryptionStatus);
            NDTLogErr(_T(" Should have returned Ndis802_11Encryption3Enabled, Returned %s"), szEncryptionText);
            NDTLogErr(_T("Driver must return the current encryption in use"));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("Encryption status returned Ndis802_11Encryption3Enabled"));
      }
      while(0);

      // Second Variation
      NDTLogMsg(_T("Verify Ndis802_11Encryption3KeyAbsent is returned when all keys are removed"));
      NDTLogMsg(_T("This test will associate with a WPA2 AP, after authentication complete all keys"));
      NDTLogMsg(_T("while be removed. It will then query OID_802_11_ENCRYPTION_STATUS and verify"));
      NDTLogMsg(_T("Ndis802_11Encryption3KeyAbsent is returned"));
      
      do
      {
         DWORD dwEncryptionStatus;

         dwEncryption = Ndis802_11Encryption3Enabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWPAAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeWPA2PSK, dwEncryption, 
                  NULL, ssidList[ix], FALSE ,&ulTimeout);         
        GetSsidText(szSsidText, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("Succesfully associated with %s"),szSsidText);

         // Remove Pairwise key
         NDIS_802_11_REMOVE_KEY nRemoveKey;
         nRemoveKey.KeyIndex = 0x40000000;
         memcpy(nRemoveKey.BSSID, WLAN_KEYTYPE_DEFAULT, sizeof(nRemoveKey.BSSID));

         hr = NDTWlanSetRemoveKey(hAdapter, &nRemoveKey);
         if (NOT_SUCCEEDED(hr))
             break;    
     
         hr = NDTWlanGetEncryptionStatus(hAdapter, &dwEncryptionStatus);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to query Encryption status Error:0x%x\n"),hr);
            FlagError(ErrorSevere,&rc);            
            break;
         } 

         if (Ndis802_11Encryption3KeyAbsent != dwEncryptionStatus)
         {
            NDTLogWrn(_T(" Encryption status should returned Ndis802_11Encryption3KeyAbsent (%d) Returned (%d)\n"),Ndis802_11Encryption3KeyAbsent, dwEncryptionStatus);
            NDTLogWrn(_T(" Driver appears to not support key mapping,\n"));
         }
         else
            NDTLogDbg(_T("PASS: Encryption status returned Ndis802_11Encryption2Enabled"));
      }
      while(0);
      
   }   

cleanUp:
 
   NDTWlanCleanupTest(hAdapter);

    if (g_pSupplicant)
   {
      g_pSupplicant->CloseSupplicant();
      delete g_pSupplicant;
      g_pSupplicant = NULL;
   }

   hr = NDTUnbind(hAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailUnbind, hr);
   }
   
   if (hAdapter != NULL) {
      hr = NDTClose(&hAdapter);
      if (FAILED(hr))
         NDTLogErr(g_szFailClose, hr);
   }   

   return rc;  
}

