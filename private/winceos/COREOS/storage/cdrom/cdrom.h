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

#define USE_COMPAT_SETTINGS 1

#include <windows.h>
#include <extfile.h>
#include <fsdmgr.h>
#include <dbt.h>
#include <storemgr.h>
#include <partdrv.h>
#include <scsi.h>
#include <ntddscsi.h>
#include <ntddmmc.h>
#include <cdioctl.h>
#include <dvdioctl.h>
#include <atapi2.h>

#include <types.h>

#include "cddbg.h"

#define MAX_SECTORS_PER_COMMAND 512

#define CD_STATE_NORMAL      0x00000000
#define CD_STATE_CHECK_MEDIA 0x00000001

#define CD_ROM_POLL_FREQUENCY L"CDROMPollFrequency"

typedef enum
{
    RESULT_RETRY,
    RESULT_FAIL,
    RESULT_UNKNOWN
} ERROR_RESULT;

#define CMD_RETRY_COUNT 3

//
//  This macro copies an src longword to a dst longword,
//  performing an little/big endian swap.
//
#define SwapCopyUchar4(Dst,Src) {                                        \
    *((UCHAR *)(Dst)) = *((UCHAR *)(Src) + 3);     \
    *((UCHAR *)(Dst) + 1) = *((UCHAR *)(Src) + 2); \
    *((UCHAR *)(Dst) + 2) = *((UCHAR *)(Src) + 1); \
    *((UCHAR *)(Dst) + 3) = *((UCHAR *)(Src));     \
}

class CdPartMgr_t;
class Device_t;

typedef struct
{
    Device_t* pDevice;
    USHORT SessionIndex;
} SEARCH_CONTEXT, *PSEARCH_CONTEXT, PARTITION_CONTEXT, *PPARTITION_CONTEXT;

// /////////////////////////////////////////////////////////////////////////////
// CdPartMgr_t
//
// This class handles the partition support for the device.
class CdPartMgr_t
{
public:
    CdPartMgr_t();
    ~CdPartMgr_t();

    bool Initialize( Device_t* pDevice );
    DWORD GetPartitionInfo( LPCTSTR strPartName, PD_PARTINFO* pInfo );
    DWORD GetPartitionInfo( USHORT Partition, PD_PARTINFO* pInfo );
    DWORD FindPartitionNext( PSEARCH_CONTEXT pSearchContext,
                             PD_PARTINFO *pInfo );
    bool RefreshPartInfo();

    USHORT GetNumberOfTracks() const { return m_NumTracks; }
    USHORT GetNumberOfSessions() const { return m_NumSessions; }

    BOOL GetIndexFromName( const WCHAR* strPartName, USHORT* pIndex );

    BOOL GetSessionInfo( USHORT Session,
                         PCD_PARTITION_INFO pPartInfo,
                         DWORD dwPartInfoSize,
                         DWORD* pdwBytesReturned );
    USHORT GetLastNonEmptyDataSession();
    USHORT GetLastValidTrackIndexInSession( USHORT Session );
    DWORD GetFirstSectorInSession( USHORT Session );
    BOOL CompareTrackInfo();
    BOOL IsValidPartition( USHORT Partition );

public:
    USHORT GetFirstTrack() const { return m_pTrackInfo[0].TrackNumberLsb +
                                         (m_pTrackInfo[0].TrackNumberMsb << 8); }

private:
    USHORT GetFirstTrackIndexFromSession( USHORT TargetSession );
    USHORT GetLastTrackIndexFromSession( USHORT TargetSession );
    USHORT GetFirstTrackFromSession( USHORT TargetSession );
    USHORT GetLastTrackFromSession( USHORT TargetSession );
    USHORT GetTracksInSession( USHORT TargetSession );
    USHORT GetNumberOfAudioSessions();
    USHORT GetAudioSessionIndex( USHORT AudioSession );

    DWORD GetSectorsInSession( USHORT TargetSession );

private:
    Device_t* m_pDevice;

    USHORT m_NumSessions;
    USHORT m_NumTracks;
    PTRACK_INFORMATION2 m_pTrackInfo;
};

class Device_t
{
public:
    Device_t();
    ~Device_t();

    BOOL Initialize( HANDLE hDisc );

    BOOL CdDeviceIoControl( USHORT PartitionIndex,
                            DWORD IoControlCode,
                            LPVOID pInBuf,
                            DWORD InBufSize,
                            LPVOID pOutBuf,
                            DWORD OutBufSize,
                            LPDWORD pBytesReturned,
                            LPOVERLAPPED pOverlapped );

    BOOL CdDeviceStoreIoControl( DWORD IoControlCode,
                                 LPVOID pInBuf,
                                 DWORD InBufSize,
                                 LPVOID pOutBuf,
                                 DWORD OutBufSize,
                                 LPDWORD pBytesReturned,
                                 LPOVERLAPPED pOverlapped );

    BOOL SendPassThrough( PCDB pCdb,
                          __in_ecount_opt(dwBufferSize) BYTE* pDataBuffer,
                          DWORD dwBufferSize,
                          BOOL fDataIn,
                          DWORD dwTimeout,
                          DWORD* pdwBytesReturned,
                          PSENSE_DATA pSenseData );

    BOOL SendPassThroughDirect( PCDB pCdb,
                                BYTE* pDataBuffer,
                                DWORD dwBufferSize,
                                BOOL fDataIn,
                                DWORD dwTimeout,
                                DWORD* pdwBytesReturned,
                                PSENSE_DATA pSenseData );

    USHORT GetGroupOneTimeout() const { return m_Group1Timeout; }
    USHORT GetGroupTwoTimeout() const { return m_Group2Timeout; }
    DWORD  GetMediaType() const { return m_MediaType; }

    CdPartMgr_t* GetPartMgr() { return &m_PartMgr; }

    BOOL DetermineMediaType( PVOID pOutBuffer,
                             DWORD dwOutBufSize,
                             DWORD* pdwBytesReturned );

    STORAGE_MEDIA_ATTACH_RESULT GetMediaChangeResult() const { return m_MediaChangeResult; }

protected:
    BOOL FeatureIsActive( FEATURE_NUMBER Feature );
    BOOL FeatureIsActive( FEATURE_NUMBER Feature,
                          BYTE* pBuffer,
                          DWORD BufferSize );
    BOOL Device_t::DumpActiveFeatures( BYTE* pBuffer, DWORD BufferSize );
    BOOL FeatureIsSupported( FEATURE_NUMBER Feature );
    BOOL GetTimeoutValues();
    DWORD GetBlockingFactor() const;

protected:
    BOOL GetPartitionInfo( USHORT Partition,
                           BYTE* pOutBuffer,
                           DWORD dwOutBufSize,
                           DWORD* pdwBytesReturned );
    BOOL GetCurrentFeatures( BYTE** ppBuffer, DWORD* pBufferSize );
    BOOL ReadSGTriage( PVOID pInBuffer,
                       DWORD dwInBufSize,
                       PVOID pOutBuffer,
                       DWORD dwOutBufSize,
                       DWORD* pdwBytesReturned );
    BOOL ReadSG( CDROM_READ& CdRead,
                 PSGX_BUF pBuffers,
                 DWORD* pdwBytesReturned );
    BOOL ReadCdRaw( PVOID pInBuffer,
                    DWORD dwInBufSize,
                    PVOID pOutBuffer,
                    DWORD dwOutBufSize,
                    DWORD* pdwBytesReturned );
    BOOL TestUnitReady( PVOID pOutBuffer,
                        DWORD dwOutBufSize,
                        DWORD* pdwBytesReturned );
    BOOL GetDiscInformation( PVOID pInBuffer,
                             DWORD dwInBufSize,
                             PVOID pOutBuffer,
                             DWORD dwOutBufSize,
                             DWORD* pdwBytesReturned );
    BOOL EjectMedia();
    BOOL LoadMedia();
    BOOL IssueInquiry( VOID* pOutBuffer,
                       DWORD dwOutBufSize,
                       DWORD* pdwBytesReturned );
    BOOL ReadTOC( VOID* pOutBuffer,
                  DWORD dwOutBufSize,
                  DWORD* pdwBytesReturned );

    BOOL StartDVDSession( VOID* pOutBuffer,
                          DWORD dwOutBufSize,
                          DWORD* pdwBytesReturned );
    BOOL ReadDVDKey( VOID* pInBuffer,
                     DWORD dwInBufSize,
                     VOID* pOutBuffer,
                     DWORD dwOutBufSize,
                     DWORD* pdwBytesReturned );
    BOOL EndDVDSession( VOID* pInBuffer, DWORD dwInBufSize );
    BOOL SendDVDKey( VOID* pInBuffer, DWORD dwInBufSize );
    BOOL GetRegion( VOID* pOutBuffer,
                    DWORD dwOutBufSize,
                    DWORD* pdwBytesReturned );
    BOOL ReadQSubChannel( VOID* pInBuffer,
                          DWORD dwInBufSize,
                          VOID* pOutBuffer,
                          DWORD dwOutBufSize,
                          DWORD* pdwBytesReturned );
    BOOL ControlAudio( DWORD dwIoCode,
                       VOID* pInBuffer,
                       DWORD dwInBufSize );

    DWORD HandleCommandError( PCDB pCdb, SENSE_DATA* pSenseData );

public:
    void PollUnitReady();
    void Lock();
    void Unlock();

private:
    HANDLE m_hDisc;
    BOOL m_fIsPlayingAudio;
    BOOL m_fUseLegacyReadIOCTL;

    STORAGE_MEDIA_ATTACH_RESULT m_MediaChangeResult;

    CdPartMgr_t m_PartMgr;

    USHORT m_Group1Timeout;
    USHORT m_Group2Timeout;

    BOOL m_fMediaPresent;
    DWORD m_MediaType;
    DWORD m_State;
    DWORD m_dwPollFrequency;

    BOOL m_fPausePollThread;
    HANDLE m_hWakeUpEvent;
    HANDLE m_hTestUnitThread;
    CRITICAL_SECTION m_cs;
};

DWORD PollDeviceThread( LPVOID lParam );


typedef struct _PASS_THROUGH_DIRECT
{
    SCSI_PASS_THROUGH_DIRECT PassThrough;
    SENSE_DATA SenseData;
} PASS_THROUGH_DIRECT, *PPASS_THROUGH_DIRECT;
