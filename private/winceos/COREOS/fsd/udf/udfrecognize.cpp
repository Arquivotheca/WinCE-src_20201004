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

#include "Udf.h"
#include "UdfRecognize.h"

// /////////////////////////////////////////////////////////////////////////////
// UDF_RecognizeVolumeCD
//
// This is exported as the detector function for UDF to identify this media as
// being UDF mountable or not.
//
LRESULT UDF_RecognizeVolumeCD( HANDLE hDisk,
                               const BYTE* pBootSector,
                               DWORD dwBootSectorSize )
{
    LRESULT lResult = ERROR_BAD_FORMAT;
    DWORD dwSectorSize = CD_SECTOR_SIZE;
    DWORD dwBytesReturned = 0;
    DWORD dwBufferSize = 0;
    USHORT TrackIndx = 0;
    CD_PARTITION_INFO* pPartInfo = NULL;
    TRACK_INFORMATION2* pTrackInfo = NULL;

    //
    // These are used when actually reading the VRS from the disc.
    //
    const DWORD dwBuffSize = MAX_READ_SIZE;
    CDROM_READ CdRead = { 0 };
    BOOL fFoundBEA = FALSE;
    BOOL fFoundTEA = FALSE;
    BOOL fFoundNSR = FALSE;
    BYTE* pVRSBuffer = NULL;
    DWORD dwRead = 0;

    dwBufferSize = sizeof(CD_PARTITION_INFO) +
                   MAXIMUM_NUMBER_TRACKS_LARGE * sizeof(TRACK_INFORMATION2);

    pPartInfo = (PCD_PARTITION_INFO)new(UDF_TAG_BYTE_BUFFER) BYTE[ dwBufferSize ];
    if( !pPartInfo )
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    //
    // Find out where our session begins, track information, etc.
    //
    if( !FSDMGR_DiskIoControl( (DWORD)hDisk,
                               IOCTL_CDROM_GET_PARTITION_INFO,
                               NULL,
                               0,
                               pPartInfo,
                               dwBufferSize,
                               &dwBytesReturned,
                               NULL ) )
    {
        lResult = GetLastError();
        goto exit_udf_recognizevolume;
    }

    //
    // CD Audio will be handled by the CDFS file system.
    //
    if( pPartInfo->fAudio )
    {
        goto exit_udf_recognizevolume;
    }

    if( pPartInfo->NumberOfTracks == 0 )
    {
        //
        // Is this necessary here or is there already a check for it?
        //
        goto exit_udf_recognizevolume;
    }

    //
    // If we are trying to mount this "partition" and it's not an audio
    // partition then the last track better not be zero or we have issues.
    //
    ASSERT( pPartInfo->LastValidTrackInPartition != 0 );

    //
    // Now check for a valid UDF VRS starting at "the first sector starting
    // after byte 32767" in the session.  We're reading in max of 16 sectors
    // here.
    //
    pVRSBuffer = new (UDF_TAG_BYTE_BUFFER) BYTE[dwBuffSize];
    if( !pVRSBuffer )
    {
        lResult = ERROR_NOT_ENOUGH_MEMORY;
        goto exit_udf_recognizevolume;
    }

    for( USHORT Track = pPartInfo->NumberOfTracks;
         (lResult != ERROR_SUCCESS) && Track;
         Track-- )
    {
        DWORD dwFirstSector = 0;

        pTrackInfo = &pPartInfo->TrackInformation[Track - 1];

        dwFirstSector = (pTrackInfo->TrackStartAddress[0] << 24) +
                        (pTrackInfo->TrackStartAddress[1] << 16) +
                        (pTrackInfo->TrackStartAddress[2] << 8) +
                        (pTrackInfo->TrackStartAddress[3]);

        CdRead.StartAddr.Mode = CDROM_ADDR_LBA;

        CdRead.StartAddr.Address.lba = 32 * 1024 / dwSectorSize;
        CdRead.StartAddr.Address.lba += dwFirstSector;

        CdRead.sgcount = 1;
        CdRead.TransferLength = MAX_READ_SIZE / dwSectorSize;
        CdRead.sglist[0].sb_buf = pVRSBuffer;
        CdRead.sglist[0].sb_len = dwBuffSize;

        if( !FSDMGR_DiskIoControl( (DWORD)hDisk,
                                   IOCTL_CDROM_READ_SG,
                                   &CdRead,
                                   sizeof(CdRead),
                                   NULL,
                                   0,
                                   &dwBytesReturned,
                                   NULL ) )
        {
            lResult = GetLastError();
            goto exit_udf_recognizevolume;
        }

        //
        // Note that we're re-using "dwRead" here to tell us how many
        // sectors we've read through the buffer.  We'll go through the
        // buffer as long as we haven't found the required descriptors
        // for UDF, or until we've hit the end of the buffer.
        //
        // Note that 16K should be enough for any VRS, but there's not really
        // a requirement saying it will be less than 16K.  Or, if a device
        // comes out with large sector sizes, this might not be enough.
        //
        dwRead = 0;
        while( (!fFoundBEA || !fFoundTEA || !fFoundNSR) &&
               (dwRead < dwBuffSize / dwSectorSize) )
        {
            PVSD_GENERIC pVRSSector = (PVSD_GENERIC)(&pVRSBuffer[dwRead * dwSectorSize]);

            dwRead += 1;

            if( pVRSSector->Type == 0 )
            {
                if( strncmp( (char*)pVRSSector->Ident, "TEA01", 5 ) == 0 )
                {
                    fFoundTEA = TRUE;
                    continue;
                }

                if( strncmp( (char*)pVRSSector->Ident, "BEA01", 5 ) == 0 )
                {
                    fFoundBEA = TRUE;
                    continue;
                }

                if( strncmp( (char*)pVRSSector->Ident, "NSR02", 5 ) == 0 )
                {
                    fFoundNSR = TRUE;
                    continue;
                }

                if( strncmp( (char*)pVRSSector->Ident, "NSR03", 5 ) == 0 )
                {
                    fFoundNSR = TRUE;
                    continue;
                }
            }
        }

        if( fFoundBEA && fFoundTEA && fFoundNSR )
        {
            lResult = ERROR_SUCCESS;
        }
    }

exit_udf_recognizevolume:
    if( pVRSBuffer )
    {
        delete [] pVRSBuffer;
    }

    if( pPartInfo )
    {
        delete [] pPartInfo;
    }

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// UDF_RecognizeVolumeHD
//
// This is exported as the detector function for UDF to identify this media as
// being UDF mountable or not.
//
LRESULT UDF_RecognizeVolumeHD( HANDLE hDisk,
                               const BYTE* pBootSector,
                               DWORD dwBootSectorSize )
{
    LRESULT lResult = ERROR_SUCCESS;
    DWORD dwBytesReturned = 0;

    //
    // These are used when actually reading the VRS from the disc.
    //
    const DWORD dwBuffSize = MAX_READ_SIZE;
    BOOL fFoundBEA = FALSE;
    BOOL fFoundTEA = FALSE;
    BOOL fFoundNSR = FALSE;
    BYTE* pVRSBuffer = NULL;
    DWORD dwRead = 0;
    DWORD dwSectorSize = 512;

    FSD_DISK_INFO DiskInfo = { 0 };
    DWORD dwFirstSector = 0;
    FSD_SCATTER_GATHER_INFO ReadInfo;
    FSD_BUFFER_INFO BufferInfo;
    FSD_SCATTER_GATHER_RESULTS FsdResults = { 0 };

    //
    // We need to get the sector size, read in the vrs, and check for the
    // appropriate descriptors here.
    //
    lResult = ::FSDMGR_GetDiskInfo( (DWORD)hDisk, &DiskInfo );
    if( lResult != ERROR_SUCCESS )
    {
        goto exit_udf_recognizevolumehd;
    }

    dwSectorSize = DiskInfo.cbSector;
    if( !dwSectorSize )
    {
        lResult = ERROR_INVALID_BLOCK_LENGTH;
        goto exit_udf_recognizevolumehd;
    }

    //
    // Now check for a valid UDF VRS starting at "the first sector starting
    // after byte 32767" in the session.  We're reading in max of 16 sectors
    // here.
    //
    pVRSBuffer = new (UDF_TAG_BYTE_BUFFER) BYTE[dwBuffSize];
    if( !pVRSBuffer )
    {
        lResult = ERROR_NOT_ENOUGH_MEMORY;
        goto exit_udf_recognizevolumehd;
    }

    ZeroMemory( &ReadInfo, sizeof(ReadInfo) );
    ReadInfo.cfbi = 1;
    ReadInfo.cSectors = MAX_READ_SIZE / dwSectorSize;
    ReadInfo.dwSector = 32 * 1024 / dwSectorSize;
    ReadInfo.idDsk = (DWORD)hDisk;
    ReadInfo.pfbi = &BufferInfo;

    BufferInfo.cbBuffer = MAX_READ_SIZE;
    BufferInfo.pBuffer = pVRSBuffer;

    lResult = FSDMGR_ReadDiskEx( &ReadInfo, &FsdResults );
    if( lResult != ERROR_SUCCESS )
    {
        goto exit_udf_recognizevolumehd;
    }

    if( FsdResults.cSectorsTransferred != (MAX_READ_SIZE / dwSectorSize) )
    {
        ASSERT( FALSE );
        lResult = ERROR_READ_FAULT;
        goto exit_udf_recognizevolumehd;
    }

    //
    // Note that we're re-using "dwRead" here to tell us how many
    // sectors we've read through the buffer.  We'll go through the
    // buffer as long as we haven't found the required descriptors
    // for UDF, or until we've hit the end of the buffer.
    //
    // Note that 16K should be enough for any VRS, but there's not really
    // a requirement saying it will be less than 16K.  Or, if a device
    // comes out with large sector sizes, this might not be enough.
    //
    dwRead = 0;
    while( (!fFoundBEA || !fFoundTEA || !fFoundNSR) &&
           (dwRead < dwBuffSize / max(dwSectorSize,sizeof(VSD_GENERIC))) )
    {
        PVSD_GENERIC pVRSSector = (PVSD_GENERIC)(&pVRSBuffer[dwRead * max(dwSectorSize,sizeof(VSD_GENERIC))]);

        dwRead += 1;

        if( pVRSSector->Type == 0 )
        {
            if( strncmp( (char*)pVRSSector->Ident, "TEA01", 5 ) == 0 )
            {
                fFoundTEA = TRUE;
                continue;
            }

            if( strncmp( (char*)pVRSSector->Ident, "BEA01", 5 ) == 0 )
            {
                fFoundBEA = TRUE;
                continue;
            }

            if( strncmp( (char*)pVRSSector->Ident, "NSR02", 5 ) == 0 )
            {
                fFoundNSR = TRUE;
                continue;
            }

            if( strncmp( (char*)pVRSSector->Ident, "NSR03", 5 ) == 0 )
            {
                fFoundNSR = TRUE;
                continue;
            }
        }
    }

    if( fFoundBEA && fFoundTEA && fFoundNSR )
    {
        lResult = ERROR_SUCCESS;
    }
    else
    {
        lResult = ERROR_BAD_FORMAT;
    }

exit_udf_recognizevolumehd:
    if( pVRSBuffer )
    {
        delete [] pVRSBuffer;
    }

    return lResult;
}


