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

extern "C"
{

HANDLE UDF_CreateFile( CVolume* pVolume,
                       HANDLE hProc,
                       LPCWSTR FileName,
                       DWORD Access,
                       DWORD ShareMode,
                       LPSECURITY_ATTRIBUTES pSecurityAttributes,
                       DWORD Create,
                       DWORD FlagsAndAttributes,
                       HANDLE hTemplateFile )
{
    HANDLE Handle = INVALID_HANDLE_VALUE;
    LRESULT Result = ERROR_SUCCESS;
    DWORD dwSize = 0;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_CreateFileW(%s)\r\n"), FileName));

    if( !::IsKModeAddr( (DWORD)pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit_createfile;
    }

    Result = pVolume->CreateFile( hProc,
                                  FileName,
                                  Access,
                                  ShareMode,
                                  pSecurityAttributes,
                                  Create,
                                  FlagsAndAttributes,
                                  &Handle );


exit_createfile:
    if (Result != ERROR_SUCCESS)
    {
        SetLastError (Result);
    }

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_CreateFileW returned Handle: 0x%x, LRESULT 0x%x\r\n"), Handle, Result));
    return Handle;
}


BOOL UDF_CloseFile( PFILE_HANDLE pHandle )
{
    LRESULT Result = ERROR_SUCCESS;
    CVolume* pVolume = NULL;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_CloseFile(0x%x)\r\n"), pHandle));

    if (!pHandle)
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
        !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    pVolume = pHandle->pStream->GetVolume();
    if( pVolume )
    {
        //
        // TODO::When do we want to close the flush the file handle?
        //
        Result = pVolume->CloseFile( pHandle, FALSE );
    }

exit:
    if (Result != ERROR_SUCCESS)
    {
        SetLastError (Result);
    }

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_CloseFile returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}


BOOL UDF_ReadFile( PFILE_HANDLE pHandle,
                   LPVOID Buffer,
                   DWORD BytesToRead,
                   LPDWORD pNumBytesRead,
                   LPOVERLAPPED pOverlapped )
{
    DWORD NumBytesRead = 0;
    LRESULT Result = ERROR_SUCCESS;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_ReadFile(Handle: 0x%x, 0x%x bytes, Position 0x%x)\r\n"), pHandle, BytesToRead, pHandle->Position));

    if( !pHandle )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }
    if( !(pHandle->dwAccess & GENERIC_READ) )
    {
        Result = ERROR_ACCESS_DENIED;
        goto exit;
    }

    if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
        !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    //
    // This lock shouldn't be needed but...
    // This isn't too big a deal as we will need to lock
    // internally once this is a writeable file system anyways.
    //
    pHandle->pStream->Lock();

    if( pHandle->pStream )
    {
        Result = pHandle->pStream->ReadFile( pHandle->Position,
                                             Buffer,
                                             BytesToRead,
                                             &NumBytesRead );
    }

    pHandle->Position += NumBytesRead;
    pHandle->pStream->Unlock();

    if( pNumBytesRead )
    {
        *pNumBytesRead = NumBytesRead;
    }

exit:
    if (Result != ERROR_SUCCESS)
    {
        SetLastError (Result);
    }

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_ReadFile returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}


BOOL UDF_ReadFileWithSeek( PFILE_HANDLE pHandle,
                           LPVOID Buffer,
                           DWORD BytesToRead,
                           LPDWORD pNumBytesRead,
                           LPOVERLAPPED pOverlapped,
                           DWORD LowOffset,
                           DWORD HighOffset )
{
    DWORD NumBytesRead = 0;
    LRESULT Result = ERROR_SUCCESS;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_ReadFileWithSeek (Handle: 0x%x, 0x%x bytes, Position 0x%x,0x%x)\r\n"), pHandle, BytesToRead, HighOffset, LowOffset));

    if( !pHandle )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }
    if( !(pHandle->dwAccess & GENERIC_READ) )
    {
        Result = ERROR_ACCESS_DENIED;
        goto exit;
    }

    if( Buffer == NULL && BytesToRead == 0 )
    {
        // The caller is probing to see if the file supports RFWS
        Result = ERROR_SUCCESS;
        goto exit;
    }

    ULARGE_INTEGER Offset;
    Offset.LowPart = LowOffset;
    Offset.HighPart = HighOffset;

    if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
        !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( pHandle->pStream )
    {
        Result = pHandle->pStream->ReadFile( Offset.QuadPart,
                                             Buffer,
                                             BytesToRead,
                                             &NumBytesRead );
    }

    if( pNumBytesRead )
    {
        *pNumBytesRead = NumBytesRead;
    }

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_ReadFileWithSeek returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}

BOOL UDF_ReadFileScatter( PFILE_HANDLE pHandle,
                          FILE_SEGMENT_ELEMENT SegmentArray[],
                          DWORD NumberOfBytesToRead,
                          LPDWORD pReserved,
                          LPOVERLAPPED pOverlapped )
{
    LRESULT Result = ERROR_SUCCESS;

    // Interpret lpReserved as an array of 64-bit offsets if provided.
    FILE_SEGMENT_ELEMENT* OffsetArray = (FILE_SEGMENT_ELEMENT*)pReserved;

    DEBUGMSG(ZONE_APIS, (TEXT("UDFS!UDF_ReadFileScatter(Handle: 0x%x, 0x%x bytes)\r\n"), pHandle, NumberOfBytesToRead));

    if( !pHandle )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }
    if( !(pHandle->dwAccess & GENERIC_READ) )
    {
        Result = ERROR_ACCESS_DENIED;
        goto exit;
    }

    if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
        !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( pHandle->pStream )
    {
        Result = pHandle->pStream->ReadFileScatter( SegmentArray,
                                                    NumberOfBytesToRead,
                                                    OffsetArray,
                                                    pHandle->Position );
    }

    if( (Result == ERROR_SUCCESS) && !OffsetArray )
    {
        // Update file handle position
        //
        pHandle->Position += NumberOfBytesToRead;
    }

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_ReadFileScatter returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);

}

BOOL UDF_WriteFile( PFILE_HANDLE pHandle,
                    LPVOID Buffer,
                    DWORD BytesToWrite,
                    LPDWORD pNumBytesWritten,
                    LPOVERLAPPED pOverlapped )
{
    DWORD NumBytesWritten = 0;
    LRESULT Result = ERROR_SUCCESS;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_WriteFile(Handle: 0x%x, 0x%x bytes, Position 0x%x)\r\n"), pHandle, BytesToWrite, pHandle->Position));

    if( !pHandle )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }
    if( !(pHandle->dwAccess & GENERIC_WRITE) )
    {
        Result = ERROR_ACCESS_DENIED;
        goto exit;
    }

    if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
        !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( pHandle->pStream )
    {
        Result = pHandle->pStream->WriteFile( pHandle->Position,
                                              Buffer,
                                              BytesToWrite,
                                              &NumBytesWritten );
    }

    pHandle->Position += NumBytesWritten;

    if( pNumBytesWritten )
    {
        *pNumBytesWritten = NumBytesWritten;
    }

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_WriteFile returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}


BOOL UDF_WriteFileWithSeek( PFILE_HANDLE pHandle,
                            LPVOID Buffer,
                            DWORD BytesToWrite,
                            LPDWORD pNumBytesWritten,
                            LPOVERLAPPED pOverlapped,
                            DWORD LowOffset,
                            DWORD HighOffset )
{
    DWORD NumBytesWritten = 0;
    LRESULT Result = ERROR_SUCCESS;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_WriteFileWithSeek (Handle: 0x%x, 0x%x bytes, Position 0x%x,0x%x)\r\n"), pHandle, BytesToWrite, HighOffset, LowOffset));

    if( !pHandle )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }
    if( !(pHandle->dwAccess & GENERIC_WRITE) )
    {
        Result = ERROR_ACCESS_DENIED;
        goto exit;
    }

    ULARGE_INTEGER Offset;
    Offset.LowPart = LowOffset;
    Offset.HighPart = HighOffset;

    if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
        !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( pHandle->pStream )
    {
        Result = pHandle->pStream->WriteFile( Offset.QuadPart,
                                              Buffer,
                                              BytesToWrite,
                                              &NumBytesWritten );
    }

    if( pNumBytesWritten )
    {
        *pNumBytesWritten = NumBytesWritten;
    }

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_WriteFileWithSeek returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}


BOOL UDF_WriteFileGather( PFILE_HANDLE pHandle,
                          FILE_SEGMENT_ELEMENT SegmentArray[],
                          DWORD NumberOfBytesToWrite,
                          LPDWORD pReserved,
                          LPOVERLAPPED pOverlapped )
{
    LRESULT Result = ERROR_SUCCESS;

    // Interpret lpReserved as an array of 64-bit offsets if provided.
    FILE_SEGMENT_ELEMENT* OffsetArray = (FILE_SEGMENT_ELEMENT*)pReserved;

    DEBUGMSG(ZONE_APIS, (TEXT("UDFS!UDF_WriteFileGather(Handle: 0x%x, 0x%x bytes)\r\n"), pHandle, NumberOfBytesToWrite));

    if( !pHandle )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }
    if( !(pHandle->dwAccess & GENERIC_WRITE) )
    {
        Result = ERROR_ACCESS_DENIED;
        goto exit;
    }

    if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
        !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( pHandle->pStream )
    {
        Result = pHandle->pStream->WriteFileGather( SegmentArray,
                                                    NumberOfBytesToWrite,
                                                    OffsetArray,
                                                    pHandle->Position );
    }

    if( (Result == ERROR_SUCCESS) && !OffsetArray )
    {
        // Update file handle position
        //
        pHandle->Position += NumberOfBytesToWrite;
    }

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_WriteFileGather returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}


DWORD UDF_SetFilePointer( PFILE_HANDLE pHandle,
                          LONG DistanceToMove,
                          PLONG pDistanceToMoveHigh,
                          DWORD MoveMethod )
{
    LARGE_INTEGER NewPosition;
    LRESULT Result = ERROR_SUCCESS;
    DWORD Return = (DWORD)-1;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_SetFilePointer(Handle: 0x%x, Offset 0x%x, Method %d)\r\n"), pHandle, DistanceToMove, MoveMethod));

    if( !pHandle )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
        !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    CStream* pStream = pHandle->pStream;
    if( !pStream )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( pDistanceToMoveHigh )
    {
        NewPosition.LowPart = DistanceToMove;
        NewPosition.HighPart = *pDistanceToMoveHigh;
    }
    else
    {
        NewPosition.QuadPart = DistanceToMove;
    }

    switch( MoveMethod )
    {
        case FILE_BEGIN:
        break;

        case FILE_CURRENT:
        {
            NewPosition.QuadPart += pHandle->Position;
        }
        break;

        case FILE_END:
        {
            NewPosition.QuadPart += pStream->GetFileSize();
        }
        break;

        default:
        {
            Result = ERROR_INVALID_PARAMETER;
            goto exit;
        }
    }

    pHandle->Position = NewPosition.QuadPart;

    Return = NewPosition.LowPart;
    if (pDistanceToMoveHigh)
    {
        *pDistanceToMoveHigh = NewPosition.HighPart;
    }

  exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS || ZONE_ERROR && Result,(TEXT("UDFS!UDF_SetFilePointer returned 0x%x (LRESULT: %d)\r\n"), Return, Result));

    return Return;
}


DWORD UDF_GetFileSize( PFILE_HANDLE pHandle, LPDWORD pFileSizeHigh )
{
    LRESULT Result = ERROR_SUCCESS;
    ULARGE_INTEGER Return;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_GetFileSize(Handle: 0x%x)\r\n"), pHandle));

    if( !pHandle || !pHandle->pStream )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
        !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Return.QuadPart = pHandle->pStream->GetFileSize();

    if( pFileSizeHigh )
    {
        *pFileSizeHigh = Return.HighPart;
    }


  exit:
    if( Result != ERROR_SUCCESS )
    {
        Return.LowPart = 0;
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS || ZONE_ERROR && Result,(TEXT("UDFS!UDF_GetFileSize returned 0x%x (%d)\r\n"), Return, Result));

    return Return.LowPart;
}


BOOL UDF_GetFileInformationByHandle( PFILE_HANDLE pHandle,
                                     LPBY_HANDLE_FILE_INFORMATION pFileInfo )
{
    LRESULT Result = ERROR_SUCCESS;
    BY_HANDLE_FILE_INFORMATION FileInfo;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_GetFileInformationByHandle(Handle: 0x%x)\r\n"), pHandle));

    if( !pHandle || !pFileInfo )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
        !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    CStream* pStream = pHandle->pStream;
    if( !pStream )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    ZeroMemory( &FileInfo, sizeof(FileInfo) );

    Result = pStream->GetFileInformation( pHandle->pLink, &FileInfo );

    if( Result == ERROR_SUCCESS )
    {
        __try
        {
            CopyMemory( pFileInfo, &FileInfo, sizeof(FileInfo) );
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            Result = ERROR_INVALID_PARAMETER;
        }
    }

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS || ZONE_ERROR && Result,(TEXT("UDFS!UDF_GetFileInformationByHandle returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}


BOOL UDF_FlushFileBuffers( PFILE_HANDLE pHandle )
{
    DWORD NumBytesWritten = 0;
    LRESULT Result = ERROR_SUCCESS;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_FlushFileBuffers(Handle: 0x%x)\r\n"), pHandle));

    if( !pHandle || !pHandle->pStream )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( !(pHandle->dwAccess & GENERIC_WRITE) )
    {
        Result = ERROR_ACCESS_DENIED;
        goto exit;
    }

    if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
        !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Result = pHandle->pStream->FlushFileBuffers();

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS || ZONE_ERROR && Result,(TEXT("UDFS!UDF_FlushFileBuffers returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}


BOOL UDF_GetFileTime( PFILE_HANDLE pHandle,
                      LPFILETIME pCreation,
                      LPFILETIME pLastAccess,
                      LPFILETIME pLastWrite )
{
    LRESULT Result = ERROR_SUCCESS;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_GetFileTime(Handle: 0x%x)\r\n"), pHandle));

    if( !pHandle )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
        !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    CStream* pStream = pHandle->pStream;
    if( !pStream )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Result = pStream->GetFileTime( pCreation,
                                 pLastAccess,
                                 pLastWrite );

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS || ZONE_ERROR && Result,(TEXT("UDFS!UDF_GetFileTime returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}


BOOL UDF_SetFileTime( PFILE_HANDLE pHandle,
                      const FILETIME *pCreation,
                      const FILETIME *pLastAccess,
                      const FILETIME *pLastWrite )
{
    LRESULT Result = ERROR_SUCCESS;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_SetFileTime(Handle: 0x%x)\r\n"), pHandle));

    if( !pHandle )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( !(pHandle->dwAccess & GENERIC_WRITE) )
    {
        Result = ERROR_ACCESS_DENIED;
        goto exit;
    }

    if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
        !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    CStream* pStream = pHandle->pStream;
    if (!pStream)
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Result = pStream->SetFileTime( pCreation,
                                   pLastAccess,
                                   pLastWrite );

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS || ZONE_ERROR && Result,(TEXT("UDFS!UDF_SetFileTime returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}


BOOL UDF_SetEndOfFile( PFILE_HANDLE pHandle )
{
    LRESULT Result = ERROR_SUCCESS;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_SetEndOfFile(Handle: 0x%x)\r\n"), pHandle));

    if( !pHandle )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( !(pHandle->dwAccess & GENERIC_WRITE) )
    {
        Result = ERROR_ACCESS_DENIED;
        goto exit;
    }

    if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
        !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    CStream* pStream = pHandle->pStream;
    if (!pStream)
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Result = pStream->SetEndOfFile( pHandle->Position );

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS || ZONE_ERROR && Result,(TEXT("UDFS!UDF_SetEndOfFile returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}

// /////////////////////////////////////////////////////////////////////////////
// UDF_DeviceIoControl
//
BOOL UDF_DeviceIoControl( PFILE_HANDLE pHandle,
                          DWORD IoControlCode,
                          LPVOID pInBuf,
                          DWORD InBufSize,
                          LPVOID pOutBuf,
                          DWORD OutBufSize,
                          LPDWORD pBytesReturned,
                          LPOVERLAPPED pOverlapped )
{
    LRESULT Result = ERROR_SUCCESS;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_DeviceIoControl(0x%x,0x%x)\r\n"), pHandle, IoControlCode));

    if( !pHandle )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( !pHandle->pStream )
    {
        //
        // TODO::How do we get a handle without a stream attached to it?  If
        // this is the case, we can always use the link for something else
        // like a volume pointer.  However, this is never done right now and
        // we will need to add it.
        //

        if( !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
        {
            Result = ERROR_INVALID_PARAMETER;
            goto exit;
        }

        //
        // This is a volume handle
        //
        ASSERT(pHandle->pVolume);
        Result = pHandle->pVolume->DeviceIoControl( IoControlCode,
                                                    pInBuf,
                                                    InBufSize,
                                                    pOutBuf,
                                                    OutBufSize,
                                                    pBytesReturned,
                                                    pOverlapped );

    }
    else
    {
        if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
            !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
        {
            ASSERT(FALSE);
            Result = ERROR_INVALID_PARAMETER;
            goto exit;
        }

        //
        // We need to pass in the file handle for any IOCTLs that might
        // require a file offset.  Specifically, I'm referring to the DVD
        // IOCTLs.
        //
        Result = pHandle->pStream->DeviceIoControl( pHandle,
                                                    IoControlCode,
                                                    pInBuf,
                                                    InBufSize,
                                                    pOutBuf,
                                                    OutBufSize,
                                                    pBytesReturned,
                                                    pOverlapped );
        if ( Result == ERROR_CALL_NOT_IMPLEMENTED )
        {
            //
            // Failed file IOCTL, try volume IOCTL instead.
            //
            Result = pHandle->pStream->GetVolume()->DeviceIoControl( IoControlCode,
                                                                     pInBuf,
                                                                     InBufSize,
                                                                     pOutBuf,
                                                                     OutBufSize,
                                                                     pBytesReturned,
                                                                     pOverlapped );
        }
    }

  exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS || ZONE_ERROR && Result,(TEXT("UDFS!UDF_DeviceIoControl returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}

// /////////////////////////////////////////////////////////////////////////////
// UDF_FsIoControl
//
BOOL UDF_FsIoControl( CVolume* pVolume,
                      DWORD FsIoControlCode,
                      LPVOID pInBuf,
                      DWORD InBufSize,
                      LPVOID pOutBuf,
                      DWORD OutBufSize,
                      LPDWORD pBytesReturned,
                      LPOVERLAPPED pOverlapped )
{
    LRESULT Result = ERROR_SUCCESS;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_FsIoControl(0x%p,0x%x)\r\n"), pVolume, FsIoControlCode));

    if( !pVolume )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( !::IsKModeAddr( (DWORD)pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Result = pVolume->FsIoControl( FsIoControlCode,
                                   pInBuf,
                                   InBufSize,
                                   pOutBuf,
                                   OutBufSize,
                                   pBytesReturned,
                                   pOverlapped );

  exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS || ZONE_ERROR && Result,(TEXT("UDFS!UDF_FsIoControl returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}

BOOL UDF_LockFileEx( PFILE_HANDLE pHandle,
                     DWORD Flags,
                     DWORD Reserved,
                     DWORD NumberOfBytesToLockLow,
                     DWORD NumberOfBytesToLockHigh,
                     LPOVERLAPPED pOverlapped )
{
    // TODO: Implement FAT_LockFileEx
    PREFAST_DEBUGCHK(pHandle);
    return FALSE;

#if 0
    return FSDMGR_InstallFileLock(AcquireFileLockState,
                                  ReleaseFileLockState,
                                  (DWORD)pHandle,
                                  Flags,
                                  Reserved,
                                  NumberOfBytesToLockLow,
                                  NumberOfBytesToLockHigh,
                                  pOverlapped);
#endif
}

BOOL UDF_UnlockFileEx( PFILE_HANDLE pHandle,
                       DWORD Reserved,
                       DWORD NumberOfBytesToLockLow,
                       DWORD NumberOfBytesToLockHigh,
                       LPOVERLAPPED pOverlapped )
{
    // TODO: Implement FAT_UnlockFileEx
    PREFAST_DEBUGCHK(pHandle);
    return FALSE;

#if 0
    return FSDMGR_RemoveFileLock(AcquireFileLockState,
                                 ReleaseFileLockState,
                                 (DWORD)pHandle,
                                 Reserved,
                                 NumberOfBytesToLockLow,
                                 NumberOfBytesToLockHigh,
                                 pOverlapped);
#endif
}


BOOL UDF_GetVolumeInfo( CVolume* pVolume, FSD_VOLUME_INFO *pInfo )
{
    LRESULT Result = ERROR_SUCCESS;
    FSD_VOLUME_INFO TempInfo = { 0 };

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_GetVolumeInfo(Handle: 0x%x)\r\n"), pVolume));

    if( !::IsKModeAddr( (DWORD)pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Result = pVolume->GetVolumeInfo( &TempInfo );

    if( Result == ERROR_SUCCESS )
    {
        if( !CeSafeCopyMemory( pInfo, &TempInfo, sizeof(TempInfo) ) )
        {
            Result = ERROR_INVALID_PARAMETER;
        }
    }

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS || ZONE_ERROR && Result,(TEXT("UDFS!UDF_GetVolumeInfo returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}

BOOL UDF_FindClose( PSEARCH_HANDLE pHandle )
{
    LRESULT Result = ERROR_SUCCESS;
    CVolume* pVolume = NULL;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_FindClose(Handle: 0x%08x)\r\n"), pHandle));

    if( !pHandle )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
        !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    pVolume = pHandle->pStream->GetVolume();
    if( pVolume )
    {
        pVolume->CloseFile( pHandle, FALSE );
    }

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS || ZONE_ERROR && Result,(TEXT("UDFS!UDF_FindClose returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}

HANDLE UDF_FindFirstFile( CVolume* pVolume,
                          HANDLE hProc,
                          PCWSTR FileSpec,
                          PWIN32_FIND_DATAW pFindData )
{
    LRESULT Result = ERROR_SUCCESS;
    HANDLE Handle = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW FindData;
    WCHAR* strFileName = NULL;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_FindFirstFileW(Pattern: %s)\r\n"), FileSpec));

    if( !::IsKModeAddr( (DWORD)pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( !FileSpec || !pFindData )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    ZeroMemory( &FindData, sizeof(FindData) );

    __try
    {
        DWORD dwLength = wcslen( FileSpec ) + 1;

        strFileName = new (UDF_TAG_STRING) WCHAR[ dwLength ];
        if( !strFileName )
        {
            Result = ERROR_NOT_ENOUGH_MEMORY;
            goto exit;
        }

        ZeroMemory( strFileName, dwLength * sizeof(WCHAR) );

        StringCchPrintfW( strFileName, dwLength, L"%s", FileSpec );
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Result = pVolume->FindFirstFile( hProc,
                                     strFileName,
                                     &FindData,
                                     &Handle );

    if( Result == ERROR_SUCCESS )
    {
        __try
        {
            CopyMemory( pFindData, &FindData, sizeof(FindData) );
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            UDF_FindClose( (PSEARCH_HANDLE)&Handle );
            Result = ERROR_INVALID_PARAMETER;
        }
    }

exit:
    if( strFileName )
    {
        delete[] strFileName;
    }

    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS,
              (TEXT("UDFS!UDF_FindFirstFileW returned (Handle: 0x%x, File: %s, LRESULT: 0x%x)\r\n"),
              Handle,
              Result == ERROR_SUCCESS ? pFindData->cFileName : TEXT(""),
              Result));

    return Handle;
}


BOOL UDF_FindNextFile( PSEARCH_HANDLE pHandle, PWIN32_FIND_DATAW pFindData )
{
    LRESULT Result = ERROR_SUCCESS;
    CFileInfo FileInfo;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_FindNextFile(Handle: 0x%08x, Pattern: %s)\r\n"), pHandle, pHandle->pFileName->GetCurrentToken()));

    if( !pHandle || !pFindData )
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if( !::IsKModeAddr( (DWORD)pHandle->pStream ) ||
        !::IsKModeAddr( (DWORD)pHandle->pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    __try
    {
        CDirectory* pDirectory = (CDirectory*)pHandle->pStream->GetFile();
        ASSERT (pDirectory);

        Result = pDirectory->FindNextFile( pHandle, &FileInfo );

        if( Result == ERROR_SUCCESS )
        {
            //
            // Update last position and search pattern in search handle
            //
            FileInfo.GetFindData( pFindData );
        }
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        Result = ERROR_INVALID_PARAMETER;
    }


exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS,
              (TEXT("UDFS!UDF_FindNextFile (Handle: 0x%x) returned (File: %s, LRESULT: 0x%x)\r\n"),
              pHandle,
              Result == ERROR_SUCCESS ? pFindData->cFileName : TEXT(""),
              Result));

    return (Result == ERROR_SUCCESS);
}



BOOL UDF_CreateDirectory( CVolume* pVolume,
                          PCWSTR PathName,
                          LPSECURITY_ATTRIBUTES pSecurityAttributes )
{
    LRESULT Result = ERROR_SUCCESS;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_CreateDirectoryW(%s)\r\n"), PathName));

    if( !::IsKModeAddr( (DWORD)pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Result = pVolume->CreateDirectory( PathName, pSecurityAttributes );

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_CreateDirectoryW returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}


BOOL UDF_RemoveDirectory( CVolume* pVolume, PCWSTR PathName )
{
    LRESULT Result = ERROR_SUCCESS;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_RemoveDirectoryW(%s)\r\n"), PathName));

    if( !::IsKModeAddr( (DWORD)pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Result = pVolume->RemoveDirectory( PathName );

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_RemoveDirectoryW returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}


DWORD UDF_GetFileAttributes( CVolume* pVolume, PCWSTR FileName )
{
    LRESULT Result = ERROR_SUCCESS;
    DWORD Attributes = INVALID_FILE_ATTRIBUTES;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_GetFileAttributesW(%s)\r\n"), FileName));

    if( !::IsKModeAddr( (DWORD)pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Result = pVolume->GetFileAttributes( FileName, &Attributes );

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_GetFileAttributesW returned 0x%x (LRESULT 0x%x)\r\n"), Attributes, Result));
    return Attributes;
}


BOOL UDF_SetFileAttributes( CVolume* pVolume,
                            PCWSTR FileName,
                            DWORD Attributes )
{
    LRESULT Result = ERROR_SUCCESS;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_SetFileAttributesW(File: %s, Attributes: %d)\r\n"), FileName, Attributes));

    if( !::IsKModeAddr( (DWORD)pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Result = pVolume->SetFileAttributes( FileName, Attributes );

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_SetFileAttributesW returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}


BOOL UDF_DeleteFile( CVolume* pVolume, PCWSTR FileName )
{
    LRESULT Result = ERROR_SUCCESS;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_DeleteFileW(File: %s)\r\n"), FileName));

    if( !::IsKModeAddr( (DWORD)pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Result = pVolume->DeleteFile( FileName );

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_DeleteFileW returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}


BOOL UDF_MoveFile( CVolume* pVolume,
                   PCWSTR OldFileName,
                   PCWSTR NewFileName )
{
    LRESULT Result = ERROR_SUCCESS;

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_MoveFileW(OldFile: %s, NewFile: %s)\r\n"), OldFileName, NewFileName));

    if( !::IsKModeAddr( (DWORD)pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Result = pVolume->MoveFile( OldFileName, NewFileName );

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_MoveFileW returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}


BOOL UDF_RegisterFileSystemNotification( CVolume* pVolume, HWND hwnd )
{
    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_RegisterFileSystemNotification(0x%x)\r\n"), hwnd));

    return FALSE;
}


BOOL UDF_RegisterFileSystemFunction( CVolume* pVolume, SHELLFILECHANGEFUNC_t pfn )
{
    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_RegisterFileSystemFunction(0x%x)\r\n"), pfn));

#ifdef SHELL_CALLBACK_NOTIFICATION
    // pfnShell = pfn;
    return TRUE;
#else
    return FALSE;
#endif
}


BOOL UDF_DeleteAndRenameFile( CVolume* pVolume,
                              PCWSTR OldFileName,
                              PCWSTR NewFileName )
{
    LRESULT Result = ERROR_SUCCESS;
    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_PrestoChangoFileName(OldFile: %s, NewFile: %s)\r\n"), OldFileName, NewFileName));

    if( !::IsKModeAddr( (DWORD)pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Result = pVolume->DeleteAndRenameFile( OldFileName, NewFileName );

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_PrestoChangoFileName returned LRESULT 0x%x\r\n"), Result));
    return (Result == ERROR_SUCCESS);
}


BOOL UDF_GetDiskFreeSpace( CVolume* pVolume,
                           PCWSTR PathName,
                           PDWORD pSectorsPerCluster,
                           PDWORD pBytesPerSector,
                           PDWORD pFreeClusters,
                           PDWORD pClusters )
{
    LRESULT Result = ERROR_SUCCESS;
    DEBUGMSG(ZONE_APIS,(TEXT("UDFS!UDF_GetDiskFreeSpace(%s)\r\n"), PathName ? PathName : TEXT("")));

    if( !::IsKModeAddr( (DWORD)pVolume ) )
    {
        ASSERT(FALSE);
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Result = pVolume->GetDiskFreeSpace( pSectorsPerCluster,
                                        pBytesPerSector,
                                        pFreeClusters,
                                        pClusters );

exit:
    if( Result != ERROR_SUCCESS )
    {
        SetLastError( Result );
    }

    DEBUGMSG(ZONE_APIS || ZONE_ERROR && Result,(TEXT("UDFS!UDF_GetDiskFreeSpace returned LRESULT 0x%x, 0x%x clusters free\r\n"), Result, pFreeClusters ? *pFreeClusters : 0));
    return (Result == ERROR_SUCCESS);
}

LRESULT UDF_FormatVolume( HDSK hDsk )
{
    LRESULT Result = ERROR_SUCCESS;

    // TODO::Format the volume.

    return ERROR_CALL_NOT_IMPLEMENTED;
//    return FSDMGR_FormatVolume( hDsk, (LPVOID)&FormatParams );
}

void UDF_Notify( CVolume* pVolume, DWORD Flags )
{
    // TODO::What needs done here?
}

// /////////////////////////////////////////////////////////////////////////////
// UDF_RecognizeVolume
//
// This is exported as the detector function for UDF to identify this media as
// being UDF mountable or not.
//
BOOL UDF_RecognizeVolume( HANDLE hDisk,
                          const GUID* pGuid,
                          const BYTE* pBootSector,
                          DWORD dwBootSectorSize )
{
    BOOL fResult = TRUE;
    LRESULT lResult = ERROR_SUCCESS;
    DWORD dwBytesReturned = 0;
    STORAGEDEVICEINFO DeviceInfo = { 0 };

    DeviceInfo.cbSize = sizeof(DeviceInfo);

    if( !FSDMGR_DiskIoControl( (DWORD)hDisk,
                               IOCTL_DISK_DEVICE_INFO,
                               &DeviceInfo,
                               sizeof(DeviceInfo),
                               &DeviceInfo,
                               sizeof(DeviceInfo),
                               &dwBytesReturned,
                               NULL ) )
    {
        return FALSE;
    }

    if( DeviceInfo.dwDeviceClass == STORAGE_DEVICE_CLASS_MULTIMEDIA )
    {
        lResult = UDF_RecognizeVolumeCD( hDisk, pBootSector, dwBootSectorSize );
    }
    else
    {
        lResult = UDF_RecognizeVolumeHD( hDisk, pBootSector, dwBootSectorSize );
    }

    if( lResult != ERROR_SUCCESS )
    {
        SetLastError( lResult );
        fResult = FALSE;
    }
    else
    {
        CVolume* pVolume;

        pVolume = new( UDF_TAG_VOLUME ) CVolume();
        if( pVolume == NULL )
        {
            SetLastError( ERROR_NOT_ENOUGH_MEMORY );
            fResult = FALSE;
        }
        else
        {
            fResult = pVolume->ValidateUDFVolume( ( HDSK )hDisk );
            delete pVolume;
            pVolume = NULL;
        }
    }

    return fResult;
}

}

