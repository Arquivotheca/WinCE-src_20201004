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

TEST_FUNCTION(TestWpaEncryption)
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

   BOOL bWPASupported;
   
   

   NDTLogMsg(
      _T("Start 1c_wpa_encryption on the adapter %s"), g_szTestAdapter
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

   hr = NDTWlanIsWPASupported(hAdapter, &bWPASupported);
   if (FAILED(hr))
   {
      NDTLogErr(_T("Failed to get IsWPASupported Error:0x%x"),hr);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }
   if (FALSE == bWPASupported)
   {
      NDTLogErr(_T("%s does not support WPA, skipping test"), g_szTestAdapter);
      rc = TPR_SKIP;
      goto cleanUp;
   }
   
   //Test against WPA AP 
   dwSsidCount = 1;
   ssidList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WPA_AP1);
   memcpy((ssidList[dwSsidCount -1].Ssid), WLAN_WPA_AP1,ssidList[dwSsidCount -1].SsidLength);
   
   // Execute test by ssid
   for (ix= 0; ix<dwSsidCount; ix++)
   {
      TCHAR szSsidText[33];
         
      GetSsidText(szSsidText, ssidList[ix]);

      // First Variation
      NDTLogMsg(_T("Variation 1: Associate with WPA AP %s and verify OID_802_11_ENCRYPTION_STATUS\n"), szSsidText);
      NDTLogMsg(_T("returns Ndis802_11Encryption2Enabled This test will associate with the WPA AP and verify TKIP encryption is returned"));

      do 
      {
         DWORD dwEncryptionStatus;
                  
         dwEncryption = Ndis802_11Encryption2Enabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWPAAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeWPAPSK, dwEncryption, 
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

         if (Ndis802_11Encryption2Enabled != dwEncryptionStatus)
         {
            NDTLogErr(_T(" Should have returned TKIP enabled\n"));
            NDTLogErr(_T("Driver must return the current encryption in use\n"));
            FlagError(ErrorSevere,&rc);
         }
         else
            NDTLogDbg(_T("PASS: Returned TKIP enabled"));
      }
      while(0);

      // Second Variation
      NDTLogMsg(_T("Variation 2: Set OID_802_11_ENCRYPTION_STATUS with Ndis802_11Encryption2Enabled\n"));
      NDTLogMsg(_T("This test will set OID_802_11_ENCRYPTION_STATUS to enable TKIP encryption and verify the request succeeds\n"));
      
      do
      {
         DWORD dwEncryptionStatus;
         
         // This will remove all keys
         NDTWlanReset(hAdapter, TRUE);

         dwEncryptionStatus = Ndis802_11Encryption2Enabled;
         hr = NDTWlanSetEncryptionStatus(hAdapter, dwEncryptionStatus);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set Encryption status Error:0x%x\n"),hr);
            NDTLogErr(_T("Driver must allow OID_802_11_ENCRYPTION_STATUS to be set with Ndis802_11Encryption2Enabled when WPA is supported"));
            FlagError(ErrorSevere,&rc);            
            break;
         }

         hr = NDTWlanGetEncryptionStatus(hAdapter, &dwEncryptionStatus);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to query Encryption status Error:0x%x\n"),hr);
            FlagError(ErrorSevere,&rc);            
            break;
         } 

         if (Ndis802_11Encryption2KeyAbsent != dwEncryptionStatus)
         {
            NDTLogWrn(_T(" Encryption status should returned Ndis802_11Encryption2KeyAbsent (%d) Returned (%d)\n"),Ndis802_11Encryption2KeyAbsent, dwEncryptionStatus);
            NDTLogWrn(_T(" Driver appears to not support key mapping,\n"));
         }
         else
            NDTLogDbg(_T("PASS: Returned Ndis802_11Encryption2KeyAbsent"));
      }
      while(0);

      BOOL bIsAESSupported;
      hr = NDTWlanIsAESSupported(hAdapter, &bIsAESSupported);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Failed to check IsAESSupported Error:0x%x"), NDIS_FROM_HRESULT(hr));
         FlagError(ErrorSevere,&rc);
      }
      else
      {
         if (TRUE == bIsAESSupported)
         {
            NDTLogDbg(_T("Checkin for AES as its supported in the card"));
            do
            {
               DWORD dwEncryptionStatus;
       
               NDTWlanReset(hAdapter,TRUE);

               dwEncryptionStatus = Ndis802_11Encryption3Enabled;
               hr = NDTWlanSetEncryptionStatus(hAdapter, dwEncryptionStatus);
               if (NOT_SUCCEEDED(hr))
               {
                  NDTLogErr(_T("Failed to set Encryption status Error:0x%x\n"),hr);
                  NDTLogErr(_T("Driver must allow OID_802_11_ENCRYPTION_STATUS to be set with Ndis802_11Encryption3Enabled when AES is supported"));
                  FlagError(ErrorSevere,&rc);            
                  break;
               }

               hr = NDTWlanGetEncryptionStatus(hAdapter, &dwEncryptionStatus);
               if (NOT_SUCCEEDED(hr))
               {
                  NDTLogErr(_T("Failed to query Encryption status Error:0x%x\n"),hr);
                  FlagError(ErrorSevere,&rc);            
                  break;
               } 

               if (Ndis802_11Encryption3Enabled != dwEncryptionStatus && Ndis802_11Encryption3KeyAbsent != dwEncryptionStatus)
               {
                  NDTLogErr(_T(" Encryption status should returned Ndis802_11Encryption3KeyAbsent\n"));
                  NDTLogErr(_T("Driver must allow OID_802_11_ENCRYPTION_STATUS to be set with Ndis802_11Encryption3Enabled when AES is supported\n"));
                  FlagError(ErrorSevere,&rc);
               }
               else
                  NDTLogDbg(_T("PASS: Encryption status returned %d for AES"),dwEncryptionStatus);
               
            }
            while(0);
         }
      }
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

