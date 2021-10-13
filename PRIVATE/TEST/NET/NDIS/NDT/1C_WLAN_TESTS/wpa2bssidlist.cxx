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

TEST_FUNCTION(TestWpa2Bssidlist)
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
   NDIS_802_11_SSID ssidList[3];
   DWORD dwSsidCount;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;

   BOOL bWPA2Supported = FALSE;

   const UINT iCount = 5;
   const UINT iScanCount = 10 ;

   BOOL bSsidFound = FALSE;
   PNDIS_WLAN_BSSID_EX pnWLanBssidEx=NULL;
   TCHAR szBssidExText[80];                                       
                
   
   

   NDTLogMsg(
      _T("Start 1c_wpa2_bssidlist test on the adapter %s\n"), g_szTestAdapter
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
       
      //First Variation
      NDTLogMsg(_T("Variation 1: Query OID_802_11_BSSID_LIST and verify RSN IE is returned\n"));
      NDTLogMsg(_T("This test will query OID_802_11_BSSID_LIST and verify the"));
      NDTLogMsg(_T("RSN IE in the WPA2 Access Point BSSID\n"));

      do
      {                      
         PNDIS_WLAN_BSSID_EX  pnBssidListExCopy= NULL;  

         hr = NDTWlanSetBssidListScan(hAdapter);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogMsg(_T("Failed to set OID_802_11_BSSID_LIST_SCAN\n"));
         }

         Sleep(WLAN_BSSID_LIST_SCAN_TIMEOUT);

   
         // Query for BSSID list multiple times
         for (UINT iy=0;  iy<iCount; iy++)
         {              
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

               // Loop through the list of bssid and search for WPA2 bssid
               pnWLanBssidEx= &(pnBSSIDListEx->Bssid[0]);
               for (UINT iz=0; iz<(pnBSSIDListEx->NumberOfItems) && bGotBssidList; iz++)
               {                
                  memset(szBssidExText,_T('\0'),sizeof(szBssidExText));
                  GetBssidExText(szBssidExText, *pnWLanBssidEx);
                  NDTLogDbg(_T("%s\n"),szBssidExText);
                  
                  if (pnWLanBssidEx->Ssid.SsidLength == ssidList[ix].SsidLength && !memcmp(pnWLanBssidEx->Ssid.Ssid,ssidList[ix].Ssid,ssidList[ix].SsidLength))
                  {
                     bSsidFound = TRUE;
                     break;
                  }

                  pnWLanBssidEx = (PNDIS_WLAN_BSSID_EX)((BYTE*)pnWLanBssidEx + (pnWLanBssidEx->Length));
               }

               // If we found the WPA2 bssid, print it and save a copy of the Bssid list ex structure
               if (TRUE == bSsidFound)
               {
                  memset(szBssidExText,_T('\0'),sizeof(szBssidExText));
                  GetBssidExText(szBssidExText, *pnWLanBssidEx);
                  NDTLogMsg(_T("Found WPA2 bssid %s"),szBssidExText); 
                  pnBssidListExCopy = (PNDIS_WLAN_BSSID_EX)malloc(pnWLanBssidEx->Length);
                  memcpy(pnBssidListExCopy,pnWLanBssidEx,pnWLanBssidEx->Length);
                  break;
               }

               if (pnBSSIDListEx)
                  free(pnBSSIDListEx);
         }

         if (FALSE ==bSsidFound)
         {
            NDTLogErr(_T("WPA2 BSSID not found in BSSID list"));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("Found WPA2 Bssid in list"));

         BOOL bFoundRSNIE = FALSE;
         // Loop through all the IEs and verify the RSN IE exists  
         PNDIS_802_11_FIXED_IEs pnFixedIes = (PNDIS_802_11_FIXED_IEs) pnBssidListExCopy->IEs;
         PNDIS_802_11_VARIABLE_IEs pnVariableIes;

         pnVariableIes = (PNDIS_802_11_VARIABLE_IEs) (pnFixedIes + 1);
         while((void *)pnVariableIes< ((void *)&(pnBssidListExCopy->IEs[pnBssidListExCopy->IELength])))
         {
            if (WLAN_IE_RSN == pnVariableIes->ElementID  ) 
            {
              bFoundRSNIE = TRUE;
            }               
            pnVariableIes= (PNDIS_802_11_VARIABLE_IEs)(&(pnVariableIes->data[pnVariableIes->Length]));            
         }

         if (pnBssidListExCopy)
            free (pnBssidListExCopy);
            
         if (FALSE == bFoundRSNIE)
         {
            NDTLogErr(_T("RSN IE not found in WP2 Bssid IE list"));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("Found RSN IE in WPA2 Bssid List"));
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

