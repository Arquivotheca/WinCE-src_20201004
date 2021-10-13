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

TEST_FUNCTION(TestWpa2Misc)
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
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;
   DWORD dwEncryption;

   BOOL bWPA2Supported;
   BOOL status;
   
   
   memset(ahAdapter, 0, sizeof(ahAdapter));

   NDTLogMsg(
      _T("Start 2c_wpa2_misc test\n")
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
         NDTLogErr(_T("Failed to initialize Wlan %s adapter Error0x%x"),(ixAdapter?_T("Support"):_T("Test")),hr);
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

   // First Variation
   NDTLogMsg(_T("Variation 1: Associate with WEP AP using WPA2-PSK"));
   NDTLogMsg(_T("This test will associate with a Open WEP AP using WPA2-PSK and verify the"));
   NDTLogMsg(_T("association fails"));

   do
   {
      NDIS_802_11_SSID WEPSsid;
      WEPSsid.SsidLength = strlen((char*)WLAN_WEP_AP1);
      memcpy(WEPSsid.Ssid, WLAN_WEP_AP1, WEPSsid.SsidLength);
      
      dwEncryption = Ndis802_11Encryption3Enabled;
      ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
      hr = NDTWlanAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeWPA2PSK, &dwEncryption, 
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

   // Second Variation
   NDTLogMsg(_T("Variation 2: Associate with WPA-PSK AP using WPA2-PSK"));
   NDTLogMsg(_T("This test will associate with a WPA-PSK AP using WPA2-PSK and verify the"));
   NDTLogMsg(_T("association fails"));

   do
   {
      NDIS_802_11_SSID WPASsid;
      WPASsid.SsidLength = strlen((char*)WLAN_WPA_AP1);
      memcpy(WPASsid.Ssid, WLAN_WPA_AP1, WPASsid.SsidLength);
      
      dwEncryption = Ndis802_11Encryption3Enabled;
      ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
      hr = NDTWlanAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeWPA2PSK, &dwEncryption, 
              0, 0, NULL, NULL, WPASsid, FALSE ,&ulTimeout);         

      GetSsidText(szSsidText, WPASsid);
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

