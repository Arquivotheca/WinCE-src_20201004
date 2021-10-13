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

TEST_FUNCTION(TestWlanSsid)
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
   DWORD dwNumFound = 0;

   UINT cbUsed;
   UINT cbRequired;
   const UINT iCount = 2;
   const UINT iScanCount = 10 ;
   
   

   NDTLogMsg(
      _T("Start 1c_wlan_ssid test on the adapter %s\n"), g_szTestAdapter
   );

   NDTLogMsg(_T("Opening adapter\n"));
   hr = NDTOpen(g_szTestAdapter, &hAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
      goto cleanUp;
   }

   hr = NDTBind(hAdapter, bForce30, ndisMedium);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailBind, hr);
      goto cleanUp;
   }

   hr = NDTWlanInitializeTest(hAdapter);
   if (FAILED(hr))
   {
      NDTLogErr(_T("Failed to initialize Wlan test Error0x%x\n"),hr);
      goto cleanUp;
   }

    hr = NDTWlanGetDeviceType(hAdapter,&WlanDeviceType);
    if (NOT_SUCCEEDED(hr))
      goto cleanUp;
   
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
    
      //First Variation
      NDTLogMsg(_T("Variation 1: Query SSID with an invalid input buffer length\n"));
      NDTLogMsg(_T("This test will verify the driver checks the input buffer length before copying the SSID\n"));

      do
      {
         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         dwTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssidList[ix], FALSE ,&dwTimeout);         
         if (NOT_SUCCEEDED(hr))
         {
            GetSsidText(szSsidText, ssidList[ix]);
            NDTLogErr(_T("Failed to associate with %s Error:0x%x\n"),szSsidText,hr);
            FlagError(ErrorSevere,&rc);            
            break;
         }

         //Query SSID with a valid buffer but an invalid length
         BYTE abDummy[4];
         hr = NDTQueryInfo(hAdapter, OID_802_11_SSID, abDummy, sizeof(abDummy) -1, &cbUsed, &cbRequired);
         if (!NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Should not have returned NDIS_STATUS_SUCCESS\n"));
            NDTLogErr(_T("Driver must check the length of the input buffer and "));
            NDTLogErr(_T("return a valid NDIS_STATUS failure code if the input buffer is to short\n"));
         }
         else
         {
            NDTLogMsg(_T(" Returned value  0x%x\n"), NDIS_FROM_HRESULT(hr));
         }         
      }      
      while(0);

      // Second Variation      
      NDTLogMsg(_T("Variation 2: Verify association with each AP in the test environment\n"));
      NDTLogMsg(_T(" This test will verify the device can associate with each of the APs in the test environment\n"));

      do
      {
         UCHAR apList[3][20];
         UINT iNumSsids;
         
         if (WlanDeviceType == WLAN_DEVICE_TYPE_802_11AG)
         {
            iNumSsids = 3;
            strcpy((char *)apList[0], (char *)WLAN_WEP_AP1);
            strcpy((char *)apList[1], (char *)WLAN_WEP_AP2);
            strcpy((char *)apList[2], (char *)WLAN_WEP_AP3);
         }
         else
         {
            iNumSsids = 2;
            strcpy((char *)apList[0], (char *)WLAN_WEP_AP1);
            strcpy((char *)apList[1], (char *)WLAN_WEP_AP2);            
         }         

         for (UINT iy = 0; iy<iNumSsids; iy++)
         {
            NDIS_802_11_SSID ssid;
            NDIS_802_11_SSID ActualSsid;
            ssid.SsidLength = strlen((char *)apList[iy]);
            memcpy(ssid.Ssid,apList[iy],ssid.SsidLength);
            GetSsidText(szSsidText , ssid);
            
            ulKeyLength = 10;
            dwEncryption = Ndis802_11WEPEnabled;
            dwTimeout = WLAN_ASSOCIATE_TIMEOUT;
            hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                    ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssid, FALSE ,&dwTimeout);         
            if (NOT_SUCCEEDED(hr))
            {
               NDTLogErr(_T("Failed to associate with %s Error:0x%x\n"),szSsidText,hr);
               FlagError(ErrorSevere,&rc);            
               break;
            }
            else
               NDTLogDbg(_T("Succesfully associated with %s"),szSsidText);



            hr = NDTWlanGetSsid(hAdapter, &ActualSsid);
            if (FAILED(hr))
            {
               NDTLogErr(_T("Failed to query OID_802_11_SSID Error=0x%x\n"),NDIS_FROM_HRESULT(hr));
               FlagError(ErrorSevere,&rc);
               break;
            }

            if (!(ssid.SsidLength == ActualSsid.SsidLength && (0== memcmp((void *)&ssid.Ssid,(void *)&ActualSsid.Ssid,ssid.SsidLength))) )
            {
               GetSsidText(szSsidText, ssid);
               NDTLogErr(_T("OID_802_11_SSID should have returned %s \n"), szSsidText);
               GetSsidText(szSsidText, ActualSsid);
               NDTLogErr(_T(" Returned %s\n"),szSsidText);
               NDTLogErr(_T("Driver must return the SSID of the AP which it is currently associated with\n"));
               FlagError(ErrorSevere,&rc);
            }               
            else
               NDTLogDbg(_T("PASS: Returned ssid of AP associated with"));
         }
         
      }
      while(0);

      
	// Third Variation      
      NDTLogMsg(_T("Variation 3: Verify OID_802_11_SSID query returns the correct SSID after association\n"));
      NDTLogMsg(_T("This test will associate with the AP and then query OID_802_11_SSID and verify the APs SSID is returned\n"));

      do
      {
         NDIS_802_11_SSID ActualSsid;
         
         hr = NDTWlanDisassociate(hAdapter);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_DISASSOCIATE, Error : 0x%x\n"),hr);
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
            NDTLogDbg(_T("Succesfully associated with %s"),szSsidText);



        hr = NDTWlanGetSsid(hAdapter, &ActualSsid);
         if (FAILED(hr))
         {
            NDTLogErr(_T("Failed to query OID_802_11_SSID Error=0x%x\n"),NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }

         if (!(ssidList[ix].SsidLength == ActualSsid.SsidLength && (0== memcmp((void *)&ssidList[ix].Ssid,(void *)&ActualSsid.Ssid,ssidList[ix].SsidLength))) )
         {
            GetSsidText(szSsidText, ssidList[ix]);
            NDTLogErr(_T("OID_802_11_SSID should have returned %s \n"), szSsidText);
            GetSsidText(szSsidText, ActualSsid);
            NDTLogErr(_T(" Returned %s\n"),szSsidText);
            NDTLogErr(_T("Driver must return the SSID of the AP which it is currently associated with\n"));
            FlagError(ErrorSevere,&rc);
         }    
          else
               NDTLogDbg(_T("PASS: Returned ssid of AP associated with"));
      }
      while(0);


      // Fourth Variation
      
      NDTLogMsg(_T("Variation 4: Verify OID_802_11_SSID query returns 0 for SSIDLength when not associated\n"));
      NDTLogMsg(_T("This test will disassociate the device then query OID_802_11_SSID and verify SSIDLength is set to 0 \n"));

      do
      {
         NDIS_802_11_SSID ActualSsid;
      
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
            NDTLogDbg(_T("Succesfully associated with %s"),szSsidText);



         hr = NDTWlanDisassociate(hAdapter);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_DISASSOCIATE,Error : 0x%x\n"),hr);
            NDTLogErr(_T("Driver must indicate a media disconnect event after OID_802_11_DISASSOCIATE is set"));
            FlagError(ErrorSevere,&rc);
            break;
         }

         // Pause for a moment to let the disassociate complete
         Sleep(2000);

         memset(&ActualSsid, 0, sizeof(ActualSsid));
         hr = NDTWlanGetSsid(hAdapter, &ActualSsid);
         if (FAILED(hr))
         {
            NDTLogErr(_T("Failed to query OID_802_11_SSID Error=0x%x\n"),NDIS_FROM_HRESULT(hr));
            FlagError(ErrorSevere,&rc);
            break;
         }

         if (0 != ActualSsid.SsidLength)
         {
            NDTLogErr(_T("SSIDLength should have been 0, Driver must return 0 for SSIDLength when not associated\n"));            
            FlagError(ErrorMild,&rc);            
         }

         NDTWlanReset(hAdapter, TRUE);         
      }
      while(0);


    	// Fifth Variation
      
      NDTLogMsg(_T("Variation 5: Verify device can associate with any available SSID \n"));
      NDTLogMsg(_T("This test will associate with any available SSID by specifying a zero length string for the SSID \n"));

      do
      {
         NDIS_802_11_SSID nEmptySsid;
    
         nEmptySsid.SsidLength = 0;
         nEmptySsid.Ssid[0] = '\0';
            
         NDTWlanReset(hAdapter, TRUE);         

         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         dwTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, nEmptySsid, FALSE ,&dwTimeout);         
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to associate with any available SSID Error:0x%x\n"),hr);
            NDTLogErr(_T("Driver must associate with any available SSID when the SSID specified is a zero length string\n"));
            FlagError(ErrorSevere,&rc);            
            break;
         }         
         else
            NDTLogDbg(_T("Succesfully associated with some SSID when SSID specified was a zero length string"));
      }
      while(0);

      // Sixth Variation
      
      NDTLogMsg(_T("Variation 6: Set OID_802_11_SSID with an invalid SsidLength\n"));
      NDTLogMsg(_T("This test will set OID_802_11_SSID with the SsidLength field set to a value\n"));
      NDTLogMsg(_T("larger than 32 and verify the driver fails the set request\n"));

      do
      {
         NDIS_802_11_SSID nWrongSsid;
         nWrongSsid.SsidLength = 36;
         memcpy(nWrongSsid.Ssid,"INVALID_LENGTH_SSID",strlen("INVALID_LENGTH_SSID\n"));
            
         NDTWlanReset(hAdapter, TRUE);         

         ulKeyLength = 10;
         dwEncryption = Ndis802_11WEPEnabled;
         dwTimeout = WLAN_ASSOCIATE_TIMEOUT;
         hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                 ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, nWrongSsid, FALSE ,&dwTimeout);         

         if (!NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Should not have returned NDIS_STATUS_SUCCESS\n"));
            NDTLogErr(_T("Driver must check the length of the SsidLength field and fail the set request if the length exceeds the maximium Ssid field size\n"));
            FlagError(ErrorSevere,&rc);            
            break;
         }         
         else
            NDTLogDbg(_T("PASS: Failed to associate with a Ssid as expected"));
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

