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
#include <hash_set.hxx>


//------------------------------------------------------------------------------

TEST_FUNCTION(TestWpa2Capability)
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
   NDIS_802_11_DEVICE_TYPE WlanDeviceType;

   BOOL bWPA2Supported;

   BOOL bIsEncryptionSupported = FALSE;
   BOOL bIsAuthenticationSupported = FALSE;

   TCHAR szAuthModeText[80];
   TCHAR szEncrModeText[80];
   
   ce::hash_set<NDIS_802_11_ENCRYPTION_STATUS> aEncrypt;
   ce::hash_set<NDIS_802_11_AUTHENTICATION_MODE> aAuth;   

   ce::hash_set<NDIS_802_11_AUTHENTICATION_MODE>::iterator itA;
   ce::hash_set<NDIS_802_11_ENCRYPTION_STATUS>::iterator itE;

   NDTLogMsg(
      _T("Start 1c_wpa2_capability on the adapter %s"), g_szTestAdapter
   );

   NDTLogMsg(_T("Opening adapter"));
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
      NDTLogErr(_T("Failed to initialize Wlan test Error0x%x"),hr);
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
      NDTLogErr(_T("Failed to get IsWPA2Supported Error:0x%x"),hr);
      FlagError(ErrorSevere,&rc);
      goto cleanUp;
   }
   if (FALSE == bWPA2Supported)
   {
      NDTLogErr(_T("%s does not support WPA2, skipping test"), g_szTestAdapter);
      rc = TPR_SKIP;
      goto cleanUp;
   }
   
   aEncrypt.insert(Ndis802_11EncryptionDisabled);    
   aEncrypt.insert(Ndis802_11Encryption1Enabled); 
   aEncrypt.insert(Ndis802_11Encryption2Enabled);
   aEncrypt.insert(Ndis802_11Encryption3Enabled);

   aAuth.insert(Ndis802_11AuthModeOpen);
   aAuth.insert(Ndis802_11AuthModeShared);
   aAuth.insert(Ndis802_11AuthModeWPA);
   aAuth.insert(Ndis802_11AuthModeWPAPSK);
   aAuth.insert(Ndis802_11AuthModeWPANone);
   aAuth.insert(static_cast<NDIS_802_11_AUTHENTICATION_MODE>(Ndis802_11AuthModeWPA2));
   aAuth.insert(static_cast<NDIS_802_11_AUTHENTICATION_MODE>(Ndis802_11AuthModeWPA2PSK));
  
   // First Variation
   NDTLogMsg(_T("Variation 1: Query OID_802_11_CAPABILITY to verify support"));
   NDTLogMsg(_T("This test will query OID_802_11_CAPABILITY"));

   DWORD dwBufSize = sizeof(NDIS_802_11_CAPABILITY);
   PNDIS_802_11_CAPABILITY pnCapability = (PNDIS_802_11_CAPABILITY)malloc(dwBufSize);      

   do 
   {  
      hr = NDTWlanGetCapability(hAdapter, pnCapability, &dwBufSize);
      if (NOT_SUCCEEDED(hr) && NDT_STATUS_BUFFER_TOO_SHORT == hr)
      {
         pnCapability = (PNDIS_802_11_CAPABILITY)realloc(pnCapability, dwBufSize);
         hr = NDTWlanGetCapability(hAdapter, pnCapability, &dwBufSize);      
      }         
      if (NOT_SUCCEEDED(hr))      
      {
         NDTLogErr(_T("Failed to query OID_802_11_CAPABILITY"));
         NDTLogErr(_T("WPA2 drivers must support OID_802_11_CAPABILITY query request"));
         FlagError(ErrorSevere,&rc);
         goto cleanUp;
      }    
      else
         NDTLogDbg(_T("Succesfully queried OID_802_11_CAPABILITY"));
   }   
   while(0);

   bIsEncryptionSupported = FALSE;
   bIsAuthenticationSupported = FALSE;

   // Second Variation
   NDTLogMsg(_T("Variation 2: Verify driver supportes Open:None"));
   NDTLogMsg(_T("his test will verify the driver supports Open authentication with no encryption"));

   do
   {
      // For the first time print all authentication and Encryption modes
      for(UINT iIndex = 0; iIndex < pnCapability->NoOfAuthEncryptPairsSupported; iIndex++)
      {
         GetAuthModeText(szAuthModeText, pnCapability->AuthenticationEncryptionSupported[iIndex].AuthModeSupported);
         GetEncryptionText(szEncrModeText, pnCapability->AuthenticationEncryptionSupported[iIndex].EncryptStatusSupported);
         NDTLogDbg(_T("Pair %d: Authentication: %s Encryption %s"),iIndex, szAuthModeText,szEncrModeText);
         if (Ndis802_11AuthModeOpen == pnCapability->AuthenticationEncryptionSupported[iIndex].AuthModeSupported)
            bIsAuthenticationSupported = TRUE;
         if (Ndis802_11EncryptionDisabled == pnCapability->AuthenticationEncryptionSupported[iIndex].EncryptStatusSupported)
            bIsEncryptionSupported = TRUE;
      }

      if (FALSE == bIsAuthenticationSupported && FALSE == bIsEncryptionSupported)
      {
         NDTLogErr(_T("Open authentication with no encryption not supported"));
         NDTLogErr(_T("Driver must support Open authentication with no encryption"));
         FlagError(ErrorSevere,&rc);
      }
   }
   while(0);

   bIsEncryptionSupported = FALSE;
   bIsAuthenticationSupported = FALSE;

   // Third  Variation
   // Verify the driver reports supporting open authentication with WEP encryption
   NDTLogMsg(_T("Variation 2: Verify driver supportes Open/WEP"));
   NDTLogMsg(_T("his test will verify the driver supports Open authentication with WEP encryption"));

   do
   {
      for(UINT iIndex = 0; iIndex < pnCapability->NoOfAuthEncryptPairsSupported; iIndex++)
      {
         if (Ndis802_11AuthModeOpen == pnCapability->AuthenticationEncryptionSupported[iIndex].AuthModeSupported)
            bIsAuthenticationSupported = TRUE;
         if (Ndis802_11Encryption1Enabled == pnCapability->AuthenticationEncryptionSupported[iIndex].EncryptStatusSupported)
            bIsEncryptionSupported = TRUE;
      }

      if (FALSE == bIsAuthenticationSupported && FALSE == bIsEncryptionSupported)
      {
         NDTLogErr(_T("Open authentication with WEP encryption not supported"));
         NDTLogErr(_T("Driver must support Open authentication with WEP encryption"));
         FlagError(ErrorSevere,&rc);
      }
   }
   while(0);

   bIsEncryptionSupported = FALSE;
   bIsAuthenticationSupported = FALSE;

   // Fourth Variation
   // Verify the driver reports supporting WPA2 authentication with AES encryption
   NDTLogMsg(_T("Variation 2: Verify driver supportes WPA2/AES"));
   NDTLogMsg(_T("This test will verify the driver supports WPA2 authentication with AES encryption"));

   do
   {
      for(UINT iIndex = 0; iIndex < pnCapability->NoOfAuthEncryptPairsSupported; iIndex++)
      {
         if (Ndis802_11AuthModeWPA2 == pnCapability->AuthenticationEncryptionSupported[iIndex].AuthModeSupported)
            bIsAuthenticationSupported = TRUE;
         if (Ndis802_11Encryption3Enabled == pnCapability->AuthenticationEncryptionSupported[iIndex].EncryptStatusSupported)
            bIsEncryptionSupported = TRUE;
      }

      if (FALSE == bIsAuthenticationSupported && FALSE == bIsEncryptionSupported)
      {
         NDTLogErr(_T("WPA2 authentication with AES encryption not supported"));
         NDTLogErr(_T("Driver must support WPA2 authentication with AES encryption"));
         FlagError(ErrorSevere,&rc);
      }
   }
   while(0);

   //Fifth Variation
   NDTLogMsg(_T("Variation 5: Verify OID_802_11_CAPABILITY query information"));
   NDTLogMsg(_T("This test will verify the capailities returned"));

   do
   {
      //  Verify the version number
      NDTLogDbg(_T("Version number: %d"),pnCapability->Version);
      if (WLAN_WPA2_CAPABILITY_VERSION > pnCapability->Version)
      {
         NDTLogErr(_T("Invalid version number returned (Returned %d)"), pnCapability->Version);
         NDTLogErr(_T("NDIS_802_11_CAPABILITY->Version must be greater than or equal to %d"), WLAN_WPA2_CAPABILITY_VERSION);
         FlagError(ErrorSevere,&rc);
      }

      NDTLogDbg(_T("NoOfPMKIDs %d"),pnCapability->NoOfPMKIDs);
      // Verify Min supported PMK IDs
      if (WLAN_MIN_NUM_PMKIDS  > pnCapability->NoOfPMKIDs)
      {
         NDTLogErr(_T("Incorrect number of PMK IDs supported (Returned: %d)"),pnCapability->NoOfPMKIDs);
         NDTLogErr(_T("NDIS_802_11_CAPABILITY->NoOfPMKIDs must be greater than %d"),WLAN_MIN_NUM_PMKIDS);
         FlagError(ErrorSevere,&rc);
      }

      // Verify Max supported PMK IDs
      if (WLAN_MAX_NUM_PMKIDS  < pnCapability->NoOfPMKIDs)
      {
         NDTLogErr(_T("Incorrect number of PMK IDs supported (Returned: %d)"),pnCapability->NoOfPMKIDs);
         NDTLogErr(_T("NDIS_802_11_CAPABILITY->NoOfPMKIDs cannot be greater than %d"),WLAN_MAX_NUM_PMKIDS);
         FlagError(ErrorSevere,&rc);
      }

      NDTLogDbg(_T("Number of Auth/Enctypt pairs %d"),pnCapability->NoOfAuthEncryptPairsSupported);
      // Verify number of AuthEncr pairs
      if (1 > pnCapability->NoOfAuthEncryptPairsSupported)
      {
         NDTLogErr(_T("Incorrect number of Auth/Enctypt pairs returned (Returned: %d)"), pnCapability->NoOfAuthEncryptPairsSupported);
         NDTLogErr(_T("NDIS_802_11_CAPABILITY->NoOfAuthEncryptPairsSupported must be at least 1"));
         FlagError(ErrorSevere,&rc);
      }

   }
   while(0);

   // Variation 6
   NDTLogMsg(_T("Variation 6: Verify all supported authentication/encryption pairs"));
   NDTLogMsg(_T("This test will verify each of the authentication and encryption values"));
   NDTLogMsg(_T("returned are valid enumerations"));
   
   for(UINT iIndex = 0; iIndex < pnCapability->NoOfAuthEncryptPairsSupported; iIndex++)
   {
      itA = aAuth.find(pnCapability->AuthenticationEncryptionSupported[iIndex].AuthModeSupported);
      if (itA == aAuth.end())
      {
         NDTLogErr(_T("Incorrect authentication mode returned (Returned: %d"),
            pnCapability->AuthenticationEncryptionSupported[iIndex].AuthModeSupported);
         NDTLogErr(_T("Driver must return a valid authentication mode enumeration value"));
         FlagError(ErrorSevere,&rc);
      }
      else
      {
         GetAuthModeText(szAuthModeText, pnCapability->AuthenticationEncryptionSupported[iIndex].AuthModeSupported);
         NDTLogDbg(_T("PASS: Found authentication mode %s"),szAuthModeText);
      }
      
      itE = aEncrypt.find(pnCapability->AuthenticationEncryptionSupported[iIndex].EncryptStatusSupported);
      if (itE == aEncrypt.end())
      {
         NDTLogErr(_T("Incorrect encryption status returned (Returned:%d)"),
            pnCapability->AuthenticationEncryptionSupported[iIndex].EncryptStatusSupported);
         NDTLogErr(_T("Driver must return a valid encryption status enumeration value"));
         FlagError(ErrorSevere,&rc);
      }
      else
      {
         GetEncryptionText(szEncrModeText, pnCapability->AuthenticationEncryptionSupported[iIndex].EncryptStatusSupported);
         NDTLogDbg(_T("PASS: Found encryption mode %s"),szEncrModeText);
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

