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

#pragma once

#include "cdrom.h"

typedef struct
{
    HANDLE hDisk;           // The handle "below" us.
    Device_t* pCdDevice;
} CD_STORE_CONTEXT, *PCD_STORE_CONTEXT;

extern "C" {

DWORD CDROM_OpenStore( HANDLE hDisk, PDWORD pdwStoreId );
LRESULT CDROM_CloseStore( DWORD dwStoreId );
DWORD CDROM_FormatStore( DWORD dwStoreId );
DWORD CDROM_IsStoreFormatted( DWORD dwStoreId );
DWORD CDROM_GetStoreInfo( DWORD dwStoreId, PD_STOREINFO *pInfo );
DWORD CDROM_CreatePartition( DWORD dwStoreId, 
                             LPCTSTR szPartName, 
                             BYTE bPartType, 
                             SECTORNUM numSectors, 
                             BOOL bAuto );
DWORD CDROM_DeletePartition( DWORD dwStoreId, LPCTSTR szPartName );
DWORD CDROM_RenamePartition( DWORD dwStoreId, 
                             LPCTSTR szOldName, 
                             LPCTSTR szNewName );
DWORD CDROM_SetPartitionAttributes( DWORD dwStoreId, 
                                    LPCTSTR szPartName,
                                    DWORD dwAttr );
DWORD CDROM_SetPartitionSize( DWORD dwStoreId, 
                              LPCTSTR szPartName, 
                              SECTORNUM numSectors );
DWORD CDROM_FormatPartition( DWORD dwStoreId, 
                             LPCTSTR szPartName, 
                             BYTE bPartType, 
                             BOOL bAuto );
DWORD CDROM_GetPartitionInfo( DWORD dwStoreId, 
                              LPCTSTR szPartName, 
                              PD_PARTINFO *pInfo );
DWORD CDROM_FindPartitionStart( DWORD dwStoreId, PDWORD pdwSearchId );
DWORD CDROM_FindPartitionNext( DWORD dwSearchId, PD_PARTINFO *pInfo );
void CDROM_FindPartitionClose( DWORD dwSearchId );
DWORD CDROM_OpenPartition( DWORD dwStoreId, 
                           LPCTSTR szPartName, 
                           LPDWORD pdwPartitionId );
void CDROM_ClosePartition( DWORD dwPartitionId );
DWORD CDROM_DeviceIoControl( DWORD dwPartitionId,
                             DWORD Ioctl,
                             void* pInBuffer,
                             DWORD InBufferBytes,
                             void* pOutBuffer,
                             DWORD OutBufferBytes,
                             DWORD* pBytesReturned );
DWORD CDROM_MediaChangeEvent( DWORD dwStoreId, 
                              STORAGE_MEDIA_CHANGE_EVENT_TYPE EventId, 
                              STORAGE_MEDIA_ATTACH_RESULT* pResult );
BOOL CDROM_StoreIoControl( DWORD dwStoreId, 
                           DWORD Ioctl, 
                           LPVOID pInBuf, 
                           DWORD InBufSize, 
                           LPVOID pOutBuf, 
                           DWORD OutBufSize, 
                           PDWORD pBytesReturned, 
                           LPOVERLAPPED pOverlapped );


}
