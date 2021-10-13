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
#include "NDTError.h"
#include "NDTLib.h"
#include "NDTLibEx.h"
#include "NDTLibWlan.h"
#include "NDTNDIS.h"
#include "Capability.h"
#include "wpadata.h"
#include "Supplicant.h"
#include "ndtlog.h"
#include "ndtlibwlan.h"

//Capability object for Test & Support adapters.
CWlanCapability* g_pCapabilityTest = NULL;
CWlanCapability* g_pCapabilitySupp = NULL;
   
CSupplicant* g_pSupplicant = NULL;

//Queries OID_802_11_NETWORK_TYPE_IN_USE & returns network subtype value for the physical layer.
//Possible values for NDIS_802_11_NETWORK_TYPE defined in ntddndis.h
//OID_802_11_NETWORK_TYPE_IN_USE is Mandatory
HRESULT NDTWlanGetNetworkTypeInUse(HANDLE hAdapter, PDWORD pdwNetworkTypeInUse)
{
	NDIS_OID ndisOid = OID_802_11_NETWORK_TYPE_IN_USE;
	UINT cbBuffer = sizeof(DWORD);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanGetNetworkTypeInUse"));

	if (!pdwNetworkTypeInUse)
		return E_FAIL;

	return NDTQueryInfo(hAdapter, ndisOid, pdwNetworkTypeInUse, cbBuffer, &cbUsed, &cbRequired);
}

//Uses OID_802_11_NETWORK_TYPE_IN_USE to set network subtype value for the physical layer.
//Possible values for NDIS_802_11_NETWORK_TYPE defined in ntddndis.h
//OID_802_11_NETWORK_TYPE_IN_USE is Mandatory
HRESULT NDTWlanSetNetworkTypeInUse(HANDLE hAdapter, DWORD dwNetworkTypeInUse)
{
	NDIS_OID ndisOid = OID_802_11_NETWORK_TYPE_IN_USE;
	UINT cbBuffer = sizeof(DWORD);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanSetNetworkTypeInUse"));

	return NDTSetInfo(hAdapter, ndisOid, &dwNetworkTypeInUse, cbBuffer, &cbUsed, &cbRequired);
}

//The OID_802_11_NETWORK_TYPES_SUPPORTED OID requests the miniport driver to return an array of 
//all physical layer network subtypes that the IEEE 802.11 NIC and the driver support.
//OID OID_802_11_NETWORK_TYPES_SUPPORTED is recommended.
//If Buffer is too short then this function returns NDT_STATUS_BUFFER_TOO_SHORT & required size in 
//*pdwBufSize.
HRESULT NDTWlanGetNetworkTypesSuppported(HANDLE hAdapter, PVOID pdwNetworkTypesInUse, PDWORD pdwBufSize)
{
	NDIS_OID ndisOid = OID_802_11_NETWORK_TYPES_SUPPORTED;
	UINT cbBuffer = (UINT)*pdwBufSize;
	UINT cbUsed = 0;
	UINT cbRequired = 0;
	HRESULT hr = E_FAIL;

     NDTLogVbs(_T("NDTWlanGetNetworkTypesSuppported"));

	if (!pdwNetworkTypesInUse)
		return hr;

	hr = NDTQueryInfo(hAdapter, ndisOid, pdwNetworkTypesInUse, cbBuffer, &cbUsed, &cbRequired);

	if (FAILED(hr))
	{
		if ((hr == NDT_STATUS_BUFFER_OVERFLOW) || (hr == NDT_STATUS_INVALID_LENGTH) 
			|| (hr==NDT_STATUS_BUFFER_TOO_SHORT))
		{
			hr = NDT_STATUS_BUFFER_TOO_SHORT;
			*pdwBufSize = cbRequired;
		}
	}
	return hr;
}

// Prepares the STA to connect to a WEP AP and calls NDTWlanSetSsid to associate
//A connect event must be generated 
// OID_802_11_SSID is mandatory
HRESULT NDTWlanAssociate(HANDLE hAdapter, DWORD dwInfrastructureMode, 
      DWORD dwAuthenticationMode, PDWORD pdwEncryption, ULONG ulKeyLength,
      ULONG ulKeyIndex, PBYTE pbKeyMaterial, NDIS_802_11_MAC_ADDRESS* pBssid, 
      NDIS_802_11_SSID ssid, BOOL bIsBssid, PULONG pulTimeout)
{
   HRESULT hr = E_FAIL;
   UCHAR pucDestAddr[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
   UCHAR *aucDestAddrs[1];
   HANDLE hSend;
   HANDLE hStatus;
   ULONG cbAddr = 6;
   ULONG ulPacketsSent, ulSendTime,ulSendPacketsCompleted,ulSendBytesSent;
   TCHAR szText [80];  
   GetAuthModeText(szText,dwAuthenticationMode);   
   NDTLogVbs(_T("NDTWlanAssociate: Authentication mode:%s KeyLength:%d KeyIndex:0x%x"),szText,
      ulKeyLength,ulKeyIndex);
   GetInfrastructureModeText(szText , dwInfrastructureMode);
   NDTLogVbs(_T("InfrastructureMode:%s "), szText);   
   GetEncryptionText(szText , *pdwEncryption);
   NDTLogVbs(_T("Encryption:%s "), szText);
   GetSsidText(szText,ssid);
   NDTLogVbs(_T("Ssid:%s"));  
   GetInfrastructureModeText(szText , dwInfrastructureMode);
   NDTLogVbs(_T("InfrastructureMode:%s "), szText);


   hr = NDTClearCounters(hAdapter);      

   hr = NDTWlanSetInfrastructureMode(hAdapter, dwInfrastructureMode);
   if (FAILED(hr))
      return hr;

   hr = NDTWlanSetAuthenticationMode(hAdapter, dwAuthenticationMode);
   if (FAILED(hr))
      return hr;

   // If the key material is null, then don't try to enable encryption
   if (NULL != pbKeyMaterial) 
   {
      // If bssid is not null then use the AddKey() function
      if (NULL != pBssid) 
      {
         DWORD dwSize = FIELD_OFFSET(NDIS_802_11_KEY, KeyMaterial[ulKeyLength]);
         PNDIS_802_11_KEY pnKey = (PNDIS_802_11_KEY)malloc(dwSize);            
         
         pnKey->Length =dwSize;
         pnKey->KeyIndex = ulKeyIndex;
         pnKey->KeyLength = ulKeyLength;
         memcpy(pnKey->BSSID , pBssid, sizeof(pnKey->BSSID));
            
         memcpy(pnKey->KeyMaterial, pbKeyMaterial, ulKeyLength);         

         hr = NDTWlanSetAddKey(hAdapter, pnKey, dwSize);
         if (FAILED(hr)) 
            return hr;

         free(pnKey);
      }
      //otherwise use the AddWep() function
      else
      {
         DWORD dwSize = FIELD_OFFSET(NDIS_802_11_WEP, KeyMaterial[ulKeyLength]);
         PNDIS_802_11_WEP pnWEPKey = (PNDIS_802_11_WEP) malloc(dwSize);

         pnWEPKey->Length = dwSize;
         pnWEPKey->KeyIndex = ulKeyIndex;
         pnWEPKey->KeyLength = ulKeyLength;
         memcpy(pnWEPKey->KeyMaterial , pbKeyMaterial,ulKeyLength);
         
         hr = NDTWlanSetAddWep(hAdapter, pnWEPKey, dwSize);
         if (FAILED(hr)) 
            return hr;

         free(pnWEPKey);
      }      
   }

   if (NULL != pdwEncryption)
   {
      hr = NDTWlanSetEncryptionStatus(hAdapter, *pdwEncryption);
      if (FAILED(hr))
         return hr;
   }

   if (NULL != pulTimeout)
   {
      hr = NDTStatus(hAdapter, NDIS_FROM_HRESULT(NDT_STATUS_MEDIA_CONNECT), &hStatus);

      if (FAILED(hr)) 
         return hr;
         
   }   

   if (bIsBssid) 
   {
      hr =NDTWlanSetBssid(hAdapter, (NDIS_802_11_MAC_ADDRESS*)ssid.Ssid);
      if (FAILED(hr))
         return hr;
   }
   else
   {
      hr = NDTWlanSetSsid(hAdapter, &ssid);
      if (FAILED(hr))
         return hr;
   }

   if (NULL != pulTimeout)
   {
      hr = NDTStatusWait(hAdapter, hStatus, (*pulTimeout) * 1000);

      if ( FAILED(hr) || (hr == NDT_STATUS_PENDING) ) 
         return hr;
   }
   
   aucDestAddrs[0] = pucDestAddr;
   hr = NDTSend(hAdapter, cbAddr, NULL, 1, aucDestAddrs, NDT_RESPONSE_NONE,
         NDT_PACKET_TYPE_FIXED, 250, 10, 0, 0, &hSend);
   
   ulPacketsSent = 0;
   ulSendPacketsCompleted = 0;
   hr = NDTSendWait(hAdapter, hSend,INFINITE, &ulPacketsSent , &ulSendPacketsCompleted,
         NULL, NULL, NULL, &ulSendTime, &ulSendBytesSent, NULL);
   
  
   //   NDTGetCounter(hAdapter, ULONG nIndex, ULONG * pnValue)   
   // Doc says this resets counters so using ClearCounters instead 
   // NDTClearCounters(hAdapter);

   NDTLogDbg(_T("WlanAssociate: Associated succesfully to WEP AP"));
   return NDT_STATUS_SUCCESS;
}

//Requests that the miniport driver command its NIC to disassociate from the current service 
//set and turn off the radio.
//OID OID_802_11_DISASSOCIATE is Mandatory.
//A disconnect event must be generated when OID_802_11_DISASSOCIATE is set.
HRESULT NDTWlanDisassociate(HANDLE hAdapter)
{
	NDIS_OID ndisOid = OID_802_11_DISASSOCIATE;
	UINT cbBuffer = 0;
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanDisassociate"));

      HRESULT hr = NDTSetInfo(hAdapter, ndisOid, NULL, cbBuffer, &cbUsed, &cbRequired);
  /*    if (NDIS_STATUS_ADAPTER_NOT_READY == NDIS_FROM_HRESULT(hr))
      {
         NDTLogMsg(_T("##Card returned error as we are trying to disassociate the card which is not associated##"));
         NDTLogMsg(_T("##This is not flagged as error"));
         hr = NDT_STATUS_SUCCESS;
      }

 */
      return hr;
}

//Returns either Infrastructure or IBSS, unknown.
//OID OID_802_11_INFRASTRUCTURE_MODE is Mandatory.
//Possible values for NDIS_802_11_NETWORK_INFRASTRUCTURE defined in ntddndis.h.
HRESULT NDTWlanGetInfrastructureMode(HANDLE hAdapter, PDWORD pdwInfrastructureMode)
{
	NDIS_OID ndisOid = OID_802_11_INFRASTRUCTURE_MODE;
	UINT cbBuffer = sizeof(DWORD);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanGetInfrastructureMode"));

	return NDTQueryInfo(hAdapter, ndisOid, pdwInfrastructureMode, cbBuffer, &cbUsed, &cbRequired);
}

//Sets mode to Infrastructure or IBSS, or automatic switch between the two.
//OID OID_802_11_INFRASTRUCTURE_MODE is Mandatory.
//This also resets the network association algorithm.
//Possible values for NDIS_802_11_NETWORK_INFRASTRUCTURE defined in ntddndis.h.
HRESULT NDTWlanSetInfrastructureMode(HANDLE hAdapter, DWORD dwInfrastructureMode)
{
	NDIS_OID ndisOid = OID_802_11_INFRASTRUCTURE_MODE;
	UINT cbBuffer = sizeof(DWORD);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanSetInfrastructureMode"));
     
	return NDTSetInfo(hAdapter, ndisOid, &dwInfrastructureMode, cbBuffer, &cbUsed, &cbRequired);
}

//Returns Authentication mode Open, shared or WPA etc.
//OID OID_802_11_AUTHENTICATION_MODE is Mandatory.
//Possible values for NDIS_802_11_AUTHENTICATION_MODE defined in ntddndis.h.
HRESULT NDTWlanGetAuthenticationMode(HANDLE hAdapter, PDWORD pdwAuthenticationMode)
{
	NDIS_OID ndisOid = OID_802_11_AUTHENTICATION_MODE;
	UINT cbBuffer = sizeof(DWORD);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanGetAuthenticationMode"));
     
	return NDTQueryInfo(hAdapter, ndisOid, pdwAuthenticationMode, cbBuffer, &cbUsed, &cbRequired);
}

//Sets Authentication mode to Open or shared or WPA etc.
//OID OID_802_11_AUTHENTICATION_MODE is Mandatory.
//Possible values for NDIS_802_11_AUTHENTICATION_MODE defined in ntddndis.h.
HRESULT NDTWlanSetAuthenticationMode(HANDLE hAdapter, DWORD dwAuthenticationMode)
{
	NDIS_OID ndisOid = OID_802_11_AUTHENTICATION_MODE;
	UINT cbBuffer = sizeof(DWORD);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanSetAuthenticationMode"));
     
	return NDTSetInfo(hAdapter, ndisOid, &dwAuthenticationMode, cbBuffer, &cbUsed, &cbRequired);
}

//Returns current encryption status.
//OID OID_802_11_ENCRYPTION_STATUS is Mandatory. But NDIS_STATUS_NOT_SUPPORTED means that
//NIC does not support encryption.
//This OID was the OID_802_11_WEP_STATUS OID
//Possible values for NDIS_802_11_WEP_STATUS or NDIS_802_11_ENCRYPTION_STATUS defined in ntddndis.h.
HRESULT NDTWlanGetEncryptionStatus(HANDLE hAdapter, PDWORD pdwEncryptionStatus)
{
	NDIS_OID ndisOid = OID_802_11_ENCRYPTION_STATUS;
	UINT cbBuffer = sizeof(DWORD);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanGetEncryptionStatus"));
     
	return NDTQueryInfo(hAdapter, ndisOid, pdwEncryptionStatus, cbBuffer, &cbUsed, &cbRequired);
}

//Permits to enable or disable encryption and the encryption modes.
//OID OID_802_11_ENCRYPTION_STATUS is Mandatory. But NDIS_STATUS_NOT_SUPPORTED means that
//NIC does not support encryption.
//This OID was the OID_802_11_WEP_STATUS OID
//Possible values for NDIS_802_11_WEP_STATUS or NDIS_802_11_ENCRYPTION_STATUS defined in ntddndis.h.
HRESULT NDTWlanSetEncryptionStatus(HANDLE hAdapter, DWORD dwEncryptionStatus)
{
	NDIS_OID ndisOid = OID_802_11_ENCRYPTION_STATUS;
	UINT cbBuffer = sizeof(DWORD);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanSetEncryptionStatus"));
     
	return NDTSetInfo(hAdapter, ndisOid, &dwEncryptionStatus, cbBuffer, &cbUsed, &cbRequired);
}

//Returns the current NDIS_802_11_POWER_MODE.
//OID OID_802_11_POWER_MODE is recommended.
//Possible values for NDIS_802_11_POWER_MODE defined in ntddndis.h.
HRESULT NDTWlanGetPowerMode(HANDLE hAdapter, PDWORD pdwPowerMode)
{
	NDIS_OID ndisOid = OID_802_11_POWER_MODE;
	UINT cbBuffer = sizeof(DWORD);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanGetPowerMode"));
     
	return NDTQueryInfo(hAdapter, ndisOid, pdwPowerMode, cbBuffer, &cbUsed, &cbRequired);
}

//Sets the current value of NDIS_802_11_POWER_MODE.
//OID OID_802_11_POWER_MODE is recommended.
//Possible values for NDIS_802_11_POWER_MODE defined in ntddndis.h.
HRESULT NDTWlanSetPowerMode(HANDLE hAdapter, PDWORD pdwPowerMode)
{
	NDIS_OID ndisOid = OID_802_11_POWER_MODE;
	UINT cbBuffer = sizeof(DWORD);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanSetPowerMode"));
     
	return NDTSetInfo(hAdapter, ndisOid, pdwPowerMode, cbBuffer, &cbUsed, &cbRequired);
}

//Returns the SSID with which the NIC is associated. The driver returns 0 SSIDLength 
//if the NIC is not associated with any SSID. 
//OID OID_802_11_SSID is Mandatory.
HRESULT NDTWlanGetSsid(HANDLE hAdapter, PNDIS_802_11_SSID psSSID)
{
	NDIS_OID ndisOid = OID_802_11_SSID;
	UINT cbBuffer = sizeof(NDIS_802_11_SSID);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanGetSsid"));
     
	return NDTQueryInfo(hAdapter, ndisOid, psSSID, cbBuffer, &cbUsed, &cbRequired);
}

//Sets the SSID to specified value. An empty string (that is, a string with the first 
//byte set to zero) requests the 802.11 NIC to associate with any available SSID.
//OID OID_802_11_SSID is Mandatory.
HRESULT NDTWlanSetSsid(HANDLE hAdapter, PNDIS_802_11_SSID psSSID)
{
	NDIS_OID ndisOid = OID_802_11_SSID;
	UINT cbBuffer = sizeof(NDIS_802_11_SSID);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanSetSsid"));
     
	return NDTSetInfo(hAdapter, ndisOid, psSSID, cbBuffer, &cbUsed, &cbRequired);
}

//Returns the MAC address of the associated AP or IBSS MAC address if in ad hoc mode. 
//Returns an error code of NDIS_STATUS_ADAPTER_NOT_READY if the NIC is not associated. 
//OID OID_802_11_BSSID is Mandatory.
HRESULT NDTWlanGetBssid(HANDLE hAdapter, NDIS_802_11_MAC_ADDRESS* pbMac)
{
	NDIS_OID ndisOid = OID_802_11_BSSID;
	UINT cbBuffer = sizeof(NDIS_802_11_MAC_ADDRESS);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanGetBssid"));
     
	return NDTQueryInfo(hAdapter, ndisOid, pbMac, cbBuffer, &cbUsed, &cbRequired);
}

//Sets the MAC address of the desired AP. 
//Returns an error code of NDIS_STATUS_ADAPTER_NOT_READY if the NIC is not associated. 
//OID OID_802_11_BSSID is Mandatory.
HRESULT NDTWlanSetBssid(HANDLE hAdapter, NDIS_802_11_MAC_ADDRESS* pbMac)
{
	NDIS_OID ndisOid = OID_802_11_BSSID;
	UINT cbBuffer = sizeof(NDIS_802_11_MAC_ADDRESS);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanSetBssid"));
     
	return NDTSetInfo(hAdapter, ndisOid, pbMac, cbBuffer, &cbUsed, &cbRequired);
}

//requests that the miniport driver remove the key at the specified key index for 
//the current association. 
//OID OID_802_11_REMOVE_KEY is Mandatory for WPA.
HRESULT NDTWlanSetRemoveKey(HANDLE hAdapter, PNDIS_802_11_REMOVE_KEY psRemoveKey)
{
	NDIS_OID ndisOid = OID_802_11_REMOVE_KEY;
	UINT cbBuffer = sizeof(NDIS_802_11_REMOVE_KEY);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanSetRemoveKey"));
     
	return NDTSetInfo(hAdapter, ndisOid, psRemoveKey, cbBuffer, &cbUsed, &cbRequired);
}

//Requests that the miniport driver direct the IEEE 802.11 NIC to request a survey of BSSs. 
//OID OID_802_11_BSSID_LIST_SCAN is Mandatory.
HRESULT NDTWlanSetBssidListScan(HANDLE hAdapter)
{
	NDIS_OID ndisOid = OID_802_11_BSSID_LIST_SCAN;
	UINT cbBuffer = 0;
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanSetBssidListScan"));
     
	return NDTSetInfo(hAdapter, ndisOid, NULL, cbBuffer, &cbUsed, &cbRequired);
}

//Returns the list of all BSSIDs detected including their attributes specified in the 
//data structure.
//OID OID_802_11_BSSID_LIST is Mandatory.
//If Buffer is too short then this function returns NDT_STATUS_BUFFER_TOO_SHORT & required size in 
//*pdwSize.
HRESULT NDTWlanGetBssidList(HANDLE hAdapter, PNDIS_802_11_BSSID_LIST_EX psBSSIDListEx, PDWORD pdwSize)
{
	NDIS_OID ndisOid = OID_802_11_BSSID_LIST;
	UINT cbBuffer = (UINT)*pdwSize;
	UINT cbUsed = 0;
	UINT cbRequired = 0;
	HRESULT hr = E_FAIL;
     NDTLogVbs(_T("NDTWlanGetBssidList"));
     
	if (!psBSSIDListEx)
		return hr;

	hr = NDTQueryInfo(hAdapter, ndisOid, psBSSIDListEx, cbBuffer, &cbUsed, &cbRequired);

	if (FAILED(hr))
	{
		if ((hr == NDT_STATUS_BUFFER_OVERFLOW) || (hr == NDT_STATUS_INVALID_LENGTH) 
			|| (hr==NDT_STATUS_BUFFER_TOO_SHORT))
		{
			hr = NDT_STATUS_BUFFER_TOO_SHORT;
			*pdwSize = cbRequired;
		}
	}
	return hr;
}

//Requests that the miniport driver set a key to a specified value. A key can be a preshared key
//(a key that is provided to the NICs before use) for authentication, encryption, or both. 
//OID OID_802_11_ADD_KEY is Mandatory for WPA.
// It will convert a 64 digit hexadecimal string (if length is 64) to hex form
HRESULT NDTWlanSetAddKey(HANDLE hAdapter, PNDIS_802_11_KEY psKey, DWORD dwSize)
{
	NDIS_OID ndisOid = OID_802_11_ADD_KEY;
	UINT cbBuffer = dwSize;
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanSetAddKey"));

     NDTLogVbs(_T("AddKey (Length = %d, Key Index = %x, Key Length = %d) called "), psKey->Length,
         psKey->KeyIndex, psKey->KeyLength);
     
     if (64 == psKey->KeyLength)
     {
         NDTLogDbg(_T("Converting 64 bit hex string to a 32 byte arry of hex values"));
         // We have received a string containig the 64 hex numbers, which we need to convert to hex bytes
         UINT index =0;
         ULONG ulNewLength; 
         UCHAR* auString = (UCHAR *)malloc(psKey->KeyLength);
         
         memcpy(auString,psKey->KeyMaterial, psKey->KeyLength);

         // we will calculate new length as we recreate the KeyMaterial
         ulNewLength = 0;
         for(index =0; index<psKey->KeyLength; index++)
         {
            UCHAR uHi= 0;
            UCHAR uLo = 0;
            uHi = auString[index];
            if (uHi >= '0' && uHi <= '9')
               uHi = uHi - '0';            
           else
               if (uHi >= 'A' && uHi <= 'F')
                  uHi= uHi - 'A' + 10;
           else
               if (uHi >= 'a' && uHi <= 'f')
                  uHi = uHi - 'a' + 10;               
            else
                return NDT_STATUS_INVALID_DATA;

            index++;
            uLo = auString[index];
            if (uLo >= '0' && uLo <= '9')
               uLo = uLo - '0';
           else
               if (uLo >= 'A' && uLo <= 'F')
                  uLo = uLo - 'A' +10;
           else
               if (uLo >= 'a' && uLo <= 'f')
                  uLo = uLo - 'a' + 10;
            else
                return NDT_STATUS_INVALID_DATA;

            psKey->KeyMaterial[ulNewLength] = (uHi << 4) |uLo;               
            ulNewLength++;
         }

         // Change length values 
         psKey->KeyLength = ulNewLength;
         psKey->Length = FIELD_OFFSET(NDIS_802_11_KEY, KeyMaterial[ulNewLength]);
         free(auString);
     }
     else
         NDTLogDbg(_T("Key length passed in is not 64 bytes"));
         
	return NDTSetInfo(hAdapter, ndisOid, psKey, cbBuffer, &cbUsed, &cbRequired);
}

//Returns the information elements used in the last association/reassociation request to an AP 
//and in the last association/reassociation response from the AP
//OID OID_802_11_ASSOCIATION_INFORMATION is Mandatory for WPA.
//If Buffer is too short then this function returns NDT_STATUS_BUFFER_TOO_SHORT & required size in 
//*pdwSize.
HRESULT NDTWlanGetAssociationInformation(HANDLE hAdapter, PNDIS_802_11_ASSOCIATION_INFORMATION psAssoInfo
										 , PDWORD pdwSize)
{
	NDIS_OID ndisOid = OID_802_11_ASSOCIATION_INFORMATION;
	UINT cbBuffer = (UINT)*pdwSize;
	UINT cbUsed = 0;
	UINT cbRequired = 0;
	HRESULT hr = E_FAIL;
     NDTLogVbs(_T("NDTWlanGetAssociationInformation"));
     
	if (!psAssoInfo)
		return hr;

	hr = NDTQueryInfo(hAdapter, ndisOid, psAssoInfo, cbBuffer, &cbUsed, &cbRequired);

	if (FAILED(hr))
	{
		if ((hr == NDT_STATUS_BUFFER_OVERFLOW) || (hr == NDT_STATUS_INVALID_LENGTH) 
			|| (hr==NDT_STATUS_BUFFER_TOO_SHORT))
		{
			hr = NDT_STATUS_BUFFER_TOO_SHORT;
			*pdwSize = cbRequired;
		}
	}
	return hr;
}


//Sets the desired information elements used in the last association/reassociation request to an AP 
//and in the last association/reassociation response from the AP
//OID OID_802_11_ASSOCIATION_INFORMATION is Mandatory for WPA.
HRESULT NDTWlanSetAssociationInformation(HANDLE hAdapter, PNDIS_802_11_ASSOCIATION_INFORMATION psAssoInfo
										 , DWORD dwSize)
{
	NDIS_OID ndisOid = OID_802_11_ASSOCIATION_INFORMATION;
	UINT cbBuffer = (UINT)dwSize;
	UINT cbUsed = 0;
	UINT cbRequired = 0;
	HRESULT hr = E_FAIL;
     NDTLogVbs(_T("NDTWlanSetAssociationInformation"));
     
	if (!psAssoInfo)
		return hr;

	return NDTSetInfo(hAdapter, ndisOid, psAssoInfo, cbBuffer, &cbUsed, &cbRequired);
}


//Requests IEEE 802.11 NIC Driver/Firmware to reload the available default settings (from registry 
//key or flash settings) for the specified type field into the running configuration.
//All current encryption and integrity keys should be discarded before loading default keys.
//OID OID_802_11_RELOAD_DEFAULTS is Mandatory.
HRESULT NDTWlanSetReloadDefaults(HANDLE hAdapter, DWORD dwDefaults)
{
	NDIS_OID ndisOid = OID_802_11_RELOAD_DEFAULTS;
	UINT cbBuffer = sizeof(DWORD);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanSetReloadDefaults"));
     
	return NDTSetInfo(hAdapter, ndisOid, &dwDefaults, cbBuffer, &cbUsed, &cbRequired);
}

//Returns the current radio configuration.
//OID OID_802_11_CONFIGURATION is Mandatory.
//If Buffer is too short then this function returns NDT_STATUS_BUFFER_TOO_SHORT & required size in 
//*pdwSize.
HRESULT NDTWlanGetConfiguration(HANDLE hAdapter, PNDIS_802_11_CONFIGURATION psConfig
										 , PDWORD pdwSize)
{
	NDIS_OID ndisOid = OID_802_11_CONFIGURATION;
	UINT cbBuffer = (UINT)*pdwSize;
	UINT cbUsed = 0;
	UINT cbRequired = 0;
	HRESULT hr = E_FAIL;
     NDTLogVbs(_T("NDTWlanGetConfiguration"));
     
	if (!psConfig)
		return hr;

	hr = NDTQueryInfo(hAdapter, ndisOid, psConfig, cbBuffer, &cbUsed, &cbRequired);

	if (FAILED(hr))
	{
		if ((hr == NDT_STATUS_BUFFER_OVERFLOW) || (hr == NDT_STATUS_INVALID_LENGTH) 
			|| (hr==NDT_STATUS_BUFFER_TOO_SHORT))
		{
			hr = NDT_STATUS_BUFFER_TOO_SHORT;
			*pdwSize = cbRequired;
		}
	}
	return hr;
}

//Sets the current radio configuration. This OID should only be set when the device is not 
//associated with an AP.
//OID OID_802_11_CONFIGURATION is Mandatory.
HRESULT NDTWlanSetConfiguration(HANDLE hAdapter, PNDIS_802_11_CONFIGURATION psConfig
										 , DWORD dwSize)
{
	NDIS_OID ndisOid = OID_802_11_CONFIGURATION;
	UINT cbBuffer = (UINT)dwSize;
	UINT cbUsed = 0;
	UINT cbRequired = 0;
	HRESULT hr = E_FAIL;
     NDTLogVbs(_T("NDTWlanSetConfiguration"));
     
	if (!psConfig)
		return hr;

	return NDTSetInfo(hAdapter, ndisOid, psConfig, cbBuffer, &cbUsed, &cbRequired);
}


//Queries the miniport driver for its supported wireless network authentication and encryption capabilities.
//OID OID_802_11_CONFIGURATION is used for WPA2.
//If Buffer is too short then this function returns NDT_STATUS_BUFFER_TOO_SHORT & required size in 
//*pdwSize.
HRESULT NDTWlanGetCapability(HANDLE hAdapter, PNDIS_802_11_CAPABILITY psCapability
										 , PDWORD pdwSize)
{
	NDIS_OID ndisOid = OID_802_11_CAPABILITY;
	UINT cbBuffer = (UINT)*pdwSize;
	UINT cbUsed = 0;
	UINT cbRequired = 0;
	HRESULT hr = E_FAIL;
     NDTLogVbs(_T("NDTWlanGetCapability"));
     
	if (!psCapability)
		return hr;

	hr = NDTQueryInfo(hAdapter, ndisOid, psCapability, cbBuffer, &cbUsed, &cbRequired);

	if (FAILED(hr))
	{
		if ((hr == NDT_STATUS_BUFFER_OVERFLOW) || (hr == NDT_STATUS_INVALID_LENGTH) 
			|| (hr==NDT_STATUS_BUFFER_TOO_SHORT))
		{
			hr = NDT_STATUS_BUFFER_TOO_SHORT;
			*pdwSize = cbRequired;
		}
	}
	return hr;
}


//Queries the list of WPA2 pre-authentication PMKIDs within the miniport driver's pairwise master key (PMK) cache.
//OID OID_802_11_PMKID is used for WPA2.
//If Buffer is too short then this function returns NDT_STATUS_BUFFER_TOO_SHORT & required size in 
//*pdwSize.
HRESULT NDTWlanGetPMKID(HANDLE hAdapter, PNDIS_802_11_PMKID psPmkid
										 , PDWORD pdwSize)
{
	NDIS_OID ndisOid = OID_802_11_PMKID;
	UINT cbBuffer = (UINT)*pdwSize;
	UINT cbUsed = 0;
	UINT cbRequired = 0;
	HRESULT hr = E_FAIL;

	if (!psPmkid)
		return hr;

	hr = NDTQueryInfo(hAdapter, ndisOid, psPmkid, cbBuffer, &cbUsed, &cbRequired);

	if (FAILED(hr))
	{
		if ((hr == NDT_STATUS_BUFFER_OVERFLOW) || (hr == NDT_STATUS_INVALID_LENGTH) 
			|| (hr==NDT_STATUS_BUFFER_TOO_SHORT))
		{
			hr = NDT_STATUS_BUFFER_TOO_SHORT;
			*pdwSize = cbRequired;
		}
	}
	return hr;
}

//Sets the list of WPA2 pre-authentication PMKIDs within the miniport driver's pairwise master key (PMK) cache.
//OID OID_802_11_PMKID is used for WPA2.
HRESULT NDTWlanSetPMKID(HANDLE hAdapter, PNDIS_802_11_PMKID psPmkid
										 , DWORD dwSize)
{
	NDIS_OID ndisOid = OID_802_11_PMKID;
	UINT cbBuffer = (UINT)dwSize;
	UINT cbUsed = 0;
	UINT cbRequired = 0;
	HRESULT hr = E_FAIL;

	if (!psPmkid)
		return hr;

	return NDTSetInfo(hAdapter, ndisOid, psPmkid, cbBuffer, &cbUsed, &cbRequired);
}


//Requests the miniport driver to return the current statistics for the IEEE 802.11 interface.
//OID OID_802_11_STATISTICS is recommended.
//If Buffer is too short then this function returns NDT_STATUS_BUFFER_TOO_SHORT & required size in 
//*pdwSize.
HRESULT NDTWlanGetStatistics(HANDLE hAdapter, PNDIS_802_11_STATISTICS psStats
										 , PDWORD pdwSize)
{
	NDIS_OID ndisOid = OID_802_11_STATISTICS;
	UINT cbBuffer = (UINT)*pdwSize;
	UINT cbUsed = 0;
	UINT cbRequired = 0;
	HRESULT hr = E_FAIL;
     NDTLogVbs(_T("NDTWlanGetStatistics"));
     
	if (!psStats)
		return hr;

	hr = NDTQueryInfo(hAdapter, ndisOid, psStats, cbBuffer, &cbUsed, &cbRequired);

	if (FAILED(hr))
	{
		if ((hr == NDT_STATUS_BUFFER_OVERFLOW) || (hr == NDT_STATUS_INVALID_LENGTH) 
			|| (hr==NDT_STATUS_BUFFER_TOO_SHORT))
		{
			hr = NDT_STATUS_BUFFER_TOO_SHORT;
			*pdwSize = cbRequired;
		}
	}
	return hr;
}

//Requests the miniport driver to set an IEEE 802.11 wired equivalent privacy (WEP) key 
//to a specified value. A WEP key can be a preshared key (a key that is provided to the NICs 
//before use) for authentication, encryption, or both.
//OID OID_802_11_ADD_WEP is Mandatory.
// The function will convert a 10/26 digit hexadecimal string to hex format
HRESULT NDTWlanSetAddWep(HANDLE hAdapter, PNDIS_802_11_WEP psWEPKey
										 , DWORD dwSize)
{
	NDIS_OID ndisOid = OID_802_11_ADD_WEP;
	UINT cbBuffer = (UINT)dwSize;
	UINT cbUsed = 0;
	UINT cbRequired = 0;
	HRESULT hr = E_FAIL;
     NDTLogVbs(_T("NDTWlanSetAddWep"));

     NDTLogVbs(_T("WEP Key (Length %d Index 0x%x Length %d) being added"), psWEPKey->Length, 
         psWEPKey->KeyIndex, psWEPKey->KeyLength);
     
	if (!psWEPKey)
		return hr;

     if (10 == psWEPKey->KeyLength || 26 == psWEPKey->KeyLength)
     {
         NDTLogDbg(_T("%d byte String containing numbers is converted to its byte form"), psWEPKey->KeyLength);
         // We have received a string containig the numbers, which we need to convert to hex bytes
         UINT index =0;
         ULONG ulNewLength; 
         UCHAR* auString = (UCHAR *)malloc(psWEPKey->KeyLength);
         NDTLogVbs(_T(""));
          
         memcpy(auString,psWEPKey->KeyMaterial, psWEPKey->KeyLength);

         // we will calculate new length as we recreate the KeyMaterial
         ulNewLength = 0;
         for(index =0; index<psWEPKey->KeyLength; index++)
         {
            UCHAR uHi= 0;
            UCHAR uLo = 0;
            uHi = auString[index];
            if (uHi >= '0' && uHi <= '9')
               uHi = uHi - '0';            
           else
               if (uHi >= 'A' && uHi <= 'F')
                  uHi= uHi - 'A' + 10;
           else
               if (uHi >= 'a' && uHi <= 'f')
                  uHi = uHi - 'a' + 10;               
            else
                return NDT_STATUS_INVALID_DATA;

            index++;
            uLo = auString[index];
            if (uLo >= '0' && uLo <= '9')
               uLo = uLo - '0';
           else
               if (uLo >= 'A' && uLo <= 'F')
                  uLo = uLo - 'A' +10;
           else
               if (uLo >= 'a' && uLo <= 'f')
                  uLo = uLo - 'a' + 10;
            else
                return NDT_STATUS_INVALID_DATA;

            psWEPKey->KeyMaterial[ulNewLength] = (uHi << 4) |uLo;               
            ulNewLength++;
         }

         // Change length values 
         psWEPKey->KeyLength = ulNewLength;
         psWEPKey->Length = FIELD_OFFSET(NDIS_802_11_KEY, KeyMaterial[ulNewLength]);
         free(auString);
     }
     else
         NDTLogDbg(_T("Key Material is not being converted to hex form as its length %d is not 10 or 26"),
            psWEPKey->KeyLength);

	return NDTSetInfo(hAdapter, ndisOid, psWEPKey, cbBuffer, &cbUsed, &cbRequired);
}


//Requests that the miniport driver remove the key at the specified key index for the current association.
//OID OID_802_11_REMOVE_WEP is Mandatory.
HRESULT NDTWlanSetRemoveWep(HANDLE hAdapter, DWORD dwKeyIndex)
{
	NDIS_OID ndisOid = OID_802_11_REMOVE_WEP;
	UINT cbBuffer = sizeof(DWORD);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanSetRemoveWep with Index 0x%x called"),dwKeyIndex);
     
	return NDTSetInfo(hAdapter, ndisOid, &dwKeyIndex, cbBuffer, &cbUsed, &cbRequired);
}

//Requests that the miniport driver return its current media streaming status.
//OID OID_802_11_MEDIA_STREAM_MODE is used for 802.11 media streaming.
HRESULT NDTWlanGetMediaStreamMode(HANDLE hAdapter, PDWORD pdwMediaStreamMode)
{
	NDIS_OID ndisOid = OID_802_11_MEDIA_STREAM_MODE;
	UINT cbBuffer = sizeof(DWORD);
	UINT cbUsed = 0;
	UINT cbRequired = 0;
     NDTLogVbs(_T("NDTWlanGetMediaStreamMode"));
     
	return NDTQueryInfo(hAdapter, ndisOid, pdwMediaStreamMode, cbBuffer, &cbUsed, &cbRequired);
}

//Requests the miniport driver to enter media streaming mode or exit media streaming mode.
//OID OID_802_11_MEDIA_STREAM_MODE is used for 802.11 media streaming.
HRESULT NDTWlanSetMediaStreamMode(HANDLE hAdapter, DWORD dwMediaStreamMode)
{
	NDIS_OID ndisOid = OID_802_11_MEDIA_STREAM_MODE;
	UINT cbBuffer = sizeof(DWORD);
	UINT cbUsed = 0;
	UINT cbRequired = 0;

      NDTLogVbs(_T("Setting media stream mode"));
   
	return NDTSetInfo(hAdapter, ndisOid, &dwMediaStreamMode, cbBuffer, &cbUsed, &cbRequired);
}

// Disassociate the card, Remove keys
// Set mode to infrastructure and set a dummy ssid to turn radio back on
HRESULT NDTWlanReset(HANDLE hAdapter, BOOL bRemoveKeys)
{   
   HRESULT hr = E_FAIL;
   UINT ix;
   NDIS_802_11_SSID  dummySsid;

   NDTLogVbs(_T("NDTWlanReset"));

   NDTLogDbg(_T("Disabled disassociate, Some cards like CISCO turn off their radios and dont turn it back on"));   
/*
   hr = NDTWlanDisassociate(hAdapter);
   if (FAILED(hr))
       return hr;
*/
  
   // Remove all keys
   if (bRemoveKeys)
   { 
      NDTLogDbg(_T("Keys are being removed"));
      
      // If we support WPA or WPA2, we need to remove pairwise keys
      NDIS_802_11_CAPABILITY nCapability;
      DWORD dwSize = sizeof(NDIS_802_11_CAPABILITY );
      hr = NDTWlanGetCapability( hAdapter,  &nCapability, &dwSize);
      if (!FAILED(hr))
      {
         NDTLogVbs(_T("WPA\\WPA2 Pairwise Keys being removed"));
         NDIS_802_11_REMOVE_KEY nRemoveKey;
         nRemoveKey.KeyIndex = 0x40000000;
         memcpy(nRemoveKey.BSSID, WLAN_KEYTYPE_DEFAULT, sizeof(nRemoveKey.BSSID));

         // Removing pairwise key
         hr = NDTWlanSetRemoveKey(hAdapter, &nRemoveKey);
         if (FAILED(hr))
             return hr;

         //Remove all other keys
         NDTLogVbs(_T("All other WPA\\WPA2 Keys being removed"));         
         NDIS_802_11_REMOVE_KEY nRemoveAllKey;
         memcpy(nRemoveAllKey.BSSID, WLAN_KEYTYPE_DEFAULT, sizeof(nRemoveKey.BSSID));

         for(ix = 0; ix<=3; ix++)
         {
            nRemoveAllKey.KeyIndex = ix;            
            hr = NDTWlanSetRemoveKey( hAdapter, &nRemoveAllKey);
            if (FAILED(hr))
               return hr;
         }      
      }
      else
      {
         //Remove all WEP keys
         NDTLogVbs(_T("WEP Keys being removed"));
         for(ix = 0; ix<=3; ix++)
         {
               hr = NDTWlanSetRemoveWep(hAdapter, ix);
               if (FAILED(hr))
                  return hr;
         }
      }
   }

   hr = NDTWlanSetInfrastructureMode( hAdapter, Ndis802_11Infrastructure);
   if (FAILED(hr))
      return hr;


   //Set this bogus SSID here to turn the radio back on after the call to disassociate above     
   NDTLogVbs(_T("Setting bogus SSID to turn the radio back on"));
   dummySsid.SsidLength = strlen((char *)WLAN_RADIO_ON_SSID);
   memcpy((dummySsid.Ssid), WLAN_RADIO_ON_SSID, dummySsid.SsidLength);
   hr = NDTWlanSetSsid( hAdapter, &dummySsid);
   if (FAILED(hr))
         return hr;
   
   Sleep(2000);


   //If the device supportes media stream mode, then set it to off just incase  
   // 
   // Media stream not supported yet in CE
   //
   /*
   DWORD dwMediaStreamMode;
   HRESULT hIsSupported = S_OK;
   hIsSupported = NDTWlanGetMediaStreamMode(hAdapter, &dwMediaStreamMode);
   if (!FAILED(hr) && ((dwMediaStreamMode == Ndis802_11MediaStreamOn) || (dwMediaStreamMode == Ndis802_11MediaStreamOff)))
   {
         dwMediaStreamMode = Ndis802_11MediaStreamOff;
         DEBUGMSG(TRUE,(_T("NDTWlanSetMediaStreamMode")));
         hr = NDTWlanSetMediaStreamMode(hAdapter, dwMediaStreamMode);
         if (FAILED(hr) || NDIS_STATUS_PENDING == hr)
            return hr;
   }
   */

   return hr;
}

// Return appropriate capability object depending on the adapter handle passed in
CWlanCapability* GetCapabilityObject(HANDLE hAdapter)
{
	if ((g_pCapabilityTest) && (g_pCapabilityTest->m_hAdapter == hAdapter))
		return g_pCapabilityTest;
	else if ((g_pCapabilitySupp) && (g_pCapabilitySupp->m_hAdapter == hAdapter))
		return g_pCapabilitySupp;
	else
		return NULL;
}

// The second parameter is used to decide which capability object needs to be created
// If its set to 1 the capability object for the support card is created
// It has a default value of 0 so that 1 card tests dont need to pass this value
// Device capabilities are queried and stored
HRESULT NDTWlanInitializeTest(HANDLE hAdapter, BOOL bSupportCard)
{
   HRESULT hr = E_FAIL;
   NDTLogVbs(_T("NDTWlanInitializeTest"));
   CWlanCapability* pOWlanCap = NULL;

   if (!bSupportCard)
   {
	   if (!g_pCapabilityTest)
	   {
		   g_pCapabilityTest = new CWlanCapability(hAdapter);
		   pOWlanCap = g_pCapabilityTest;
	   }
	   else
	   {
              NDTLogWrn(_T("Attempt to create Test Capablity object when it already exists"));
              NDTLogWrn(_T("Initialize test will return error"));
		   return hr;
	   }
   }
   else
   {
	   if (!g_pCapabilitySupp)
	   {
		   g_pCapabilitySupp = new CWlanCapability(hAdapter);
		   pOWlanCap = g_pCapabilitySupp;
	   }
	   else
	   {
		   NDTLogWrn(_T("Attempt to create Support Capablity object when it already exists"));
              NDTLogWrn(_T("Initialize test will return error"));
		   return hr;
	   }
   }
   
   if (!pOWlanCap)
	   return hr;

   NDTLogDbg(_T("Get Device Capabillities"));
   hr = pOWlanCap->GetDeviceCapabilities();
   if (FAILED(hr))
   {
      NDTLogWrn(_T("Failed to get device capabilities"));
      return hr;
   }

   NDTLogDbg(_T("Print Device Capabillities"));   
   hr = pOWlanCap->PrintCapablities();

   hr = NDTWlanReset(hAdapter, TRUE);
   NDTLogDbg(_T("NDTWlanReset returned %x"),hr);

   return hr;
}

// This will cleanup capability object corresponding to the adapter passed in  and 
// Reset the Wlan state
HRESULT NDTWlanCleanupTest(HANDLE hAdapter)
{
   HRESULT hr = E_FAIL;
   NDTLogVbs(_T("NDTWlanCleanupTest"));

   if ((g_pCapabilityTest) && (g_pCapabilityTest->m_hAdapter == hAdapter))
   {
	   delete g_pCapabilityTest;
	   g_pCapabilityTest = NULL;
   }
   else if ((g_pCapabilitySupp) && (g_pCapabilitySupp->m_hAdapter == hAdapter))
   {
	   delete g_pCapabilitySupp;
	   g_pCapabilitySupp = NULL;
   }
   else
	   return hr;

   return NDTWlanReset(hAdapter, TRUE);
}

HRESULT NDTWlanGetDeviceType(HANDLE hAdapter, PNDIS_802_11_DEVICE_TYPE pnWlanDeviceType)
{
   HRESULT hr = E_FAIL;
   CWlanCapability* pWlanCapability;
   pWlanCapability = GetCapabilityObject(hAdapter);
   NDTLogVbs(_T("NDTWlanGetDeviceType"));
   
   if (NULL == pWlanCapability)
      return hr;

   *pnWlanDeviceType = (pWlanCapability->m_dwDeviceType);
   hr = NDT_STATUS_SUCCESS;
   
   return hr;
}

HRESULT NDTWlanIsWPASupported(HANDLE hAdapter, PBOOL pbResult)
{
   HRESULT hr = E_FAIL;
   CWlanCapability* pWlanCapability;
   NDTLogVbs(_T("NDTWlanIsWPASupported"));

   pWlanCapability = GetCapabilityObject(hAdapter);
   if (NULL == pWlanCapability)
      return hr;

   *pbResult = pWlanCapability->m_bWPASupported;
   hr = NDT_STATUS_SUCCESS;
   return hr;
}

HRESULT NDTWlanIsWPAAdhocSupported(HANDLE hAdapter, PBOOL pbResult)
{
   HRESULT hr = E_FAIL;
   CWlanCapability* pWlanCapability;
   NDTLogVbs(_T("NDTWlanIsWPAAdhocSupported"));

   pWlanCapability = GetCapabilityObject(hAdapter);
   if (NULL == pWlanCapability)
      return hr;

   *pbResult = pWlanCapability->m_bWPAAdhocSupported;
   hr = NDT_STATUS_SUCCESS;
   return hr;
}

HRESULT NDTWlanIsWPA2Supported(HANDLE hAdapter, PBOOL pbResult)
{
   HRESULT hr = E_FAIL;
   CWlanCapability* pWlanCapability;
   pWlanCapability = GetCapabilityObject(hAdapter);
   NDTLogVbs(_T("NDTWlanIsWPA2Supported"));

   if (NULL == pWlanCapability)
      return hr;

   *pbResult = pWlanCapability->m_bWPA2Supported;
   hr = NDT_STATUS_SUCCESS;
   return hr;
}

HRESULT NDTWlanIsMediaStreamSupported(HANDLE hAdapter, PBOOL pbResult)
{
   HRESULT hr = E_FAIL;
   CWlanCapability* pWlanCapability;
   NDTLogVbs(_T("NDTWlanIsMediaStreamSupported"));
      
   pWlanCapability = GetCapabilityObject(hAdapter);
   if (NULL == pWlanCapability)
      return hr;

   *pbResult = pWlanCapability->m_bMediaStreamSupported;
   hr = NDT_STATUS_SUCCESS;
   return hr;
}

HRESULT NDTWlanIsAESSupported(HANDLE hAdapter, PBOOL pbResult)
{
   HRESULT hr = E_FAIL;
   CWlanCapability* pWlanCapability;
   NDTLogVbs(_T("NDTWlanIsAESSupported"));

   pWlanCapability = GetCapabilityObject(hAdapter);
   if (NULL == pWlanCapability)
      return hr;
 
   // When old style capability is used m_bAESSupported is set
   // When OID_802_11_CAPABILITY is supported m_bWPAAESSupported and m_bWPA2AESSupported is set
   // Either one of the three means the card supports AES
   *pbResult = pWlanCapability->m_bAESSupported || pWlanCapability->m_bWPAAESSupported || pWlanCapability->m_bWPA2AESSupported;
   hr = NDT_STATUS_SUCCESS;
   return hr;
}

HRESULT NDTWlanGetNumPKIDs( HANDLE hAdapter, PULONG pulNumPKIDs)
{
   HRESULT hr = E_FAIL;
   CWlanCapability* pWlanCapability;
   NDTLogVbs(_T("NDTWlanIsAESSupported"));

   pWlanCapability = GetCapabilityObject(hAdapter);
   if (NULL == pWlanCapability)
      return hr;

   *pulNumPKIDs= pWlanCapability->m_NumPMKIDsSupported;
   hr = NDT_STATUS_SUCCESS;
   return hr;
}


// Prepares the STA to connect to a WPA AP and calls NDTWlanSetSsid to associate
// It uses its own supplicant to implement authentication using WPA-PSK
// OID_802_11_SSID is mandatory
HRESULT NDTWPAAssociate(HANDLE hAdapter, DWORD dwInfrastructureMode, 
      DWORD dwAuthenticationMode, DWORD dwEncryption, NDIS_802_11_MAC_ADDRESS* pBssid, 
      NDIS_802_11_SSID ssid, BOOL bIsBssid, PULONG pulTimeout)
{
   HRESULT hr = E_FAIL;
   UCHAR pucDestAddr[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};  
   UCHAR *aucDestAddrs[1];
   HANDLE hSend;
   HANDLE hStatus;
   ULONG cbAddr = 6;
   ULONG ulPacketsSent, ulSendTime,ulSendPacketsCompleted,ulSendBytesSent;

    TCHAR szText[80];
   GetAuthModeText(szText,dwAuthenticationMode);   
   NDTLogVbs(_T("NDTWPAAssociate: Authentication mode:%s "),szText);
   GetInfrastructureModeText(szText , dwInfrastructureMode);
   NDTLogVbs(_T("InfrastructureMode:%s "), szText);   
   GetEncryptionText(szText , dwEncryption);
   NDTLogVbs(_T("Encryption:%s "), szText);
   GetSsidText(szText,ssid);
   NDTLogVbs(_T("Ssid:%s"));
  
   hr = NDTWlanDisassociate(hAdapter);

   NDTClearCounters(hAdapter);

   if (!g_pSupplicant)
   {
      NDTLogErr(_T("Supplicant object not created by test"));
      goto cleanUp;
   }

   hr = NDTWlanSetInfrastructureMode(hAdapter, dwInfrastructureMode);
   if (FAILED(hr))
     goto cleanUp;

   hr = NDTWlanSetAuthenticationMode(hAdapter, dwAuthenticationMode);
   if (FAILED(hr))
     goto cleanUp;
   
     
   hr = NDTWlanSetEncryptionStatus(hAdapter, dwEncryption);
   if (FAILED(hr))
      goto cleanUp;

   NDTLogDbg(_T("WPAAuthenticate: Starting Authenticator"));
    if ( FALSE == g_pSupplicant->StartAuthenticator((char *)WLAN_DEFAULT_PMK_PASSWORD, ssid))
    {
      hr = NDT_STATUS_START_AUTHENTICATOR_FAILED;
      goto cleanUp;      
    }

   NDTLogDbg(_T("WPAAuthenticate: Setting ssid"));

   if (NULL != pulTimeout)
   {
      
      hr = NDTStatus(hAdapter, NDIS_FROM_HRESULT(NDT_STATUS_MEDIA_CONNECT), &hStatus);

      if (FAILED(hr)) 
         return hr;        
   }
   
   hr = NDTWlanSetSsid(hAdapter, &ssid);
   if (FAILED(hr))
      goto cleanUp;

   if (NULL != pulTimeout)
   {
      hr = NDTStatusWait(hAdapter, hStatus, (*pulTimeout) * 1000);

      if ( FAILED(hr) || (hr == NDT_STATUS_PENDING) ) 
      {
         return hr;
      }         
   }


   //  If we failed to associate then do not try and authenticate
   if (NDT_STATUS_ASSOCIATION_FAILED == hr)
   { 
      NDTLogDbg(_T("WPAAssociate: waiting for Association failed"));      
      goto cleanUp;
   }
   else
       NDTLogDbg(_T("WPAAssociate: associated with AP... waiting for authentication to complete"));

   if (FALSE == g_pSupplicant->WaitAuthenticationComplete(WLAN_HANDSHAKE_TIMEOUT))
   {
      hr = NDT_STATUS_PSK_AUTHENTICATION_FAILED;
      goto cleanUp;
   }
   else
      NDTLogDbg(_T("WPA Association succeeded"));
   
   aucDestAddrs[0] = pucDestAddr;
   hr = NDTSend(hAdapter, cbAddr, NULL, 1, aucDestAddrs, NDT_RESPONSE_NONE,
         NDT_PACKET_TYPE_FIXED, 250, 10, 0, 0, &hSend);
   
   ulPacketsSent = 0;
   ulSendPacketsCompleted = 0;
   hr = NDTSendWait(hAdapter, hSend,INFINITE, &ulPacketsSent , &ulSendPacketsCompleted,
         NULL, NULL, NULL, &ulSendTime, &ulSendBytesSent, NULL);
   
   //   NDTGetCounter(hAdapter, ULONG nIndex, ULONG * pnValue)   
   //DOc says this resets counters so using ClearCounters instead 
   NDTClearCounters(hAdapter);

   NDTLogDbg(_T("Successfully associated with WPA\\WPA2 AP"));
   hr = NDT_STATUS_SUCCESS;
   
   cleanUp:
    return hr;  
}

// This sends packets with unicast destination address from the hSender adapter 
// to the hReceiver adapter
// OUTPUT parameters: pulPacketsSent, pulPacketsReceived
HRESULT NDTWlanDirectedSend(HANDLE hSender, HANDLE hReceiver, ULONG cbAddr,
   NDIS_MEDIUM nNdisMedium, 
   ULONG ulPacketsToSend, PULONG pulPacketsSent, PULONG pulPacketsReceived)
{
   HRESULT hr = E_FAIL;
   HANDLE hReceive = NULL;
   HANDLE hSend = NULL;   
   ULONG ulSendTime = 0;
   ULONG ulSendBytesSent = 0;
   ULONG  ulRecvTime = 0;
   ULONG  ulRecvBytesRecv = 0;  
   ULONG  ulSendPacketsCompleted = 0;
         
   UCHAR* pucPermanentAddr = NULL;

   NDTLogVbs(_T("NDTWlanDirectedSend"));
   
   hr = NDTGetPermanentAddr(hReceiver, nNdisMedium, &pucPermanentAddr);
   if (FAILED(hr)) 
   {
      NDTLogWrn(_T("WlanDirectedSend: Failed to get Permanent Address for Receiver"));
      if (NULL == pucPermanentAddr)
      {
         return hr;      
      }
     else     
         goto cleanUp;
   }
   else
   {
       NDTLogDbg(_T("WlanDirectedSend: Permanent Address for Receiver is %x:%x:%x:%x:%x:%x"),pucPermanentAddr[0],pucPermanentAddr[1],
         pucPermanentAddr[2],pucPermanentAddr[3],pucPermanentAddr[4],pucPermanentAddr[5]);
   }   

   hr = NDTSetPacketFilter(hReceiver, NDT_FILTER_DIRECTED);   
   if (FAILED(hr))
   {
      NDTLogWrn(_T("WlanDirectedSend: Failed to set Directed packet filter"));
      goto cleanUp;
   }
   else
       NDTLogVbs(_T("WlanDirectedSend: Succesfully set Directed packet filter"));

   hr = NDTReceive(hReceiver, &hReceive);
   if (FAILED(hr)) 
   {
      NDTLogWrn(_T("WlanDirectedSend: NDTReceive returned error %x"), hr);
      goto cleanUp;
   }
   else
      NDTLogVbs(_T("WlanDirectedSend: Successfully called NDTReceive"));

   // Send
   hr = NDTSend(hSender, cbAddr, NULL,  1, &pucPermanentAddr,
      NDT_RESPONSE_NONE, NDT_PACKET_TYPE_FIXED, 250, ulPacketsToSend, 0, 0, &hSend);
   if (FAILED(hr))
   {
      NDTLogWrn(_T("WlanDirectedSend: NDTSend returned error %x"), hr);
      goto cleanUp;
   }
   else
      NDTLogVbs(_T("WlanDirectedSend: Successfully called NDTSend"));
      

    // Wait till sending is done
   hr = NDTSendWait(hSender, hSend,INFINITE, pulPacketsSent , &ulSendPacketsCompleted,
         NULL, NULL, NULL, &ulSendTime, &ulSendBytesSent, NULL);
   if (FAILED(hr))
   {
      NDTLogWrn(_T("WlanDirectedSend: NDTSendWait returned error %x"), hr);
      goto cleanUp;
   }
   else
      NDTLogVbs(_T("WlanDirectedSend: NDTSendWait success: Packets Sent %d"),*pulPacketsSent);
   
   // Stop receiving and check results
   hr = NDTReceiveStop(
      hReceiver, hReceive, pulPacketsReceived, NULL, NULL, 
      &ulRecvTime, NULL, &ulRecvBytesRecv);   
   if (FAILED(hr)) 
   {
      NDTLogWrn(_T("WlanDirectedSend: NDTReceiveStop returned error %x"), hr);
      goto cleanUp;
   }
   else
      NDTLogVbs(_T("WlanDirectedSend: NDTReceiveStop success: Packets received %d"),*pulPacketsReceived);

   cleanUp:
   delete pucPermanentAddr;
   return hr;
}

// This sends packets with broadcast destination address from the hSender adapter 
// to the hReceiver adapter
// OUTPUT parameters: pulPacketsSent, pulPacketsReceived
HRESULT NDTWlanBroadcastSend(HANDLE hSender, HANDLE hReceiver, ULONG cbAddr,
   NDIS_MEDIUM nNdisMedium, 
   ULONG ulPacketsToSend, PULONG pulPacketsSent, PULONG pulPacketsReceived)
{
   HRESULT hr = E_FAIL;
   HANDLE hReceive = NULL;
   HANDLE hSend = NULL;   
   ULONG ulSendTime = 0;
   ULONG ulSendBytesSent = 0;
   ULONG  ulRecvTime = 0;
   ULONG  ulRecvBytesRecv = 0;  
   ULONG  ulSendPacketsCompleted = 0;

   UCHAR* pucBroadcastAddr = NULL;

   NDTLogVbs(_T("NDTWlanBroadcastSend"));
    
   pucBroadcastAddr = NDTGetBroadcastAddr( nNdisMedium);

   if (NULL == pucBroadcastAddr)
   {
       NDTLogWrn(_T("WlanBroadcastSend: Failed to get Broadcast Address"));     
	   return hr;
   }

   hr = NDTSetPacketFilter(hReceiver, NDT_FILTER_BROADCAST);   
   if (FAILED(hr))
   {
      NDTLogWrn(_T("WlanBroadcastSend: Setting packet filter to broadcast returned error 0x%x"),hr);
      goto cleanUp;
   }

   hr = NDTReceive(hReceiver, &hReceive);
   if (FAILED(hr)) 
   {
      NDTLogWrn(_T("WlanBroadcastSend: NDTReceive returned error %x"), hr);
      goto cleanUp;
   }
   else
      NDTLogVbs(_T("WlanBroadcastSend: Successfully called NDTReceive"));

   // Send
   hr = NDTSend(hSender, cbAddr, NULL,  1, &pucBroadcastAddr,
      NDT_RESPONSE_NONE, NDT_PACKET_TYPE_FIXED, 250, ulPacketsToSend, 0, 0, &hSend);
   if (FAILED(hr))
   {
      NDTLogWrn(_T("WlanBroadcastSend: NDTSend returned error %x"), hr);
      goto cleanUp;
   }
   else
      NDTLogVbs(_T("WlanBroadcastSend: Successfully called NDTSend"));
      

    // Wait till sending is done
   hr = NDTSendWait(hSender, hSend,INFINITE, pulPacketsSent , &ulSendPacketsCompleted,
         NULL, NULL, NULL, &ulSendTime, &ulSendBytesSent, NULL);
   if (FAILED(hr))
   {
      NDTLogWrn(_T("WlanBroadcastSend: NDTSendWait returned error %x"), hr);
      goto cleanUp;
   }
   else
      NDTLogVbs(_T("WlanBroadcastSend: NDTSendWait success: Packets Sent %d"),*pulPacketsSent);
    
   // Stop receiving and check results
   hr = NDTReceiveStop(
      hReceiver, hReceive, pulPacketsReceived, NULL, NULL, 
      &ulRecvTime, NULL, &ulRecvBytesRecv);     
   if (FAILED(hr)) 
   {
      NDTLogWrn(_T("WlanBroadcastSend: NDTReceiveStop returned error %x"), hr);
      goto cleanUp;
   }
   else
      NDTLogVbs(_T("WlanBroadcastSend: NDTReceiveStop success: Packets received %d"),*pulPacketsReceived);

   cleanUp:
   if (pucBroadcastAddr)
	   delete pucBroadcastAddr;
   return hr;
}

HRESULT NDTWlanGetCardAddress(HANDLE hAdapter, NDIS_802_11_MAC_ADDRESS* pbMac)
{
   NDIS_OID ndisOid = OID_802_3_CURRENT_ADDRESS;
   UINT cbBuffer = sizeof(NDIS_802_11_MAC_ADDRESS);
   UINT cbUsed = 0;
   UINT cbRequired = 0;
   NDTLogVbs(_T("NDTWlanGetCardAddress"));
      
   return NDTQueryInfo(hAdapter, ndisOid, pbMac, cbBuffer, &cbUsed, &cbRequired);  
}


HRESULT GetEvents(HANDLE hAdapter, PULONG pulResets, PULONG pulDisconnects, PULONG pulConnects)
{
   HRESULT hr = E_FAIL;

   ULONG ulUnexEvents;
   ULONG ulResetStarts;
   ULONG ulResetEnds;
   ULONG ulDisconnects;
   ULONG ulConnects;
   NDTLogVbs(_T("GetEvents"));

   if (!pulResets || !pulDisconnects || !pulConnects)
      return hr;
   
   hr = NDTGetCounter(hAdapter, NDT_COUNTER_UNEXPECTED_EVENTS, &ulUnexEvents);
   if (FAILED(hr)) {
//      NDTLogErr(g_szFailGetCounter, hr);
      goto cleanUp;
   }
   if (ulUnexEvents != 0) {
//      NDTLogErr(_T("Unexpected events occured - see a debug log output"));      
      goto cleanUp;
   }

   hr = NDTGetCounter(hAdapter, NDT_COUNTER_STATUS_MEDIA_CONNECT, &ulConnects);
   if (FAILED(hr)) {
//      NDTLogErr(g_szFailGetCounter, hr);
      goto cleanUp;
   }
   if (!pulConnects && ulConnects)
   {
//      NDTLogErr(_T("FAILURE:  %d unexpected connect events (0 expected)"),ulConnects);
   }
   else
      *pulConnects = ulConnects;

   hr = NDTGetCounter(hAdapter, NDT_COUNTER_STATUS_MEDIA_DISCONNECT, &ulDisconnects);
   if (FAILED(hr)) {
//      NDTLogErr(g_szFailGetCounter, hr);
      goto cleanUp;
   }
   if (!pulDisconnects && ulDisconnects)
   {
//      NDTLogErr(_T("FAILURE:  %d unexpected connect events (0 expected)"),ulDisconnects);
   }
   else
      *pulDisconnects = ulDisconnects;

   hr = NDTGetCounter(hAdapter, NDT_COUNTER_STATUS_RESET_START, &ulResetStarts);
   if (FAILED(hr)) {
//      NDTLogErr(g_szFailGetCounter, hr);
      goto cleanUp;
   }

   hr = NDTGetCounter(hAdapter, NDT_COUNTER_STATUS_RESET_START, &ulResetEnds);
   if (FAILED(hr)) {
//      NDTLogErr(g_szFailGetCounter, hr);
      goto cleanUp;
   }   
   
   if (ulResetStarts != ulResetEnds)
   {
  //    NDTLogErr(_T("FAILURE:  unequal reset start %dand reset ends %d"),ulResetStarts, ulResetEnds);
      if (ulResetStarts > ulResetEnds)
         ulResetStarts = ulResetEnds;
   }

   if (!pulResets&& ulResetStarts)
   {
//      NDTLogErr(_T("FAILURE:  %d unexpected connect events (0 expected)"),ulDisconnects);
   }
   else
      *pulResets= ulResetStarts;

   cleanUp:
   return hr;
}

// This instucts the card to create an IBSS
HRESULT NDTWlanCreateIBSS(HANDLE hAdapter, DWORD dwAuthenticationMode, 
      DWORD dwEncryption, ULONG ulKeyLength, ULONG ulKeyIndex, PBYTE pbKeyMaterial,
      NDIS_802_11_MAC_ADDRESS* pBssid,   NDIS_802_11_SSID ssid)
{   
   HRESULT hr=E_FAIL;
   TCHAR szText[80];
   GetAuthModeText(szText,dwAuthenticationMode);   
   NDTLogVbs(_T("NDTWlanCreateIBSS: KeyLength:%d KeyIndex:0x%x Authentication mode:%s "), 
         ulKeyLength, ulKeyIndex,szText);
   GetEncryptionText(szText , dwEncryption);
   NDTLogVbs(_T("Encryption:%s "), szText);
   GetSsidText(szText,ssid);
   NDTLogVbs(_T("Ssid:%s"));

   do
   {   
      hr = NDTClearCounters(hAdapter);

      hr = NDTWlanSetInfrastructureMode(hAdapter, Ndis802_11IBSS);
      if (FAILED(hr))
      {
         NDTLogWrn(_T("Failed to set infrastructure mode to Ndis802_11IBSS"));
         break;
      }

      hr = NDTWlanSetAuthenticationMode(hAdapter, dwAuthenticationMode);
      if (FAILED(hr))
      {         
         NDTLogWrn(_T("Failed to set Authentication mode"));
         break;
      }

      hr = NDTWlanSetEncryptionStatus(hAdapter, dwEncryption);
      if (FAILED(hr))
      {
         NDTLogWrn(_T("Failed to set Encryption mode"));        
         break;
      }

      if (NULL != pBssid) 
      {
         DWORD dwSize = FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial[ulKeyLength]);
         PNDIS_802_11_KEY pnKey = (PNDIS_802_11_KEY)malloc(dwSize);

         pnKey->Length =dwSize;
         pnKey->KeyIndex = ulKeyIndex;
         pnKey->KeyLength = ulKeyLength;
         memcpy(pnKey->BSSID , pBssid, sizeof(pnKey->BSSID));
         memcpy(pnKey->KeyMaterial, pbKeyMaterial, ulKeyLength);

         hr = NDTWlanSetAddKey(hAdapter, pnKey, dwSize);
         free(pnKey);
         if (FAILED(hr)) 
            break;
      }
      //otherwise use the AddWep() function
      else
      {
         DWORD dwSize = FIELD_OFFSET(NDIS_802_11_WEP,KeyMaterial[ulKeyLength]);
         PNDIS_802_11_WEP pnWEPKey = (PNDIS_802_11_WEP) malloc(dwSize);

         pnWEPKey->Length = dwSize;
         pnWEPKey->KeyIndex = ulKeyIndex;
         pnWEPKey->KeyLength = ulKeyLength;
         memcpy(pnWEPKey->KeyMaterial , pbKeyMaterial,ulKeyLength);
         
         hr = NDTWlanSetAddWep(hAdapter, pnWEPKey, dwSize);
         free(pnWEPKey);
         
         if (FAILED(hr)) 
            break;
      }      

      hr = NDTWlanSetSsid(hAdapter, &ssid);
      if (FAILED(hr)) 
      {
         NDTLogWrn(_T("Failed to set SSID"));
         break;
      }

   }
   while(0);

   if (!FAILED(hr))
   {
      ULONG ulResets =0;
      ULONG ulDisconnects =0;
      ULONG ulConnects =0;
      
      GetEvents(hAdapter, &ulResets,&ulDisconnects,&ulConnects);
   }
   
   return hr;
}

// Instructs the adapter to join an IBSS
HRESULT NDTWlanJoinIBSS(HANDLE hAdapter, DWORD dwAuthenticationMode, 
      DWORD dwEncryption, ULONG ulKeyLength, ULONG ulKeyIndex, PBYTE pbKeyMaterial,
      NDIS_802_11_MAC_ADDRESS* pBssid,   NDIS_802_11_SSID ssid, PULONG pulTimeout)
{
    HRESULT hr=E_FAIL;
   TCHAR szText[80];
   GetAuthModeText(szText,dwAuthenticationMode);   
   NDTLogVbs(_T("NDTWlanJoinIBSS: KeyLength:%d KeyIndex:0x%x Authentication mode:%s "), 
         ulKeyLength, ulKeyIndex,szText);
   GetEncryptionText(szText , dwEncryption);
   NDTLogVbs(_T("Encryption:%s "), szText);
   GetSsidText(szText,ssid);
   NDTLogVbs(_T("Ssid:%s"));
   
   do
   {
       hr = NDTClearCounters(hAdapter);

       hr = NDTWlanSetInfrastructureMode(hAdapter, Ndis802_11IBSS);
       if (FAILED(hr))
            return hr;

      hr = NDTWlanSetAuthenticationMode(hAdapter, dwAuthenticationMode);
         if (FAILED(hr))
            break;
       
   
      if (NULL != pBssid) 
      {
         DWORD dwSize = FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial[ulKeyLength]);
         PNDIS_802_11_KEY pnKey = (PNDIS_802_11_KEY)malloc(dwSize);

         pnKey->Length =dwSize;
         pnKey->KeyIndex = ulKeyIndex;
         pnKey->KeyLength = ulKeyLength;
         memcpy(pnKey->BSSID , pBssid, sizeof(pnKey->BSSID));
         memcpy(pnKey->KeyMaterial, pbKeyMaterial, ulKeyLength);

         hr = NDTWlanSetAddKey(hAdapter, pnKey, dwSize);
         free(pnKey);
         if (FAILED(hr)) 
            break;
      }
      //otherwise use the AddWep() function
      else
      {
         DWORD dwSize = FIELD_OFFSET(NDIS_802_11_WEP,KeyMaterial[ulKeyLength]);
         PNDIS_802_11_WEP pnWEPKey = (PNDIS_802_11_WEP) malloc(dwSize);

         pnWEPKey->Length = dwSize;
         pnWEPKey->KeyIndex = ulKeyIndex;
         pnWEPKey->KeyLength = ulKeyLength;
         memcpy(pnWEPKey->KeyMaterial , pbKeyMaterial,ulKeyLength);
         
         hr = NDTWlanSetAddWep(hAdapter, pnWEPKey, dwSize);
         free(pnWEPKey);
         
         if (FAILED(hr)) 
            break;
      }      

      hr = NDTWlanSetSsid(hAdapter, &ssid);
      if (FAILED(hr)) 
      {
         NDTLogWrn(_T("Failed to set ssid"));
         break;
      }

      //If timeout is Null then don't wait for a connect event
      if (NULL != pulTimeout)
      {
         ULONG    ulConnectStatus = 0;
         ULONG ulTimer = 0;
         while(ulTimer < *pulTimeout)
         {
            Sleep(1000);
            UINT cbUsed = 0;
            UINT cbRequired = 0;
            
            hr = NDTQueryInfo(hAdapter, OID_GEN_MEDIA_CONNECT_STATUS, &ulConnectStatus,
                     sizeof(ulConnectStatus),&cbUsed,&cbRequired);
            if (NOT_SUCCEEDED(hr)) 
               break;
         
            if (ulConnectStatus == NdisMediaStateConnected)
               break;      

            ulTimer++;
         } 

         if (ulTimer > *pulTimeout)
            break;
      }
   }
   while(0);

   if (!FAILED(hr))
   {
      ULONG ulResets =0;
      ULONG ulDisconnects =0;
      ULONG ulConnects =0;
      
      GetEvents(hAdapter, &ulResets,&ulDisconnects,&ulConnects);
   }   

   return hr;
}

// This is used to turn on the radio after a call to disassociate, which may turn it off
HRESULT NDTWlanSetDummySsid(HANDLE hAdapter)
{  
   HRESULT hr = E_FAIL;
   NDIS_802_11_SSID dummySsid;
   NDTLogVbs(_T("NDTWlanSetDummySsid"));
   
   dummySsid.SsidLength = strlen((char *)WLAN_RADIO_ON_SSID);
   memcpy((dummySsid.Ssid), WLAN_RADIO_ON_SSID, dummySsid.SsidLength);
   hr = NDTWlanSetSsid( hAdapter, &dummySsid);

   return hr;
}

// This is used to display a list of bssids seen by the test adapter before the start of the tests
HRESULT NDTWlanPrintBssidList(HANDLE hAdapter)
{
   HRESULT hr=E_FAIL;
 
   do
   {
         NDTLogMsg(_T("##########Printing BSSID list #################"));
         hr = NDTWlanSetBssidListScan(hAdapter);
         if (NOT_SUCCEEDED(hr))
         {
            NDTLogErr(_T("Failed to set OID_802_11_BSSID_LIST_SCAN\n"));
            break;
         }

         Sleep(WLAN_BSSID_LIST_SCAN_TIMEOUT);

         DWORD dwSize = 2048;            
         PNDIS_802_11_BSSID_LIST_EX pnBSSIDListEx = (PNDIS_802_11_BSSID_LIST_EX) malloc(dwSize);
         UINT retries = 20;
         do 
         {
            hr = NDTWlanGetBssidList(hAdapter, pnBSSIDListEx, &dwSize);
            
            if (NOT_SUCCEEDED(hr) && (hr == NDT_STATUS_BUFFER_TOO_SHORT))             
            {
               if (dwSize < 1024)
               {
                  NDTLogErr(_T("Invalid number of bytes required returned %d expected > 1024"), dwSize);
                  break;
               }                    
               pnBSSIDListEx = (PNDIS_802_11_BSSID_LIST_EX) realloc(pnBSSIDListEx,dwSize);                  
            }
            else if (NOT_SUCCEEDED(hr))
            {
                  NDTLogErr(_T("Failed to get OID_802_11_BSSID_LIST_SCAN Error =0x%x\n"), hr);
                  break;
             }
            else if  (!NOT_SUCCEEDED(hr))
            {
               NDTLogVbs(_T("Sucessfully got BSSID list \n"));
               break;
            }                  
         }            
         while(--retries >0);
         if (retries <= 0)
         {
            NDTLogErr(_T("Failed to get OID_802_11_BSSID_LIST_SCAN after multiple attempts Error =0x%x\n"), hr);        
            break;
         }

         // Loop through the list of bssid and search for each known ssid
         PNDIS_WLAN_BSSID_EX pnWLanBssidEx= &(pnBSSIDListEx->Bssid[0]);
         for (UINT iz=0; iz<(pnBSSIDListEx->NumberOfItems); iz++)
         {                  
            TCHAR szBssidExText[80];                                       
            memset(szBssidExText,_T('\0'),sizeof(szBssidExText));
            GetBssidExText(szBssidExText, *pnWLanBssidEx);
            NDTLogMsg(_T("%s\n"),szBssidExText);               
             pnWLanBssidEx = (PNDIS_WLAN_BSSID_EX)((BYTE*)pnWLanBssidEx + (pnWLanBssidEx->Length));
         }
         
         if (pnBSSIDListEx)
            free(pnBSSIDListEx);

         NDTLogMsg(_T("###############################################"));
   }
   while(0);

   return hr;
}
