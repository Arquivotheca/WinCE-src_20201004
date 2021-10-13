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

TEST_FUNCTION(TestWpaNetworkTypesSupported)
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

   BOOL bWPASupported;
   
   

   NDTLogMsg(
      _T("Start 1c_wpa_networktypessupported on the adapter %s\n"), g_szTestAdapter
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

   hr = NDTWlanIsWPASupported(hAdapter, &bWPASupported);
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
      NDTLogMsg(_T("Variation 1: Verify OID_802_11_NETWORK_TYPES_SUPPORTED retuns at least one type\n"));
      NDTLogMsg(_T("This test will query OID_802_11_NETWORK_TYPE_SUPPORTED and verify the correct types are returned\n"));

      do 
      {
         DWORD dwBufSize;                
         PVOID pvNetworkTypesInUse;

         pvNetworkTypesInUse = malloc(sizeof(NDIS_802_11_NETWORK_TYPE_LIST));
         dwBufSize = sizeof(NDIS_802_11_NETWORK_TYPE_LIST);
         hr = NDTWlanGetNetworkTypesSuppported(hAdapter, pvNetworkTypesInUse, &dwBufSize);
         if (NOT_SUCCEEDED(hr) && NDT_STATUS_BUFFER_TOO_SHORT != hr)
         {
            NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_SUPPORTED Error:0x%x\n"), NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else if (NOT_SUCCEEDED(hr))
         {
            pvNetworkTypesInUse = realloc(pvNetworkTypesInUse, dwBufSize);
            hr = NDTWlanGetNetworkTypesSuppported(hAdapter, pvNetworkTypesInUse, &dwBufSize);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_SUPPORTED after reallocating buffer Error:0x%x\n"), NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }           
         }

         PNDIS_802_11_NETWORK_TYPE_LIST pnNetworkTypeList = (PNDIS_802_11_NETWORK_TYPE_LIST) pvNetworkTypesInUse;
         if (0 == pnNetworkTypeList->NumberOfItems)
         {
            NDTLogErr(_T("Should have returned at least one supported network type\n"));
            NDTLogErr(_T("Driver must return at least Ndis802_11DS in the supported types list \n"));
            FlagError(ErrorSevere,&rc);            
         }
         else
            NDTLogDbg(_T("PASS: Returned atleast 1 network type"));


         if (pvNetworkTypesInUse)
            free(pvNetworkTypesInUse);
      }
      while(0);

      //If this is an 802.11a device, then verify Only OFDM5 is returned in the supported list
      if (WLAN_DEVICE_TYPE_802_11A == WlanDeviceType)
      {
         // Second Variation
      
         NDTLogMsg(_T("Variation 2: Verify OID_802_11_NETWORK_TYPES_SUPPORTED returns only Ndis802_11OFDM5\n"));
         NDTLogMsg(_T(" This test will query OID_802_11_NETWORK_TYPE_SUPPORTED and since this is an 802.11a device "));
         NDTLogMsg(_T("It will verify only Ndis802_11OFDM5 is returned in the list\n"));
         do
         {
            BOOL abSupportedTypes[3] = {FALSE, FALSE, FALSE};
            DWORD dwBufSize;                
            PVOID pvNetworkTypesInUse;

            pvNetworkTypesInUse = malloc(sizeof(NDIS_802_11_NETWORK_TYPE_LIST));
            dwBufSize = sizeof(NDIS_802_11_NETWORK_TYPE_LIST);
            hr = NDTWlanGetNetworkTypesSuppported(hAdapter, pvNetworkTypesInUse, &dwBufSize);
            if (NOT_SUCCEEDED(hr) && NDT_STATUS_BUFFER_TOO_SHORT != hr)
            {
               NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_SUPPORTED Error:0x%x\n"), NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }
            else if (NOT_SUCCEEDED(hr))
            {
               pvNetworkTypesInUse = realloc(pvNetworkTypesInUse, dwBufSize);
               hr = NDTWlanGetNetworkTypesSuppported(hAdapter, pvNetworkTypesInUse, &dwBufSize);
               if (NOT_SUCCEEDED(hr))
               {
                  NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_SUPPORTED after reallocating buffer Error:0x%x\n"), NDIS_FROM_HRESULT(hr));
                  FlagError(ErrorSevere,&rc);
                  break;
               }           
            }

            PNDIS_802_11_NETWORK_TYPE_LIST pnNetworkTypeList = (PNDIS_802_11_NETWORK_TYPE_LIST) pvNetworkTypesInUse;
            if (1 != pnNetworkTypeList->NumberOfItems)
            {
               NDTLogErr(_T("NumberOfItems should have been 1 (Returned: %d\n"), pnNetworkTypeList->NumberOfItems);
               NDTLogErr(_T("Driver must set NumberOfItems to 1 for an 802.11a device\n"));
               FlagError(ErrorSevere,&rc);            
            }

            for (UINT iy=0; iy<pnNetworkTypeList->NumberOfItems; iy++)               
            {
               switch(pnNetworkTypeList->NetworkType[iy])
               {
                  case Ndis802_11OFDM5:
                     NDTLogDbg(_T("Ndis802_11OFDM5 found"));                     
                     abSupportedTypes[0]= TRUE;
                     break;
                  case Ndis802_11DS:
                     NDTLogDbg(_T("Ndis802_11DS found"));                     
                     abSupportedTypes[1]= TRUE;
                     break;
                  case Ndis802_11OFDM24:
                     NDTLogDbg(_T("Ndis802_11OFDM24 found"));                     
                     abSupportedTypes[2]= TRUE;
                     break;                     
               }               
            }

            if (FALSE == abSupportedTypes[0])
            {
               NDTLogErr(_T("Ndis802_11OFDM5 not found in supported list\n"));
               NDTLogErr(_T("Device is an 802.11a device so the driver must return Ndis802_11OFDM5 in the supported list\n"));
               FlagError(ErrorSevere,&rc);               
            }
            if (TRUE == abSupportedTypes[1])
            {
               NDTLogErr(_T("Ndis802_11DS should not have been returned in supported list\n"));
               NDTLogErr(_T("Device is an 802.11a device so the driver must not return Ndis802_11DS in the supported list\n"));
               FlagError(ErrorSevere,&rc);               
            }
            if (TRUE == abSupportedTypes[2])
            {
               NDTLogErr(_T("Ndis802_11OFDM24 should not have been returned in supported list\n"));
               NDTLogErr(_T("Device is an 802.11a device so the driver must not return Ndis802_11OFDM24 in the supported list\n"));
               FlagError(ErrorSevere,&rc);               
            }         

            if (pvNetworkTypesInUse)
               free(pvNetworkTypesInUse);
         }
         while(0);      
      }

      //If this is an 802.11b device, then verify Only DSSS is returned in the supported list
      if (WLAN_DEVICE_TYPE_802_11B == WlanDeviceType)
      {
         // Third Variation
      
         NDTLogMsg(_T("Variation 3: Verify OID_802_11_NETWORK_TYPES_SUPPORTED returns only Ndis802_11DS\n"));
         NDTLogMsg(_T(" This test will query OID_802_11_NETWORK_TYPE_SUPPORTED and since this is an 802.11b device it will verify only Ndis802_11DS is returned in the list\n"));

         do
         {
            BOOL abSupportedTypes[3] = {FALSE, FALSE, FALSE};
            DWORD dwBufSize;                
            PVOID pvNetworkTypesInUse;

            pvNetworkTypesInUse = malloc(sizeof(NDIS_802_11_NETWORK_TYPE_LIST));
            dwBufSize = sizeof(NDIS_802_11_NETWORK_TYPE_LIST);
            hr = NDTWlanGetNetworkTypesSuppported(hAdapter, pvNetworkTypesInUse, &dwBufSize);
            if (NOT_SUCCEEDED(hr) && NDT_STATUS_BUFFER_TOO_SHORT != hr)
            {
               NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_SUPPORTED Error:0x%x\n"), NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }
            else if (NOT_SUCCEEDED(hr))
            {
               pvNetworkTypesInUse = realloc(pvNetworkTypesInUse, dwBufSize);
               hr = NDTWlanGetNetworkTypesSuppported(hAdapter, pvNetworkTypesInUse, &dwBufSize);
               if (NOT_SUCCEEDED(hr))
               {
                  NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_SUPPORTED after reallocating buffer Error:0x%x\n"), NDIS_FROM_HRESULT(hr));
                  FlagError(ErrorSevere,&rc);
                  break;
               }           
            }

            PNDIS_802_11_NETWORK_TYPE_LIST pnNetworkTypeList = (PNDIS_802_11_NETWORK_TYPE_LIST) pvNetworkTypesInUse;
            if (1 != pnNetworkTypeList->NumberOfItems)
            {
               NDTLogErr(_T("NumberOfItems should have been 1 (Returned: %d\n"),pnNetworkTypeList->NumberOfItems);
               NDTLogErr(_T("Driver must set NumberOfItems to 1 for an 802.11b device\n"));
               FlagError(ErrorSevere,&rc);            
            }

            for (UINT iy=0; iy<pnNetworkTypeList->NumberOfItems; iy++)               
            {
               switch(pnNetworkTypeList->NetworkType[iy])
               {
               case Ndis802_11OFDM5:
                  NDTLogDbg(_T("Ndis802_11OFDM5 found"));                     
                  abSupportedTypes[0]= TRUE;
                  break;
               case Ndis802_11DS:
                  NDTLogDbg(_T("Ndis802_11DS found"));                     
                  abSupportedTypes[1]= TRUE;
                  break;
               case Ndis802_11OFDM24:
                  NDTLogDbg(_T("Ndis802_11OFDM24 found"));                     
                  abSupportedTypes[2]= TRUE;
                  break;                 
               }               
            }

            if (TRUE == abSupportedTypes[0])
            {
               NDTLogErr(_T("Ndis802_11OFDM5 should not have been returned in supported list\n"));
               NDTLogErr(_T("Device is an 802.11b device so the driver must not return Ndis802_11OFDM5 in the supported list\n"));
               FlagError(ErrorSevere,&rc);               
            }
            if (FALSE == abSupportedTypes[1])
            {
               NDTLogErr(_T("Ndis802_11DS not found in supported list\n"));
               NDTLogErr(_T("Device is an 802.11b device so the driver must return Ndis802_11DS in the supported list\n"));
               FlagError(ErrorSevere,&rc);               
            }
            if (TRUE == abSupportedTypes[2])
            {
               NDTLogErr(_T("Ndis802_11OFDM24 should not have been returned in supported list\n"));
               NDTLogErr(_T("Device is an 802.11b device so the driver must not return Ndis802_11OFDM24 in the supported list\n"));
               FlagError(ErrorSevere,&rc);               
            }                  
            if (pvNetworkTypesInUse)
               free(pvNetworkTypesInUse);
         }
         while(0);      
      }

      //If this is an 802.11g device, then verify DSSS and OFDM24 are returned in the supported list
      if (WLAN_DEVICE_TYPE_802_11G == WlanDeviceType)
      {
         // Fourth Variation
      
         NDTLogMsg(_T("Variation 4: Verify OID_802_11_NETWORK_TYPES_SUPPORTED returns Ndis802_11DS and Ndis802_11OFDM24\n"));
         NDTLogMsg(_T(" This test will query OID_802_11_NETWORK_TYPE_SUPPORTED and since this is an 802.11g device it will verify \n"));
         NDTLogMsg(_T(" both Ndis802_11DS and Ndis802_11OFD24 are returned in the list\n"));

         do
         {
            BOOL abSupportedTypes[3] = {FALSE, FALSE, FALSE};
            DWORD dwBufSize;                
            PVOID pvNetworkTypesInUse;

            pvNetworkTypesInUse = malloc(sizeof(NDIS_802_11_NETWORK_TYPE_LIST));
            dwBufSize = sizeof(NDIS_802_11_NETWORK_TYPE_LIST);
            hr = NDTWlanGetNetworkTypesSuppported(hAdapter, pvNetworkTypesInUse, &dwBufSize);
            if (NOT_SUCCEEDED(hr) && NDT_STATUS_BUFFER_TOO_SHORT != hr)
            {
               NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_SUPPORTED Error:0x%x\n"), NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }
            else if (NOT_SUCCEEDED(hr))
            {
               pvNetworkTypesInUse = realloc(pvNetworkTypesInUse, dwBufSize);
               hr = NDTWlanGetNetworkTypesSuppported(hAdapter, pvNetworkTypesInUse, &dwBufSize);
               if (NOT_SUCCEEDED(hr))
               {
                  NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_SUPPORTED after reallocating buffer Error:0x%x\n"), NDIS_FROM_HRESULT(hr));
                  FlagError(ErrorSevere,&rc);
                  break;
               }           
            }

            PNDIS_802_11_NETWORK_TYPE_LIST pnNetworkTypeList = (PNDIS_802_11_NETWORK_TYPE_LIST) pvNetworkTypesInUse;
            if (2 != pnNetworkTypeList->NumberOfItems)
            {
               NDTLogErr(_T("NumberOfItems should have been 2 (Returned: %d\n"),pnNetworkTypeList->NumberOfItems);
               NDTLogErr(_T("Driver must set NumberOfItems to 2 for an 802.11g device\n"));
               FlagError(ErrorSevere,&rc);            
            }

            for (UINT iy=0; iy<pnNetworkTypeList->NumberOfItems; iy++)               
            {
               switch(pnNetworkTypeList->NetworkType[iy])
               {
                  case Ndis802_11OFDM5:
                     NDTLogDbg(_T("Ndis802_11OFDM5 found"));                     
                     abSupportedTypes[0]= TRUE;
                     break;
                  case Ndis802_11DS:
                     NDTLogDbg(_T("Ndis802_11DS found"));                     
                     abSupportedTypes[1]= TRUE;
                     break;
                  case Ndis802_11OFDM24:
                     NDTLogDbg(_T("Ndis802_11OFDM24 found"));                     
                     abSupportedTypes[2]= TRUE;
                     break;                            
               }               
            }

            if (TRUE == abSupportedTypes[0])
            {
               NDTLogErr(_T("Ndis802_11OFDM5 should not have been returned in supported list\n"));
               NDTLogErr(_T("Device is an 802.11g device so the driver must not return Ndis802_11OFDM5 in the supported list\n"));
               FlagError(ErrorSevere,&rc);               
            }
            if (FALSE == abSupportedTypes[1])
            {
               NDTLogErr(_T("Ndis802_11DS not found in supported list\n"));
               NDTLogErr(_T("Device is an 802.11g device so the driver must return Ndis802_11DS in the supported list\n"));
               FlagError(ErrorSevere,&rc);               
            }
            if (FALSE == abSupportedTypes[2])
            {
               NDTLogErr(_T("Ndis802_11OFDM24 not found in supported list\n"));
               NDTLogErr(_T("Device is an 802.11g device so the driver must return Ndis802_11OFDM24 in the supported list\n"));
               FlagError(ErrorSevere,&rc);               
            }         
            if (pvNetworkTypesInUse)
               free(pvNetworkTypesInUse);            
         }
         while(0);      
      }
     
      //If this is an 802.11a/b device, then verify DSSS and OFDM5 are returned in the supported list
      if (WLAN_DEVICE_TYPE_802_11AB == WlanDeviceType)
      {
         // Fifth Variation
      
         NDTLogMsg(_T("Variation 5: Verify OID_802_11_NETWORK_TYPES_SUPPORTED returns Ndis802_11DS and Ndis802_11OFDM5\n"));
         NDTLogMsg(_T(" This test will query OID_802_11_NETWORK_TYPE_SUPPORTED and since this is an 802.11a/b device it will verify \n"));
         NDTLogMsg(_T(" both Ndis802_11DS and Ndis802_11OFD5 are returned in the list\n"));

         do
         {
            BOOL abSupportedTypes[3] = {FALSE, FALSE, FALSE};
            DWORD dwBufSize;                
            PVOID pvNetworkTypesInUse;

            pvNetworkTypesInUse = malloc(sizeof(NDIS_802_11_NETWORK_TYPE_LIST));
            dwBufSize = sizeof(NDIS_802_11_NETWORK_TYPE_LIST);
            hr = NDTWlanGetNetworkTypesSuppported(hAdapter, pvNetworkTypesInUse, &dwBufSize);
            if (NOT_SUCCEEDED(hr) && NDT_STATUS_BUFFER_TOO_SHORT != hr)
            {
               NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_SUPPORTED Error:0x%x\n"), NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }
            else if (NOT_SUCCEEDED(hr))
            {
               pvNetworkTypesInUse = realloc(pvNetworkTypesInUse, dwBufSize);
               hr = NDTWlanGetNetworkTypesSuppported(hAdapter, pvNetworkTypesInUse, &dwBufSize);
               if (NOT_SUCCEEDED(hr))
               {
                  NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_SUPPORTED after reallocating buffer Error:0x%x\n"), NDIS_FROM_HRESULT(hr));
                  FlagError(ErrorSevere,&rc);
                  break;
               }           
            }

            PNDIS_802_11_NETWORK_TYPE_LIST pnNetworkTypeList = (PNDIS_802_11_NETWORK_TYPE_LIST) pvNetworkTypesInUse;
            if (2 != pnNetworkTypeList->NumberOfItems)
            {
               NDTLogErr(_T("NumberOfItems should have been 2 (Returned: %d\n"),pnNetworkTypeList->NumberOfItems);
               NDTLogErr(_T("Driver must set NumberOfItems to 2 for an 802.11a/b device\n"));
               FlagError(ErrorSevere,&rc);            
            }

            for (UINT iy=0; iy<pnNetworkTypeList->NumberOfItems; iy++)               
            {
               switch(pnNetworkTypeList->NetworkType[iy])
               {
                   case Ndis802_11OFDM5:
                     NDTLogDbg(_T("Ndis802_11OFDM5 found"));                     
                     abSupportedTypes[0]= TRUE;
                     break;
                  case Ndis802_11DS:
                     NDTLogDbg(_T("Ndis802_11DS found"));                     
                     abSupportedTypes[1]= TRUE;
                     break;
                  case Ndis802_11OFDM24:
                     NDTLogDbg(_T("Ndis802_11OFDM24 found"));                     
                     abSupportedTypes[2]= TRUE;
                     break;                    
               }               
            }

            if (FALSE == abSupportedTypes[0])
            {
               NDTLogErr(_T("Ndis802_11OFDM5 not found in supported list\n"));
               NDTLogErr(_T("Device is an 802.11a/b device so the driver must return Ndis802_11OFDM5 in the supported list\n"));
               FlagError(ErrorSevere,&rc);               
            }
            if (FALSE == abSupportedTypes[1])
            {
               NDTLogErr(_T("Ndis802_11DS not found in supported list\n"));
               NDTLogErr(_T("Device is an 802.11a/b device so the driver must return Ndis802_11DS in the supported list\n"));
               FlagError(ErrorSevere,&rc);               
            }
            if (TRUE == abSupportedTypes[2])
            {
               NDTLogErr(_T("Ndis802_11OFDM24 should not have been returned in supported list\n"));
               NDTLogErr(_T("Device is an 802.11a/b device so the driver must not return Ndis802_11OFDM24 in the supported list\n"));
               FlagError(ErrorSevere,&rc);               
            }         
         if (pvNetworkTypesInUse)
            free(pvNetworkTypesInUse);
         }
         while(0);      
      }

      //If this is an 802.11a/g device, then verify DSSS, OFDM5 and OFDM24 are returned in the supported list
      if (WLAN_DEVICE_TYPE_802_11AG == WlanDeviceType)
      {
         // Sixth Variation
      
         NDTLogMsg(_T("Variation 6: Verify OID_802_11_NETWORK_TYPES_SUPPORTED returns Ndis802_11DS,Ndis802_11OFDM5 and Ndis802_11OFD24\n"));
         NDTLogMsg(_T(" This test will query OID_802_11_NETWORK_TYPE_SUPPORTED and since this is an 802.11a/g device it will verify \n"));
         NDTLogMsg(_T(" Ndis802_11DS, Ndis802_11OFDM5 and Ndis802_11OFDM24 are returned in the list\n"));

         do
         {
            BOOL abSupportedTypes[3] = {FALSE, FALSE, FALSE};
            DWORD dwBufSize;                
            PVOID pvNetworkTypesInUse;

            pvNetworkTypesInUse = malloc(sizeof(NDIS_802_11_NETWORK_TYPE_LIST));
            dwBufSize = sizeof(NDIS_802_11_NETWORK_TYPE_LIST);
            hr = NDTWlanGetNetworkTypesSuppported(hAdapter, pvNetworkTypesInUse, &dwBufSize);
            if (NOT_SUCCEEDED(hr) && NDT_STATUS_BUFFER_TOO_SHORT != hr)
            {
               NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_SUPPORTED Error:0x%x\n"), NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }
            else if (NOT_SUCCEEDED(hr))
            {
               pvNetworkTypesInUse = realloc(pvNetworkTypesInUse, dwBufSize);
               hr = NDTWlanGetNetworkTypesSuppported(hAdapter, pvNetworkTypesInUse, &dwBufSize);
               if (NOT_SUCCEEDED(hr))
               {
                  NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_SUPPORTED after reallocating buffer Error:0x%x\n"), NDIS_FROM_HRESULT(hr));
                  FlagError(ErrorSevere,&rc);
                  break;
               }           
            }

            PNDIS_802_11_NETWORK_TYPE_LIST pnNetworkTypeList = (PNDIS_802_11_NETWORK_TYPE_LIST) pvNetworkTypesInUse;
            if (3 != pnNetworkTypeList->NumberOfItems)
            {
               NDTLogErr(_T("NumberOfItems should have been 3 (Returned: %d\n"),pnNetworkTypeList->NumberOfItems);
               NDTLogErr(_T("Driver must set NumberOfItems to 3 for an 802.11a/g device\n"));
               FlagError(ErrorSevere,&rc);            
            }

            for (UINT iy=0; iy<pnNetworkTypeList->NumberOfItems; iy++)               
            {
               switch(pnNetworkTypeList->NetworkType[iy])
               {
                 case Ndis802_11OFDM5:
                     NDTLogDbg(_T("Ndis802_11OFDM5 found"));                     
                     abSupportedTypes[0]= TRUE;
                     break;
                  case Ndis802_11DS:
                     NDTLogDbg(_T("Ndis802_11DS found"));                     
                     abSupportedTypes[1]= TRUE;
                     break;
                  case Ndis802_11OFDM24:
                     NDTLogDbg(_T("Ndis802_11OFDM24 found"));                     
                     abSupportedTypes[2]= TRUE;
                     break;                 
               }               
            }

            if (FALSE == abSupportedTypes[0])
            {
               NDTLogErr(_T("Ndis802_11OFDM5 not found in supported list\n"));
               NDTLogErr(_T("Device is an 802.11a/g device so the driver must return Ndis802_11OFDM5 in the supported list\n"));
               FlagError(ErrorSevere,&rc);               
            }
            if (FALSE == abSupportedTypes[1])
            {
               NDTLogErr(_T("Ndis802_11DS not found in supported list\n"));
               NDTLogErr(_T("Device is an 802.11a/g device so the driver must return Ndis802_11DS in the supported list\n"));
               FlagError(ErrorSevere,&rc);               
            }
            if (FALSE == abSupportedTypes[2])
            {
               NDTLogErr(_T("Ndis802_11OFDM24 not found in supported list\n"));
               NDTLogErr(_T("Device is an 802.11a/g device so the driver must return Ndis802_11OFDM24 in the supported list\n"));
               FlagError(ErrorSevere,&rc);               
            }         
            if (pvNetworkTypesInUse)
               free(pvNetworkTypesInUse);            
         }
         while(0);      
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

