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

#include "CdRom.h"

#define CD_SECTOR_SIZE    2048              // All CD/DVD devices are 2K sectors
#define DEFAULT_CACH_SIZE (1*1024*1024)/CD_SECTOR_SIZE  // 1 MB cache in sectors

// -----------------------------------------------------------------------------
// CdPartMgr_t
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CdPartMgr_t::CdPartMgr_t()
: m_pDevice( NULL ),
  m_NumSessions( 0 ),
  m_NumTracks( 0 ),
  m_pTrackInfo( NULL )
{
}

// /////////////////////////////////////////////////////////////////////////////
// Destructor
//
CdPartMgr_t::~CdPartMgr_t()
{
    if( m_pTrackInfo )
    {
        delete [] m_pTrackInfo;
    }
}

// /////////////////////////////////////////////////////////////////////////////
// Initialize
//
bool CdPartMgr_t::Initialize( Device_t* pDevice )
{
    ASSERT( pDevice != NULL );

    m_pDevice = pDevice;

    return true;
}

// /////////////////////////////////////////////////////////////////////////////
// GetPartitionInfo
//
DWORD CdPartMgr_t::GetPartitionInfo( LPCTSTR strPartName, PD_PARTINFO *pInfo )
{
    USHORT Partition = 0;

    if( !GetIndexFromName( strPartName, &Partition ) )
    {
        return GetLastError();
    }

    return GetPartitionInfo( Partition, pInfo );
}

// /////////////////////////////////////////////////////////////////////////////
// GetPartitionInfo
//
DWORD CdPartMgr_t::GetPartitionInfo( USHORT Partition, PD_PARTINFO* pInfo )
{
    if( !IsValidPartition( Partition ) )
    {
        return GetLastError();
    }

    __try
    {
        ZeroMemory( pInfo, sizeof(PD_PARTINFO) );

        pInfo->snNumSectors = GetSectorsInSession( (USHORT)Partition );
        pInfo->cbSize = sizeof(PD_PARTINFO);
        _snwprintf( pInfo->szPartitionName, 6, L"PART%02d", Partition );
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        return ERROR_INVALID_PARAMETER;
    }

    return ERROR_SUCCESS;
}

// /////////////////////////////////////////////////////////////////////////////
// FindPartitionNext
//
DWORD CdPartMgr_t::FindPartitionNext( PSEARCH_CONTEXT pSearchContext,
                                      PD_PARTINFO *pInfo )
{
    ASSERT( pSearchContext != NULL);
    ASSERT( pInfo != NULL );

    //
    // Find the next valid session number.  This will skip data partitions
    // except for the last non-empty data partition on the disc.  All audio
    // partitions will be reported here.
    //
    while( pSearchContext->SessionIndex <= GetNumberOfSessions() &&
           !IsValidPartition( pSearchContext->SessionIndex ) )
    {
        pSearchContext->SessionIndex += 1;
    }

    //
    // Check if there are any sessions left.
    //
    if( pSearchContext->SessionIndex > GetNumberOfSessions() )
    {
        return ERROR_NO_MORE_FILES;
    }

    //
    // We do have a valid session, so get the information.
    //
    return GetPartitionInfo( pSearchContext->SessionIndex++, pInfo );
}

#define SWAP_ULONG_BYTEORDER(x)  ((((ULONG)((x)>>24))&0xFF) | (((ULONG)((x)>>8))&0xFF00) | (((ULONG)((x)<<8))&0xFF0000) | (((ULONG)((x)<<24))&0xFF000000))

// /////////////////////////////////////////////////////////////////////////////
// RefreshPartInfo
//
bool CdPartMgr_t::RefreshPartInfo()
{
    bool fResult = true;
    READ_CAPACITY_DATA ReadCapacity = { 0 };
    DISC_INFORMATION DiscInfo;
    DISC_INFO di = DI_SCSI_INFO;
    DISK_INFO DiskInfo = { 0 };
    CDB Cdb = { 0 };
    SENSE_DATA SenseData = { 0 };
    USHORT FirstTrack = 1;
    USHORT LastTrack = 1;
    DWORD dwBytesReturned = 0;

    //
    // In case this is on USB, we need to make sure the driver knows the sector
    // size.  Call and set IOCTL_DISK_GETINFO/IOCTL_DISK_SETINFO.  Ignore errors
    // as ATAPI doesn't support this.
    //

    Cdb.READ_CAPACITY16.OperationCode = SCSIOP_READ_CAPACITY;
    if( !m_pDevice->SendPassThroughDirect( &Cdb,
                                           (BYTE*)&ReadCapacity,
                                           sizeof(ReadCapacity),
                                           TRUE,
                                           m_pDevice->GetGroupOneTimeout(),
                                           &dwBytesReturned,
                                           &SenseData ) )
    {
        fResult = false;
        goto exit_refreshpartinfo;
    }

    /* data comes back from passthrough in big-endian (natural) format */
    DiskInfo.di_bytes_per_sect = SWAP_ULONG_BYTEORDER(ReadCapacity.BytesPerBlock);
    DiskInfo.di_total_sectors = SWAP_ULONG_BYTEORDER(ReadCapacity.LogicalBlockAddress);

    m_pDevice->CdDeviceStoreIoControl( IOCTL_DISK_SETINFO,
                                       &DiskInfo,
                                       sizeof(DiskInfo),
                                       NULL,
                                       0,
                                       &dwBytesReturned,
                                       NULL );


    if( m_pTrackInfo )
    {
        delete [] m_pTrackInfo;
        m_pTrackInfo = NULL;
    }

    m_NumSessions = 0;
    m_NumTracks = 0;

    //
    // We are going to read in the track information for all tracks currently
    // on the disc.  In order to determine the number of tracks, call into
    // the DISC_INFO function.
    //
    DEBUGMSG( ZONE_IO, (L"CDROM::RefreshPartInfo calling IOCTL_CDROM_DISC_INFO.\r\n") );
    if( !m_pDevice->CdDeviceIoControl( 0,
                                       IOCTL_CDROM_DISC_INFO,
                                       &di,
                                       sizeof(di),
                                       &DiscInfo,
                                       sizeof(DiscInfo),
                                       &dwBytesReturned,
                                       NULL ) )
    {
        fResult = false;
        goto exit_refreshpartinfo;
    }

    LastTrack = (DiscInfo.LastSessionLastTrackMsb << 8) +
                 DiscInfo.LastSessionLastTrackLsb;
    FirstTrack = DiscInfo.FirstTrackNumber;

    if( LastTrack < FirstTrack )
    {
        SetLastError( ERROR_INVALID_MEDIA );
        fResult = false;
        goto exit_refreshpartinfo;
    }

    m_NumTracks = LastTrack - FirstTrack + 1;
    m_NumSessions = DiscInfo.NumberOfSessionsLsb +
                   (DiscInfo.NumberOfSessionsMsb << 8 );

    m_pTrackInfo = new TRACK_INFORMATION2[ m_NumTracks ];
    if( !m_pTrackInfo )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        fResult = false;
        goto exit_refreshpartinfo;
    }

    //
    // Store all the information we will need later to determine "partition"
    // information and to confirm that a disc has either been changed or
    // re-inserted.
    //
    for( USHORT Track = FirstTrack; Track <= LastTrack; Track++ )
    {
        TRACK_INFORMATION2 TrackInfo = { 0 };

        ZeroMemory( &SenseData, sizeof(SenseData) );
        ZeroMemory( &Cdb, sizeof(Cdb) );
        Cdb.READ_TRACK_INFORMATION.OperationCode = SCSIOP_READ_TRACK_INFORMATION;
        Cdb.READ_TRACK_INFORMATION.Track = 1;
        Cdb.READ_TRACK_INFORMATION.BlockAddress[2] = (UCHAR)((Track >> 8) & 0xFF);
        Cdb.READ_TRACK_INFORMATION.BlockAddress[3] = (UCHAR)(Track & 0xFF);
        Cdb.READ_TRACK_INFORMATION.AllocationLength[0] = (UCHAR)((sizeof(TrackInfo) >> 8) & 0xFF);
        Cdb.READ_TRACK_INFORMATION.AllocationLength[1] = (UCHAR)(sizeof(TrackInfo) & 0xFF);

        DEBUGMSG( ZONE_IO, (L"CDROM::RefreshPartInfo calling READ_TRACK_INFORMATION.\r\n") );
        if( !m_pDevice->SendPassThroughDirect( &Cdb,
                                               (BYTE*)&TrackInfo,
                                               sizeof(TrackInfo),
                                               TRUE,
                                               m_pDevice->GetGroupOneTimeout(),
                                               &dwBytesReturned,
                                               &SenseData ) )
        {
            fResult = false;
            goto exit_refreshpartinfo;
        }

        CopyMemory( &m_pTrackInfo[Track - FirstTrack], &TrackInfo, sizeof(TrackInfo) );
    }

exit_refreshpartinfo:

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetFirstTrackIndexFromSession
//
// Returning Track USHRT_MAX specifies an error condition.
//
USHORT CdPartMgr_t::GetFirstTrackIndexFromSession( USHORT TargetSession )
{
    if( TargetSession > GetNumberOfSessions() )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return USHRT_MAX;
    }

    for( USHORT Track = 0; Track < GetNumberOfTracks(); Track++ )
    {
        USHORT SessionNumber = 0;

        SessionNumber = m_pTrackInfo[Track].SessionNumberLsb +
                       (m_pTrackInfo[Track].SessionNumberMsb << 8 );

        if( SessionNumber == TargetSession )
        {
            return Track;
        }
    }

    return USHRT_MAX;
}

// /////////////////////////////////////////////////////////////////////////////
// GetLastTrackIndexFromSession
//
// Returning Track USHRT_MAX specifies an error condition.
//
USHORT CdPartMgr_t::GetLastTrackIndexFromSession( USHORT TargetSession )
{
    USHORT Start = GetFirstTrackIndexFromSession( TargetSession );
    USHORT Track = Start + 1;

    if( Start == USHRT_MAX )
    {
        return USHRT_MAX;
    }

    for( ; Track < GetNumberOfTracks(); Track++ )
    {
        USHORT SessionNumber = 0;

        SessionNumber = m_pTrackInfo[Track].SessionNumberLsb +
                       (m_pTrackInfo[Track].SessionNumberMsb << 8 );

        if( SessionNumber != TargetSession )
        {
            break;
        }
    }

    return Track - 1;
}

// /////////////////////////////////////////////////////////////////////////////
// GetFirstTrackFromSession
//
// Returning track USHRT_MAX specifies an error condition.
//
USHORT CdPartMgr_t::GetFirstTrackFromSession( USHORT TargetSession )
{
    if( TargetSession > GetNumberOfSessions() )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return USHRT_MAX;
    }

    for( USHORT Track = 0; Track < GetNumberOfTracks(); Track++ )
    {
        USHORT SessionNumber = 0;

        SessionNumber = m_pTrackInfo[Track].SessionNumberLsb +
                       (m_pTrackInfo[Track].SessionNumberMsb << 8 );

        if( SessionNumber == TargetSession )
        {
            USHORT TrackNumber = m_pTrackInfo[Track].TrackNumberLsb +
                                (m_pTrackInfo[Track].TrackNumberMsb << 8);
            return TrackNumber;
        }
    }

    return USHRT_MAX;
}

// /////////////////////////////////////////////////////////////////////////////
// GetLastTrackFromSession
//
// Returning track 0 specifies an error condition.
//
USHORT CdPartMgr_t::GetLastTrackFromSession( USHORT TargetSession )
{
    bool fFound = false;
    USHORT LastTrack = 0;

    if( TargetSession > GetNumberOfSessions() )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }

    for( USHORT Track = 0; Track < GetNumberOfTracks(); Track++ )
    {
        USHORT SessionNumber = 0;

        SessionNumber = m_pTrackInfo[Track].SessionNumberLsb +
                       (m_pTrackInfo[Track].SessionNumberMsb << 8 );

        if( SessionNumber == TargetSession )
        {
            LastTrack = m_pTrackInfo[Track].TrackNumberLsb +
                       (m_pTrackInfo[Track].TrackNumberMsb << 8);
        }
    }

    return LastTrack;
}

// /////////////////////////////////////////////////////////////////////////////
// GetLastTrackFromSession
//
// Returning track 0 specifies an error condition.
//
USHORT CdPartMgr_t::GetTracksInSession( USHORT TargetSession )
{
    USHORT FirstTrack = GetFirstTrackFromSession( TargetSession );
    USHORT LastTrack = GetLastTrackFromSession( TargetSession );

    if( FirstTrack == USHRT_MAX || LastTrack == USHRT_MAX )
    {
        return 0;
    }

    return LastTrack - FirstTrack + 1;
}

// /////////////////////////////////////////////////////////////////////////////
// GetNumberOfAudioSessions
//
USHORT CdPartMgr_t::GetNumberOfAudioSessions()
{
    USHORT NumSessions = 0;

    for( USHORT Session = 1; Session <= GetNumberOfSessions(); Session++ )
    {
        USHORT Track = GetFirstTrackIndexFromSession( Session );

        if( Track == USHRT_MAX )
        {
            continue;
        }

        if( (m_pTrackInfo[Track].TrackMode & 0x4) == 0 )
        {
            NumSessions += 1;
        }
    }

    return NumSessions;
}

// /////////////////////////////////////////////////////////////////////////////
// GetAudioSessionIndex
//
// This will return the session number that corresponds to the given audio
// session number.
//
// A return value of USHRT_MAX indicates an error.
//
// NOTE::Not sure this fits into the partitioning scheme anymore.  Delete it?
//
USHORT CdPartMgr_t::GetAudioSessionIndex( USHORT AudioSession )
{
    USHORT AudioSessions = 0;

    for( USHORT Session = 1; Session <= GetNumberOfSessions(); Session++ )
    {
        USHORT Track = GetFirstTrackIndexFromSession( Session );

        if( Track == USHRT_MAX )
        {
            continue;
        }

        if( (m_pTrackInfo[Track].TrackMode & 0x4) == 0 )
        {
            AudioSessions += 1;
        }

        if( AudioSessions == AudioSession )
        {
            return Session;
        }
    }

    return USHRT_MAX;
}

// /////////////////////////////////////////////////////////////////////////////
// GetSectorsInSession
//
DWORD CdPartMgr_t::GetSectorsInSession( USHORT TargetSession )
{
    USHORT StartTrack = GetFirstTrackIndexFromSession( TargetSession );
    USHORT LastTrack = GetLastTrackIndexFromSession( TargetSession );
    DWORD dwNumSectors = 0;

    if( StartTrack == USHRT_MAX || LastTrack == USHRT_MAX )
    {
        return 0;
    }

    for( USHORT Track = StartTrack; Track <= LastTrack; Track++ )
    {
        DWORD TrackSize = 0;

        SwapCopyUchar4(&TrackSize, m_pTrackInfo[Track].TrackSize)

        dwNumSectors += TrackSize;
    }

    return dwNumSectors;
}

// /////////////////////////////////////////////////////////////////////////////
// IsValidPartition
//
BOOL CdPartMgr_t::IsValidPartition( USHORT Partition )
{
    USHORT FirstTrackIndex = 0;

    //
    // Make sure the value is possible.
    //
    if( Partition > GetNumberOfSessions() )
    {
        return FALSE;
    }

    FirstTrackIndex = GetFirstTrackIndexFromSession( Partition );
    if( FirstTrackIndex == USHRT_MAX )
    {
        return FALSE;
    }

    ASSERT( m_pTrackInfo[FirstTrackIndex].SessionNumberLsb +
            (m_pTrackInfo[FirstTrackIndex].SessionNumberMsb << 8) == Partition );

    //
    // If this is a data track then make sure it's the last one on the disc or
    // this is not a true partition.
    //
    if( m_pTrackInfo[FirstTrackIndex].TrackMode & 0x4 )
    {
        USHORT LastValidDataSession = GetLastNonEmptyDataSession();

        if( Partition != GetLastNonEmptyDataSession() )
        {
            return FALSE;
        }
    }

    if( m_pTrackInfo[FirstTrackIndex].Blank )
    {
        return FALSE;
    }

    return TRUE;
}

// /////////////////////////////////////////////////////////////////////////////
// GetIndexFromName
//
// Given the name of a partition, this will give us the index into that
// partition.
//
BOOL CdPartMgr_t::GetIndexFromName( const WCHAR* strPartName, USHORT* pIndex )
{
    BOOL fResult = TRUE;
    DWORD Partition = 0;

    ASSERT( pIndex != NULL );

    //
    // We need to determine if the partition name is valid.  It should be
    // PARTxxx.  The xxx is the session number corresponding to the given
    // partition.
    //
    __try
    {
        if( _wcsnicmp( strPartName, L"PART", 4 ) == 0 )
        {
            Partition = wcstoul( strPartName + 4, NULL, 10 );
        }
        else
        {
            fResult = FALSE;
            SetLastError( ERROR_INVALID_PARAMETER );
            goto exit_getindexfromname;
        }
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        ASSERT(FALSE);
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_getindexfromname;
    }

    if( Partition > USHRT_MAX )
    {
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_getindexfromname;
    }

    *pIndex = (USHORT)Partition;

exit_getindexfromname:

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetSessionInfo
//
// This will return information about the given "partition" or session for a
// call into IOCTL_CDROM_GET_PARTITION_INFO.
//
BOOL CdPartMgr_t::GetSessionInfo( USHORT Session,
                                  PCD_PARTITION_INFO pPartInfo,
                                  DWORD dwPartInfoSize,
                                  DWORD* pdwBytesReturned )
{
    //
    // The MMC documentation hints that the first track in the disc doesn't have
    // to be zero or one on CD Media.  So I'm playing it safe using and index
    // based on the track number of the first track.  Probably won't make a bit
    // of difference in the long run.
    //
    USHORT TracksInSession = 0;
    USHORT FirstTrack = GetFirstTrack();
    USHORT FirstTrackIndx = 0;
    USHORT LastNonEmptyDataSession = 0;
    USHORT LastValidTrackInLastSession = 0;

    if( Session > GetNumberOfSessions() )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    if( !pPartInfo || !pdwBytesReturned )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    TracksInSession = GetTracksInSession( Session );
    FirstTrackIndx = GetFirstTrackIndexFromSession( Session );

    if( FirstTrackIndx == USHRT_MAX )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    //
    // Whether the buffer is big enough or not, we will let the user know the
    // required size for this buffer for this session.
    //
    __try
    {
        *pdwBytesReturned = sizeof(CD_PARTITION_INFO) +
                            (TracksInSession - 1) * sizeof(TRACK_INFORMATION2);
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    //
    // Check that the user buffer can hold all the information for the session.
    //
    if( dwPartInfoSize < sizeof(CD_PARTITION_INFO) +
                         (TracksInSession - 1) * sizeof(TRACK_INFORMATION2) )
    {
        SetLastError( ERROR_INSUFFICIENT_BUFFER );
        return FALSE;
    }

    LastValidTrackInLastSession =
            GetLastValidTrackIndexInSession( Session );

    if( LastValidTrackInLastSession == USHRT_MAX )
    {
        ASSERT(FALSE);
        return FALSE;
    }

    __try
    {
        SwapCopyUchar4( &pPartInfo->dwStartingLBA,
                        m_pTrackInfo[FirstTrackIndx].TrackStartAddress);
        pPartInfo->fAudio = (m_pTrackInfo[FirstTrackIndx].TrackMode & 0x4) == 0;
        pPartInfo->NumberOfTracks = TracksInSession;
        pPartInfo->LastValidTrackInPartition =
                        m_pTrackInfo[LastValidTrackInLastSession].TrackNumberLsb +
                       (m_pTrackInfo[LastValidTrackInLastSession].TrackNumberMsb << 8 );
        CopyMemory( pPartInfo->TrackInformation,
                    &m_pTrackInfo[FirstTrackIndx],
                    TracksInSession * sizeof(TRACK_INFORMATION2) );
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    return TRUE;
}

// /////////////////////////////////////////////////////////////////////////////
// GetLastNonEmptyDataSession
//
// This will return the number of the last session on the disc that has data
// track(s) that are not blank.  Since the first session on a disc has to be
// 1, a return value of 0 means there is no session meeting the requirements.
//
USHORT CdPartMgr_t::GetLastNonEmptyDataSession()
{
    USHORT SessionNumber = GetNumberOfSessions();

    //
    // Look through all the sessions, starting with the last sesion on the disc.
    //
    for( ; SessionNumber > 0; SessionNumber -= 1 )
    {
        USHORT NumTracks = GetTracksInSession( SessionNumber );
        USHORT TrackIndex = GetFirstTrackIndexFromSession( SessionNumber );

        if( TrackIndex == USHRT_MAX )
        {
            SetLastError( ERROR_MEDIA_INCOMPATIBLE );
            return 0;
        }

        //
        // Audio/Data tracks cannot be mixed in a session.
        //
        if( (m_pTrackInfo[TrackIndex].TrackMode & 0x4) == 0 )
        {
            //
            // Audio session.
            //
            continue;
        }

        //
        // Look through all the tracks, starting with the last track of the
        // session.  If the track is blank we won't use it.
        //
        PREFAST_SUPPRESS(6293, "Coded with intent.");
        for( USHORT x = NumTracks - 1; x <= MAXIMUM_NUMBER_TRACKS_LARGE; x -= 1 )
        {
            if( m_pTrackInfo[TrackIndex + x].Blank )
            {
                continue;
            }

            break;
        }

        if( x > MAXIMUM_NUMBER_TRACKS_LARGE )
        {
            //
            // If we hit this then all the tracks on this session are blank.
            // Most likely the last session was closed and a new session was
            // opened with a blank track.  We want to use the previous session.
            //
            continue;
        }

        //
        // We've found the last session on the disc that contains a valid data
        // track.
        //
        break;
    }

    return SessionNumber;
}

// /////////////////////////////////////////////////////////////////////////////
// GetLastValidTrackIndexInSession
//
// This will return the number of the last track in the given session that is
// neither blank or reserved.
//
// USHRT_MAX inicates an error.
//
USHORT CdPartMgr_t::GetLastValidTrackIndexInSession( USHORT Session )
{
    USHORT NumTracks = GetTracksInSession( Session );
    USHORT TrackIndex = GetFirstTrackIndexFromSession( Session );

    if( TrackIndex == USHRT_MAX )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }

    //
    // Look through all the tracks, starting with the last track of the
    // session.  If the track is blank we won't use it.
    //
    PREFAST_SUPPRESS(6293, "Coded with intent.");
    for( USHORT x = NumTracks - 1; x <= MAXIMUM_NUMBER_TRACKS_LARGE; x -= 1 )
    {
        if( m_pTrackInfo[TrackIndex + x].Blank )
        {
            continue;
        }

        return TrackIndex + x;
    }

    SetLastError( ERROR_INVALID_PARAMETER );
    return USHRT_MAX;
}

// /////////////////////////////////////////////////////////////////////////////
// GetFirstSectorInSession
//
// Returning ULONG_MAX indicates an error.
//
DWORD CdPartMgr_t::GetFirstSectorInSession( USHORT Session )
{
    USHORT TrackIndx = 0;
    DWORD dwSectorOffset = 0;

    TrackIndx = GetFirstTrackIndexFromSession( Session );

    if( TrackIndx == USHRT_MAX )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return ULONG_MAX;
    }

    SwapCopyUchar4( &dwSectorOffset, m_pTrackInfo[TrackIndx].TrackStartAddress);

    return dwSectorOffset;
}

// /////////////////////////////////////////////////////////////////////////////
// CompareTrackInfo
//
BOOL CdPartMgr_t::CompareTrackInfo()
{
    BOOL fResult = FALSE;
    DISC_INFORMATION DiscInfo;
    DISC_INFO di = DI_SCSI_INFO;
    PTRACK_INFORMATION2 pTrackInfo = NULL;
    USHORT FirstTrack = 1;
    USHORT LastTrack = 1;
    DWORD dwBytesReturned = 0;
    USHORT NumTracks = 0;
    USHORT NumSessions = 0;

    //
    // We are going to read in the track information for all tracks currently
    // on the disc.  In order to determine the number of tracks, call into
    // the DISC_INFO function.
    //
    if( !m_pDevice->CdDeviceIoControl( 0,
                                       IOCTL_CDROM_DISC_INFO,
                                       &di,
                                       sizeof(di),
                                       &DiscInfo,
                                       sizeof(DiscInfo),
                                       &dwBytesReturned,
                                       NULL ) )
    {
        goto exit_comparetrackinfo;
    }

    LastTrack = DiscInfo.LastSessionLastTrackMsb << 8 | DiscInfo.LastSessionLastTrackLsb;
    FirstTrack = DiscInfo.LastSessionFirstTrackMsb << 8 | DiscInfo.LastSessionFirstTrackLsb;

    if( LastTrack < FirstTrack )
    {
        SetLastError( ERROR_INVALID_MEDIA );
        goto exit_comparetrackinfo;
    }

    NumTracks = LastTrack - FirstTrack + 1;
    if( NumTracks != m_NumTracks )
    {
        goto exit_comparetrackinfo;
    }

    NumSessions = DiscInfo.NumberOfSessionsLsb +
                 (DiscInfo.NumberOfSessionsMsb << 8 );
    if( NumSessions != m_NumSessions )
    {
        goto exit_comparetrackinfo;
    }

    pTrackInfo = new TRACK_INFORMATION2[ NumTracks ];
    if( !pTrackInfo )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        goto exit_comparetrackinfo;
    }

    //
    // Store all the information we will need later to determine "partition"
    // information and to confirm that a disc has either been changed or
    // re-inserted.
    //
    for( USHORT Track = FirstTrack; Track <= LastTrack; Track++ )
    {
        CDB Cdb;
        TRACK_INFORMATION2 TrackInfo = { 0 };
        SENSE_DATA SenseData = { 0 };

        ZeroMemory( &Cdb, sizeof(Cdb) );
        Cdb.READ_TRACK_INFORMATION.OperationCode = SCSIOP_READ_TRACK_INFORMATION;
        Cdb.READ_TRACK_INFORMATION.Track = 1;
        Cdb.READ_TRACK_INFORMATION.BlockAddress[2] = (UCHAR)((Track >> 8) & 0xFF);
        Cdb.READ_TRACK_INFORMATION.BlockAddress[3] = (UCHAR)(Track & 0xFF);
        Cdb.READ_TRACK_INFORMATION.AllocationLength[0] = (UCHAR)((sizeof(TrackInfo) >> 8) & 0xFF);
        Cdb.READ_TRACK_INFORMATION.AllocationLength[1] = (UCHAR)(sizeof(TrackInfo) & 0xFF);

        if( !m_pDevice->SendPassThroughDirect( &Cdb,
                                               (BYTE*)&TrackInfo,
                                               sizeof(TrackInfo),
                                               TRUE,
                                               m_pDevice->GetGroupOneTimeout(),
                                               &dwBytesReturned,
                                               &SenseData ) )
        {
            goto exit_comparetrackinfo;
        }

        CopyMemory( &pTrackInfo[Track - FirstTrack], &TrackInfo, sizeof(TrackInfo) );
    }

    if( memcmp( pTrackInfo,
                m_pTrackInfo,
                sizeof(TRACK_INFORMATION2) * NumTracks ) != 0 )
    {
        goto exit_comparetrackinfo;
    }

    fResult = TRUE;

exit_comparetrackinfo:
    if( pTrackInfo )
    {
        delete [] pTrackInfo;
    }

    return fResult;
}

// -----------------------------------------------------------------------------
// Device_t
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
Device_t::Device_t()
: m_hDisc( NULL ),
  m_fIsPlayingAudio( FALSE ),
  m_Group1Timeout( 0 ),
  m_Group2Timeout( 0 ),
  m_fMediaPresent( FALSE ),
  m_MediaType( MED_TYPE_UNKNOWN ),
  m_hWakeUpEvent( NULL ),
  m_fPausePollThread( FALSE ),
  m_hTestUnitThread( NULL ),
  m_State( 0 ),
  m_MediaChangeResult( StorageMediaAttachResultUnchanged )
{
    InitializeCriticalSection( &m_cs );
}

// /////////////////////////////////////////////////////////////////////////////
// Destructor
//
Device_t::~Device_t()
{
    if( m_hWakeUpEvent )
    {
        SetEvent( m_hWakeUpEvent );

        WaitForSingleObject( m_hTestUnitThread, 6000 );

        CloseHandle( m_hWakeUpEvent );
        CloseHandle( m_hTestUnitThread );
    }

    DeleteCriticalSection( &m_cs );
}

// /////////////////////////////////////////////////////////////////////////////
// Initialize
//
// This function will attempt to gather information about any media currently in
// the device.  If there is no media in the device, it is not an error.  In this
// case we will simply report zero partitions, and no file system will be
// mounted on the media.  The polling thread will still be active and will be
// searching for media changes.  As soon as we are able to find disc information
// on a piece of media, the file system manager will tear end up tearing down
// this partition driver and opening a new one.  This time we should be able to
// correctly the query the disc for disc information and the file systems will
// be mounted.
//
// TODO::Should we report one partition by default with a zero size that will be
// mounted by a "raw" file system?
//
// If GESN is supported according to the registry settings, then register for
// events here.  Otherwise, start a thread to poll the device for media changes.
//
BOOL Device_t::Initialize( HANDLE hDisc )
{
    BOOL fResult = TRUE;
    DWORD dwBytesReturned = 0;
    DWORD dwThreadID = 0;
    CDROM_TESTUNITREADY CdReady;

    WCHAR* strDiscName = new WCHAR[MAX_PATH];
    DWORD hStore = 0;

    if( !strDiscName )
    {
        fResult = FALSE;
        goto exit_initialize;
    }

    if( m_hDisc != NULL )
    {
        //
        // This has already been initialized.  Someone is trying to open the
        // device twice which isn't currently supported.
        //
        ASSERT(FALSE);
        fResult = FALSE;
        goto exit_initialize;
    }

    m_hDisc = hDisc;

    ASSERT( hDisc != NULL );
    ASSERT( hDisc != INVALID_HANDLE_VALUE );

    m_PartMgr.Initialize( this );

    hStore = (DWORD)FSDMGR_DeviceHandleToHDSK( m_hDisc );
    if( !hStore )
    {
        fResult = FALSE;
        goto exit_initialize;
    }

    if( !FSDMGR_GetDiskName( (DWORD)hStore, strDiscName ) )
    {
        fResult = FALSE;
        goto exit_initialize;
    }

    //
    // Determine whether or not we should translate IOCTL_CDROM_READ_SG into
    // a SCSI command or just pass it directly to the block driver below.
    //
    if( !FSDMGR_GetRegistryValue( hStore, L"UseLegacyReadIOCTL", (DWORD*)&m_fUseLegacyReadIOCTL ) )
    {
        m_fUseLegacyReadIOCTL = FALSE;
    }

    //
    // Check if there is a polling frequency defined for the CD-ROM drive
    //
    if ( !FSDMGR_GetRegistryValue( hStore, CD_ROM_POLL_FREQUENCY, (DWORD*) &m_dwPollFrequency ) )
    {
        // default to 2 seconds
        m_dwPollFrequency = 2000;
    }

    if( !GetTimeoutValues() )
    {
        //
        // We'll try just using some conservative timeout values.
        //
        m_Group1Timeout = 0x2000;
        m_Group2Timeout = 0x1000;
    }

/*
    //
    // If the drive is not new enough to support MultiRead, introduced in
    // 1997, then we will not attempt to use this device.
    //
    if( !FeatureIsSupported( FeatureMultiRead ) )
    {
        DEBUGMSG( ZONE_INIT, (L"CDROM::MultiRead is not supported - try upgrading the firmware.\r\n") );
        if( strDiscName )
        {
            delete [] strDiscName;
            strDiscName = NULL;
        }

        return FALSE;
    }
*/

    //
    // If it appears that there is a disc in the device then we will try to
    // get the media type and disc information.  If we cannot do this then we
    // will still mount the CDROM partition driver, and the polling thread
    // will try to get the data at a later time.
    //
    if( TestUnitReady( &CdReady, sizeof(CdReady), &dwBytesReturned ) && CdReady.bUnitReady )
    {
        if( DetermineMediaType( &m_MediaType, sizeof(m_MediaType), &dwBytesReturned ) )
        {
            if( !m_PartMgr.RefreshPartInfo() )
            {
                m_MediaType = 0;
            }
            else if( !AdvertiseInterface( &STORAGE_MEDIA_GUID,
                                          strDiscName,
                                          TRUE ) )
            {
                if( GetLastError() != ERROR_ALREADY_EXISTS )
                {
                    DEBUGMSG(ZONE_CURRENT, (L"CDROM!Initialize - Failed to advertise STORAGE_MEDIA_GUID - TRUE.\r\n"));
                    m_MediaType = 0;
                }
                else
                {
                    DEBUGMSG(ZONE_CURRENT, (L"CDROM!Initialize - Failed to advertise STORAGE_MEDIA_GUID - TRUE (183).\r\n"));
                    m_fMediaPresent = TRUE;
                }
            }
            else
            {
                DEBUGMSG(ZONE_CURRENT, (L"CDROM!Initialize - Successfully advertised STORAGE_MEDIA_GUID - TRUE.\r\n"));
                m_fMediaPresent = TRUE;
            }
        }
    }
    else
    {
        if( AdvertiseInterface( &STORAGE_MEDIA_GUID, strDiscName, FALSE ) )
        {
            DEBUGMSG(ZONE_CURRENT, (L"CDROM!Initialize - Successfully advertised STORAGE_MEDIA_GUID - FALSE.\r\n"));
        }
        else
        {
            DEBUGMSG(ZONE_CURRENT, (L"CDROM!Initialize - Failed to advertise STORAGE_MEDIA_GUID - FALSE: %d.\r\n", GetLastError()));
        }
    }

    //
    // TODO::Read in the default cache size from the registry.
    //

    //
    // TODO::Check whether to use polling or GESN.  Defaulting to polling for now.
    //

    //
    // TODO::Add a thread priority registry option for the polling thread.
    //


    m_hWakeUpEvent = ::CreateEvent( NULL, TRUE, FALSE, NULL );
    m_hTestUnitThread = ::CreateThread( NULL,
                                        0,
                                        (LPTHREAD_START_ROUTINE)PollDeviceThread,
                                        this,
                                        0,
                                        &dwThreadID );

    fResult = (m_hTestUnitThread != NULL) && (m_hWakeUpEvent != NULL);

exit_initialize:
    if( strDiscName )
    {
        delete [] strDiscName;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// DeviceIoControl
//
BOOL Device_t::CdDeviceIoControl( USHORT PartitionIndex,
                                  DWORD IoControlCode,
                                  LPVOID pInBuf,
                                  DWORD InBufSize,
                                  LPVOID pOutBuf,
                                  DWORD OutBufSize,
                                  LPDWORD pBytesReturned,
                                  LPOVERLAPPED pOverlapped )
{
    BOOL fResult = TRUE;

    switch( IoControlCode )
    {
    case IOCTL_CDROM_GET_PARTITION_INFO:
        fResult = GetPartitionInfo( PartitionIndex,
                                    (BYTE*)pOutBuf,
                                    OutBufSize,
                                    pBytesReturned );
        break;

    default:
        //
        // These IO Controls can be called from either the storage layer or
        // through the partition layer.
        //
        fResult = CdDeviceStoreIoControl( IoControlCode,
                                          pInBuf,
                                          InBufSize,
                                          pOutBuf,
                                          OutBufSize,
                                          pBytesReturned,
                                          pOverlapped );
        break;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// DeviceStoreIoControl
//
BOOL Device_t::CdDeviceStoreIoControl( DWORD IoControlCode,
                                       LPVOID pInBuf,
                                       DWORD InBufSize,
                                       LPVOID pOutBuf,
                                       DWORD OutBufSize,
                                       LPDWORD pBytesReturned,
                                       LPOVERLAPPED pOverlapped )
{
    BOOL fResult = TRUE;

    switch( IoControlCode )
    {
    //
    // TODO::Should we reroute all PASS THROUGH commands through a function to
    // make use of the HandleCommandError function?
    //

    case IOCTL_CDROM_PAUSE_POLLING:
    {
        CD_PAUSE_POLLING* pPause = (CD_PAUSE_POLLING*)pInBuf;
        if( InBufSize < sizeof(CD_PAUSE_POLLING) ||
            pPause->cbSize != sizeof(CD_PAUSE_POLLING) )
        {
            fResult = FALSE;
            SetLastError( ERROR_INVALID_PARAMETER );
        }
        else
        {
            EnterCriticalSection( &m_cs );
            if( pPause->fPause )
            {
                if( !m_fPausePollThread )
                {
                    m_fPausePollThread = TRUE;
                }
            }
            else
            {
                if( m_fPausePollThread )
                {
                    m_fPausePollThread = FALSE;
                    SetEvent( m_hWakeUpEvent );
                }
            }
            LeaveCriticalSection( &m_cs );
        }

        break;
    }

    case IOCTL_CDROM_GET_MEDIA_TYPE:
        fResult = DetermineMediaType( pOutBuf, OutBufSize, pBytesReturned );
        break;

    case IOCTL_CDROM_READ_SG:
        fResult = ReadSGTriage( pInBuf,
                                InBufSize,
                                pOutBuf,
                                OutBufSize,
                                pBytesReturned );
        break;

    case IOCTL_CDROM_RAW_READ:
            fResult = ReadCdRaw( pInBuf,
                                 InBufSize,
                                 pOutBuf,
                                 OutBufSize,
                                 pBytesReturned );
        break;

    case IOCTL_CDROM_TEST_UNIT_READY:
        fResult = TestUnitReady( pOutBuf, OutBufSize, pBytesReturned );
        break;

    case IOCTL_CDROM_DISC_INFO:
        fResult = GetDiscInformation( pInBuf,
                                      InBufSize,
                                      pOutBuf,
                                      OutBufSize,
                                      pBytesReturned );
        break;

    case IOCTL_CDROM_EJECT_MEDIA:
        fResult = EjectMedia();
        break;

    case IOCTL_CDROM_LOAD_MEDIA:
        fResult = LoadMedia();
        break;

    case IOCTL_CDROM_ISSUE_INQUIRY:
        fResult = IssueInquiry( pOutBuf,
                                OutBufSize,
                                pBytesReturned );
        break;

    case IOCTL_CDROM_READ_TOC:
        fResult = ReadTOC( pOutBuf,
                           OutBufSize,
                           pBytesReturned );
        break;

#if 0 // TODO: Translate DVD session/key IOCTLs correctly into SCSI commands.
    case IOCTL_DVD_START_SESSION:
        fResult = StartDVDSession( pOutBuf,
                                   OutBufSize,
                                   pBytesReturned );
        break;

    case IOCTL_DVD_READ_KEY:
        fResult = ReadDVDKey( pInBuf,
                              InBufSize,
                              pOutBuf,
                              OutBufSize,
                              pBytesReturned );
        break;

    case IOCTL_DVD_END_SESSION:
        fResult = EndDVDSession( pInBuf, InBufSize );
        break;

    case IOCTL_DVD_SEND_KEY:
        fResult = SendDVDKey( pInBuf, InBufSize );
        break;

    case IOCTL_DVD_GET_REGION:
        fResult = GetRegion( pOutBuf, OutBufSize, pBytesReturned );
        break;

    case IOCTL_DVD_SET_REGION:
        //
        // This IOCTL has never been supported on CE.  However, you can always
        // just call IOCTL_DVD_SEND_KEY with a KeyType of DvdSetRPC.
        //
        return ERROR_NOT_SUPPORTED;
#endif
    case IOCTL_DVD_START_SESSION:
    case IOCTL_DVD_READ_KEY:
    case IOCTL_DVD_END_SESSION:
    case IOCTL_DVD_SEND_KEY:
    case IOCTL_DVD_GET_REGION:
    case IOCTL_DVD_SET_REGION:
        fResult = ::ForwardDeviceIoControl( m_hDisc,
                                            IoControlCode,
                                            pInBuf,
                                            InBufSize,
                                            pOutBuf,
                                            OutBufSize,
                                            pBytesReturned,
                                            pOverlapped );
        break;

    case IOCTL_CDROM_READ_Q_CHANNEL:
        fResult = ReadQSubChannel( pInBuf,
                                   InBufSize,
                                   pOutBuf,
                                   OutBufSize,
                                   pBytesReturned );
        break;

    case IOCTL_CDROM_PLAY_AUDIO_MSF:
    case IOCTL_CDROM_SEEK_AUDIO_MSF:
    case IOCTL_CDROM_RESUME_AUDIO:
    case IOCTL_CDROM_STOP_AUDIO:
    case IOCTL_CDROM_PAUSE_AUDIO:
    case IOCTL_CDROM_SCAN_AUDIO:
        fResult = ControlAudio( IoControlCode, pInBuf, InBufSize );
        break;

    default:
        fResult = ::ForwardDeviceIoControl( m_hDisc,
                                            IoControlCode,
                                            pInBuf,
                                            InBufSize,
                                            pOutBuf,
                                            OutBufSize,
                                            pBytesReturned,
                                            pOverlapped );
        break;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// SendPassThrough
//
// The data IOCTL_SCSI_PASS_THROUGH is all sent down in one combined flat
// buffer.  Usually this is reserved for sending and receiving smaller amounts
// of data, such as 16K or less.  IOCTL_SCSI_PASS_THROUGH_DIRECT allows for
// a pointer to a data buffer and can be used to send larger amounts of data
// without double allocated memory for the data.
//
BOOL Device_t::SendPassThrough( PCDB pCdb,
                                __in_ecount_opt(dwBufferSize) BYTE* pDataBuffer,
                                DWORD dwBufferSize,
                                BOOL fDataIn,
                                DWORD dwTimeout,
                                DWORD* pdwBytesReturned,
                                PSENSE_DATA pSenseData )
{
    BOOL fResult = TRUE;
    BYTE* pBuffer = NULL;
    DWORD dwSize = 0;
    PSCSI_PASS_THROUGH pPassThrough = NULL;
    DWORD dwBytesReturned = 0;
    DWORD dwRetryCount = CMD_RETRY_COUNT;
    BOOL fRetry = FALSE;

    ASSERT( pdwBytesReturned != NULL );

    if( dwBufferSize && !pDataBuffer )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_sendpassthrough;
    }

    //
    // Make sure that we won't overflow on a 16-bit variable.
    //
    if( USHRT_MAX - sizeof(SCSI_PASS_THROUGH) - sizeof(SENSE_DATA) < dwBufferSize )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_sendpassthrough;
    }

    dwSize = dwBufferSize +
             sizeof(SCSI_PASS_THROUGH) +
             sizeof(SENSE_DATA);

    pBuffer = new BYTE[dwSize];
    if( !pBuffer )
    {
        fResult = FALSE;
        goto exit_sendpassthrough;
    }

    ZeroMemory( pBuffer, dwSize );

    pPassThrough = (PSCSI_PASS_THROUGH)pBuffer;

    if( !CeSafeCopyMemory( &pPassThrough->Cdb, pCdb, sizeof(CDB) ) )
    {
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_sendpassthrough;
    }

    //
    // See the SCSI Architecture Model (SAM-2) for an explanation of this.
    //
    switch( (pCdb->AsByte[0] >> 5) & 0x7 )
    {
    case 0:
        pPassThrough->CdbLength = 6;
        break;

    case 1:
    case 2:
        pPassThrough->CdbLength = 10;
        break;

    case 5:
        pPassThrough->CdbLength = 12;
        break;

    default:
        ASSERT(FALSE);
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_sendpassthrough;
    }

    //
    // If DataIn is set, then we are reading data from the device.
    //
    pPassThrough->DataIn = fDataIn ? SCSI_IOCTL_DATA_IN : SCSI_IOCTL_DATA_OUT;

    //
    // TODO::
    // I know that the SCSI_PASS_THROUGH_DIRECT documentation on MSDN does not
    // match the actual behavior on the desktop.  The Length field should be
    // set to the size of the SCSI_PASS_THROUGH_DIRECT structure and should not
    // include the size of accompanying data and request sense buffers.  Is that
    // true for SCSI_PASS_THROUGH as well?
    //
    pPassThrough->Length = (USHORT)dwSize;
    pPassThrough->DataTransferLength = dwBufferSize;
    pPassThrough->DataBufferOffset = sizeof(SCSI_PASS_THROUGH);
    pPassThrough->SenseInfoLength = sizeof(SENSE_DATA);
    pPassThrough->SenseInfoOffset = sizeof(SCSI_PASS_THROUGH) + dwBufferSize;
    pPassThrough->TimeOutValue = dwTimeout;

    if( !fDataIn && pBuffer )
    {
        //
        // We are writing data to the device and we need to copy the data from
        // the user mode to the kernel mode buffer prior to sending the command.
        //
        if( !CeSafeCopyMemory( pBuffer + pPassThrough->DataBufferOffset,
                               pDataBuffer,
                               dwBufferSize ) )
        {
            fResult = FALSE;
            SetLastError( ERROR_INVALID_PARAMETER );
            goto exit_sendpassthrough;
        }
    }

    do
    {
        fRetry = FALSE;
        fResult = DeviceIoControl( m_hDisc,
                                   IOCTL_SCSI_PASS_THROUGH,
                                   pPassThrough,
                                   pPassThrough->Length,
                                   pPassThrough,
                                   pPassThrough->Length,
                                   &dwBytesReturned,
                                   NULL );

        if( !fResult &&
            dwRetryCount &&
            HandleCommandError( pCdb,
                                (SENSE_DATA*)(pBuffer + pPassThrough->SenseInfoOffset) ) == RESULT_RETRY )
        {
            fRetry = TRUE;
            dwRetryCount -= 1;
        }
    } while( fRetry );

    if( fResult && fDataIn && pBuffer && pDataBuffer )
    {
        if( !CeSafeCopyMemory( pDataBuffer,
                               pBuffer + pPassThrough->DataBufferOffset,
                               dwBufferSize ) )
        {
            fResult = FALSE;
            SetLastError( ERROR_INVALID_PARAMETER );
            goto exit_sendpassthrough;
        }

        *pdwBytesReturned = dwBufferSize;
    }

    if( pSenseData )
    {
        CeSafeCopyMemory( pSenseData,
                          pBuffer + pPassThrough->SenseInfoOffset,
                          sizeof(SENSE_DATA) );
    }

 exit_sendpassthrough:
    if( pBuffer )
    {
        delete [] pBuffer;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// SendPassThroughDirect
//
// Pass through direct uses the user's buffer instead of allocating one here.
// It is passed down as a pointer in the SCSI_PASS_THROUGH_DIRECT structure.
// This is used for larger buffers so that we don't have to re-allocate the
// buffer.
//
BOOL Device_t::SendPassThroughDirect( PCDB pCdb,
                                      BYTE* pDataBuffer,
                                      DWORD dwBufferSize,
                                      BOOL fDataIn,
                                      DWORD dwTimeout,
                                      DWORD* pdwBytesReturned,
                                      PSENSE_DATA pSenseData )
{
    BOOL fResult = TRUE;
    PASS_THROUGH_DIRECT PassThroughDirect = { 0 };
    SCSI_PASS_THROUGH_DIRECT& PassThrough = PassThroughDirect.PassThrough;
    DWORD dwBytesReturned = 0;
    DWORD dwRetryCount = CMD_RETRY_COUNT;
    BOOL fRetry = FALSE;

    if( !pCdb )
    {
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_sendpassthroughdirect;
    }

    if( !CeSafeCopyMemory( &PassThrough.Cdb[0], pCdb, sizeof(CDB) ) )
    {
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_sendpassthroughdirect;
    }

    //
    // See the SCSI Architecture Model (SAM-2) for an explanation of this.
    //
    switch( (pCdb->AsByte[0] >> 5) & 0x7 )
    {
    case 0:
        PassThrough.CdbLength = 6;
        break;

    case 1:
    case 2:
        PassThrough.CdbLength = 10;
        break;

    case 5:
        PassThrough.CdbLength = 12;
        break;

    default:
        ASSERT(FALSE);
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_sendpassthroughdirect;
    }

    //
    // I know that the SCSI_PASS_THROUGH_DIRECT documentation on MSDN does not
    // match the actual behavior on the desktop.  The Length field should be
    // set to the size of the SCSI_PASS_THROUGH_DIRECT structure and should not
    // include the size of accompanying data and request sense buffers.
    //
    PassThrough.Length = sizeof(PassThrough);

    //
    // If DataIn is set, then we are reading data from the device.
    //
    PassThrough.DataIn = fDataIn ? SCSI_IOCTL_DATA_IN : SCSI_IOCTL_DATA_OUT;
    PassThrough.DataTransferLength = dwBufferSize;
    PassThrough.DataBuffer = pDataBuffer;
    PassThrough.SenseInfoLength = sizeof(SENSE_DATA);
    PassThrough.SenseInfoOffset = sizeof(PassThrough);
    PassThrough.TimeOutValue = dwTimeout;

    do
    {
        fRetry = FALSE;
        __try
        {
            fResult = DeviceIoControl( m_hDisc,
                                       IOCTL_SCSI_PASS_THROUGH_DIRECT,
                                       &PassThroughDirect,
                                       sizeof(PassThroughDirect),
                                       &PassThroughDirect,
                                       sizeof(PassThroughDirect),
                                       &dwBytesReturned,
                                       NULL );

            *pdwBytesReturned = PassThroughDirect.PassThrough.DataTransferLength;
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
            goto exit_sendpassthroughdirect;
        }

        if( fResult == FALSE &&
            dwRetryCount &&
            HandleCommandError( pCdb, &PassThroughDirect.SenseData ) == RESULT_RETRY )
        {
            fRetry = TRUE;
            dwRetryCount -= 1;
        }
    } while( fRetry );

    if( pSenseData )
    {
        CeSafeCopyMemory( pSenseData,
                          &PassThroughDirect.SenseData,
                          sizeof(SENSE_DATA) );
    }



exit_sendpassthroughdirect:

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// FeatureIsActive
//
// This version of FeatureIsActive will actually query the underlying device for
// the given feature.  It is not as efficient
//
BOOL Device_t::FeatureIsActive( FEATURE_NUMBER Feature )
{
    BOOL fActive = FALSE;
    PGET_CONFIGURATION_HEADER FeatureHeader = NULL;
    PFEATURE_HEADER pHeader = NULL;
    SENSE_DATA SenseData = { 0 };
    ULONG DataLength = 0;
    USHORT usCurFeature = 0;

    DWORD dwBytesReturned = 0;
    BYTE Buffer[32];
    CDB Cdb = { 0 };

    ZeroMemory( Buffer, sizeof(Buffer) );

    Cdb.GET_CONFIGURATION.OperationCode = SCSIOP_GET_CONFIGURATION;
    Cdb.GET_CONFIGURATION.RequestType = SCSI_GET_CONFIGURATION_REQUEST_TYPE_CURRENT;
    Cdb.GET_CONFIGURATION.StartingFeature[0] = (UCHAR)(Feature >> 8);
    Cdb.GET_CONFIGURATION.StartingFeature[1] = (UCHAR)(Feature & 0xff);
    Cdb.GET_CONFIGURATION.AllocationLength[0] = (UCHAR)(sizeof(Buffer) >> 8);
    Cdb.GET_CONFIGURATION.AllocationLength[1] = (UCHAR)(sizeof(Buffer) & 0xff);

    SendPassThrough( &Cdb,
                     Buffer,
                     sizeof(Buffer),
                     TRUE,
                     50,         // TODO::This is just a random number.  Change it.
                     &dwBytesReturned,
                     &SenseData );

    FeatureHeader = (PGET_CONFIGURATION_HEADER)Buffer;
    DataLength = (FeatureHeader->DataLength[0] << 24) |
                 (FeatureHeader->DataLength[1] << 16) |
                 (FeatureHeader->DataLength[2] << 8) |
                 (FeatureHeader->DataLength[3]);

    if( DataLength > 4 )
    {
        pHeader = (PFEATURE_HEADER)(FeatureHeader + 1);
        usCurFeature = (pHeader->FeatureCode[0] << 8) |
                       (pHeader->FeatureCode[1]);
        if( usCurFeature == Feature )
        {
            fActive = TRUE;
        }
    }

    return fActive;
}

// /////////////////////////////////////////////////////////////////////////////
// FeatureIsSupported
//
// We are given a list of active features and we must simply search through them
// for the requested feature.  This is done to save extra requests to the device
// for each feature in question.
//
BOOL Device_t::FeatureIsActive( FEATURE_NUMBER Feature,
                                BYTE* pBuffer,
                                DWORD BufferSize )
{
    bool fFeatureIsActive = false;

    PGET_CONFIGURATION_HEADER ConfigHeader = NULL;
    PFEATURE_HEADER pFeatureHeader = NULL;
    DWORD DataLength = 0;
    DWORD BufferOffset = 0;

    ConfigHeader = (PGET_CONFIGURATION_HEADER)pBuffer;
    DataLength = (ConfigHeader->DataLength[0] << 24) |
                 (ConfigHeader->DataLength[1] << 16) |
                 (ConfigHeader->DataLength[2] << 8) |
                 (ConfigHeader->DataLength[3]);

    DataLength -= sizeof(GET_CONFIGURATION_HEADER) - sizeof(DataLength);

    if( DataLength > BufferSize )
    {
        //wprintf( L"The data length value in the configuration header is larger than the available buffer size.\n" );
        DataLength = BufferSize - sizeof(GET_CONFIGURATION_HEADER );
    }

    BufferOffset = sizeof(GET_CONFIGURATION_HEADER);

    while( !fFeatureIsActive && DataLength )
    {
        USHORT FeatureCode = 0;
        pFeatureHeader = (PFEATURE_HEADER)(pBuffer + BufferOffset);

        FeatureCode = pFeatureHeader->FeatureCode[1] + (pFeatureHeader->FeatureCode[0] << 8);

        if( pFeatureHeader->AdditionalLength % 4 )
        {
            //wprintf( L"The feature code for feature %d has an additional length that is not a multiple of 4.\n", FeatureCode );
        }

        if( (FEATURE_NUMBER)FeatureCode == Feature )
        {
            fFeatureIsActive = true;
        }

        BufferOffset += sizeof(FEATURE_HEADER) + pFeatureHeader->AdditionalLength;
        if( (sizeof(FEATURE_HEADER) + pFeatureHeader->AdditionalLength) >
            DataLength )
        {
            DEBUGMSG( ZONE_IO, (L"CDROM::Feature lengths != DataLength.\r\n") );
            DataLength = 0;
        }
        else
        {
            DataLength -= sizeof(FEATURE_HEADER) + pFeatureHeader->AdditionalLength;
        }
    }

    return fFeatureIsActive;
}

// /////////////////////////////////////////////////////////////////////////////
// DumpActiveFeatures
//
BOOL Device_t::DumpActiveFeatures( BYTE* pBuffer, DWORD BufferSize )
{
    PGET_CONFIGURATION_HEADER ConfigHeader = NULL;
    PFEATURE_HEADER pFeatureHeader = NULL;
    DWORD DataLength = 0;
    DWORD BufferOffset = 0;

    ConfigHeader = (PGET_CONFIGURATION_HEADER)pBuffer;
    DataLength = (ConfigHeader->DataLength[0] << 24) |
                 (ConfigHeader->DataLength[1] << 16) |
                 (ConfigHeader->DataLength[2] << 8) |
                 (ConfigHeader->DataLength[3]);

    DataLength -= sizeof(GET_CONFIGURATION_HEADER) - sizeof(DataLength);

    if( DataLength > BufferSize )
    {
        DataLength = BufferSize - sizeof(GET_CONFIGURATION_HEADER );
    }

    BufferOffset = sizeof(GET_CONFIGURATION_HEADER);

    while( DataLength )
    {
        USHORT FeatureCode = 0;
        pFeatureHeader = (PFEATURE_HEADER)(pBuffer + BufferOffset);

        FeatureCode = pFeatureHeader->FeatureCode[1] + (pFeatureHeader->FeatureCode[0] << 8);

        BufferOffset += sizeof(FEATURE_HEADER) + pFeatureHeader->AdditionalLength;

        if( (sizeof(FEATURE_HEADER) + pFeatureHeader->AdditionalLength) >
            DataLength )
        {
            DEBUGMSG( ZONE_IO, (L"CDROM::Feature lengths != DataLength.\r\n") );
            DataLength = 0;
        }
        else
        {
            DataLength -= sizeof(FEATURE_HEADER) + pFeatureHeader->AdditionalLength;
        }
    }

    return TRUE;
}

// /////////////////////////////////////////////////////////////////////////////
// FeatureIsSupported
//
BOOL Device_t::FeatureIsSupported( FEATURE_NUMBER Feature )
{
    BOOL fSupported = FALSE;
    PGET_CONFIGURATION_HEADER FeatureHeader = NULL;
    PFEATURE_HEADER pHeader = NULL;
    SENSE_DATA SenseData = { 0 };
    ULONG DataLength = 0;
    USHORT usCurFeature = 0;

    DWORD dwBytesReturned = 0;
    BYTE Buffer[32];
    CDB Cdb = { 0 };

    ZeroMemory( Buffer, sizeof(Buffer) );

    Cdb.GET_CONFIGURATION.OperationCode = SCSIOP_GET_CONFIGURATION;
    Cdb.GET_CONFIGURATION.RequestType = SCSI_GET_CONFIGURATION_REQUEST_TYPE_ONE;
    Cdb.GET_CONFIGURATION.StartingFeature[0] = (UCHAR)(Feature >> 8);
    Cdb.GET_CONFIGURATION.StartingFeature[1] = (UCHAR)(Feature & 0xff);
    Cdb.GET_CONFIGURATION.AllocationLength[0] = (UCHAR)(sizeof(Buffer) >> 8);
    Cdb.GET_CONFIGURATION.AllocationLength[1] = (UCHAR)(sizeof(Buffer) & 0xff);

    SendPassThrough( &Cdb,
                     Buffer,
                     sizeof(Buffer),
                     TRUE,
                     500,         // TODO::This is just a random number.  Change it.
                     &dwBytesReturned,
                     &SenseData );

    FeatureHeader = (PGET_CONFIGURATION_HEADER)Buffer;
    DataLength = (FeatureHeader->DataLength[0] << 24) |
                 (FeatureHeader->DataLength[1] << 16) |
                 (FeatureHeader->DataLength[2] << 8) |
                 (FeatureHeader->DataLength[3]);

    if( DataLength > 4 )
    {
        pHeader = (PFEATURE_HEADER)(FeatureHeader + 1);
        usCurFeature = (pHeader->FeatureCode[0] << 8) |
                       (pHeader->FeatureCode[1]);
        if( usCurFeature == Feature )
        {
            fSupported = TRUE;
        }
    }

    if (!fSupported)
    {
        SetLastError( ERROR_NOT_SUPPORTED );
    }

    return fSupported;
}

// /////////////////////////////////////////////////////////////////////////////
// GetTimeoutValues
//
BOOL Device_t::GetTimeoutValues()
{
    const DWORD BUFFER_SIZE = sizeof(CDVD_INACTIVITY_TIMEOUT_PAGE) +
                              sizeof(MODE_HEADER_LENGTH10) +
                              20;
    BOOL fResult = TRUE;
    CDB Cdb = { 0 };
    BYTE Buffer[BUFFER_SIZE] = { 0 };
    PMODE_PARAMETER_HEADER10 pHeader = (PMODE_PARAMETER_HEADER10)Buffer;
    PCDVD_INACTIVITY_TIMEOUT_PAGE pPage = NULL;
    SENSE_DATA SenseData= { 0 };
    DWORD dwBytesReturned = 0;

    BOOL fEnableTimeOut = FALSE;

    DEBUGMSG( ZONE_IO, (L"CDROM::GetTimeoutValues entered.\r\n") );

    Cdb.MODE_SENSE10.OperationCode = SCSIOP_MODE_SENSE10;
    Cdb.MODE_SENSE10.PageCode = MODE_PAGE_CDVD_INACTIVITY;
    Cdb.MODE_SENSE10.AllocationLength[0] = (sizeof(CDVD_INACTIVITY_TIMEOUT_PAGE) >> 8) & 0xFF;
    Cdb.MODE_SENSE10.AllocationLength[1] = (sizeof(CDVD_INACTIVITY_TIMEOUT_PAGE)) & 0xFF;

    fResult = SendPassThrough( &Cdb,
                               Buffer,
                               BUFFER_SIZE,
                               TRUE,
                               0x2000, // TODO::This is just a random number.  Change it.
                               &dwBytesReturned,
                               &SenseData );

    if( fResult )
    {
        if( pHeader->BlockDescriptorLength[0] == 0 &&
            pHeader->BlockDescriptorLength[1] == 0 )
        {
            pPage = (PCDVD_INACTIVITY_TIMEOUT_PAGE)(Buffer + sizeof(MODE_PARAMETER_HEADER10));
            if( pPage->TMOE == 0 )
            {
                fEnableTimeOut = TRUE;
            }

            m_Group1Timeout = (pPage->GroupOneMinimumTimeout[0] << 8) +
                              (pPage->GroupOneMinimumTimeout[1]);
            m_Group2Timeout = (pPage->GroupTwoMinimumTimeout[0] << 8) +
                              (pPage->GroupTwoMinimumTimeout[1]);

            if( m_Group1Timeout == 0 )
            {
                m_Group1Timeout = 0x1000;
                pPage->GroupOneMinimumTimeout[0] = 0x10;
                pPage->GroupOneMinimumTimeout[1] = 0x00;
            }

            if( m_Group2Timeout == 0 )
            {
                m_Group2Timeout = 0x1000;
                pPage->GroupTwoMinimumTimeout[0] = 0x10;
                pPage->GroupTwoMinimumTimeout[1] = 0x10;
            }
        }
        else
        {
            DEBUGMSG( ZONE_WARNING, (L"CDROM: Device is returning block descriptors on mode sense.\r\n") );
        }
    }

    //
    // Enable timeout error for group one calls.
    //
    if( fEnableTimeOut )
    {
        ZeroMemory( &Cdb, sizeof(Cdb) );
        Cdb.MODE_SELECT10.OperationCode = SCSIOP_MODE_SELECT10;
        Cdb.MODE_SELECT10.ParameterListLength[0] = (sizeof(CDVD_INACTIVITY_TIMEOUT_PAGE) >> 8) & 0xFF;
        Cdb.MODE_SELECT10.ParameterListLength[1] = sizeof(CDVD_INACTIVITY_TIMEOUT_PAGE) & 0xFF;
        Cdb.MODE_SELECT10.SPBit = pPage->PSBit;

        pPage->TMOE = 1;

        fResult = SendPassThrough( &Cdb,
                                   (BYTE*)pPage,
                                   sizeof(CDVD_INACTIVITY_TIMEOUT_PAGE),
                                   FALSE,
                                   50, // TODO::This is just a random number.  Change it.
                                   &dwBytesReturned,
                                   &SenseData );
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetBlockingFactor
//
//
DWORD Device_t::GetBlockingFactor() const
{
    DWORD dwBlockingFactor = 1;

    if( m_MediaType == MED_TYPE_CD_RAM )
    {
        //
        // The blocking factor for IoMega REV devices is actually 64.
        //
        dwBlockingFactor = 64;
    }
    else
    {
        dwBlockingFactor = m_MediaType & MED_FLAG_DVD ? 16 : 32;
    }

    return dwBlockingFactor;
}

// /////////////////////////////////////////////////////////////////////////////
// GetMediaType
//
BOOL Device_t::DetermineMediaType( PVOID pOutBuffer,
                                   DWORD dwOutBufSize,
                                   DWORD* pdwBytesReturned  )
{
    DWORD dwMediaType = MED_TYPE_UNKNOWN;
    BYTE* pCurrentFeatures = NULL;
    DWORD BufferSize = 0;

    BOOL fResult = TRUE;
    BOOL fRandomWrite = FALSE;
    BOOL fMRW = FALSE;
    BOOL fFormattable = FALSE;
    BOOL fDefectManaged = FALSE;
    BOOL fRestrictedOverwrite = FALSE;
    BOOL fIncrementalWrite = FALSE;
    BOOL fReadDVD = FALSE;
    BOOL fDvdMinusWrite = FALSE;
    BOOL fPlusRW = FALSE;
    BOOL fPlusR = FALSE;
    BOOL fMastering = FALSE;
    BOOL fDvdRecordableWrite = FALSE;
    BOOL fReadCD = FALSE;

    DEBUGMSG( ZONE_IO, (L"CDROM::DetermineMediaType entered.\r\n") );

    if( pOutBuffer == NULL )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_getmediatype;
    }

    if( dwOutBufSize < sizeof( dwMediaType ) )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_getmediatype;
    }

    //
    // This way we try to get all the features in one go instead of requerying
    // the drive for each feature.  However, this is untested on many drives,
    // and though it seems like some basic functionality, we've yet to see how
    // many drives handle it correctly.
    //
    if( !GetCurrentFeatures( &pCurrentFeatures, &BufferSize ) )
    {
        fResult = FALSE;
        goto exit_getmediatype;
    }

    ASSERT( DumpActiveFeatures( pCurrentFeatures, BufferSize ) );

    //
    // Now check for some features.
    //
    fReadCD              = FeatureIsActive( FeatureCdRead,
                                            pCurrentFeatures,
                                            BufferSize );
    fRandomWrite         = FeatureIsActive( FeatureRandomWritable,
                                            pCurrentFeatures,
                                            BufferSize );
    fMRW                 = FeatureIsActive( FeatureMrw,
                                            pCurrentFeatures,
                                            BufferSize );
    fFormattable         = FeatureIsActive( FeatureFormattable,
                                            pCurrentFeatures,
                                            BufferSize );
    fDefectManaged       = FeatureIsActive( FeatureDefectManagement,
                                            pCurrentFeatures,
                                            BufferSize );
    fRestrictedOverwrite = FeatureIsActive( FeatureRestrictedOverwrite,
                                            pCurrentFeatures,
                                            BufferSize );
    fIncrementalWrite    = FeatureIsActive( FeatureIncrementalStreamingWritable,
                                            pCurrentFeatures,
                                            BufferSize );
    fReadDVD             = FeatureIsActive( FeatureDvdRead,
                                            pCurrentFeatures,
                                            BufferSize );
    fDvdRecordableWrite  = FeatureIsActive( FeatureDvdRecordableWrite,
                                            pCurrentFeatures,
                                            BufferSize );
    fPlusR               = FeatureIsActive( (FEATURE_NUMBER)FeatureDvdPlusR,
                                            pCurrentFeatures,
                                            BufferSize );
    fPlusRW              = FeatureIsActive( FeatureDvdPlusRW,
                                            pCurrentFeatures,
                                            BufferSize );
    fMastering           = FeatureIsActive( FeatureCdMastering,
                                            pCurrentFeatures,
                                            BufferSize );
    fDvdMinusWrite       = FeatureIsActive( FeatureDvdRecordableWrite,
                                            pCurrentFeatures,
                                            BufferSize );

    if( fReadDVD || fDvdMinusWrite || fPlusR || fPlusRW ) // If DVD Media
    {
        if( fDefectManaged )
        {
            if( fMRW )
            {
                dwMediaType = MED_TYPE_DVD_PLUS_MRW;
            }
            else
            {
                dwMediaType = MED_TYPE_DVD_RAM;
            }
        }
        else // Not defect managed.
        {
            if( fFormattable )
            {
                if( fPlusRW )
                {
                    dwMediaType = MED_TYPE_DVD_PLUS_RW;
                }
                else
                {
                    dwMediaType = MED_TYPE_DVD_MINUS_RW;
                }
            }
            else // Not formattable
            {
                if( fPlusR )
                {
                    dwMediaType = MED_TYPE_DVD_PLUS_R;
                }
                else if( fDvdRecordableWrite )
                {
                    dwMediaType = MED_TYPE_DVD_R;
                }
                else
                {
                    dwMediaType = MED_TYPE_DVD_ROM;
                }
            }
        }
    }
    else // CD Media
    {
        if( fDefectManaged )
        {
            if( fMRW )
            {
                dwMediaType = MED_TYPE_CD_MRW;
            }
            else
            {
                dwMediaType = MED_TYPE_CD_RAM; // Iomega cartridge
            }
        }
        else if( fFormattable || fRestrictedOverwrite )
        {
            dwMediaType = MED_TYPE_CD_RW;
        }
        else
        {
            if( fMastering || fIncrementalWrite )
            {
                dwMediaType = MED_TYPE_CD_R;
            }
            else if( fReadCD )
            {
                dwMediaType = MED_TYPE_CD_ROM;
            }
        }
    }

    __try
    {
        CopyMemory( pOutBuffer, &dwMediaType, sizeof( dwMediaType ) );
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_getmediatype;
    }

exit_getmediatype:
    if( pCurrentFeatures )
    {
        delete [] pCurrentFeatures;
    }

    return fResult;
}


/*
BOOL Device_t::GetMediaType( PVOID pOutBuffer,
                             DWORD dwOutBufSize,
                             DWORD* pdwBytesReturned )
{
    DWORD dwMediaType = 0;

    BOOL fResult = TRUE;
    BOOL fRandomWrite = FALSE;
    BOOL fMRW = FALSE;
    BOOL fFormattable = FALSE;
    BOOL fDefectManaged = FALSE;
    BOOL fRestrictedOverwrite = FALSE;
    BOOL fIncrementalWrite = FALSE;
    BOOL fReadDVD = FALSE;
    BOOL fDvdMinusWrite = FALSE;
    BOOL fPlusRW = FALSE;
    BOOL fPlusR = FALSE;
    BOOL fMastering = FALSE;

    if( pOutBuffer == NULL )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_getmediatype;
    }

    if( dwOutBufSize < sizeof( dwMediaType ) )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_getmediatype;
    }

    //
    // Now check for some features.
    //
    fRandomWrite = FeatureIsActive( FeatureRandomWritable );
    fMRW = FeatureIsActive( FeatureMrw );
    fFormattable = FeatureIsActive( FeatureFormattable );
    fDefectManaged = FeatureIsActive( FeatureDefectManagement );
    fRestrictedOverwrite = FeatureIsActive( FeatureRestrictedOverwrite );
    fIncrementalWrite = FeatureIsActive( FeatureIncrementalStreamingWritable );
    fReadDVD = FeatureIsActive( FeatureDvdRead );

    //
    // One drive doesn't report DVD-Read, but does report write
    // for DVD-RW drives.
    //
    fDvdMinusWrite = FeatureIsActive( FeatureDvdRecordableWrite );

    if( fReadDVD || fDvdMinusWrite ) // If DVD Media
    {
        if( fDefectManaged )
        {
            if( fMRW )
            {
                dwMediaType = MED_TYPE_DVD_PLUS_MRW;
            }
            else
            {
                dwMediaType = MED_TYPE_DVD_RAM;
            }
        }
        else // Not defect managed.
        {
            if( fFormattable )
            {
                fPlusRW = FeatureIsActive( FeatureDvdPlusRW );
                if( fPlusRW )
                {
                    dwMediaType = MED_TYPE_DVD_PLUS_RW;
                }
                else
                {
                    dwMediaType = MED_TYPE_DVD_MINUS_RW;
                }
            }
            else // Not restricted overwrite
            {
                fPlusR = FeatureIsActive( FeatureDvdPlusR );
                if( fPlusR )
                {
                    dwMediaType = MED_TYPE_DVD_PLUS_R;
                }
                else
                {
                    dwMediaType = MED_TYPE_DVD_PLUS_R;
                }
            }
        }
    }
    else // CD Media
    {
        if( fDefectManaged )
        {
            if( fMRW )
            {
                dwMediaType = MED_TYPE_CD_MRW;
            }
            else
            {
                dwMediaType = MED_TYPE_CD_RAM; // Iomega cartridge
            }
        }
        else if( fFormattable || fRestrictedOverwrite )
        {
            dwMediaType = MED_TYPE_CD_RW;
        }
        else
        {
            fMastering = FeatureIsActive( FeatureCdMastering );
            if( fMastering || fIncrementalWrite )
            {
                dwMediaType = MED_TYPE_CD_R;
            }
            else
            {
                dwMediaType = MED_TYPE_CD_ROM;
            }
        }
    }

    __try
    {
        CopyMemory( pOutBuffer, &dwMediaType, sizeof( dwMediaType ) );
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_getmediatype;
    }

 exit_getmediatype:

    return fResult;
}
*/

// /////////////////////////////////////////////////////////////////////////////
// GetPartitionInfo
//
BOOL Device_t::GetPartitionInfo( USHORT Partition,
                                 BYTE* pOutBuffer,
                                 DWORD dwOutBufSize,
                                 DWORD* pdwBytesReturned )
{
    return m_PartMgr.GetSessionInfo( Partition,
                                     (PCD_PARTITION_INFO)pOutBuffer,
                                     dwOutBufSize,
                                     pdwBytesReturned );
}

// /////////////////////////////////////////////////////////////////////////////
// GetCurrentFeatures
//
BOOL Device_t::GetCurrentFeatures( BYTE** ppBuffer, DWORD* pBufferSize )
{
    BOOL fResult = TRUE;

    GET_CONFIGURATION_HEADER ConfigHeader = { 0 };
    CDB Cdb = { 0 };
    SENSE_DATA SenseData = { 0 };
    DWORD dwBytesReturned = 0;

    BYTE* pBuffer = NULL;
    DWORD BufferSize = 0;

    PGET_CONFIGURATION_HEADER pConfigHeader = NULL;
    DWORD DataLength = 0;

    Cdb.GET_CONFIGURATION.OperationCode = SCSIOP_GET_CONFIGURATION;
    Cdb.GET_CONFIGURATION.RequestType = SCSI_GET_CONFIGURATION_REQUEST_TYPE_CURRENT;
    Cdb.GET_CONFIGURATION.StartingFeature[0] = 0;
    Cdb.GET_CONFIGURATION.StartingFeature[1] = 3;
    Cdb.GET_CONFIGURATION.AllocationLength[0] = (UCHAR)((sizeof(ConfigHeader) >> 8) & 0xFF);
    Cdb.GET_CONFIGURATION.AllocationLength[1] = (UCHAR)(sizeof(ConfigHeader) & 0xFF);

    if( !SendPassThrough( &Cdb,
                          (BYTE*)&ConfigHeader,
                          sizeof(ConfigHeader),
                          TRUE,
                          50,         // TODO::This is just a random number.  Change it.
                          &dwBytesReturned,
                          &SenseData ) )
    {
        BufferSize = 4096;
    }
    else
    {
        BufferSize = (ConfigHeader.DataLength[0] << 24) +
                     (ConfigHeader.DataLength[1] << 16) +
                     (ConfigHeader.DataLength[2] << 8) +
                     (ConfigHeader.DataLength[3]) +
                     sizeof(ConfigHeader.DataLength);

        if( BufferSize < sizeof(ConfigHeader) )
        {
            SetLastError( ERROR_IO_DEVICE );
            fResult = FALSE;
            goto exit_getcurrentfeatures;
        }

    }

    pBuffer = new BYTE[BufferSize];
    if( !pBuffer )
    {
        fResult = FALSE;
        goto exit_getcurrentfeatures;
    }

    ZeroMemory( pBuffer, BufferSize );

    Cdb.GET_CONFIGURATION.OperationCode = SCSIOP_GET_CONFIGURATION;
    Cdb.GET_CONFIGURATION.RequestType = SCSI_GET_CONFIGURATION_REQUEST_TYPE_CURRENT;
    Cdb.GET_CONFIGURATION.StartingFeature[0] = 0;
    Cdb.GET_CONFIGURATION.StartingFeature[1] = 3;
    Cdb.GET_CONFIGURATION.AllocationLength[0] = (UCHAR)((BufferSize >> 8) & 0xFF);
    Cdb.GET_CONFIGURATION.AllocationLength[1] = (UCHAR)(BufferSize & 0xFF);

    if( !SendPassThrough( &Cdb,
                          pBuffer,
                          BufferSize,
                          TRUE,
                          50,         // TODO::This is just a random number.  Change it.
                          &dwBytesReturned,
                          &SenseData ) )
    {
        fResult = FALSE;
        goto exit_getcurrentfeatures;
    }

    pConfigHeader = (PGET_CONFIGURATION_HEADER)pBuffer;
    BufferSize = (pConfigHeader->DataLength[0] << 24) +
                 (pConfigHeader->DataLength[1] << 16) +
                 (pConfigHeader->DataLength[2] << 8) +
                 (pConfigHeader->DataLength[3]);

    //
    // The DataLength field is the size of the data to be returned following
    // the DataLength field iteself.  This must be at least the size of the
    // ConfigHeader minus the size of the DataLength field or it is worthless.
    //
    if( BufferSize < (sizeof(ConfigHeader) - sizeof(ConfigHeader.DataLength)) )
    {
        SetLastError( ERROR_IO_DEVICE );
        fResult = FALSE;
        goto exit_getcurrentfeatures;
    }

    *pBufferSize = dwBytesReturned;
    *ppBuffer = pBuffer;

exit_getcurrentfeatures:
    if( !fResult && pBuffer )
    {
        delete [] pBuffer;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ReadSGTriage
//
BOOL Device_t::ReadSGTriage( PVOID pInBuffer,
                             DWORD dwInBufSize,
                             PVOID pOutBuffer,
                             DWORD dwOutBufSize,
                             DWORD* pdwBytesReturned )
{
    BOOL fResult = TRUE;
    CDROM_READ CdRead;
    PSGX_BUF pBuffers = NULL;
    DWORD dwRequestedBytes = 0;
    BOOL fUserMode = ::GetDirectCallerProcessId() != ::GetCurrentProcessId();

    if( !pInBuffer )
    {
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_readsgtriage;
    }

    if( dwInBufSize < sizeof(CDROM_READ) )
    {
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_readsgtriage;
    }

    if( !CeSafeCopyMemory( &CdRead, pInBuffer, sizeof(CDROM_READ) ) )
    {
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_readsgtriage;
    }

    if( m_fUseLegacyReadIOCTL && !CdRead.bRawMode )
    {
        fResult = ::ForwardDeviceIoControl( m_hDisc,
                                            IOCTL_CDROM_READ_SG,
                                            pInBuffer,
                                            dwInBufSize,
                                            pOutBuffer,
                                            dwOutBufSize,
                                            pdwBytesReturned,
                                            NULL );
        goto exit_readsgtriage;
    }

    //
    // The user could change the locations of the buffers they sent at any
    // time on another thread.  We must make a copy of the current locations
    // and validate that they are ok.
    //
    pBuffers = new SGX_BUF[ CdRead.sgcount ];
    if( !pBuffers )
    {
        fResult = FALSE;
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        goto exit_readsgtriage;
    }

    if( !CeSafeCopyMemory( pBuffers,
                           &((CDROM_READ*)pInBuffer)->sglist[0],
                           sizeof(SGX_BUF) * CdRead.sgcount ) )
    {
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_readsgtriage;
    }

    //
    // Check that each of the individual buffers are valid.
    //
    for( DWORD x = 0; x < CdRead.sgcount; x++ )
    {
        if( fUserMode && !::IsKModeAddr (reinterpret_cast<DWORD> (pInBuffer)) )
        {
            if( !::IsValidUsrPtr( &pBuffers[x], dwInBufSize, TRUE ) )
            {
                SetLastError( ERROR_INVALID_PARAMETER );
                fResult = FALSE;
                goto exit_readsgtriage;
            }
        }

        if( ULONG_MAX - dwRequestedBytes < pBuffers[x].sb_len )
        {
            //
            // Reading more than 4GB of data at once?  I don't think that's
            // happening on a CE device anytime soon.
            //
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
            goto exit_readsgtriage;
        }

        dwRequestedBytes += pBuffers[x].sb_len;
    }

    if( dwRequestedBytes / CD_SECTOR_SIZE < CdRead.TransferLength )
    {
        //
        // There isn't enough room in the buffers for the amount of data
        // requested.
        //
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_readsgtriage;
    }

    if( CdRead.bRawMode )
    {
        if( m_MediaType & MED_FLAG_DVD )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
            goto exit_readsgtriage;
        }
    }

    fResult = ReadSG( CdRead,
                      pBuffers,
                      pdwBytesReturned );

exit_readsgtriage:
    if( pBuffers )
    {
        delete [] pBuffers;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ReadSG
//
// It is probable that the individual buffers are not block aligned.  Because of
// this we must use an internal buffer as the transfer size is in blocks.  The
// default size is sector size * MAX_SECTORS_PER_COMMAND.
//
// This must calculate the offset to the session.
//
BOOL Device_t::ReadSG( CDROM_READ& CdRead,
                       PSGX_BUF pBuffers,
                       DWORD* pdwBytesReturned )
{
    BOOL fResult = TRUE;
    BYTE* pCache = NULL;
    DWORD dwCacheSize = 0;
    DWORD dwCacheStart = 0;
    DWORD dwTotalBlocks = 0;
    DWORD dwStartingBlock = 0;
    DWORD dwCurrentBlock = 0;
    DWORD dwCurrentBuffer = 0;
    DWORD dwBufferOffset = 0;

    CDB Cdb = { 0 };
    DWORD dwSectorSize = 0;
    DWORD& dwLBA = *((DWORD*)(&Cdb.CDB10.LogicalBlockByte0));
    SENSE_DATA SenseData = { 0 };

    if( CdRead.StartAddr.Mode == CDROM_ADDR_MSF )
    {
        CDROM_MSF_TO_LBA( &CdRead.StartAddr );
    }
    else if( CdRead.StartAddr.Mode != CDROM_ADDR_LBA )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_readsg;
    }

    __try
    {
        *pdwBytesReturned = 0;
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_readsg;
    }

    dwTotalBlocks = CdRead.TransferLength;

    dwStartingBlock = CdRead.StartAddr.Address.lba;
    dwCurrentBlock = dwStartingBlock;

    if( CdRead.bRawMode == FALSE )
    {
        dwCacheSize = CD_SECTOR_SIZE * MAX_SECTORS_PER_COMMAND;
        Cdb.CDB10.OperationCode = SCSIOP_READ;
        Cdb.CDB10.TransferBlocksMsb = (BYTE)((MAX_SECTORS_PER_COMMAND >> 8) & 0xFF);
        Cdb.CDB10.TransferBlocksLsb = (BYTE)(MAX_SECTORS_PER_COMMAND & 0xFF);

        dwSectorSize = CD_SECTOR_SIZE;
    }
    else
    {
        switch( CdRead.TrackMode )
        {
        //
        // Apparently we read the entire 2352 bytes when reading
        // XAForm2/YellowMode2 even if the "user data" is a different
        // size.
        //
        case YellowMode2:
            Cdb.READ_CD.ExpectedSectorType = 3;
            break;

        case XAForm2:
            Cdb.READ_CD.ExpectedSectorType = 5;
            break;

        case CDDA:
            Cdb.READ_CD.ExpectedSectorType = 1;
            break;
        }

        dwSectorSize = CDROM_RAW_SECTOR_SIZE;
        Cdb.READ_CD.IncludeUserData = 1;
        Cdb.READ_CD.HeaderCode = 3;
        Cdb.READ_CD.IncludeSyncData = 1;

        dwCacheSize = dwSectorSize * MAX_SECTORS_PER_COMMAND;
        Cdb.READ_CD.OperationCode = SCSIOP_READ_CD;
        Cdb.READ_CD.TransferBlocks[1] = (BYTE)((MAX_SECTORS_PER_COMMAND >> 8) & 0xFF);
        Cdb.READ_CD.TransferBlocks[2] = (BYTE)(MAX_SECTORS_PER_COMMAND & 0xFF);
    }

    pCache = new BYTE[ dwCacheSize ];
    if( !pCache )
    {
        fResult = FALSE;
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        goto exit_readsg;
    }

    while( dwCurrentBlock - dwStartingBlock < dwTotalBlocks )
    {
        DWORD dwBlocksToRead = dwTotalBlocks - (dwCurrentBlock - dwStartingBlock);
        const BYTE* pOffset = pCache;
        DWORD dwBytesReturned = 0;

        SwapCopyUchar4( &dwLBA, &dwCurrentBlock);

        if( dwBlocksToRead < MAX_SECTORS_PER_COMMAND )
        {
            dwCurrentBlock += dwBlocksToRead;

            if( CdRead.bRawMode == FALSE )
            {
                Cdb.CDB10.TransferBlocksMsb = (BYTE)((dwBlocksToRead >> 8) & 0xFF);
                Cdb.CDB10.TransferBlocksLsb = (BYTE)(dwBlocksToRead & 0xFF);
            }
            else
            {
                Cdb.READ_CD.TransferBlocks[1] = (BYTE)((dwBlocksToRead >> 8) & 0xFF);
                Cdb.READ_CD.TransferBlocks[2] = (BYTE)(dwBlocksToRead & 0xFF);
            }

            dwCacheSize = dwBlocksToRead * dwSectorSize;
        }
        else
        {
            dwBlocksToRead = MAX_SECTORS_PER_COMMAND;
            dwCurrentBlock += MAX_SECTORS_PER_COMMAND;
        }

        if( !SendPassThroughDirect( &Cdb,
                                    pCache,
                                    dwCacheSize,
                                    TRUE,
                                    m_Group1Timeout,
                                    &dwBytesReturned,
                                    &SenseData ) )
        {
            fResult = FALSE;
            goto exit_readsg;
        }

        if( dwBytesReturned > (dwBlocksToRead * dwSectorSize) )
        {
            ASSERT(FALSE);
            dwBytesReturned = (dwBlocksToRead * dwSectorSize);
        }

        //
        // Copy the data from our internal cache into the user buffers.
        //
        while( dwBytesReturned && (dwCurrentBuffer < CdRead.sgcount) )
        {
            if( dwBytesReturned >= (pBuffers[dwCurrentBuffer].sb_len - dwBufferOffset) )
            {
                DWORD dwSize = pBuffers[dwCurrentBuffer].sb_len -
                               dwBufferOffset;
                //
                // The internal buffer has more data than can be put into the
                // current user buffer, or will completely fill it up.
                //
                __try
                {
                    CopyMemory( pBuffers[dwCurrentBuffer].sb_buf + dwBufferOffset,
                                pOffset,
                                dwSize );
                    *pdwBytesReturned += dwSize;
                }
                __except( EXCEPTION_EXECUTE_HANDLER )
                {
                    SetLastError( ERROR_INVALID_PARAMETER );
                    fResult = FALSE;
                    goto exit_readsg;
                }

                //
                // Reduce the number of bytes left in our buffer, and set the
                // pointer beyond what we just read.
                //
                pOffset += dwSize;
                dwBytesReturned -= dwSize;

                //
                // Use the next SG buffer the next time around.
                //
                dwCurrentBuffer += 1;
                dwBufferOffset = 0;
            }
            else
            {
                //
                // The user buffer can hold all of the data in the internal
                // buffer.
                //
                __try
                {
                    CopyMemory( pBuffers[dwCurrentBuffer].sb_buf + dwBufferOffset,
                                pOffset,
                                dwBytesReturned );
                    *pdwBytesReturned += dwBytesReturned;
                }
                __except( EXCEPTION_EXECUTE_HANDLER )
                {
                    SetLastError( ERROR_INVALID_PARAMETER );
                    fResult = FALSE;
                    goto exit_readsg;
                }

                //
                // We will still be reading into the same buffer.
                //
                dwBufferOffset += dwBytesReturned;
                dwBytesReturned = 0;
            }
        }
    }

exit_readsg:
    if( pCache )
    {
        delete [] pCache;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ReadCdRaw
//
BOOL Device_t::ReadCdRaw( PVOID pInBuffer,
                          DWORD dwInBufSize,
                          PVOID pOutBuffer,
                          DWORD dwOutBufSize,
                          DWORD* pdwBytesReturned )
{
    BOOL fResult = TRUE;
    CDROM_READ CdRead = { 0 };
    DWORD dwStartSector = 0;
    PRAW_READ_INFO pRawReadInfo = (PRAW_READ_INFO)pInBuffer;

    //
    // Validate the input/output buffers and their respective values.
    //
    if( pInBuffer == NULL )
    {
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_readcdraw;
    }

    if( pOutBuffer == NULL )
    {
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_readcdraw;
    }

    if( dwInBufSize < sizeof(RAW_READ_INFO) )
    {
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_readcdraw;
    }

    //
    // Construct a CDROM_READ structure to pass to the ReadSG function.
    //
    CdRead.bRawMode = TRUE;
    CdRead.sgcount = 1;
    CdRead.TrackMode = pRawReadInfo->TrackMode;

    CopyMemory( &CdRead.StartAddr, &pRawReadInfo->CdAddress, sizeof(CDROM_ADDR) );

    CdRead.TransferLength = pRawReadInfo->SectorCount;
    CdRead.sglist[0].sb_buf = (PUCHAR)pOutBuffer;
    CdRead.sglist[0].sb_len = dwOutBufSize;

    //
    // All the work will be done by ReadSG.
    //
    if( !ReadSG( CdRead, CdRead.sglist, pdwBytesReturned ) )
    {
        fResult = FALSE;
        goto exit_readcdraw;
    }

exit_readcdraw:

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// TestUnitReady
//
BOOL Device_t::TestUnitReady( PVOID pOutBuffer,
                              DWORD dwOutBufSize,
                              DWORD* pdwBytesReturned )
{
    CDB Cdb = { 0 };
    SENSE_DATA SenseData = { 0 };
    PCDROM_TESTUNITREADY pReady = (PCDROM_TESTUNITREADY)pOutBuffer;
    BOOL fResult = TRUE;
    DWORD dwBytesReturned = 0;

    DEBUGMSG( ZONE_IO, (L"CDROM::TestUnitReady entered.\r\n") );

    if( !pOutBuffer || dwOutBufSize < sizeof(CDROM_TESTUNITREADY) )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    fResult = SendPassThrough( &Cdb,
                               NULL,
                               0,
                               TRUE,
                               GetGroupOneTimeout(),
                               &dwBytesReturned,
                               &SenseData );
    __try
    {
        pReady->bUnitReady = fResult;
        *pdwBytesReturned = sizeof( CDROM_TESTUNITREADY);
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    return TRUE;
}

// /////////////////////////////////////////////////////////////////////////////
// GetDiscInformation
//
BOOL Device_t::GetDiscInformation( PVOID pInBuffer,
                                   DWORD dwInBufSize,
                                   PVOID pOutBuffer,
                                   DWORD dwOutBufSize,
                                   DWORD* pdwBytesReturned )
{
    BOOL fResult = TRUE;
    DISC_INFO OutType = DI_NONE;
    DWORD dwBytesRead = 0;
    CDB Cdb = { 0 };
    SENSE_DATA SenseData = { 0 };
    CDROM_TESTUNITREADY UnitReady = { 0 };

    if( !pOutBuffer )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_getdiscinformation;
    }

    if( pInBuffer )
    {
        if( dwInBufSize != sizeof(DISC_INFO) )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
            goto exit_getdiscinformation;
        }

        __try
        {
            OutType = *((DISC_INFO*)pInBuffer);
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
            goto exit_getdiscinformation;
        }

        if( OutType > DI_SCSI_INFO )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
            goto exit_getdiscinformation;
        }
    }

    if( !TestUnitReady( &UnitReady,
                        sizeof(UnitReady),
                        &dwBytesRead) )
    {
        SetLastError( ERROR_NOT_READY );
        fResult = FALSE;
        goto exit_getdiscinformation;
    }

    if( !UnitReady.bUnitReady )
    {
        SetLastError( ERROR_NOT_READY );
        fResult = FALSE;
        goto exit_getdiscinformation;
    }

    if( OutType == DI_SCSI_INFO )
    {
        if( dwOutBufSize < sizeof(DISC_INFORMATION) )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
            goto exit_getdiscinformation;
        }
    }
    else
    {
        if( dwOutBufSize < sizeof(CDROM_DISCINFO) )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
            goto exit_getdiscinformation;
        }

        //
        // This function never returned anything other than ERROR_SUCCESS.
        //
        goto exit_getdiscinformation;
    }

    Cdb.READ_DISC_INFORMATION.OperationCode = SCSIOP_READ_DISC_INFORMATION;
    Cdb.READ_DISC_INFORMATION.AllocationLength[0] = (UCHAR)((dwOutBufSize >> 8) & 0xFF);
    Cdb.READ_DISC_INFORMATION.AllocationLength[1] = (UCHAR)(dwOutBufSize & 0xFF);

    if( dwOutBufSize > USHRT_MAX )
    {
        Cdb.READ_DISC_INFORMATION.AllocationLength[0] = 0xFF;
        Cdb.READ_DISC_INFORMATION.AllocationLength[1] = 0xFF;
    }

    __try
    {
        if( !SendPassThroughDirect( &Cdb,
                                    (BYTE*)pOutBuffer,
                                    dwOutBufSize,
                                    TRUE,
                                    GetGroupOneTimeout(),
                                    &dwBytesRead,
                                    &SenseData ) )
        {
            fResult = FALSE;
        }
        else
        {
            *pdwBytesReturned = dwBytesRead;
        }
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
    }

exit_getdiscinformation:

#ifdef USE_COMPAT_SETTINGS
    if( !fResult )
    {
        DISC_INFORMATION DiscInfo = { 0 };

        //
        // Fake a complete disc.
        //
        DiscInfo.DiscStatus = DISK_STATUS_COMPLETE;
        DiscInfo.LastSessionStatus = LAST_SESSION_COMPLETE;

        //
        // Fake one session on the disc.
        //
        DiscInfo.NumberOfSessionsLsb = 1;

        //
        // Fake one track in the session.
        //
        DiscInfo.LastSessionFirstTrackLsb = 1;
        DiscInfo.LastSessionLastTrackLsb = 1;
    }
#endif

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// EjectMedia
//
BOOL Device_t::EjectMedia()
{
    CDB Cdb = { 0 };
    SENSE_DATA SenseData = { 0 };
    DWORD dwBytesReturned = 0;

    Cdb.START_STOP.OperationCode = SCSIOP_START_STOP_UNIT;
    Cdb.START_STOP.LoadEject = 1;

    if( !SendPassThroughDirect( &Cdb,
                                NULL,
                                0,
                                FALSE,
                                GetGroupOneTimeout(),
                                &dwBytesReturned,
                                &SenseData ) )
    {
        return FALSE;
    }

    return TRUE;
}

// /////////////////////////////////////////////////////////////////////////////
// LoadMedia
//
BOOL Device_t::LoadMedia()
{
    CDB Cdb = { 0 };
    SENSE_DATA SenseData = { 0 };
    DWORD dwBytesReturned = 0;

    Cdb.START_STOP.OperationCode = SCSIOP_START_STOP_UNIT;
    Cdb.START_STOP.LoadEject = 1;
    Cdb.START_STOP.Start = 1;

    if( !SendPassThroughDirect( &Cdb,
                                NULL,
                                0,
                                FALSE,
                                GetGroupOneTimeout(),
                                &dwBytesReturned,
                                &SenseData ) )
    {
        return FALSE;
    }

    return TRUE;
}

// /////////////////////////////////////////////////////////////////////////////
// IssueInquiry
//
BOOL Device_t::IssueInquiry( VOID* pOutBuffer,
                             DWORD dwOutBufSize,
                             DWORD* pdwBytesReturned )
{
    BOOL fResult = TRUE;
    CDB Cdb = { 0 };
    DWORD dwBytesReturned = 0;
    INQUIRY_DATA InquiryData = { 0 };
    SENSE_DATA SenseData = { 0 };

    if( !pOutBuffer || !pdwBytesReturned )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    if( dwOutBufSize < sizeof(INQUIRY_DATA) )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    Cdb.CDB6INQUIRY.OperationCode = SCSIOP_INQUIRY;
    Cdb.CDB6INQUIRY.AllocationLength = (UCHAR)sizeof(SENSE_DATA);

    fResult = SendPassThroughDirect( &Cdb,
                                     (BYTE*)&InquiryData,
                                     sizeof(InquiryData),
                                     TRUE,
                                     50,
                                     &dwBytesReturned,
                                     &SenseData );

    if( fResult )
    {
        __try
        {
            CopyMemory( pdwBytesReturned, &dwBytesReturned, sizeof(dwBytesReturned) );
            CopyMemory( pOutBuffer, &InquiryData, sizeof(InquiryData) );
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return FALSE;
        }
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ReadTOC
//
BOOL Device_t::ReadTOC( VOID* pOutBuffer,
                        DWORD dwOutBufSize,
                        DWORD* pdwBytesReturned )
{
    BOOL fResult = TRUE;
    CDB Cdb = { 0 };
    SENSE_DATA SenseData = { 0 };

    Cdb.READ_TOC.OperationCode = SCSIOP_READ_TOC;
    Cdb.READ_TOC.Msf = 1;
    Cdb.READ_TOC.AllocationLength[0] = (UCHAR)(dwOutBufSize >> 8 & 0xFF);
    Cdb.READ_TOC.AllocationLength[1] = (UCHAR)(dwOutBufSize & 0xFF);

    if( dwOutBufSize > USHRT_MAX )
    {
        Cdb.READ_TOC.AllocationLength[0] = 0xFF;
        Cdb.READ_TOC.AllocationLength[1] = 0xFF;
    }

    __try
    {
        fResult = SendPassThroughDirect( &Cdb,
                                         (BYTE*)pOutBuffer,
                                         dwOutBufSize,
                                         TRUE,
                                         GetGroupOneTimeout(),
                                         pdwBytesReturned,
                                         &SenseData );
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    return fResult;
}

#if 0
// /////////////////////////////////////////////////////////////////////////////
// StartDVDSession
//
// In order to read CSS data from a CD there is a sequence of steps that must
// be followed.  This is the first step in the sequence.  We must get a session
// ID that will be used in the future requests to identify this client.
//
BOOL Device_t::StartDVDSession( VOID* pOutBuffer,
                                DWORD dwOutBufSize,
                                DWORD* pdwBytesReturned )
{
    BOOL fResult = TRUE;
    CDB Cdb = { 0 };
    SENSE_DATA SenseData = { 0 };
    DWORD dwBytesReturned = 0;
    RKFMT_AGID ReturnBuffer = { 0 };
    DVD_SESSION_ID* pSessionId = (DVD_SESSION_ID*)pOutBuffer;

    if( !pOutBuffer || !pdwBytesReturned || dwOutBufSize < sizeof(DVD_SESSION_ID) )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    Cdb.REPORT_KEY.OperationCode = SCSIOP_REPORT_KEY;

    fResult = SendPassThroughDirect( &Cdb,
                                     (BYTE*)&ReturnBuffer,
                                     sizeof(ReturnBuffer),
                                     TRUE,
                                     GetGroupOneTimeout(),
                                     &dwBytesReturned,
                                     &SenseData );
    if( fResult )
    {
        __try
        {
            *pSessionId = ReturnBuffer.agid;
            *pdwBytesReturned = dwBytesReturned;
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
        }
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ReadDVDKey
//
// After a session ID is obtained, we need to gather keys to
// a) ensure that the device conforms to the DVD standards.
//
BOOL Device_t::ReadDVDKey( VOID* pInBuffer,
                           DWORD dwInBufSize,
                           VOID* pOutBuffer,
                           DWORD dwOutBufSize,
                           DWORD* pdwBytesReturned )
{
    BOOL fResult = TRUE;
    CDB Cdb = { 0 };
    SENSE_DATA SenseData = { 0 };
    DWORD dwBytesReturned = 0;
    BYTE Buffer[32] = { 0 };

    PDVD_COPY_PROTECT_KEY pKey = (PDVD_COPY_PROTECT_KEY)pInBuffer;

    if( !pInBuffer || dwInBufSize < sizeof(DVD_COPY_PROTECT_KEY) )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_readdvdkey;
    }

    if( !pOutBuffer || dwOutBufSize < sizeof(DVD_COPY_PROTECT_KEY) )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_readdvdkey;
    }

    Cdb.REPORT_KEY.OperationCode = SCSIOP_REPORT_KEY;
    Cdb.REPORT_KEY.AllocationLength[0] = (UCHAR)((sizeof(RKFMT_DISC) >> 8) & 0xFF);
    Cdb.REPORT_KEY.AllocationLength[1] = (UCHAR)(sizeof(RKFMT_DISC) & 0xFF);

    __try
    {
        Cdb.REPORT_KEY.AGID = (UCHAR)pKey->SessionId;
        Cdb.REPORT_KEY.KeyFormat = (UCHAR)pKey->KeyType;

        //
        // This is currently only used with the TITLE KEY for CSS/CPPM or CPRM.
        //
        SwapCopyUchar4( Cdb.REPORT_KEY.LogicalBlockAddress, pKey->StartAddr );

    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_readdvdkey;
    }

    if( !SendPassThroughDirect( &Cdb,
                                Buffer,
                                sizeof(Buffer),
                                TRUE,
                                GetGroupOneTimeout(),
                                &dwBytesReturned,
                                &SenseData ) )
    {
        fResult = FALSE;
        goto exit_readdvdkey;
    }

    __try
    {
        switch( pKey->KeyType )
        {
        case DvdTitleKey:
        {
            PRKFMT_TITLE pResult = (PRKFMT_TITLE)Buffer;

            if( pKey->KeyLength < sizeof(pResult->title) )
            {
                SetLastError( ERROR_INVALID_PARAMETER );
                fResult = FALSE;
                goto exit_readdvdkey;
            }

            pKey->KeyFlags = pResult->cgms;
            CopyMemory( pKey->KeyData, pResult->title, sizeof(pResult->title) );
        }
        break;

        //
        // Apparently we just return a blob for all requests other than the
        // title key.  I'm not sure why this was done, but there you have it.
        //
        default:
        {
            //
            // Using PRKFMT_BUS is simply a random choice.  It doesn't matter
            // which of the structures we use.  The important part is the first
            // 2 fields, which are identical across all the pertinent structures.
            //
            PRKFMT_BUS pResult = (PRKFMT_BUS)Buffer;

            if( pKey->KeyLength < (sizeof(pResult->Len) - 4) )
            {
                SetLastError( ERROR_INVALID_PARAMETER );
                fResult = FALSE;
                goto exit_readdvdkey;
            }

            CopyMemory( pKey->KeyData, pResult->bus, sizeof(pResult->bus) - 4 );
        }
        break;
        }

    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_readdvdkey;
    }

exit_readdvdkey:

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// EndDVDSession
//
// This will invalidate the session key that we opened to read the DVD.
//
BOOL Device_t::EndDVDSession( VOID* pInBuffer, DWORD dwInBufSize )
{
    BOOL fResult = TRUE;
    CDB Cdb = { 0 };
    SENSE_DATA SenseData = { 0 };
    DWORD dwBytesReturned = 0;

    PDVD_COPY_PROTECT_KEY pKey = (PDVD_COPY_PROTECT_KEY)pInBuffer;

    if( !pInBuffer || dwInBufSize < sizeof(DVD_COPY_PROTECT_KEY) )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_enddvdsession;
    }

    Cdb.REPORT_KEY.OperationCode = SCSIOP_REPORT_KEY;
    Cdb.REPORT_KEY.KeyFormat = 0x3F;

    __try
    {
        Cdb.REPORT_KEY.AGID = (UCHAR)pKey->SessionId;

    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_enddvdsession;
    }

    if( !SendPassThroughDirect( &Cdb,
                                NULL,
                                0,
                                FALSE,
                                GetGroupOneTimeout(),
                                &dwBytesReturned,
                                &SenseData ) )
    {
        fResult = FALSE;
        goto exit_enddvdsession;
    }

exit_enddvdsession:

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// SendDVDKey
//
BOOL Device_t::SendDVDKey( VOID* pInBuffer, DWORD dwInBufSize )
{
    BOOL fResult = TRUE;
    CDB Cdb = { 0 };
    DWORD dwBytesReturned = 0;
    SENSE_DATA SenseData = { 0 };
    BYTE Buffer[32] = { 0 };
    USHORT BytesToSend = 0;
    BYTE* pLength = NULL;

    PDVD_COPY_PROTECT_KEY pKey = (PDVD_COPY_PROTECT_KEY)pInBuffer;

    if( !pInBuffer || dwInBufSize < sizeof(DVD_COPY_PROTECT_KEY) )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_senddvdkey;
    }

    Cdb.SEND_KEY.OperationCode = SCSIOP_REPORT_KEY;

    __try
    {
        Cdb.SEND_KEY.AGID = (UCHAR)pKey->SessionId;
        Cdb.SEND_KEY.KeyFormat = (UCHAR)pKey->KeyType;
        Cdb.SEND_KEY.ParameterListLength[0] = (UCHAR)((pKey->KeyLength >> 8) & 0xFF);
        Cdb.SEND_KEY.ParameterListLength[1] = (UCHAR)(pKey->KeyLength & 0xFF);

        switch( Cdb.SEND_KEY.KeyFormat )
        {
        case DvdChallengeKey:
            {
                PRKFMT_CHLGKEY pChallengeKey = (PRKFMT_CHLGKEY)Buffer;
                pLength = (BYTE*)&pChallengeKey->Len;

                pLength[1] = (BYTE)(sizeof(RKFMT_CHLGKEY) - 2);
                if( pKey->KeyLength < sizeof(pChallengeKey->chlgkey) )
                {
                    SetLastError( ERROR_INVALID_PARAMETER );
                    fResult = FALSE;
                    goto exit_senddvdkey;
                }
            }
            break;

        case DvdBusKey2:
            {
                PRKFMT_BUS pBusKey = (PRKFMT_BUS)Buffer;
                pLength = (BYTE*)&pBusKey->Len;

                pLength[1] = (BYTE)(sizeof(RKFMT_BUS) - 2);
                if( pKey->KeyLength < sizeof(pBusKey->bus) )
                {
                    SetLastError( ERROR_INVALID_PARAMETER );
                    fResult = FALSE;
                    goto exit_senddvdkey;
                }
            }
            break;

        //
        // Technically there is a separate IOCTL for setting the
        // region, but there's really no reason it cannot be done
        // here.  We just ignore the AGID.  The other IOCTL has
        // never worked anyways.  It just returns ERROR_NOT_SUPPORTED.
        //
        case DvdSetRPC:
            {
                PRKFMT_RPC_SET pSetRPC = (PRKFMT_RPC_SET)Buffer;
                pLength = (BYTE*)&pSetRPC->Len;

                Cdb.SEND_KEY.AGID = 0;

                pLength[1] = (BYTE)(sizeof(RKFMT_BUS) - 2);
                if( pKey->KeyLength < sizeof(pSetRPC->SystemRegion) )
                {
                    SetLastError( ERROR_INVALID_PARAMETER );
                    fResult = FALSE;
                    goto exit_senddvdkey;
                }
            }
            break;

        default:
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
            goto exit_senddvdkey;
        }

    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_senddvdkey;
    }

    //
    // We must check that pLength is not NULL in order to satisfy the
    // not-so-bright OACR.  So much for performance.
    //

    if( pLength &&
        !SendPassThroughDirect( &Cdb,
                                Buffer,
                                pLength[1] + 2,
                                FALSE,
                                GetGroupOneTimeout(),
                                &dwBytesReturned,
                                &SenseData ) )
    {
        fResult = FALSE;
        goto exit_senddvdkey;
    }

exit_senddvdkey:

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetRegion
//
BOOL Device_t::GetRegion( VOID* pOutBuffer,
                          DWORD dwOutBufSize,
                          DWORD* pdwBytesReturned )
{
    BOOL fResult = TRUE;
    DVD_REGIONCE CeRegionInfo = { 0 };
    RKFMT_RPC DvdRegionInfo = { 0 };
    CDB Cdb = { 0 };
    SENSE_DATA SenseData = { 0 };
    DWORD dwBytesReturned = 0;

    //
    // The only other data structure defined is RDVDFMT_Copy, and that structure
    // just isn't very helpful.
    //
    struct
    {
        BYTE DataLength[2];
        BYTE Reserved0[2];
        BYTE CopyProtectionSystem;
        BYTE RegionManagementInformation;
        BYTE Reserved1[2];
    } DvdCopyrightInfo = { 0 };

    if( !pOutBuffer || (dwOutBufSize < sizeof(DVD_REGIONCE)) )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_getregion;
    }

    //
    // First we will gather information about the drive itself.  This will tell
    // us what regions the drive can actually play.
    //
    Cdb.REPORT_KEY.OperationCode = SCSIOP_REPORT_KEY;
    Cdb.REPORT_KEY.AllocationLength[0] = (UCHAR)((sizeof(RKFMT_RPC) >> 8 ) & 0xFF);
    Cdb.REPORT_KEY.AllocationLength[1] = (UCHAR)(sizeof(RKFMT_RPC) & 0xFF);
    Cdb.REPORT_KEY.KeyFormat = DvdGetRPC;

    if( !SendPassThroughDirect( &Cdb,
                                (BYTE*)&DvdRegionInfo,
                                sizeof(DvdRegionInfo),
                                TRUE,
                                GetGroupOneTimeout(),
                                &dwBytesReturned,
                                &SenseData ) )
    {
        fResult = FALSE;
        goto exit_getregion;
    }

    //
    // Check if the region for the device has ever been set.
    //
    if( DvdRegionInfo.ResetCounts & 0xC0 )
    {
        //
        // The region on the device has been set already.  For some reason we've
        // decided to invert the value returned from the device.  This means
        // that if the bit is set, then the device supports that region.
        //
        CeRegionInfo.SystemRegion = DvdRegionInfo.SystemRegion ^ 0xFF;
        CeRegionInfo.ResetCount = DvdRegionInfo.ResetCounts & 0x7;
    }
    else
    {
        //
        // The drive region has never been set.  Again, for an unknown reason we
        // are going to set this to 0xFF.  This seems wrong given that we just
        // inverted the regions above.  We are essentially saying that this
        // device supports every region instead of no region.
        //
        CeRegionInfo.SystemRegion = 0xFF;
        CeRegionInfo.ResetCount = 5;
    }

    //
    // Now we gather information about the disc in the drive, assuming there is
    // one.
    //
    ZeroMemory( &Cdb, sizeof(Cdb) );
    Cdb.READ_DVD_STRUCTURE.OperationCode = SCSIOP_READ_DVD_STRUCTURE;
    Cdb.READ_DVD_STRUCTURE.Format = DVDSTRUC_FMT_COPY;
    Cdb.READ_DVD_STRUCTURE.AllocationLength[0] = (UCHAR)((sizeof(DvdCopyrightInfo) >> 8) & 0xFF);
    Cdb.READ_DVD_STRUCTURE.AllocationLength[1] = (UCHAR)(sizeof(DvdCopyrightInfo) & 0xFF);

    if( !SendPassThroughDirect( &Cdb,
                                (BYTE*)&DvdCopyrightInfo,
                                sizeof(DvdCopyrightInfo),
                                TRUE,
                                GetGroupOneTimeout(),
                                &dwBytesReturned,
                                &SenseData ) )
    {
        fResult = FALSE;
        goto exit_getregion;
    }

    //
    // At least we're being consistent.  If the media supports a region, the
    // bit identifying that region will be set to one.  This wouldn't be so
    // bad if we only documented that we were doing this.
    //
    CeRegionInfo.CopySystem = DvdCopyrightInfo.CopyProtectionSystem;
    CeRegionInfo.RegionData = DvdCopyrightInfo.RegionManagementInformation ^ 0xFF;

    //
    // Now just copy the data into the user's buffer.
    //
    __try
    {
        CopyMemory( pOutBuffer, &CeRegionInfo, sizeof(CeRegionInfo) );
        if( pdwBytesReturned )
        {
            *pdwBytesReturned = sizeof( CeRegionInfo );
        }
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_getregion;
    }

exit_getregion:

    return fResult;
}
#endif // 0

// /////////////////////////////////////////////////////////////////////////////
// PollUnitReady
//
BOOL Device_t::ReadQSubChannel( VOID* pInBuffer,
                                DWORD dwInBufSize,
                                VOID* pOutBuffer,
                                DWORD dwOutBufSize,
                                DWORD* pdwBytesReturned )
{
    BOOL fResult = TRUE;
    CDB Cdb = { 0 };
    SENSE_DATA SenseData = { 0 };
    DWORD dwBytesReturned = 0;
    CDROM_SUB_Q_DATA_FORMAT SubChannelRequest = { 0 };
    USHORT DataSize = 0;
    BYTE Buffer[32] = { 0 };

    if( !pInBuffer || !pOutBuffer || dwInBufSize < sizeof(CDROM_SUB_Q_DATA_FORMAT) )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_readqsubchannel;
    }

    __try
    {
        CopyMemory( &SubChannelRequest, pInBuffer, sizeof(SubChannelRequest) );
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_readqsubchannel;
    }

    switch( SubChannelRequest.Format )
    {
    case IOCTL_CDROM_CURRENT_POSITION:
        DataSize = sizeof(SUB_Q_CURRENT_POSITION);
        break;

    case IOCTL_CDROM_MEDIA_CATALOG:
        DataSize = sizeof(SUB_Q_MEDIA_CATALOG_NUMBER);
        break;

    case IOCTL_CDROM_TRACK_ISRC:
        DataSize = sizeof(SUB_Q_TRACK_ISRC);
        Cdb.SUBCHANNEL.TrackNumber = SubChannelRequest.Track;
        break;

    default:
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_readqsubchannel;
    }

    if( dwOutBufSize < DataSize )
    {
        fResult = FALSE;
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_readqsubchannel;
    }

    Cdb.SUBCHANNEL.OperationCode = SCSIOP_READ_SUB_CHANNEL;
    Cdb.SUBCHANNEL.SubQ = 1;
    Cdb.SUBCHANNEL.Format = SubChannelRequest.Format;
    Cdb.SUBCHANNEL.AllocationLength[0] = (UCHAR)((DataSize >> 8) & 0xFF );
    Cdb.SUBCHANNEL.AllocationLength[1] = (UCHAR)(DataSize & 0xFF);

    if( !SendPassThroughDirect( &Cdb,
                                Buffer,
                                DataSize,
                                TRUE,
                                GetGroupOneTimeout(),
                                &dwBytesReturned,
                                &SenseData ) )
    {
        fResult = FALSE;
        goto exit_readqsubchannel;
    }

    __try
    {
        CopyMemory( pOutBuffer, Buffer, DataSize );
        *pdwBytesReturned = dwBytesReturned;
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_readqsubchannel;
    }

exit_readqsubchannel:

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ControlAudio
//
// This takes care of all the audio controls in one simple function.
//
BOOL Device_t::ControlAudio( DWORD dwIoCode,
                             VOID* pInBuffer,
                             DWORD dwInBufSize )
{
    BOOL fResult = TRUE;
    CDB Cdb = { 0 };
    SENSE_DATA SenseData = { 0 };
    DWORD dwBytesReturned = 0;

    switch( dwIoCode )
    {
    case IOCTL_CDROM_PLAY_AUDIO_MSF:
    {
        PCDROM_PLAY_AUDIO_MSF pPlayInfo = (PCDROM_PLAY_AUDIO_MSF)pInBuffer;

        if( !pPlayInfo )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
            goto exit_controlaudio;
        }

        if( dwInBufSize < sizeof(CDROM_PLAY_AUDIO_MSF) )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
            goto exit_controlaudio;
        }

        Cdb.PLAY_AUDIO_MSF.OperationCode = SCSIOP_PLAY_AUDIO_MSF;
        Cdb.PLAY_AUDIO_MSF.StartingM = pPlayInfo->StartingM;
        Cdb.PLAY_AUDIO_MSF.StartingS = pPlayInfo->StartingS;
        Cdb.PLAY_AUDIO_MSF.StartingF = pPlayInfo->StartingF;
        Cdb.PLAY_AUDIO_MSF.EndingM = pPlayInfo->EndingM;
        Cdb.PLAY_AUDIO_MSF.EndingS = pPlayInfo->EndingS;
        Cdb.PLAY_AUDIO_MSF.EndingF = pPlayInfo->EndingF;
        break;
    }

    case IOCTL_CDROM_SEEK_AUDIO_MSF:
    {
        PCDROM_SEEK_AUDIO_MSF pSeekInfo = (PCDROM_SEEK_AUDIO_MSF)pInBuffer;
        DWORD                 dwLBA;

        if( !pSeekInfo )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
            goto exit_controlaudio;
        }

        if( dwInBufSize < sizeof(CDROM_SEEK_AUDIO_MSF) )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
            goto exit_controlaudio;
        }

        dwLBA = CDROM_MSFCOMP_TO_LBA(pSeekInfo->M, pSeekInfo->S, pSeekInfo->F);

        Cdb.SEEK.OperationCode = SCSIOP_SEEK;
        Cdb.SEEK.LogicalBlockAddress[0] = (UCHAR)((dwLBA >> 24) & 0xFF);
        Cdb.SEEK.LogicalBlockAddress[1] = (UCHAR)((dwLBA >> 16) & 0xFF);
        Cdb.SEEK.LogicalBlockAddress[2] = (UCHAR)((dwLBA >> 8) & 0xFF);
        Cdb.SEEK.LogicalBlockAddress[3] = (UCHAR)(dwLBA & 0xFF);
        break;
    }

    case IOCTL_CDROM_SCAN_AUDIO:
    {
        PCDROM_SCAN_AUDIO pScanInfo = (PCDROM_SCAN_AUDIO)pInBuffer;

        if( !pScanInfo )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
            goto exit_controlaudio;
        }

        if( dwInBufSize < sizeof(CDROM_SCAN_AUDIO) ) {
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
            goto exit_controlaudio;
        }

        Cdb.SCAN_CD.OperationCode = SCSIOP_SCAN_CD;
        Cdb.SCAN_CD.Direct = pScanInfo->Direction;
        Cdb.SCAN_CD.StartingAddress[0] = pScanInfo->Address[0];
        Cdb.SCAN_CD.StartingAddress[1] = pScanInfo->Address[1];
        Cdb.SCAN_CD.StartingAddress[2] = pScanInfo->Address[2];
        Cdb.SCAN_CD.StartingAddress[3] = pScanInfo->Address[3];
        Cdb.SCAN_CD.Type = pScanInfo->AddressType;
        break;
    }

    case IOCTL_CDROM_STOP_AUDIO:
        Cdb.STOP_PLAY_SCAN.OperationCode = SCSIOP_STOP_PLAY_SCAN;
        break;

    case IOCTL_CDROM_PAUSE_AUDIO:
        Cdb.PAUSE_RESUME.OperationCode = SCSIOP_PAUSE_RESUME;
        break;

    case IOCTL_CDROM_RESUME_AUDIO:
        Cdb.PAUSE_RESUME.OperationCode = SCSIOP_PAUSE_RESUME;
        Cdb.PAUSE_RESUME.Action = 1;
        break;
    }

    if( !SendPassThroughDirect( &Cdb,
                                NULL,
                                0,
                                FALSE,
                                GetGroupOneTimeout(),
                                &dwBytesReturned,
                                &SenseData ) )
    {
            SetLastError( ERROR_INVALID_PARAMETER );
            fResult = FALSE;
            goto exit_controlaudio;
    }

exit_controlaudio:

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// HandleCommandError
//
// This function must determine the reason for the failure based on the sense
// code, and then take appropriate actions based on the command and the failure
// reason.
//
// In general we will opt to retry commands when the failure is due to a unit
// attention error.
//
// TODO::This function will need to be expanded to handle more situations
// gracefully.  In the case of streaming media for example we will not want
// to always follow the same path as that for writing from a file system.  Also,
// these results are mostly guesses and should be tweaked when we know more.
//
DWORD Device_t::HandleCommandError( PCDB pCdb, SENSE_DATA* pSenseData )
{
    DWORD dwResult = RESULT_FAIL;
    BOOL fTestUnitReady = FALSE;

    if( pSenseData->SenseKey == 0x06 )
    {
        //
        // Unit attention errors.
        //

        if( pSenseData->AdditionalSenseCode == 0x28 )
        {
            //
            // The device is letting us know that something has changed so we
            // will go ahead and fail this call so that we don't read or write
            // the wrong disc.
            //
            dwResult = RESULT_FAIL;
        }

        if( pSenseData->AdditionalSenseCode == 0x29 )
        {
            //
            // The device has been reset, was powered on, or the bus was reset.
            // Try the command again.
            //
            dwResult = RESULT_RETRY;
            fTestUnitReady = TRUE;
        }

        if( pSenseData->AdditionalSenseCode == 0x2A )
        {
            //
            // Some device parameters have changed.  Not sure, but think we can
            // just retry the command.
            //
            dwResult = RESULT_RETRY;
            fTestUnitReady = TRUE;
        }

        if( pSenseData->AdditionalSenseCode == 0x2E )
        {
            //
            // This idicates there was not enough time to complete the command.
            //
            switch( pCdb->AsByte[0] )
            {
            case SCSIOP_GET_CONFIGURATION:
            case SCSIOP_GET_EVENT_STATUS:
            case SCSIOP_INQUIRY:
            case SCSIOP_RECEIVE_DIAGNOSTIC:
            case SCSIOP_RELEASE_UNIT:
            case SCSIOP_RELEASE_ELEMENT:
            case SCSIOP_REQUEST_SENSE:
            case SCSIOP_RESERVE_ELEMENT:
            case SCSIOP_RESERVE_UNIT:
            case SCSIOP_SEND_DIAGNOSTIC:
                //
                // These commands should never timeout.
                //
                ASSERT( FALSE );
                dwResult = RESULT_FAIL;
                break;

            case SCSIOP_BLANK:
            case SCSIOP_CLOSE_TRACK_SESSION:
            case SCSIOP_FORMAT_UNIT:
            case SCSIOP_LOAD_UNLOAD_SLOT:
            case SCSIOP_RESERVE_TRACK_RZONE:
            case SCSIOP_SYNCHRONIZE_CACHE:
            case SCSIOP_VERIFY:
            case SCSIOP_VERIFY12:
                dwResult = RESULT_FAIL;
                break;

            default:
                dwResult = RESULT_RETRY;
                break;
            }

        }

        if( pSenseData->AdditionalSenseCode == 0x3B )
        {
            //
            // For one reason or another we cannot access the medium.
            //
            dwResult = RESULT_FAIL;
        }

        if( pSenseData->AdditionalSenseCode == 0x3F )
        {
            //
            // Something big has changed on the device.  Like the firmware.
            // Go ahead and retry the command?
            //
            dwResult = RESULT_RETRY;
            fTestUnitReady = TRUE;
        }

        if( pSenseData->AdditionalSenseCode == 0x5A )
        {
            //
            // The user has changed something?
            //
            dwResult = RESULT_RETRY;
            fTestUnitReady = TRUE;
        }

        if( pSenseData->AdditionalSenseCode == 0x5B )
        {
            //
            // An exception or some kind of treshold has been reached.
            //
            dwResult = RESULT_FAIL;
        }

        if( pSenseData->AdditionalSenseCode == 0x5E )
        {
            //
            // We're notified that some drive condition has changed.
            //
            dwResult = RESULT_RETRY;
            fTestUnitReady = TRUE;
        }

    }
    else if( pSenseData->SenseKey == 0x02 )
    {
        //
        // Unit readiness errors.
        //

        if( pSenseData->AdditionalSenseCode ==  0x04 )
        {
            //
            // The unit is not ready for one of the following reasons.
            //

            if( pSenseData->AdditionalSenseCodeQualifier == 0x00 )
            {
                //
                // Unknown reason.
                //
                dwResult = RESULT_FAIL;
            }

            if( pSenseData->AdditionalSenseCodeQualifier == 0x01 )
            {
                //
                // The device is becoming ready.
                //
                dwResult = RESULT_RETRY;
                fTestUnitReady = TRUE;
            }

            if( pSenseData->AdditionalSenseCodeQualifier == 0x02 )
            {
                //
                // Some intializing command is required.
                // TODO::Do we need to reset the device?
                //
                dwResult = RESULT_FAIL;
            }

            if( pSenseData->AdditionalSenseCodeQualifier == 0x03 )
            {
                //
                // Manual intervention is required.
                //
                dwResult = RESULT_FAIL;
            }

            if( pSenseData->AdditionalSenseCodeQualifier == 0x04 )
            {
                //
                // Format in progress.
                //
                dwResult = RESULT_FAIL;
            }

            if( pSenseData->AdditionalSenseCodeQualifier == 0x07 )
            {
                //
                // Operation in progress.
                //
                dwResult = RESULT_FAIL;
            }

            if( pSenseData->AdditionalSenseCodeQualifier == 0x08 )
            {
                //
                // Long write in progress.
                //
                dwResult = RESULT_FAIL;
            }
        }

        if( pSenseData->AdditionalSenseCode == 0x0C )
        {
            //
            // Write errors or defects found.
            //
            dwResult = RESULT_FAIL;
        }

        if( pSenseData->AdditionalSenseCode == 0x30 )
        {
            //
            // The media is in some way not compatible with the drive.
            //
            dwResult = RESULT_FAIL;
        }

        if( pSenseData->AdditionalSenseCode == 0x3A )
        {
            //
            // There is no media available.  This seems to be returned on the
            // first command after media was removed, or after the device was
            // started.  In this case, we will want to repeat commands that
            // don't need media present, except TEST UNIT READY which should
            // return this information.
            //
            switch( pCdb->AsByte[0] )
            {
            case SCSIOP_REQUEST_SENSE:
            case SCSIOP_INQUIRY:
            case SCSIOP_START_STOP_UNIT:
            case SCSIOP_MEDIUM_REMOVAL:
            case SCSIOP_GET_CONFIGURATION:
            case SCSIOP_GET_EVENT_STATUS:
            case SCSIOP_LOG_SELECT:
            case SCSIOP_LOG_SENSE:
            case SCSIOP_READ_BUFFER_CAPACITY:
            case SCSIOP_RECEIVE_DIAGNOSTIC:
            case SCSIOP_SEND_DIAGNOSTIC:
            case SCSIOP_WRITE_DATA_BUFF:
            case SCSIOP_LOAD_UNLOAD_SLOT:
            case SCSIOP_MECHANISM_STATUS:
//            case SCSIOP_MODE_SELECT10:
//            case SCSIOP_MODE_SENSE10:
                fTestUnitReady = TRUE;
                dwResult = RESULT_RETRY;
                break;

            default:
                dwResult = RESULT_FAIL;
                break;
            }

        }

        if( pSenseData->AdditionalSenseCode == 0x3E )
        {
            //
            // The drive hasn't "self-configured" yet.  huh
            //
            dwResult = RESULT_RETRY;
            fTestUnitReady = TRUE;
        }
    }
    else if( pSenseData->SenseKey == 0x05 )
    {
        //
        // From what I can see none of these are retry errors.  Either invalid
        // data has been sent to the device, the data was sent in an invalid
        // protocol, or the media is not compatible with the drive.
        //
        dwResult = RESULT_FAIL;
    }
    else if( pSenseData->SenseKey == 0x03 )
    {
        if( pSenseData->AdditionalSenseCode == 0x06 )
        {
            //
            // No reference position is found.
            //
            dwResult = RESULT_FAIL;
        }

        if( pSenseData->AdditionalSenseCode == 0x11 )
        {
            //
            // A general problem reading the media.  Is it going bad?
            //
            dwResult = RESULT_FAIL;
        }

        if( pSenseData->AdditionalSenseCode == 0x15 )
        {
            //
            // Mechanical/drive failure to read the media.
            //
            dwResult = RESULT_FAIL;
        }

        if( pSenseData->AdditionalSenseCode == 0x31 )
        {
            //
            // A physical formatting error on the media.
            //
            dwResult = RESULT_FAIL;
        }

        if( pSenseData->AdditionalSenseCode == 0x57 )
        {
            //
            // Cannot find TOC.
            //
            dwResult = RESULT_FAIL;
        }

        if( pSenseData->AdditionalSenseCode == 0x73 )
        {
            //
            // CD Control Error
            //
            dwResult = RESULT_FAIL;
        }
    }

    if( pCdb->AsByte[0] == 0 )
    {
        //
        // Don't get stuck in infinite recursion.
        //
        fTestUnitReady = FALSE;
    }

    if( fTestUnitReady )
    {
        CDROM_TESTUNITREADY CdReady = {0};
        DWORD dwBytesReturned = 0;
        BOOL fDone = FALSE;
        DWORD dwRetry = CMD_RETRY_COUNT;

        while( !fDone && dwRetry )
        {
            if( TestUnitReady( &CdReady, sizeof(CdReady), &dwBytesReturned ) &&
                CdReady.bUnitReady )
            {
                fDone = TRUE;
            }

            dwRetry -= 1;
        }
    }

    return dwResult;
}

// /////////////////////////////////////////////////////////////////////////////
// PollUnitReady
//
// This function just continues to check the for a media insertion/removal every
// n Seconds.  n can be defined in the registry.  The default is 2 seconds.
//
void Device_t::PollUnitReady()
{
    CDB Cdb = { 0 };
    SENSE_DATA SenseData = { 0 };
    DWORD dwBytesReturned = 0;
    WCHAR* strDiscName = new WCHAR[MAX_PATH];
    DWORD hStore = 0;
    DWORD dwTimeOut = 0;

    //
    // hStore will actually resolve to a StoreDisk_t object.
    //
    hStore = (DWORD)FSDMGR_DeviceHandleToHDSK( m_hDisc );

    if( !hStore )
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(L"CDROM!PollUnitReady - Unable to get device handle\r\n"));
        return;
    }

    if( !strDiscName )
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(L"CDROM!PollUnitReady - Unable to allocate space for the device name: %d\r\n", GetLastError()));
        return;
    }

    if( !FSDMGR_GetDiskName( hStore, strDiscName ) )
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(L"CDROM!PollUnitReady - Unable to query the disc name: %d\r\n", GetLastError()));
        goto exit_pollunitready;
    }

    while( TRUE )
    {
        if( !m_fPausePollThread &&
            !SendPassThrough( &Cdb,
                              NULL,
                              0,
                              TRUE,
                              GetGroupOneTimeout(),
                              &dwBytesReturned,
                              &SenseData ) )
        {
            if( SenseData.SenseKey == 2 &&
                SenseData.AdditionalSenseCode == 0x3A )
            {
                DEBUGMSG(ZONE_MEDIA,(TEXT("CDROM!PollUnitReady - Media is not present: %d\r\n"),SenseData.AdditionalSenseCodeQualifier));
            }
            else
            {
                DEBUGMSG(ZONE_MEDIA,(TEXT("CDROM!PollUnitReady - Unknown sense returned: %x\\%x\\%x\r\n"),SenseData.SenseKey,SenseData.AdditionalSenseCode,SenseData.AdditionalSenseCodeQualifier));
            }

            if( m_fMediaPresent )
            {
                //
                // TODO::If advertise interface fails then we should still try this again right?
                //
                m_fIsPlayingAudio = FALSE;
                m_fMediaPresent = FALSE;
                m_State |= CD_STATE_CHECK_MEDIA;

                if( !AdvertiseInterface( &STORAGE_MEDIA_GUID,
                                         strDiscName,
                                         FALSE ) )
                {
                    DEBUGMSG(ZONE_ERROR,(L"CDROM!PollUnitReady - Unable to advertise the media change: %d\r\n",GetLastError()));
                }
                else
                {
                    DEBUGMSG(ZONE_CURRENT,(L"CDROM!PollUnitReady - Successfully removed STORAGE_MEDIA_GUID advert.\r\n") );
                }
            }
        }
        else if( !m_fPausePollThread &&
                 m_fMediaPresent == FALSE )
        {

            DWORD dwMediaType = MED_TYPE_UNKNOWN;
            DWORD dwBytesReturned = 0;
            bool fAdvertise = true;

            if( DetermineMediaType( &dwMediaType,
                                    sizeof(dwMediaType),
                                    &dwBytesReturned ) )
            {
                m_MediaChangeResult = StorageMediaAttachResultUnchanged;

                if( dwMediaType != GetMediaType() )
                {
                    m_MediaChangeResult = StorageMediaAttachResultChanged;
                }

                if( !m_PartMgr.CompareTrackInfo() )
                {
                    m_MediaChangeResult = StorageMediaAttachResultChanged;
                }

                if( fAdvertise )
                {
                    if( !AdvertiseInterface( &STORAGE_MEDIA_GUID,
                                             strDiscName,
                                             TRUE ) )
                    {
                        DEBUGMSG(ZONE_ERROR,(L"CDROM!PollUnitReady - Unable to advertise the media change: %d\r\n",GetLastError()));
                        if( m_MediaChangeResult == StorageMediaAttachResultChanged )
                        {
                            //
                            // We need to clear the media type so that the next
                            // time we come through the polling thread we will
                            // be sure to mark the media as having changed again.
                            //
                            m_MediaType = 0;
                        }
                    }
                    else
                    {
                        DEBUGMSG(ZONE_CURRENT, (L"CDROM!PollUnitReady - Successfully advertised STORAGE_MEDIA_GUID - TRUE.\r\n"));
                        m_fMediaPresent = TRUE;
                    }
                }

                //
                // When the media has changed the file system manager will tear
                // down this partition driver and mount a new one.  We don't
                // want this thread to poll anymore as the new partition driver
                // will actually create one.
                //
                if( m_fMediaPresent &&
                    m_MediaChangeResult == StorageMediaAttachResultChanged )
                {
                    goto exit_pollunitready;
                }
            }

        }

        if( m_fPausePollThread )
        {
            dwTimeOut = WaitForSingleObject( m_hWakeUpEvent, INFINITE );
            if( dwTimeOut != WAIT_OBJECT_0 )
            {
                break;
            }
        }
        else
        {
            dwTimeOut = WaitForSingleObject( m_hWakeUpEvent, m_dwPollFrequency );
            if( dwTimeOut != WAIT_TIMEOUT )
            {
                break;
            }
        }
    }

exit_pollunitready:
    if( strDiscName )
    {
        delete [] strDiscName;
    }
}

// /////////////////////////////////////////////////////////////////////////////
// Lock
//
void Device_t::Lock()
{
    EnterCriticalSection( &m_cs );
}

// /////////////////////////////////////////////////////////////////////////////
// Unlock
//
void Device_t::Unlock()
{
    LeaveCriticalSection( &m_cs );
}

// -----------------------------------------------------------------------------
// Utility Functions
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// PollDeviceThread
//
DWORD PollDeviceThread( LPVOID lParam )
{
    Device_t* pDevice = (Device_t*)lParam;

    pDevice->PollUnitReady();

    return 0;
}



