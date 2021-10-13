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

TEST_FUNCTION(TestWlanMediaevents)
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
   NDIS_802_11_SSID nSupportSsid;

   ULONG ulArrStatus[2];
   ULONG ulStatusIndices[] = { NDT_COUNTER_STATUS_MEDIA_CONNECT, NDT_COUNTER_STATUS_MEDIA_DISCONNECT };

   
   

   NDTLogMsg(
      _T("Start 1c_wlan_mediaevents test on the adapter %s\n"), g_szTestAdapter
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
      NDTLogMsg(_T("Variation 1: Verify a connect event is indicated after association. This test will associate with %s\n"),szSsidText);
      NDTLogMsg(_T("and then verify a connect event is indicated\n"));

      do 
      {
         NDTWlanReset(hAdapter, TRUE);
         //Continue anyway on error

         hStatus = NULL;
         hr = NDTStatus(hAdapter, NDIS_STATUS_MEDIA_CONNECT, &hStatus);
         if (FAILED(hr))
         {
            NDTLogErr(_T("Starting wait for connect on %s failed Error:0x%x\n"),g_szTestAdapter, hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

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
         else
            NDTLogDbg(_T("Succesfully associated with %s"), szSsidText);

         // Expect MEDIA_CONNECT within 10 seconds
         hr = NDTStatusWait(hAdapter, hStatus, 10000);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("No connect event indicated Error : 0x%x\n"), NDIS_FROM_HRESULT(hr));
            NDTLogMsg(_T("Driver must indicate a connect event after a successful association\n"));
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("PASS: Media connect event indicated"));
      }
      while (0);


      // Second variation
      NDTLogMsg(_T("Variation 2: Verify media connect events are indicated when associating with the same AP\\SSID\n"));
      NDTLogMsg(_T("This test will associate the device with an AP\\SSID and then associate again to the same AP\\SSID\n"));
      NDTLogMsg(_T(" and verify a connect event is indicated\n"));

      do
      {
         NDTWlanReset(hAdapter, TRUE);         

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
         else
            NDTLogDbg(_T("Succesfully associated with %s"), szSsidText);


         hStatus = NULL;
         hr = NDTStatus(hAdapter, NDIS_STATUS_MEDIA_CONNECT, &hStatus);
         if (FAILED(hr))
         {
            NDTLogErr(_T("Starting wait for connect on %s failed Error:0x%x\n"),g_szTestAdapter, hr);
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
            FlagError(ErrorSevere,&rc);            
            break;
         }

         // Expect MEDIA_CONNECT within 10 seconds
         hr = NDTStatusWait(hAdapter, hStatus, 10000);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("No connect event indicated Error : 0x%x\n"), NDIS_FROM_HRESULT(hr));
            NDTLogMsg(_T("Driver must indicate a connect event when associating with the same AP\\SSID\n"));
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
             NDTLogDbg(_T("PASS: Media connect event indicated"));
      }
      while(0);

      
      // Third Variation
      NDTLogMsg(_T("Variation 3: Verify media disconnect & connect event are indicated when associating with different AP\\SSID\n"));
      NDTLogMsg(_T("This test will assoicate with an AP\\SSID then associate with a different AP\\SSID and verify a\n"));
      NDTLogMsg(_T(" disconnect and connect events are indicated\n"));
      
      do{
         if (!memcmp(szSsidText, WLAN_WEP_AP1,strlen((char *)WLAN_WEP_AP1)))
         {
            // If the supportSsid and the ssid are the same, change the support ssid
            nSupportSsid.SsidLength = strlen((char *)WLAN_WEP_AP2);
            memcpy(nSupportSsid.Ssid,WLAN_WEP_AP2,nSupportSsid.SsidLength);
         }
         else
         {
            nSupportSsid.SsidLength = strlen((char *)WLAN_WEP_AP1);
            memcpy(nSupportSsid.Ssid,WLAN_WEP_AP1,nSupportSsid.SsidLength);
         }

         NDTWlanReset(hAdapter, TRUE);
        
         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         dwTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, nSupportSsid, FALSE ,&dwTimeout);

         GetSsidText(szSsidText, nSupportSsid);               
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x\n"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("Succesfully associated with %s"), szSsidText);


         hr = NDTClearCounters(hAdapter);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to clear counters Error:0x%x\n"),hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }

         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         dwTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&dwTimeout);

         // Get events counter
         NDTLogMsg(_T("Reading Status/Event counters for miniport adapter %s.\n"), g_szTestAdapter);
         for (UINT iy=0;iy<2;++iy)
         {
      	   hr = NDTGetCounter(hAdapter, ulStatusIndices[iy], &ulArrStatus[iy]);
      	   if (FAILED(hr)) {
      		   NDTLogErr(g_szFailGetCounter, hr);
                   FlagError(ErrorSevere,&rc);
      		   break;
      	   }
         }

         // Check connects
         if ( 1 != ulArrStatus[0])
         {
            NDTLogErr(_T("Incorrect number of connect events indicated (Received: %d, Expected: 1\n"), ulArrStatus[0]);
            FlagError(ErrorSevere,&rc);
         }
         else
            NDTLogDbg(_T("PASS: 1 Connect event"));

         if ( 1 != ulArrStatus[1])
         {
            NDTLogErr(_T("Incorrect number of disconnect events indicated (Received: %d, Expected: 1\n"), ulArrStatus[1]);
            FlagError(ErrorSevere,&rc);
         }
         else
            NDTLogDbg(_T("PASS: 1 Disconnect event"));
                     
      }     
      while(0);


      // Fourth Variation
      GetSsidText(szSsidText, ssidList[ix]);
         
      NDTLogMsg(_T("Variation 4: Verify a disconnect event is indicated after disassociating\n"));
       NDTLogMsg(_T("This test will associate with %s then set OID_802_11_DISASSOCIATE and verify a disconnect event is indicated\n"),szSsidText);

      do
      {        
         NDTWlanReset(hAdapter, TRUE);

         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         dwTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, nSupportSsid, FALSE ,&dwTimeout);

         GetSsidText(szSsidText, ssidList[ix]);         
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with %s Error:0x%x\n"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }
         else
            NDTLogDbg(_T("Succesfully associated with %s"),szSsidText);


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
            NDTLogErr(_T("Failed to set OID_802_11_DISASSOCIATE, Error : 0x%x\n"),hr);
            FlagError(ErrorSevere,&rc);
            break;
         }

         // Expect DISCONNECT notification in 12 seconds
         hr = NDTStatusWait(hAdapter, hStatus, 12000);
         if (NOT_SUCCEEDED(hr)) {
            NDTLogErr(_T("No disconnect event indicated  Error : 0x%x\n"), NDIS_FROM_HRESULT(hr));
            NDTLogMsg(_T("Driver must indicate a disconnect event after disassociating from an AP\n"));
            FlagError(ErrorSevere,&rc);
            break;
         }       
         else
            NDTLogDbg(_T("PASS: Disconnect event notified"));
         
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

