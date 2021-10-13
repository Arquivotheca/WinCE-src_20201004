//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "main.h"
#include "globals.h"

BOOL Dsk_GetDiskInfo(HANDLE hDisk, DISK_INFO *pDiskInfo);
BOOL Dsk_FormatMedia(HANDLE hDisk);
BOOL Dsk_WriteSectors(HANDLE hDisk, DWORD startSector, DWORD cSectors, PBYTE pData);
BOOL Dsk_ReadSectors(HANDLE hDisk, DWORD startSector, DWORD cSectors, PBYTE pData);
BOOL Dsk_DeleteSectors(HANDLE hDisk, DWORD startSector, DWORD cSectors);
