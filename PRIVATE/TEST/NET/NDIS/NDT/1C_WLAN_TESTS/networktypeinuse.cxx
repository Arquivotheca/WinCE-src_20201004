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

HRESULT Verify80211B(HANDLE hAdapter, const UCHAR szAP[])
{
   HRESULT   hr = NDT_STATUS_SUCCESS;   
   ULONG ulKeyLength;
   ULONG ulTimeout;
   DWORD dwEncryption;
   NDIS_802_11_SSID ssid;
   DWORD dwNetworkTypeInUse;
   TCHAR szSsidText[33];
   TCHAR szText[40];
      
   hr = NDTWlanDisassociate(hAdapter);
  // Depending on strictness, this is flagged as error
  if (NOT_SUCCEEDED(hr) &&  g_dwStrictness == SevereStrict)
   {
      NDTLogErr(_T("Failed to disassociate Error:0x%x"), NDIS_FROM_HRESULT(hr));
      return hr;
   }

   GetNetworkTypeText(szText, Ndis802_11DS);                        
   hr = NDTWlanSetNetworkTypeInUse(hAdapter, Ndis802_11DS);
   if (NOT_SUCCEEDED(hr))
   {
      NDTLogErr(_T("Failed to set OID_802_11_NETWORK_TYPE_IN_USE to type %s Error0x:%x"),
         szText,NDIS_FROM_HRESULT(hr));
      return hr;
   }
   else
      NDTLogDbg(_T("Succesfully set netowrk type in use to %s"),szText);

   ssid.SsidLength = strlen((char *)szAP);
   memcpy(ssid.Ssid,szAP,ssid.SsidLength);   
   ulKeyLength = 10;
   dwEncryption = Ndis802_11WEPEnabled;
   ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
   hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
           ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssid, FALSE ,&ulTimeout);         

   GetSsidText(szSsidText, ssid);
   if (NOT_SUCCEEDED(hr))
   {
      NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
      return hr;
   }
   else
      NDTLogDbg(_T("Succesfully associated with %s"),szSsidText);



  hr = NDTWlanGetNetworkTypeInUse(hAdapter, &dwNetworkTypeInUse);
   if (NOT_SUCCEEDED(hr))
   {
      NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_IN_USE Error0x:%x"),NDIS_FROM_HRESULT(hr));
      return hr;
   }

   if ( Ndis802_11DS != dwNetworkTypeInUse)
   {
        TCHAR szText[40];
        GetNetworkTypeText(szText, dwNetworkTypeInUse);
        NDTLogErr(_T("Invalid network type %s returned"), szText);
        GetNetworkTypeText(szText, Ndis802_11DS);
        NDTLogErr(_T("Expected network type %s"), szText);                             
        NDTLogErr(_T("Driver must return Ndis802_11DS when associated with an 802.11b access point"));
        hr = NDT_STATUS_FAILURE;
        return hr;
   }      
   else
      NDTLogDbg(_T("PASS: Returned network type in use as Ndis802_11DS as expected"));

   return hr;
}


HRESULT Verify80211G(HANDLE hAdapter, const UCHAR szAP[])
{
   HRESULT   hr = NDT_STATUS_SUCCESS;   
   ULONG ulKeyLength;
   ULONG ulTimeout;
   DWORD dwEncryption;
   NDIS_802_11_SSID ssid;
   DWORD dwNetworkTypeInUse;
   TCHAR szText[40];
   TCHAR szSsidText[33];

   hr = NDTWlanDisassociate(hAdapter);
   // Depending on strictness, this is flagged as error
   if (NOT_SUCCEEDED(hr) &&  g_dwStrictness == SevereStrict)
  {
      NDTLogErr(_T("Failed to disassociate Error:0x%x"), NDIS_FROM_HRESULT(hr));
      return hr;
   }

   hr = NDTWlanSetNetworkTypeInUse(hAdapter, Ndis802_11OFDM24);

   GetNetworkTypeText(szText, Ndis802_11OFDM24);                     
   if (NOT_SUCCEEDED(hr))
   {
      NDTLogErr(_T("Failed to set OID_802_11_NETWORK_TYPE_IN_USE to type %s Error0x:%x"),
         szText,NDIS_FROM_HRESULT(hr));
      return hr;
   }
   else
      NDTLogDbg(_T("Succesfully set network type in use to %s"),szText);

   ssid.SsidLength = strlen((char *)szAP);
   memcpy(ssid.Ssid,szAP,ssid.SsidLength);   
   ulKeyLength = 10;
   dwEncryption = Ndis802_11WEPEnabled;
   ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
   hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
           ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssid, FALSE ,&ulTimeout);         

   GetSsidText(szSsidText, ssid);
   if (NOT_SUCCEEDED(hr))
   {
      NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
      return hr;
   }         
   else
      NDTLogDbg(_T("Succesfully associated with %s"),szSsidText);



  hr = NDTWlanGetNetworkTypeInUse(hAdapter, &dwNetworkTypeInUse);
   if (NOT_SUCCEEDED(hr))
   {
      NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_IN_USE Error0x:%x"),NDIS_FROM_HRESULT(hr));
      return hr;
   }

   if ( Ndis802_11OFDM24 != dwNetworkTypeInUse)
   {
        GetNetworkTypeText(szText, dwNetworkTypeInUse);
        NDTLogErr(_T("Invalid network type %s returned"), szText);
        GetNetworkTypeText(szText, Ndis802_11OFDM24);
        NDTLogErr(_T("Expected network type %s"), szText);                             
        NDTLogErr(_T("Driver must return Ndis802_11OFDM24 when associated with an 802.11g access point"));
        hr = NDT_STATUS_FAILURE;
        return hr;
   }      
   else
      NDTLogDbg(_T("PASS: Network type in use queried returned Ndis802_11OFDM24 as expected"));

   return hr;
}

HRESULT Verify80211A(HANDLE hAdapter, const UCHAR szAP[])
{
   HRESULT   hr = NDT_STATUS_SUCCESS;    
   ULONG ulKeyLength;
   ULONG ulTimeout;
   DWORD dwEncryption;
   NDIS_802_11_SSID ssid;
   DWORD dwNetworkTypeInUse;
   
   hr = NDTWlanDisassociate(hAdapter);
   // Depending on strictness, this is flagged as error
   if (NOT_SUCCEEDED(hr) &&  g_dwStrictness == SevereStrict)
   {
      NDTLogErr(_T("Failed to disassociate Error:0x%x"), NDIS_FROM_HRESULT(hr));
      return hr;
   }

   hr = NDTWlanSetNetworkTypeInUse(hAdapter, Ndis802_11OFDM5);
   if (NOT_SUCCEEDED(hr))
   {
      TCHAR szText[40];
      GetNetworkTypeText(szText, Ndis802_11OFDM5);                     
      NDTLogErr(_T("Failed to set OID_802_11_NETWORK_TYPE_IN_USE to type %s Error0x:%x"),
         szText,NDIS_FROM_HRESULT(hr));
      return hr;
   }

   ssid.SsidLength = strlen((char *)szAP);
   memcpy(ssid.Ssid,szAP,ssid.SsidLength);   
   ulKeyLength = 10;
   dwEncryption = Ndis802_11WEPEnabled;
   ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
   hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
           ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssid, FALSE ,&ulTimeout);         
   if (NOT_SUCCEEDED(hr))
   {
      TCHAR szSsidText[33];
      GetSsidText(szSsidText, ssid);
      NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
      return hr;
   }

  hr = NDTWlanGetNetworkTypeInUse(hAdapter, &dwNetworkTypeInUse);
   if (NOT_SUCCEEDED(hr))
   {
      NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_IN_USE Error0x:%x"),NDIS_FROM_HRESULT(hr));
      return hr;
   }

   if ( Ndis802_11OFDM5 != dwNetworkTypeInUse)
   {
        TCHAR szText[40];
        GetNetworkTypeText(szText, dwNetworkTypeInUse);
        NDTLogErr(_T("Invalid network type %s returned"), szText);
        GetNetworkTypeText(szText, Ndis802_11OFDM5);
        NDTLogErr(_T("Expected network type %s"), szText);                             
        NDTLogErr(_T("Driver must return Ndis802_11OFDM5 when associated with an 802.11a access point"));
        hr = NDT_STATUS_FAILURE;
        return hr;
   }      

   return hr;
}


//------------------------------------------------------------------------------

TEST_FUNCTION(TestNetworkTypeInUse)
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
   
   HANDLE hStatus = NULL;
   NDIS_802_11_SSID ssidList[3];
   DWORD dwSsidCount;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;
   
   

   NDTLogMsg(
      _T("Start 1c_wlan_networktypesinuse on the adapter %s\n"), g_szTestAdapter
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

   NDTLogMsg(_T("Variation 1: Load/unload driver\n"));
   NDTLogMsg(_T("This is done to place the device in an auto mode configuration since the "));
   NDTLogMsg(_T("previous tests may have changed the PHY type in use\n"));

   // TODO: Load Unload driver
      
   // Execute test by ssid
   for (ix= 0; ix<dwSsidCount; ix++)
   {
      TCHAR szSsidText[33];
      GetSsidText(szSsidText, ssidList[ix]);

      
      NDTLogMsg(_T("\nVariation 2: Set OID_802_11_NETWORK_TYPE_IN_USE and verify success \n"));
      NDTLogMsg(_T("This test will set OID_802_11_NETWORK_TYPE_IN_USE and verify the request succeeds\n"));

      do
      {
         if (WLAN_DEVICE_TYPE_802_11A == WlanDeviceType)
         {
            hr = NDTWlanSetNetworkTypeInUse(hAdapter, Ndis802_11OFDM5);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to set OID_802_11_NETWORK_TYPE_IN_USE to Ndis802_11OFDM5 Error0x:%x"),NDIS_FROM_HRESULT(hr));
               NDTLogErr(_T("Driver must support setting OID_802_11_NETWORK_TYPE_IN_USE"));
               FlagError(ErrorSevere,&rc);
               break;
            }
         }
         else
         {
            hr = NDTWlanSetNetworkTypeInUse(hAdapter, Ndis802_11DS);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to set OID_802_11_NETWORK_TYPE_IN_USE to Ndis802_11DS Error0x:%x"),NDIS_FROM_HRESULT(hr));
               NDTLogErr(_T("Driver must support setting OID_802_11_NETWORK_TYPE_IN_USE"));
               FlagError(ErrorSevere,&rc);
               break;
            } 
         }            
      }
      while (0);
    
      NDTLogMsg(_T("\nVariation 3: Set OID_802_11_NETWORK_TYPE_IN_USE with invalid value \n"));
      NDTLogMsg(_T("This test will set OID_802_11_NETWORK_TYPE_IN_USE with an invalid value and verify the request fails\n"));

      do
      {
         hr = NDTWlanSetNetworkTypeInUse(hAdapter, 255);
         if (NDT_STATUS_INVALID_DATA != hr)
         {
            NDTLogErr(_T("Should have returned NDIS_STATUS_INVALID_DATA - Returned 0x:%x"),NDIS_FROM_HRESULT(hr));
            NDTLogErr(_T("Driver must return NDIS_STATUS_INVALID_DATA when OID_802_11_NETWORK_TYPE_IN_USE is set with an invalid value"));
            FlagError(ErrorSevere,&rc);
            break;
         }
      }
      while (0);

      BOOL bIsWPa;
      hr = NDTWlanIsWPASupported(hAdapter, &bIsWPa);
      if (!NOT_SUCCEEDED(hr))
      {
         if (TRUE == bIsWPa)         
         {
            NDTLogMsg(_T("\nVariation 4: Set OID_802_11_NETWORK_TYPE_IN_USE for each of the values return in the supported list \n"));
            NDTLogMsg(_T("This test will query OID_802_11_NETWORK_TYPES_SUPPORTED and then set OID_802_11_NETWORK_TYPE_IN_USE\n"));
            NDTLogMsg(_T("with each of the values supported"));
            do
            {
               DWORD dwSize = sizeof(NDIS_802_11_NETWORK_TYPE_LIST);
               PNDIS_802_11_NETWORK_TYPE_LIST pnNetworkTypeList = (PNDIS_802_11_NETWORK_TYPE_LIST)malloc(dwSize);

               hr = NDTWlanGetNetworkTypesSuppported(hAdapter, (PVOID)pnNetworkTypeList, &dwSize);
               if (NOT_SUCCEEDED(hr) && NDT_STATUS_BUFFER_TOO_SHORT == hr)
               {
                  pnNetworkTypeList = (PNDIS_802_11_NETWORK_TYPE_LIST)malloc(dwSize);
                  hr = NDTWlanGetNetworkTypesSuppported(hAdapter, (PVOID)pnNetworkTypeList, &dwSize);               
               }

               if (NOT_SUCCEEDED(hr))
               {
                    NDTLogErr(_T("Failed to query NetworkTypesSupported Error:0x%x"),NDIS_FROM_HRESULT(hr));
                    FlagError(ErrorSevere,&rc);
                    break;
               }

               for(UINT iIndex = 0; iIndex<pnNetworkTypeList->NumberOfItems; iIndex++)
               {
                  DWORD dwNetworkTypeInUse;
                  TCHAR szNetworkTypeText[80];
                  GetNetworkTypeText(szNetworkTypeText,pnNetworkTypeList->NetworkType[iIndex] );
                  NDTLogDbg(_T("Setting network type in use to %s"),szNetworkTypeText);
                  
                  hr = NDTWlanSetNetworkTypeInUse(hAdapter, pnNetworkTypeList->NetworkType[iIndex]);
                  if (NOT_SUCCEEDED(hr))
                  {
                     TCHAR szText[40];
                     GetNetworkTypeText(szText, pnNetworkTypeList->NetworkType[iIndex]);                     
                     NDTLogErr(_T("Failed to set OID_802_11_NETWORK_TYPE_IN_USE to type %s Error0x:%x"),
                        szText,NDIS_FROM_HRESULT(hr));
                     FlagError(ErrorSevere,&rc);
                     break;
                  }

                  hr = NDTWlanGetNetworkTypeInUse(hAdapter, &dwNetworkTypeInUse);
                  if (NOT_SUCCEEDED(hr))
                  {
                     NDTLogErr(_T("Failed to query OID_802_11_NETWORK_TYPE_IN_USE Error0x:%x"),NDIS_FROM_HRESULT(hr));
                     FlagError(ErrorSevere,&rc);
                     break;
                  }

                  if ( pnNetworkTypeList->NetworkType[iIndex] != dwNetworkTypeInUse)
                  {
                       TCHAR szText[40];
                       GetNetworkTypeText(szText, dwNetworkTypeInUse);
                       NDTLogErr(_T("Invalid network type %s returned"), szText);
                       GetNetworkTypeText(szText, pnNetworkTypeList->NetworkType[iIndex]);
                       NDTLogErr(_T("Expected network type %s"), szText);                       
                       NDTLogErr(_T("Driver must allow each of the network types supported to be set"));
                       FlagError(ErrorSevere,&rc);                     
                  }
               }
               
            }
            while(0);
         }
      }
      else
         NDTLogErr(_T("Failed to query IsWPASupported Error:0x%x"), NDIS_FROM_HRESULT(hr));

      // AP MATRIX   |  AP1      AP2      AP3   AP4
      //--------------------------------------------
      //  802.11a       |  11a      11a      N/A   11a
      //  802.11b       |  11b      11b      N/A   
      //  802.11g       |  11b      11g      N/A
      //  802.11a/b    |  11b      11a      N/A
      //  802.11a/g    |  11b      11a      11g

     TCHAR szNetworkTypeText[80];
     GetNetworkTypeText(szNetworkTypeText, WlanDeviceType);
     NDTLogMsg(_T("Network Device is of type %s"), szNetworkTypeText);
     NDTLogMsg(_T("Depending on type, the card is associated with various APs and the network type is queried and verified"));

      
      if (WLAN_DEVICE_TYPE_802_11A == WlanDeviceType)
      {
         NDTLogMsg(_T("Variation 5: Verify Ndis802_11OFDM5 is returned when associated with an 802.11a AP\n"));
         NDTLogMsg(_T("This test will associate with the 802.11a AP and then verify"));
         NDTLogMsg(_T("OID_802_11_NETWORK_TYPE_IN_USE returns Ndis802_11OFDM5"));

         hr = Verify80211A(hAdapter, WLAN_WEP_AP1);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Verifying 802.11a Network type in use failed"));
            FlagError(ErrorSevere,&rc);
         }
      }
      if (WLAN_DEVICE_TYPE_802_11B == WlanDeviceType)
      {
         NDTLogMsg(_T("Variation 5: Verify Ndis802_11DS is returned when associated with an 802.11b AP\n"));
         NDTLogMsg(_T("This test will associate with the 802.11b AP and then verify"));
         NDTLogMsg(_T("OID_802_11_NETWORK_TYPE_IN_USE returns Ndis802_11DS"));

         hr = Verify80211B(hAdapter, WLAN_WEP_AP1);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Verifying 802.11b Network type in use failed"));
            FlagError(ErrorSevere,&rc);
         }
      }
      if (WLAN_DEVICE_TYPE_802_11G == WlanDeviceType)
      {
         NDTLogMsg(_T("Variation 5: Verify Ndis802_11DS is returned when associated with an 802.11b AP\n"));
         NDTLogMsg(_T("This test will associate with the 802.11b AP and then verify"));
         NDTLogMsg(_T("OID_802_11_NETWORK_TYPE_IN_USE returns Ndis802_11DS"));

         hr = Verify80211B(hAdapter, WLAN_WEP_AP1);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Verifying 802.11b Network type in use failed"));
            FlagError(ErrorSevere,&rc);
         }


         NDTLogMsg(_T("Variation 6: Verify Ndis802_11OFDM24 is returned when associated with an 802.11g AP\n"));
         NDTLogMsg(_T("This test will associate with the 802.11g AP and then verify"));
         NDTLogMsg(_T("OID_802_11_NETWORK_TYPE_IN_USE returns Ndis802_11OFDM24"));

         hr = Verify80211G(hAdapter, WLAN_WEP_AP2);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Verifying 802.11g Network type in use failed"));
            FlagError(ErrorSevere,&rc);
         }

      }
      if (WLAN_DEVICE_TYPE_802_11AB == WlanDeviceType)
      {
         NDTLogMsg(_T("Variation 5: Verify Ndis802_11DS is returned when associated with an 802.11b AP\n"));
         NDTLogMsg(_T("This test will associate with the 802.11b AP and then verify"));
         NDTLogMsg(_T("OID_802_11_NETWORK_TYPE_IN_USE returns Ndis802_11DS"));

         hr = Verify80211B(hAdapter, WLAN_WEP_AP1);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Verifying 802.11b Network type in use failed"));
            FlagError(ErrorSevere,&rc);
         }



         NDTLogMsg(_T("Variation 6: Verify Ndis802_11OFDM5 is returned when associated with an 802.11a AP\n"));
         NDTLogMsg(_T("This test will associate with the 802.11a AP and then verify"));
         NDTLogMsg(_T("OID_802_11_NETWORK_TYPE_IN_USE returns Ndis802_11OFDM5"));

         hr = Verify80211A(hAdapter, WLAN_WEP_AP2);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Verifying 802.11a Network type in use failed"));
            FlagError(ErrorSevere,&rc);
         }

      }    
      if (WLAN_DEVICE_TYPE_802_11AG == WlanDeviceType)
      {
         NDTLogMsg(_T("Variation 5: Verify Ndis802_11DS is returned when associated with an 802.11b AP\n"));
         NDTLogMsg(_T("This test will associate with the 802.11b AP and then verify"));
         NDTLogMsg(_T("OID_802_11_NETWORK_TYPE_IN_USE returns Ndis802_11DS"));

         hr = Verify80211B(hAdapter, WLAN_WEP_AP1);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Verifying 802.11b Network type in use failed"));
            FlagError(ErrorSevere,&rc);
         }


         NDTLogMsg(_T("Variation 6: Verify Ndis802_11OFDM5 is returned when associated with an 802.11a AP\n"));
         NDTLogMsg(_T("This test will associate with the 802.11a AP and then verify"));
         NDTLogMsg(_T("OID_802_11_NETWORK_TYPE_IN_USE returns Ndis802_11OFDM5"));

         hr = Verify80211A(hAdapter, WLAN_WEP_AP2);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Verifying 802.11a Network type in use failed"));
            FlagError(ErrorSevere,&rc);
         }


         NDTLogMsg(_T("Variation 7: Verify Ndis802_11OFDM24 is returned when associated with an 802.11g AP\n"));
         NDTLogMsg(_T("This test will associate with the 802.11g AP and then verify"));
         NDTLogMsg(_T("OID_802_11_NETWORK_TYPE_IN_USE returns Ndis802_11OFDM24"));

         hr = Verify80211G(hAdapter, WLAN_WEP_AP3);   
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Verifying 802.11g Network type in use failed"));
            FlagError(ErrorSevere,&rc);
         }

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

