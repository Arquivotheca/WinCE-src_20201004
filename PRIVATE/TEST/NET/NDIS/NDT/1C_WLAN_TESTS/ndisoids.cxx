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
#include <map>
#include <string.hxx>



// These define when the OID was first introduced
const ULONG OID_SUPPORT_WLAN      = 0x00000001;
const ULONG OID_SUPPORT_WPA       = 0x00000002; 
const ULONG OID_SUPPORT_WME       = 0x00000004;
const ULONG OID_SUPPORT_WPA2      = 0x00000008; 

typedef enum
{
SUPPORT_INVALID_VALUE = 0, // Not used except as a default value
SUPPORT_MANDATORY = 1,
SUPPORT_OPTIONAL,
SUPPORT_RECOMMENDED
} OidSupport;

const ULONG REQUEST_TYPE_SET     = 0x00000001;
const ULONG REQUEST_TYPE_QUERY   = 0x00000002;

#ifdef _UNICODE
#define tstring wstring
#else
#define tstring string
#endif

class OidInfo 
{
   public :
      ce::tstring m_sName;
      NDIS_OID m_nValue;
      DWORD m_fRequestType;
      OidSupport m_Support;
      DWORD m_fDriverSupport;
      BOOL m_bSupported;

   OidInfo(TCHAR* Name, NDIS_OID Value, DWORD RequestType, OidSupport Support, DWORD DriverSupport) 
      : m_sName(Name), m_nValue(Value), m_fRequestType(RequestType), m_Support(Support), m_fDriverSupport(DriverSupport)
  {      
  }

   OidInfo():m_sName(_T("\n")),m_nValue(0),m_fRequestType(0), m_Support(SUPPORT_INVALID_VALUE), m_fDriverSupport(0)
   {
   }
 };

struct lessOidInfo
{   
   BOOL operator() (const NDIS_OID& val1, const NDIS_OID& val2) const
   {
      return ( val1 < val2);
   }
};
   
typedef std::map<NDIS_OID, OidInfo, lessOidInfo> OidList;

OidList   WlanList;
OidList SupportedList;

typedef std::map<NDIS_OID, OidInfo, lessOidInfo>::iterator pOidListIter;

//------------------------------------------------------------------------------

TEST_FUNCTION(TestWlanNdisOids)
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

   UINT cbUsed = 0;
   UINT cbRequired = 0;
   UINT cSupportedOids;

   NDIS_OID aSupportedOids[256];
   BOOL bWPASupported;
   BOOL bMediaStreamSupported;
   BOOL bWPA2Supported;
   DWORD fDriverSupport;
   
   

   NDTLogMsg(
      _T("Start 1c_wlan_ndisoids on the adapter %s\n"), g_szTestAdapter
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
   

   OidInfo* pOI = new  OidInfo(_T("OID_802_11_BSSID\n"), OID_802_11_BSSID, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_MANDATORY, OID_SUPPORT_WLAN);
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_SSID\n"), OID_802_11_SSID, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_MANDATORY, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue] = *pOI;
   delete pOI;

   pOI = new OidInfo(_T("OID_802_11_NETWORK_TYPES_SUPPORTED\n"), OID_802_11_NETWORK_TYPES_SUPPORTED, REQUEST_TYPE_QUERY, SUPPORT_MANDATORY, OID_SUPPORT_WPA );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_NETWORK_TYPE_IN_USE\n"), OID_802_11_NETWORK_TYPE_IN_USE, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_MANDATORY, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_TX_POWER_LEVEL\n"), OID_802_11_TX_POWER_LEVEL, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_OPTIONAL, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_RSSI\n"), OID_802_11_RSSI, REQUEST_TYPE_QUERY, SUPPORT_MANDATORY, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_RSSI_TRIGGER\n"), OID_802_11_RSSI_TRIGGER, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_OPTIONAL, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_INFRASTRUCTURE_MODE\n"), OID_802_11_INFRASTRUCTURE_MODE, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_MANDATORY, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_FRAGMENTATION_THRESHOLD\n"), OID_802_11_FRAGMENTATION_THRESHOLD, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_OPTIONAL, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_RTS_THRESHOLD\n"), OID_802_11_RTS_THRESHOLD, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_OPTIONAL, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_NUMBER_OF_ANTENNAS\n"), OID_802_11_NUMBER_OF_ANTENNAS, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_OPTIONAL, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_RX_ANTENNA_SELECTED\n"), OID_802_11_RX_ANTENNA_SELECTED, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_OPTIONAL, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_TX_ANTENNA_SELECTED\n"), OID_802_11_TX_ANTENNA_SELECTED, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_OPTIONAL, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_SUPPORTED_RATES\n"), OID_802_11_SUPPORTED_RATES, REQUEST_TYPE_QUERY, SUPPORT_MANDATORY, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_DESIRED_RATES\n"), OID_802_11_DESIRED_RATES, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_OPTIONAL, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_CONFIGURATION\n"), OID_802_11_CONFIGURATION, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_MANDATORY, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_STATISTICS\n"), OID_802_11_STATISTICS, REQUEST_TYPE_QUERY, SUPPORT_MANDATORY, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_ADD_WEP\n"), OID_802_11_ADD_WEP, REQUEST_TYPE_SET, SUPPORT_MANDATORY, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_REMOVE_WEP\n"), OID_802_11_REMOVE_WEP, REQUEST_TYPE_SET, SUPPORT_MANDATORY, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;


   // This OID is skipped because it is a set only with no assoicated data       
   // pOI = new OidInfo(_T("OID_802_11_DISASSOCIATE\n"), OID_802_11_DISASSOCIATE, REQUEST_TYPE_SET, SUPPORT_MANDATORY, OID_SUPPORT_WLAN );
   // WlanList[pOI->m_nValue]= *pOI;
   // delete pOI;
   
   pOI = new OidInfo(_T("OID_802_11_POWER_MODE\n"), OID_802_11_POWER_MODE, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_MANDATORY, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_BSSID_LIST\n"), OID_802_11_BSSID_LIST, REQUEST_TYPE_QUERY, SUPPORT_MANDATORY, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_AUTHENTICATION_MODE\n"), OID_802_11_AUTHENTICATION_MODE, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_MANDATORY, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_PRIVACY_FILTER\n"), OID_802_11_PRIVACY_FILTER, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_OPTIONAL, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   
   //This OID is skipped because it is a set only with no assoicated data 
   // pOI = new OidInfo(_T("OID_802_11_BSSID_LIST_SCAN\n"), OID_802_11_BSSID_LIST_SCAN, REQUEST_TYPE_SET, SUPPORT_OPTIONAL, OID_SUPPORT_WLAN );
   // WlanList[pOI->m_nValue]= *pOI;
   // delete pOI;
   
   pOI = new OidInfo(_T("OID_802_11_ENCRYPTION_STATUS\n"), OID_802_11_ENCRYPTION_STATUS, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_MANDATORY, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_RELOAD_DEFAULTS\n"), OID_802_11_RELOAD_DEFAULTS, REQUEST_TYPE_SET, SUPPORT_MANDATORY, OID_SUPPORT_WLAN );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;

   //WPA OIDS 
   pOI = new OidInfo(_T("OID_802_11_ADD_KEY\n"), OID_802_11_ADD_KEY, REQUEST_TYPE_SET, SUPPORT_MANDATORY, OID_SUPPORT_WPA );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_REMOVE_KEY\n"), OID_802_11_REMOVE_KEY, REQUEST_TYPE_SET, SUPPORT_MANDATORY, OID_SUPPORT_WPA );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   pOI = new OidInfo(_T("OID_802_11_ASSOCIATION_INFORMATION\n"), OID_802_11_ASSOCIATION_INFORMATION, REQUEST_TYPE_QUERY, SUPPORT_OPTIONAL, OID_SUPPORT_WPA );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   
   //WPA2 OIDS 
   pOI = new OidInfo(_T("ID_802_11_CAPABILITY\n"), OID_802_11_CAPABILITY, REQUEST_TYPE_QUERY, SUPPORT_MANDATORY, OID_SUPPORT_WPA2 );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;

   pOI = new OidInfo(_T("OID_802_11_PMKID"), OID_802_11_PMKID, REQUEST_TYPE_QUERY | REQUEST_TYPE_SET, SUPPORT_MANDATORY, OID_SUPPORT_WPA2 );
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
   
   //WME/WPA2 OIDS 
   pOI = new OidInfo(_T("OID_802_11_MEDIA_STREAM_MODE"), OID_802_11_MEDIA_STREAM_MODE, REQUEST_TYPE_SET | REQUEST_TYPE_QUERY, SUPPORT_RECOMMENDED, OID_SUPPORT_WME);
   WlanList[pOI->m_nValue]= *pOI;
   delete pOI;
 
   // get the supported from the driver
   NDTLogMsg(_T("Variation 1: Query driver supported OID list\n"));
   
   hr = NDTQueryInfo( hAdapter, OID_GEN_SUPPORTED_LIST, aSupportedOids,
                      sizeof(aSupportedOids), &cbUsed, &cbRequired );
   if (NOT_SUCCEEDED(hr))
   {   
      NDTLogErr(_T("Falied to query drivers supported OID list Error:0x%x\n"),NDIS_FROM_HRESULT(hr));
      FlagError(ErrorSevere,&rc); 
      goto cleanUp;
   };

   cSupportedOids = cbUsed / sizeof(NDIS_OID);

   //Add each oid to the supported list dictionary
   for(UINT iOid=0; iOid < cSupportedOids; iOid++)
   {
      NDIS_OID nOidValue = aSupportedOids[iOid];
      pOidListIter pInfo=SupportedList.find(nOidValue); 
      if ( pInfo == SupportedList.end())
      { 
         // Add the oid and a dummy OidInfo object
         SupportedList.insert(OidList::value_type(nOidValue,OidInfo()));
      }      
   }

   fDriverSupport = OID_SUPPORT_WLAN;

   hr = NDTWlanIsWPA2Supported(hAdapter, &bWPA2Supported);
   if (NOT_SUCCEEDED(hr))
   {
        NDTLogErr(_T("Cannot query IsWPA2Supported Error:0x%x\n"),NDIS_FROM_HRESULT(hr));
        FlagError(ErrorSevere,&rc);
        goto cleanUp;
   }
   if (bWPA2Supported)
      fDriverSupport = fDriverSupport | OID_SUPPORT_WPA2;

   hr = NDTWlanIsMediaStreamSupported(hAdapter, &bMediaStreamSupported);
   if (NOT_SUCCEEDED(hr))
   {
        NDTLogErr(_T("Cannot query IsMediaStreamSupported Error:0x%x\n"),NDIS_FROM_HRESULT(hr));
        FlagError(ErrorSevere,&rc);
        goto cleanUp;
   }   
   if (bMediaStreamSupported)
      fDriverSupport = fDriverSupport | OID_SUPPORT_WME;


   hr = NDTWlanIsWPASupported(hAdapter, &bWPASupported);
   if (NOT_SUCCEEDED(hr))
   {
        NDTLogErr(_T("Cannot query IsWPASupported Error:0x%x\n"),NDIS_FROM_HRESULT(hr));
        FlagError(ErrorSevere,&rc);
        goto cleanUp;
   }
   if (bWPASupported)
      fDriverSupport = fDriverSupport | OID_SUPPORT_WPA;

   // Variation 2
   NDTLogMsg(_T("Variation 2: Verify all mandatory OIDs are in the drivers supported OID list\n"));
   NDTLogMsg(_T("Verify that all mandatory wireless OIDs were returned in the supported OID\n"));
   NDTLogMsg(_T("list, and warn for any recommended OIDs that are not in the support list\n"));

   for (pOidListIter pInfo = WlanList.begin(); pInfo != WlanList.end(); pInfo++)
   {
      //If this oid is in the supported list mark it as supported for later use testing 
      pOidListIter pSuppInfo = SupportedList.find(pInfo->first);
      if (pSuppInfo != SupportedList.end())
      {
         pInfo->second.m_bSupported = TRUE;
      }

      //If the driver and OID both have the same support the run the test
      // for example if the OID is for WPA and the driver supports WPA then 
      //verify it is supported 
      if ((fDriverSupport & pInfo->second.m_fDriverSupport) == pInfo->second.m_fDriverSupport)
      {
         if (SUPPORT_MANDATORY == pInfo->second.m_Support)
         {
           if (pSuppInfo == SupportedList.end())
           {
               NDTLogErr(_T("Supported OID list did not contain mandatory %s \n"),pInfo->second.m_sName);
               NDTLogErr(_T("This is a mandatory OID that must be supported by the driver\n"));
               FlagError(ErrorSevere,&rc); 
           }           
         }

         if (SUPPORT_RECOMMENDED == pInfo->second.m_Support)
         {
           if (pSuppInfo == SupportedList.end())
           {
               NDTLogWrn(_T("Supported OID list did not contain recommended %s\n"),pInfo->second.m_sName);
               NDTLogWrn(_T("This is an optional OID but Microsoft does recommend that this OID be supported\n"));          
           }           
         }         
      }
      else
      {
         NDTLogMsg(_T(" %s verification skipped\n"),pInfo->second.m_sName);
      }      
   }

   /*
   for (pOidListIter pInfo = WlanList.begin(); pInfo != WlanList.end(); pInfo++)
   {
      if (TRUE == pInfo->second.m_bSupported)
      {
         switch(pInfo->second.m_nValue)
         {
            case  OID_802_11_CONFIGURATION :
                  NDTWlanReset(hAdapter, TRUE);
               break;
         }

         // Third Variation 
         NDTLogMsg(_T("Variation 3: Set %s with an invalid information buffer length and verify the request fails\n"), pInfo->second.m_sName);

         if ( REQUEST_TYPE_SET == (REQUEST_TYPE_SET & pInfo->second.m_fRequestType))
         {
            BYTE abBuffer[1];
            abBuffer[0] = 0;
            hr = NDTSetInfo(hAdapter, pInfo->second.m_nValue, abBuffer, sizeof(abBuffer) - 1, &cbUsed, &cbRequired);
            if ( NDIS_STATUS_INVALID_LENGTH != NDIS_FROM_HRESULT(hr) && NDIS_STATUS_BUFFER_TOO_SHORT != NDIS_FROM_HRESULT(hr))
            {
               NDTLogErr(_T("Should have return NDIS_STATUS_INVALID_LENGTH or NDIS_STATUS_BUFFER_TOO_SHORT \n"));
               NDTLogErr(_T("Instead returned %x while trying to set %s \n"),NDIS_FROM_HRESULT(hr), pInfo->second.m_sName);
               FlagError(ErrorSevere,&rc);              
            }
         }         

         // Fourth Variation 
         NDTLogMsg(_T("Variation 4: Query %s with an invalid information buffer length and verify the request fails\n"), pInfo->second.m_sName);

         if ( REQUEST_TYPE_QUERY == (REQUEST_TYPE_QUERY & pInfo->second.m_fRequestType))
         {
            BYTE abBuffer[1];
            abBuffer[0] = 0;
            hr = NDTSetInfo(hAdapter, pInfo->second.m_nValue, abBuffer, sizeof(abBuffer) - 1, &cbUsed, &cbRequired);
            if ( NDIS_STATUS_INVALID_LENGTH != NDIS_FROM_HRESULT(hr) && NDIS_STATUS_BUFFER_TOO_SHORT != NDIS_FROM_HRESULT(hr))
            {
               NDTLogErr(_T("Should have return NDIS_STATUS_INVALID_LENGTH or NDIS_STATUS_BUFFER_TOO_SHORT \n"));
               NDTLogErr(_T("Instead returned %x while querying %s \n"),NDIS_FROM_HRESULT(hr), pInfo->second.m_sName);
               FlagError(ErrorSevere,&rc);          
            }
         }         

         switch(pInfo->second.m_nValue)
         {
            case  OID_802_11_CONFIGURATION :
                  NDIS_802_11_SSID ssid;
                  TCHAR szSsidText[33];
                  
                  ulKeyLength = 10;
                  dwEncryption = Ndis802_11WEPEnabled;
                  ulTimeout = WLAN_ASSOCIATE_TIMEOUT;
                  ssid.SsidLength = strlen((char *)WLAN_WEP_AP1);
                  memcpy(ssid.Ssid, WLAN_WEP_AP1, ssid.SsidLength);
                  hr = NDTWlanAssociate(hAdapter, Ndis802_11Infrastructure, Ndis802_11AuthModeOpen, &dwEncryption, 
                          ulKeyLength, 0x80000000, (PBYTE)WLAN_KEY_WEP, NULL, ssid , FALSE ,&ulTimeout);         
                  if (NOT_SUCCEEDED(hr))
                  {
                     GetSsidText(szSsidText, ssid);
                     NDTLogErr(_T("Failed to associate with %s Error:0x%x\n"),szSsidText,hr);
                     FlagError(ErrorSevere,&rc);            
                     break;
                  }
               break;
         }

      }
   }
   */




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

