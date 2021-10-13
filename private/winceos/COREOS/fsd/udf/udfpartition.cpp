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
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "udf.h"

const Uint16 INVALID_REFERENCE = 0xFFFF;

// -----------------------------------------------------------------------------
// CPhysicalPartition
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CPhysicalPartition::CPhysicalPartition()
: m_pVolume( NULL ),
  m_ReferenceNumber( INVALID_REFERENCE )
{
    InitializeCriticalSection( &m_cs );
}

// /////////////////////////////////////////////////////////////////////////////
// Destructor
//
CPhysicalPartition::~CPhysicalPartition()
{
    DeleteCriticalSection( &m_cs );
}

// /////////////////////////////////////////////////////////////////////////////
// Initialize
//
BOOL CPhysicalPartition::Initialize( CVolume* pVolume, Uint16 ReferenceNumber )
{
    BOOL fResult = TRUE;

    ASSERT( pVolume != NULL );

    m_pVolume = pVolume;
    m_ReferenceNumber = ReferenceNumber;

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// Read
//
LRESULT CPhysicalPartition::Read( NSRLBA lba,
                                  Uint32 Offset,
                                  Uint32 Size,
                                  BYTE* pBuffer )
{
    ASSERT( pBuffer != NULL );
    ASSERT( lba.Partition == m_ReferenceNumber );

    LRESULT lResult = ERROR_SUCCESS;
    DWORD dwBytesRead = 0;
    Uint32 Block = m_pVolume->GetPartitionStart( m_ReferenceNumber );

    Block += lba.Lbn;

    lResult = m_pVolume->GetMedia()->ReadRange( Block,
                                                Offset,
                                                Size,
                                                pBuffer,
                                                &dwBytesRead );

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// Write
//
LRESULT CPhysicalPartition::Write( NSRLBA lba,
                                   Uint32 Offset,
                                   Uint32 Size,
                                   BYTE* pBuffer )
{
    LRESULT lResult = ERROR_CALL_NOT_IMPLEMENTED;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetPhysicalBlock
//
Uint32 CPhysicalPartition::GetPhysicalBlock( NSRLBA lba )
{
    Uint32 Block = 0;

    if( lba.Partition == m_ReferenceNumber )
    {
        Block = m_pVolume->GetPartitionStart( m_ReferenceNumber ) + lba.Lbn;
    }

    return Block;
}

// -----------------------------------------------------------------------------
// CSparingPartition
// -----------------------------------------------------------------------------

//
// TODO::Currently we just allocate memory for the entire sparing area and read
// the whole thing in.  This could potentially be 512K of memory even though
// only a small part of that may be in use.  This was a very simple approach
// that will work, but a more memory efficient approach would be better.
//

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CSparingPartition::CSparingPartition()
: CPhysicalPartition(),
  m_PacketLength( 0 ),
  m_NumberOfTables( 0 ),
  m_TableSize( 0 ),
  m_NumberSparedEntries( 0 ),
  m_NumberOfDeadEntries( 0 ),
  m_TotalNumberOfEntries( 0 ),
  m_pEntries( NULL )
{
    ZeroMemory( m_TableStart, sizeof( m_TableStart ) );
}

// /////////////////////////////////////////////////////////////////////////////
// Destructor
//
CSparingPartition::~CSparingPartition()
{
    if( m_pEntries )
    {
        delete [] m_pEntries;
    }
}

// /////////////////////////////////////////////////////////////////////////////
// InitializeSparingInfo
//
// The sparing table is limited in size by the Reallocation Table Length field
// in the sparing table header.  This is a 16 bit value, meaning there can be
// 64K of map entries, which are 8 bytes each.  This means a maximum of 512K
// bytes.
//
// Also, these entries must be sorted in ascending order by the original
// location field in the entry.
//
BOOL CSparingPartition::InitializeSparingInfo( PPARTMAP_SPARABLE pSparingMap )
{
    BOOL fResult = TRUE;
    ULONGLONG ullOffset = 0;
    BYTE* pBuffer = NULL;
    BYTE* pReadBuffer = NULL;
    DWORD dwReadSize = 0;
    LRESULT lResult = ERROR_SUCCESS;
    Uint16 EntriesRead = 0;

    //
    // TODO::Read in the registry value for the maximum sparing size.
    //

    //
    // TODO::Validate the UDF version supports sparing tables.  Should we check
    // the identifier?
    //

    m_PacketLength = pSparingMap->PacketLength;
    m_NumberOfTables = pSparingMap->NumSparingTables;
    m_TableSize = pSparingMap->TableSize;

    if( m_TableSize > USHRT_MAX * sizeof(SPARE_ENTRY) +
                      sizeof(SPARING_TABLE_HEADER) )
    {
        DEBUGMSG( ZONE_ERROR,(TEXT("UDFS!InitializeSparingInfo found an invalid table size: %d\n"),m_TableSize));
        SetLastError( ERROR_DISK_CORRUPT );
        goto exit_initializesparinginfo;
    }

    //
    // Check that the packet length matches what is expected for this media, and
    // that the media type in question should have sparing tables on it.
    //
    if( (m_pVolume->GetMediaType() & MED_TYPE_CD_RW) == MED_TYPE_CD_RW )
    {
        if( m_PacketLength & (CDRW_PACKET_LENGTH - 1) )
        {
            DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!InitializeSparingInfo an invalid packet length: %d.\n"), m_PacketLength));
            SetLastError( ERROR_DISK_CORRUPT );
            goto exit_initializesparinginfo;
        }
    }
    else if( (m_pVolume->GetMediaType() & MED_TYPE_DVD_MINUS_RW) == MED_TYPE_DVD_MINUS_RW )
    {
        if( m_PacketLength & (DVDRW_PACKET_LENGTH - 1) )
        {
            DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!InitializeSparingInfo an invalid packet length: %d.\n"), m_PacketLength));
            SetLastError( ERROR_DISK_CORRUPT );
            goto exit_initializesparinginfo;
        }
    }
    else
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!InitializeSparingInfo Media type should not use sparing tables.\n")));
        ASSERT( 0 ); // Unexpected, but maybe we can mount it anyway?
    }

    if( m_NumberOfTables > 4 )
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!InitializeSparingInfo found more than 4 sparing tables.\n")));
        SetLastError( ERROR_DISK_CORRUPT );
        goto exit_initializesparinginfo;
    }

    dwReadSize = m_pVolume->GetSectorSize() *
                 m_pVolume->GetMedia()->GetMinBlocksPerRead();

    pBuffer = new (UDF_TAG_BYTE_BUFFER) BYTE[dwReadSize];
    if( !pBuffer )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        goto exit_initializesparinginfo;
    }

    //
    // TODO::Validate that all the sparing tables match.
    //

    DWORD BytesToRead = m_TableSize;
    m_TableStart[0] = pSparingMap->TableLocation[0];

    //
    // Now that we have the locations of the sparing tables, we must read them
    // and confirm they are valid.  We will eventually need to write all of the
    // tables out if we change any data, but they must be identical by
    // definition, so we will only store the data from one.
    //
    ullOffset = m_TableStart[0] * m_pVolume->GetSectorSize();
    pReadBuffer = pBuffer;

    while( BytesToRead > 0 )
    {
        DWORD dwBytesRead = 0;
        DWORD dwBlockOffset = 0;

        if( BytesToRead < dwReadSize )
        {
            dwReadSize = BytesToRead;
        }

        lResult = m_pVolume->GetMedia()->ReadRange( (DWORD)(ullOffset >> m_pVolume->GetShiftCount()),
                                                    (DWORD)(ullOffset % m_pVolume->GetSectorSize()),
                                                    dwReadSize,
                                                    pReadBuffer,
                                                    &dwBytesRead );
        if( lResult != ERROR_SUCCESS )
        {
            SetLastError( lResult );
            goto exit_initializesparinginfo;
        }

        if( BytesToRead == m_TableSize )
        {
            //
            // This is the beginning of the sparing table, so we have
            // to read in the header first, which just happens to be a
            // multiple of the spare entry size.
            //
            PSPARING_TABLE_HEADER pHeader = (PSPARING_TABLE_HEADER)pBuffer;

            if( sizeof(SPARING_TABLE_HEADER) +
                pHeader->TableEntries * sizeof(SPARE_ENTRY) > m_TableSize )
            {
                DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!InitializeSparingInfo found conflicting sparing table sizes.\n")));
                SetLastError( ERROR_DISK_CORRUPT );
                goto exit_initializesparinginfo;
            }

            m_TotalNumberOfEntries = pHeader->TableEntries;

            m_pEntries = new (UDF_TAG_SPARE_ENTRY) SPARE_ENTRY[m_TotalNumberOfEntries];
            if( !m_pEntries )
            {
                SetLastError( ERROR_NOT_ENOUGH_MEMORY );
                goto exit_initializesparinginfo;
            }

            BytesToRead = pHeader->TableEntries * sizeof(SPARE_ENTRY);

            dwBytesRead -= sizeof(SPARING_TABLE_HEADER);
            ullOffset += sizeof(SPARING_TABLE_HEADER);
            pReadBuffer += sizeof(SPARING_TABLE_HEADER);

            ASSERT( dwBytesRead % sizeof(SPARE_ENTRY) == 0 );
            CopyMemory( m_pEntries, pReadBuffer, dwBytesRead );
        }

        EntriesRead += (Uint16)(dwBytesRead / sizeof(SPARE_ENTRY));
        pReadBuffer = (BYTE*)(&m_pEntries[EntriesRead]);
        BytesToRead -= dwBytesRead;
        ullOffset += dwBytesRead;
    }

    //
    // Walk the list and determine how many defective and live entries
    // are in there.
    //
    for( Uint16 x = 0; x < EntriesRead; x++ )
    {
        if( m_pEntries[x].OriginalBlock == UDF_SPARING_AVALIABLE )
        {
            break;
        }
        else if( m_pEntries[x].OriginalBlock == UDF_SPARING_DEFECTIVE )
        {
            m_NumberOfDeadEntries += 1;
        }
        else
        {
            m_NumberSparedEntries += 1;
        }
    }

exit_initializesparinginfo:
    if( pBuffer )
    {
        delete [] pBuffer;
    }

    if( GetLastError() != ERROR_SUCCESS )
    {
        fResult = FALSE;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// Read
//
LRESULT CSparingPartition::Read( NSRLBA lba,
                                 Uint32 Offset,
                                 Uint32 Size,
                                 BYTE* pBuffer )
{
    LRESULT lResult = ERROR_SUCCESS;
    DWORD dwBytesRead = 0;
    DWORD dwBlock = 0;

    Lock();

    dwBlock = GetPacketLocation( lba.Lbn );

    lResult = m_pVolume->GetMedia()->ReadRange( dwBlock,
                                                Offset,
                                                Size,
                                                pBuffer,
                                                &dwBytesRead );
    Unlock();

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// Write
//
LRESULT CSparingPartition::Write( NSRLBA lba,
                                  Uint32 Offset,
                                  Uint32 Size,
                                  BYTE* pBuffer )
{
    LRESULT lResult = ERROR_NOT_SUPPORTED;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// AddSparingEntry
//
// Syncronization: This object shoudl be locked on entering this method.
//
LRESULT CSparingPartition::AddSparingEntry( Uint32 Original, Uint32 Spared )
{
    LRESULT lResult = ERROR_NOT_SUPPORTED;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetPacketLocation
//
// Syncronization: This object should be locked on entering this method.
//
Uint32 CSparingPartition::GetPacketLocation( Uint32 Original )
{
    Uint32 PartStart = m_pVolume->GetPartitionStart( m_ReferenceNumber );

    Uint16 Difference = (Uint16)(Original % m_PacketLength);
    DWORD dwPacket = Original - Difference;
    DWORD dwReturn = dwPacket + PartStart;

    for( Uint16 Indx = 0; Indx < m_NumberSparedEntries; Indx++ )
    {
        if( m_pEntries[Indx].OriginalBlock == dwPacket )
        {
            dwReturn = m_pEntries[Indx].SparedBlock;
            goto exit_getpacketlocation;
        }

        if( m_pEntries[Indx].OriginalBlock == -1 )
        {
            goto exit_getpacketlocation;
        }
    }

exit_getpacketlocation:
    dwReturn += Difference;

    return dwReturn;
}

// -----------------------------------------------------------------------------
// CVirtualPartition
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CVirtualPartition::CVirtualPartition()
: m_pVolume( NULL ),
  m_pBasePartition( NULL ),
  m_ReferenceNumber( INVALID_REFERENCE ),
  m_pMedia( NULL ),
  m_NumEntries( 0 ),
  m_pEntries( NULL ),
  m_FileType( 0 )
{
    InitializeCriticalSection( &m_cs );
    ZeroMemory( &m_Header, sizeof(m_Header) );
    ZeroMemory( &m_Trailer, sizeof(m_Trailer) );
}

// /////////////////////////////////////////////////////////////////////////////
// Destructor
//
CVirtualPartition::~CVirtualPartition()
{
    if( m_pEntries )
    {
        delete [] m_pEntries;
    }

    DeleteCriticalSection( &m_cs );
}

// /////////////////////////////////////////////////////////////////////////////
// Initialize
//
LRESULT CVirtualPartition::Initialize( CPhysicalPartition* pBasePartition,
                                       Uint16 ReferenceNumber )
{
    Uint32 VATSector = 0;
    NSRLBA Location = { 0 };
    Uint16 PhysicalReference = INVALID_REFERENCE;
    CStream* pStream = NULL;
    Uint64 FileSize = 0;
    DWORD dwBytesRead = 0;
    LRESULT lResult = ERROR_SUCCESS;
    DWORD dwEntryOffset = 0;

    ASSERT( pBasePartition != NULL );

    m_pBasePartition = pBasePartition;
    m_pVolume = pBasePartition->GetVolume();
    m_ReferenceNumber = ReferenceNumber;

    m_pMedia = (CWriteOnceMedia*)m_pVolume->GetMedia();

    PhysicalReference = pBasePartition->GetReferenceNumber();

    if( PhysicalReference == INVALID_REFERENCE )
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!CVirtualPartition::Initialize - Cannot get the physical reference number.\r\n")));
        lResult = ERROR_DISK_CORRUPT;
        goto exit_initialize;
    }

    //
    // It's kind of bogus, but this will also return the type of VAT we should
    // expect.  It is stored in the m_FileType variable.  0 means we should
    // expect a 1.50 VAT, while ICBTAG_FILE_T_VAT means we should have a 2.00+
    // VAT.
    //
    if( (VATSector = GetVATSector()) == 0 )
    {
        DEBUGMSG(ZONE_MEDIA,(TEXT("UDFS!CVirtualPartition:Initialize - No VAT found on media.\r\n")));
        lResult = ERROR_DISK_CORRUPT;
        goto exit_initialize;
    }


    Location.Lbn = VATSector - m_pVolume->GetPartitionStart( PhysicalReference );
    Location.Partition = PhysicalReference;

    DEBUGMSG(ZONE_MEDIA,(TEXT("UDFS!CVirtualPartition::Initialize - Looking for VAT at %d:%d\r\n"), Location.Partition, Location.Lbn));

    //
    // Try to read the VAT ICB.
    //
    lResult = m_Vat.Initialize( m_pVolume, Location );
    if( lResult != ERROR_SUCCESS )
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!CVirtualPartition::Initialize - Can't find VAT at %d:%d\r\n"), Location.Partition, Location.Lbn));
        goto exit_initialize;
    }

    //
    // We want to keep the VAT in memory for faster reference.  Allocate space
    // for the file data and read it in.
    //
    pStream = m_Vat.GetMainStream();

    FileSize = pStream->GetFileSize();

    if( m_FileType == ICBTAG_FILE_T_VAT &&
        FileSize < sizeof(m_Header) )
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!CVirtualPartition::Initialize - The VAT size is invalid.\r\n")));
        lResult = ERROR_DISK_CORRUPT;
        goto exit_initialize;
    }
    else if( m_FileType == ICBTAG_FILE_T_NOTSPEC &&
             FileSize < sizeof(m_Trailer) )
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!CVirtualPartition::Initialize - The VAT size is invalid.\r\n")));
        lResult = ERROR_DISK_CORRUPT;
        goto exit_initialize;
    }

    if( m_FileType == ICBTAG_FILE_T_VAT )
    {
        lResult = pStream->ReadFile( 0, &m_Header, sizeof(m_Header), &dwBytesRead );
        if( lResult != ERROR_SUCCESS )
        {
            goto exit_initialize;
        }

        //
        // TODO::Validate that the read/write revision are ok for us.
        //

        FileSize -= sizeof(m_Header) + m_Header.ImpUseLength;

        dwEntryOffset = sizeof(m_Header) + m_Header.ImpUseLength;
    }
    else
    {
        lResult = pStream->ReadFile( FileSize - sizeof(m_Trailer),
                                     &m_Trailer,
                                     sizeof(m_Trailer),
                                     &dwBytesRead );
        if( lResult != ERROR_SUCCESS )
        {
            goto exit_initialize;
        }

        if( strcmp( (char*)m_Trailer.RegId.Identifier, "*UDF Virtual Alloc Tbl" ) )
        {
            lResult = ERROR_DISK_CORRUPT;
            goto exit_initialize;
        }

        FileSize -= sizeof(m_Trailer);
    }

    if( FileSize > ULONG_MAX )
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!CVirtualPartition::Initialize - The VAT is too large to read in!\r\n")));
        lResult = ERROR_NOT_ENOUGH_MEMORY;
        goto exit_initialize;
    }

    m_NumEntries = (DWORD)FileSize / 4;
    m_pEntries = new (UDF_TAG_BUFFER_32) Uint32[m_NumEntries];
    if( !m_pEntries )
    {
        lResult = ERROR_NOT_ENOUGH_MEMORY;
        goto exit_initialize;
    }

    lResult = pStream->ReadFile( dwEntryOffset,
                                 m_pEntries,
                                 (DWORD)m_NumEntries * sizeof(Uint32),
                                 &dwBytesRead );
    if( lResult != ERROR_SUCCESS )
    {
        goto exit_initialize;
    }

exit_initialize:

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// InitializeVirtualInfo
//
/*
BOOL CVirtualPartition::InitializeVirtualInfo( PPARTMAP_VIRTUAL pVirtualMap )
{
    ASSERT( pVirtualMap != NULL );
    BOOL fResult = TRUE;

    if( (m_pVolume->GetMediaType() & MED_FLAG_INCREMENTAL) == 0 )
    {
        SetLastError( ERROR_INVALID_MEDIA );
        fResult = FALSE;
        goto exit_initializevirtualinfo;
    }

    //
    // TODO::Confirm that the UDF revision indicated supports virtual partitions
    // and that the identifier in the partition type identifier is correct.
    //


exit_initializevirtualinfo:

    return fResult;
}
*/

// /////////////////////////////////////////////////////////////////////////////
// Read
//
LRESULT CVirtualPartition::Read( NSRLBA lba,
                                 Uint32 Offset,
                                 Uint32 Size,
                                 BYTE* pBuffer )
{
    LRESULT lResult = ERROR_SUCCESS;

    Lock();

    if( lba.Partition == GetReferenceNumber() )
    {
        lba.Lbn = GetBlockThroughVAT( lba.Lbn );
        lba.Partition = m_pBasePartition->GetReferenceNumber();
    }

    ASSERT( lba.Partition == m_pBasePartition->GetReferenceNumber() );

    lResult = m_pBasePartition->Read( lba,
                                      Offset,
                                      Size,
                                      pBuffer );

    Unlock();

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// Write
//
LRESULT CVirtualPartition::Write( NSRLBA lba,
                                  Uint32 Offset,
                                  Uint32 Size,
                                  BYTE* pBuffer )
{
    LRESULT lResult = ERROR_ACCESS_DENIED;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetPhysicalBlock
//
Uint32 CVirtualPartition::GetPhysicalBlock( NSRLBA lba )
{
    Uint32 Block;

    if( lba.Partition == m_ReferenceNumber )
    {
        lba.Lbn = GetBlockThroughVAT( lba.Lbn );
    }

    Block = m_pBasePartition->GetPhysicalBlock( lba );

    return Block;
}

// /////////////////////////////////////////////////////////////////////////////
// GetVATICB
//
// TODO::Add support for UDF 1.50 media.  This VAT is different than that found
// on 2.00 and later.  We need revision detection built in to catch this.
//
Uint32 CVirtualPartition::GetVATSector()
{
    PASS_THROUGH_DIRECT PassThrough = { 0 };
    SCSI_PASS_THROUGH_DIRECT& PassThroughDirect = PassThrough.PassThrough;
    CDB& Cdb = *((CDB*)PassThroughDirect.Cdb);
    Uint32 VATSector = 0;
    Uint32 ReadSector = 0;
    BYTE* pBuffer = NULL;
    DWORD dwBufferSize = 0;
    LRESULT lResult = ERROR_READ_FAULT;
    DWORD dwBytesReturned = 0;

    ReadSector = m_pVolume->GetLastSector();

    dwBufferSize = m_pVolume->GetSectorSize();
    pBuffer = new (UDF_TAG_BYTE_BUFFER) BYTE[ dwBufferSize ];
    if( !pBuffer )
    {
        goto exit_getvatsector;
    }

    if( m_pVolume->GetMediaType() & MED_FLAG_CD )
    {
        DWORD dwBlocks[] =
            { m_pVolume->GetLastSector() - 2,     // Account for the run-out blocks
              m_pVolume->GetLastSector() - 152,   // Try for run-out blocks and lead-out
              m_pVolume->GetLastSector(),         // Just try the last sector
              m_pVolume->GetLastSector() - 150 }; // Try accounting for lead-out

        ZeroMemory( &PassThroughDirect, sizeof(PassThroughDirect) );
        PassThroughDirect.CdbLength = 12;
        PassThroughDirect.DataBuffer = pBuffer;
        PassThroughDirect.DataIn = 1;
        PassThroughDirect.Length = sizeof(PassThroughDirect);
        PassThroughDirect.TimeOutValue = 50;
        PassThroughDirect.SenseInfoOffset = sizeof(PassThroughDirect);

        for( int x = 0; x < sizeof(dwBlocks) / sizeof(dwBlocks[0]); x++ )
        {
            ReadSector = dwBlocks[x];

            //
            // These values are changed underneath us and need to be reset each
            // time.
            //
            PassThroughDirect.DataTransferLength = dwBufferSize;
            PassThroughDirect.SenseInfoLength = sizeof(SENSE_DATA);

            //
            // According to the MMC 5 spec, Mode 2 Form 1 cannot return
            // a combination of User Data + Header data.  It can do user
            // data, sync, and all headers which is strange.  Anyways, we
            // can do them both separately if we want, but not together.
            // It does work on some drives, but not others.
            //
            Cdb.READ_CD.OperationCode = SCSIOP_READ_CD;
            Cdb.READ_CD.StartingLBA[0] = (BYTE)((ReadSector >> 24) & 0xFF);
            Cdb.READ_CD.StartingLBA[1] = (BYTE)((ReadSector >> 16) & 0xFF);
            Cdb.READ_CD.StartingLBA[2] = (BYTE)((ReadSector >> 8) & 0xFF);
            Cdb.READ_CD.StartingLBA[3] = (BYTE)(ReadSector & 0xFF);
            Cdb.READ_CD.TransferBlocks[2] = 1;
            Cdb.READ_CD.IncludeUserData = 1;
            Cdb.READ_CD.HeaderCode = 0;
            Cdb.READ_CD.IncludeSyncData = 0;

            ZeroMemory( pBuffer, dwBufferSize );

            if( m_pVolume->UDFSDeviceIoControl( IOCTL_SCSI_PASS_THROUGH_DIRECT,
                                                &PassThrough,
                                                sizeof(PassThrough),
                                                &PassThrough,
                                                sizeof(PassThrough),
                                                &dwBytesReturned,
                                                NULL ) )
            {
                Uint16 Reference = m_pBasePartition->GetReferenceNumber();
                Uint32 BlockNumber = ReadSector -
                                     m_pVolume->GetPartitionStart( Reference );

                if( FoundVAT( BlockNumber,
                              pBuffer,
                              dwBufferSize ) )
                {
                    VATSector = ReadSector;
                    goto exit_getvatsector;
                }
            }
#ifdef DEBUG
            else
            {
                if( GetLastError() == ERROR_INVALID_PARAMETER )
                {
                    ASSERT(FALSE);
                }
                else
                {
                    DEBUGMSG(ZONE_MEDIA,(TEXT("UDFS!CVirtualPartition::GetVATSector - Sense %x\\%x\\%x\r\n"), PassThrough.SenseData.SenseKey, PassThrough.SenseData.AdditionalSenseCode, PassThrough.SenseData.AdditionalSenseCodeQualifier));
                }
            }
#endif
        }
    }
    else
    {
        DWORD dwBlocks[] =
            { m_pVolume->GetLastSector(),         // Try the last sector
              m_pVolume->GetLastSector() - 150 }; // Account for lead-out

        ZeroMemory( &PassThroughDirect, sizeof(PassThroughDirect) );
        PassThroughDirect.CdbLength = 12;
        PassThroughDirect.DataBuffer = pBuffer;
        PassThroughDirect.DataIn = 1;
        PassThroughDirect.Length = sizeof(PassThroughDirect);
        PassThroughDirect.TimeOutValue = 50;
        PassThroughDirect.SenseInfoOffset = sizeof(PassThroughDirect);

        for( int x = 0; x < sizeof(dwBlocks) / sizeof(dwBlocks[0]); x++ )
        {
            ReadSector = dwBlocks[x];

            PassThroughDirect.DataTransferLength = dwBufferSize;
            PassThroughDirect.SenseInfoLength = sizeof(SENSE_DATA);

            ZeroMemory( pBuffer, dwBufferSize );

            Cdb.READ12.OperationCode = SCSIOP_READ12;
            Cdb.READ12.LogicalBlock[0] = (BYTE)(ReadSector >> 24);
            Cdb.READ12.LogicalBlock[1] = (BYTE)((ReadSector >> 16) & 0xFF);
            Cdb.READ12.LogicalBlock[2] = (BYTE)((ReadSector >> 8) & 0xFF);
            Cdb.READ12.LogicalBlock[3] = (BYTE)(ReadSector & 0xFF);
            Cdb.READ12.TransferLength[3] = 1;

            if( m_pVolume->UDFSDeviceIoControl( IOCTL_SCSI_PASS_THROUGH_DIRECT,
                                                &PassThrough,
                                                sizeof(PassThrough),
                                                &PassThrough,
                                                sizeof(PassThrough),
                                                &dwBytesReturned,
                                                NULL ) )
            {
                Uint16 Reference = m_pBasePartition->GetReferenceNumber();
                Uint32 BlockNumber = ReadSector -
                                     m_pVolume->GetPartitionStart( Reference );

                if( FoundVAT( BlockNumber,
                              pBuffer,
                              dwBufferSize ) )
                {
                    VATSector = ReadSector;
                    goto exit_getvatsector;
                }
            }
        }

    }

exit_getvatsector:
    if( pBuffer )
    {
        delete [] pBuffer;
    }

    return VATSector;
}

// /////////////////////////////////////////////////////////////////////////////
// FoundVAT
//
BOOL CVirtualPartition::FoundVAT( DWORD dwSector,
                                  BYTE* pBuffer,
                                  DWORD dwBufferSize )
{
    BOOL fVat = FALSE;
    ASSERT( pBuffer );
    PDESTAG pDesTag = (PDESTAG)pBuffer;
    PICBTAG pIcbTag = (PICBTAG)(pBuffer + sizeof(DESTAG));

    //
    // The VAT must be either a file entry or extended file entry.
    //
    if( !VerifyDescriptor( dwSector, pBuffer, dwBufferSize ) )
    {
        goto exit_foundvat;
    }

    if( pDesTag->Ident != DESTAG_ID_NSR_EXT_FILE &&
        pDesTag->Ident != DESTAG_ID_NSR_FILE )
    {
        goto exit_foundvat;
    }

    //
    // UDF 1.50  = ICBTAG_FILE_T_NOTSPEC
    // UDF 2.00+ = ICBTAG_FILE_T_VAT
    //
    if( pIcbTag->FileType != ICBTAG_FILE_T_VAT &&
        pIcbTag->FileType != ICBTAG_FILE_T_NOTSPEC )
    {
        goto exit_foundvat;
    }

    //
    // Store this so taht we know what type of VAT we have later.
    //
    m_FileType = pIcbTag->FileType;

    //
    // TODO::More checks - check file size for sizeof(VAT_TRAILER) or
    // sizeof(VAT_HEADER).
    //

    fVat = TRUE;

exit_foundvat:

    return fVat;
}

// -----------------------------------------------------------------------------
// CMetadataPartition
// -----------------------------------------------------------------------------

//
// TODO::Add some caching so that we don't have to access the metadata file on
// each MD read.
//

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CMetadataPartition::CMetadataPartition()
: m_pVolume( NULL ),
  m_pBasePartition( NULL ),
  m_ReferenceNumber( INVALID_REFERENCE ),
  m_MetadataFileLocation( 0 ),
  m_MirrorLocation( 0 ),
  m_BitmapLocation( 0 ),
  m_AllocationUnitSize( 0 ),
  m_AlignmentUnitSize( 0 ),
  m_fMirror( FALSE )
{
    InitializeCriticalSection( &m_cs );
}

// /////////////////////////////////////////////////////////////////////////////
// Destructor
//
CMetadataPartition::~CMetadataPartition()
{
    DeleteCriticalSection( &m_cs );
}

// /////////////////////////////////////////////////////////////////////////////
// Initialize
//
LRESULT CMetadataPartition::Initialize( CPhysicalPartition* pBasePartition,
                                     Uint16 ReferenceNumber )
{
    ASSERT( pBasePartition != NULL );

    m_pBasePartition = pBasePartition;
    m_pVolume = pBasePartition->GetVolume();
    m_ReferenceNumber = ReferenceNumber;

    return ERROR_SUCCESS;
}

// /////////////////////////////////////////////////////////////////////////////
// InitializeMetadataInfo
//
BOOL CMetadataPartition::InitializeMetadataInfo( PPARTMAP_METADATA pMetadataMap )
{
    BOOL fResult = TRUE;
    NSRLBA Location = { 0 };
    LRESULT lResult = ERROR_SUCCESS;

    m_MetadataFileLocation = pMetadataMap->FileLocation;
    m_MirrorLocation = pMetadataMap->MirrorFileLocation;
    m_BitmapLocation = pMetadataMap->BitmapLocation;
    m_AllocationUnitSize = pMetadataMap->AllocationUnitSize;
    m_AlignmentUnitSize = pMetadataMap->AlignmentUnitSize;

    m_fMirror = (pMetadataMap->Flags & PARTMAP_METADATA_F_DUPLICATE) != 0;

    Location.Lbn = m_MetadataFileLocation;
    Location.Partition = m_pBasePartition->GetReferenceNumber();

    lResult = m_MDFile.Initialize( m_pVolume, Location );
    if( lResult != ERROR_SUCCESS )
    {
        DEBUGMSG(ZONE_INIT,(TEXT("UDFS!CMetadataPartition::InitializeMetadataInfo - Unable to read the metadata file.\r\n")));
        SetLastError( lResult );
        fResult = FALSE;
        goto exit_initializemetadatainfo;
    }

    //
    // We will ignore the metadata bitmap file until we add write support.
    // TODO::Read in MD Bitmap file for write support.  Note that the bitmap
    // location can be 0xFFFFFFFF which means the disc was mastered and there
    // is no bitmap to use.
    //

    //
    // TODO::Read in MD Mirror file.
    //

exit_initializemetadatainfo:

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// Read
//
LRESULT CMetadataPartition::Read( NSRLBA lba,
                                  Uint32 Offset,
                                  Uint32 Size,
                                  BYTE* pBuffer )
{
    LRESULT lResult = ERROR_SUCCESS;
    CStream* pStream = m_MDFile.GetMainStream();
    DWORD dwBytesRead = 0;

    if( lba.Partition == m_ReferenceNumber )
    {
        ASSERT( pStream != NULL );

        __try
        {
            Uint64 Position = (lba.Lbn << m_pVolume->GetShiftCount()) + Offset;

            lResult = pStream->ReadFile( Position,
                                         pBuffer,
                                         Size,
                                         &dwBytesRead );
            if( lResult == ERROR_SUCCESS && dwBytesRead != Size )
            {
                //
                // TODO::What is the right failure for this?
                //
                lResult = ERROR_GEN_FAILURE;
            }
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            lResult = ERROR_INVALID_PARAMETER;
        }
    }
    else
    {
        lResult = m_pBasePartition->Read( lba, Offset, Size, pBuffer );
    }

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// Write
//
LRESULT CMetadataPartition::Write( NSRLBA lba,
                                   Uint32 Offset,
                                   Uint32 Size,
                                   BYTE* pBuffer )
{
    LRESULT lResult = ERROR_CALL_NOT_IMPLEMENTED;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetPhysicalBlock
//
Uint32 CMetadataPartition::GetPhysicalBlock( NSRLBA lba )
{
    Uint32 Block;
    CStream* pStream = m_MDFile.GetMainStream();
    Uint64 Offset = (Uint64)(lba.Lbn) << m_pVolume->GetShiftCount();
    LONGAD longAd;

    if( lba.Partition == m_ReferenceNumber )
    {
        ASSERT( pStream != NULL );

        if( pStream->GetExtentInformation( Offset, &longAd ) == ERROR_SUCCESS )
        {
            lba = longAd.Start;
        }
        else
        {
            DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!CMetadataPartition::GetPhysicalBlock - Cannot find extent in the MD file.\r\n")));
            return 0;
        }
    }

    Block = m_pBasePartition->GetPhysicalBlock( lba );

    return Block;
}

