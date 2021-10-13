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
#include "NDT_Request.h"
#include "ndterror.h"
#include "Driver.h"
#include "Capability.h"
#include "Request.h"
#include "Marshal.h"
#include "Utils.h"
#include "ndtlibwlan.h"


CWlanCapability::CWlanCapability(HANDLE hAdapter)
{
   // Right now placeholder for storing pair of pointers;
   m_hAdapter = hAdapter;

   m_dwDeviceType = WLAN_DEVICE_TYPE_802_11B;
      
   m_bWPASupported = FALSE;
   m_bWPA2Supported = FALSE;
   m_bTKIPSupported = FALSE;
   m_bAESSupported = FALSE;
   m_bWPAAdhocSupported = FALSE;
   m_bKeyMappingKeysSupported = FALSE;
   m_bWMESupported = FALSE;
   m_bMediaStreamSupported = FALSE;
         
   m_bWPATKIPSupported = FALSE;
   m_bWPAAESSupported = FALSE;
   m_bWPAPSKTKIPSupported = FALSE;
   m_bWPAPSKAESSupported = FALSE;
       
   m_bWPA2TKIPSupported = FALSE;
   m_bWPA2AESSupported = FALSE;
   m_bWPA2PSKTKIPSupported = FALSE;
   m_bWPA2PSKAESSupported = FALSE;
       
   m_bWPANoneTKIPSupported = FALSE;
   m_bWPANoneAESSupported = FALSE;

   m_NumPMKIDsSupported = 0;

   m_bSessionSet = FALSE;
}

HRESULT CWlanCapability::GetDeviceCapabilities()
{
   HRESULT hr=S_OK;
   UINT ix;

   //  0(802.11a), 1(802.11b), 2(802.11g
   BOOL phyTypes[3] = {FALSE,FALSE,FALSE};

   
   // GetSession
   //If this returns true then we already had capablity info in session
   if ( FALSE == m_bSessionSet)
   {   
   
       //Determine network types supported   
      PVOID pvNetworkTypesInUse = (PVOID)malloc(sizeof(DWORD));
      DWORD dwBufSize = 0;
      
      hr = NDTWlanGetNetworkTypesSuppported(m_hAdapter, (PVOID)pvNetworkTypesInUse, &dwBufSize);
      if (NDT_STATUS_BUFFER_TOO_SHORT == hr)
      {
         pvNetworkTypesInUse = (PVOID) realloc(pvNetworkTypesInUse, dwBufSize);
         hr = NDTWlanGetNetworkTypesSuppported(m_hAdapter, (PVOID)pvNetworkTypesInUse, &dwBufSize);
      }

      if ( NOT_SUCCEEDED(hr))
      {
         /* // TODO: use logging to let user know
            "Failed to query OID_802_11_NETWORK_TYPES_SUPPORTED")
            _T("It is recommended that your driver support this OID, since this OID was not supported or failed ")
            _T(" the tests will use your current network type, this may cause failures for multi-band devices"));
            */
      
         DWORD dwNetworkTypeInUse;
         hr = NDTWlanGetNetworkTypeInUse( m_hAdapter, &dwNetworkTypeInUse);
         if (NOT_SUCCEEDED(hr))
         {
            dwNetworkTypeInUse = Ndis802_11DS;
         }
          
         switch (dwNetworkTypeInUse)
         {
            case Ndis802_11DS :
               m_dwDeviceType = WLAN_DEVICE_TYPE_802_11B;
               break;
            case Ndis802_11FH :
               m_dwDeviceType = WLAN_DEVICE_TYPE_802_11B;
               break;
            case Ndis802_11OFDM5 :
               m_dwDeviceType = WLAN_DEVICE_TYPE_802_11A;
               break;
            case Ndis802_11OFDM24:
               m_dwDeviceType = WLAN_DEVICE_TYPE_802_11G;
               break;
          }
      }   
      else
      {
         PNDIS_802_11_NETWORK_TYPE_LIST netWorkType  = (PNDIS_802_11_NETWORK_TYPE_LIST) pvNetworkTypesInUse;
         // For each networktype in the list
         for (ix = 0; ix < netWorkType->NumberOfItems; ix++)      
         {
            switch(netWorkType->NetworkType[ix])
            {
               case Ndis802_11DS:
                  phyTypes[1] = TRUE;
                     break;
               case Ndis802_11FH:
                  phyTypes[1] = TRUE;
                     break;
               case Ndis802_11OFDM5:
                  phyTypes[0] = TRUE;
                     break;
               case Ndis802_11OFDM24:
                  phyTypes[2] = TRUE;
                     break;
            }
         }
         
         if (FALSE == phyTypes[0] && TRUE == phyTypes[1] && FALSE == phyTypes[2])
               // 802.11b
               m_dwDeviceType = WLAN_DEVICE_TYPE_802_11B;
         else if(TRUE == phyTypes[0] && FALSE == phyTypes[1] && FALSE == phyTypes[2])
               // 802.11a    
               m_dwDeviceType = WLAN_DEVICE_TYPE_802_11A;
         else if(FALSE == phyTypes[0] && TRUE == phyTypes[1] && TRUE == phyTypes[2])
              // 802.11g    
             m_dwDeviceType = WLAN_DEVICE_TYPE_802_11G;
         else if (TRUE == phyTypes[0] && TRUE == phyTypes[1] && FALSE == phyTypes[2]) 
             // 802.11a/b
             m_dwDeviceType = WLAN_DEVICE_TYPE_802_11AB;
         else if(TRUE == phyTypes[0] && TRUE == phyTypes[1] && TRUE == phyTypes[2]) 
            // 802.11a/g
            m_dwDeviceType = WLAN_DEVICE_TYPE_802_11AG;
      }
      
      //CapabilitySupported
      //Determine authentication and encryption supported
      dwBufSize = sizeof(NDIS_802_11_CAPABILITY);
      PNDIS_802_11_CAPABILITY pnCapability = (PNDIS_802_11_CAPABILITY)malloc(dwBufSize);      

      hr = NDTWlanGetCapability(m_hAdapter, pnCapability, &dwBufSize);
      if (NOT_SUCCEEDED(hr) && NDT_STATUS_BUFFER_TOO_SHORT == hr)
      {
         pnCapability = (PNDIS_802_11_CAPABILITY)realloc(pnCapability, dwBufSize);
         hr = NDTWlanGetCapability(m_hAdapter, pnCapability, &dwBufSize);      
      }         
      if (NOT_SUCCEEDED(hr))         
      {
         //OID_802_11_CAPABILITY not supported, using old style capability discovery
         DWORD dwAuthenticationMode;
         DWORD dwInfrastructureMode;
         DWORD dwEncryptionStatus;
         
         //First check Authentication supported
         dwInfrastructureMode = Ndis802_11Infrastructure;
         hr = NDTWlanSetInfrastructureMode(m_hAdapter, dwInfrastructureMode);
         if (FAILED(hr))
            return hr;

         do
         {
            // Check if driver supports WPA  
            hr = NDTWlanSetAuthenticationMode(m_hAdapter, Ndis802_11AuthModeWPA);
            if (FAILED(hr))
               break;
   
            hr = NDTWlanGetAuthenticationMode(m_hAdapter, &dwAuthenticationMode);
            if (FAILED(hr))
               break;

            if (Ndis802_11AuthModeWPA == dwAuthenticationMode)
               m_bWPASupported = TRUE;

         }while(0);

         do
         {
            // If WPA is supported then check if WPA adhoc is supported
            if (TRUE == m_bWPASupported)
            {
               hr = NDTWlanSetAuthenticationMode(m_hAdapter, Ndis802_11AuthModeWPANone);
               if (FAILED(hr))
                  break;

               hr = NDTWlanGetAuthenticationMode(m_hAdapter, &dwAuthenticationMode);
               if (FAILED(hr))
                  break;

               if (Ndis802_11AuthModeWPANone== dwAuthenticationMode)
                  m_bWPAAdhocSupported = TRUE;         
            }
         }
         while(0);
         
         // Secondly check Encryption supported

         do
         {
            //  Check if TKIP is supported
            hr = NDTWlanSetEncryptionStatus(m_hAdapter, Ndis802_11Encryption2Enabled);
            if (FAILED(hr))
               break;

            hr = NDTWlanGetEncryptionStatus(m_hAdapter, &dwEncryptionStatus);
            if (FAILED(hr))
               break;

            if (Ndis802_11Encryption2Enabled == dwEncryptionStatus || Ndis802_11Encryption2KeyAbsent)
               m_bTKIPSupported = TRUE;
         }
         while(0);

         do
         {
            // Check if AES is supported
            hr = NDTWlanSetEncryptionStatus(m_hAdapter, Ndis802_11Encryption3Enabled);
            if (FAILED(hr))
               break;

            hr = NDTWlanGetEncryptionStatus(m_hAdapter, &dwEncryptionStatus);
            if (FAILED(hr))
               break;

            if (Ndis802_11Encryption3Enabled == dwEncryptionStatus || Ndis802_11Encryption3KeyAbsent)
               m_bAESSupported= TRUE;
         }
         while(0);
         
      }
      else
      {
         m_bWPA2Supported = TRUE;
         m_NumPMKIDsSupported = pnCapability->NoOfPMKIDs;

         // Get extended capabilities
         for (ix = 0; ix<pnCapability->NoOfAuthEncryptPairsSupported;ix++)
         {
            switch (pnCapability->AuthenticationEncryptionSupported[ix].AuthModeSupported)
            {
               case Ndis802_11AuthModeWPA:
                   m_bWPASupported = TRUE;
                  // WPA:TKIP
                  if (Ndis802_11Encryption2Enabled == pnCapability->AuthenticationEncryptionSupported[ix].EncryptStatusSupported ) 
                     m_bWPATKIPSupported = TRUE;
                  
                  // WPA:AES
                  if(Ndis802_11Encryption3Enabled == pnCapability->AuthenticationEncryptionSupported[ix].EncryptStatusSupported) 
                     m_bWPAAESSupported = TRUE;
                  break;
                  
               case Ndis802_11AuthModeWPAPSK:
                  // WPAPSK:TKIP
                  if(Ndis802_11Encryption2Enabled == pnCapability->AuthenticationEncryptionSupported[ix].EncryptStatusSupported ) 
                     m_bWPAPSKTKIPSupported = TRUE;

                  // WPAPSK:AES
                  if (Ndis802_11Encryption3Enabled == pnCapability->AuthenticationEncryptionSupported[ix].EncryptStatusSupported)
                     m_bWPAPSKAESSupported = TRUE;
                  break;
                  
               case Ndis802_11AuthModeWPA2:
                  // WPA2:TKIP
                  if(Ndis802_11Encryption2Enabled == pnCapability->AuthenticationEncryptionSupported[ix].EncryptStatusSupported )
                     m_bWPA2TKIPSupported = TRUE;

                  // WPA2:AES
                  if(Ndis802_11Encryption3Enabled == pnCapability->AuthenticationEncryptionSupported[ix].EncryptStatusSupported) 
                     m_bWPA2AESSupported = TRUE;
                  break;
                  
               case Ndis802_11AuthModeWPA2PSK:
                  // WPA2PSK:TKIP
                  if(Ndis802_11Encryption2Enabled == pnCapability->AuthenticationEncryptionSupported[ix].EncryptStatusSupported) 
                     m_bWPA2PSKTKIPSupported = TRUE;
                  
                  // WPA2PSK:AES
                  if(Ndis802_11Encryption3Enabled == pnCapability->AuthenticationEncryptionSupported[ix].EncryptStatusSupported)
                     m_bWPA2PSKAESSupported = TRUE;
                  break;
                  
               case Ndis802_11AuthModeWPANone:
                  // WPANone:TKIP
                  if(Ndis802_11Encryption2Enabled == pnCapability->AuthenticationEncryptionSupported[ix].EncryptStatusSupported) 
                     m_bWPANoneTKIPSupported = TRUE;
                  
                  // WPANone:AES
                  if(Ndis802_11Encryption3Enabled == pnCapability->AuthenticationEncryptionSupported[ix].EncryptStatusSupported) 
                     m_bWPANoneAESSupported = TRUE;
                  break;
                  
                }
         }
      }

      if (pnCapability)
         free(pnCapability);
      
      //If device supports WPA, check if it supports key mapping keys
      if (TRUE == m_bWPASupported)
      {
         BYTE arrBssid[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
         DWORD dwSize = FIELD_OFFSET(NDIS_802_11_KEY, KeyMaterial[strlen((char *)WLAN_KEY_TKIP)]);
         PNDIS_802_11_KEY pnKey = (PNDIS_802_11_KEY)malloc(dwSize);

         pnKey->Length = dwSize;
         pnKey->KeyIndex = 0xC0000000;
         pnKey->KeyLength = strlen((char *)WLAN_KEY_TKIP);
         memcpy(pnKey->KeyMaterial,WLAN_KEY_TKIP,pnKey->KeyLength);      
         memcpy(pnKey->BSSID, arrBssid,sizeof(pnKey->BSSID));
         
         hr = NDTWlanSetAddKey(m_hAdapter, pnKey, dwSize);
         if (NOT_SUCCEEDED(hr))
            m_bKeyMappingKeysSupported = FALSE;
         else
            m_bKeyMappingKeysSupported = TRUE;

         free(pnKey);
      }

      //MediaStreamSupported
      //Query OID to see if it is supported
      DWORD dwMediaStreamMode;
      hr = NDTWlanGetMediaStreamMode(m_hAdapter, &dwMediaStreamMode);

      //If it is supported verify it returns a valid enumeration value
      if (!NOT_SUCCEEDED(hr))
      {
         if( Ndis802_11MediaStreamOn == dwMediaStreamMode || Ndis802_11MediaStreamOff == dwMediaStreamMode) 
            m_bMediaStreamSupported = TRUE;
      }

      free(pvNetworkTypesInUse);
   }

   // SetSession
   m_bSessionSet = TRUE;

   // We will mask out other errors while querying capabilities as unsupported capabilities return errors
   hr = S_OK;    
   return hr;
}

HRESULT CWlanCapability::PrintCapablities()
{
   HRESULT hr=S_OK;    

//   cleanup:
   return hr;
}


