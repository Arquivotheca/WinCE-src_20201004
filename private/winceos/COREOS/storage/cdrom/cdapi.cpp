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

// -----------------------------------------------------------------------------
// Version 1 (2/2006)
//
// CDRom.DLL is used to provide functionality across all CD-ROM type devices
// independent of the file system or the underlying bus.  It is assumed that the
// underlying bus will support the IOCTL_SCSI_PASS_THROUGH and
// IOCTL_SCSI_PASS_THROUGH_DIRECT functions.
//
// Each CD-ROM type device should support the SCSI MMC x command set.
//
// This version will focus on the basics.  For example, GESN support for events
// can be added to eliminate device polling for media changes.  However, as not
// all devices support this functionality (let alone support it correctly), it
// has not been added at this time.
//
// Standard IOCTLS which are support on both the desktop and CE will be here.
// This includes support for CSS/AACS.
// -----------------------------------------------------------------------------

#include "cdapi.h"

// /////////////////////////////////////////////////////////////////////////////
// CDROM_OpenStore
//
DWORD CDROM_OpenStore( HANDLE hDisk, PDWORD pdwStoreId )
{
    DWORD dwResult = ERROR_SUCCESS;
    PCD_STORE_CONTEXT pContext = NULL;
    Device_t* pCdDevice = NULL;

    //
    // Save the hook information.  We will initialize the CD Device in
    // the Open call with the hook information we have here.
    //
    pContext = new CD_STORE_CONTEXT;
    if( !pContext )
    {
        dwResult = ERROR_NOT_ENOUGH_MEMORY;
        goto exit_hookstore;
    }

    pContext->hDisk = hDisk;

    pCdDevice = new Device_t();
    if( !pCdDevice )
    {
        dwResult = ERROR_NOT_ENOUGH_MEMORY;
        goto exit_hookstore;
    }

    if( !pCdDevice->Initialize( hDisk ) )
    {
        dwResult = GetLastError();
        if( NO_ERROR == dwResult ) {
            dwResult = ERROR_GEN_FAILURE;
        }
        goto exit_hookstore;
    }

    pContext->pCdDevice = pCdDevice;

    *pdwStoreId = (DWORD)pContext;

    pCdDevice = NULL;
    pContext = NULL;

exit_hookstore:
    if( pContext )
    {
        delete pContext;
    }

    if( pCdDevice )
    {
        delete pCdDevice;
    }

    return dwResult;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_CloseStore
//
LRESULT CDROM_CloseStore( DWORD dwStoreId )
{

    LRESULT lResult = ERROR_SUCCESS;
    PCD_STORE_CONTEXT pContext = (PCD_STORE_CONTEXT)dwStoreId;

    ASSERT( pContext != NULL );

    if( pContext != NULL )
    {
        if( pContext->pCdDevice != NULL )
        {
            delete pContext->pCdDevice;
        }

        delete pContext;
    }

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_FormatStore
//
DWORD CDROM_FormatStore( DWORD dwStoreId )
{
    //
    // TODO::Add Support
    // This call can eventually be implemented to issue a physical format for
    // CD/DVD media that requires a full physical format.  Note that this is
    // only required for packet writing, or in order to blank some discs to
    // change writing formats (packet to sao/sao.)
    //

    return ERROR_GEN_FAILURE;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_IsStoreFormatted
//
DWORD CDROM_IsStoreFormatted( DWORD dwStoreId )
{
    return ERROR_SUCCESS;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_GetStoreInfo
//
// TODO::IOCTL_DISK_GETINFO is going to fail each time as ATAPI does not support
// this command on CDROMs.  We need to change this, though it doesn't seem to
// hurt the storage manager at all right now for read-only support.
//
//
DWORD CDROM_GetStoreInfo( DWORD dwStoreId, PD_STOREINFO *pInfo )
{
    DISK_INFO DiskInfo = { 0 };
    DWORD dwBytesReturned = 0;
    PCD_STORE_CONTEXT pContext = (PCD_STORE_CONTEXT)dwStoreId;

    ZeroMemory( &DiskInfo, sizeof(DiskInfo) );
    ZeroMemory( pInfo, sizeof(PD_STOREINFO) );

    if( pInfo->cbSize != sizeof(PD_STOREINFO) )
    {
        return ERROR_INVALID_PARAMETER;
    }

    //
    // TODO::Deteremine if the media is blank and/or read-only.
    //
    pInfo->dwAttributes = STORE_ATTRIBUTE_REMOVABLE;
    pInfo->dwBytesPerSector = DiskInfo.di_bytes_per_sect;
    pInfo->snNumSectors = DiskInfo.di_total_sectors;

    return TRUE;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_CreatePartition
//
DWORD CDROM_CreatePartition( DWORD dwStoreId,
                             LPCTSTR szPartName,
                             BYTE bPartType,
                             SECTORNUM numSectors,
                             BOOL bAuto )
{
    return ERROR_GEN_FAILURE;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_DeletePartition
//
DWORD CDROM_DeletePartition( DWORD dwStoreId, LPCTSTR szPartName )
{
    return ERROR_GEN_FAILURE;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_RenamePartition
//
DWORD CDROM_RenamePartition( DWORD dwStoreId,
                             LPCTSTR szOldName,
                             LPCTSTR szNewName )
{
    return ERROR_GEN_FAILURE;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_SetPartitionAttrs
//
DWORD CDROM_SetPartitionAttributes( DWORD dwStoreId,
                                    LPCTSTR szPartName,
                                    DWORD dwAttr )
{
    return ERROR_GEN_FAILURE;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_SetPartitionSize
//
DWORD CDROM_SetPartitionSize( DWORD dwStoreId,
                              LPCTSTR szPartName,
                              SECTORNUM numSectors )
{
    return ERROR_GEN_FAILURE;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_FormatPartition
//
DWORD CDROM_FormatPartition( DWORD dwStoreId,
                             LPCTSTR szPartName,
                             BYTE bPartType,
                             BOOL bAuto )
{
    return ERROR_GEN_FAILURE;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_GetPartitionInfo
//
// TODO::We're only going to report one partition for now.  Add support for
// mixed CDs, with an audio and a data track.  This is very simple now.  We will
// want to consider all possible combinations of tracks, modes, etc later.
//
// When adding multiple volumes, consider building a table of all the necessary
// information for the media with functions that just query from that
// information.
//
DWORD CDROM_GetPartitionInfo( DWORD dwStoreId,
                              LPCTSTR szPartName,
                              PD_PARTINFO *pInfo )
{
    PCD_STORE_CONTEXT pContext = (PCD_STORE_CONTEXT)dwStoreId;

    ASSERT( pContext != NULL );
    ASSERT( pContext->pCdDevice != NULL );
    ASSERT( pContext->hDisk != INVALID_HANDLE_VALUE );

    CdPartMgr_t* pPartMgr = pContext->pCdDevice->GetPartMgr();

    return pPartMgr->GetPartitionInfo( szPartName, pInfo );
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_FindPartitionStart
//
DWORD CDROM_FindPartitionStart( DWORD dwStoreId, PDWORD pdwSearchId )
{
    PCD_STORE_CONTEXT pStoreContext = (PCD_STORE_CONTEXT)dwStoreId;
    PSEARCH_CONTEXT pdwStoreId = new SEARCH_CONTEXT;

    if( !pdwStoreId )
    {
        return ERROR_OUTOFMEMORY;
    }

    pdwStoreId->pDevice = pStoreContext->pCdDevice;
    pdwStoreId->SessionIndex = 1;
    *pdwSearchId = (DWORD)pdwStoreId;

    return ERROR_SUCCESS;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_FindPartitionNext
//
DWORD CDROM_FindPartitionNext( DWORD dwSearchId, PD_PARTINFO *pInfo )
{
    PSEARCH_CONTEXT pSearchContext = (PSEARCH_CONTEXT)dwSearchId;
    CdPartMgr_t* pPartMgr = NULL;

    pPartMgr = pSearchContext->pDevice->GetPartMgr();

    return pPartMgr->FindPartitionNext( pSearchContext, pInfo );
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_FindPartitionClose
//
void CDROM_FindPartitionClose( DWORD dwSearchId )
{
    delete (PSEARCH_CONTEXT)dwSearchId;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_OpenPartition
//
DWORD CDROM_OpenPartition( DWORD dwStoreId,
                           LPCTSTR szPartName,
                           LPDWORD pdwPartitionId )
{
    PCD_STORE_CONTEXT pStoreContext = (PCD_STORE_CONTEXT)dwStoreId;
    PPARTITION_CONTEXT pPartContext = new PARTITION_CONTEXT;
    USHORT Partition = 0;
    DWORD dwResult = ERROR_SUCCESS;

    if( !pStoreContext->pCdDevice->GetPartMgr()->GetIndexFromName( szPartName, &Partition ) )
    {
        dwResult = GetLastError();
        goto exit_openpartition;
    }

    if( !pStoreContext->pCdDevice->GetPartMgr()->IsValidPartition( Partition ) )
    {
        dwResult = ERROR_INVALID_PARAMETER;
        goto exit_openpartition;
    }

    if( pPartContext == NULL )
    {
        dwResult = ERROR_INVALID_PARAMETER;
        goto exit_openpartition;
    }

    pPartContext->pDevice = pStoreContext->pCdDevice;
    pPartContext->SessionIndex = Partition;

exit_openpartition:
    if( pPartContext && dwResult != ERROR_SUCCESS )
    {
        delete pPartContext;
    }

    *pdwPartitionId = (DWORD)pPartContext;
    return ERROR_SUCCESS;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_ClosePartition
//
void CDROM_ClosePartition( DWORD dwPartitionId )
{
    delete (PPARTITION_CONTEXT)dwPartitionId;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_IoControl
//
DWORD CDROM_DeviceIoControl( DWORD dwPartitionId,
                             DWORD Ioctl,
                             void* pInBuffer,
                             DWORD InBufferBytes,
                             void* pOutBuffer,
                             DWORD OutBufferBytes,
                             DWORD* pBytesReturned )
{
    DWORD dwResult = ERROR_SUCCESS;
    PPARTITION_CONTEXT pPartContext = (PPARTITION_CONTEXT)dwPartitionId;
    Device_t* pCdDevice = pPartContext->pDevice;

    //
    // This try/except should never really be used, but makes a good catch-
    // all in case I missed something somewhere else.
    //
    __try
    {
        if( !pCdDevice->CdDeviceIoControl( pPartContext->SessionIndex,
                                           Ioctl,
                                           pInBuffer,
                                           InBufferBytes,
                                           pOutBuffer,
                                           OutBufferBytes,
                                           pBytesReturned,
                                           NULL ) )
        {
            dwResult = GetLastError();
        }
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        ASSERT(FALSE);
        dwResult = ERROR_INVALID_PARAMETER;

    }

    return dwResult;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_MediaChangeEvent
//
DWORD CDROM_MediaChangeEvent( DWORD dwStoreId,
                              STORAGE_MEDIA_CHANGE_EVENT_TYPE EventId,
                              STORAGE_MEDIA_ATTACH_RESULT* pResult )
{
    DWORD dwResult = ERROR_SUCCESS;
    PCD_STORE_CONTEXT pStoreContext = (PCD_STORE_CONTEXT)dwStoreId;
    Device_t* pDevice = pStoreContext->pCdDevice;

    if( pResult )
    {
        *pResult = pDevice->GetMediaChangeResult();
    }

    return dwResult;
}

// /////////////////////////////////////////////////////////////////////////////
// CDROM_MediaChangeEvent
//
BOOL CDROM_StoreIoControl( DWORD dwStoreId,
                           DWORD Ioctl,
                           LPVOID pInBuf,
                           DWORD InBufSize,
                           LPVOID pOutBuf,
                           DWORD OutBufSize,
                           PDWORD pBytesReturned,
                           LPOVERLAPPED pOverlapped )
{
    PCD_STORE_CONTEXT pContext = (PCD_STORE_CONTEXT)dwStoreId;
    Device_t* pDevice = pContext->pCdDevice;

    return pDevice->CdDeviceStoreIoControl( Ioctl,
                                          pInBuf,
                                          InBufSize,
                                          pOutBuf,
                                          OutBufSize,
                                          pBytesReturned,
                                          pOverlapped );
}
