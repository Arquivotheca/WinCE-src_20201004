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
#include <ndis.h>

//------------------------------------------------------------------------------

TEST_FUNCTION(TestWlanBssid)
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
   UINT  iy = 0;
   UINT  iz = 0;
   DWORD nValue = 0;

   UINT iScanCount= 3;
   BYTE arrbMacAddress[6] ={0x0,0x0,0x0,0x0,0x0,0x0};   
   const BYTE arrbEmptyMacAddress[6] ={0x0,0x0,0x0,0x0,0x0,0x0};   
   NDIS_802_11_MAC_ADDRESS bMac;
   
   HANDLE hStatus = NULL;
   ULONG ulKeyIndex;
   ULONG ulKeyLength;
   ULONG ulTimeout;
   NDIS_802_11_SSID ssidList[3];
   DWORD dwSsidCount;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;
   DWORD dwEncryption;
   
   

   NDTLogMsg(
      _T("Start 1c_wlan_bssid on the adapter %s\n"), g_szTestAdapter
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
      

   NDTLogMsg(_T("Variation 1: Verify OID_802_11_BSSID return value while associated,\n"));
   NDTLogMsg(_T(" This test will associate the device and verify it returns the MAC address of the AP the device is associated with\n"));
   
   // Execute test by ssid
   for (ix= 0; ix<dwSsidCount; ix++)
   {
      TCHAR szSsidText[33];

      GetSsidText(szSsidText, ssidList[ix]);
      NDTLogMsg(_T("Testing Association with ssid %s\n"),szSsidText);

      NDTWlanReset(hAdapter, TRUE);

      do
      {
         
         for (iy=0; iy<= iScanCount; iy++)
         {
            hr = NDTWlanSetBssidListScan(hAdapter);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to set OID_802_11_BSSID_LIST_SCAN Error =0x%x\n"), hr);
               FlagError(ErrorSevere,&rc);
               break;
            }

            Sleep (WLAN_BSSID_LIST_SCAN_TIMEOUT);

            DWORD dwSize = 2048;            
            PNDIS_802_11_BSSID_LIST_EX pnBSSIDListEx = (PNDIS_802_11_BSSID_LIST_EX) malloc(dwSize);
            BOOL bGotBssidList = FALSE;
            UINT retries = 20;
            do 
            {
               hr = NDTWlanGetBssidList(hAdapter, pnBSSIDListEx, &dwSize);
               
               if (NOT_SUCCEEDED(hr) && (hr == NDT_STATUS_BUFFER_TOO_SHORT))
               {
                  if (dwSize < 104)
                  {
                     NDTLogErr(_T("Invalid number of bytes required returned %d expected > 104"), dwSize);
                     FlagError(ErrorSevere,&rc);
                     break;
                  }                    
                  pnBSSIDListEx = (PNDIS_802_11_BSSID_LIST_EX) realloc(pnBSSIDListEx,dwSize);                  
               }
               else if (NOT_SUCCEEDED(hr))
               {
                     NDTLogErr(_T("Failed to get OID_802_11_BSSID_LIST_SCAN Error =0x%x\n"), hr);
                     FlagError(ErrorSevere,&rc);
                     break;
                }
               else if  (!NOT_SUCCEEDED(hr))
               {
                  NDTLogVbs(_T("Sucessfully got BSSID list \n"));
                  bGotBssidList = TRUE;
                  break;
               }                  
            }            
            while(--retries >0);
            if (retries <= 0)
            {
               NDTLogErr(_T("Failed to get OID_802_11_BSSID_LIST_SCAN after multiple attempts Error =0x%x\n"), hr);        
               FlagError(ErrorSevere,&rc);
               break;
            }

            PNDIS_WLAN_BSSID_EX pnWlanBssidEx= &(pnBSSIDListEx->Bssid[0]);
            for (iz = 0; iz<pnBSSIDListEx->NumberOfItems && bGotBssidList; iz++)
            {
               if (pnWlanBssidEx->Ssid.SsidLength == ssidList[ix].SsidLength
                  && !memcmp((char *)(pnWlanBssidEx->Ssid.Ssid), (char *)(ssidList[ix].Ssid),ssidList[ix].SsidLength))
               {
                  GetSsidText(szSsidText, pnWlanBssidEx->Ssid);
                  NDTLogMsg(_T("Found Ssid = %s\n"),szSsidText);
                  memcpy(arrbMacAddress, pnWlanBssidEx->MacAddress, sizeof(arrbMacAddress));
                  break;
               }
               pnWlanBssidEx = (PNDIS_WLAN_BSSID_EX)((BYTE*)pnWlanBssidEx + (pnWlanBssidEx->Length));;
            }

            // If arrbMacAddress is not empty we found the ssid
            if (0 != memcmp(arrbMacAddress, arrbEmptyMacAddress, sizeof(arrbEmptyMacAddress)))
            {
                break;
            }

            if (pnBSSIDListEx)
               free (pnBSSIDListEx);
            
         }

         // If arrbMacAddress is empty we did not find the ssid
         if (0 == memcmp(arrbMacAddress, arrbEmptyMacAddress, sizeof(arrbEmptyMacAddress)))
         {
            NDTLogErr(_T("We did not find ssid %s in list\n"), szSsidText);
            FlagError(ErrorSevere,&rc);
         }
         
         ulKeyIndex = 0x80000000;
         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, ulKeyIndex, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&ulTimeout);
         
         if (NOT_SUCCEEDED(hr))
         {
            GetSsidText(szSsidText, ssidList[ix]);
            NDTLogErr(_T("Failed to associate with %s Error=0x%x\n"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

         hr = NDTWlanGetBssid( hAdapter, &bMac);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to query BSSID Error = 0x%x\n"), hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

         TCHAR czBssidText[18];
         if (0 != memcmp(bMac, arrbMacAddress, sizeof(arrbMacAddress)))
         {
            NDTLogErr(_T("Failed Test : MAC address returned does not match MAC address in NDIS_WLAN_BSSID_EX->MacAddress\n"));
            NDTLogErr(_T("Driver must return the MAC address of the AP the device is currently associated with\n"));
            GetBssidText(czBssidText,bMac);
            NDTLogMsg(_T("\nActual mac : %s\t\n"),czBssidText);
            GetBssidText(czBssidText,arrbMacAddress);
            NDTLogMsg(_T("Expected mac : %s\n"),arrbMacAddress);
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
         {
            GetBssidText(czBssidText,bMac);
            NDTLogDbg(_T("PASS: MAC returned %s matches MAC address in BssidEx field"),czBssidText);
         }
      }
      while(0);

      NDTLogMsg(_T("\nVariation 2: Verify OID_802_11_BSSID return value while disassociated \n"));
      NDTLogMsg(_T("This test will verify NDIS_STATUS_ADAPTER_NOT_READY is returned when the device is not associated\n"));

     hr = NDTStatus(hAdapter, NDIS_STATUS_MEDIA_DISCONNECT, &hStatus);
     if (FAILED(hr))
     {
        NDTLogErr(_T("Starting wait for disconnect on %s failed Error=0x%x\n"),g_szTestAdapter,hr);
        FlagError(ErrorSevere,&rc);
        continue;
     }        
      
      
      NDTWlanDisassociate(hAdapter);

     // Expect DISCONNECT notification in 10 seconds
     hr = NDTStatusWait(hAdapter, hStatus, 10000);
     if (NOT_SUCCEEDED(hr)) {
        NDTLogErr(_T("No disconnect event indicated  Error : 0x%x\n"), NDIS_FROM_HRESULT(hr));
        NDTLogMsg(_T("Miniport adapter %s failed to Disconnect to %s\n"), g_szTestAdapter, szSsidText);
        FlagError(ErrorSevere,&rc);
        continue;
     }      

      memset(bMac, 0, sizeof(bMac));
      hr = NDTWlanGetBssid(hAdapter, &bMac);
      if (NDT_STATUS_ADAPTER_NOT_READY != hr)
      {
         NDTLogMsg(_T("Should have returned NDIS_STATUS_ADAPTER_NOT_READY (Returned: 0x%x\n"), NDIS_FROM_HRESULT(hr));
         NDTLogMsg(_T("Driver must return NDIS_STATUS_ADAPTER_NOT_READY when OID_802_11_BSSID is queried while disassociated\n"));
         FlagError(ErrorSevere,&rc);
         continue;
       }   
      else
      {
         NDTLogDbg(_T("PASS: Driver returned NDIS_STATUS_ADAPTER_NOT_READY"));
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

