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
#ifndef __VOLUME_H__
#define __VOLUME_H__

#include "main.h"
#include <pnp.h>
#include <devload.h>
#include <msgqueue.h>

// ----------------------------------------------------------------------------
// Define a custom activation GUID for the PDD Driver
// ----------------------------------------------------------------------------
static const GUID FLASH_PDD_MOUNT_GUID = { 0xf609a06, 0x5970, 0x46f0, { 0x90, 0x5b, 0xf2, 0x7c, 0xd4, 0xec, 0x29, 0x6a } };
#define FLASH_PDD_MOUNT_GUID_STRING    _T("{0F609A06-5970-46f0-905B-F27CD4EC296A}")
#define MAX_DEVNAME_LEN         100
#define QUEUE_ITEM_SIZE         (sizeof(DEVDETAIL) + MAX_DEVNAME_LEN)

// ----------------------------------------------------------------------------
// Functions for initializing the volume as well as performing PDD operations
// ----------------------------------------------------------------------------
BOOL InitVolume();

HANDLE LoadDevice(
    IN LPTSTR szPDDLibraryName,
    IN LPTSTR szPDDRegistryKey,
    IN OUT PHANDLE phActivateHandle);

BOOL GetFlashRegionCount(
    IN HANDLE hDevice,
    OUT LPDWORD lpdwFlashRegionCount);

BOOL GetFlashRegionInfo(
    IN HANDLE hDevice,
    OUT FLASH_REGION_INFO * pFlashRegionInfoTable,
    IN DWORD dwFlashRegionInfoTableSize);

void ShowFlashRegionInfo(
    IN DWORD dwFlashRegionCount,
    IN FLASH_REGION_INFO * pFlashRegionInfoTable);

BOOL GetBlockStatus(
    IN HANDLE hDevice,
    IN FLASH_GET_BLOCK_STATUS_REQUEST * pBlockStatusRequest,
    OUT PULONG pStatusBuffer,
    IN DWORD dwStatusBufferSize);

BOOL SetBlockStatus(
    IN HANDLE hDevice,
    IN FLASH_SET_BLOCK_STATUS_REQUEST * pBlockStatusRequest);

BOOL ReadSectors(
    IN HANDLE hDevice,
    IN FLASH_PDD_TRANSFER * pFlashTransferRequests,
    IN DWORD dwFlashTransferSize);

BOOL WriteSectors(
    IN HANDLE hDevice,
    IN FLASH_PDD_TRANSFER * pFlashTransferRequests,
    IN DWORD dwFlashTransferSize);

BOOL EraseBlocks(
    IN HANDLE hDevice,
    IN BLOCK_RUN * pBlockRun,
    IN DWORD dwBlockRunArraySize);

BOOL CopySectors(
    IN HANDLE hDevice,
    IN FLASH_PDD_COPY * pFlashPddCopy,
    IN DWORD dwPddCopyArraySize,
    IN BOOL fCopySupported = TRUE);

BOOL WriteReadVerify(
    IN HANDLE hDevice,
    IN FLASH_PDD_TRANSFER * pWriteRequest,
    IN DWORD dwRequestSize,
    IN DWORD dwDataBufferSize,
    IN DWORD dwBytesPerRequest);

BOOL WriteCopyAndVerify(
    IN HANDLE hDevice,
    IN FLASH_REGION_INFO flashRegionInfo,
    IN FLASH_PDD_COPY * pFlashPddCopy,
    IN DWORD dwFlashPddCopySize);

ULONG GetGoodBlockRun(
    IN DWORD dwRequestedRunSize,
    IN ULONGLONG ullBlockCount,
    IN OUT PULONGLONG pullCurrentBlock,
    IN PULONG pBlockStatusBuffer,
    OUT BLOCK_RUN * pBlockRun,
    IN BOOL fUseReservedSectors);

BOOL GetStatusForAllBlocks(
    IN HANDLE hDevice,
    IN FLASH_REGION_INFO flashRegionInfo,
    IN DWORD dwFlashRegionIndex,
    IN OUT PULONG * ppulStatusBuffer);

BOOL EraseAndVerify(
    IN HANDLE hDevice,
    IN FLASH_REGION_INFO flashRegionInfo,
    IN BLOCK_RUN * pBlockRun,
    IN DWORD dwBlockRunArraySize);

void ShowProgress(
    IN OUT PULONGLONG pulPercent,
    IN ULONGLONG ulCurrent,
    IN ULONGLONG ulTotal);

HKEY OpenRegistryKey(
    IN LPCTSTR pszKeyName);

BOOL WriteRegistryStringValue(
    IN LPCTSTR pszKeyName,
    IN LPCTSTR pszValueName,
    IN LPCTSTR pszStringValue);

BOOL WriteRegistryMultiStringValue(
    IN LPCTSTR pszKeyName,
    IN LPCTSTR pszValueName,
    IN LPCTSTR pszStringValue);

BOOL PowerOfTwo(
    IN DWORD dwNumber);

#endif //__VOLUME_H__