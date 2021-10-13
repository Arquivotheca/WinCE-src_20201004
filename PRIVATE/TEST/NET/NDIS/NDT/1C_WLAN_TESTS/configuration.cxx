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

//------------------------------------------------------------------------------

TEST_FUNCTION(TestWlanConfiguration)
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
   ULONG ulKeyLength;
   ULONG ulTimeout;
   NDIS_802_11_SSID ssidList[3];
   DWORD dwSsidCount;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;
   DWORD dwEncryption;
   DWORD dwNetworkTypeInUse;
   
   

   NDTLogMsg(
      _T("Start 1c_wlan_configuration on the adapter %s\n"), g_szTestAdapter
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
      
   
   
   // Execute test by ssid
   for (ix= 0; ix<dwSsidCount; ix++)
   {
      NDIS_802_11_CONFIGURATION nConfig;
      DWORD dwSize;
      TCHAR szSsidText[33];
         
      NDTWlanReset(hAdapter, TRUE);
      GetSsidText(szSsidText, ssidList[ix]);
      
      // First Test
      NDTLogMsg(_T("Variation 1: Querying OID_802_11_CONFIGURATION\n"));
      dwSize = sizeof(NDIS_802_11_CONFIGURATION);
      hr = NDTWlanGetConfiguration(hAdapter, &nConfig, &dwSize);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Failed to query OID_802_11_CONFIGURATION Error:0x%x\n"),hr);
         FlagError(ErrorSevere,&rc);
         continue;
      }
      else
         NDTLogDbg(_T("PASS: Succesfully queried  OID_802_11_CONFIGURATION"));

      // Second Test
      NDTLogMsg(_T("Variation 2: Verify OID_802_11_CONFIGURATION can be set\n"));
      NDTLogMsg(_T("This test will set OID_802_11_CONFIGURATION and verify the request succeeds\n"));

      nConfig.BeaconPeriod = 100;
      dwSize = sizeof(NDIS_802_11_CONFIGURATION);
      hr = NDTWlanSetConfiguration(hAdapter, &nConfig, dwSize);      
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Failed to set OID_802_11_CONFIGURATION, Driver must support setting"));
         NDTLogErr(_T("OID_802_11_CONFIGURATION Error:0x%x\n"),hr);
         FlagError(ErrorSevere,&rc);
      }
      else
         NDTLogDbg(_T("PASS: Succesfully set OID_802_11_CONFIGURATION"));

      // Third Test
       NDTLogMsg(_T("Variation 3: Query OID_802_11_CONFIGURATION and verify fields\n"));
       NDTLogMsg(_T("This test will query OID_802_11_CONFIGURATION and verify the data in the structure returned\n"));

      dwEncryption = Ndis802_11WEPEnabled;
      ulKeyLength = 10;
      ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
      hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen,
         &dwEncryption, ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP,NULL, ssidList[ix], FALSE,&ulTimeout);
      if (FAILED(hr ))
      {
         GetSsidText(szSsidText, ssidList[ix]);
         NDTLogErr(_T("Failed to associate with %s Error = 0x%x\n"),szSsidText, hr);
         FlagError(ErrorSevere,&rc);
         continue;
      }

      hr = NDTWlanGetNetworkTypeInUse(hAdapter, &dwNetworkTypeInUse);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_IN_USE Error : 0x%x\n"), NDIS_FROM_HRESULT(hr));
         FlagError(ErrorSevere,&rc);
         continue;
      }

      dwSize = sizeof(NDIS_802_11_CONFIGURATION);
      hr = NDTWlanGetConfiguration(hAdapter, &nConfig, &dwSize);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Failed to query OID_802_11_CONFIGURATION Error : 0x%x\n"), NDIS_FROM_HRESULT(hr));
         FlagError(ErrorSevere,&rc);
         continue;
      }

      if (sizeof(nConfig) != nConfig.Length)
      {
         NDTLogErr(_T("Length field %u was not equal to sizeof(NDIS_802_11_CONFIGURATION) %d\n"),nConfig.Length,
           sizeof(NDIS_802_11_CONFIGURATION));
         FlagError(ErrorSevere,&rc);
      }
      else
         NDTLogDbg(_T("PASS: Length field correct"));

       if (0 == nConfig.BeaconPeriod)
      {
         NDTLogErr(_T("BeaconPeriod field must not be zero when associated\n"));
         FlagError(ErrorSevere,&rc);
      }
      else
         NDTLogDbg(_T("PASS: BeaconPeriod non zero"));

      if (0> nConfig.ATIMWindow || nConfig.ATIMWindow > 65535)
      {
         NDTLogErr(_T("ATIMWindow field should be between 1 and 65535 (Actual value %u)\n"),nConfig.ATIMWindow );
         FlagError(ErrorSevere,&rc);      
      }
      else
         NDTLogDbg(_T("PASS: ATIMWindow field correct : Value %d"),nConfig.ATIMWindow);

      NDTLogDbg(_T("DSCOnfig (Returned %u)\n"),nConfig.DSConfig);
      switch (dwNetworkTypeInUse)
      {
      case Ndis802_11DS :
         if (nConfig.DSConfig < 2412000 || nConfig.DSConfig > 2484000)
         {
            NDTLogErr(_T("DSConfig should be between 2412000 and 2484000 for network type NDIS802_11DS\n"));
            FlagError(ErrorSevere,&rc);                 
         }
         break;
         
      case Ndis802_11OFDM5 :
         if (nConfig.DSConfig < 5000000|| nConfig.DSConfig > 6000000)
         {
            NDTLogErr(_T("DSConfig should be between 5000000 and 6000000 for network type NDIS802_11DS\n"));
            FlagError(ErrorSevere,&rc);
         }
         break;
         
      case Ndis802_11OFDM24:         
         if (nConfig.DSConfig < 2412000 || nConfig.DSConfig > 2484000)
         {
            NDTLogErr(_T("DSConfig should be between 2412000 and 2484000 for network type NDIS802_11OFDM24\n"));
            FlagError(ErrorSevere,&rc);
         }
         break;
      }
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

