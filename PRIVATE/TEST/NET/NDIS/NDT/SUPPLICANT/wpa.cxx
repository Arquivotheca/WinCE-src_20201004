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
#include "wpa.h"
#include "supplib.h"

extern BYTE EtherType[SIZE_ETHERNET_TYPE];
extern UINT LastMessage;

//-----------------------------------------------------------------------------------------------//
DWORD
ProcessWPAPacket(
   HANDLE         ndisUio, 
   EAPOL_PACKET*  pEapolPacket
)
{
   NDTLogVbs(  _T(": Enter ProcessWPAPacket\n")); 

   WPA_KEY_DESCRIPTOR*  pKeyDescriptor    = (WPA_KEY_DESCRIPTOR*)((PBYTE)pEapolPacket + FIELD_OFFSET(EAPOL_PACKET, PacketBody));
   USHORT               keyInformation    = WireToHost16(pKeyDescriptor->KeyInformation);
   DWORD                status            = SUPPLICANT_FAILURE;
 
   //
   // Message 1
   //
   if((keyInformation & SSN_KEY_DESC_KEY_TYPE_BIT) == SSN_KEY_TYPE_PAIRWISE && 
      (keyInformation & SSN_KEY_DESC_MIC_BIT) == 0)
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
      status = ProcessWPAMessage1(ndisUio, pKeyDescriptor);
   }
   
   //
   // Message 3
   //
   if((keyInformation & SSN_KEY_DESC_KEY_TYPE_BIT) == SSN_KEY_TYPE_PAIRWISE && 
      (keyInformation & SSN_KEY_DESC_MIC_BIT) == SSN_KEY_DESC_MIC_BIT && 
      (keyInformation & SSN_KEY_DESC_KEY_USAGE_BIT) == SSN_KEY_DESC_KEY_USAGE_BIT)
   {   
      LastMessage = 3;
      status = ProcessWPAMessage3(ndisUio, pKeyDescriptor);
   }
   
   //
   // Message 5 (Group Key)
   //
   if((keyInformation & SSN_KEY_DESC_KEY_TYPE_BIT) == SSN_KEY_TYPE_GROUP)
   {
      LastMessage = 5;
      status = ProcessWPAGroupKey(ndisUio, pKeyDescriptor);
   }

   NDTLogVbs(  _T(": Exit (Status = %d)\n"), status);
   return status;
}

//-----------------------------------------------------------------------------------------------//
DWORD
ProcessWPAMessage1(
   HANDLE               ndisUio, 
   WPA_KEY_DESCRIPTOR*  pKeyDescriptor
)
{
   NDTLogVbs(_T(": Enter ProcessWPAMessage1\n"));
   
   DWORD keyInformation = WireToHost16(pKeyDescriptor->KeyInformation);
   DWORD status         = SUPPLICANT_FAILURE;

   do
   {
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
         NDTLogErr( _T("Failed to generate PTK\n"));
         break;
      }
   
      //
      // Get the WPA information element to send back in the response
      //
      AuthContext.pIE = GetWPAIE(ndisUio);
      if(AuthContext.pIE == NULL)
      {
         NDTLogErr( _T("Failed to get WPA information element\n"));
         break;
      }
   
      //
      // If the ack bit is set, send a response
      //
      if((keyInformation & SSN_KEY_DESC_ACK_BIT) == SSN_KEY_DESC_ACK_BIT)
      {
         if(SendPacket(ndisUio, 
                       EAPOL_KEY_DESCRIPTOR_TYPE_WPA,
                       AuthContext.SupplicantAddress, 
                       AuthContext.AuthenticatorAddress, 
                       AuthContext.PTK.MICKey, 
                       pKeyDescriptor->ReplayCounter,
                       (SSN_KEY_DESC_VERSION_TKIP | SSN_KEY_TYPE_PAIRWISE | SSN_KEY_DESC_MIC_BIT),
                       AuthContext.SNonce, 
                       (BYTE*)AuthContext.pIE,
                       AuthContext.pIE->length + 2) == false)
         {
            NDTLogErr( _T("Failed to send message 2 response\n"));
            break;
         }

         NDTLogDbg( _T("Message 2 response sent\n"));
      }
      
      status = SUPPLICANT_SUCCESS;

   }while(FALSE);

   free(AuthContext.pIE);

   NDTLogVbs(_T(": Exit (Status = 0x%08x)\n"), status);
   return status;
}

//-----------------------------------------------------------------------------------------------//
DWORD
ProcessWPAMessage3(
   HANDLE               ndisUio, 
   WPA_KEY_DESCRIPTOR*  pKeyDescriptor
)
{
   NDTLogVbs(_T(": Enter ProcessWPAMessage3\n"));

   USHORT   keyInformation    = WireToHost16(pKeyDescriptor->KeyInformation);
   DWORD    status            = SUPPLICANT_FAILURE;
   
   do
   {
      
      //
      // Send the response if required
      //
      if(keyInformation & SSN_KEY_DESC_ACK_BIT)
      {
         if(SendPacket(ndisUio, 
                       EAPOL_KEY_DESCRIPTOR_TYPE_WPA,
                       AuthContext.SupplicantAddress, 
                       AuthContext.AuthenticatorAddress, 
                       AuthContext.PTK.MICKey, 
                       pKeyDescriptor->ReplayCounter,
                       (SSN_KEY_DESC_VERSION_TKIP | SSN_KEY_TYPE_PAIRWISE | SSN_KEY_DESC_MIC_BIT),
                       NULL, 
                       0, 
                       NULL) == false)
         {
            NDTLogErr( _T("Failed to send message 4 response\n"));
            break;
         }

         NDTLogDbg( _T("Message 4 response sent\n"));
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
         NDTLogErr( _T("Failed to add pairwise key\n"));
         break;
      }

      NDTLogDbg( _T("Pairwise key installed successfully (BSSID = %02x-%02x-%02x-%02x-%02x-%02x)\n"), AuthContext.AuthenticatorAddress[0],
                                                                                                                AuthContext.AuthenticatorAddress[1],
                                                                                                                AuthContext.AuthenticatorAddress[2],
                                                                                                                AuthContext.AuthenticatorAddress[3],
                                                                                                                AuthContext.AuthenticatorAddress[4],
                                                                                                                AuthContext.AuthenticatorAddress[5]);

      status = SUPPLICANT_HANDSHAKE_COMPLETE;

   }while(FALSE);
   
   NDTLogVbs(_T(": Exit (Status = 0x%08x)\n"), status);  
   return status;
}

//-----------------------------------------------------------------------------------------------//
DWORD
ProcessWPAGroupKey(
   HANDLE               ndisUio, 
   WPA_KEY_DESCRIPTOR*  pKeyDescriptor 
)
{
   NDTLogVbs(_T(": Enter ProcessWPAGroupKey\n"));

   USHORT   keyInformation       = WireToHost16(pKeyDescriptor->KeyInformation);
   BYTE     bssid[]              = {0xff,0xff,0xff,0xff,0xff,0xff};
   BYTE*    pDecryptedData       = NULL;
   DWORD    decryptedDataLength  = 0;
   DWORD    status               = SUPPLICANT_FAILURE;
   DWORD    keyIndex             = 0;

   do
   {
      //
      // Decrypt the group key
      //
      if(DecryptKeyMaterial(AuthContext.PTK.EncryptionKey, 16, pKeyDescriptor, &pDecryptedData, &decryptedDataLength) == FALSE)
      {
         NDTLogErr( _T("DecryptKeyMaterial() failed\n"));
         break;
      }
     
      //
      // Send the Response if required
      //
      if(keyInformation & SSN_KEY_DESC_ACK_BIT)
      {
         if(SendPacket(ndisUio, 
                       EAPOL_KEY_DESCRIPTOR_TYPE_WPA,
                       AuthContext.SupplicantAddress, 
                       AuthContext.AuthenticatorAddress, 
                       AuthContext.PTK.MICKey, 
                       pKeyDescriptor->ReplayCounter,
                       (SSN_KEY_DESC_VERSION_TKIP | SSN_KEY_TYPE_GROUP | SSN_KEY_DESC_MIC_BIT | SSN_KEY_DESC_SECURE_BIT),
                       NULL, 
                       0, 
                       NULL) == false)
         {
            NDTLogErr( _T("Failed to send group key message response\n"));
            break;
         }
         
         NDTLogDbg( _T("Group key message response sent\n"));
      }
      
      keyIndex = (keyInformation & SSN_KEY_DESC_KEY_INDEX_BITS) >> SSN_KEY_DESC_INDEX_BITS_OFFSET;

      //
      // Save the index in our context so the script can access it
      // 
      AuthContext.GroupKeyIndex = keyIndex;

      keyIndex |= 0x20000000;        // Bit 29 (Key Rsc set)
      if(keyInformation & SSN_KEY_DESC_KEY_USAGE_BIT)
      {
         keyIndex |= 0x80000000;    // Bit 31 (Transmit key)
      }
     
      //
      // Install the group key
      //
      if(InstallKey(ndisUio, 
                    FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial) + WireToHost16(pKeyDescriptor->KeyLength),
                    &bssid[0],
                    keyIndex,
                    decryptedDataLength,
                    WireToHost64(pKeyDescriptor->KeyRSC),
                    pDecryptedData) == FALSE)
      {
         NDTLogErr( _T("Failed to add group key\n"));
         break;
      }

      NDTLogDbg( _T("Group key installed successfully (Index = 0x%08x)\n"), keyIndex);

      status = SUPPLICANT_GROUP_KEY_ADDED;

   }while(FALSE);
   
   free(pDecryptedData);

   NDTLogVbs(_T(": Exit (Status = 0x%08x)\n"), status);
   return status;
}