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

#include "udf.h"

// -----------------------------------------------------------------------------
// CUDFMedia
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CUDFMedia::CUDFMedia()
: m_pVolume( NULL )
{
}

// /////////////////////////////////////////////////////////////////////////////
// Intitialize
//
BOOL CUDFMedia::Initialize( CVolume* pVolume )
{
    ASSERT( pVolume != NULL );

    m_pVolume = pVolume;

    return TRUE;
}

// -----------------------------------------------------------------------------
// CReadOnlyMedia
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// ReadRange
//
LRESULT CReadOnlyMedia::ReadRange( DWORD dwSector,
                                   DWORD dwSectorOffset,
                                   DWORD dwSize,
                                   BYTE* pBuffer,
                                   DWORD* pdwBytesRead )
{
    ASSERT(m_pVolume != NULL);

    LRESULT lResult = ERROR_SUCCESS;
    BYTE* pDummyBuffer = NULL;
    BYTE RequestBuffer[sizeof(CDROM_READ) + sizeof(SGX_BUF) * 2];
    PCDROM_READ pReadInfo = (PCDROM_READ)RequestBuffer;

    DWORD dwSectorSize = m_pVolume->GetSectorSize();
    DWORD dwPreBytes = dwSectorOffset;
    DWORD dwPostBytes = ((dwPreBytes + dwSize) & (dwSectorSize - 1))
                        ? dwSectorSize - ((dwPreBytes + dwSize) & (dwSectorSize - 1))
                        : 0;
    DWORD dwTotalSize = dwPreBytes + dwPostBytes + dwSize;
    DWORD dwBytesRead = 0;

    //
    // If after aligning the buffer it doesn't fit into a DWORD, then return an
    // error.
    //
    if( (ULONG_MAX - dwSize) < (dwPreBytes + dwPostBytes) )
    {
        return ERROR_INVALID_PARAMETER;
    }

    if( !pBuffer || !pdwBytesRead )
    {
        return ERROR_INVALID_PARAMETER;
    }

    *pdwBytesRead = 0;

    ZeroMemory( RequestBuffer, sizeof(RequestBuffer) );

    pReadInfo->bRawMode = FALSE;
    pReadInfo->TransferLength = dwTotalSize >> m_pVolume->GetShiftCount();
    pReadInfo->TrackMode = YellowMode2;
    pReadInfo->StartAddr.Mode = CDROM_ADDR_LBA;
    pReadInfo->StartAddr.Address.lba = dwSector;

    if( dwPreBytes || dwPostBytes )
    {
        pDummyBuffer = new (UDF_TAG_BYTE_BUFFER) BYTE[ dwSectorSize ];
        if( !pDummyBuffer )
        {
            return ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    if( dwPreBytes )
    {
        pReadInfo->sglist[pReadInfo->sgcount].sb_buf = pDummyBuffer;
        pReadInfo->sglist[pReadInfo->sgcount].sb_len = dwPreBytes;
        pReadInfo->sgcount += 1;
    }

    pReadInfo->sglist[pReadInfo->sgcount].sb_buf = pBuffer;
    pReadInfo->sglist[pReadInfo->sgcount].sb_len = dwSize;
    pReadInfo->sgcount += 1;

    if( dwPostBytes )
    {
        pReadInfo->sglist[pReadInfo->sgcount].sb_buf = pDummyBuffer;
        pReadInfo->sglist[pReadInfo->sgcount].sb_len = dwPostBytes;
        pReadInfo->sgcount += 1;
    }

    if( !m_pVolume->UDFSDeviceIoControl( IOCTL_CDROM_READ_SG,
                                         RequestBuffer,
                                         sizeof(CDROM_READ) + sizeof(SGX_BUF) * (pReadInfo->sgcount - 1),
                                         NULL,
                                         0,
                                         &dwBytesRead,
                                         NULL ) )
    {
        DEBUGMSG(ZONE_IO|ZONE_ERROR,(TEXT("UDFS!ReadRange Unable to read from CD at sector 0x%x"), dwSector));
        lResult = GetLastError();
    }

    if( dwBytesRead < dwPreBytes + dwSize )
    {
        ASSERT( FALSE );
        //
        // TODO::Is there a better message for this?
        //
        lResult = ERROR_READ_FAULT;
    }

    *pdwBytesRead = dwSize;

    if( pDummyBuffer )
    {
        delete [] pDummyBuffer;
        pDummyBuffer = NULL;
    }

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// WriteRange
//
// Easiest function ever to implement.  ROM media (or closed R) cannot be
// written to.
//
LRESULT CReadOnlyMedia::WriteRange( DWORD dwSector,
                                    DWORD dwSectorOffset,
                                    DWORD dwSize,
                                    BYTE* pBuffer,
                                    DWORD* pdwBytesWritten )
{
    ASSERT(m_pVolume != NULL);
    LRESULT lResult = ERROR_WRITE_PROTECT;

    return lResult;
}

// -----------------------------------------------------------------------------
// CWriteOnceMedia
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CWriteOnceMedia::CWriteOnceMedia()
: CUDFMedia(),
  m_fDiscClosed( FALSE )
{
}

// /////////////////////////////////////////////////////////////////////////////
// Initialize
//
// There could be write once media without a VAT starting in UDF 2.60.  All
// mastered incremental media should be reported as ReadOnlyMedia.  But
// packet written incremental media that has been closed will still need to
// access through the VAT.
//
BOOL CWriteOnceMedia::Initialize( CVolume* pVolume )
{
    BOOL fResult = CUDFMedia::Initialize( pVolume );

    DWORD dwBytesReturned = 0;
    DISC_INFORMATION DiscInfo = { 0 };
    DISC_INFO di = DI_SCSI_INFO;

    if( !fResult )
    {
        goto exit_initialize;
    }

    //
    // Gather general information about the disc.
    //
    fResult = pVolume->UDFSDeviceIoControl( IOCTL_CDROM_DISC_INFO,
                                            &di,
                                            sizeof(DISC_INFO),
                                            &DiscInfo,
                                            sizeof( DiscInfo ),
                                            &dwBytesReturned,
                                            NULL );

    if( !fResult )
    {
        goto exit_initialize;
    }

    if( DiscInfo.DiscStatus == DISK_STATUS_COMPLETE )
    {
        //
        // If the disk is closed then there will never be a next writable
        // address but we will need to be able to reference the VAT.
        //
        m_fDiscClosed = TRUE;
    }

exit_initialize:

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ReadRange
//
//
LRESULT CWriteOnceMedia::ReadRange( DWORD dwSector,
                                    DWORD dwSectorOffset,
                                    DWORD dwSize,
                                    BYTE* pBuffer,
                                    DWORD* pdwBytesRead )
{
    ASSERT(m_pVolume != NULL);
    LRESULT lResult = ERROR_SUCCESS;

    BYTE* pDummyBuffer = NULL;
    DWORD dwDummyBufferSize = 0;
    BYTE RequestBuffer[sizeof(CDROM_READ) + sizeof(SGX_BUF) * 2];
    PCDROM_READ pReadInfo = (PCDROM_READ)RequestBuffer;

    DWORD dwSectorSize = m_pVolume->GetSectorSize();
    DWORD dwShiftCount = m_pVolume->GetShiftCount();

    DWORD dwFirstSector = 0;
    DWORD dwNumSectors = 0;
    DWORD dwPreBytes = 0;
    DWORD dwPostBytes = 0;
    DWORD dwBytesRead = 0;


    if( dwSectorOffset >= m_pVolume->GetSectorSize() )
    {
        lResult = ERROR_INVALID_PARAMETER;
        goto exit_readrange;
    }

    if( FAILED(CeULongAdd3( dwSize,
                            dwSectorOffset,
                            dwSectorSize - 1,
                            &dwNumSectors )) )
    {
        lResult = ERROR_INVALID_PARAMETER;
        goto exit_readrange;
    }

    dwFirstSector = dwSector;
    dwNumSectors = dwNumSectors >> dwShiftCount;

    dwPreBytes = dwSectorOffset;
    dwPostBytes = (dwNumSectors << dwShiftCount) - dwSize - dwPreBytes;

    if( !pBuffer || !pdwBytesRead )
    {
        lResult = ERROR_INVALID_PARAMETER;
        goto exit_readrange;
    }

    *pdwBytesRead = 0;

    ZeroMemory( RequestBuffer, sizeof(RequestBuffer) );

    pReadInfo->bRawMode = FALSE;
    pReadInfo->TransferLength = dwNumSectors;
    pReadInfo->TrackMode = YellowMode2;
    pReadInfo->StartAddr.Mode = CDROM_ADDR_LBA;
    pReadInfo->StartAddr.Address.lba = dwFirstSector;

    //
    // Allocate a buffer big enough to hold either the pre or post bytes.
    //
    if( dwPreBytes || dwPostBytes )
    {
        dwDummyBufferSize = max(dwPreBytes,dwPostBytes);

        pDummyBuffer = new (UDF_TAG_BYTE_BUFFER) BYTE[dwDummyBufferSize];
        if( !pDummyBuffer )
        {
            lResult = ERROR_NOT_ENOUGH_MEMORY ;
            goto exit_readrange;
        }
    }

    //
    // Setup the scatter/gather structures.
    //
    if( dwPreBytes )
    {
        pReadInfo->sglist[pReadInfo->sgcount].sb_buf = pDummyBuffer;
        pReadInfo->sglist[pReadInfo->sgcount].sb_len = dwPreBytes;
        pReadInfo->sgcount += 1;
    }

    pReadInfo->sglist[pReadInfo->sgcount].sb_buf = pBuffer;
    pReadInfo->sglist[pReadInfo->sgcount].sb_len = dwSize;
    pReadInfo->sgcount += 1;

    if( dwPostBytes )
    {
        pReadInfo->sglist[pReadInfo->sgcount].sb_buf = pDummyBuffer;
        pReadInfo->sglist[pReadInfo->sgcount].sb_len = dwPostBytes;
        pReadInfo->sgcount += 1;
    }

    if( !m_pVolume->UDFSDeviceIoControl( IOCTL_CDROM_READ_SG,
                                         RequestBuffer,
                                         sizeof(CDROM_READ) + sizeof(SGX_BUF) * (pReadInfo->sgcount - 1),
                                         NULL,
                                         0,
                                         &dwBytesRead,
                                         NULL ) )
    {
        DEBUGMSG(ZONE_IO|ZONE_ERROR,(TEXT("UDFS!ReadRange Unable to read from CD at sector 0x%x"), dwSector));
        lResult = GetLastError();
        goto exit_readrange;
    }

    if( dwBytesRead < dwPreBytes + dwSize )
    {
        ASSERT( FALSE );
        //
        // TODO::Is there a better message for this?
        //
        lResult = ERROR_READ_FAULT;
    }

    *pdwBytesRead = dwSize;

exit_readrange:
    if( pDummyBuffer )
    {
        delete [] pDummyBuffer;
    }

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// WriteRange
//
LRESULT CWriteOnceMedia::WriteRange( DWORD dwSector,
                                     DWORD dwSectorOffset,
                                     DWORD dwSize,
                                     BYTE* pBuffer,
                                     DWORD* pdwBytesWritten )
{
    ASSERT(m_pVolume != NULL);
    LRESULT lResult = ERROR_CALL_NOT_IMPLEMENTED;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetMinBlocksPerRead
//
DWORD CWriteOnceMedia::GetMinBlocksPerRead() const
{
    //
    // Technically you can read in single sectors (at least on DVD), but that
    // won't help much becuase the device will actually read the complete ECC
    // block, de-interleave the sectors, and then return the data for that one
    // sector.  It is better just to read the entire block.
    //
    ASSERT(m_pVolume != NULL);
    return m_pVolume->GetMediaType() & MED_FLAG_DVD ? 16 : 32;
}

// /////////////////////////////////////////////////////////////////////////////
// GetMinBlocksPerWrite
//
DWORD CWriteOnceMedia::GetMinBlocksPerWrite() const
{
    ASSERT(m_pVolume != NULL);
    return m_pVolume->GetMediaType() & MED_FLAG_DVD ? 16 : 32;
}

// -----------------------------------------------------------------------------
// CRewritableMedia
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// ReadRange
//
LRESULT CRewritableMedia::ReadRange( DWORD dwSector,
                                     DWORD dwSectorOffset,
                                     DWORD dwSize,
                                     BYTE* pBuffer,
                                     DWORD* pdwBytesRead )
{
    ASSERT(m_pVolume != NULL);
    LRESULT lResult = ERROR_SUCCESS;

    BYTE* pDummyBuffer = NULL;
    DWORD dwDummyBufferSize = 0;
    BYTE RequestBuffer[sizeof(CDROM_READ) + sizeof(SGX_BUF) * 2];
    PCDROM_READ pReadInfo = (PCDROM_READ)RequestBuffer;

    DWORD dwSectorSize = m_pVolume->GetSectorSize();
    DWORD dwShiftCount = m_pVolume->GetShiftCount();

    DWORD dwFirstSector = 0;
    DWORD dwNumSectors = 0;
    DWORD dwPreBytes = 0;
    DWORD dwPostBytes = 0;
    DWORD dwBytesRead = 0;

    if( dwSectorOffset >= m_pVolume->GetSectorSize() )
    {
        lResult = ERROR_INVALID_PARAMETER;
        goto exit_readrange;
    }

    if( FAILED(CeULongAdd3( dwSize,
                            dwSectorOffset,
                            dwSectorSize - 1,
                            &dwNumSectors )) )
    {
        lResult = ERROR_INVALID_PARAMETER;
        goto exit_readrange;
    }


    dwFirstSector = dwSector;
    dwNumSectors = dwNumSectors >> dwShiftCount;

    dwPreBytes = dwSectorOffset;
    dwPostBytes = (dwNumSectors << dwShiftCount) - dwSize - dwPreBytes;

    if( !pBuffer || !pdwBytesRead )
    {
        lResult = ERROR_INVALID_PARAMETER;
        goto exit_readrange;
    }

    *pdwBytesRead = 0;

    ZeroMemory( RequestBuffer, sizeof(RequestBuffer) );

    pReadInfo->bRawMode = FALSE;
    pReadInfo->TransferLength = dwNumSectors;
    pReadInfo->TrackMode = YellowMode2;
    pReadInfo->StartAddr.Mode = CDROM_ADDR_LBA;
    pReadInfo->StartAddr.Address.lba = dwFirstSector;

    //
    // Allocate a buffer big enough to hold either the pre or post bytes.
    //
    if( dwPreBytes || dwPostBytes )
    {
        dwDummyBufferSize = max(dwPreBytes,dwPostBytes);

        pDummyBuffer = new (UDF_TAG_BYTE_BUFFER) BYTE[dwDummyBufferSize];
        if( !pDummyBuffer )
        {
            lResult = ERROR_NOT_ENOUGH_MEMORY;
            goto exit_readrange;
        }
    }

    //
    // Setup the scatter/gather structures.
    //
    if( dwPreBytes )
    {
        pReadInfo->sglist[pReadInfo->sgcount].sb_buf = pDummyBuffer;
        pReadInfo->sglist[pReadInfo->sgcount].sb_len = dwPreBytes;
        pReadInfo->sgcount += 1;
    }

    pReadInfo->sglist[pReadInfo->sgcount].sb_buf = pBuffer;
    pReadInfo->sglist[pReadInfo->sgcount].sb_len = dwSize;
    pReadInfo->sgcount += 1;

    if( dwPostBytes )
    {
        pReadInfo->sglist[pReadInfo->sgcount].sb_buf = pDummyBuffer;
        pReadInfo->sglist[pReadInfo->sgcount].sb_len = dwPostBytes;
        pReadInfo->sgcount += 1;
    }

    if( !m_pVolume->UDFSDeviceIoControl( IOCTL_CDROM_READ_SG,
                                         RequestBuffer,
                                         sizeof(CDROM_READ) + sizeof(SGX_BUF) * (pReadInfo->sgcount - 1),
                                         NULL,
                                         0,
                                         &dwBytesRead,
                                         NULL ) )
    {
        DEBUGMSG(ZONE_IO|ZONE_ERROR,(TEXT("UDFS!ReadRange Unable to read from CD at sector 0x%x"), dwSector));
        lResult = GetLastError();
        goto exit_readrange;
    }

    if( dwBytesRead < dwPreBytes + dwSize )
    {
        ASSERT( FALSE );
        //
        // TODO::Is there a better message for this?
        //
        lResult = ERROR_READ_FAULT;
    }

    *pdwBytesRead = dwSize;

exit_readrange:
    if( pDummyBuffer )
    {
        delete [] pDummyBuffer;
    }

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// WriteRange
//
LRESULT CRewritableMedia::WriteRange( DWORD dwSector,
                                      DWORD dwSectorOffset,
                                      DWORD dwSize,
                                      BYTE* pBuffer,
                                      DWORD* pdwBytesWritten )
{
    ASSERT(m_pVolume != NULL);
    LRESULT lResult = ERROR_CALL_NOT_IMPLEMENTED;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetMinBlocksPerRead
//
DWORD CRewritableMedia::GetMinBlocksPerRead() const
{
    ASSERT(m_pVolume != NULL);
    return m_pVolume->GetMediaType() & MED_FLAG_DVD ? 16 : 32;
}

// /////////////////////////////////////////////////////////////////////////////
// GetMinBlocksPerWrite
//
DWORD CRewritableMedia::GetMinBlocksPerWrite() const
{
    ASSERT(m_pVolume != NULL);
    return m_pVolume->GetMediaType() & MED_FLAG_DVD ? 16 : 32;
}

// -----------------------------------------------------------------------------
// CHardDiskMedia
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// ReadRange
//
LRESULT CHardDiskMedia::ReadRange( DWORD dwSector,
                                   DWORD dwSectorOffset,
                                   DWORD dwSize,
                                   BYTE* pBuffer,
                                   DWORD* pdwBytesRead )
{
    ASSERT(m_pVolume != NULL);
    LRESULT lResult = ERROR_SUCCESS;

    BYTE* pDummyBuffer = NULL;
    DWORD dwDummyBufferSize = 0;
    BYTE RequestBuffer[sizeof(FSD_SCATTER_GATHER_INFO) + sizeof(FSD_BUFFER_INFO) * 3];
    FSD_SCATTER_GATHER_INFO* pReadInfo = (FSD_SCATTER_GATHER_INFO*)RequestBuffer;
    FSD_BUFFER_INFO* pBuffers = (FSD_BUFFER_INFO*)(pReadInfo + 1);
    FSD_SCATTER_GATHER_RESULTS FsdResults = { 0 };

    DWORD dwSectorSize = m_pVolume->GetSectorSize();
    DWORD dwShiftCount = m_pVolume->GetShiftCount();

    DWORD dwFirstSector = 0;
    DWORD dwNumSectors = 0;
    DWORD dwPreBytes = 0;
    DWORD dwPostBytes = 0;

    if( dwSectorOffset >= m_pVolume->GetSectorSize() )
    {
        lResult = ERROR_INVALID_PARAMETER;
        goto exit_readrange;
    }

    if( FAILED(CeULongAdd3( dwSize,
                            dwSectorOffset,
                            dwSectorSize - 1,
                            &dwNumSectors )) )
    {
        lResult = ERROR_INVALID_PARAMETER;
        goto exit_readrange;
    }

    dwFirstSector = dwSector;
    dwNumSectors = dwNumSectors >> dwShiftCount;

    dwPreBytes = dwSectorOffset;
    dwPostBytes = (dwNumSectors << dwShiftCount) - dwSize - dwPreBytes;

    if( !pBuffer || !pdwBytesRead )
    {
        lResult = ERROR_INVALID_PARAMETER;
        goto exit_readrange;
    }

    *pdwBytesRead = 0;

    ZeroMemory( RequestBuffer, sizeof(RequestBuffer) );

    pReadInfo->idDsk = m_pVolume->GetDisk();
    pReadInfo->cSectors = dwNumSectors;
    pReadInfo->dwSector = dwFirstSector;
    pReadInfo->pfbi = pBuffers;

    //
    // Allocate a buffer big enough to hold either the pre or post bytes.
    //
    if( dwPreBytes || dwPostBytes )
    {
        dwDummyBufferSize = max(dwPreBytes,dwPostBytes);

        pDummyBuffer = new (UDF_TAG_BYTE_BUFFER) BYTE[dwDummyBufferSize];
        if( !pDummyBuffer )
        {
            lResult = ERROR_NOT_ENOUGH_MEMORY;
            goto exit_readrange;
        }
    }

    //
    // Setup the scatter/gather structures.
    //
    if( dwPreBytes )
    {
        pBuffers[pReadInfo->cfbi].pBuffer = pDummyBuffer;
        pBuffers[pReadInfo->cfbi].cbBuffer = dwPreBytes;
        pReadInfo->cfbi += 1;
    }

    pBuffers[pReadInfo->cfbi].pBuffer = pBuffer;
    pBuffers[pReadInfo->cfbi].cbBuffer = dwSize;
    pReadInfo->cfbi += 1;

    if( dwPostBytes )
    {
        pBuffers[pReadInfo->cfbi].pBuffer = pDummyBuffer;
        pBuffers[pReadInfo->cfbi].cbBuffer = dwPostBytes;
        pReadInfo->cfbi += 1;
    }

    lResult = FSDMGR_ReadDiskEx( pReadInfo, &FsdResults );
    if( lResult != ERROR_SUCCESS )
    {
        DEBUGMSG(ZONE_IO|ZONE_ERROR,(TEXT("UDFS!ReadRange Unable to read from CD at sector 0x%x"), dwSector));
        goto exit_readrange;
    }

    if( FsdResults.cSectorsTransferred != dwNumSectors )
    {
        ASSERT( FALSE );
        //
        // TODO::Is there a better message for this?
        //
        lResult = ERROR_READ_FAULT;
    }
    else
    {
        *pdwBytesRead = dwSize;
    }

exit_readrange:
    if( pDummyBuffer )
    {
        delete [] pDummyBuffer;
    }

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// WriteRange
//
LRESULT CHardDiskMedia::WriteRange( DWORD dwSector,
                                    DWORD dwSectorOffset,
                                    DWORD dwSize,
                                    BYTE* pBuffer,
                                    DWORD* pdwBytesWritten )
{
    ASSERT(m_pVolume != NULL);
    LRESULT lResult = ERROR_CALL_NOT_IMPLEMENTED;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetMinBlocksPerRead
//
DWORD CHardDiskMedia::GetMinBlocksPerRead() const
{
    return 1;
}

// /////////////////////////////////////////////////////////////////////////////
// GetMinBlocksPerWrite
//
DWORD CHardDiskMedia::GetMinBlocksPerWrite() const
{
    return 1;
}

