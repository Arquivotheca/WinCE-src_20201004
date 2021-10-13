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

    WaveFile.cpp

Abstract:

    This file contains the code for WAVE-RIFF file library.  This library
    creates, opens, reads, writes, and closes WAVE-RIFF files.

Notes:

--*/
#include "WaveFile.h"

inline DWORD CWaveFile::WaveFileError( void )
{
    dwLastError = GetLastError();
    Close();
    return dwLastError;

} // end inline DWORD CWaveFile::WaveFileError( void )

inline DWORD CWaveFile::WaveFileError( DWORD dwError )
{
    dwLastError = dwError;
    Close();
    return dwLastError;

} // end

/*++

CWaveFile::Close:

    This function closes all of the file handles.

Arguments:

    NONE

Return Value:

    NONE

Notes:

--*/
inline VOID CWaveFile::Close()
{
    BOOL    bRtn;
    DWORD   dwRtn;
    BYTE    byTemp;

    if(INVALID_HANDLE_VALUE != hWaveData)
    {


    dwRtn = GetFileSize( hWaveData, NULL );
    if( dwRtn % 2 )
    {
        SetFilePointer( hWaveData, 0, NULL, FILE_END );
        byTemp = 0;
        WriteFile( hWaveData, &byTemp, 1, &dwRtn, NULL );
    }
    }

    if (INVALID_HANDLE_VALUE != hWaveData)
    {
        bRtn = CloseHandle( hWaveData );
        if( bRtn )
            hWaveData = INVALID_HANDLE_VALUE;

    }

    if (INVALID_HANDLE_VALUE != hRiffSize)
    {
        bRtn = CloseHandle( hRiffSize );
        if( bRtn )
            hRiffSize = INVALID_HANDLE_VALUE;
    }

    if (INVALID_HANDLE_VALUE != hWaveSize)
    {
        bRtn = CloseHandle( hWaveSize );
        if( bRtn )
            hWaveSize = INVALID_HANDLE_VALUE;
    }

    /* --------------------------------------------------------------------
        Reset all offset and sizes to zero
    -------------------------------------------------------------------- */
    nRiffSize = 0;
    nWaveSize = 0;
    dwDataOffSet = 0;
    dwInfoOffSet = 0;

    nSamples = 0;

} // end CWaveFile:Close();

/*++

CWaveFile::WriteSize:

    This function writes the new value of the RIFF chunks data size.

Arguments:

    HANDLE  hFileSize   -> Handle to write to.
    DWORD   nBytes     -> The number of bytes in the chunks data size.

Return Value:

    TRUE    ->  Success
    FALSE   ->  Failed

Notes:

--*/
inline BOOL CWaveFile::WriteSize(HANDLE hFileSize, DWORD nBytes)
{
    BOOL    bRtn;
    DWORD   dwRtn;

    if ( INVALID_HANDLE_VALUE == hFileSize )
        {
        return false;
        }

    bRtn = WriteFile( hFileSize, &nBytes, 4, &dwRtn, NULL );
    dwLastError = GetLastError();

    if( dwRtn < 4 )
        bRtn = FALSE;

    dwRtn = SetFilePointer( hFileSize,
                            -4,
                            NULL,
                            FILE_CURRENT );

    if( 0xFFFFFFFF == dwRtn )
    {
        bRtn = FALSE;
        dwLastError = GetLastError();
    }

    return bRtn;

} // end inline BOOL CWaveFile::WriteSize(HANDLE hFileSize, DWORD nBytes)

/*++

CWaveFile::CWaveFile:

    Default Constructor for CWaveFile

Arguments:

    NONE

Return Value:

    NONE

Notes:

--*/
CWaveFile::CWaveFile( )
{
    hWaveData = INVALID_HANDLE_VALUE;
    hRiffSize = INVALID_HANDLE_VALUE;
    hWaveSize = INVALID_HANDLE_VALUE;

    nRiffSize = 0;
    nWaveSize = 0;
    dwDataOffSet = 0;
    dwInfoOffSet = 0;

    nSamples = 0;

    dwLastError = ERROR_FILE_NOT_FOUND;

} // end CWaveFile::~CWaveFile()


/*++

CWaveFile::CWaveFile:

    Full constructor for a WAVE-RIFF file.

Arguments:

    lpFileName            -> Same As CreateFile
    dwDesiredAccess       -> Same As CreateFile, except can't be 0
    dwCreationDisposition -> Same As CreateFile
    dwFlagsAndAttributes  -> Same As CreateFile
    lplpWaveFormat        -> Pointer to a pointerto a wave format structure
    lplpInfo              -> Pointer to a pointer to LIST-INFO chunk data.

Return Value:

    NONE:

Notes:

    Most file CreateFile options are supported with the limitation that an
    open WAVE-RIFF file must be able to played and/or recorded.

    Also, security and templetes are not supported.

--*/
CWaveFile::CWaveFile( LPCTSTR   lpFileName,
                       DWORD    dwDesiredAccess,
                       DWORD    dwCreationDisposition ,
                       DWORD    dwFlagsAndAttributes,
                       LPVOID*  lplpWaveFormat,
                       LPVOID*  lplpInfo )
{
    hWaveData = INVALID_HANDLE_VALUE;
    hRiffSize = INVALID_HANDLE_VALUE;
    hWaveSize = INVALID_HANDLE_VALUE;

    nRiffSize = 0;
    nWaveSize = 0;
    dwDataOffSet = 0;
    dwInfoOffSet = 0;

    nSamples = 0;

    dwLastError = Create(lpFileName,
                         dwDesiredAccess,
                         dwCreationDisposition ,
                         dwFlagsAndAttributes,
                         lplpWaveFormat,
                         lplpInfo );

} // end CWaveFile::CWaveFile( ... )



/*++

CWaveFile::~CWaveFile:

    This is the class Destructor.

Arguments:

    NONE

Return Value:

    NONE

Notes:

--*/
CWaveFile::~CWaveFile()
{
    Close();

} // CWaveFile::~CWaveFile()


/*++

CWaveFile::Create:

    This function creates or opens WAVE-RIFF file with complete header
    information.

    If the file already exists and the file creation parameters allow it
    to be opened. lpPcmWaveFormat is filled with the WAVE-RIFF file's
    format data.  It is the calling function's responsability to call
    GetLastError in order to see if the file already existed.

    If the file already exists but isn't a legal wave file the HANDLE will
    be closed and INVALID_HANDLE_VALUE will be returned. GetLastError will
    be set to ERROR_FILE_CORRUPT.

Arguments:

    lpFileName            -> Same As CreateFile
    dwDesiredAccess       -> Same As CreateFile, except can't be 0
    dwCreationDisposition -> Same As CreateFile
    dwFlagsAndAttributes  -> Same As CreateFile
    lplpPcmWaveFormat     -> Pointer to a pointer to a wave format structure
    lplpInfo              -> Pointer to a pointer that will hold the info data

Return Value:

    If function is successful then ERROR_SUCCESS is returned.

    If the specified file exists before the function call and
    dwCreationDisposition is CREATE_ALWAYS or OPEN_ALWAYS, then
    ERROR_ALREADY_EXISTS (even though the function has succeeded).
    If the file does not exist before the call, function returns zero.

    If the function fails, the file creation error is returned.

Notes:

    Most file CreateFile options are supported with the limitation that an
    open WAVE-RIFF file must be able to be played and/or recorded.

    Also, security and templetes are not supported.

    If the file is new and lplpInfo isn't NULL then it is assumed that
    lplpInfo contains a valid LIST-INFO chunk and that chunk is writen
    to the file.

--*/
DWORD CWaveFile::Create( LPCTSTR         lpFileName,
                         DWORD           dwDesiredAccess,
                         DWORD           dwCreationDisposition ,
                         DWORD           dwFlagsAndAttributes,
                         LPVOID          *lplpPcmWaveFormat,
                         LPVOID          *lplpInfo )
{

    hWaveData = CreateFile( lpFileName,
                            dwDesiredAccess,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL,
                            dwCreationDisposition ,
                            dwFlagsAndAttributes,
                            NULL );
    if( INVALID_HANDLE_VALUE == hWaveData )
        return dwLastError = GetLastError();

    /* --------------------------------------------------------------------
        NOTE:  hRiffSize and hWaveSize are write optimizations.  They are
        not needed for read only files and therefore will not be used for
        read only files.
    -------------------------------------------------------------------- */
    if( dwDesiredAccess & GENERIC_WRITE )
    {
        hRiffSize = CreateFile( lpFileName,
                                dwDesiredAccess,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                dwCreationDisposition ,
                                dwFlagsAndAttributes,
                                NULL );
        if( INVALID_HANDLE_VALUE == hRiffSize )
            return dwLastError = GetLastError();

        hWaveSize = CreateFile( lpFileName,
                                dwDesiredAccess,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                dwCreationDisposition ,
                                dwFlagsAndAttributes,
                                NULL );
        if( INVALID_HANDLE_VALUE == hWaveSize )
            return dwLastError = GetLastError();

    } // if( GENERIC_WRITE )

    /* --------------------------------------------------------------------
        If file exists and has data verify that data is for a WAVE-RIFF
        file.  If so copy <wave-formate> and <PCM-format-specific> data to
        lpPcmWaveFormat.
    -------------------------------------------------------------------- */
    if( GetFileSize( hWaveData, NULL ) )
    {
        return GetFormat( lplpPcmWaveFormat, lplpInfo );
    }

    /* --------------------------------------------------------------------
        Initalize the WAVE-RIFF file
    -------------------------------------------------------------------- */
    return InitFile( (PCMWAVEFORMAT*)lplpPcmWaveFormat, lplpInfo );

} // end DWORD CWaveFile::Create( ... )


/*++

CWave::ReadData:

    This function reads the wave data from a wave file and puts it in the
    passed buffer.

Arguments:

    lpWaveData,                // address of buffer that receives WAVE data
    nNumberOfBytesToRead,    // number of bytes to read
    lpNumberOfBytesRead,    // address of number of bytes read

Return Value:

    Same as ReadFile

Notes:

--*/
BOOL CWaveFile::ReadData( LPVOID          lpWaveData,
                          DWORD           nNumberOfBytesToRead,
                          LPDWORD         lpNumberOfBytesRead )
{
    BOOL    bRtn;

    if(NULL == lpWaveData) {
        dwLastError = ERROR_INVALID_PARAMETER;
        return bRtn = FALSE;
    }

    bRtn = ReadFile( hWaveData,
                     lpWaveData,
                     nNumberOfBytesToRead,
                     lpNumberOfBytesRead,
                     NULL );

    dwLastError = GetLastError();

    return bRtn;

} // end BOOL CWaveFile::ReadData( ... )


/*++

CWaveFile::WriteData:

    This function writes a buffers worth of WAVE data to the WAVE file.

Arguments:

    lpWaveData,                  // address of buffer that receives WAVE data
    nNumberOfBytesToWrite,    // number of bytes to write
    lpNumberOfBytesWritten,    // address of number of bytes written


Return Value:

    SameAsWriteFile

Notes:

--*/
BOOL CWaveFile::WriteData( LPCVOID         lpWaveData,
                           DWORD           nNumberOfBytesToWrite,
                           LPDWORD         lpNumberOfBytesWritten )
{
    BOOL    bRtn;

    if(NULL == lpWaveData) {
        dwLastError = ERROR_INVALID_PARAMETER;
        return bRtn = FALSE;
    }

    bRtn = WriteFile( hWaveData,
                      lpWaveData,
                      nNumberOfBytesToWrite,
                      lpNumberOfBytesWritten,
                      NULL );
    dwLastError = GetLastError();

    if( bRtn )
    {
        dwLastError = ERROR_SUCCESS;

        nRiffSize += *lpNumberOfBytesWritten;
        nWaveSize += *lpNumberOfBytesWritten;
        /* ----------------------------------------------------------------
            Set the sizes sizes.
        ---------------------------------------------------------------- */
        SetEndOfFile( hWaveData ),
        WriteSize( hRiffSize, nRiffSize );
        WriteSize( hWaveSize, nWaveSize );
    } // end if( bRtn )

    return bRtn;

} // end BOOL CWaveFile::WriteData( ... );

/*++

DWORD CWaveFile::Rewind:

    This function rewinds the hWaveData pointer to
    the start of the wave data.

Arguments:

    NONE

Return Value:

    TRUE    ->  Success
    FALSE   ->  Failure

Notes:

--*/
BOOL CWaveFile::Rewind()
{
    DWORD   dwRtn;

    dwRtn = SetFilePointer( hWaveData, dwDataOffSet, NULL, FILE_BEGIN );

    dwLastError = GetLastError();

    return 0xFFFFFFFF != dwRtn;

} // end BOOL CWaveFile::Rewind()

/*++

DWORD CWaveFile::End:

    This function puts the hWaveData file pointer at the end of the file

Arguments:

    NONE

Return Value:

    TRUE    ->  Success
    FALSE   ->  Failure

Notes:

--*/
BOOL CWaveFile::End()
{
    DWORD   dwRtn;

    dwRtn = SetFilePointer( hWaveData, dwDataOffSet, NULL, FILE_END );

    dwLastError = GetLastError();

    return 0xFFFFFFFF != dwRtn;

} // end BOOL CWaveFile::End()


/*++

CWaveFile::MoveTo:

    This function moves the file pointer to passed sample offset.

Arguments:

    dwSample    The sample to move the file pointer to.

Return Value:

    ==  dwSample Move was a success.
    <   dwSample Move is not at end of file
    0   Some other error check CWaveFile::LastError()

Notes:

--*/
DWORD CWaveFile::MoveTo( DWORD dwSample )
{
    DWORD    dwOffset;
    DWORD    dwRtn;

    dwOffset = dwDataOffSet + m_PcmWaveFormat.wf.nBlockAlign * dwSample;

    dwRtn = SetFilePointer( hWaveData, dwOffset, NULL, FILE_BEGIN );
    if( 0xFFFFFFFF == dwRtn )
    {
        WaveFileError();
        return 0;
    }

    return (dwRtn - dwDataOffSet) / m_PcmWaveFormat.wf.nBlockAlign;

} // end DWORD CWaveFile::MoveTo( DWORD dwSample )

/*++

CWaveFile::GetFormat:

    This function reads the wave file from the front to determine if it
    is a WAVE-RIFF file and to read the format data.

Arguments:

    PCMWAVEFORMAT* lpPcmWaveFormat -> Pointer to the structure to fill with the
                                      wave format data.
    LPVOID*        lplpInfo        -> Currently not used, zeroed if not NULL.

Return Value:

    If the file is not a WAVE-RIFF file then ERROR_FILE_CORRUPT.

    Other errors will return file error codes

    For sucess ERROR_NOERROR is returned.

Notes:

--*/
DWORD CWaveFile::GetFormat( LPVOID* lplpWaveFormat,
                            LPVOID* lplpInfo )
{
    DWORD   dwBuffer;
    DWORD   dwRtn;
    BOOL    bRtn;

    if(NULL == lplpWaveFormat) {
        dwLastError = ERROR_INVALID_PARAMETER;
        return bRtn = FALSE;
    }

    if( NULL != lplpInfo ) *lplpInfo = NULL;

    /* --------------------------------------------------------------------
           Verify that the file is a RIFF formated file.
    -------------------------------------------------------------------- */
    bRtn = ReadFile( hWaveData, &dwBuffer, 4, &dwRtn, NULL );
    if( (!bRtn) || (4 != dwRtn) ) return WaveFileError();

    if( RIFF_FOURCC != dwBuffer ) return WaveFileError( ERROR_FILE_CORRUPT );

    /* --------------------------------------------------------------------
        If writing, set the file pointer to the RIFF chunk size DWORD.
    -------------------------------------------------------------------- */
    if( INVALID_HANDLE_VALUE != hRiffSize )
    {
        dwRtn = SetFilePointer( hRiffSize, 4, NULL, FILE_BEGIN );
        if( 0xFFFFFFFF == dwRtn ) return WaveFileError();
    }

    /* --------------------------------------------------------------------
        Read RIFF chunk size
    -------------------------------------------------------------------- */
    bRtn = ReadFile( hWaveData, &nRiffSize, 4, &dwRtn, NULL );
    if( (!bRtn) || (4 != dwRtn) ) return WaveFileError();

    /* --------------------------------------------------------------------
        Verify that the file is a WAVE type.
    -------------------------------------------------------------------- */
    bRtn = ReadFile( hWaveData, &dwBuffer, 4, &dwRtn, NULL );
    if( (!bRtn) || (4 != dwRtn) ) return WaveFileError();
    if( WAVE_FOURCC != dwBuffer ) return WaveFileError( ERROR_FILE_CORRUPT );

    /* --------------------------------------------------------------------
        Read the next FOURCC
    -------------------------------------------------------------------- */
    bRtn = ReadFile( hWaveData, &dwBuffer, 4, &dwRtn, NULL );
    if( (!bRtn) || (4 != dwRtn) ) return WaveFileError();

    /* --------------------------------------------------------------------
        Read all of the non data chunks.
    -------------------------------------------------------------------- */
    while ( data_FOURCC != dwBuffer )
    {
        switch( dwBuffer ) // FOURCC
        {

            case fmt_FOURCC:
                if ( ReadWaveFormat( lplpWaveFormat ) )
                    return dwLastError;
                break;

            case LIST_FOURCC:
                if ( ReadListInfo( lplpInfo ) )
              delete [] *lplpInfo; // cleanup since we dont use lplpInfo
                 *lplpInfo = NULL;
                    return dwLastError;
                break;

            case fact_FOURCC:
                if ( ReadFactChunk( ) )
                    return dwLastError;
                break;

            default:    // Skip unknown Chunk
                bRtn = ReadFile( hWaveData, &dwBuffer, 4, &dwRtn, NULL );
                if( !bRtn ) return WaveFileError();

                dwRtn = SetFilePointer( hWaveData, dwBuffer, NULL, FILE_CURRENT );
                if( 0xFFFFFFFF == dwRtn ) return WaveFileError();

        } // switch( dwBuffer )

        // Read Next FOURCC
        bRtn = ReadFile( hWaveData, &dwBuffer, 4, &dwRtn, NULL );
        if( (!bRtn) || (4 != dwRtn) ) return WaveFileError();

    } // end while ( data_FOURCC != dwBuffer )

    /* --------------------------------------------------------------------
        If writing Set hWaveSize pointer.
    -------------------------------------------------------------------- */
    if( INVALID_HANDLE_VALUE != hWaveSize )
    {
        // Get current file offset
        dwRtn = SetFilePointer( hWaveData, 0, NULL, FILE_CURRENT );
        if( 0xFFFFFFFF == dwRtn ) return WaveFileError();

        // Set hWaveSize to current file offset
        // thus we are moving the file dwRtn bytes from the file beginning
        dwRtn = SetFilePointer( hWaveSize, dwRtn, NULL, FILE_BEGIN );
        if( 0xFFFFFFFF == dwRtn ) return WaveFileError();
    }

    /* --------------------------------------------------------------------
        Read the wave data size
    -------------------------------------------------------------------- */
    bRtn = ReadFile( hWaveData, &nWaveSize, 4, &dwRtn, NULL );
    if( (!bRtn) || (4 != dwRtn) ) return WaveFileError();

    /* --------------------------------------------------------------------
        Set the data offset
    -------------------------------------------------------------------- */
    dwRtn = SetFilePointer( hWaveData, 0, NULL, FILE_CURRENT );
    if( 0xFFFFFFFF == dwRtn ) return WaveFileError();

    dwDataOffSet = dwRtn;

    return ERROR_SUCCESS;

} // DWORD CWaveFile::GetFormat( ... )


/*++

CWaveFile::ReadListInfo:

    This function reads a list info chunk started and hWaveData

Arguments:

    lplpInfo    pointer to pointer to info data

Return Value:

    ERROR_SUCCESS( 0 ) on success

Notes:

--*/
DWORD CWaveFile::ReadListInfo( LPVOID *lplpInfo )
{
    DWORD   dwInfoSize = 0;
    DWORD   dwRtn;
    BOOL    bRtn;

    if(NULL == lplpInfo) {
        dwLastError = ERROR_INVALID_PARAMETER;
        return bRtn = FALSE;
    }

    /* ----------------------------------------------------------------
        Set the LIST-INFO offset
    ---------------------------------------------------------------- */
    dwInfoOffSet = SetFilePointer( hWaveData, 0, NULL, FILE_CURRENT ) - 4;

    /* ----------------------------------------------------------------
        Get the LIST chunk size
    ---------------------------------------------------------------- */
    bRtn = ReadFile( hWaveData, &dwInfoSize, 4, &dwRtn, NULL );
    if( (!bRtn) || (4 != dwRtn) ) return WaveFileError();

    /* ----------------------------------------------------------------
        If the user wants the Info read it in else skip it.
    ---------------------------------------------------------------- */
    if( NULL != lplpInfo )
    {
        dwRtn = SetFilePointer( hWaveData, -8, NULL, FILE_CURRENT );
        if( 0xFFFFFFFF == dwRtn ) return WaveFileError();

        dwInfoSize += 8; // The complete size of the buffer.

        // Get a new buffer.
        *lplpInfo = new BYTE[dwInfoSize];

        /* ------------------------------------------------------------
            NOTE: if memory allocation fails simply skip reading
            info block and don't indicate error.
        ------------------------------------------------------------ */
        if( NULL != *lplpInfo )
        {

            bRtn = ReadFile( hWaveData, *lplpInfo, dwInfoSize, &dwRtn, NULL );
            if( (!bRtn) || (dwRtn != (dwInfoSize+8)) )
            {
                delete [] *lplpInfo;
                *lplpInfo = NULL;
                dwInfoOffSet = 0;
                return WaveFileError( ERROR_FILE_CORRUPT );

            } // end if( (!bRtn) || (dwRtn != (dwInfoSize+8)) )

        } // end if( NULL != *lplpInfo )

        else // ship info
        {
            dwRtn = SetFilePointer( hWaveData, dwInfoSize+8, NULL, FILE_CURRENT );
            if( 0xFFFFFFFF == dwRtn ) return WaveFileError();

        } // end if( NULL != *lplpInfo ) else

    } // end if( NULL != lplpInfo )

    else // skip info
    {
        dwRtn = SetFilePointer( hWaveData, dwInfoSize, NULL, FILE_CURRENT );
        if( 0xFFFFFFFF == dwRtn ) return WaveFileError();

    } // end if( NULL != lplpInfo )


    return dwLastError = ERROR_SUCCESS;

} // end DWORD CWaveFile::ReadListInfo( LPVOID *lplpInfo )

/*++

CWaveFile::ReadWaveFormat:

    This function reads the wave format from hWaveData

Arguments:

    lplpWaveFormat  Wave format data

Return Value:

    ERROR_SUCCESS(0) on success

Notes:

--*/
DWORD CWaveFile::ReadWaveFormat( LPVOID *lplpWaveFormat )
{
    DWORD   dwRtn;
    DWORD   dwBuffer;
    BOOL    bRtn;

   if(NULL == lplpWaveFormat){
        dwLastError = ERROR_INVALID_PARAMETER;
        return bRtn = FALSE;
    }

    /* --------------------------------------------------------------------
        Verify format data size.  Must be >= to sizeof(PCMWAVEFORMAT)
    -------------------------------------------------------------------- */
    bRtn = ReadFile( hWaveData, &dwBuffer, 4, &dwRtn, NULL );
    if( (!bRtn) || (4 != dwRtn) ) return WaveFileError();

    if( sizeof(PCMWAVEFORMAT) > dwBuffer )
        return WaveFileError( ERROR_FILE_CORRUPT );

    // Assumption lplpWaveFormat should not be NULL by above check
    // The additional size allows access to a WAVEFORMATEX
    // size member
    *lplpWaveFormat = new BYTE[dwBuffer+sizeof(WORD)];
    if( NULL == *lplpWaveFormat )
        return WaveFileError( ERROR_NOT_ENOUGH_MEMORY );

     ZeroMemory( *lplpWaveFormat, dwBuffer+sizeof(WORD) );
    /* --------------------------------------------------------------------
        Read the Wave file format information. All data from the fmt
        chunk is copyed.  Then the PCM data is saved locally and the
        chunk data is returned to the client.
    -------------------------------------------------------------------- */
    bRtn = ReadFile( hWaveData,
                     *lplpWaveFormat,
                     dwBuffer,
                     &dwRtn, NULL );
    if( (!bRtn) || (dwBuffer != dwRtn) ) return WaveFileError();

    /* --------------------------------------------------------------------
        Copy PCM Wave file fomat
    -------------------------------------------------------------------- */
    CopyMemory( &m_PcmWaveFormat, *lplpWaveFormat, sizeof(PCMWAVEFORMAT) );

    return dwLastError = ERROR_SUCCESS;

} // end DWORD CWaveFile::ReadWaveFormat( LPVOID *lplpWaveFormat )

/*++

CWaveFile::ReadFactChunk:

    This fuction reads nSamples from the fact chunk at hWaveData.

Arguments:

    NONE

Return Value:

    ERROR_SUCCESS(0) on success

Notes:

--*/

DWORD CWaveFile::ReadFactChunk( void )
{

    DWORD   dwRtn;
    DWORD   dwChunkLength;
    BOOL    bRtn;

    // Get the chunk size.
    bRtn = ReadFile( hWaveData, &dwChunkLength, 4, &dwRtn, NULL );
    if( (!bRtn) || (4 != dwRtn) ) return WaveFileError();

    if( 4 > dwChunkLength ) return dwLastError = ERROR_SUCCESS;

    // Read samples
    bRtn = ReadFile( hWaveData, &nSamples, 4, &dwRtn, NULL );
    if( (!bRtn) || (4 != dwRtn) ) return WaveFileError();

    // Skip the rest, if any.
    if( 4 == dwChunkLength ) return dwLastError = ERROR_SUCCESS;


    dwRtn = SetFilePointer( hWaveData, dwChunkLength - 4, NULL, FILE_CURRENT );
    if( 0xFFFFFFFF == dwRtn ) return WaveFileError();

    return dwLastError = ERROR_SUCCESS;

}  // end DWORD CWaveFile::ReadFactChunk( )

/*++

CWaveFile::InitFile:

    This function sets up a new WAVE-RIFF file.

Arguments:

       PCMWAVEFORMAT*  lpPcmWaveFormat -> Pointer to the structure to write the
                                          wave format data.
    LPVOID*         lplpInfo        -> Pointer to a pointer to wave info

Return Value:

    Return file error codes.

Notes:

--*/
DWORD CWaveFile::InitFile( PCMWAVEFORMAT*    lpPcmWaveFormat,
                           LPVOID*           lplpInfo )
{
    DWORD   dwChunkSize = 0;
    DWORD   dwRtn;
    BOOL    bRtn;
    LPDWORD lpChunk    = NULL;
    RIFFCK  riffck;
    CKHDR   ckhdr;

    if(NULL == lpPcmWaveFormat){
        dwLastError = ERROR_INVALID_PARAMETER;
        return bRtn = FALSE;
    }

    /* --------------------------------------------------------------------
        Write the RIFF-WAVE chunk
    -------------------------------------------------------------------- */
    nRiffSize = 4 + sizeof(CKHDR) + sizeof(PCMWAVEFORMAT) + sizeof(CKHDR);
    riffck.ckID = RIFF_FOURCC;
    riffck.ckSize = nRiffSize;
    riffck.ckForm = WAVE_FOURCC;

    bRtn = WriteFile( hWaveData, &riffck, sizeof(RIFFCK), &dwRtn, NULL);
    if( (!bRtn) || (sizeof(RIFFCK) != dwRtn) ) return WaveFileError();

    dwRtn = SetFilePointer( hRiffSize, 4, NULL, FILE_BEGIN );
    if( 0xFFFFFFFF == dwRtn ) return WaveFileError();

    /* --------------------------------------------------------------------
        If there is a chunk write it here.  It is assumed that the second word
        of the chunk contains the chunk size in bytes, and that this size
        doesn't include the first 2 words.  This block of code was originally
        intended to write an info chunk (Hence the variable name, lplpInfo) but
        it will write any chunk that fits the above description.
    -------------------------------------------------------------------- */
    if( NULL != lplpInfo )
    {
        lpChunk = LPDWORD(*lplpInfo);
        if( NULL != lpChunk )
        {
            if( INFO_FOURCC == *lpChunk ) dwInfoOffSet
                = SetFilePointer( hWaveData, 0, NULL, FILE_CURRENT );

            // Read the chunk data size directly from the second word of the
            // chunk then add 8 (for the Chunk ID & Chunk Data Size fields) to
            // get the full chunk size.
            dwChunkSize = *( lpChunk + 1 ) + 8;

            bRtn = WriteFile( hWaveData, lpChunk, dwChunkSize, &dwRtn, NULL);
            if( ( !bRtn ) || ( dwChunkSize != dwRtn ) ) return WaveFileError();
            nRiffSize += dwChunkSize;
            WriteSize( hRiffSize, nRiffSize );

        } // end if( NULL != *lpChunk )
    } // end if( NULL != lplpInfo )

    /* --------------------------------------------------------------------
        Write the WAVE-fmt chunk header
    -------------------------------------------------------------------- */
    ckhdr.ckID = fmt_FOURCC;
    ckhdr.ckSize = sizeof(PCMWAVEFORMAT);

    bRtn = WriteFile( hWaveData, &ckhdr, sizeof(CKHDR), &dwRtn, NULL);
    if( (!bRtn) || (sizeof(CKHDR) != dwRtn) ) return WaveFileError();

    bRtn = WriteFile( hWaveData,
                      lpPcmWaveFormat,
                      sizeof(PCMWAVEFORMAT),
                      &dwRtn, NULL);
    if( (!bRtn) || (sizeof(PCMWAVEFORMAT) != dwRtn) ) return WaveFileError();

    /* --------------------------------------------------------------------
        Copy PCM Wave file fomat
    -------------------------------------------------------------------- */
    CopyMemory( &m_PcmWaveFormat, lpPcmWaveFormat, sizeof(PCMWAVEFORMAT) );

    /* --------------------------------------------------------------------
        Write the WAVE-data chunk header
    -------------------------------------------------------------------- */
    ckhdr.ckID = data_FOURCC;
    ckhdr.ckSize = 0;

    bRtn = WriteFile( hWaveData, &ckhdr, sizeof(CKHDR), &dwRtn, NULL);
    if( (!bRtn) || (sizeof(CKHDR) != dwRtn) ) return WaveFileError();

    if( INVALID_HANDLE_VALUE != hWaveSize )
    {
        dwRtn = SetFilePointer( hWaveData, 0, NULL, FILE_CURRENT );
        if( 0xFFFFFFFF == dwRtn ) return WaveFileError();

        dwDataOffSet = dwRtn;

        dwRtn = SetFilePointer( hWaveSize, (dwDataOffSet - 4), NULL, FILE_BEGIN );
        if( 0xFFFFFFFF == dwRtn ) return WaveFileError();
    }

    return ERROR_SUCCESS;

} // end DWORD CWaveFile::InitFile( ... )