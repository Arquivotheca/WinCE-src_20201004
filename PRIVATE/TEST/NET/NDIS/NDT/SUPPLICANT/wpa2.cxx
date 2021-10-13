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
#include "stdafx.h"
#include "wpa2.h"
#include "supplib.h"

extern BYTE EtherType[SIZE_ETHERNET_TYPE];
extern UINT LastMessage;

//-----------------------------------------------------------------------------------------------//
DWORD 
ProcessRSNPacket(
   HANDLE         ndisUio, 
   EAPOL_PACKET*  pEapolPacket
)
{
   NDTLogVbs( _T(": Enter ProcessRSNPacket\n"));
   
   WPA_KEY_DESCRIPTOR*  pKeyDescriptor    = (WPA_KEY_DESCRIPTOR*)((PBYTE)pEapolPacket + FIELD_OFFSET(EAPOL_PACKET, PacketBody));
   USHORT               keyInformation    = WireToHost16(pKeyDescriptor->KeyInformation);
   DWORD                status            = SUPPLICANT_FAILURE;

   //
   // Verify correct version
   //
   //if((keyInformation & SSN_KEY_DESC_VERSION_AES) != SSN_KEY_DESC_VERSION_AES)
   //{
  //    NDTLogErr(_T("Incorrect key description version\n"));
  //    return status;
  // }

   //
   // Message 1
   //
   if((keyInformation & SSN_KEY_DESC_KEY_TYPE_BIT) == SSN_KEY_TYPE_PAIRWISE && (keyInformation & SSN_KEY_DESC_MIC_BIT) == 0)
   {  
      //
      // Zero the authentication context info
      //
      ZeroMemory(&AuthContext.PTK,         sizeof(PAIRWISE_TEMPEROL_KEY));
      

      //
      // Generate a new Nonce
      //
      NextNonce(AuthContext.SNonce);

      LastMessage = 1;
      status = ProcessRSNMessage1(ndisUio, pKeyDescriptor);
   }
   
   //
   // Message 3
   //
   if((keyInformation & SSN_KEY_DESC_KEY_TYPE_BIT) == SSN_KEY_TYPE_PAIRWISE && 
      (keyInformation & SSN_KEY_DESC_MIC_BIT) == SSN_KEY_DESC_MIC_BIT)
   {   
      LastMessage = 3;
      status = ProcessRSNMessage3(ndisUio, pKeyDescriptor);
   }
   
   NDTLogVbs(  _T(": Exit (Status = 0x%08x)\n"), status);
   return status;
}

//-----------------------------------------------------------------------------------------------//
DWORD 
ProcessRSNMessage1(
   HANDLE ndisUio, 
   WPA_KEY_DESCRIPTOR* pKeyDescriptor
)
{
   NDTLogVbs(  _T(": Enter ProcessRSNMessage1\n"));

   DWORD          keyInformation = WireToHost16(pKeyDescriptor->KeyInformation);
   BYTE*          pKeyData       = NULL;
   DWORD          keyDataLength  = 0;
   DWORD          status         = SUPPLICANT_FAILURE;
   
   do
   {
      //
      // If the data is encrypted, decrypt it
      //
      if((keyInformation & SSN_KEY_DESC_ENCRYPT_BIT) == SSN_KEY_DESC_ENCRYPT_BIT)
      {
         NDTLogErr(_T("Encryption bit set, bailing out\n"));
         break;
         // BUGBUG: This may only be required when rekeying is enabled
      }
      else
      {
         pKeyData       = pKeyDescriptor->KeyData;
         keyDataLength  = WireToHost16(pKeyDescriptor->KeyDataLength);
      }
   
      //
      // Must fill in the key length before we call to generate the PTK
      // 
      AuthContext.PTK.KeyLength = WireToHost16(pKeyDescriptor->KeyLength);

      //
      // Generate and return the PTK
      //
      if(GeneratePTK(&AuthContext.PreSharedKey,
                     AuthContext.AuthenticatorAddress,
                     pKeyDescriptor->KeyNonce, 
                     AuthContext.SupplicantAddress,
                     AuthContext.SNonce, 
                     &AuthContext.PTK) == false)
      {
         NDTLogErr(_T("Failed to generate PTK\n"));
         break;
      }

      //
      // Get the RSN information element to send back in the response
      //
      AuthContext.pIE = GetRSNIE(ndisUio);
      if(AuthContext.pIE == NULL)
      {
         NDTLogErr(_T("Failed to get RSN information element\n"));
         break;
      }

      //
      // If the ack bit is set, send a response
      //
      if((keyInformation & SSN_KEY_DESC_ACK_BIT) == SSN_KEY_DESC_ACK_BIT)
      {
         if(SendPacket(ndisUio, 
                       EAPOL_KEY_DESCRIPTOR_TYPE_WPA2,
                       AuthContext.SupplicantAddress, 
                       AuthContext.AuthenticatorAddress, 
                       AuthContext.PTK.MICKey, 
                       pKeyDescriptor->ReplayCounter,
                       (SSN_KEY_DESC_VERSION_TKIP | SSN_KEY_TYPE_PAIRWISE | SSN_KEY_DESC_MIC_BIT),
                       AuthContext.SNonce, 
                       (BYTE*)AuthContext.pIE,
                       AuthContext.pIE->length + 2) == false)
         {
            NDTLogErr(_T("Failed to send message 2 response\n"));
            break;
         }

         NDTLogDbg(_T("Message 2 response sent\n"));
      }
      
      status = SUPPLICANT_SUCCESS;

   }while(FALSE);

   free(AuthContext.pIE);

   NDTLogVbs(  _T(": Exit (Status = 0x%08x)\n"), status);
   return status;
}

//-----------------------------------------------------------------------------------------------//
DWORD 
ProcessRSNMessage3(
   HANDLE ndisUio, 
   WPA_KEY_DESCRIPTOR* pKeyDescriptor
)
{
   NDTLogVbs(  _T(": Enter ProcessRSNMessage3\n"));

   DWORD keyInformation = WireToHost16(pKeyDescriptor->KeyInformation);
   BYTE* pKeyData       = NULL;
   DWORD keyDataLength  = WireToHost16(pKeyDescriptor->KeyLength);
   DWORD offset         = 0;
   DWORD tag            = 0;
   DWORD length         = 0;
   BYTE* pValue         = NULL;
   DWORD status         = SUPPLICANT_FAILURE;
  
   do
   {
      //
      // If the data is encrypted, decrypt it
      //
      if((keyInformation & SSN_KEY_DESC_ENCRYPT_BIT) == SSN_KEY_DESC_ENCRYPT_BIT)
      {
         BYTE* pDecryptedData       = NULL;
         DWORD decryptedDataLength  = 0;
                  
         //
         // Decrypt the group key
         //
         if(DecryptKeyMaterial(AuthContext.PTK.EncryptionKey, 16, pKeyDescriptor, &pDecryptedData, &decryptedDataLength) == FALSE)
         {
            NDTLogErr(_T("DecryptKeyMaterial() failed\n"));
            break;
         }
      
         //
         // Add just the key material to account for padding
         //
         AdjustKeyMaterialLength(pDecryptedData, &decryptedDataLength);

         pKeyData       = pDecryptedData;
         keyDataLength  = decryptedDataLength;

      }
      else
      {
         pKeyData       = pKeyDescriptor->KeyData;
         keyDataLength  = WireToHost16(pKeyDescriptor->KeyDataLength);
      }
   
      //
      // If the ack bit is set, send a response
      //
      if((keyInformation & SSN_KEY_DESC_ACK_BIT) == SSN_KEY_DESC_ACK_BIT)
      {
         if(SendPacket(ndisUio, 
                       EAPOL_KEY_DESCRIPTOR_TYPE_WPA2,
                       AuthContext.SupplicantAddress, 
                       AuthContext.AuthenticatorAddress, 
                       AuthContext.PTK.MICKey, 
                       pKeyDescriptor->ReplayCounter,
                       (SSN_KEY_DESC_VERSION_TKIP | SSN_KEY_TYPE_PAIRWISE | SSN_KEY_DESC_MIC_BIT | SSN_KEY_DESC_SECURE_BIT),
                       NULL, 
                       NULL,
                       0) == false)
         {
            NDTLogErr(_T("Failed to send message 4 response\n"));
            break;
         }

         NDTLogDbg(_T("Message 4 response sent\n"));
      }
      
      //
      // Install the pairwise key
      //
      if(InstallKey(ndisUio, 
                    FIELD_OFFSET(NDIS_802_11_KEY, KeyMaterial) +  AuthContext.PTK.KeyLength,
                    &AuthContext.AuthenticatorAddress[0],
                    0xC0000000, // Bits: 31(Transmit), 30(Pairwise)
                    AuthContext.PTK.KeyLength,
                    WireToHost64(pKeyDescriptor->KeyRSC),
                    &AuthContext.PTK.Key[0]) == FALSE)
      {
         NDTLogErr(_T("Failed to add pairwise key\n"));
         break;
      }
      
      NDTLogDbg(_T("Pairwise key installed successfully (BSSID = %02x-%02x-%02x-%02x-%02x-%02x)\n"), AuthContext.AuthenticatorAddress[0],
                                                                                                                AuthContext.AuthenticatorAddress[1],
                                                                                                                AuthContext.AuthenticatorAddress[2],
                                                                                                                AuthContext.AuthenticatorAddress[3],
                                                                                                                AuthContext.AuthenticatorAddress[4],
                                                                                                                AuthContext.AuthenticatorAddress[5]);

      status = SUPPLICANT_HANDSHAKE_COMPLETE;

      // Install the Group Key if any.
      while(offset < keyDataLength) 
      {
         tag      = pKeyData[offset];
         length   = pKeyData[offset + 1];
         pValue   = pKeyData + offset + 2;

         if(tag == RSN_KEY_DATA_ENCAPSULATION_TAG)
         {
            if(memcmp(pValue, RSN_KEY_DATA_OUI, 3) == 0)
            {
               if(pValue[3] == RSN_KEY_DATA_OUI_TYPE_GKEY) 
               {
                  BYTE* pGTKIE      = pValue + 4;
                  DWORD gtkLength   = length - 4;
                  BYTE* pKey        = pGTKIE + 2;
                  DWORD keyLength   = gtkLength - 2;
                  DWORD keyIndex    = 0;
                  BYTE  bssid[]     = {0xff,0xff,0xff,0xff,0xff,0xff};
                  BYTE  keyInfo     = pGTKIE[0];

                  NDTLogDbg(_T(": Received Group Key IE in 3rd packet of 4 Way Handshake\n"));
                  
                  keyIndex = (keyInfo & GTK_INDEX_BITS);

                  //
                  // Save the index in our context so the script can access it
                  // 
                  AuthContext.GroupKeyIndex = keyIndex;

                  keyIndex |= 0x20000000; // Bit 29 (RSC)
                  if((keyInfo & GTK_TX_BITS) == GTK_INDEX_BITS)
                  {
                     keyIndex |= 0x80000000;    // Bit 31 (Transmit)
                  }

                  //
                  // Install the group key
                  //
                  if(InstallKey(ndisUio, 
                                FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial) + keyLength,
                                &bssid[0],
                                keyIndex,
                                keyLength,
                                WireToHost64(pKeyDescriptor->KeyRSC),
                                pKey) == FALSE)
                  {
                     NDTLogErr(_T(": Failed to add group key\n"));
                     break;
                  }       
                  
                  NDTLogDbg(_T("Group key installed successfully (Index = 0x%08x)\n"), keyIndex);  
                  
                  status = SUPPLICANT_GROUP_KEY_ADDED;         
               }
            }
         }

         offset += 2 + length;
      }

   }while(FALSE);
      
   //status = SUPPLICANT_SUCCESS;

   NDTLogVbs(  _T(": Exit (Status = 0x%08x)\n"), status);
   return status;
}

