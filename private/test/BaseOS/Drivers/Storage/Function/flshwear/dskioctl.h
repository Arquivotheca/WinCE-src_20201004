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
#include "main.h"
#include "globals.h"

BOOL Dsk_GetDiskInfo(HANDLE hDisk, DISK_INFO *pDiskInfo);
BOOL Dsk_FormatMedia(HANDLE hDisk);
BOOL Dsk_WriteSectors(HANDLE hDisk, DWORD startSector, DWORD cSectors, PBYTE pData);
BOOL Dsk_ReadSectors(HANDLE hDisk, DWORD startSector, DWORD cSectors, PBYTE pData);
BOOL Dsk_DeleteSectors(HANDLE hDisk, DWORD startSector, DWORD cSectors);
BOOL Dsk_GetFlashPartitionInfo(HANDLE hDisk, FLASH_PARTITION_INFO * pPartTable);
BOOL Dsk_GetFlashRegionInfo(HANDLE hDisk, FLASH_REGION_INFO * pRegionTable);
