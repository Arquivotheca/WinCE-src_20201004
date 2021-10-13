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

TEST_FUNCTION(TestWpaAuthentication)
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
   NDIS_802_11_SSID ssidList[3];
   DWORD dwSsidCount;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;
   DWORD dwEncryption;

   BOOL bWPASupported;
   BOOL bWPAAdhocSupported;
   BOOL status;
   
   UINT   cbAddr = 0;
   UINT   cbHeader = 0;   
   DWORD  dwReceiveDelay = 0;   

   
   memset(ahAdapter, 0, sizeof(ahAdapter));

   NDTLogMsg(
      _T("Start 2c_wpa_authentication test\n")
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
   ssidList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WPA_AP1);
   memcpy((ssidList[dwSsidCount -1].Ssid), WLAN_WPA_AP1,ssidList[dwSsidCount -1].SsidLength);
   
   // Execute test by ssid
   for (ix= 0; ix<dwSsidCount; ix++)
   {
      TCHAR szSsidText[33];
         
      GetSsidText(szSsidText, ssidList[ix]);

      // First Variation
      NDTLogMsg(_T("Variation 1: Set OID_802_11_AUTHENTICATION_MODE with invalid value"));
      NDTLogMsg(_T("This test will set OID_802_11_AUTHENTICATION_MODE with an invalid authentication mode and verify the request fails"));

      DWORD dwInvalidAuthMode = 20;
      hr = NDTWlanSetAuthenticationMode(ahAdapter[0], dwInvalidAuthMode);
      if (!NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Should have returned NDIS_STATUS_INVALID_DATA"));
         NDTLogErr(_T("Returned %d instead"),NDIS_FROM_HRESULT(hr));
         FlagError(ErrorSevere,&rc);
      }
      else
         NDTLogDbg(_T("PASS: Returned ndis status invalid data as expected"));
       

      // Second Variation
      NDTLogMsg(_T("Variation 2: Verifying all valid authentication modes are supported"));
      NDTLogMsg(_T("This test will loop through and set all the valid authentication modes and verify each mode can be set successfully"));
      
      DWORD aAuthenticationModes[]={Ndis802_11AuthModeOpen, Ndis802_11AuthModeShared, Ndis802_11AuthModeWPA, 
            Ndis802_11AuthModeWPAPSK, Ndis802_11AuthModeWPANone};

      for (UINT iIndex=0; iIndex<(sizeof(aAuthenticationModes)/sizeof(aAuthenticationModes[0])); iIndex++)         
      {
         do
         {       
            DWORD dwSetAuthMode;
            TCHAR szText [80];
            GetAuthModeText(szText, aAuthenticationModes[iIndex]);
            NDTLogDbg(_T("Testing Authentication mode %s"), szText);
              
            //If WPA adhoc is not supported don't attempt to set this authentication mode  
            if (Ndis802_11AuthModeWPANone == aAuthenticationModes[iIndex] && bWPAAdhocSupported)
            {
               break;
            }

            hr = NDTWlanSetAuthenticationMode(ahAdapter[0], aAuthenticationModes[iIndex]);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to set OID_802_11_AUTHENTICATION_MODE to %s Error0x:%x")
                     ,szText, NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }

            hr = NDTWlanGetAuthenticationMode(ahAdapter[0], &dwSetAuthMode);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to get Authentication mode Error0x:%x"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }

            if (aAuthenticationModes[iIndex] != dwSetAuthMode)
            {               
               NDTLogErr(_T("Should have returned %s"), szText);
               NDTLogErr(_T("Driver must return the authentication mode previously set"));
               FlagError(ErrorSevere,&rc);
            }
            else
               NDTLogDbg(_T("PASS: Returned set authentication mode"));
         }
         while(0);
      }


      // Variation 3
      NDTLogMsg(_T("Variation 3: Associate with WEP AP using WPAPSK"));
      NDTLogMsg(_T("This test will associate with an open WEP only AP using WPAPSK and verify the association fails"));

      do
      {
         NDIS_802_11_SSID ssid;
         ssid.SsidLength = 10;
         memcpy(ssid.Ssid,WLAN_WEP_AP1,10);
        
         dwEncryption = Ndis802_11Encryption2Enabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeWPAPSK, &dwEncryption, 
                 0, 0, (PBYTE)WLAN_KEY_WEP, NULL, ssid, FALSE ,&ulTimeout);         

         GetSsidText(szSsidText, ssid);
         if (!NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Should not have associated with %s "),szSsidText);
            NDTLogErr(_T("Device must not associate with a WEP AP when using WPAPSK"));
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("PASS: Test could not associate as expected"));
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

