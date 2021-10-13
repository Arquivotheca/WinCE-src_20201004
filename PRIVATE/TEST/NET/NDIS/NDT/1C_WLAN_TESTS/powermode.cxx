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

//------------------------------------------------------------------------------

TEST_FUNCTION(TestPowerMode)
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
   NDIS_802_11_SSID ssidList[3];
   DWORD dwSsidCount;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;
   DWORD dwEncryption;
   ULONG  ulTimeout;
   
   

   NDTLogMsg(
      _T("Start 1c_wlan_powermode on the adapter %s\n"), g_szTestAdapter
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
      TCHAR szSsidText[33];
         
      GetSsidText(szSsidText, ssidList[ix]);
      
      // First Variation
      NDTLogMsg(_T("Variation 1: Associate with %s\n"), szSsidText);

      ulKeyLength = 10;
      dwEncryption = Ndis802_11WEPEnabled;
      ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
      hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
              ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);         
      if (NOT_SUCCEEDED(hr))
      {
         GetSsidText(szSsidText, ssidList[ix]);
         NDTLogErr(_T("Failed to associate with %s Error:0x%x\n"),szSsidText,hr);
         FlagError(ErrorSevere,&rc);            
         break;
      }

    	// Second Variation
      
      NDTLogMsg(_T("Variation 2: Set and Query OID_802_11_POWER_MODE and verify support\n"));
      NDTLogMsg(_T(" This test will first set OID_802_11_POWER_MODE then query OID_802_11_POWER_MODE to verify query is supported\n"));

     do
     {
           DWORD adwPowerModes[] = {Ndis802_11PowerModeCAM, Ndis802_11PowerModeMAX_PSP, Ndis802_11PowerModeFast_PSP};
           for (UINT iy=0; iy<(sizeof(adwPowerModes)/sizeof(adwPowerModes[0])); iy++)
           {
              TCHAR szPowerModeText[40];
              GetPowerModeText(szPowerModeText,adwPowerModes[iy]);
              NDTLogMsg(_T("Test setting power mode to %s"), szPowerModeText);
              
              hr = NDTWlanSetPowerMode(hAdapter, &adwPowerModes[iy]);
              if (NOT_SUCCEEDED(hr))
              {
                 NDTLogWrn(_T("Failed to set OID_802_11_POWER_MODE to %s Error0x%x\n"), szPowerModeText,NDIS_FROM_HRESULT(hr));
                 NDTLogWrn(_T("It is recommended by CE that the driver support OID_802_11_POWER_MODE set\n"));         
    		     rc = TPR_SKIP;
                 goto cleanUp;
              }     

              DWORD dwPowerMode;
              hr = NDTWlanGetPowerMode(hAdapter, &dwPowerMode);
              if (NOT_SUCCEEDED(hr))
              {
                 NDTLogWrn(_T("Failed to query OID_802_11_POWER_MODE Error0x%x\n"),NDIS_FROM_HRESULT(hr));
                 NDTLogWrn(_T("It is mandated by CE that the driver support OID_802_11_POWER_MODE query\n"));      
    		     rc = TPR_SKIP;
                 goto cleanUp;
              }         

              if(dwPowerMode != adwPowerModes[iy])
              {
                 // Now this is an error
                 NDTLogErr(_T("Expected power mode to be: %d, getting: %d\n"),adwPowerModes[iy],dwPowerMode);
                 FlagError(ErrorSevere,&rc);
                 goto cleanUp;
              }

           }
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

