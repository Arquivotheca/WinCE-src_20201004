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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "relfsd.h"
#include "cefs.h"

#define HCONSOLE -2
// #define NO_PAGING 1

HANDLE RELFSD_CreateFileW(VolumeState *pVolume, HANDLE hProc, LPCWSTR lpFileName, DWORD dwAccess, DWORD dwShareMode,
                               LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreate, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    HANDLE      hFile;
    int         nFileId;
    FileState   *pfs;
    int         nFlags = 0;
    DWORD       dwAttributes;
    BOOL  fExists = FALSE;
    WIN32_FIND_DATA wfd;
    TCHAR szWindowsName[MAX_PATH];

    if (!lpFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    if (lpFileName && (*lpFileName == L'\\')) 
        lpFileName++;  // skip leading '/'

    if (g_fSecureRelfsd && (OPEN_EXISTING != dwCreate)) {
        RETAILMSG(1, (TEXT("!ReleaseFSD: Error. Secure mode CreateFile supports only OPEN_EXISTING.\n")));
        SetLastError(ERROR_NOT_SUPPORTED);
        return INVALID_HANDLE_VALUE;
    }

    if (g_fSecureRelfsd && ((wcscmp(lpFileName, L"VOL:") == 0) ||
                            (wcscmp(lpFileName, L"reg:") == 0) ||
                            (wcscmp(lpFileName, L"con") == 0))) {
        RETAILMSG(1, (TEXT("!ReleaseFSD: Error. Secure mode CreateFile does not support special files - 'VOL:', 'reg:' and 'con'.\n")));
        SetLastError(ERROR_NOT_SUPPORTED);
        return INVALID_HANDLE_VALUE;
    }                          

    if ((OPEN_EXISTING == dwCreate) && (FILE_ATTRIBUTE_ROMMODULE == dwFlagsAndAttributes)) {
        // special loader flag to load a rom-module across relfsd for debug purpose
        dwFlagsAndAttributes = 0;   // unset the flag, go through normal process
    } else if ((OPEN_EXISTING == dwCreate) && !(GENERIC_WRITE & dwAccess)) {
        // fail the create call if the file exists in ROM
        StringCchCopy( szWindowsName, MAX_PATH,L"\\WINDOWS\\");
        szWindowsName[MAX_PATH-1] = 0;
        StringCchCatN( szWindowsName, MAX_PATH,lpFileName, MAX_PATH-20);
        szWindowsName[MAX_PATH-1] = 0;
        StringCchCopyN( wfd.cFileName, MAX_PATH,lpFileName, MAX_PATH-1);
        wfd.cFileName[MAX_PATH-1] = 0;
        wfd.cFileName[0] = wcslen(lpFileName+1);
        dwAttributes = GetFileAttributes(szWindowsName);
        if ((dwAttributes != -1) && (dwAttributes & FILE_ATTRIBUTE_INROM)) {
            DEBUGMSG( ZONE_ERROR, (TEXT("RELFSD: Aborting CreateFile of %s since module was found in rom\r\n"), lpFileName));
            SetLastError(ERROR_NO_MORE_FILES);
            return INVALID_HANDLE_VALUE;
        } 
    }
    
    hFile = INVALID_HANDLE_VALUE;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : CreateFile: %s (dwCreate = %x, dwAccess = %x, dwFlagsAndAttributes= %x)\n"), lpFileName, dwCreate, dwAccess, dwFlagsAndAttributes));

    if (dwAccess & GENERIC_WRITE)  {
        if (dwAccess & GENERIC_READ)
            nFlags = _O_RDWR;
        else
            nFlags = _O_WRONLY;
    }
    else if (dwAccess & GENERIC_READ) {
        nFlags = _O_RDONLY;
    } 

    switch (dwCreate) {
        case CREATE_NEW       : nFlags |= _O_CREAT | _O_EXCL; break;
        case CREATE_ALWAYS    : nFlags |= _O_CREAT | _O_TRUNC; break;
        case OPEN_EXISTING    : break;
        case OPEN_ALWAYS      : nFlags |= _O_CREAT; break;
        case TRUNCATE_EXISTING: nFlags |= _O_TRUNC; break;

        default:
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return INVALID_HANDLE_VALUE;
        }
    }

    EnterCriticalSectionMM(g_pcsMain);

    // Call GetFileAttributes so we can set the correct error code if the file exists.
    // This is needed because the protocol currently does not return error codes.

    if ((wcscmp(lpFileName, L"VOL:") != 0) && (wcscmp(lpFileName, L"reg:") != 0) && (wcscmp(lpFileName,L"con") != 0)) {
    
        fExists = (rgetfileattrib(lpFileName) != -1);

        if (dwCreate == CREATE_NEW && fExists) {
            SetLastError(ERROR_ALREADY_EXISTS);
            goto exit;
        }
        else if (dwCreate == OPEN_EXISTING && !fExists) {
            SetLastError(ERROR_FILE_NOT_FOUND);
            goto exit;
        }
    }
    
    nFileId = ropen(lpFileName, nFlags);
    if (nFileId != -1 || !wcscmp (lpFileName, L"VOL:")) {
        pfs = LocalAlloc(0, sizeof(FileState));
        if (pfs) {
            pfs->fs_Handle = nFileId;
            pfs->fs_Volume = pVolume;

            hFile = FSDMGR_CreateFileHandle(pVolume->vs_Handle, hProc,(ulong)pfs);
            if (hFile == INVALID_HANDLE_VALUE)
            {
                DEBUGMSG(ZONE_APIS, (TEXT("CreateFile : Invalid Handle")));
                LocalFree(pfs);
            } 
            else 
            {
                if (lpFileName && (wcscmp(lpFileName, L"celog.clg") != 0) && (wcscmp(lpFileName, L"reg:") != 0) && (wcscmp(lpFileName,L"con") != 0)) {
                    if (g_fSecureRelfsd) {
                        RETAILMSG(1,(TEXT("RELFSD: Opening file %s from desktop\r\n"), lpFileName));
                    }
                    else {
                        NKDbgPrintfW(TEXT("RELFSD: Opening file %s from desktop\r\n"), lpFileName);
                    }
                }    

                // Since attributes are not part of the protocol for CreateFile call, set them if necessary.
                if (dwFlagsAndAttributes && dwFlagsAndAttributes != FILE_ATTRIBUTE_NORMAL) {
                    rsetfileattrib(lpFileName, dwFlagsAndAttributes & CE_ATTRIBUTE_MASK);
                }

                // Set the appropriate error code so user knows if file existed previously
                if (dwCreate == CREATE_ALWAYS || dwCreate == OPEN_ALWAYS) {
                    SetLastError (fExists ? ERROR_ALREADY_EXISTS : ERROR_SUCCESS);
                }    
                    
            }        
            
        }
    }
    else
    {
        SetLastError(ERROR_FILE_NOT_FOUND);
    }

exit:
    
    LeaveCriticalSectionMM(g_pcsMain);
    return hFile;
}


BOOL RELFSD_CloseFile(FileState *pfs)
{
    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : FSD_CloseFile\n")));
    EnterCriticalSectionMM(g_pcsMain);
    rclose(pfs->fs_Handle);
    LocalFree(pfs);
    LeaveCriticalSectionMM(g_pcsMain);
    return TRUE;
}

BOOL RELFSD_ReadFile( FileState *pfs, PBYTE pBuffer, DWORD cbRead, PDWORD pNumBytesRead, LPOVERLAPPED lpOverlapped)
{
    BOOL fSuccess;
    int  nRead;

    if (pfs->fs_Handle == HCONSOLE)
    {
        EnterCriticalSectionMM(g_pcsMain);
        nRead = rread(pfs->fs_Handle, (unsigned char *) pBuffer, cbRead);
        LeaveCriticalSectionMM(g_pcsMain);
    }       
    else
    {
        DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : FSD_ReadFile %#x bytes\n"), cbRead));
        EnterCriticalSectionMM(g_pcsMain);
        nRead = rread(pfs->fs_Handle, (unsigned char *) pBuffer, cbRead);
        LeaveCriticalSectionMM(g_pcsMain);
    }
    
    if (nRead < 0) {
        nRead = 0;
        fSuccess  = FALSE;
    } else {
        fSuccess = TRUE;
    }

    if (pNumBytesRead)
        *pNumBytesRead = (DWORD)nRead;

    return fSuccess;
}



BOOL RELFSD_ReadFileWithSeek( FileState *pfs, PBYTE pBuffer, DWORD cbRead, PDWORD pNumBytesRead, 
                                    LPOVERLAPPED lpOverlapped, DWORD dwLowOffset, DWORD dwHighOffset)
{
    BOOL     fSuccess;
    LONGLONG pos;
    int      nRead;

#ifdef NO_PAGING    
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
#endif

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: FSD_ReadFileWithSeek %d bytes. Offset: %d, %d\n"), cbRead, dwHighOffset, dwLowOffset));

    // Max amount that can be read is one page size
    if (cbRead > 4*1024) {
        DEBUGMSG (ZONE_APIS, (TEXT("ReleaseFSD: FSD_ReadFileWithSeek %d bytes is too large\n"), cbRead));
        return FALSE;
    }

    /* this case is used as a probe from the kernel to see if paging is OK... */
    if ((pBuffer == NULL) && (cbRead == 0))     {
        nRead = 0;
        fSuccess  = TRUE;
    } else {
        EnterCriticalSectionMM(g_pcsMain);

        pos = ((LONGLONG)dwHighOffset << 32) | (LONGLONG)dwLowOffset;
        nRead = rreadseek (pfs->fs_Handle, pBuffer, cbRead, (LONG)pos);
#if 0        
        rlseek(pfs->fs_Handle,(LONG)pos & -1,SEEK_SET);
        nRead = rread(pfs->fs_Handle, (unsigned char *) pBuffer, cbRead);
#endif        

        LeaveCriticalSectionMM(g_pcsMain);

        if (nRead < 0) {
            nRead = 0;
            fSuccess  = FALSE;
        } else {
            fSuccess = TRUE;
        }
    }

    if (pNumBytesRead)
        *pNumBytesRead = (DWORD)nRead;


    return fSuccess;
}

BOOL RELFSD_WriteFile( FileState *pfs, LPCVOID pBuffer, DWORD cbWrite, PDWORD pNumBytesWritten, LPOVERLAPPED lpOverlapped)
{
    BOOL fSuccess;
    int  nWritten;
    
    if (g_fSecureRelfsd) {
        RETAILMSG(1, (TEXT("!ReleaseFSD: Error. Secure mode does not support WriteFile.\n")));
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }
    
    if (pfs->fs_Handle == HCONSOLE)
    {   
        EnterCriticalSectionMM(g_pcsMain);
        nWritten = rwrite(pfs->fs_Handle, (unsigned char *)pBuffer, cbWrite);
        LeaveCriticalSectionMM(g_pcsMain);
    }   
    else
    {
        DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: FSD_WriteFile %#x bytes\n"), cbWrite));
        EnterCriticalSectionMM(g_pcsMain);
        nWritten = rwrite(pfs->fs_Handle, (unsigned char *)pBuffer, cbWrite);
        LeaveCriticalSectionMM(g_pcsMain);
    }
    
    if (nWritten < 0) {
        nWritten = 0;
        fSuccess = FALSE;
    } else {
        fSuccess = TRUE;
    }

    if (pNumBytesWritten)
        *pNumBytesWritten = (DWORD)nWritten;

    return fSuccess;
}



BOOL RELFSD_WriteFileWithSeek( FileState *pfs, PCVOID pBuffer, DWORD cbWrite, PDWORD pNumBytesWritten,
                                     LPOVERLAPPED lpOverlapped, DWORD dwLowOffset, DWORD dwHighOffset)
{
    BOOL     fSuccess;
    LONGLONG pos;
    int      nWritten;

#ifdef NO_PAGING    
    SetLastError (ERROR_NOT_SUPPORTED); 
    return FALSE;
#endif

    if (g_fSecureRelfsd) {
        RETAILMSG(1, (TEXT("!ReleaseFSD: Error. Secure mode does not support WriteFileWithSeek.\n")));
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: FSD_WriteFileWithSeek %#x bytes\n"), cbWrite));

    fSuccess = FALSE;

    EnterCriticalSectionMM(g_pcsMain);
    pos = ((LONGLONG)dwHighOffset << 32) | (LONGLONG)dwLowOffset;
    rlseek(pfs->fs_Handle,(LONG)pos & -1,SEEK_SET);
    nWritten = rwrite(pfs->fs_Handle, (unsigned char *) pBuffer, cbWrite);
    LeaveCriticalSectionMM(g_pcsMain);

    if (nWritten < 0) {
        nWritten = 0;
        fSuccess     = FALSE;
    } else {
        fSuccess = TRUE;
    }

    if (pNumBytesWritten)
        *pNumBytesWritten = (DWORD)nWritten;

    return fSuccess;
}



DWORD RELFSD_SetFilePointer( FileState *pfs, LONG lDistanceToMove, PLONG pDistanceToMoveHigh, DWORD dwMoveMethod)
{
    LONG lPos;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: FSD_SetFilePointer(%d,%d)\n"), lDistanceToMove, dwMoveMethod));

    EnterCriticalSectionMM(g_pcsMain);

    switch (dwMoveMethod) {
        case FILE_BEGIN:
        {
            lPos = rlseek(pfs->fs_Handle, lDistanceToMove, SEEK_SET);
            break;
        }

        case FILE_CURRENT:
        {
            lPos = rlseek(pfs->fs_Handle, lDistanceToMove, SEEK_CUR);
            break;
        }

        case FILE_END :
        {
            lPos = rlseek(pfs->fs_Handle, lDistanceToMove, SEEK_END);
            break;
        }

        default:
        {
            LeaveCriticalSectionMM(g_pcsMain);
            SetLastError(ERROR_INVALID_PARAMETER);
            return 0xffffffff;
        }
    }

    if (pDistanceToMoveHigh)
        *pDistanceToMoveHigh = 0;

    SetLastError(NO_ERROR);
    LeaveCriticalSectionMM(g_pcsMain);

    return lPos;
}


DWORD RELFSD_GetFileSize(FileState *pfs, PDWORD pFileSizeHigh)
{
    LONG lSize;
    LONG lPos;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: FSD_GetFileSize\n")));

    EnterCriticalSectionMM(g_pcsMain);

    lPos = rlseek(pfs->fs_Handle, 0, SEEK_CUR);
    lSize   = rlseek(pfs->fs_Handle, 0, SEEK_END);
    rlseek(pfs->fs_Handle, lPos, SEEK_SET);

    LeaveCriticalSectionMM(g_pcsMain);

    if (pFileSizeHigh)
        *pFileSizeHigh = 0;

    return lSize;
}


BOOL RELFSD_GetFileInformationByHandle(FileState *pfs, PBY_HANDLE_FILE_INFORMATION pFileInfo)
{
    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: FSD_GetFileInformationByHandle\n")));

    memset(pFileInfo, 0, sizeof(*pFileInfo));
    pFileInfo->dwFileAttributes     = FILE_ATTRIBUTE_NORMAL;
    pFileInfo->dwVolumeSerialNumber = (DWORD)pfs->fs_Volume;
    pFileInfo->nFileSizeLow         = RELFSD_GetFileSize(pfs, &pFileInfo->nFileSizeHigh);
    pFileInfo->nNumberOfLinks       = 1;
    pFileInfo->dwOID                = (DWORD)INVALID_HANDLE_VALUE;

    return TRUE;
}


BOOL RELFSD_FlushFileBuffers(FileState *pfs)
{
    DWORD dwError =ERROR_NOT_SUPPORTED;
    BOOL fSuccess = FALSE;

    if (dwError) {
        SetLastError( dwError);
    }
    return fSuccess;
}


BOOL RELFSD_GetFileTime( FileState *pfs, PFILETIME pCreation, PFILETIME pLastAccess, PFILETIME pLastWrite) 
{
    BOOL fSuccess = FALSE;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: GetFileTime\n")));


    EnterCriticalSectionMM(g_pcsMain);
    fSuccess = rgettime(pfs->fs_Handle, pCreation, pLastAccess, pLastWrite);  
    LeaveCriticalSectionMM(g_pcsMain);

    return fSuccess;
}


BOOL RELFSD_SetFileTime(FileState *pfs, PFILETIME pCreation, PFILETIME pLastAccess, PFILETIME pLastWrite) 
{
    BOOL fSuccess = FALSE;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: SetFileTime\n")));

    if (g_fSecureRelfsd) {
        RETAILMSG(1, (TEXT("!ReleaseFSD: Error. Secure mode does not support SetFileTime.\n")));
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }
    
    EnterCriticalSectionMM(g_pcsMain);
    fSuccess = rsettime(pfs->fs_Handle, pCreation, pLastAccess, pLastWrite);  
    LeaveCriticalSectionMM(g_pcsMain);

    return fSuccess;
}


BOOL RELFSD_SetEndOfFile(FileState *pfs)
{
    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: FSD_SetEndOfFile\n")));
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}


BOOL RELFSD_DeviceIoControl( FileState *pfs, DWORD dwIoControlCode, PBYTE lpInBuf, DWORD nInBufSize,
                                  PBYTE lpOutBuf, DWORD nOutBufSize, PDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    BOOL result = 0;
    DWORD hKey, dwType, dwSize, dwIndex;
    CHAR* szName;
    LPBYTE lpbData;

    if (g_fSecureRelfsd) {
        RETAILMSG(1, (TEXT("!ReleaseFSD: Error. Secure mode does not support RELFSD Ioctls.\n")));
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }

    EnterCriticalSectionMM(g_pcsMain);

    switch (dwIoControlCode)
    {
        case IOCTL_REG_OPEN:
        {
            DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: RegOpen\n")));

            memcpy( (BYTE *)&hKey, lpInBuf, sizeof(DWORD));
            szName = lpInBuf + sizeof(DWORD);
            result = rRegOpen (hKey, szName, (DWORD*)lpOutBuf);

            if (result != -1) {
                *lpBytesReturned = nOutBufSize;
            }

            DEBUGMSG(ZONE_APIS, (TEXT("RegOpen returned %08X, result=%08X on %08X:%a\n"), *((DWORD *)lpOutBuf), result, hKey, szName));
        }
        break;
        case IOCTL_REG_CLOSE:
        {
            DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: RegClose\n")));
            memcpy( (BYTE *)&hKey, lpInBuf, sizeof(DWORD));
            *lpBytesReturned = nOutBufSize;
            result = rRegClose (hKey);
            DEBUGMSG(ZONE_APIS, (TEXT("RegClose returned %d\n"), result));
        }
        break;
        case IOCTL_REG_GET:
        {
            DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: RegGet\n")));
            memcpy( (BYTE *)&hKey, lpInBuf, sizeof(DWORD));
            szName = lpInBuf + sizeof(DWORD);
            lpbData = (LPBYTE) HeapAlloc (GetProcessHeap(), 0, MAX_PATH);
            result = rRegGet (hKey, szName, &dwType, lpbData, &dwSize);

            *lpBytesReturned = nOutBufSize;
            memcpy (lpOutBuf, &dwType, sizeof(DWORD));
            memcpy (lpOutBuf + sizeof(DWORD), &dwSize, sizeof(DWORD));
            memcpy (lpOutBuf + 2*sizeof(DWORD), lpbData, dwSize);

            HeapFree (GetProcessHeap(), 0, lpbData);

            DEBUGMSG(ZONE_APIS, (TEXT("RegGet returned %d on %a\n"), result, szName));
        }
        break;
        case IOCTL_REG_ENUM:
        {
            DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: RegEnum\n")));
            memcpy (&hKey, lpInBuf, sizeof(DWORD));
            memcpy (&dwIndex, lpInBuf+sizeof(DWORD), sizeof(DWORD));
            lpbData = (LPBYTE) HeapAlloc (GetProcessHeap(), 0, MAX_PATH);
            result = rRegEnum (hKey, dwIndex, lpbData, &dwSize);

            *lpBytesReturned = nOutBufSize;
            memcpy (lpOutBuf, &dwSize, sizeof(DWORD));
            memcpy (lpOutBuf + sizeof(DWORD), lpbData, dwSize);

            HeapFree (GetProcessHeap(), 0, lpbData);

            DEBUGMSG(ZONE_APIS, (TEXT("RegEnum returned %d\n"), result));
        }
        break;
        case IOCTL_WRITE_DEBUG:
        {
            WCHAR* wzName;
            
            DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: PpfsWriteDebugString\n")));
            wzName = (WCHAR*) lpInBuf;
            PpfsWriteDebugString (wzName);

            *lpBytesReturned = nOutBufSize;
            result = TRUE;

            DEBUGMSG(ZONE_APIS, (TEXT("Leaving PpfsWriteDebugString")));
        }
        break;
        
    }

    LeaveCriticalSectionMM(g_pcsMain);

    return result;
}

BOOL RELFSD_GetVolumeInfo(VolumeState *pVolume, FSD_VOLUME_INFO *pInfo)
{
    pInfo->dwFlags = FSD_FLAG_NETWORK;
    return TRUE;
}
