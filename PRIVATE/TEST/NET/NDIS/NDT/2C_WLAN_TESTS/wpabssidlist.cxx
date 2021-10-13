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

TEST_FUNCTION(TestWpaBssidlist)
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
   ULONG ulKeyLength;
   NDIS_802_11_SSID ssidList[3];
   DWORD dwSsidCount;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;
   DWORD dwEncryption;

   BOOL bWPASupported;
   BOOL status;
   
   UINT   cbAddr = 0;
   UINT   cbHeader = 0;   
   DWORD  dwReceiveDelay = 0;   

   ULONG ulSendPacketsCompleted=0;
   ULONG ulPacketsToSend ;
   ULONG ulPacketsSent = 0;
   ULONG ulPacketsReceived = 0;
   ULONG ulMinDirectedPass;
   ULONG ulSendTime = 0;
   ULONG ulSendBytesSent = 0;
   ULONG ulRecvTime=0;
   ULONG ulRecvBytesReceived =0;
   UINT   uiMinimumPacketSize = 64;

   PNDIS_WLAN_BSSID_EX pnWLanBssidEx=NULL;

   memset(ahAdapter, 0, sizeof(ahAdapter));

   NDTLogMsg(
      _T("Start 2c_wpa_bssidlist test\n")
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
      NDTLogMsg(_T("Variation 1: Verify IBSS node is visible in list"));
      NDTLogMsg(_T("This test will create an IBSS cell on the support device then perform a list scan on the test device"));
      NDTLogMsg(_T("and verify the IBSS cell is visible in the list"));      

      do
      {
         NDIS_802_11_SSID ssidIBSS;
         ssidIBSS.SsidLength = strlen((char *)WLAN_IBSS);         
         memcpy(ssidIBSS.Ssid, WLAN_IBSS, ssidIBSS.SsidLength);

         ulKeyLength = 10;         
         hr = NDTWlanCreateIBSS(ahAdapter[1],  Ndis802_11AuthModeOpen, Ndis802_11WEPEnabled, ulKeyLength,
               0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidIBSS);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to Create IBSS Error:0x%x"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("Support adapter Successfully created IBSS"));

         BOOL bFoundIbss = FALSE;       
                  
         for (UINT iIndex =0; iIndex<2; iIndex++ )
         {
            hr = NDTWlanSetBssidListScan(ahAdapter[0]);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to Set bssid list scan Error:0x%x"),hr);
               FlagError(ErrorSevere,&rc);
               break;
            }

            DWORD dwSize = 2048;            
            PNDIS_802_11_BSSID_LIST_EX pnBSSIDListEx = (PNDIS_802_11_BSSID_LIST_EX) malloc(dwSize);
            BOOL bGotBssidList = FALSE;
            if (!pnBSSIDListEx)
            {
               NDTLogWrn(_T("Malloc failed for Bssidlistex"));
               break;
            }
                  
            UINT retries = 20;
            do
            {                         
               hr = NDTWlanGetBssidList(ahAdapter[0], pnBSSIDListEx, &dwSize);            
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
            while(--retries > 0);
            if (retries <= 0)
            {
               NDTLogErr(_T("Failed to get OID_802_11_BSSID_LIST_SCAN after multiple attempts Error =0x%x\n"), hr);       
               FlagError(ErrorSevere,&rc);
               break;
            }
            
            // Loop through the list of bssid and search for each known ssid            
            pnWLanBssidEx= &(pnBSSIDListEx->Bssid[0]);
            for (UINT iz=0; iz<(pnBSSIDListEx->NumberOfItems) && bGotBssidList; iz++)
            {                  
               TCHAR szBssidExText[80];                                       
               memset(szBssidExText,_T('\0'),sizeof(szBssidExText));
               GetBssidExText(szBssidExText, *pnWLanBssidEx);
               NDTLogMsg(_T("%s\n"),szBssidExText);
               
                if (pnWLanBssidEx->Ssid.SsidLength == ssidIBSS.SsidLength && !memcmp(pnWLanBssidEx->Ssid.Ssid,ssidIBSS.Ssid,ssidIBSS.SsidLength))
                {
                     bFoundIbss = TRUE;
                 }
                 pnWLanBssidEx = (PNDIS_WLAN_BSSID_EX)((BYTE*)pnWLanBssidEx + (pnWLanBssidEx->Length));
              }
            
              if (pnBSSIDListEx)
                  free(pnBSSIDListEx);
         }

         TCHAR szSsidText[33];
         GetSsidText(szSsidText, ssidIBSS);
         if (FALSE == bFoundIbss)
         {
            NDTLogErr(_T("Did not find %s"), szSsidText);
            FlagError(ErrorMild,&rc);            
         }
         else
            NDTLogDbg(_T("PASS: Found IBSS %s"),szSsidText);
      }
      while(0);

      // Variation 2
      NDTLogMsg(_T("Variation 2: Verify BSSID information elements"));
      NDTLogMsg(_T("This test will verify that the BSSID for the WPA AP contains valid information elements"));
      do
      {
            hr = NDTWlanSetBssidListScan(ahAdapter[0]);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to Set bssid list scan Error:0x%x"),hr);
               FlagError(ErrorSevere,&rc);
               break;
            }

            DWORD dwSize = 2048;            
            PNDIS_802_11_BSSID_LIST_EX pnBSSIDListEx = (PNDIS_802_11_BSSID_LIST_EX) malloc(dwSize);
            BOOL bGotBssidList = FALSE;
            if (!pnBSSIDListEx)
            {
               NDTLogWrn(_T("Malloc failed for Bssidlistex"));
               break;
            }
                  
            UINT retries = 20;
            do
            {                         
               hr = NDTWlanGetBssidList(ahAdapter[0], pnBSSIDListEx, &dwSize);            
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
            while(--retries > 0);
            if (retries <= 0)
            {
               NDTLogErr(_T("Failed to get OID_802_11_BSSID_LIST_SCAN after multiple attempts Error =0x%x\n"), hr);       
               FlagError(ErrorSevere,&rc);
               break;
            }


            // Loop through the list of bssid and search for each known ssid            
            pnWLanBssidEx= &(pnBSSIDListEx->Bssid[0]);
            for (UINT iz=0; iz<(pnBSSIDListEx->NumberOfItems) && bGotBssidList; iz++)
            {                  
               TCHAR szBssidExText[80];                                       
               memset(szBssidExText,_T('\0'),sizeof(szBssidExText));
               GetBssidExText(szBssidExText, *pnWLanBssidEx);
               NDTLogMsg(_T("%s\n"),szBssidExText);
               
                if (pnWLanBssidEx->Ssid.SsidLength == ssidList[ix].SsidLength && !memcmp(pnWLanBssidEx->Ssid.Ssid,ssidList[ix].Ssid,ssidList[ix].SsidLength))
                {
                     if ( 0 == pnWLanBssidEx->IELength)
                     {
                        NDTLogErr(_T("BSSID did not contain any information elements"));
                        NDTLogErr(_T("The driver is expected to return the information elements for the WPA AP"));
                        FlagError(ErrorSevere,&rc);
                     }     
                     else
                        NDTLogDbg(_T("PASS: WPA AP contains IEs "));
                 }
               pnWLanBssidEx = (PNDIS_WLAN_BSSID_EX)((BYTE*)pnWLanBssidEx + (pnWLanBssidEx->Length));
            }
            
            if (pnBSSIDListEx)
               free(pnBSSIDListEx);         
      }
      while(0);

      // Variation 3
      NDTLogMsg(_T("Variation 3: Verify packets sent while a scan is in progress are received"));
      NDTLogMsg(_T("This test will send packets while a scan is in progress and verify the packets are received"));

      do
      {
         ulPacketsToSend = 25;
         ulMinDirectedPass = g_dwDirectedPasspercentage * ulPacketsToSend /100;

         NDIS_802_11_SSID ssidWEP;          
         ssidWEP.SsidLength = strlen((char *)WLAN_WEP_AP1);         
         memcpy(ssidWEP.Ssid, WLAN_WEP_AP1, ssidWEP.SsidLength);
   
         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[1], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidWEP, FALSE ,&ulTimeout);         

         GetSsidText(szSsidText, ssidWEP);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("Succesfully associated support with %s"),szSsidText);          

         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidWEP, FALSE ,&ulTimeout);         

         GetSsidText(szSsidText, ssidWEP);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate test adapter with %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("Succesfully associated test with %s"),szSsidText);          
        

         hr = NDTWlanDirectedSend(ahAdapter[0], ahAdapter[1], cbAddr, ndisMedium,
            ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("DirectedSend failed %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;        
         }         
         
         if (ulPacketsReceived < ulMinDirectedPass)
         {
            NDTLogErr(_T("Test -> Support -Received less than the required amount of directed packets with WLAN Directed"));
            NDTLogErr(_T("send pass Percentage at %d (Received: %d, Expected: %d)"),g_dwDirectedPasspercentage,ulPacketsReceived, ulMinDirectedPass);
            FlagError(ErrorSevere,&rc);
         }
		 else
			 NDTLogDbg(_T("Directed Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);


         hr = NDTWlanSetBssidListScan(ahAdapter[0]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to Set bssid list scan Error:0x%x"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }


         ulPacketsSent = 0;
         ulPacketsReceived = 0;
         hr = NDTWlanDirectedSend(ahAdapter[0], ahAdapter[1], cbAddr, ndisMedium,
            ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("DirectedSend failed %s Error:0x%x"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;        
         }         
         
         if (ulPacketsReceived < ulMinDirectedPass)
         {
            NDTLogErr(_T("Test -> Support -Received less than the required amount of directed packets with WLAN Directed "));
            NDTLogErr(_T("send pass Percentage at %d (Received: %d, Expected: %d)"),g_dwDirectedPasspercentage,ulPacketsReceived, ulMinDirectedPass);
            FlagError(ErrorSevere,&rc);
         }
		 else
			 NDTLogDbg(_T("Directed Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);
         
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

