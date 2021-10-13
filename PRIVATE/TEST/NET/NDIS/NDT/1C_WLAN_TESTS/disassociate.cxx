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

TEST_FUNCTION(TestWlanDisassociate)
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

   UINT cbUsed = 0;
   UINT cbRequired = 0;


   HANDLE hStatus = NULL;
   ULONG ulKeyLength;
   NDIS_802_11_SSID ssidList[3];
   DWORD dwSsidCount;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;
   DWORD dwEncryption;
   ULONG ulTimeout;
   
   

   NDTLogMsg(
      _T("Start 1c_wlan_disassociate test on the adapter %s\n"), g_szTestAdapter
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
      ULONG ulConnectStatus;

      GetSsidText(szSsidText, ssidList[ix]);

      // First variation
      NDTLogMsg(_T("Variation 1: Set OID_802_11_DISASSOCIATE and verify device disassociates\n"));
      NDTLogMsg(_T("This test will associate with %s, set OID_802_11_DISASSOCIATE \n"),szSsidText);
      NDTLogMsg(_T("and verify the device indicates a disconnect\n"));

      do
      {
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

         hStatus = NULL;
         hr = NDTStatus(hAdapter, NDIS_STATUS_MEDIA_DISCONNECT, &hStatus);
         if (FAILED(hr))
         {
            NDTLogErr(_T("Starting wait for disconnect on %s failed Error=0x%x\n"),g_szTestAdapter,hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

         hr = NDTWlanDisassociate(hAdapter);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_DISASSOCIATE, \n"));
            NDTLogErr(_T("Driver must indicate a media disconnect event after OID_802_11_DISASSOCIATE \n"));
            NDTLogErr(_T("is set Error : 0x%x\n"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

         // Expect DISCONNECT notification in 10 seconds
         hr = NDTStatusWait(hAdapter, hStatus, 10000);
         if (NOT_SUCCEEDED(hr)) {
            NDTLogErr(_T("No disconnect event indicated  Error : 0x%x\n"), NDIS_FROM_HRESULT(hr));
            NDTLogMsg(_T("Miniport adapter %s failed to connect to %s\n"), g_szTestAdapter, szSsidText);
            FlagError(ErrorSevere,&rc);
            break;
         }
      
         ulConnectStatus = 0;
         cbUsed = 0;
         cbRequired = 0;
         hr = NDTQueryInfo(hAdapter, OID_GEN_MEDIA_CONNECT_STATUS, &ulConnectStatus,
                  sizeof(ulConnectStatus),&cbUsed,&cbRequired);
         if (NOT_SUCCEEDED(hr)) 
         {
            NDTLogErr(_T("Unable to query OID_GEN_MEDIA_CONNECT_STATUS with hr=0x%08x\n"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

         if (ulConnectStatus == NdisMediaStateConnected)
        {
            NDTLogErr(_T("Media connect status should have been disconnect. \n"));
            NDTLogErr(_T("Driver must set the media connect status to disconnect when the device is not associated\n"));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("PASS: Media state disconnected"));
      }
      while(0);
      
      // Second variation
      NDTLogMsg(_T("Variation 2: SetOID_802_11_DISASSOCIATE while the test device is already"));
      NDTLogMsg(_T("disassociated and verify NDIS_STATUS_SUCCESS is returned\n"));

      do
      {
         ulConnectStatus = 0;
         cbUsed = 0;
         cbRequired = 0;
         hr = NDTQueryInfo(hAdapter, OID_GEN_MEDIA_CONNECT_STATUS, &ulConnectStatus,
                  sizeof(ulConnectStatus),&cbUsed,&cbRequired);
         if (FAILED(hr)) 
         {
            NDTLogErr(_T("Unable to query OID_GEN_MEDIA_CONNECT_STATUS with hr=0x%08x\n"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

         
         if (ulConnectStatus == NdisMediaStateConnected)
        {
            hr = NDTWlanDisassociate(hAdapter);
            if (FAILED(hr))
            {
               NDTLogErr(_T("Failed to set OID_802_11_DISASSOCIATE\n"),hr);
               FlagError(ErrorSevere,&rc);
               break;
            }
         }

         hr = NDTWlanDisassociate(hAdapter);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_DISASSOCIATE, \n"));
            NDTLogErr(_T("Driver must indicate a media disconnect event after OID_802_11_DISASSOCIATE \n"));
            NDTLogErr(_T("is set Error : 0x%x\n"),hr);
            FlagError(ErrorMild,&rc);
            break;
         }
         else
            NDTLogDbg(_T("PASS: NDIS_STATUS_SUCCESS returned"));
      }
      while(0);
      

      // Third variation
        NDTLogMsg(_T("Variation 3: Set OID_802_11_DISASSOCIATE while a scanning, This test will associate with %s"),szSsidText);
        NDTLogMsg(_T("start a list scan and then set OID_802_11_DISASSOCIATE,  it will verify the device disconnects\n"));

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
         continue;
      }

      // Start scanning
      hr = NDTWlanSetBssidListScan(hAdapter);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Failed to set OID_802_11_BSSID_LIST_SCAN Error : 0x%x\n"),hr);
         FlagError(ErrorSevere,&rc);
         continue;
      }

      // Disconnect and check status notification within 10 sec
      hStatus = NULL;
      hr = NDTStatus(hAdapter, NDIS_STATUS_MEDIA_DISCONNECT, &hStatus);
      if (FAILED(hr))
      {
         NDTLogErr(_T("Starting wait for disconnect on %s failed Error : 0x%x\n"),g_szTestAdapter,hr);
         FlagError(ErrorSevere,&rc);
         continue;
      }

      hr = NDTWlanDisassociate(hAdapter);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Failed to set OID_802_11_DISASSOCIATE,"));
         NDTLogErr(_T("Driver must indicate a media disconnect event after OID_802_11_DISASSOCIATE"));
         NDTLogErr(_T("is set Error : 0x%x\n"),NDIS_FROM_HRESULT(hr));
         FlagError(ErrorSevere,&rc);
         continue;
      }

      // Expect DISCONNECT notification in 10 seconds
      hr = NDTStatusWait(hAdapter, hStatus, 10000);
      if (NOT_SUCCEEDED(hr)) {
         NDTLogErr(_T("No disconnect event indicated  Error : 0x%x\n"), NDIS_FROM_HRESULT(hr));
         NDTLogMsg(_T("Miniport adapter %s failed to connect to %s\n"), g_szTestAdapter, szSsidText);
         FlagError(ErrorSevere,&rc);
         continue;
      }
   
      ulConnectStatus = 0;
      cbUsed = 0;
      cbRequired = 0;
      hr = NDTQueryInfo(hAdapter, OID_GEN_MEDIA_CONNECT_STATUS, &ulConnectStatus,
               sizeof(ulConnectStatus),&cbUsed,&cbRequired);
      if (NOT_SUCCEEDED(hr)) 
      {
         NDTLogErr(_T("Unable to query OID_GEN_MEDIA_CONNECT_STATUS with hr=0x%08x\n"),hr);
         FlagError(ErrorSevere,&rc);
         continue;
      }

      if (ulConnectStatus == NdisMediaStateConnected)
     {
         hr=NDT_STATUS_FAILURE;
         NDTLogErr(_T("Media connect status should have been disconnect. \n"));
          NDTLogErr(_T("Driver must set the media connect status to disconnect when the device is not associated\n"));
         FlagError(ErrorSevere,&rc);
         continue;
      } 
      else
          NDTLogDbg(_T("PASS: Media state disconnected"));
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

