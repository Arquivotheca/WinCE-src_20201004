//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    romsig.h

Abstract:


Revision History:
    1/23/01         created

--*/

#pragma once

#include "udcryptoapi.h"
#include "udsha.h"

#pragma pack(push)
#pragma pack(1)

// HWID Type version info
#define ROMSIG_HWID_TYPE_V1       1UL

// Key ID
#define ROMSIG_KEY_ID_V1          L"OEM512"

// General version
#define ROMSIG_VERSION_V1         {1, 0, 0, 0}

// Uh oh, it's magic
#define ROMSIG_MAGIC_ROM_MODULE  0x53494730

//
// structure that gets written into the sig file
// contains the signature and the areas of memory
// that were used to create it
//

typedef struct tagRomSigMemRegion {
  LPVOID  lpvAddress;     // physical address of region
  DWORD   dwLength;       // length of region
}ROMSIG_MEMREGION, *PROMSIG_MEMREGION;

typedef struct tagRomSigInfo {
  BYTE    byDigest[CUDSha1::DIGESTSIZE];      // the hash 
  DWORD   dwMagic;                            // ex: ROMSIG_MAGIC_ROM_MODULE
  WORD    rgwVersion[4];                      // ex: 1.0.0.0
  WCHAR   wszKeyId[32];                       // ex: L"OEM512"
  DWORD   dwOEMIoControl;                     // location of OEMIoControl function
  DWORD   dwHWIDType;                         // version number for HWID method
  DWORD   dwNumMemRegions;                    // number of ROMSIG_MEMREGIONs following
}ROMSIG_INFO, *PROMSIG_INFO;

#pragma pack(pop)

