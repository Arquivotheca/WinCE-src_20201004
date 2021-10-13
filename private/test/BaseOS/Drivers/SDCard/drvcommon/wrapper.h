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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
// Copyright (c) Microsoft Corporation.  All rights reserved.//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------
//

#ifndef WRAPPER_H
#define WRAPPER_H

#include <sdcard.h>
#include <sdcardddk.h>
#include <sdmem.h>
#include <windows.h>

//1 SDClient Wrappers
//2 SD & SDIO
SD_API_STATUS wrap_SDCardInfoQuery(SD_DEVICE_HANDLE hDevice, SD_INFO_TYPE InfoType, PVOID pCardInfo,  ULONG  StructureSize);

SD_API_STATUS wrap_SDSynchronousBusRequest(SD_DEVICE_HANDLE  hDevice, UCHAR Command, DWORD Argument,
                    SD_TRANSFER_CLASS TransferClass, SD_RESPONSE_TYPE ResponseType, PSD_COMMAND_RESPONSE pResponse, ULONG NumBlocks,
                    ULONG  BlockSize, PUCHAR pBuffer, DWORD Flags, BOOL bAppCMD=FALSE, PSD_CARD_STATUS pCrdStat = NULL, BOOL bSkipState = FALSE);

SD_API_STATUS wrap_SDBusRequest(SD_DEVICE_HANDLE  hDevice, UCHAR Command, DWORD Argument, SD_TRANSFER_CLASS TransferClass,
                    SD_RESPONSE_TYPE ResponseType, ULONG NumBlocks, ULONG BlockSize, PUCHAR pBuffer, PSD_BUS_REQUEST_CALLBACK pCallback,
                                DWORD RequestParam, PSD_BUS_REQUEST *ppRequest, DWORD Flags, BOOL bAppCMD=FALSE);

VOID wrap_SDFreeBusRequest(PSD_BUS_REQUEST  pRequest);

BOOLEAN wrap_SDCancelBusRequest(PSD_BUS_REQUEST  pRequest);

SD_API_STATUS wrap_SDSetCardFeature(SD_DEVICE_HANDLE hDevice, SD_SET_FEATURE_TYPE CardFeature, PVOID pCardInfo, ULONG StructureSize);

//2 SDIO only
SD_API_STATUS wrap_SDIOConnectInterrupt(SD_DEVICE_HANDLE hDevice, PSD_INTERRUPT_CALLBACK pIsrFunction);

void wrap_SDIODisconnectInterrupt(SD_DEVICE_HANDLE hDevice);

SD_API_STATUS wrap_SDGetTuple(SD_DEVICE_HANDLE hDevice, UCHAR TupleCode, PUCHAR pBuffer, PULONG pBufferSize, BOOL CommonCIS);

SD_API_STATUS wrap_SDReadWriteRegistersDirect(SD_DEVICE_HANDLE hDevice, SD_IO_TRANSFER_TYPE ReadWrite, UCHAR Function, DWORD Address, BOOLEAN ReadAfterWrite, PUCHAR pBuffer, ULONG BufferLength);

//1 Non-Client wrappers
DWORD wrap_GetBitSlice(UCHAR const *pBuffer, ULONG BufferSize, DWORD BitOffset, UCHAR NumberOfBits);

void wrap_SDOutputBuffer(PVOID pBuffer, ULONG BufferSize);

SD_MEMORY_LIST_HANDLE wrap_SDCreateMemoryList(ULONG Tag, ULONG Depth, ULONG EntrySize);

void wrap_SDDeleteMemList(SD_MEMORY_LIST_HANDLE hList);

void wrap_SDFreeToMemList(SD_MEMORY_LIST_HANDLE hList, PVOID pMemory);

PVOID wrap_SDAllocateFromMemList(SD_MEMORY_LIST_HANDLE hList);

#endif //WRAPPER_H

