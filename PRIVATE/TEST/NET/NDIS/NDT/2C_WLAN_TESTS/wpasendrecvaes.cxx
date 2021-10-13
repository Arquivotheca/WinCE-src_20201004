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

BOOL bIsAESSupported = FALSE;
BOOL bIsTKIPSupported = FALSE;

HRESULT CheckAPSupport(HANDLE hAdapter, NDIS_802_11_SSID ssid)
{
   HRESULT hr = NDT_STATUS_SUCCESS;
   hr = NDTWlanSetBssidListScan(hAdapter);
   if (NOT_SUCCEEDED(hr))
   {
      NDTLogErr(_T("Failed to set OID_802_11_BSSID_LIST_SCAN Error:0x%x\n"),NDIS_FROM_HRESULT(hr));
      return hr;
   }
   
   Sleep(WLAN_BSSID_LIST_SCAN_TIMEOUT * 2);

   DWORD dwSize = 2048;            
   PNDIS_802_11_BSSID_LIST_EX pnBSSIDListEx = (PNDIS_802_11_BSSID_LIST_EX) malloc(dwSize);           
   BOOL bGotBssidList = FALSE;
   hr = NDTWlanGetBssidList(hAdapter, pnBSSIDListEx, &dwSize);
   if (NOT_SUCCEEDED(hr) && (hr == NDT_STATUS_BUFFER_TOO_SHORT))                  
   {
      PNDIS_802_11_BSSID_LIST_EX pnTemp = NULL;
      pnTemp = (PNDIS_802_11_BSSID_LIST_EX) realloc(pnBSSIDListEx,dwSize);
      if (NULL == pnTemp)
      {
         NDTLogErr(_T("Cannot reallocate memory"));
         hr = NDT_STATUS_FAILURE;
         goto cleanUp;
      }
      else
      {
         pnBSSIDListEx = pnTemp;
      }   
      hr = NDTWlanGetBssidList(hAdapter, pnBSSIDListEx, &dwSize);   
   }

   if (NOT_SUCCEEDED(hr))
   {
      NDTLogErr(_T("FAiled to get Bssid list Error:0x%x"),NDIS_FROM_HRESULT(hr));
      goto cleanUp;
   }
   else
      bGotBssidList = TRUE;
   
   // Loop through the list of bssid and search for each known ssid
   PNDIS_WLAN_BSSID_EX pnWLanBssidEx=NULL;

   BOOL bFoundSsid = FALSE;
   BOOL bFoundWPAIE = FALSE;
   pnWLanBssidEx= &(pnBSSIDListEx->Bssid[0]);
   for (UINT iz=0; iz<(pnBSSIDListEx->NumberOfItems) && bGotBssidList; iz++)
   {                  
      TCHAR szBssidExText[80];                                       
      memset(szBssidExText,_T('\0'),sizeof(szBssidExText));
      GetBssidExText(szBssidExText, *pnWLanBssidEx);
      NDTLogMsg(_T("%s\n"),szBssidExText);      

      if (pnWLanBssidEx->Ssid.SsidLength == ssid.SsidLength && !memcmp(pnWLanBssidEx->Ssid.Ssid,ssid.Ssid,ssid.SsidLength))
      {
         bFoundSsid = TRUE;
         if (pnWLanBssidEx->Length > 104 && pnWLanBssidEx->IELength > 0)
         {
            PNDIS_802_11_FIXED_IEs pnFixedIes = (PNDIS_802_11_FIXED_IEs) pnWLanBssidEx->IEs;
            PNDIS_802_11_VARIABLE_IEs pnVariableIes;

            pnVariableIes = (PNDIS_802_11_VARIABLE_IEs) (pnFixedIes + 1);
            while((void *)pnVariableIes< ((void *)&(pnWLanBssidEx->IEs[pnWLanBssidEx->IELength])))
            {
               if (WLAN_IE_WPA == pnVariableIes->ElementID  )
               {
                  if ( 0x00 == pnVariableIes->data[0] && 0x50 == pnVariableIes->data[1]  && 0xF2 == pnVariableIes->data[2] )
                  {
                     bFoundWPAIE = TRUE;

                     for (INT iIndex=0; iIndex < (pnVariableIes->Length - 4 -1); iIndex++)
                     {
                        // AES/CCMP
                        if (0x00 == pnVariableIes->data[iIndex] && 0x50 == pnVariableIes->data[iIndex + 1]
                           && 0xF2 == pnVariableIes->data[iIndex + 2] && 0x04 == pnVariableIes->data[iIndex + 3])
                        {
                           bIsAESSupported = TRUE;
                        }
                        // AES/WRAP
                        if (0x00 == pnVariableIes->data[iIndex] && 0x50 == pnVariableIes->data[iIndex + 1]
                           && 0xF2 == pnVariableIes->data[iIndex + 2] && 0x03 == pnVariableIes->data[iIndex + 3])
                        {
                           bIsAESSupported = TRUE;
                        }
                        // TKIP
                        if (0x00 == pnVariableIes->data[iIndex] && 0x50 == pnVariableIes->data[iIndex + 1]
                           && 0xF2 == pnVariableIes->data[iIndex + 2] && 0x02 == pnVariableIes->data[iIndex + 3])
                        {
                           bIsTKIPSupported = TRUE;
                        }
                     }
                  }
               }               
               pnVariableIes= (PNDIS_802_11_VARIABLE_IEs)(&(pnVariableIes->data[pnVariableIes->Length]));            
            }
            
         }
      }

      pnWLanBssidEx = (PNDIS_WLAN_BSSID_EX)((BYTE*)pnWLanBssidEx + (pnWLanBssidEx->Length));
   }

  TCHAR szSsidText[33];
  GetSsidText(szSsidText, ssid);

   if (FALSE == bFoundSsid)
   {
      NDTLogErr(_T("Did not find %s in Bssid list"), szSsidText);
      goto cleanUp;
   }
   else
      if (FALSE == bFoundWPAIE)
      {
         NDTLogErr(_T(" %s did not contain a WPA Information Element"), szSsidText);
         goto cleanUp;
      }

   cleanUp:
   if (pnBSSIDListEx)
      free (pnBSSIDListEx);
   return hr;
}

//------------------------------------------------------------------------------

TEST_FUNCTION(TestWpaSendrecvAes)
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
   NDIS_802_11_SSID ssidTestList[3];
   NDIS_802_11_SSID ssidSuppList[3];
   DWORD dwSsidCount;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;
   DWORD dwEncryption;

   BOOL bWPASupported;
   BOOL bAESSupported;
   BOOL status;

   ULONG ulSendPacketsCompleted=0;
   ULONG ulPacketsToSend = 50;
   ULONG ulPacketsSent = 0;
   ULONG ulPacketsReceived = 0;
   ULONG ulMinDirectedPass;
   ULONG ulMinBroadcastPass;
   ULONG ulSendTime = 0;
   ULONG ulSendBytesSent = 0;
   ULONG ulRecvTime=0;
   ULONG ulRecvBytesReceived =0;
   UINT   uiMinimumPacketSize = 64;
   
   
   UINT   cbAddr = 0;
   UINT   cbHeader = 0;   
   DWORD  dwReceiveDelay = 0;   

   
   memset(ahAdapter, 0, sizeof(ahAdapter));

   NDTLogMsg(
      _T("Start 2c_wpa_sendrecvaes test\n")
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
         NDTLogErr(_T("Failed to initialize Wlan %s adapter Error0x%x"), (ixAdapter ? _T("Support") : _T("Test")) ,hr);
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

   hr = NDTWlanIsWPASupported(ahAdapter[1], &bWPASupported);
   if (FAILED(hr))
   {
      NDTLogErr(_T("Failed to get IsWPASupported for support Error:0x%x\n"),hr);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }
   if (FALSE == bWPASupported)
   {
      NDTLogErr(_T("Support %s does not support WPA, skipping test\n"), g_szHelpAdapter);
      rc = TPR_SKIP;
      goto cleanUp;
   }

   hr = NDTWlanIsAESSupported(ahAdapter[0], &bAESSupported);
   if (FAILED(hr))
   {
      NDTLogErr(_T("Failed to get IsAESSupported Error:0x%x\n"),hr);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }
   if (FALSE == bAESSupported)
   {
      NDTLogErr(_T("%s does not support AES, skipping test\n"), g_szTestAdapter);
      rc = TPR_SKIP;
      goto cleanUp;
   }

   hr = NDTWlanIsAESSupported(ahAdapter[1], &bAESSupported);
   if (FAILED(hr))
   {
      NDTLogErr(_T("Failed to get IsAESSupported for support Error:0x%x\n"),hr);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }
   if (FALSE == bAESSupported)
   {
      NDTLogErr(_T("Support %s does not support AES, skipping test\n"), g_szHelpAdapter);
      rc = TPR_SKIP;
      goto cleanUp;
   }

  NDIS_802_11_SSID ssidTemp;
  ssidTemp.SsidLength= strlen((char *)WLAN_WPA_AP1);
  memcpy((ssidTemp.Ssid), WLAN_WPA_AP1,ssidTemp.SsidLength);

  hr = CheckAPSupport(ahAdapter[0], ssidTemp);
  if (NOT_SUCCEEDED(hr))
   {
      NDTLogErr(_T("Failed to check AP support Error:0x%x\n"), NDIS_FROM_HRESULT(hr));
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }

   if (FALSE == bIsAESSupported)
   {
      NDTLogErr(_T("The test device indicates that it supports AES, however %S does not have AES enabled"), WLAN_WPA_AP1);
      NDTLogErr(_T("Please enable AES encryption on %S and rerun test"), WLAN_WPA_AP1);
      rc = TPR_SKIP;
      goto cleanUp;
   }

   
   dwSsidCount = 0;
   // Test APs depending on Network Type
   if (WlanDeviceType == WLAN_DEVICE_TYPE_802_11B || WlanDeviceType == WLAN_DEVICE_TYPE_802_11A)
   {
      dwSsidCount++;
      ssidTestList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WPA_AP1);
      memcpy((ssidTestList[dwSsidCount -1].Ssid), WLAN_WPA_AP1,ssidTestList[dwSsidCount -1].SsidLength);

      ssidSuppList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WEP_AP1);
      memcpy((ssidSuppList[dwSsidCount -1].Ssid), WLAN_WEP_AP1,ssidSuppList[dwSsidCount -1].SsidLength);      
   }
   else
      if (WlanDeviceType == WLAN_DEVICE_TYPE_802_11G || WlanDeviceType == WLAN_DEVICE_TYPE_802_11AB)
   {
      dwSsidCount++;
      ssidTestList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WPA_AP1);
      memcpy((ssidTestList[dwSsidCount -1].Ssid), WLAN_WPA_AP1,ssidTestList[dwSsidCount -1].SsidLength);

      ssidSuppList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WEP_AP2);
      memcpy((ssidSuppList[dwSsidCount -1].Ssid), WLAN_WEP_AP2,ssidSuppList[dwSsidCount -1].SsidLength);      
   }
   else
      if (WlanDeviceType == WLAN_DEVICE_TYPE_802_11AG)
   {
      dwSsidCount++;
      ssidTestList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WPA_AP1);
      memcpy((ssidTestList[dwSsidCount -1].Ssid), WLAN_WPA_AP1,ssidTestList[dwSsidCount -1].SsidLength);

      ssidSuppList[dwSsidCount -1].SsidLength= strlen((char *)WLAN_WEP_AP3);
      memcpy((ssidSuppList[dwSsidCount -1].Ssid), WLAN_WEP_AP3,ssidSuppList[dwSsidCount -1].SsidLength);
   } 
       
   ulMinDirectedPass = g_dwDirectedPasspercentage* ulPacketsToSend /100;
   ulMinBroadcastPass = WLAN_PERCENT_TO_PASS_BROADCAST* ulPacketsToSend /100;
      
   // Execute test by ssid
   for (ix= 0; ix<dwSsidCount; ix++)
   {
      TCHAR szSsidText[33];
         
       // First Variation
      NDTLogMsg(_T("Variation 1: Associate support device with WEP AP"));
   
      ulKeyLength = 10;
      dwEncryption = Ndis802_11WEPEnabled;
      ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
      hr = NDTWlanAssociate(ahAdapter[1], Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
              ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidSuppList[ix], FALSE ,&ulTimeout);         

      GetSsidText(szSsidText, ssidSuppList[ix]);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Failed to associate with %s Error:0x%x"),szSsidText,hr);
         FlagError(ErrorSevere,&rc);            
         break;
      }
      else
         NDTLogDbg(_T("Succesfully associated support with %s"),szSsidText);


      dwEncryption = Ndis802_11Encryption3Enabled;
      ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
      hr = NDTWPAAssociate(ahAdapter[0], Ndis802_11Infrastructure, Ndis802_11AuthModeWPAPSK, dwEncryption, 
               NULL, ssidTestList[ix], FALSE ,&ulTimeout);         

      GetSsidText(szSsidText, ssidTestList[ix]);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("Failed to associate test adapter with %s Error:0x%x\n"),szSsidText,hr);
         FlagError(ErrorSevere,&rc);            
         break;
      }               
      else
         NDTLogDbg(_T("Succesfully associated test with %s"),szSsidText);
      

      // Second Variation
      NDTLogMsg(_T("Variation 2: Verifying directed send\\receive"));
      NDTLogMsg(_T("This test will verify directed packets can be sent using AES"));

      // Test -> Support
      NDTLogDbg(_T("Test -> Support Directed send"));
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
         NDTLogErr(_T("Test -> Support -Received less than the required amount "));
         NDTLogErr(_T("of directed packets with WLAN Directed send pass Percentage at %d"),g_dwDirectedPasspercentage);
         NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinDirectedPass);
         FlagError(ErrorSevere,&rc);
      }
      else
   	   NDTLogDbg(_T("Directed Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);
   
       // Support -> Test
      NDTLogDbg(_T("Support -> Test Directed send"));
      hr = NDTWlanDirectedSend(ahAdapter[1], ahAdapter[0], cbAddr, ndisMedium,
         ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("DirectedSend failed %s Error:0x%x"),szSsidText,hr);
         FlagError(ErrorSevere,&rc);            
         break;        
      }         

      if (ulPacketsReceived < ulMinDirectedPass)
      {
         NDTLogErr(_T("Support -> Test -Received less than the required amount "));
         NDTLogErr(_T("of directed packets with WLAN Directed send pass Percentage at %d"),g_dwDirectedPasspercentage);
         NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinDirectedPass);
         FlagError(ErrorSevere,&rc);
      }
      else
   	   NDTLogDbg(_T("Directed Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);
   

      // Third Variation
      NDTLogMsg(_T("Variation 3: Verifying broadcast send\\receive"));
      NDTLogMsg(_T("This test will verify broadcast packets can be sent using AES"));

      // Test -> Support
      NDTLogDbg(_T("Test -> Support Broadcast send"));
      ulPacketsSent = 0;
      ulPacketsReceived = 0;
      hr = NDTWlanBroadcastSend(ahAdapter[0], ahAdapter[1], cbAddr, ndisMedium,
                       ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("BroadcastSend failed %s Error:0x%x"),szSsidText,hr);
         FlagError(ErrorSevere,&rc);            
         break;        
      }         

      if (ulPacketsReceived < ulMinBroadcastPass)
      {
         NDTLogErr(_T("Test -> Support -Received less than the required amount of broadcast packets with"));
         NDTLogErr(_T("WLAN Broadcast send pass Percentage at %d%"),WLAN_PERCENT_TO_PASS_BROADCAST);
         NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinBroadcastPass);
         FlagError(ErrorSevere,&rc);
      }
      else
          NDTLogDbg(_T("Broadcast Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);


      // Support -> Test
      NDTLogDbg(_T("Support -> Test BRoadcast send"));
      ulPacketsSent = 0;
      ulPacketsReceived = 0;
      hr = NDTWlanBroadcastSend(ahAdapter[1], ahAdapter[0], cbAddr, ndisMedium,
                       ulPacketsToSend, &ulPacketsSent, &ulPacketsReceived);
      if (NOT_SUCCEEDED(hr))
      {
         NDTLogErr(_T("BroadcastSend failed %s Error:0x%x"),szSsidText,hr);
         FlagError(ErrorSevere,&rc);            
         break;        
      }         

      if (ulPacketsReceived < ulMinBroadcastPass)
      {
         NDTLogErr(_T("Support -> Test -Received less than the required amount of broadcast packets with"));
         NDTLogErr(_T("WLAN Broadcast send pass Percentage at %d%"),WLAN_PERCENT_TO_PASS_BROADCAST);
         NDTLogErr(_T("(Received: %d, Expected: %d)"),ulPacketsReceived, ulMinBroadcastPass);
         FlagError(ErrorSevere,&rc);
      }
      else
         NDTLogDbg(_T("Broadcast Packets ulPacketsToSend %d,ulPacketsSent %d, ulPacketsReceived %d"),ulPacketsToSend , ulPacketsSent, ulPacketsReceived);

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

