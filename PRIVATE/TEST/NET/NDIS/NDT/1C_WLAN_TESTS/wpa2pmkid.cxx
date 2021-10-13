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

TEST_FUNCTION(TestWpa2Pmkid)
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

   DWORD dwSize = 0;
   UINT cbUsed = 0;
   UINT cbRequired = 0;
   ULONG ulTimeout = 0;
   DWORD dwEncryption = 0;
   PNDIS_802_11_PMKID  pnPMKIDQueryList = NULL;
   NDIS_802_11_PMKID aPMKIDList;
   PNDIS_802_11_PMKID pnPMKIDList = NULL;

   HANDLE hStatus = NULL;
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;

   BOOL bWPA2Supported = FALSE;
   
   DWORD dwActualAuthMode= 0;           
   
   UCHAR aMac[6]={0xAA,0xBC,0xCC,0xCD,0xAE,0xAF};         
   UCHAR aPMKID1[16]={2,1,2,1,7,1,4,5,3,1,9,1,1,1,3,1};

   UCHAR aPMKID2[16]={2,1,2,1,7,1,1,5,1,1,9,1,1,1,3,1};
 

   NDTLogMsg(
      _T("Start 1c_wpa2_pmkid test on the adapter %s\n"), g_szTestAdapter
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
      NDTLogWrn(_T("Test adapter %s does not support WPA2, aborting test\n"), g_szTestAdapter);
      rc = TPR_SKIP;
      goto cleanUp;
   }

   // Set to the right Auth mode first
   hr = NDTWlanSetAuthenticationMode(hAdapter, Ndis802_11AuthModeWPA2);
   if (NOT_SUCCEEDED(hr))
   {
       NDTLogErr(_T("Failed to set OID_802_11_AUTHENTICATION_MODE to Ndis802_11AuthModeWPA2 Error0x:%x"),NDIS_FROM_HRESULT(hr));
       FlagError(ErrorSevere,&rc);
       goto cleanUp;
   }         

  
  // First Variation
  NDTLogMsg(_T("Variation 1: Set OID with invalid buffer length"));
  NDTLogMsg(_T("This test case will set OID_802_11_PMKID with an invalid buffer size"));
  NDTLogMsg(_T("and verify NDIS_STATUS_INVALID_LENGTH is returned"));

  
  BYTE abBuffer[1];
  abBuffer[0] = 0;
  hr = NDTSetInfo(hAdapter, OID_802_11_PMKID, abBuffer, sizeof(abBuffer) - 1, &cbUsed, &cbRequired);
  if ( NDIS_STATUS_INVALID_LENGTH != NDIS_FROM_HRESULT(hr) && NDIS_STATUS_BUFFER_TOO_SHORT != NDIS_FROM_HRESULT(hr))
  {
     NDTLogErr(_T("Should have return NDIS_STATUS_INVALID_LENGTH or NDIS_STATUS_BUFFER_TOO_SHORT \n"));
     NDTLogErr(_T("Instead returned %x while trying to set OID_802_11_PMKID"),NDIS_FROM_HRESULT(hr));
     FlagError(ErrorSevere,&rc);         
  }

  // Second Variation
  NDTLogMsg(_T("Variation 2: Unload\\Load driver and query OID_802_11_PKMID"));
  NDTLogMsg(_T("This test will unload\\load the driver, query OID_802_11_PKMID and verify"));
  NDTLogMsg(_T("no PMKID's are returned in the list"));
  
  NDTLogMsg(_T("Skipped until we find a way to load unload driver"));
  
  ULONG ulPmkidCount = 1;
     
  // Third Variation
  NDTLogMsg(_T("Variation 3: Set\\Query OID_802_11_PKMID and verify"));
  NDTLogMsg(_T("This test will set OID_802_11_PKMID with valid PMKID data, then query and"));
  NDTLogMsg(_T("verify the data returned matches the data previously set"));

  do
  {
     memcpy(aPMKIDList.BSSIDInfo[0].BSSID, aMac, sizeof(aPMKIDList.BSSIDInfo[0].BSSID));
     memcpy(aPMKIDList.BSSIDInfo[0].PMKID, aPMKID2, sizeof(aPMKIDList.BSSIDInfo[0].PMKID));

     aPMKIDList.BSSIDInfoCount = ulPmkidCount;
     aPMKIDList.Length = FIELD_OFFSET(NDIS_802_11_PMKID, BSSIDInfo) +(aPMKIDList.BSSIDInfoCount * sizeof(BSSIDInfo));

     dwSize = aPMKIDList.Length;
        
     hr = NDTWlanSetPMKID(hAdapter, &aPMKIDList, dwSize);
     if (NOT_SUCCEEDED(hr))
     {
        NDTLogErr(_T("Unable to set PMKID Error0x:%x"),NDIS_FROM_HRESULT(hr));
        NDTLogErr(_T("WPA2 drivers must support OID_802_11_PMKID"));
        FlagError(ErrorSevere,&rc);
        break;
     }

     dwSize = sizeof(NDIS_802_11_PMKID);
     pnPMKIDQueryList = (PNDIS_802_11_PMKID) malloc(dwSize);
     if (!pnPMKIDQueryList )
     {
        NDTLogErr(_T("unable to allocate memory"));
        FlagError(ErrorSevere,&rc);
        break;
     }
     
     hr = NDTWlanGetPMKID(hAdapter, pnPMKIDQueryList, &dwSize);

     if (NOT_SUCCEEDED(hr) && hr == NDT_STATUS_BUFFER_TOO_SHORT)
     {
        free(pnPMKIDQueryList);
        pnPMKIDQueryList = (PNDIS_802_11_PMKID) malloc(dwSize);
        if (!pnPMKIDQueryList )
        {
          NDTLogErr(_T("unable to allocate memory"));
          FlagError(ErrorSevere,&rc);
          break;
        }
        hr = NDTWlanGetPMKID(hAdapter, pnPMKIDQueryList , &dwSize);                     
     }
     if (NOT_SUCCEEDED(hr))
     {
        NDTLogErr(_T("Failed to get PMKID list Error:0x%x"),NDIS_FROM_HRESULT(hr));
        NDTLogErr(_T("WPA2 drivers must support OID_802_11_PMKID query request"));
        FlagError(ErrorSevere,&rc);
        break;
     }

     // Verify the info field
     if (ulPmkidCount != pnPMKIDQueryList ->BSSIDInfoCount)
     {
        NDTLogErr(_T("Incorrect value returned for BSSIDInfoCount (Returned %d Expected %d)")
           , pnPMKIDQueryList ->BSSIDInfoCount, ulPmkidCount);
        FlagError(ErrorSevere,&rc);
        break;
     }
     else
        NDTLogDbg(_T("Correct value %d returned for BSSIDInfoCount"),pnPMKIDQueryList ->BSSIDInfoCount);


    if (memcmp(pnPMKIDQueryList->BSSIDInfo[0].BSSID, aMac, sizeof(pnPMKIDQueryList->BSSIDInfo[0].BSSID)))
    {
       NDTLogErr(_T("Incorrect BSSIDInfo value for BSSID returned"));
       NDTLogErr(_T("The value returned for BSSID does not match the value previously set"));
       FlagError(ErrorSevere,&rc);
       break;
        
    }

    if (memcmp(pnPMKIDQueryList->BSSIDInfo[0].PMKID, aPMKID2, sizeof(pnPMKIDQueryList->BSSIDInfo[0].PMKID)))
    {
       NDTLogErr(_T("Incorrect BSSIDInfo value for PMKID returned"));
       NDTLogErr(_T("The value returned for PMKID does not match the value previously set"));
       FlagError(ErrorSevere,&rc);
    }      

  }
  while(0); 

  if (pnPMKIDQueryList)
  {
    free(pnPMKIDQueryList);
    pnPMKIDQueryList = NULL;
  }

  // Fourth Variation
  NDTLogMsg(_T("Variation 4: Verify PMKID list is cleared when BSSIDInfoCount is set to zero"));
  NDTLogMsg(_T("set the OID again with BSSIDInfoCount equal to zero, finally it will query"));
  NDTLogMsg(_T("& verify BSSIDInfoCount is zero on the return"));

  do
  {
     
     ulPmkidCount = 0;
     memset(&aPMKIDList,0,sizeof(aPMKIDList));
     aPMKIDList.BSSIDInfoCount = ulPmkidCount;
     aPMKIDList.Length = FIELD_OFFSET(NDIS_802_11_PMKID, BSSIDInfo) +(aPMKIDList.BSSIDInfoCount * sizeof(BSSIDInfo));
     
     
     hr = NDTWlanSetPMKID(hAdapter, &aPMKIDList, dwSize);
     if (NOT_SUCCEEDED(hr))
     {
        NDTLogErr(_T("Unable to set PMKID Error0x:%x"),NDIS_FROM_HRESULT(hr));
        NDTLogErr(_T("WPA2 drivers must support OID_802_11_PMKID"));
        FlagError(ErrorSevere,&rc);
        break;
     }

     dwSize = sizeof(NDIS_802_11_PMKID);
     pnPMKIDQueryList = (PNDIS_802_11_PMKID) malloc(dwSize);
     if (!pnPMKIDQueryList)
     {
        NDTLogErr(_T("unable to allocate memory"));
        FlagError(ErrorSevere,&rc);
        break;
     }
     hr = NDTWlanGetPMKID(hAdapter, pnPMKIDQueryList, &dwSize);

     if (NOT_SUCCEEDED(hr) && hr == NDT_STATUS_BUFFER_TOO_SHORT)
     {
        free(pnPMKIDQueryList);
        pnPMKIDQueryList = (PNDIS_802_11_PMKID) malloc(dwSize);
        if (!pnPMKIDQueryList)
        {
           NDTLogErr(_T("unable to allocate memory"));
           FlagError(ErrorSevere,&rc);
           break;
        }
        hr = NDTWlanGetPMKID(hAdapter, pnPMKIDQueryList, &dwSize);                     
     }
     if (NOT_SUCCEEDED(hr))
     {
        NDTLogErr(_T("Failed to get PMKID list Error:0x%x"),NDIS_FROM_HRESULT(hr));
        NDTLogErr(_T("WPA2 drivers must support OID_802_11_PMKID query request"));
        FlagError(ErrorSevere,&rc);
        break;
     }

     // Verify the info field
     if (ulPmkidCount != pnPMKIDQueryList->BSSIDInfoCount)
     {
        NDTLogErr(_T("Incorrect value returned for BSSIDInfoCount (Returned %d Expected %d)")
           , pnPMKIDQueryList->BSSIDInfoCount, ulPmkidCount);
        FlagError(ErrorSevere,&rc);
     }     
     else
        NDTLogDbg(_T("PASS: Correct value returned for BSSIDInfoCount %d"),pnPMKIDQueryList->BSSIDInfoCount);

  }
  while(0);

  if (pnPMKIDQueryList)
  {
    free(pnPMKIDQueryList);
    pnPMKIDQueryList = NULL;
  }

  
  // Fifth Variation
  NDTLogMsg(_T("Variation 5: Set OID_802_11_PKMID with maximium number of supported PMKIDs"));
  NDTLogMsg(_T("This test will set OID_802_11_PKMID with the maxiumium number of PMKIDs"));
  NDTLogMsg(_T("the driver supports and verify the request succeeds"));

  do
  {
     hr = NDTWlanGetNumPKIDs(hAdapter, &ulPmkidCount);
     if (NOT_SUCCEEDED(hr))
     {
        NDTLogErr(_T("Unable to get num pkids supported"));
        FlagError(ErrorSevere,&rc);
        break;
     }         

     dwSize = FIELD_OFFSET(NDIS_802_11_PMKID, BSSIDInfo) +(ulPmkidCount * sizeof(BSSIDInfo));
     pnPMKIDList = (PNDIS_802_11_PMKID)malloc(dwSize);
     if (!pnPMKIDList)
     {
        NDTLogErr(_T("unable to allocate memory"));
        FlagError(ErrorSevere,&rc);
        break;
     }

     for (UINT iIndex = 0; iIndex < ulPmkidCount; iIndex++)
     {
        memcpy(pnPMKIDList->BSSIDInfo[iIndex].BSSID, aMac, sizeof(pnPMKIDList->BSSIDInfo[0].BSSID));
        memcpy(pnPMKIDList->BSSIDInfo[iIndex].PMKID, aPMKID2, sizeof(pnPMKIDList->BSSIDInfo[0].PMKID));
     }
        
     pnPMKIDList->BSSIDInfoCount = ulPmkidCount;
     pnPMKIDList->Length = dwSize;
        
     hr = NDTWlanSetPMKID(hAdapter, pnPMKIDList, dwSize);
     if (NOT_SUCCEEDED(hr))
     {
        NDTLogErr(_T("Failed to add maximium supported PMKIDs Error0x:%x"),NDIS_FROM_HRESULT(hr));
        NDTLogErr(_T("Driver must allow the maximium number of PMKID's reported"));
        NDTLogErr(_T("in the capabilities information to be set"));
        FlagError(ErrorSevere,&rc);
        break;
     }
     else
         NDTLogDbg(_T("PASS: Succesfully added Max supported %d pmkids"), ulPmkidCount);

     pnPMKIDQueryList = (PNDIS_802_11_PMKID) malloc(dwSize);
     if (!pnPMKIDQueryList )
     {
        NDTLogErr(_T("unable to allocate memory"));
        FlagError(ErrorSevere,&rc);
        break;
     }
     
     hr = NDTWlanGetPMKID(hAdapter, pnPMKIDQueryList, &dwSize);
     if (NOT_SUCCEEDED(hr) && hr == NDT_STATUS_BUFFER_TOO_SHORT)
     {
        free(pnPMKIDQueryList);
        pnPMKIDQueryList = (PNDIS_802_11_PMKID) malloc(dwSize);
        if (!pnPMKIDQueryList )
        {
          NDTLogErr(_T("unable to allocate memory"));
          FlagError(ErrorSevere,&rc);
          break;
        }
        hr = NDTWlanGetPMKID(hAdapter, pnPMKIDQueryList , &dwSize);                     
     }
     if (NOT_SUCCEEDED(hr))
     {
        NDTLogErr(_T("Failed to get PMKID list Error:0x%x"),NDIS_FROM_HRESULT(hr));
        NDTLogErr(_T("WPA2 drivers must support OID_802_11_PMKID query request"));
        FlagError(ErrorSevere,&rc);
        break;
     }

     // Verify the info field
     if (ulPmkidCount != pnPMKIDQueryList ->BSSIDInfoCount)
     {
        NDTLogErr(_T("Incorrect value returned for BSSIDInfoCount (Returned %d Expected %d)")
           , pnPMKIDQueryList ->BSSIDInfoCount, ulPmkidCount);
        FlagError(ErrorSevere,&rc);
        break;
     }
     else
        NDTLogDbg(_T("Correct value %d returned for BSSIDInfoCount"),pnPMKIDQueryList ->BSSIDInfoCount);


     for (UINT iIndex=0; iIndex<pnPMKIDQueryList ->BSSIDInfoCount;iIndex++)
     {
        if (memcmp(pnPMKIDQueryList->BSSIDInfo[iIndex].BSSID, aMac, sizeof(pnPMKIDQueryList->BSSIDInfo[iIndex].BSSID)))
        {
           NDTLogErr(_T("Incorrect BSSIDInfo value for BSSID returned"));
           NDTLogErr(_T("The value returned for BSSID does not match the value previously set"));
           FlagError(ErrorSevere,&rc);
           break;
        }

        if (memcmp(pnPMKIDQueryList->BSSIDInfo[iIndex].PMKID, aPMKID2, sizeof(pnPMKIDQueryList->BSSIDInfo[iIndex].PMKID)))
        {
           NDTLogErr(_T("Incorrect BSSIDInfo value for PMKID returned"));
           NDTLogErr(_T("The value returned for PMKID does not match the value previously set"));
           FlagError(ErrorSevere,&rc);
           break;
        }         
     } 
     
  }
  while(0);

  if (pnPMKIDList)
  {
     free(pnPMKIDList);  
     pnPMKIDList = NULL;
  }

  // Sixth Variation
  NDTLogMsg(_T("Variation 6: Set OID_802_11_PKMID with invalid number of PMKIDs"));
  NDTLogMsg(_T("This test will set OID_802_11_PKMID with more PMKIDs than the driver "));
  NDTLogMsg(_T("supports and verify the driver failed the request"));

  do
  {
     hr = NDTWlanGetNumPKIDs(hAdapter, &ulPmkidCount);
     if (NOT_SUCCEEDED(hr))
     {
        NDTLogErr(_T("Unable to get num pkids supported"));
        FlagError(ErrorSevere,&rc);
        break;
     }         

     ulPmkidCount++;

     dwSize = FIELD_OFFSET(NDIS_802_11_PMKID, BSSIDInfo) +(ulPmkidCount * sizeof(BSSIDInfo));
     pnPMKIDList = (PNDIS_802_11_PMKID)malloc(dwSize);
     if (!pnPMKIDList)
     {
        NDTLogErr(_T("unable to allocate memory"));
        FlagError(ErrorSevere,&rc);
        break;
     }

     for (UINT iIndex = 0; iIndex < ulPmkidCount; iIndex++)
     {
        memcpy(pnPMKIDList->BSSIDInfo[iIndex].BSSID, aMac, sizeof(pnPMKIDList->BSSIDInfo[0].BSSID));
        memcpy(pnPMKIDList->BSSIDInfo[iIndex].PMKID, aPMKID2, sizeof(pnPMKIDList->BSSIDInfo[0].PMKID));
     }
        
     pnPMKIDList->BSSIDInfoCount = ulPmkidCount;
     pnPMKIDList->Length = dwSize;
        
     hr = NDTWlanSetPMKID(hAdapter, pnPMKIDList, dwSize);
     
     if(hr == NDT_STATUS_INVALID_DATA)
        NDTLogDbg(_T("PASS: Driver returned error as expected"));
     else
     {
        NDTLogErr(_T("Should have returned NDIS_STATUS_INVALID_DATA, instead return: %d"),NDIS_FROM_HRESULT(hr));
        NDTLogErr(_T("Driver must not return success when more PMKIDs are set than are supported"));
        FlagError(ErrorSevere,&rc);
        break;
     }         
  }
  while(0);

  if (pnPMKIDList)
  {
     free(pnPMKIDList);  
     pnPMKIDList = NULL;
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

