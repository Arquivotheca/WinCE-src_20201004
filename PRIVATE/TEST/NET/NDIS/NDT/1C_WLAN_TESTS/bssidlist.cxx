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

typedef struct
{
   NDIS_802_11_SSID ssid;
   BOOL bFlag;
} SsidPair;


BOOL VerifyBssid(NDIS_WLAN_BSSID_EX const* pnAp, const NDIS_802_11_NETWORK_TYPE nNetworkType)
{
   BOOL bResult = TRUE;

   //If this AP is an 802.11b   
   if(Ndis802_11DS == nNetworkType) 
   {
      if (nNetworkType != pnAp->NetworkTypeInUse)
      {
         TCHAR szActualType[20];            
         TCHAR szExpectedType[20];            
         
         GetNetworkTypeText(szActualType,pnAp->NetworkTypeInUse);
         GetNetworkTypeText(szExpectedType, nNetworkType);
         
         NDTLogErr(_T("Invalid value for NetworkTypeInUse (Returned: %s)\n"),szActualType);
         NDTLogErr(_T("Driver must set NDIS_WLAN_BSSID_EX->NetworkTypeInUse to %s\n"),szExpectedType);
         bResult = FALSE;
      }
      
      if (pnAp->Configuration.DSConfig < 2412000 || pnAp->Configuration.DSConfig  > 2484000)
      {
         NDTLogErr(_T("Invalid value for DSConfig. AP must be an 802.11b AP, therefore driver must set \n"));
         NDTLogErr(_T("NDIS_WLAN_BSSID_EX->Configuration.DSConfig to a value between 2,412,000 and 2,484,000\n"));
         bResult = FALSE;
      }      
   }

   if ( Ndis802_11OFDM24 == nNetworkType)
   {
      if (nNetworkType != pnAp->NetworkTypeInUse)
      {
         TCHAR szActualType[20];            
         TCHAR szExpectedType[20];            
         
         GetNetworkTypeText(szActualType,pnAp->NetworkTypeInUse);
         GetNetworkTypeText(szExpectedType, nNetworkType);
         
         NDTLogErr(_T("Invalid value for NetworkTypeInUse (Returned: %s)\n"),szActualType);
         NDTLogErr(_T("Driver must set NDIS_WLAN_BSSID_EX->NetworkTypeInUse to %s\n"),szExpectedType);
         bResult = FALSE;
      }

      if (pnAp->Configuration.DSConfig < 2412000 || pnAp->Configuration.DSConfig  > 2484000)
      {
         NDTLogErr(_T("Invalid value for DSConfig. AP must be an 802.11g AP, therefore driver must set \n"));
         NDTLogErr(_T("NDIS_WLAN_BSSID_EX->Configuration.DSConfig to a value between 2,412,000 and 2,484,000\n"));
         bResult = FALSE;
      }    
   }

   if ( Ndis802_11OFDM5 == nNetworkType)
   {
      if (nNetworkType != pnAp->NetworkTypeInUse)
      {
         TCHAR szActualType[20];            
         TCHAR szExpectedType[20];            
         
         GetNetworkTypeText(szActualType,pnAp->NetworkTypeInUse);
         GetNetworkTypeText(szExpectedType, nNetworkType);
         
         NDTLogErr(_T("Invalid value for NetworkTypeInUse (Returned: %s)\n"),szActualType);
         NDTLogErr(_T("Driver must set NDIS_WLAN_BSSID_EX->NetworkTypeInUse to %s\n"),szExpectedType);
         bResult = FALSE;
      }

      if (pnAp->Configuration.DSConfig < 5000000 || pnAp->Configuration.DSConfig  > 6000000)
      {
         NDTLogErr(_T("Invalid value for DSConfig. AP must be an 802.11a AP, therefore driver must set \n"));
         NDTLogErr(_T("NDIS_WLAN_BSSID_EX->Configuration.DSConfig to a value between 5,000,000 and 6,000,000\n"));
         bResult = FALSE;
      }    
   }
   
   if (1 != pnAp->Privacy)      
   {
      NDTLogErr(_T("Invalid value for Privacy\n"));
      NDTLogErr(_T("AP must have WEP enabled, therefore driver must set NDIS_WLAN_BSSID_EX->Privacy to 1\n"));
      bResult = FALSE;         
   }

   if (pnAp->Rssi > -10 || pnAp->Rssi < -200) 
   {      
      NDTLogErr( _T("Invalid value for Rssi\n"));
      NDTLogErr(_T("Driver must set NDIS_WLAN_BSSID_EX->Rssi to a value between -10 and -200\n"));
      bResult = FALSE;
   }

   if( Ndis802_11Infrastructure != pnAp->InfrastructureMode) 
   {
      NDTLogErr( _T("Invalid value for InfrastructureMode\n"));
      NDTLogErr(_T("AP is an access point, therefore driver must set NDIS_WLAN_BSSID_EX->InfrastructureMode to Ndis802_11Infrastructure\n"));
      bResult = FALSE;
   }      
   return bResult;
}
   

//------------------------------------------------------------------------------

TEST_FUNCTION(TestWlanBssidList)
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
   DWORD dwTimeout;
   UCHAR aWlanAPList[3][33];
   DWORD dwNumSsids;
   DWORD dwNumFound = 0;

   UINT cbUsed;
   UINT cbRequired;
   const UINT iCount = 2;
   const UINT iScanCount = 10 ;

   SsidPair ssids[3];
   PNDIS_WLAN_BSSID_EX pnWLanBssidEx=NULL;
   
   

   NDTLogMsg(
      _T("Start 1c_wlan_bssidlist test on the adapter %s\n"), g_szTestAdapter
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

      switch(WlanDeviceType)
      {
         case WLAN_DEVICE_TYPE_802_11B:
         case WLAN_DEVICE_TYPE_802_11G:
         case WLAN_DEVICE_TYPE_802_11AB:
         case WLAN_DEVICE_TYPE_802_11A:
            dwNumSsids = 2;
            strcpy((char *)aWlanAPList[0],(char *)WLAN_WEP_AP1);
            strcpy((char *)aWlanAPList[1],(char *)WLAN_WEP_AP2);
               break;
         case WLAN_DEVICE_TYPE_802_11AG:
            dwNumSsids = 3;
            strcpy((char *)aWlanAPList[0],(char *)WLAN_WEP_AP1);
            strcpy((char *)aWlanAPList[1],(char *)WLAN_WEP_AP2);
            strcpy((char *)aWlanAPList[2],(char *)WLAN_WEP_AP2);
               break;
         default:
            dwNumSsids = 0;
      }

      for (UINT iy = 0; iy<dwNumSsids; iy++)
      {         
         ssids[iy].ssid.SsidLength = strlen((char*)aWlanAPList[iy]);
         memcpy(ssids[iy].ssid.Ssid,aWlanAPList[iy],ssids[iy].ssid.SsidLength);
         ssids[iy].bFlag = FALSE; 
      }

      //First Variation
      NDTLogMsg(_T("Variation 1: Query OID_802_11_BSSID_LIST with an invalid buffer length\n"));
      NDTLogMsg(_T("his test will query OID_802_11_BSSID_LIST with a buffer that is too short \n"));
      NDTLogMsg(_T("verify one of the correct STATUS codes is returned\n"));

      do
      {         
         hr = NDTWlanSetBssidListScan(hAdapter);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogMsg(_T("Failed to set OID_802_11_BSSID_LIST_SCAN\n"));
         }

         Sleep(WLAN_BSSID_LIST_SCAN_TIMEOUT);

         BYTE abBuffer[103];

         // Query more than one with an invalid buffer length
         for (UINT iy = 0; iy<=9; iy++) 
         {            
            hr = NDTQueryInfo(hAdapter, OID_802_11_BSSID_LIST, abBuffer, sizeof(abBuffer)-1, &cbUsed, &cbRequired);
            if (!(NDT_STATUS_BUFFER_TOO_SHORT == hr)
               && !(NDT_STATUS_INVALID_LENGTH == hr)
               && !(NDT_STATUS_BUFFER_OVERFLOW == hr))
            {
               NDTLogErr(_T("Should have return NDIS_STATUS_BUFFER_TOO_SHORT, INVALID_LENGTH or BUFFER_OVERFLOW\n"));
               NDTLogErr(_T(" Ndis result = 0x%x \n"), NDIS_FROM_HRESULT(hr));
               NDTLogErr(_T("Driver must return a valid NDIS_STATUS when OID_802_11_BSSID_LIST is queried with an invalid buffer length\n"));         
               FlagError(ErrorSevere,&rc);
            }
            else
               NDTLogDbg(_T("PASS: NDTQueryInfo returned %x\n"),NDIS_FROM_HRESULT(hr));               
         }
      }
      while(0);

      // Second Variation
      NDTLogMsg(_T("Variation 2: Verify all required SSID's are visible in list\n"));
      NDTLogMsg(_T("This test will perform a list scan and verify the required AP's are visible in the list\n"));

      do
      {
         UCHAR szText[110];
         UCHAR *ptr = szText;
         for (UINT iy=0; iy<dwNumSsids; iy++)
         {
            memcpy(ptr,ssids[iy].ssid.Ssid,ssids[iy].ssid.SsidLength);
            ptr+= ssids[iy].ssid.SsidLength;
            *ptr = ' ';
            ptr++;
         }
         *ptr = '\0';
            
         NDTLogMsg(_T("Searching for %S in list\n"),szText);      
         NDTWlanReset(hAdapter, TRUE);

         // Query for BSSID list multiple times
         for (UINT iy=0;  iy<iCount; iy++)
         {
               hr = NDTWlanSetBssidListScan(hAdapter);
               if (NOT_SUCCEEDED(hr))
               {
                  NDTLogErr(_T("Failed to set OID_802_11_BSSID_LIST_SCAN\n"));
                  FlagError(ErrorSevere,&rc);
                  break;
               }

               Sleep(WLAN_BSSID_LIST_SCAN_TIMEOUT);

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

               // Loop through the list of bssid and search for each known ssid
               pnWLanBssidEx= &(pnBSSIDListEx->Bssid[0]);
               for (UINT iz=0; iz<(pnBSSIDListEx->NumberOfItems) && bGotBssidList; iz++)
               {                  
                  TCHAR szBssidExText[80];                                       
                  memset(szBssidExText,_T('\0'),sizeof(szBssidExText));
                  GetBssidExText(szBssidExText, *pnWLanBssidEx);
                  NDTLogMsg(_T("%s\n"),szBssidExText);
                  
                  for (UINT iNumSsid=0; iNumSsid<dwNumSsids; iNumSsid++)
                     if (pnWLanBssidEx->Ssid.SsidLength == ssids[iNumSsid].ssid.SsidLength && !memcmp(pnWLanBssidEx->Ssid.Ssid,ssids[iNumSsid].ssid.Ssid,ssids[iNumSsid].ssid.SsidLength))
                     {
                        ssids[iNumSsid].bFlag = TRUE;
                        dwNumFound++;
                     }

                  pnWLanBssidEx = (PNDIS_WLAN_BSSID_EX)((BYTE*)pnWLanBssidEx + (pnWLanBssidEx->Length));
               }

               // If we found all the APs, bail out
               if (dwNumFound == dwNumSsids)
                  break;

               if (pnBSSIDListEx)
                  free(pnBSSIDListEx);
         }

         for (UINT iNumSsid=0; iNumSsid<dwNumSsids; iNumSsid++)         
         {
            GetSsidText(szSsidText, ssids[iNumSsid].ssid);
            if (FALSE == ssids[iNumSsid].bFlag)
            {
               NDTLogErr(_T("Did not find %s in list\n"), szSsidText);
               FlagError(ErrorSevere,&rc);
            }
            else
               NDTLogDbg(_T("PASS: Found %s in List\n"),szSsidText);
         }
      }
      while(0);
         
      // Third Variation
      NDTLogMsg(_T("Variation 3: Verify no list items are returned in list when radio is off\n"));
      NDTLogMsg(_T("This test will set OID_802_11_DISASSOCIATE to turn the radio off, \n"));
      NDTLogMsg(_T("then query the scan list and verify no BSSID's are returned in the list\n"));

      do
      {         
         // Turns radio off   
         hr = NDTWlanDisassociate(hAdapter);
         // Depending on strictness, this is flagged as error
         if (NOT_SUCCEEDED(hr) &&  g_dwStrictness == SevereStrict)
         {
            NDTLogErr(_T("Failed to set OID_802_11_DISASSOCIATE, Error : 0x%x\n"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

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
         
         if (bGotBssidList && 0 != pnBSSIDListEx->NumberOfItems)
         {
            NDTLogErr(_T("NDIS_802_11_BSSID_LIST_EX->NumberOfItems should be 0 when radio is off (Returned: %d, Expected = 0)\n"), pnBSSIDListEx->NumberOfItems);
            NDTLogErr(_T("Driver must set NDIS_802_11_BSSID_LIST_EX->NumberOfItems to 0 when a scan is performed while the radio is off\n"));
            FlagError(ErrorMild,&rc);
         }          
         else
            NDTLogDbg(_T("PASS: NDIS_802_11_BSSID_LIST_EX->NumberOfItems was returned 0"));

         if (pnBSSIDListEx)
            free (pnBSSIDListEx);

         // To turn radio back on
         NDTWlanSetDummySsid(hAdapter);

      }
      while(0);

      //Fourth Variation      
      NDTLogMsg(_T("Variation 4: SKIPPED as no WDM functionality in CE - Perform list scan and unload\\load driver\n"));
      NDTLogMsg(_T("This test will start a list scan and unload the driver while the scan is in progress \n"));
      NDTLogMsg(_T(" then it will load the driver an perform an list scan and verify at least one item is returned\n"));


      // Fifth Variation
      NDTLogMsg(_T("Variation 5: Verify the currently associated SSID is returned in list\n"));
      NDTLogMsg(_T("This test will will associate with an access point then query the list and verify the \n")); 
      NDTLogMsg(_T("SSID\\BSSID of the associated access point it returned in the list\n"));
      do
      {         
         NDTWlanReset(hAdapter,TRUE);

         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         dwTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&dwTimeout);         
        GetSsidText(szSsidText, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x\n"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
         
         hr = NDTWlanSetBssidListScan(hAdapter);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_BSSID_LIST_SCAN\n"));
            FlagError(ErrorSevere,&rc);
            break;
         }

         Sleep(WLAN_BSSID_LIST_SCAN_TIMEOUT);

         BOOL bFoundBssid = FALSE;
         BYTE abTemp[6]; 

         hr = NDTWlanGetBssid(hAdapter, &abTemp);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to query OID_802_11_BSSID\n"));
            FlagError(ErrorSevere,&rc);
            break;
         }


         DWORD dwSize = 2048;            
         PNDIS_802_11_BSSID_LIST_EX pnBSSIDListEx = (PNDIS_802_11_BSSID_LIST_EX) malloc(dwSize);
         UINT retries = 20;
         BOOL bGotBssidList = FALSE;
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

         // Loop through the list of bssid and search for each known ssid
         pnWLanBssidEx= &(pnBSSIDListEx->Bssid[0]);
         for (UINT iz=0; iz<(pnBSSIDListEx->NumberOfItems) && bGotBssidList; iz++)
         {                  
            TCHAR szBssidExText[80];                                       
            memset(szBssidExText,_T('\0'),sizeof(szBssidExText));
            GetBssidExText(szBssidExText, *pnWLanBssidEx);
            NDTLogMsg(_T("%s"),szBssidExText);
            if ( !memcmp(abTemp, pnWLanBssidEx->MacAddress, 6) 
               && ( pnWLanBssidEx->Ssid.SsidLength == ssidList[ix].SsidLength)
               && !memcmp(pnWLanBssidEx->Ssid.Ssid, ssidList[ix].Ssid, ssidList[ix].SsidLength)
               )
            {               
                bFoundBssid = TRUE;
                break;
            }          
            pnWLanBssidEx = (PNDIS_WLAN_BSSID_EX)((BYTE*)pnWLanBssidEx + (pnWLanBssidEx->Length));
         }

         if (FALSE == bFoundBssid)
         {
            NDTLogErr(_T("Did not find currently associated AP in list\n"));
            NDTLogErr(_T("Driver must return the BSSID\\SSID of the currently associated access point in the list\n"));
            FlagError(ErrorSevere,&rc);
         }
         else
            NDTLogDbg(_T("PASS: Found bssid\\ssid of the currently associated access point"));

         if (pnBSSIDListEx)
            free(pnBSSIDListEx);
      }      
      while(0);


         
       // Sixth Variation
       NDTLogMsg(_T("Variation 6: Verify device does not disconnect while performing a list scan\n"));
       NDTLogMsg(_T("This test will perform several list scans while associated and verify the device does not disconnect\n"));
        
      do
      {
         NDTWlanReset(hAdapter, TRUE);

         ULONG ulStatusIndices[4] = {NDT_COUNTER_STATUS_RESET_START, NDT_COUNTER_STATUS_RESET_START, 
            NDT_COUNTER_STATUS_MEDIA_DISCONNECT, NDT_COUNTER_STATUS_MEDIA_CONNECT};
         ULONG ulArrStatus[4] ;

         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         dwTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&dwTimeout);         

         GetSsidText(szSsidText, ssidList[ix]);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x\n"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }

         NDTClearCounters(hAdapter);

         for (UINT iIndex=0; iIndex<sizeof(ulStatusIndices)/sizeof(ulStatusIndices[0]); iIndex++)
         {
            hr = NDTGetCounter(hAdapter, ulStatusIndices[iIndex], &ulArrStatus[iIndex]);
            if (FAILED(hr)) {
         	   NDTLogErr(g_szFailGetCounter, hr);
                 FlagError(ErrorSevere,&rc);
         	   break;
        	   }
         }
         NDTLogDbg(_T("Connects = %d, Disconnects = %d, Resets = %d"),
               ulArrStatus[3],ulArrStatus[2],ulArrStatus[0]);
         if (ulArrStatus[3] != 0 ||ulArrStatus[2] != 0 || ulArrStatus[0] != 0)
         {
            NDTLogWrn(_T("Connects %d Disconnects %d and Resets %d not Reset to zero"), ulArrStatus[3],ulArrStatus[2],ulArrStatus[0]);
         }

         for (UINT iy=0; iy<iScanCount ;iy++)
         {
            hr = NDTWlanSetBssidListScan(hAdapter);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to set OID_802_11_BSSID_LIST_SCAN\n"));
               FlagError(ErrorSevere,&rc);
               break;
            }

            Sleep(WLAN_BSSID_LIST_SCAN_TIMEOUT);
         
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

              for (UINT iIndex=0; iIndex<sizeof(ulStatusIndices)/sizeof(ulStatusIndices[0]); iIndex++)
               {
                  hr = NDTGetCounter(hAdapter, ulStatusIndices[iIndex], &ulArrStatus[iIndex]);
                  if (FAILED(hr)) {
               	   NDTLogErr(g_szFailGetCounter, hr);
                       FlagError(ErrorSevere,&rc);
               	   break;
              	   }
                  NDTLogDbg(_T("Connects = %d, Disconnects = %d, Resets = %d"),ulArrStatus[3],ulArrStatus[2],ulArrStatus[0]);               
              }
            }            
            while(--retries >0);
            if (retries <= 0)
            {
               NDTLogErr(_T("Failed to get OID_802_11_BSSID_LIST_SCAN after multiple attempts Error =0x%x\n"), hr);        
               FlagError(ErrorSevere,&rc);
               break;
            }
            if (pnBSSIDListEx)
               free(pnBSSIDListEx);
         }

         hr = NDTGetCounter(hAdapter, ulStatusIndices[2], &ulArrStatus[2]);
         if (FAILED(hr)) {
      	   NDTLogErr(g_szFailGetCounter, hr);
              FlagError(ErrorSevere,&rc);
      	   break;
     	   }

         // CHeck disconnects
         if (0 != ulArrStatus[2])
         {
            NDTLogErr(_T("Incorrect number of disconnect events indicated (Received: %d, Expected: 0\n"), ulArrStatus[2]);
            FlagError(ErrorSevere,&rc);
         }
         else
            NDTLogDbg(_T("PASS: Zero disconnect events indicated"));

      }
      while(0);


      // Seventh Variation
      NDTLogMsg(_T("Variation 7:Do NdisReset while performing a list scan\n"));
      NDTLogMsg(_T("This test will start a list scan issue an NdisReset and verify the scan completes successfully\n"));

      do
      {
         hr = NDTWlanSetBssidListScan(hAdapter);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_BSSID_LIST_SCAN\n"));
            FlagError(ErrorSevere,&rc);
            break;
         }

         NDTReset(hAdapter);
         
         Sleep(WLAN_BSSID_LIST_SCAN_TIMEOUT);

         DWORD dwSize = 2048;            
         PNDIS_802_11_BSSID_LIST_EX pnBSSIDListEx = (PNDIS_802_11_BSSID_LIST_EX) malloc(dwSize);
         BOOL bGotBssidList= FALSE;
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
         else
            NDTLogDbg(_T("PASS: Succesfully got Bssid list"));

         if (pnBSSIDListEx)
            free(pnBSSIDListEx);
      }
      while (0);
      

      // Eight Variation
      GetSsidText(szSsidText, ssidList[ix]);
      
      NDTLogMsg(_T("Variation 8: Associate while a scan is in progress\n"));
      NDTLogMsg(_T("This test will associate with %s while a scan is in progress and verify the request succeeds\n"),szSsidText);

      do
      {
         NDTWlanReset(hAdapter, TRUE);

         hr = NDTWlanSetBssidListScan(hAdapter);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_BSSID_LIST_SCAN\n"));
            FlagError(ErrorSevere,&rc);
            break;
         }

         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         dwTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&dwTimeout);         
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x\n"),szSsidText,hr);
            NDTLogErr(_T("Driver must be able to associate while a scan is in progress\n"));
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("PASS: Succesfully associated while a scan was in progress"));
         
      }
      while(0);



      // Ninth Variation
      
      NDTLogMsg(_T("Variation 9: Verify NDIS_WLAN_BSSID_EX fields for each access point\n"));
      NDTLogMsg( _T("This test will verify the data fields for each access point in the environment are set correctly \n"));

      do
      {
         PNDIS_WLAN_BSSID_EX  pnAp1= NULL;
         PNDIS_WLAN_BSSID_EX  pnAp2= NULL;
         PNDIS_WLAN_BSSID_EX  pnAp3= NULL;
         PNDIS_WLAN_BSSID_EX  pnAp4= NULL;

         for(UINT iy=0; iy<iScanCount; iy++)
         {
            hr = NDTWlanSetBssidListScan(hAdapter);
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to set OID_802_11_BSSID_LIST_SCAN\n"));
               FlagError(ErrorSevere,&rc);
               break;
            } 

            Sleep(WLAN_BSSID_LIST_SCAN_TIMEOUT);

            DWORD dwSize = 2048;            
            PNDIS_802_11_BSSID_LIST_EX pnBSSIDListEx = (PNDIS_802_11_BSSID_LIST_EX) malloc(dwSize);
            BOOL bGotBssidList=FALSE;
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

            // Loop through the list of bssid and search for each known ssid      
            pnWLanBssidEx= &(pnBSSIDListEx->Bssid[0]);
            NDTLogMsg(_T("Total BSSID List size: %d"),pnBSSIDListEx->NumberOfItems);
            for (UINT iz=0; iz<(pnBSSIDListEx->NumberOfItems) && bGotBssidList; iz++)
            {               
               TCHAR szBssidExText[80];           
               memset(szBssidExText,_T('\0'),sizeof(szBssidExText));
               GetBssidExText(szBssidExText, *pnWLanBssidEx);
               NDTLogMsg(_T("%s"),szBssidExText);
               
               if ( pnWLanBssidEx->Ssid.SsidLength == strlen((char *)WLAN_WEP_AP1) 
                     && !(memcmp(WLAN_WEP_AP1, pnWLanBssidEx->Ssid.Ssid,pnWLanBssidEx->Ssid.SsidLength)) 
                  )                   
               { 
                  pnAp1 = (PNDIS_WLAN_BSSID_EX)malloc(pnWLanBssidEx->Length);
                  memcpy(pnAp1,pnWLanBssidEx,pnWLanBssidEx->Length);
               }
               if ( pnWLanBssidEx->Ssid.SsidLength == strlen((char *)WLAN_WEP_AP2) 
                     && !(memcmp(WLAN_WEP_AP2, pnWLanBssidEx->Ssid.Ssid,pnWLanBssidEx->Ssid.SsidLength)) 
                  )                   
               {               
                  pnAp2 = (PNDIS_WLAN_BSSID_EX)malloc(pnWLanBssidEx->Length);
                  memcpy(pnAp2,pnWLanBssidEx,pnWLanBssidEx->Length);
               }
               if ( pnWLanBssidEx->Ssid.SsidLength == strlen((char *)WLAN_WEP_AP3) 
                     && !(memcmp(WLAN_WEP_AP3, pnWLanBssidEx->Ssid.Ssid,pnWLanBssidEx->Ssid.SsidLength)) 
                  )                   
               {               
                  pnAp3 = (PNDIS_WLAN_BSSID_EX)malloc(pnWLanBssidEx->Length);
                  memcpy(pnAp3,pnWLanBssidEx,pnWLanBssidEx->Length);
               }               
               if ( pnWLanBssidEx->Ssid.SsidLength == strlen((char *)WLAN_WPA_AP1) 
                     && !(memcmp(WLAN_WPA_AP1, pnWLanBssidEx->Ssid.Ssid,pnWLanBssidEx->Ssid.SsidLength)) 
                  )                   
               {               
                  pnAp4 = (PNDIS_WLAN_BSSID_EX)malloc(pnWLanBssidEx->Length);
                  memcpy(pnAp4,pnWLanBssidEx,pnWLanBssidEx->Length);
               }                              
               pnWLanBssidEx = (PNDIS_WLAN_BSSID_EX)((BYTE*)pnWLanBssidEx + (pnWLanBssidEx->Length));
            }

            if (pnBSSIDListEx)
               free(pnBSSIDListEx);
         }
         
         if (NULL == pnAp1)
         {
            NDTLogErr(_T("Did not find %S (AP1) in list\n"), WLAN_WEP_AP1);
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
            NDTLogDbg(_T("PASS: Found %S (AP1) in list\n"), WLAN_WEP_AP1);

         if (NULL == pnAp2)
         {
            NDTLogErr(_T("Did not find %S (AP2) in list\n"), WLAN_WEP_AP2);
            FlagError(ErrorSevere,&rc);
            break;
         }
         else
             NDTLogDbg(_T("PASS: Found %S (AP2) in list\n"), WLAN_WEP_AP2);
         
         //If the test device is 802.11a/g there there must be a third WEP AP in the environment, verify it was fount in the list
         if (WLAN_DEVICE_TYPE_802_11AG == WlanDeviceType)
         {
            NDTLogMsg(_T("As device is of type a/g checking for %S (AP3)"),WLAN_WEP_AP3);
            if (NULL == pnAp3)
            {
               NDTLogErr(_T("Did not find %S (AP3) in list\n"), WLAN_WEP_AP3);
               FlagError(ErrorSevere,&rc);
               break;
            }
            else
               NDTLogDbg(_T("PASS: Found %S (AP3) in list\n"), WLAN_WEP_AP3);
         }

         // If the test device supports WPA verify the WPA access point (AP4) was fount in the list
         BOOL bWPASupport;
         NDTWlanIsWPASupported(hAdapter, &bWPASupport);
                  
         if (bWPASupport)
         {
            NDTLogMsg(_T("Verifying WPA AP is found in the list as test device supports WPA"));
            if (NULL == pnAp4)
            {
               NDTLogErr(_T("Did not find %S (AP4) in list\n"), WLAN_WPA_AP1);
               FlagError(ErrorSevere,&rc);
               break;
            }
            else
               NDTLogDbg(_T("PASS: Found %S (AP4) in list"), WLAN_WPA_AP1);
         }


         // Tenth Variation         
         //Verify bssid fields for AP1 are set correctly
         NDTLogMsg(_T("Variation 10: Verify BSSID fields for %S are set correctly\n"),WLAN_WEP_AP1);
         if ( WLAN_DEVICE_TYPE_802_11B == WlanDeviceType)
         {
            if (!VerifyBssid(pnAp1, Ndis802_11DS))
               FlagError(ErrorSevere,&rc);
            else
               NDTLogDbg(_T("PASS: Verified BSSID fields for %S"), WLAN_WEP_AP1);
         }
         else if ( WLAN_DEVICE_TYPE_802_11A == WlanDeviceType)
         {
            if (!VerifyBssid(pnAp1, Ndis802_11OFDM5))
               FlagError(ErrorSevere,&rc);
            else
               NDTLogDbg(_T("PASS: Verified BSSID fields for %S"), WLAN_WEP_AP1);

         }
         else
            NDTLogMsg(_T("Skipping variation 10 as test device is not of type 802.11b or 802.11a"));

         // Eleventh Variation
         //Verify bssid fields for AP2 are set correctly
        NDTLogMsg(_T("Variation 11: Verify BSSID fields for AP2 are set correctly\n"));         
         if ( WLAN_DEVICE_TYPE_802_11B == WlanDeviceType)
         { 
            if (!VerifyBssid(pnAp2, Ndis802_11DS))
               FlagError(ErrorSevere,&rc);
            else
               NDTLogDbg(_T("PASS: Verified BSSID fields for AP2"));           
         }
         else if ( WLAN_DEVICE_TYPE_802_11A == WlanDeviceType)
         {
            if (!VerifyBssid(pnAp2, Ndis802_11OFDM5))
               FlagError(ErrorSevere,&rc);
            else
               NDTLogDbg(_T("PASS: Verified BSSID fields for AP2"));           
         }
         else if ( WLAN_DEVICE_TYPE_802_11G == WlanDeviceType)
         {
            if (!VerifyBssid(pnAp2, Ndis802_11OFDM24))
               FlagError(ErrorSevere,&rc);
            else
               NDTLogDbg(_T("PASS: Verified BSSID fields for AP2"));           
         }
         else if ( WLAN_DEVICE_TYPE_802_11AB == WlanDeviceType)
         {
            if (!VerifyBssid(pnAp2, Ndis802_11OFDM5))
               FlagError(ErrorSevere,&rc);
            else
               NDTLogDbg(_T("PASS: Verified BSSID fields for AP2"));           
         }
         else if ( WLAN_DEVICE_TYPE_802_11AG == WlanDeviceType)
         {
            if (!VerifyBssid(pnAp2, Ndis802_11OFDM5))
               FlagError(ErrorSevere,&rc);
            else
               NDTLogDbg(_T("PASS: Verified BSSID fields for AP2"));           
         }

          if ( WLAN_DEVICE_TYPE_802_11AG == WlanDeviceType)
          {
            // Twelfth Variation
             NDTLogMsg(_T("Variation 12:Verify BSSID for AG contains an information element as device is of type AG\n"));
             if (!VerifyBssid(pnAp3, Ndis802_11OFDM24))
               FlagError(ErrorSevere,&rc);
            else
               NDTLogDbg(_T("PASS: Verified BSSID fields for AP3"));                        
          }
            NDTLogMsg(_T("Skipping Variation 12 as test device is not of type 802_11AG"));

          //IF WPA is supported
          if (bWPASupport)
          {
            // Thirteenth Variation
            NDTLogMsg(_T("Variation 13:Verify BSSID for %S contains an information element\n"),WLAN_WPA_AP1);
        
            if (0 >= pnAp4->IELength)
            {
               NDTLogErr(_T("IELength should not be zero\n"));
         	     NDTLogErr(_T("BSSID for %S must contain a WPA information element\n"),WLAN_WPA_AP1);
                FlagError(ErrorSevere,&rc);
            }

            //Loop through each of the information elements searching for the MSFT WPA IE
            
            BOOL bFoundWPAIE = FALSE;
            PNDIS_802_11_FIXED_IEs pnFixedIes = (PNDIS_802_11_FIXED_IEs) pnAp4->IEs;
            PNDIS_802_11_VARIABLE_IEs pnVariableIes;

            pnVariableIes = (PNDIS_802_11_VARIABLE_IEs) (pnFixedIes + 1);
            while((void *)pnVariableIes< ((void *)&(pnAp4->IEs[pnAp4->IELength])))
            {
               if (WLAN_IE_WPA == pnVariableIes->ElementID  )
               {
                  if ( 0x00 == pnVariableIes->data[0] && 0x50 == pnVariableIes->data[1]  && 0xF2 == pnVariableIes->data[2] )
                     bFoundWPAIE = TRUE;
               }               
               pnVariableIes= (PNDIS_802_11_VARIABLE_IEs)(&(pnVariableIes->data[pnVariableIes->Length]));            
            }

            if (FALSE == bFoundWPAIE)
            {
               NDTLogErr(_T("Failed to find MSFT WPA Information Element in BSSID\n"));
               FlagError(ErrorSevere,&rc);
            }
            else
               NDTLogDbg(_T("PASS: Found MSFT WPA Information Element in BSSID\n"));
            
          }
          else
            NDTLogMsg(_T("Skipping Variation 13 as adapter does not support WPA\n"));

         if (pnAp1)
            free(pnAp1);         
         if (pnAp2)
            free(pnAp2);
         if (pnAp3)
            free(pnAp3);
         if (pnAp4)
            free(pnAp4);
          
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

