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
/*++

Module Name:

    WaveFileEx.cpp

Abstract:

    This file contains the implementation code for an extension of the
    WAVE-RIFF file library.  This extension provides additional functionality,
    such as adding and manipulating chunks within the WAV file.

Notes:

--*/
#include "WaveFileEx.h"

void LogWaveFormat( PCMWAVEFORMAT PcmWaveFormat )
{

    LOG( TEXT( "-- Format tag = %d, Channels = %d, samples/sec = %d, Bytes Per Sec = %d, Block Align = %d, Bits Per Sample = %d" ),
        PcmWaveFormat.wf.wFormatTag,
        PcmWaveFormat.wf.nChannels,
        PcmWaveFormat.wf.nSamplesPerSec,
        PcmWaveFormat.wf.nAvgBytesPerSec,
        PcmWaveFormat.wf.nBlockAlign,
        PcmWaveFormat.wBitsPerSample    );
    return;
} // LogWaveFormat( PCMWAVEFORMAT

void LogWaveFormat( WAVEFORMAT wf )
{
    LOG( TEXT(
        "-- Format tag = %d, Channels = %d, samples/sec = %d, Bytes Per Sec = %d, Block Align = %d" ),
        wf.wFormatTag,
        wf.nChannels,
        wf.nSamplesPerSec,
        wf.nAvgBytesPerSec,
        wf.nBlockAlign    );
    return;
} // LogWaveFormat( WAVEFORMAT

/*++

CWaveFile::CWaveFile:

    Default Constructor for CWaveFileEx

Arguments:

    NONE

Return Value:

    NONE

Notes:

--*/
CWaveFileEx::CWaveFileEx() : CWaveFile()
{
   LOG( TEXT( "Constructing audio file." ) );

    dwNoOfCueChunks  = 0;
    dwNoOfFactChunks = 0;
    dwNoOfPlstChunks = 0;

}

/*++

CWaveFileEx::CWaveFileEx:

    Full constructor for a CWaveFileEx WAVE-RIFF

Arguments:

    lpFileName            -> Same As CreateFile
    dwDesiredAccess       -> Same As CreateFile, except can't be 0
    dwCreationDisposition -> Same As CreateFile
    dwFlagsAndAttributes  -> Same As CreateFile
    lplpWaveFormat        -> Pointer to a pointer to a structure containing
                             wave format parameters
    lplpOptChunk          -> Pointer to a pointer to a structure containing
                             optional chunk information. The second word must
                             contain the size - 8 (in bytes) of the chunck.

Return Value:

    NONE:

Notes:

    CWaveFile supports most file CreateFile options with the limitation that an
    open WAVE-RIFF file must be able to played and/or recorded.

    Also, security and templetes are not supported.

--*/
CWaveFileEx::CWaveFileEx( LPCTSTR lpFileName,
                          DWORD   dwDesiredAccess,
                          DWORD   dwCreationDisposition,
                          DWORD   dwFlagsAndAttributes,
                          LPVOID* lplpWaveFormat,
                          LPVOID* lplpOptChunk         ) : CWaveFile()
{
    LOG( TEXT( "Constructing audio file." ) );

    if( !lplpWaveFormat )
    {
        LOG( TEXT( "ERROR in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
        LOG( TEXT( "\tlplpWaveFormat is NULL." ) );
        return;
    } // if !lplpWaveFormat
    const PCMWAVEFORMAT* pWF = (PCMWAVEFORMAT*)lplpWaveFormat;

    if( !pWF )
    {
        LOG( TEXT( "ERROR in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
        LOG( TEXT( "\tpWF is NULL." ) );
        return;
    } // if !lplpWaveFormat
    PCMWAVEFORMAT   WF   = *pWF;

    //LogWaveFormat( pPCMWF );
    if( lplpWaveFormat != NULL) LogWaveFormat( WF );

    dwLastError = Create( lpFileName, dwDesiredAccess, dwCreationDisposition,
                dwFlagsAndAttributes, lplpWaveFormat , lplpOptChunk         );
    switch( dwLastError )
    {
    case ERROR_SUCCESS:
        LOG( TEXT( "File %s created." ), lpFileName );
        break;
    case ERROR_ALREADY_EXISTS:
        if(    ( CREATE_ALWAYS == ( CREATE_ALWAYS & dwCreationDisposition ) )
            || ( OPEN_ALWAYS   == ( OPEN_ALWAYS   & dwCreationDisposition ) ) )

            LOG( TEXT(
            "File %s already exists.  It\'s content has been overwritten." ),
            lpFileName );

        else
        {
            LOG(
                TEXT( "ERROR in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
            LOG( TEXT( "\tFile %s already exists." ) );
        } // else

        break;
    default:
        LOG(
            TEXT( "ERROR in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
        LOG( TEXT( "\tCreate() failed.  File %s not created." ), lpFileName );
        break;
   }

    if( lplpWaveFormat != NULL) LogWaveFormat( WF );

    dwNoOfCueChunks  = 0;
    dwNoOfFactChunks = 0;
    dwNoOfPlstChunks = 0;
} // CWaveFileEx::CWaveFileEx

/*++

CWaveFileEx::~CWaveFileEx:

    This is the class Destructor.

Arguments:

    NONE

Return Value:

    NONE

Notes:

--*/

CWaveFileEx::~CWaveFileEx()
{
   LOG( TEXT( "Destructing audio file." ) );
}

/*++

CWaveFileEx::AddRandomChunk:

    This function randomely writes a chunk of random type to the audio file.

Arguments:

    NONE

Return Value:

    0 -> no chunk added
    1 -> cue chunk added
    2 -> fact chunk added
    3 -> plst chunk added

Notes:

--*/

int CWaveFileEx::AddRandomChunk()
{
    // Purpose: To append a random chunk onto the audio file.
    int iChunkNum;
    UINT uNumber = 0;

    rand_s(&uNumber);
    iChunkNum = uNumber % 4;
    switch( iChunkNum )
    {
    case 1:
        if( WriteCueChunk() ) LOG(
            TEXT( "AddRandomChunk() added a cue chunk to the audio file." ) );
        else iChunkNum = 0;
        break;
    case 2:
        if( WriteFactChunk() ) LOG(
            TEXT( "AddRandomChunk() added a fact chunk to the audio file." ) );
        else iChunkNum = 0;
        break;
    case 3:
        if( WritePlstChunk() ) LOG(
            TEXT( "AddRandomChunk() added a plst chunk to the audio file." ) );
        else iChunkNum = 0;
        break;
    default:
        iChunkNum = 0;
        break;
    } // switch iChunkNum

    if( 0 == iChunkNum )
        LOG( TEXT(
        "AddRandomChunk() called, but didn\'t add any new chunks to the audio file."
        ) );

    return iChunkNum;

} // AddRandomChunk

/*++

CWaveFileEx::WriteCueChunk:

    This function writes an empty cues chunk to the audio file.

Arguments:

    NONE

Return Value:

    TRUE  -> chunk added
    FALSE -> chunk not added

Notes:

--*/

BOOL CWaveFileEx::WriteCueChunk()
{

    BOOL  bRtn;
    CUECK cueck;
    DWORD dwNumberOfBytesWritten = 0;

    if( dwNoOfCueChunks )
    {
        LOG( TEXT(
            "INFORMATION:\tWriteCueChunk() returning without adding cue chunk because the audio file already contains one."
            ) );
        return FALSE;
    } // if dwNoOfCueChunks
    LOG( TEXT( "Adding cue chunk to file." ) );

    cueck.ckID      = cue_FOURCC;
    cueck.ckSize    = 4;
    cueck.numCuePts = 0;

    bRtn = WriteFile(
        hWaveData, &cueck, sizeof( CUECK ), &dwNumberOfBytesWritten, NULL );
    dwLastError = GetLastError();

    if( ( !bRtn ) || ( sizeof( CUECK ) != dwNumberOfBytesWritten ) )
    {
        LOG( TEXT( "ERROR in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
        LOG( TEXT( "\tWriteFile() failed." ) );
        return FALSE;
    } // if

    nRiffSize += dwNumberOfBytesWritten;
    SetEndOfFile( hWaveData );
    WriteSize( hRiffSize, nRiffSize );

    dwNoOfCueChunks++;
    return TRUE;

} // CWaveFileEx::WriteCueChunk()

/*++

CWaveFileEx::WriteFactChunk:

    This function writes a fact chunk to the audio file.

Arguments:

    NONE

Return Value:

    TRUE  -> chunk added
    FALSE -> chunk not added

Notes:

--*/

BOOL CWaveFileEx::WriteFactChunk()
{

    BOOL   bRtn;
    FACTCK factck;
    DWORD  dwNumberOfBytesWritten = 0;

    if( dwNoOfFactChunks )
    {
        LOG( TEXT(
            "INFORMATION:\tWriteFactChunk() returning without adding fact chunk because the audio file already contains one."
            ) );
        return FALSE;
    } // if dwNoOfFactChunks
    LOG( TEXT( "Adding fact chunk to file." ) );

    factck.ckID   = fact_FOURCC;
    factck.ckSize = 4;
    factck.ckData = nSamples;

    bRtn = WriteFile(
        hWaveData, &factck, sizeof( FACTCK ), &dwNumberOfBytesWritten, NULL );
    dwLastError = GetLastError();

    if( ( !bRtn ) || ( sizeof( FACTCK ) != dwNumberOfBytesWritten ) )
    {
        LOG( TEXT( "ERROR in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
        LOG( TEXT( "\tWriteFile() failed." ) );
        return TRUE;
    } // if

    nRiffSize += dwNumberOfBytesWritten;
    SetEndOfFile( hWaveData );
    WriteSize( hRiffSize, nRiffSize );

    dwNoOfFactChunks++;
    return TRUE;

} // CWaveFileEx::WriteFactChunk()

/*++

CWaveFileEx::WritePlstChunk:

    This function writes a plst chunk to the audio file.

Arguments:

    NONE

Return Value:

    TRUE  -> chunk added
    FALSE -> chunk not added

Notes:

--*/

BOOL CWaveFileEx::WritePlstChunk()
{

    BOOL   bRtn;
    PLSTCK plstck;
    DWORD  dwNumberOfBytesWritten = 0;

    if( dwNoOfPlstChunks )
    {
        LOG( TEXT(
            "INFORMATION:\tWritePlstChunk() returning without adding plst chunk because the audio file already contains one."
            ) );
        return FALSE;
    } // if dwNoOfPlstChunks
    LOG( TEXT( "Adding plst chunk to file." ) );

    plstck.ckID    = plst_FOURCC;
    plstck.ckSize  = 0;
    plstck.numSegs = 0;

    bRtn = WriteFile(
        hWaveData, &plstck, sizeof( PLSTCK ), &dwNumberOfBytesWritten, NULL );
    dwLastError = GetLastError();

    if( ( !bRtn ) || ( sizeof( PLSTCK ) != dwNumberOfBytesWritten ) )
    {
        LOG( TEXT( "ERROR in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
        LOG( TEXT( "\tWriteFile() failed." ) );
        return FALSE;
    } // if

    nRiffSize += dwNumberOfBytesWritten;
    SetEndOfFile( hWaveData );
    WriteSize( hRiffSize, nRiffSize );

    dwNoOfPlstChunks++;
    return TRUE;

} // CWaveFileEx::WritePlstChunk()