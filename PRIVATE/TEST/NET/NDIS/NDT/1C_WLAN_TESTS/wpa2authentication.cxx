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
#include "ndt_1c_wlan.h"
#include <ndis.h>


//------------------------------------------------------------------------------

TEST_FUNCTION(TestWpa2Authentication)
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

   ULONG ulTimeout = 0;
   DWORD dwEncryption = 0;

   HANDLE hStatus = NULL;
   NDIS_802_11_SSID ssidList[3];
   DWORD dwSsidCount;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;

   BOOL bWPA2Supported = FALSE;
   
   DWORD dwActualAuthMode= 0;           
   
   

   NDTLogMsg(
      _T("Start 1c_wpa2_authentication test on the adapter %s\n"), g_szTestAdapter
   );

   NDTLogMsg(_T("Opening adapter\n"));
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
      NDTLogErr(_T("Failed to initialize Wlan test Error0x%x\n"),hr);
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
      NDTLogErr(_T("Failed to get IsWPA2Supported Error:0x%x\n"),hr);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }
   if (FALSE == bWPA2Supported)
   {
      NDTLogErr(_T("Test adapter %s does not support WPA2, skipping test\n"), g_szTestAdapter);
      rc = TPR_SKIP;
      goto cleanUp;
   }
   
   //Test against WPA2 AP 
   dwSsidCount = 1;
   ssidList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WPA2_AP1);
   memcpy((ssidList[dwSsidCount -1].Ssid), WLAN_WPA2_AP1,ssidList[dwSsidCount -1].SsidLength);
         
   
   // Execute test by ssid
   for (ix= 0; ix<dwSsidCount; ix++)
   {
      TCHAR szSsidText[33];
      GetSsidText(szSsidText, ssidList[ix]);

      // First Variation
      NDTLogMsg(_T("Variation 1: Set OID_802_11_AUTHENTICATION_MODE with invalid value"));
      NDTLogMsg(_T("This test will set OID_802_11_AUTHENTICATION_MODE with an invalid authentication mode and verify the request fails"));

      DWORD dwInvalidAuthMode = 20;
      hr = NDTWlanSetAuthenticationMode(hAdapter, dwInvalidAuthMode);
      if (!NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Should have returned NDIS_STATUS_INVALID_DATA"));
         NDTLogErr(_T("Returned %d instead"),NDIS_FROM_HRESULT(hr));
         FlagError(ErrorSevere,&rc);
      }

       // Second Variation
       NDTLogMsg(_T("Variation 2: Set OID_802_11_AUTHENTICATION_MODE with Ndis802_11AuthModeWPA2 with no key set"));
       NDTLogMsg(_T("This test will set OID_802_11_AUTHENTICATION_MODE with Ndis802_11AuthModeWPA2"));
       NDTLogMsg(_T("with no key set and verify the request succeeds"));

       do
       {            
         hr = NDTWlanSetAuthenticationMode(hAdapter, Ndis802_11AuthModeWPA2);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_AUTHENTICATION_MODE to Ndis802_11AuthModeWPA2 Error0x:%x"),NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }         

         hr = NDTWlanGetAuthenticationMode(hAdapter, &dwActualAuthMode);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to query OID_802_11_AUTHENTICATION_MODE Error0x:%x"),NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }

         if (Ndis802_11AuthModeWPA2 != dwActualAuthMode)
         {
            TCHAR szText[40];
            GetAuthModeText(szText, dwActualAuthMode);
            NDTLogErr(_T("Unexpcted authentication mode returned (Returned: %s Expected Ndis802_11AuthModeWPA2)"), szText);
            FlagError(ErrorSevere,&rc);
         }
          else
            NDTLogDbg(_T("PASS: Ndis802_11AuthModeWPA2 returned"));
       }
       while(0);

      // Third Variation
      NDTLogMsg(_T("Variation 3: Set OID_802_11_AUTHENTICATION_MODE with Ndis802_11AuthModeWPA2PSK with no key set"));
      NDTLogMsg(_T("This test will set OID_802_11_AUTHENTICATION_MODE with Ndis802_11AuthModeWPA2PSK"));
      NDTLogMsg(_T("with no key set and verify the request succeeds"));

      do
      {            
         hr = NDTWlanSetAuthenticationMode(hAdapter, Ndis802_11AuthModeWPA2PSK);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_AUTHENTICATION_MODE to Ndis802_11AuthModeWPA2PSK Error0x:%x"),NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }         

         hr = NDTWlanGetAuthenticationMode(hAdapter, &dwActualAuthMode);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to query OID_802_11_AUTHENTICATION_MODE Error0x:%x"),NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }

         if (Ndis802_11AuthModeWPA2PSK!= dwActualAuthMode)
         {
            TCHAR szText[40];
            GetAuthModeText(szText, dwActualAuthMode);
            NDTLogErr(_T("Unexpcted authentication mode returned (Returned: %s Expected Ndis802_11AuthModeWPA2PSK)"), szText);
            FlagError(ErrorSevere,&rc);
         }         
         else
            NDTLogDbg(_T("PASS: Ndis802_11AuthModeWPA2PSK returned"));
      }
      while(0);

      // Fourth Variation
      NDTLogMsg(_T("Associate with WEP AP using WPA2-PSK"));
      NDTLogMsg(_T("This test will associate with a WEP Only AP using WPA2-PSK and verify the"));
      NDTLogMsg(_T("association fails"));

      do
      {
         NDIS_802_11_SSID WEPSsid;
         WEPSsid.SsidLength = strlen((char*)WLAN_WEP_AP1);
         memcpy(WEPSsid.Ssid, WLAN_WEP_AP1, WEPSsid.SsidLength);
         
         dwEncryption = Ndis802_11Encryption3Enabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeWPA2PSK, &dwEncryption, 
                 0, 0, NULL, NULL, WEPSsid, FALSE ,&ulTimeout);         

         GetSsidText(szSsidText, WEPSsid);
         if (!NOT_SUCCEEDED(hr))
         {            
            NDTLogErr(_T("Should have failed to associate with %s using Authentication WPA2PSK, Returned success"),szSsidText);
            FlagError(ErrorSevere,&rc);            
            break;
         }         
         else            
            NDTLogDbg(_T("Succesfully associated with %s"),szSsidText);

      }
      while(0);
      
   }
 
cleanUp: 
 
   NDTWlanCleanupTest(hAdapter);

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

