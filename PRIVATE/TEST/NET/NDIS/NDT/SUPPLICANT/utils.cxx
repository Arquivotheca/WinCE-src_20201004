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
#include "stdafx.h"
#include <Wincrypt.h>
#include "supplib.h"

#define MD5DIGESTLEN    16

// Change made to cache the Crypto context
static HCRYPTPROV  s_hProv                       = {0};

//-----------------------------------------------------------------------------------------------//
/*
 * This function will search for the WPA information element and return a pointer to it
 * if found
 */
BYTE* 
FindWPAIE(
   BYTE* pRequestIE, 
   DWORD nRequestIELen
)
{
   NDTLogVbs( _T(": Enter (pRequestIE = %p, nRequestIELen = %d)\n"), pRequestIE, nRequestIELen);

   BYTE*    pIE         = pRequestIE;
   DWORD    nLength     = 0;
   DWORD    nTotalLen   = 0;
   BYTE*    pWPAIE      = NULL;

   if(pRequestIE == NULL)
   {
      NDTLogErr(_T("pRequestIE is null.\n"));
      return NULL;
   }

   while(nTotalLen <= nRequestIELen)
   {
      if(pIE[0] == 0xDD)
      {
         if(pIE[2] == 0x00 && pIE[3] == 0x50 && pIE[4] == 0xF2 && pIE[5] == 0x01)
         {
            pWPAIE = pIE;
            break;
         }
      }
      
      nLength = pIE[1] + (sizeof(UCHAR) * 2);
      pIE += nLength;

      nTotalLen += nLength;
   }
   
   NDTLogVbs( _T(": Exit (WPA IE = %p)\n"), pWPAIE);
   return pWPAIE;
}

//-----------------------------------------------------------------------------------------------//
/*
 * This function will search for the RSN information element and return a pointer to it
 * if found
 */
BYTE* 
FindRSNIE(
   BYTE* pRequestIE, 
   DWORD nRequestIELen
)
{
   NDTLogVbs( _T(": Enter (pRequestIE = %p, nRequestIELen = %d)\n"), pRequestIE, nRequestIELen);

   BYTE*    pIE         = pRequestIE;
   DWORD    nLength     = 0;
   DWORD    nTotalLen   = 0;
   BYTE*    pRSNIE      = NULL;

   if(pRequestIE == NULL)
   {
      NDTLogErr(_T("pRequestIE is null.\n"));
      return NULL;
   }

   while(nTotalLen <= nRequestIELen)
   {
      if(pIE[0] == 0x30)
      {
         //if(pIE[2] == 0x00 && pIE[3] == 0x0F && pIE[4] == 0xAC && pIE[5] == 0x02)
         //{
            pRSNIE = pIE;
            break;
         //}
      }
      
      nLength = pIE[1] + (sizeof(UCHAR) * 2);
      pIE += nLength;

      nTotalLen += nLength;
   }
   
   NDTLogVbs( _T(": Exit (RSN IE = %p)\n"), pRSNIE);
   return pRSNIE;
}

//-----------------------------------------------------------------------------------------------//
/*
 * This function will query the association information OID and then parse the WPA IE from the 
 * association information and return it to the caller. The caller is responsible for freeing
 * the returned pointer.
 */
INFO_ELEMENT* 
GetWPAIE(
   HANDLE ndisUio
)
{
   NDTLogVbs( _T(": Enter GetWPAIE\n"));

   NDIS_802_11_ASSOCIATION_INFORMATION *pAssocInfo    = NULL;
   DWORD                               assocInfoLen   = 2000;
   DWORD                               retval         = ERROR_SUCCESS;
   BYTE*                               pWPAIE         = NULL;
   BYTE*                               pIE            = NULL;
      
Retry:
   
   free(pAssocInfo);

   do
   {
      // Allocate memory for association information
      pAssocInfo = (NDIS_802_11_ASSOCIATION_INFORMATION*)malloc(assocInfoLen);
      if(pAssocInfo == NULL)
      {
          NDTLogErr(_T("Failed to allocate memory for association information (Length = %d)\n"), assocInfoLen);
          break;
      }
    
      // Query association information
      retval = NdisUioQueryOIDValue(ndisUio, OID_802_11_ASSOCIATION_INFORMATION, (BYTE*)pAssocInfo, &assocInfoLen);
      if(retval == ERROR_INSUFFICIENT_BUFFER)
      {
          assocInfoLen += 2000;
          NDTLogDbg(_T("Query OID_802_11_ASSOCIATION_INFORMATION failed with buffer to short, retrying with %d\n"), assocInfoLen);
          goto Retry;
      }
      else if(retval != ERROR_SUCCESS)
      {    
          NDTLogErr(_T("Query OID_802_11_ASSOCIATION_INFORMATION failed (Error = %d)\n"), retval);
          break;
      }

      //
      // Get the WPA IE from the association information
      //
      pIE = FindWPAIE((((BYTE*)pAssocInfo) + pAssocInfo->OffsetRequestIEs), pAssocInfo->RequestIELength);
      if(pIE == NULL)
      {
         NDTLogErr(_T("Association Information did not contain a WPA IE\n"));
         break;
      }
      
      int length = (pIE[1] + 2) * sizeof(BYTE);
      
      // Allocate memory for the WPA IE, index one contains the length of the data feild
      pWPAIE = (BYTE*)malloc(length);
      if(pWPAIE == NULL)
      {
         NDTLogErr(_T("Failed to allocate memory for WPA information element\n"));
         break;
      }

      memcpy(pWPAIE, pIE, length);
     
   }while(false);
 
   free(pAssocInfo);
   
   NDTLogVbs( _T(": Exit\n"));
   return (INFO_ELEMENT*)pWPAIE;
}

//-----------------------------------------------------------------------------------------------//
/*
 * This function will query the association information OID and then parse the RSN IE from the 
 * association information and return it to the caller. The caller is responsible for freeing
 * the returned pointer.
 */
INFO_ELEMENT* 
GetRSNIE(
   HANDLE ndisUio
)
{
   NDTLogVbs( _T(": Enter GetRSNIE\n"));

   NDIS_802_11_ASSOCIATION_INFORMATION *pAssocInfo    = NULL;
   DWORD                               assocInfoLen   = 2000;
   DWORD                               retval         = ERROR_SUCCESS;
   BYTE*                               pRSNIE         = NULL;
   BYTE*                               pIE            = NULL;
      
Retry:
   
   free(pAssocInfo);

   do
   {
      //
      // Allocate memory for association information
      //
      pAssocInfo = (NDIS_802_11_ASSOCIATION_INFORMATION*)malloc(assocInfoLen);
      if(pAssocInfo == NULL)
      {
          NDTLogErr(_T("Failed to allocate memory for association information (Length = %d)\n"), assocInfoLen);
          break;
      }
    
      //
      // Query association information
      //
      retval = NdisUioQueryOIDValue(ndisUio, OID_802_11_ASSOCIATION_INFORMATION, (BYTE*)pAssocInfo, &assocInfoLen);
      if(retval == ERROR_INSUFFICIENT_BUFFER)
      {
          assocInfoLen += 2000;
          NDTLogDbg(_T("Query OID_802_11_ASSOCIATION_INFORMATION failed with buffer to short, retrying with %d\n"), assocInfoLen);
          goto Retry;
      }
      else if(retval != ERROR_SUCCESS)
      {    
          NDTLogErr(_T("Query OID_802_11_ASSOCIATION_INFORMATION failed (Error = %d)\n"), retval);
          break;
      }

      //
      // Get the RSN IE from the association information
      //
      pIE = FindRSNIE((((BYTE*)pAssocInfo) + pAssocInfo->OffsetRequestIEs), pAssocInfo->RequestIELength);
      if(pIE == NULL)
      {
         NDTLogErr(_T("FindRSNIE did not return an RSN Information Element\n"));
         break;
      }
      
      int length = (pIE[1] + 2) * sizeof(BYTE);
      
      //
      // Allocate memory for the WPA IE, index one contains the length of the data feild
      //
      pRSNIE = (BYTE*)malloc(length);
      if(pRSNIE == NULL)
      {
         NDTLogErr(_T("Failed to allocate memory for WPA information element\n"));
         break;
      }

      memcpy(pRSNIE, pIE, length);
     
   }while(false);
 
   free(pAssocInfo);

   NDTLogVbs( _T(": Exit\n"));
   return (INFO_ELEMENT*)pRSNIE;
}

//-----------------------------------------------------------------------------------------------//
VOID  
NextNonce(
   BYTE* pNonce
)
{
   NDTLogVbs( _T(": Enter NextNonce\n"));
   
   BOOL bCryptGenRandomSucceeded    = FALSE;

   if(AquireCryptContext())
   {
     if(CryptGenRandom(s_hProv, SIZE_NONCE, pNonce))
     {
         bCryptGenRandomSucceeded = TRUE;
     }
   }

   if(bCryptGenRandomSucceeded == FALSE)
     *pNonce = 128;

   
   NDTLogVbs( _T(": Exit\n"));
}

//-----------------------------------------------------------------------------------------------//
/**
 * This function compares to value and returns the min and max of the two values compared
 */
void CompareData(
   IN  BYTE  *pbOData1, 
   IN  BYTE  *pbOData2, 
   IN  DWORD cbData, 
   OUT BYTE  **ppbLowerValue, 
   OUT BYTE  **ppbHigherValue
)
{
   NDTLogVbs( _T(": Enter CompareData\n"));

   DWORD   i;
   BYTE*   pbData1 = pbOData1;
   BYTE*   pbData2 = pbOData2;

   for (i = 0; i < cbData; i++)
   {
     if (*pbData1 > *pbData2)
     {
         *ppbHigherValue = pbOData1;
         *ppbLowerValue = pbOData2;
         return;
     }
     else if (*pbData1 < *pbData2)
     {
         *ppbLowerValue = pbOData1;
         *ppbHigherValue = pbOData2;
         return;
     }

     pbData1++;
     pbData2++;
   }

   //
   // If we have reached here, then both Data streams are equivalent,
   // so return any order
   //
   *ppbHigherValue = pbOData1;
   *ppbLowerValue = pbOData2;

   NDTLogVbs( _T(": Exit\n"));
}


//-----------------------------------------------------------------------------------------------//
/* 
 * This function will generate and return the PTK which contains the MIC, and encryption key to 
 * be used 
 */
BOOL GeneratePTK(
   PRESHARED_KEY*           preKey, 
   BYTE*                    pAuthAddress, 
   BYTE*                    pAuthNonce, 
   BYTE*                    pSuppAddress, 
   BYTE*                    pSuppNonce, 
   PAIRWISE_TEMPEROL_KEY*   pPTK
)
{
   NDTLogVbs( _T(": Enter GeneratePTK\n"));

   DWORD    nOffset        = 0;
   BYTE*    pMinAddr       = NULL;
   BYTE*    pMaxAddr       = NULL;
   BYTE*    pMinNonce      = NULL;
   BYTE*    pMaxNonce      = NULL;
   DWORD    length         = MAC_ADDR_LEN * 2 + SIZE_NONCE * 2;
   BYTE*    pPRFInput      = NULL;
   BOOL     status         = TRUE;

   // Verify the keylength has been supplied or the PRF routine will fail
   if(pPTK->KeyLength <= 0)
   {
      NDTLogErr(_T("KeyLength feild <= 0\n"));
      return FALSE;
   }
   
   pPRFInput = (BYTE*)malloc(length);
   if(pPRFInput == NULL)
   {
      NDTLogErr(_T("Failed to allocate memory for PRF Input\n"));
      return FALSE;
   }

   // Compare the authenticator and supplicant MAC addresss and return 
   // the min and max of the two
   CompareData(pAuthAddress, pSuppAddress, MAC_ADDR_LEN, &pMinAddr, &pMaxAddr);

   // Compare the authenticator and supplicant nonce and return 
   // the min and max of the two
   CompareData(pAuthNonce, pSuppNonce, SIZE_NONCE, &pMinNonce, &pMaxNonce);

   memcpy(pPRFInput, pMinAddr, MAC_ADDR_LEN);
   nOffset = MAC_ADDR_LEN;

   memcpy(&pPRFInput[nOffset], pMaxAddr, MAC_ADDR_LEN);
   nOffset += MAC_ADDR_LEN;

   memcpy(&pPRFInput[nOffset], pMinNonce, SIZE_NONCE);
   nOffset += SIZE_NONCE;

   memcpy(&pPRFInput[nOffset], pMaxNonce, SIZE_NONCE);
   
   // Generate the PTK
   status = PRF(preKey->KeyMaterial, 
                preKey->KeyLength, 
                "Pairwise key expansion", 
                (DWORD)strlen("Pairwise key expansion"), 
                pPRFInput, 
                length, 
                pPTK->MICKey,
                (KCK_LENGTH + KEK_LENGTH + pPTK->KeyLength));
  
   free(pPRFInput);
   
   NDTLogVbs( _T(": Exit\n"));
   return TRUE;
}
/*
if(SendPacket(ndisUio, 
                       EAPOL_KEY_DESCRIPTOR_TYPE_WPA2,
                       AuthContext.SupplicantAddress, 
                       AuthContext.AuthenticatorAddress, 
                       AuthContext.PTK.MICKey, 
                       pKeyDescriptor->ReplayCounter,
                       (SSN_KEY_DESC_VERSION_AES | SSN_KEY_TYPE_PAIRWISE | SSN_KEY_DESC_MIC_BIT | SSN_KEY_DESC_SECURE_BIT),
                       NULL, 
                       NULL,
                       0) == false)
         {
         */
//-----------------------------------------------------------------------------------------------//
BOOL 
SendPacket(
   HANDLE   hNdisuio,
   BYTE     descriptorType, 
   BYTE*    pSource, 
   BYTE*    pDestination, 
   BYTE*    pMICKey, 
   BYTE*    pReplay, 
   USHORT   keyInformation, 
   BYTE*    pNonce, 
   BYTE*    pKeyData,
   USHORT   keyDataLength
)
{
   NDTLogVbs( _T(": Enter SendPacket\n"));

   BYTE*                pPacket        = NULL;
   ETHERNET_HEADER*     pEthernet      = NULL;
   EAPOL_PACKET*        pEapolPkt      = NULL;
   WPA_KEY_DESCRIPTOR*  pKeyDescriptor = NULL;
   DWORD                totalLength    = sizeof(ETHERNET_HEADER) + FIELD_OFFSET(EAPOL_PACKET, PacketBody) + FIELD_OFFSET(WPA_KEY_DESCRIPTOR, KeyData) + keyDataLength;
   DWORD                length         = FIELD_OFFSET(EAPOL_PACKET, PacketBody) + FIELD_OFFSET(WPA_KEY_DESCRIPTOR, KeyData) + keyDataLength;
   DWORD                md5Length      = 0;
   DWORD                bytesWritten   = 0;
   BYTE                 md5Buffer[MD5DIGESTLEN];
   OVERLAPPED           overlapped;
   
   ZeroMemory(&overlapped, sizeof(OVERLAPPED));

   //
   // Allocate the send packet
   //
   pPacket = (BYTE*)malloc(totalLength);
   if(pPacket == NULL)
   {
      NDTLogErr(_T("Failed to allocate memory for pSendPacket (Length = %d)\n"), totalLength);
      return FALSE;
   }
   
   ZeroMemory(pPacket, totalLength);

   //
   // Build Ethernet packet
   //
   pEthernet = (ETHERNET_HEADER*)pPacket;
   memcpy(pEthernet->DestinationAddr,  pDestination,  MAC_ADDR_LEN);
   memcpy(pEthernet->SourceAddr,       pSource,     MAC_ADDR_LEN);
   
   
   //
   // Build EAPOL packet
   //
   pEapolPkt = (EAPOL_PACKET*)(pPacket + sizeof(ETHERNET_HEADER));  
   memcpy((BYTE*)pEapolPkt->EtherType, EtherType, SIZE_ETHERNET_TYPE);
   pEapolPkt->ProtocolVersion = ProtocolVersion;
   pEapolPkt->PacketType      = EAPOL_Key;
   HostToWire16((WORD)(length - FIELD_OFFSET(EAPOL_PACKET,PacketBody)), (BYTE*)pEapolPkt->PacketBodyLength);  
   
   //
   // Build Key Descriptor
   //
   pKeyDescriptor = (WPA_KEY_DESCRIPTOR*)pEapolPkt->PacketBody;
   pKeyDescriptor->DescriptorType = descriptorType; //EAPOL_KEY_DESC_TYPE_WPA;
   HostToWire16((WORD)keyInformation, (BYTE*)pKeyDescriptor->KeyInformation);
   HostToWire16((WORD)0, (BYTE*)pKeyDescriptor->KeyLength);
   HostToWire16((WORD)keyDataLength, (BYTE*)pKeyDescriptor->KeyDataLength);
   
   memcpy(pKeyDescriptor->ReplayCounter, pReplay, 8);
   
   if(pNonce != NULL)
   {
      memcpy(pKeyDescriptor->KeyNonce, pNonce, SIZE_NONCE);
   }

   if((pKeyData != NULL) && (keyDataLength > 0))
   {
      memcpy(pKeyDescriptor->KeyData, pKeyData, keyDataLength);
   }   

   ZeroMemory(pKeyDescriptor->KeyMIC,  MD5DIGESTLEN);
   ZeroMemory(md5Buffer,               MD5DIGESTLEN);

   md5Length = FIELD_OFFSET(EAPOL_PACKET,PacketBody) - FIELD_OFFSET(EAPOL_PACKET,ProtocolVersion) + FIELD_OFFSET(WPA_KEY_DESCRIPTOR,KeyData) + keyDataLength;
   GetHMACMD5Digest(&pEapolPkt->ProtocolVersion, md5Length, pMICKey, 16, md5Buffer);

   memcpy(pKeyDescriptor->KeyMIC, md5Buffer, MD5DIGESTLEN);

   BOOL bResult = WriteFile(hNdisuio, pPacket, totalLength, &bytesWritten, NULL);
   if (!bResult)
   {
      NDTLogErr(_T("WriteFile() failed (Error = %d)\n"), GetLastError());
      return FALSE;
   }
   else
      NDTLogDbg(_T("Sent packet - %d bytes written"),bytesWritten);
   
   NDTLogVbs( _T(": Exit\n"));
   return TRUE;
}

//-----------------------------------------------------------------------------------------------//
/*
    A key material which is not a multiple of 8, it may be padded with DD 00 00 00 

    This routine goes over the Key Material and adjusts the length to ignore the padding.
    
*/
BOOL
AdjustKeyMaterialLength(
    BYTE*   pKeyData,
    DWORD*  pKeyDataLength
    )
{
   NDTLogVbs( _T(": Enter AdjustKeyMaterialLength\n"));

   DWORD keyDataLen        = 0;
   DWORD tag               = 0;
   DWORD offset            = 0;
   DWORD length            = 0;
   DWORD adjustedDataLen   = 0;
   BOOL  retval            = TRUE;

   keyDataLen        = *pKeyDataLength;
   adjustedDataLen   = keyDataLen;

   while(offset < keyDataLen)
   {
      
      //
      // Check if we have reached the delimitter. 
      //
      if(pKeyData[offset] == 0xDD)
      {
         if(((keyDataLen - offset) == 1) || ((keyDataLen - offset > 1) && (pKeyData[offset + 1] == 0)))
         {
            adjustedDataLen = offset;
            retval = TRUE;
            break;
         }
      }

      //
      // Should be able to read atleast 2 bytes. 
      //
      if((offset + 2) > keyDataLen)
      {
         NDTLogErr(_T("Bad key data length (1st Check)\n"));
         retval = FALSE;
         break;
      }

      tag = pKeyData[offset];
      length = pKeyData[offset + 1];

      if((offset + 2 + length) > keyDataLen) 
      {
         NDTLogErr(_T("Bad key data length (2nd Check)\n"));
         retval = FALSE;
         break;
      }

      // Sweep over the data. 
      offset += 2 + length;
   }
   
   NDTLogVbs( _T(": Exit (Return = %d\n"), retval);
   return retval;
}

//-----------------------------------------------------------------------------------------------//
/*
 * This function is called to install a pairwise or group key to the driver
 */
BOOL
InstallKey(
   HANDLE     handle,
   DWORD      length,
   BYTE*      pBssid,
   DWORD      keyIndex,
   DWORD      keyLength,
   ULONGLONG  keyRSC,
   BYTE*      pKey
   )
{
   NDTLogVbs( _T(": Enter InstallKey\n"));

   NDIS_802_11_KEY*  pKeyMaterial   = NULL;
   DWORD             status         = 0;
   
   pKeyMaterial = (NDIS_802_11_KEY*)malloc(length);
   if(pKeyMaterial == NULL)
   {
      NDTLogErr(_T("Failed to allocate memory for key\n"));
      return FALSE;
   }

   ZeroMemory(pKeyMaterial, length);
   
   /*
   DebugPrint(DBG_LOUDER, _T("Length     = %d\n"),      length);
   DebugPrint(DBG_LOUDER, _T("KeyIndex   = 0x%08x\n"),  keyIndex);
   DebugPrint(DBG_LOUDER, _T("KeyRSC     = %d\n"),      keyRSC);
   DebugPrint(DBG_LOUDER, _T("KeyLength  = %d\n"),      keyLength);
   DebugPrint(DBG_LOUDER, _T("BSSID      = %02x-%02x-%02x-%02x-%02x-%02x\n"), pBssid[0],
                                                                              pBssid[1],
                                                                              pBssid[2],
                                                                              pBssid[3],
                                                    s                          pBssid[4],
                                                                              pBssid[5]);
                                                                              */

   pKeyMaterial->Length        = length;
   pKeyMaterial->KeyIndex      = keyIndex;
   pKeyMaterial->KeyLength     = keyLength;
   pKeyMaterial->KeyRSC        = keyRSC;

   memcpy(pKeyMaterial->BSSID, pBssid, MAC_ADDR_LEN);              
   memcpy(pKeyMaterial->KeyMaterial,  pKey,  keyLength);

   //
   // Set OID to install the key
   //
   status = NdisuioSetOIDValue(handle, OID_802_11_ADD_KEY, (BYTE*)pKeyMaterial, pKeyMaterial->Length);
   if(status != ERROR_SUCCESS)
   {
      free(pKeyMaterial);

      NDTLogErr(_T("NdisuioSetOIDValue() failed\n"));
      return FALSE;
   }
   
   free(pKeyMaterial);

   NDTLogVbs( _T(": Exit\n"));
   return TRUE;
}

BOOL AquireCryptContext(void)
{
    if(s_hProv== 0)
        return CryptAcquireContext(&s_hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);

    return TRUE;
}

void ReleaseCryptContext(void)
{
    if(s_hProv)
        CryptReleaseContext(s_hProv, 0);
    s_hProv = 0;
}




