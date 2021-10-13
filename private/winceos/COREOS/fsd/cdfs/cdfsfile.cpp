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
//+-------------------------------------------------------------------------
//
//
//  File:       udfsfile.cpp
//
//  Contents:
//
//  Classes:
//
//  Functions:
//
//--------------------------------------------------------------------------

#include "cdfs.h"

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::ROFS_CreateFile
//
//  Synopsis:
//
//  Arguments:  [hProc]                --
//              [pwsFileName]          --
//              [dwAccess]             --
//              [dwShareMode]          --
//              [pSecurityAttributes]  --
//              [dwCreate]             --
//              [dwFlagsAndAttributes] --
//              [hTemplateFile]        --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

HANDLE  CReadOnlyFileSystemDriver::ROFS_CreateFile(
                HANDLE hProc,
                PCWSTR pwsFileName,
                DWORD dwAccess,
                DWORD dwShareMode,
                LPSECURITY_ATTRIBUTES pSecurityAttributes,
                DWORD dwCreate,
                DWORD dwFlagsAndAttributes,
                HANDLE hTemplateFile)
{
    PDIRECTORY_ENTRY        pDirEntry;
    BOOL                    fRet;
    PFILE_HANDLE_INFO       pshr;
    HANDLE                  hHandle = INVALID_HANDLE_VALUE;
    WCHAR                   wChar;
    int                     Count;

    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!CreateFile Entered FileName=%s\r\n"), pwsFileName ? pwsFileName : L""));

    if(dwAccess & ( GENERIC_WRITE | GENERIC_ALL))
    {
       // OK to ask for write but only in conjunction with other

        SetLastError( ERROR_ACCESS_DENIED);
        goto leave;
    }

    //
    // Deny all opens for write-only access.   We accept read-write
    // access so old applications will continue to work.  The access
    // will be denied at the time of the write.
    //
    // If this is a create call where we will fail if the file already
    // exists, or will have to alter the file's length or attributes,
    // then we deny access to the file.

    switch (dwCreate)
    {
        case    CREATE_NEW:
        case    CREATE_ALWAYS:
        case    TRUNCATE_EXISTING:
            SetLastError( ERROR_ACCESS_DENIED);
            goto leave;

        case    OPEN_EXISTING:
        case    OPEN_ALWAYS:
        case    OPEN_FOR_LOADER:
            break;

        default:
            SetLastError( ERROR_INVALID_PARAMETER);
            goto leave;
    }

    __try {
        Count = 0;

        do {
            wChar = pwsFileName[Count];
            Count++;

        } while (wChar != 0);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
    
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
    }


    EnterCriticalSection(&m_csListAccess);

    fRet = FindFile(pwsFileName, &pDirEntry);

    if (fRet == FALSE) {
        goto error;
    }

    if (pDirEntry->fFlags & FILE_ATTRIBUTE_DIRECTORY) {
        SetLastError(ERROR_ACCESS_DENIED);
        goto error;
    }    
    
    pshr = new FILE_HANDLE_INFO;

    if (pshr == NULL) {
        goto error;
    }

    memset(pshr, 0, sizeof(*pshr));

    if ((dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED) != 0) {
        //
        //  Asking for overlapped I/O
        //

        pshr->fOverlapped = TRUE;
    }

    //
    //  Add it to the handle list
    //

    if (m_pFileHandleList != NULL) {
        m_pFileHandleList->pPrevious = pshr;
    }

    pshr->pNext = m_pFileHandleList;

    m_pFileHandleList = pshr;

    pshr->fFlags = pDirEntry->fFlags;
    pshr->dwDiskLocation = pDirEntry->dwDiskLocation;
    pshr->dwDiskSize = pDirEntry->dwDiskSize;
    pshr->Time = pDirEntry->Time;
    pshr->pVol = this;
    pshr->State = StateClean;
    pshr->dwByteOffset = pDirEntry->dwByteOffset;
    pshr->MediaFlags = pDirEntry->MediaFlags;
    pshr->HeaderSize = pDirEntry->HeaderSize;
    pshr->TrackMode = pDirEntry->TrackMode;
    pshr->XAAttributes = pDirEntry->XAAttributes;
    pshr->XAFileNumber = pDirEntry->XAFileNumber;

    hHandle = ::FSDMGR_CreateFileHandle(m_hVolume, hProc, (PFILE)pshr);
error:

    LeaveCriticalSection(&m_csListAccess);

leave:

    DEBUGMSG( ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!CreateFile Done hHandle = %x\r\n"), hHandle));

    return hHandle;
}


//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::ROFS_CloseFileHandle
//
//  Synopsis:
//
//  Arguments:  [pfh] --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL  CReadOnlyFileSystemDriver::ROFS_CloseFileHandle( PFILE_HANDLE_INFO pfh)
{
    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!CloseFileHandle Entered Handle=%08X\r\n"), pfh));
    
    EnterCriticalSection(&m_csListAccess);

    if (pfh->pNext != NULL) {
        pfh->pNext->pPrevious = pfh->pPrevious;
    }

    if (pfh->pPrevious == NULL) {
        m_pFileHandleList = pfh->pNext;
    } else {
        pfh->pPrevious->pNext = pfh->pNext;
    }

    delete pfh;

    LeaveCriticalSection(&m_csListAccess);

    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!CloseFileHandle Done\r\n")));

    return TRUE;
}


//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::ROFS_GetFileAttributes
//
//  Synopsis:
//
//  Arguments:  [pwsFileName] --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

DWORD CReadOnlyFileSystemDriver::ROFS_GetFileAttributes( PCWSTR pwsFileName)
{
    BOOL                fRet;
    PDIRECTORY_ENTRY    pDirEntry;
    DWORD dwAttr;

    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!GetFileAttributes Entered File=%s\r\n"), pwsFileName ? pwsFileName : L""));

    fRet = FindFile(pwsFileName, &pDirEntry);

    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!GetFileAttributes Done\r\n")));

    if (fRet == FALSE) {
        return 0xFFFFFFFF;
    }

    dwAttr = FILE_ATTRIBUTE_READONLY;

    if (pDirEntry->fFlags & FILE_ATTRIBUTE_DIRECTORY) {
        dwAttr |= FILE_ATTRIBUTE_DIRECTORY;
    }
    
    if (pDirEntry->fFlags & FILE_ATTRIBUTE_HIDDEN) {
        dwAttr |= FILE_ATTRIBUTE_HIDDEN;
    }

    return dwAttr;
}


//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::ROFS_ReadFile
//
//  Synopsis:
//
//  Arguments:  [pfh]           --
//              [buffer]        --
//              [nBytesToRead]  --
//              [pNumBytesRead] --
//              [pOverlapped]   --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL CReadOnlyFileSystemDriver::ROFS_ReadFile( PFILE_HANDLE_INFO pfh,
                                               __in_ecount(nBytesToRead) LPVOID buffer,
                                               DWORD nBytesToRead,
                                               DWORD * pNumBytesRead,
                                               OVERLAPPED * pOverlapped )
{
    DWORD   dwCurrentBytePosition = (DWORD)-1;
    DWORD   dwSector;
    DWORD   dwStartPosition;
    DWORD   dwBytesToRead;
    BOOL    fRet;
    DWORD   dwError;
    LPDWORD pNumBytesRead2 = NULL;
    BOOL    fRaw = FALSE;
    DWORD   dwSectorSize = CD_SECTOR_SIZE;
    DWORD   dwHeaderBytes = 0;

    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!ReadFile Entered Handle=%08X\r\n"), pfh));

    dwError = ERROR_SUCCESS;

    if (IsHandleDirty(pfh)) {
        SetLastError(ERROR_MEDIA_CHANGED);
        return FALSE;
    }

    if( FlagOn(pfh->MediaFlags, MF_CHECK_TRACK_MODE) )
    {
        fRaw = TRUE;
        
        //
        // Apparently we read the entire 2352 bytes when reading 
        // XAForm2/YellowMode2 even if the "user data" is a different
        // size.
        //
        switch( pfh->TrackMode )
        {
        case YellowMode2:
        case XAForm2:
        case CDDA:
            dwSectorSize = CDROM_RAW_SECTOR_SIZE;
            break;

        default:
            ASSERT(FALSE);
            return FALSE;
        }
    }

    __try {
        ((PBYTE)buffer)[0] = 0;
        ((PBYTE)buffer)[nBytesToRead - 1] = 0;

        if (pfh->fOverlapped == TRUE) {

            if (pOverlapped != NULL) {
                if (pOverlapped->OffsetHigh != 0)
                {
                    dwError = ERROR_INVALID_PARAMETER;
                }

                dwCurrentBytePosition = pOverlapped->Offset;

                pNumBytesRead2 = pNumBytesRead;
                pNumBytesRead = &pOverlapped->InternalHigh;
            } else {
                dwError = ERROR_INVALID_PARAMETER;
            }
            
        } else {
        
            if (pOverlapped) {
                dwCurrentBytePosition = pOverlapped->Offset;
                pOverlapped = NULL;
            } else {
                if (pNumBytesRead == NULL) {
                    dwError = ERROR_INVALID_PARAMETER;
                }
            }
            
        }
        
        if (pNumBytesRead) {
            *pNumBytesRead = 0;
        }

        if (pNumBytesRead2) {
            *pNumBytesRead2 = 0;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
    }

    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
        return FALSE;
    }

    if (dwCurrentBytePosition == (DWORD)-1) {
        do {
            dwCurrentBytePosition = pfh->dwFilePointer;

            if (dwCurrentBytePosition == pfh->dwDiskSize) {
            
                SetLastError(ERROR_HANDLE_EOF);
                return TRUE;
            }

            DEBUGCHK(dwCurrentBytePosition < pfh->dwDiskSize);

            if (dwCurrentBytePosition + nBytesToRead > pfh->dwDiskSize) {
                dwBytesToRead = pfh->dwDiskSize - dwCurrentBytePosition;
            } else {
                dwBytesToRead = nBytesToRead;
            }

            if (dwBytesToRead > (65532*dwSectorSize)) {
                dwBytesToRead = 65532*dwSectorSize;
            }       

        } while ((DWORD)InterlockedTestExchange( (PLONG)&(pfh->dwFilePointer), dwCurrentBytePosition, (dwCurrentBytePosition + dwBytesToRead)) != dwCurrentBytePosition);
    } else {
        do {
            if (dwCurrentBytePosition >= pfh->dwDiskSize) {
                SetLastError(ERROR_HANDLE_EOF);
                return TRUE;
            }

            DEBUGCHK(dwCurrentBytePosition < pfh->dwDiskSize);

            if (dwCurrentBytePosition + nBytesToRead > pfh->dwDiskSize) {
                dwBytesToRead = pfh->dwDiskSize - dwCurrentBytePosition;
            } else {
                dwBytesToRead = nBytesToRead;
            }

            if (dwBytesToRead > (65532*dwSectorSize)) {
                dwBytesToRead = 65532*dwSectorSize;
            }       

        } while ((DWORD)InterlockedTestExchange( (PLONG)&(pfh->dwFilePointer), dwCurrentBytePosition, (dwCurrentBytePosition + dwBytesToRead))
                 != dwCurrentBytePosition);
    }

    nBytesToRead = dwBytesToRead;

    //
    //  Find out where to read
    //
    if( FlagOn(pfh->MediaFlags,MF_PREPEND_HEADER) &&
        (dwCurrentBytePosition < pfh->HeaderSize ) )
    {
        RIFF_HEADER Header = { 0 };
        dwHeaderBytes = pfh->HeaderSize - dwCurrentBytePosition;
        DWORD dwOffset = pfh->HeaderSize - dwHeaderBytes;

        if( dwHeaderBytes > nBytesToRead )
        {
            dwHeaderBytes = nBytesToRead;
        }
        
        m_pFileSystem->GetHeader( &Header, pfh );

        CopyMemory( buffer, &((BYTE*)&Header)[dwOffset], dwHeaderBytes );
        buffer = (LPVOID)((BYTE*)buffer + dwHeaderBytes);
        nBytesToRead -= dwHeaderBytes;

        if( !nBytesToRead )
        {
            *pNumBytesRead = dwHeaderBytes;
            return TRUE;
        }
    }

    //
    // Account for the RIFF header that we tack on to the beginning of the file
    // if it is present.
    //
    if( dwCurrentBytePosition >= pfh->HeaderSize )
    {
        dwCurrentBytePosition -= pfh->HeaderSize;
    }
    else
    {
        //
        // If there is no header, the size should be zero and we should never
        // be in the else statement.
        //
        ASSERT(FlagOn(pfh->MediaFlags,MF_PREPEND_HEADER) != 0 );
        
        dwCurrentBytePosition = 0;
    }

    //
    //  Take the current file pointer, work out how many sectors that is
    //  and add it to the start sector of this file.
    //

    dwSector = pfh->dwDiskLocation + (dwCurrentBytePosition / dwSectorSize);

    //
    //  This is the offset from the previous sector boundary to current
    //  file pointer. We need this when reading the first sector.
    //

    dwStartPosition = pfh->dwByteOffset+dwCurrentBytePosition % dwSectorSize;

    fRet =Read( dwSector, 
                dwStartPosition, 
                nBytesToRead, 
                (PBYTE)buffer, 
                pNumBytesRead, 
                pOverlapped, 
                fRaw, 
                pfh->TrackMode );

    *pNumBytesRead += dwHeaderBytes;

    if (fRet == TRUE) {
    
        if (pNumBytesRead2) {
            *pNumBytesRead2 = *pNumBytesRead;
        }
        
        if (*pNumBytesRead < nBytesToRead) {
            //
            //  Move the file pointer back by amount we didn't read.
            //  This only happens in the case of an error.
            //

            do {
                dwSector        = pfh->dwFilePointer;
                dwStartPosition = dwSector - nBytesToRead + *pNumBytesRead;

            } while (InterlockedTestExchange( (LONG*)&(pfh->dwFilePointer), dwSector, dwStartPosition) != (LONG)dwSector);
        }
    }

    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!ReadFile Done\r\n")));

    return fRet;
}

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::ROFS_ReadFileWithSeek
//
//  Synopsis:
//
//  Arguments:  [pfh]                  --
//              [buffer]               --
//              [nBytesToRead]         --
//              [lpOverlapped]         --
//              [dwLowOffset]          --
//              [dwHighOffset]         --
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------
BOOL
CReadOnlyFileSystemDriver::ROFS_ReadFileWithSeek(PFILE_HANDLE_INFO pfh,
                                                 LPVOID buffer,
                                                 DWORD nBytesToRead,
                                                 LPDWORD lpNumBytesRead,
                                                 LPOVERLAPPED lpOverlapped,
                                                 DWORD dwLowOffset,
                                                 DWORD dwHighOffset)
{
    DWORD   dwSector, dwStartPosition;
    BOOL    fRet = FALSE;
    BOOL    fRaw = FALSE;
    DWORD   dwSectorSize = CD_SECTOR_SIZE;
    DWORD   dwHeaderBytes = 0;

    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!ReadFileWithSeek Entered Handle=%08X\r\n"), pfh));
    
    if (buffer == NULL && nBytesToRead == 0) {
        return TRUE;            // yes, PageIn requests are supported
    }
    
    ASSERT(dwHighOffset == 0);
    ASSERT(lpOverlapped == NULL);

    //
    //  Find out where to read
    //

    if (IsHandleDirty(pfh))
    {
        SetLastError(ERROR_MEDIA_CHANGED);
        goto done;
    }

    if( FlagOn(pfh->MediaFlags, MF_CHECK_TRACK_MODE) )
    {
        fRaw = TRUE;
        
        //
        // Apparently we read the entire 2352 bytes when reading 
        // XAForm2/YellowMode2 even if the "user data" is a different
        // size.
        //
        switch( pfh->TrackMode )
        {
        case YellowMode2:
        case XAForm2:
        case CDDA:
            dwSectorSize = CDROM_RAW_SECTOR_SIZE;
            break;

        default:
            ASSERT(FALSE);
            return FALSE;
        }
    }

    //
    //  Find out where to read
    //
    if( FlagOn(pfh->MediaFlags,MF_PREPEND_HEADER) &&
        (dwLowOffset < pfh->HeaderSize ) )
    {
        RIFF_HEADER Header = { 0 };
        dwHeaderBytes = pfh->HeaderSize - dwLowOffset;
        DWORD dwOffset = pfh->HeaderSize - dwHeaderBytes;

        if( dwHeaderBytes > nBytesToRead )
        {
            dwHeaderBytes = nBytesToRead;
        }
        
        m_pFileSystem->GetHeader( &Header, pfh );

        CopyMemory( buffer, &((BYTE*)&Header)[dwOffset], dwHeaderBytes );
        buffer = (LPVOID)((BYTE*)buffer + dwHeaderBytes);
        nBytesToRead -= dwHeaderBytes;

        if( !nBytesToRead )
        {
            *lpNumBytesRead = dwHeaderBytes;
            return TRUE;
        }
    }

    //
    // Account for the RIFF header that we tack on to the beginning of the file
    // if it is present.
    //
    if( dwLowOffset >= pfh->HeaderSize )
    {
        dwLowOffset -= pfh->HeaderSize;
    }
    else
    {
        //
        // If there is no header, the size should be zero and we should never
        // be in the else statement.
        //
        ASSERT(FlagOn(pfh->MediaFlags,MF_PREPEND_HEADER) != 0 );
        
        dwLowOffset = 0;
    }

    //
    //  Take the file offset, work out how many sectors that is
    //  and add it to the start sector of this file.
    //

    dwSector = pfh->dwDiskLocation + (dwLowOffset / dwSectorSize);

    //
    //  This is the offset from the previous sector boundary to current
    //  file pointer. We need this when reading the first sector.
    //

    dwStartPosition = pfh->dwByteOffset+dwLowOffset % dwSectorSize;

	fRet = Read( dwSector, 
                 dwStartPosition, 
                 nBytesToRead, 
                 (PBYTE)buffer, 
                 lpNumBytesRead, 
                 NULL,
                 fRaw,
                 pfh->TrackMode );

    *lpNumBytesRead += dwHeaderBytes;

done:
    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!ReadFileWithSeek Done\r\n")));
    return fRet;
}

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::ROFS_SetFilePointer
//
//  Synopsis:
//
//  Arguments:  [pfh]                  --
//              [lDistanceToMove]      --
//              [lpDistanceToMoveHigh] --
//              [dwMoveMethod]         --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

DWORD CReadOnlyFileSystemDriver::ROFS_SetFilePointer( PFILE_HANDLE_INFO pfh, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
    __int64 lTempLocation = 0xFFFFFFFF;
    DWORD   dwFileLength;
    DWORD   dwOldPos;

    DEBUGMSG(ZONE_FUNCTION,(FS_MOUNT_POINT_NAME TEXT("UDFS!SetFilePointer Entered Handle=%08X MoveDist=%ld MoveMethod=%08X\r\n"), pfh, lDistanceToMove, dwMoveMethod));

    __try {
        do {
            dwFileLength = pfh->dwDiskSize;

            lTempLocation = dwOldPos = pfh->dwFilePointer;

            switch (dwMoveMethod) {
            
            case FILE_BEGIN:
                lTempLocation = lDistanceToMove;
                break;
            case FILE_CURRENT:
                lTempLocation += lDistanceToMove;
                break;
            case FILE_END:
                lTempLocation = (LONG)dwFileLength + lDistanceToMove;
                break;
            default:
                SetLastError(ERROR_INVALID_PARAMETER);
                lTempLocation = 0xFFFFFFFF;
                __leave;
            }

            if ((lTempLocation < 0) || (lTempLocation > dwFileLength)) {
                if (lTempLocation < 0)
                    SetLastError(ERROR_NEGATIVE_SEEK);
                else    
                    SetLastError(ERROR_ACCESS_DENIED);
                lTempLocation = 0xFFFFFFFF;
                __leave;
            }
            
            if (lpDistanceToMoveHigh) {
                *lpDistanceToMoveHigh = 0;
            }
        
        } while (InterlockedTestExchange( (PLONG)&(pfh->dwFilePointer), dwOldPos, (LONG)lTempLocation) != (LONG)dwOldPos);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_INVALID_PARAMETER);
        lTempLocation = 0xFFFFFFFF;
    }

    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!SetFilePointer Done\r\n")));

    return (DWORD)lTempLocation;
}


//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::ROFS_GetFileSize
//
//  Synopsis:
//
//  Arguments:  [pfh]           --
//              [pFileSizeHigh] --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

DWORD CReadOnlyFileSystemDriver::ROFS_GetFileSize( PFILE_HANDLE_INFO pfh, PDWORD pFileSizeHigh)
{
    // NOTE: would have to be changed if we expect files >= 4gb
    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!GetFileSize Handle=%08X\r\n"), pfh));

    if (pFileSizeHigh) {
        *pFileSizeHigh = 0;
    }
    return pfh->dwDiskSize;
}


//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::ROFS_GetFileInformationByHandle
//
//  Synopsis:
//
//  Arguments:  [pfh]       --
//              [pFileInfo] --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL CReadOnlyFileSystemDriver::ROFS_GetFileInformationByHandle( PFILE_HANDLE_INFO pfh, LPBY_HANDLE_FILE_INFORMATION pFileInfo)
{
    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!GetFileInformationByHandle Entered Handle=%08X\r\n"), pfh));

    __try {
    
        memset(pFileInfo, 0, sizeof(BY_HANDLE_FILE_INFORMATION));
        
    } __except(EXCEPTION_EXECUTE_HANDLER) {
    
        DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!GetFileInformationByHandle Done with exception\r\n")));

        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pFileInfo->dwFileAttributes = FILE_ATTRIBUTE_READONLY;

    pFileInfo->nFileSizeLow = pfh->dwDiskSize;

    if ((pfh->fFlags & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
    
        pFileInfo->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        pFileInfo->nFileSizeLow = 0;
    }

    if (pfh->fFlags & FILE_ATTRIBUTE_HIDDEN) {
        pFileInfo->dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
    }

    pFileInfo->ftCreationTime = pfh->Time;
    pFileInfo->ftLastWriteTime = pfh->Time;
    pFileInfo->ftLastAccessTime = pfh->Time;

    pFileInfo->dwOID = (DWORD)INVALID_HANDLE_VALUE;

    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!GetFileInformationByHandle Done\r\n")));

    return TRUE;
}

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::ROFS_GetFileTime
//
//  Synopsis:
//
//  Arguments:  [pfh]         --
//              [pCreation]   --
//              [pLastAccess] --
//              [pLastWrite]  --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL CReadOnlyFileSystemDriver::ROFS_GetFileTime( PFILE_HANDLE_INFO pfh, LPFILETIME pCreation, LPFILETIME pLastAccess, LPFILETIME pLastWrite)
{
    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!GetFileTime Entered Handle=%08X\r\n"), pfh));

    __try {
    
        if (pCreation) {
            *pCreation = pfh->Time;
        }
        
        if (pLastAccess) {
            *pLastAccess = pfh->Time;
        }
        
        if (pLastWrite) {
            *pLastWrite = pfh->Time;
        }
        
    } __except(EXCEPTION_EXECUTE_HANDLER) {
    
        DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!GetFileTime Done with exception\r\n")));

        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT("UDFS!GetFileTime Done\r\n")));

    return TRUE;
}

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::ROFS_DeviceIoControl
//
//  Synopsis:
//
//  Arguments:  [pwsPathName]       --
//              [pSectorsPerCluster]--
//              [pBytesPerSector]   --
//              [pClusters]         --
//
//  Returns:
//
//  Notes: Handles FSCTLS and passes IOCTLS down to the storage device.
//
//--------------------------------------------------------------------------
BOOL CReadOnlyFileSystemDriver::ROFS_DeviceIoControl( PFILE_HANDLE_INFO pfh, 
                                                      DWORD dwIoControlCode, 
                                                      LPVOID lpInBuf, 
                                                      DWORD nInBufSize, 
                                                      LPVOID lpOutBuf, 
                                                      DWORD nOutBufSize, 
                                                      LPDWORD lpBytesReturned, 
                                                      LPOVERLAPPED lpOverlapped )
{
    // TODO: Have the ability to create a dummy file like "\\CDROM"
    BOOL fResult = TRUE;

    switch(dwIoControlCode) 
    {
    case FSCTL_STORAGE_MEDIA_CHANGE_EVENT:
        //
        // We need to determine if the media looks the same as it did 
        // before it was ejected.
        //
        
        break;

    default:
        fResult = UDFSDeviceIoControl( dwIoControlCode, 
                                       lpInBuf, 
                                       nInBufSize, 
                                       lpOutBuf, 
                                       nOutBufSize, 
                                       lpBytesReturned, 
                                       lpOverlapped );
    }
    
    return fResult;
}

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::ROFS_GetDiskFreeSpace
//
//  Synopsis:
//
//  Arguments:  [pwsPathName]       --
//              [pSectorsPerCluster]--
//              [pBytesPerSector]   --
//              [pClusters]         --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------
BOOL    CReadOnlyFileSystemDriver::ROFS_GetDiskFreeSpace(PDWORD pSectorsPerCluster, PDWORD pBytesPerSector, PDWORD pFreeClusters, PDWORD pClusters)
{
    switch(m_bFileSystemType) {
        case FILE_SYSTEM_TYPE_CDFS:
            *pSectorsPerCluster = 1;
            *pBytesPerSector = 2048;
            *pFreeClusters = 0;
            *pClusters = 328220;
            break;
        case FILE_SYSTEM_TYPE_CDDA:
            *pSectorsPerCluster = 0;
            *pBytesPerSector = 0;
            *pFreeClusters = 0;
            *pClusters = 0;
            break;
    }
    return TRUE;
}

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::RemountMedia
//
//  Synopsis:
//
//  Arguments:  [pwsPathName]       --
//              [pSectorsPerCluster]--
//              [pBytesPerSector]   --
//              [pClusters]         --
//
//  Returns:
//
//  Notes: We will check the following criteria to decide if we should
//         should remount the media as the same volume or not:
//  1. The file system type (CDDA/CDFS)
//  2. CDDA - The partition information.
//  3. CDFS - The root name.
//  4. CDFS - The root directory location.
//  5. CDFS - fUnicodeFileSystem
//
//--------------------------------------------------------------------------
BOOL CReadOnlyFileSystemDriver::RemountMedia()
{
    BOOL fResult = FALSE;
    PCD_PARTITION_INFO pPartInfo = NULL;
    CCDFSFileSystem* pCDFS = NULL;
    DWORD dwBufferSize = 0;
    DWORD dwBytesReturned = 0;
    BOOL fUnicode = FALSE;
    BYTE VolumeLabel[32];
    BOOL fFound = FALSE;
    BOOL fCDXA = FALSE;
    DIRECTORY_ENTRY RootDirectoryEntry = { 0 };
    
    //
    // Get some track information about the CD.
    //
    dwBufferSize = sizeof(CD_PARTITION_INFO) +
                   MAXIMUM_NUMBER_TRACKS_LARGE * 
                   sizeof(TRACK_INFORMATION2);
    
    pPartInfo = (PCD_PARTITION_INFO)new BYTE[ dwBufferSize ];
    if( !pPartInfo )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return FALSE;
    }

    if( !UDFSDeviceIoControl( IOCTL_CDROM_GET_PARTITION_INFO,
                              NULL,
                              0,
                              pPartInfo,
                              dwBufferSize,
                              &dwBytesReturned,
                              NULL ) )
    {
        goto exit_remountmedia;
    }

    //
    // We could actually compare the track information here to make sure
    // that all the track sizes and positions match, but that's already
    // done at the CDROM level on remount for the entire disc.
    //

    //
    // The partition information is all the same.  So if this was an audio
    // CD, then we will go ahead and remount at this time.
    //
    if( pPartInfo->fAudio )
    {
        fResult = TRUE;
        goto exit_remountmedia;
    }

    fFound = CCDFSFileSystem::Recognize( GetDisk(),
                                         &RootDirectoryEntry, 
                                         &fUnicode,
                                         &fCDXA,
                                         VolumeLabel, 
                                         sizeof(VolumeLabel) );

    if( !fFound )
    {
        goto exit_remountmedia;
    }

    pCDFS = (CCDFSFileSystem*)m_pFileSystem;
    
    if( fUnicode != pCDFS->m_fUnicodeFileSystem )
    {
        goto exit_remountmedia;
    }

    if( _wcsnicmp( (WCHAR*)VolumeLabel, m_RootName, 32 ) != 0 )
    {
        goto exit_remountmedia;
    }

    if( m_RootDirectoryPointer.dwDiskLocation != 
        RootDirectoryEntry.dwDiskLocation )
    {
        goto exit_remountmedia;
    }

    if( m_RootDirectoryPointer.dwDiskSize !=
        RootDirectoryEntry.dwDiskSize )
    {
        goto exit_remountmedia;
    }

    if( memcmp( &m_RootDirectoryPointer.Time, 
                 &RootDirectoryEntry.Time,
                 sizeof(RootDirectoryEntry.Time) ) != 0 )
    {
        goto exit_remountmedia;
    }

    fResult = TRUE;

exit_remountmedia:
    if( pPartInfo )
    {
        delete [] pPartInfo;
    }

    return fResult;
}

