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
#include "wpadata.h"
#include "supplicant.h"

extern CSupplicant* g_pSupplicant;

//------------------------------------------------------------------------------

TEST_FUNCTION(TestWpaAssociationinfo)
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
   ULONG ulTimeout;
   NDIS_802_11_SSID ssidList[3];
   DWORD dwSsidCount;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;
   DWORD dwEncryption;

   BOOL bWPASupported;
   BOOL status;
   
   

   NDTLogMsg(
      _T("Start 1c_wpa_associationinfo on the adapter %s\n"), g_szTestAdapter
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
      NDTLogMsg(_T("Variation 1: Associate with WPA AP %s\n"), szSsidText);

      dwEncryption = Ndis802_11Encryption2Enabled;
      ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
      hr = NDTWPAAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeWPAPSK, dwEncryption, 
               NULL, ssidList[ix], FALSE ,&ulTimeout);         
      GetSsidText(szSsidText, ssidList[ix]);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Failed to associate with %s Error:0x%x\n"),szSsidText,hr);
         FlagError(ErrorSevere,&rc);            
         break;
      }
      else
         NDTLogDbg(_T("Succesfully associated with %s"),szSsidText);



      // Second Variation
      NDTLogMsg(_T("Variation 2: Query OID_802_11_ASSOCIATION_INFORMATION and verify data fields"));
      NDTLogMsg(_T("This test will query OID_802_11_ASSOCIATION_INFORMATION and then verify the data returned in the structure"));
      
      do
      {
         DWORD dwSize = sizeof(NDIS_802_11_ASSOCIATION_INFORMATION );
         PNDIS_802_11_ASSOCIATION_INFORMATION pnAssoInfo = (PNDIS_802_11_ASSOCIATION_INFORMATION )malloc(dwSize);
         hr = NDTWlanGetAssociationInformation(hAdapter, pnAssoInfo, &dwSize);
         if (NOT_SUCCEEDED(hr) && NDT_STATUS_BUFFER_TOO_SHORT)
         {
            pnAssoInfo = (PNDIS_802_11_ASSOCIATION_INFORMATION )malloc(dwSize);            
            hr = NDTWlanGetAssociationInformation(hAdapter, pnAssoInfo, &dwSize);            
         }            
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to query OID_802_11_ASSOCIATION_INFORMATION Error:0x%x\n"), NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("Succesfully got Association Informaiton"));

         if (0 >= pnAssoInfo->RequestIELength)
         {
            NDTLogErr(_T("RequestIELength field should not be zero\n"));
            FlagError(ErrorSevere,&rc);
         }

         //Capabilities
         //-------------------------------------------------------------------
         //| B0  |  B1  |      B2     |        B3       |    B4   | B5 - B15 |
         //-------------------------------------------------------------------
         //| ESS | IBSS | CF Pollable | CF Poll Request | Privacy | Reserved |
         //-------------------------------------------------------------------

         NDTLogDbg(_T("Querying Capabilities fields"));
         if ( NDIS_802_11_AI_REQFI_CAPABILITIES & pnAssoInfo->AvailableRequestFixedIEs)
         {
            NDTLogMsg(_T("Variation 3: Verify privacy bit is set in capabilites information of RequestFixedIE\n"));
            NDTLogMsg(_T("This test will verify the Privacy bit is set in the RequestFixedIE capabilities field\n"));
            if (0x10 !=  (pnAssoInfo->RequestFixedIEs.Capabilities & 0x10))
            {
               NDTLogErr(_T("Privacy bit in RequestFixedIEs.Capablities field not set\n"));
               FlagError(ErrorSevere,&rc);
            }


            NDTLogMsg(_T("Variation 4: Verify ESS bit is set in capabilites information of RequestFixedIE\n"));
            NDTLogMsg(_T("This test will verify the ESS bit is set in the RequestFixedIE capabilities field\n"));
            if (0x1 !=  (pnAssoInfo->RequestFixedIEs.Capabilities & 0x1))
            {
               NDTLogErr(_T("Privacy bit in RequestFixedIEs.Capablities field not set\n"));
               FlagError(ErrorSevere,&rc);
            }

            NDTLogMsg(_T("Variation 5: Verify IBSS bit is not set in capabilites information of RequestFixedIE\n"));
            NDTLogMsg(_T("This test will verify the IBSS bit is not set in the RequestFixedIE capabilites field\n"));
            if (0x2 ==  (pnAssoInfo->RequestFixedIEs.Capabilities & 0x2))
            {
               NDTLogErr(_T("IBSS bit in RequestFixedIEs.Capablities field should not be set\n"));
               FlagError(ErrorSevere,&rc);
            }
         }


         if (NDIS_802_11_AI_RESFI_CAPABILITIES & pnAssoInfo->AvailableResponseFixedIEs)
         {
            NDTLogMsg(_T("Variation 6: Verify privacy bit is set in capabilites information of ResponseFixedIE\n"));
            NDTLogMsg(_T("This test will verify the Privacy bit is set in the ResponseFixedIE capabilities field\n"));
            if (0x10 !=  (pnAssoInfo->ResponseFixedIEs.Capabilities & 0x10))
            {
               NDTLogErr(_T("Privacy bit in ResponseFixedIEs.Capablities field not set\n"));
               FlagError(ErrorSevere,&rc);
            }

            NDTLogMsg(_T("Variation 7: Verify ESS bit is set in capabilites information of ResponseFixedIE\n"));
            NDTLogMsg(_T("his test will verify the ESS bit is set in the ResponseFixedIE capabilities field\n"));
            if (0x1 !=  (pnAssoInfo->ResponseFixedIEs.Capabilities & 0x1))
            {
               NDTLogErr(_T("ESS bit in ResponseFixedIEs.Capablities field not set\n"));
               FlagError(ErrorSevere,&rc);
            }

             NDTLogMsg(_T("Variation 8: Verify IBSS bit is not set in capabilites information of ResponseFixedIE\n"));
             NDTLogMsg(_T("This test will verify the IBSS bit is not set in the ResponseFixedIE capabilites field\n"));
            if (0x2 ==  (pnAssoInfo->ResponseFixedIEs.Capabilities & 0x2))
            {
               NDTLogErr(_T("IBSS bit in ResponseFixedIEs.Capablities field should not be set\n"));
               FlagError(ErrorSevere,&rc);
            }            
         }

         if (NDIS_802_11_AI_REQFI_CURRENTAPADDRESS & pnAssoInfo->AvailableRequestFixedIEs)
         {
            NDTLogMsg(_T("Variation 9: Verify the CurrentAPAddress field of RequestFixedIE\n"));
            NDTLogMsg(_T("This test will verify the CurrentAPAddress field of the RequestFixedIE\n"));
            NDTLogMsg(_T("matches the BSSID returned from OID_802_11_BSSID\n"));
               
            NDIS_802_11_MAC_ADDRESS  nCurrentAPAddress;
            NDIS_802_11_MAC_ADDRESS nBssid;
            memcpy(nCurrentAPAddress,pnAssoInfo->RequestFixedIEs.CurrentAPAddress, 6 );

            hr = NDTWlanGetBssid(hAdapter, &nBssid);
            if (NOT_SUCCEEDED(hr))
            {
              NDTLogErr(_T("Failed to query OID_802_11_BSSID Error:0x%x\n"), NDIS_FROM_HRESULT(hr));
              FlagError(ErrorSevere,&rc);
              break;
            }

            if ( 0 != memcmp( nBssid, nCurrentAPAddress, sizeof(NDIS_802_11_MAC_ADDRESS)))
            {
               TCHAR szBssidText [18];
               GetBssidText(szBssidText, nCurrentAPAddress);
               NDTLogErr(_T("CurrentAPAddress should have been equal to BSSID (Returned: %s \n"),szBssidText);
               GetBssidText(szBssidText, nBssid);
               NDTLogErr(_T("Expected %s\n"), szBssidText);
               FlagError(ErrorSevere,&rc);
            }
         }

         // WPA Information Element
         //---------------------------------------------------------------------------------------------------------------------------------------------------------------
         //|  1  |     3    |    2    |      4      |          2           |         4-m         |            2             |            4-n          |         2        | Octets
         //---------------------------------------------------------------------------------------------------------------------------------------------------------------
         //| OUI | OUI Type | Version | Group Suite | Pairwise Suite Count | Pairwise Suite List | Auth Key Man Suite Count | Auth Key Man Suite List | WPA Capabilities | 
         //---------------------------------------------------------------------------------------------------------------------------------------------------------------
         
         NDTLogMsg(_T("Validate association request WPA information element\n"));
         NDTLogMsg(_T("This test will verify that the WPA information element is present in the variable IEs\n"));

         BOOL bFoundWPAIE = FALSE;
        
         PNDIS_802_11_VARIABLE_IEs pnVariableIes = (PNDIS_802_11_VARIABLE_IEs)((BYTE *)pnAssoInfo +pnAssoInfo->OffsetRequestIEs);
         while((BYTE *)pnVariableIes < ((BYTE *)pnAssoInfo + pnAssoInfo->OffsetRequestIEs +pnAssoInfo->RequestIELength))
         {
            if (221 == pnVariableIes->ElementID)
            {
               if ( 0x00 == pnVariableIes->data[0] && 0x50 == pnVariableIes->data[1]  && 0xF2 == pnVariableIes->data[2] )
                  if(0x01 != pnVariableIes->data[3])
                  {
                     NDTLogErr(_T("OUI Type was not equal to 01\n"));
                     FlagError(ErrorSevere,&rc);
                  }
                  bFoundWPAIE = TRUE;
                  break;
            }               
            pnVariableIes = (PNDIS_802_11_VARIABLE_IEs)(&(pnVariableIes->data[pnVariableIes->Length]));
         }

         if (FALSE == bFoundWPAIE)
         {
            NDTLogErr(_T("Association request information did not contain a WPA information element\n"));
            NDTLogErr(_T("A WPA information element must be present in the association request\n"));
            FlagError(ErrorSevere,&rc);
         }
         
         
      }
      while(0);
   }
   

cleanUp:
 
   NDTWlanCleanupTest(hAdapter);

   if (g_pSupplicant)
   {
      status = g_pSupplicant->CloseSupplicant();
      delete g_pSupplicant;
      g_pSupplicant = NULL;
   }

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

