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
#ifndef _UTILS_H
#define _UTILS_H

#include "wpadata.h"

//-----------------------------------------------------------------------------------------------//
BOOL
DecryptKeyMaterial(
    BYTE*               pKey,
    DWORD               keyLength,
    WPA_KEY_DESCRIPTOR* pKeyDescriptor,
    BYTE**              ppDecryptedData,
    DWORD*              pDecryptedDataLength
);

//-----------------------------------------------------------------------------------------------//
BOOL
AdjustKeyMaterialLength(
    BYTE*   pKeyData,
    DWORD*  pKeyDataLength
    );

//-----------------------------------------------------------------------------------------------//
VOID  
NextNonce(
   BYTE* pNonce
   );

//-----------------------------------------------------------------------------------------------//
DWORD 
GetRandomData(
   IN BYTE* pData, 
   IN DWORD nLength
   );

//-----------------------------------------------------------------------------------------------//
BOOL
InstallKey(
   HANDLE     handle,
   DWORD      length,
   BYTE*      pBssid,
   DWORD      keyIndex,
   DWORD      keyLength,
   ULONGLONG  keyRSC,
   BYTE*      pKey
   );

//-----------------------------------------------------------------------------------------------//
VOID  
CompareData(
   IN BYTE *pbOData1, 
   IN BYTE *pbOData2, 
   IN DWORD cbData, 
   OUT BYTE **ppbLowerValue, 
   OUT BYTE **ppbHigherValue
   );

//-----------------------------------------------------------------------------------------------//
BOOL 
GeneratePTK(
   PRESHARED_KEY*           preKey, 
   BYTE*                    pAuthAddress, 
   BYTE*                    pAuthNonce, 
   BYTE*                    pSuppAddress, 
   BYTE*                    pSuppNonce, 
   PAIRWISE_TEMPEROL_KEY*   pPTK
   );

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
   );

//-----------------------------------------------------------------------------------------------//
INFO_ELEMENT* 
GetWPAIE(
   HANDLE ndisUio
   );

//-----------------------------------------------------------------------------------------------//
INFO_ELEMENT* 
GetRSNIE(
   HANDLE ndisUio
   );

//-----------------------------------------------------------------------------------------------//
BYTE* 
FindWPAIE(
   BYTE* pRequestIE, 
   DWORD nRequestIELen
);

//-----------------------------------------------------------------------------------------------//
BYTE* 
FindRSNIE(
   BYTE* pRequestIE, 
   DWORD nRequestIELen
);

//-----------------------------------------------------------------------------------------------//
BOOL
AquireCryptContext(void);

//-----------------------------------------------------------------------------------------------//
void 
ReleaseCryptContext(void);



#endif
