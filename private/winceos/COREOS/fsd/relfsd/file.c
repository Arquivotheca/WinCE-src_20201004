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
#include "relfsd.h"
#include "cefs.h"
#include <ppfs.h>

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
    BOOL fSpecialFile=FALSE;

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


    if ((wcscmp(lpFileName, L"VOL:") == 0) ||
        (wcscmp(lpFileName, L"reg:") == 0) ||
        (wcscmp(lpFileName, L"con") == 0)) {
        fSpecialFile = TRUE;
    }
    if (g_fSecureRelfsd && fSpecialFile) {
        RETAILMSG(1, (TEXT("!ReleaseFSD: Error. Secure mode CreateFile does not support special files - 'VOL:', 'reg:' and 'con'.\n")));
        SetLastError(ERROR_NOT_SUPPORTED);
        return INVALID_HANDLE_VALUE;
    }                          

    if ((OPEN_EXISTING == dwCreate) && (FILE_ATTRIBUTE_ROMMODULE == dwFlagsAndAttributes)) {
        // special loader flag to load a rom-module across relfsd for debug purpose
        dwFlagsAndAttributes = 0;   // unset the flag, go through normal process
    } else if (!fSpecialFile && (OPEN_EXISTING == dwCreate) && !(GENERIC_WRITE & dwAccess)) {
        // fail the create call if the file exists in ROM
        StringCchCopy( szWindowsName, MAX_PATH,L"\\WINDOWS\\");
        szWindowsName[MAX_PATH-1] = 0;
        StringCchCatN( szWindowsName, MAX_PATH,lpFileName, MAX_PATH-20);
        szWindowsName[MAX_PATH-1] = 0;
        StringCchCopyN( wfd.cFileName, MAX_PATH,lpFileName, MAX_PATH-1);
        wfd.cFileName[MAX_PATH-1] = 0;
        wfd.cFileName[0] = (WCHAR)wcslen(lpFileName+1); // safe due to MAX_PATH
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
        SetLastError(fExists ? ERROR_ACCESS_DENIED : ERROR_FILE_NOT_FOUND);
        // This could also be ERROR_SHARING_VIOLATION but we have no way of figuring this out
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
        nRead = rread(pfs->fs_Handle, (PCHAR) pBuffer, cbRead);
        LeaveCriticalSectionMM(g_pcsMain);
    }       
    else
    {
        DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : FSD_ReadFile %#x bytes\n"), cbRead));
        EnterCriticalSectionMM(g_pcsMain);
        nRead = rread(pfs->fs_Handle, (PCHAR) pBuffer, cbRead);
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
        nRead = rreadseek (pfs->fs_Handle, (PCHAR)pBuffer, cbRead, (LONG)pos);
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
        nWritten = rwrite(pfs->fs_Handle, (PCHAR)pBuffer, cbWrite);
        LeaveCriticalSectionMM(g_pcsMain);
    }   
    else
    {
        DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: FSD_WriteFile %#x bytes\n"), cbWrite));
        EnterCriticalSectionMM(g_pcsMain);
        nWritten = rwrite(pfs->fs_Handle, (PCHAR)pBuffer, cbWrite);
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
    nWritten = rwrite(pfs->fs_Handle, (PCHAR) pBuffer, cbWrite);
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
    BOOL fSuccess = FALSE;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: SetEndOfFile\n")));

    EnterCriticalSectionMM(g_pcsMain);
    fSuccess = rsetendoffile(pfs->fs_Handle);
    LeaveCriticalSectionMM(g_pcsMain);

    return fSuccess;
}

// call from within kernel space
BOOL IntDeviceIoControl( FileState *pfs, DWORD dwIoControlCode, PBYTE lpInBuf, DWORD nInBufSize,
                                  PBYTE lpOutBuf, DWORD nOutBufSize, PDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    DWORD result = (DWORD)-1;
    DWORD dwErr = NO_ERROR;

    EnterCriticalSectionMM (g_pcsMain);
    
    switch (dwIoControlCode)
    {
        case IOCTL_REG_OPEN:
        {
            PCEFSINPUTSTRUCT pInBuf = (PCEFSINPUTSTRUCT) lpInBuf;
            LPDWORD lphKey = (LPDWORD)lpOutBuf;
            
            DEBUGMSG(ZONE_APIS, (L"ReleaseFSD: RegOpen\n"));
                
            result = rRegOpen (pInBuf->hKey, pInBuf->szName, lphKey);
            if ((-1 != result) && lpBytesReturned) {
                *lpBytesReturned = sizeof(DWORD);
            }
            
            DEBUGMSG(ZONE_APIS, (L"RegOpen returned %08X, result=%08X on %08X:%a\n", *lphKey, result, pInBuf->hKey, pInBuf->szName));
        }
        break;
        
        case IOCTL_REG_CLOSE:
        {
            DEBUGMSG(ZONE_APIS, (L"ReleaseFSD: RegClose\n"));
                
            result = rRegClose (*(LPDWORD)lpInBuf);
            if ((-1 != result) && lpBytesReturned) {
                *lpBytesReturned = 0;
            }

            DEBUGMSG(ZONE_APIS, (L"RegClose returned %d\n", result));
        }
        break;
        
        case IOCTL_REG_GET:
        {            
            PCEFSINPUTSTRUCT pInBuf = (PCEFSINPUTSTRUCT) lpInBuf;
            PCEFSOUTPUTSTRUCT pOutBuf = (PCEFSOUTPUTSTRUCT) lpOutBuf;
            
            DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD: RegGet\n")));

            result = rRegGet (pInBuf->hKey, pInBuf->szName, &(pOutBuf->dwType), pOutBuf->data, &(pOutBuf->dwSize));
            if (result && lpBytesReturned) {
                *lpBytesReturned = pOutBuf->dwSize + offsetof(CEFSOUTPUTSTRUCT, data);
            }

            DEBUGMSG(ZONE_APIS, (L"RegGet returned %d on %a\n", result, pInBuf->szName));
        }
        break;

        default:
            dwErr = ERROR_NOT_SUPPORTED;
    }

    LeaveCriticalSectionMM(g_pcsMain);
    
    if (dwErr) {
        SetLastError(dwErr);
    }
    
    return result;
}

// call from user space
BOOL ExtDeviceIoControl( FileState *pfs, DWORD dwIoControlCode, PBYTE lpInBuf, DWORD nInBufSize,
                                  PBYTE lpOutBuf, DWORD nOutBufSize, PDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    DWORD dwErr = NO_ERROR;
    DWORD result = (DWORD)-1;

    switch (dwIoControlCode)
    {
        case IOCTL_REG_OPEN:
        {                        
            DWORD hKey;

            if (!lpInBuf
                || !lpOutBuf 
                || (nInBufSize != sizeof(CEFSINPUTSTRUCT))
                || (nOutBufSize != sizeof(hKey))) {
                dwErr = ERROR_INVALID_PARAMETER;

            } else {
                // copy the in arguments
                CEFSINPUTSTRUCT cefsin = *((PCEFSINPUTSTRUCT)lpInBuf);
                cefsin.szName[_countof(cefsin.szName) - 1] = 0;

                // call the internal api                    
                result = IntDeviceIoControl (pfs, dwIoControlCode, (PBYTE)&cefsin, sizeof(cefsin), (PBYTE)&hKey, sizeof(hKey), NULL, NULL);

                // copy the out arguments
                if (-1 != result) {
                    __try {
                        if (lpBytesReturned)
                            *lpBytesReturned = sizeof(hKey);
                        memcpy (lpOutBuf, &hKey, sizeof(hKey));
                    } __except (EXCEPTION_EXECUTE_HANDLER) {
                        result = (DWORD)-1;
                        rRegClose (hKey);
                        dwErr = ERROR_INVALID_PARAMETER;
                    }
                }
            }
        }
        break;
        
        case IOCTL_REG_CLOSE:
        {
            if (!lpInBuf
                || (nInBufSize != sizeof(DWORD))) {
                dwErr = ERROR_INVALID_PARAMETER;

            } else {
                // copy the in arguments
                DWORD hKey = *(LPDWORD)lpInBuf;

                // call the internal api
                result = IntDeviceIoControl (pfs, dwIoControlCode, (PBYTE)&hKey, sizeof(hKey), NULL, 0, NULL, NULL);

                // copy the out arguments
                if ((-1 != result) && lpBytesReturned) {
                    *lpBytesReturned = 0;
                }
            }
        }
        break;
        
        case IOCTL_REG_GET:
        {
            PCEFSOUTPUTSTRUCT pOutBuf = (PCEFSOUTPUTSTRUCT) lpOutBuf;

            if (!lpInBuf
                || !lpOutBuf
                || (nInBufSize != sizeof(CEFSINPUTSTRUCT))
                || (nOutBufSize != sizeof(CEFSOUTPUTSTRUCT))
                || (pOutBuf->dwSize > sizeof(pOutBuf->data))) {
                result = 0; // ioctl return value is different from others on failure
                dwErr = ERROR_INVALID_PARAMETER;

            } else {
                // copy the in arguments
                DWORD dwSize;
                CEFSOUTPUTSTRUCT cefsout;
                CEFSINPUTSTRUCT cefsin = *((PCEFSINPUTSTRUCT) lpInBuf);
                cefsin.szName[_countof(cefsin.szName) - 1] = 0;
                cefsout.dwSize = sizeof(pOutBuf->data);
                
                // call the internal api
                result = IntDeviceIoControl (pfs, dwIoControlCode, (PBYTE)&cefsin, sizeof(cefsin), (PBYTE)&cefsout, sizeof(cefsout), &dwSize, NULL);

                // copy the out arguments                
                if (result) {
                    if (lpBytesReturned)
                        *lpBytesReturned = dwSize;
                    memcpy (pOutBuf, &cefsout, dwSize);
                }
            }
        }
        break;

        default:
            dwErr = ERROR_NOT_SUPPORTED;
    }

    if (dwErr) {
        SetLastError (dwErr);
    }
    
    return result;
}

// re-direct to external or internal interface based on the caller
BOOL RELFSD_DeviceIoControl( FileState *pfs, DWORD dwIoControlCode, PBYTE lpInBuf, DWORD nInBufSize,
                                  PBYTE lpOutBuf, DWORD nOutBufSize, PDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    return ((GetDirectCallerProcessId() == GetCurrentProcessId()) ?
                IntDeviceIoControl (pfs, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped) :
                ExtDeviceIoControl (pfs, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped));
}

BOOL RELFSD_GetVolumeInfo(VolumeState *pVolume, FSD_VOLUME_INFO *pInfo)
{
    pInfo->dwFlags = FSD_FLAG_NETWORK;
    return TRUE;
}

BOOL RELFSD_FsIoControl(VolumeState* pVolume, 
                        DWORD dwIoctl, 
                        VOID *pInBuf,
                        DWORD cbInBuf, 
                        VOID *pOutBuf, 
                        DWORD cbOutBuf, 
                        DWORD *pcbReturned, 
                        OVERLAPPED *pOverlapped)
{
    BOOL    bReturn = FALSE;

    UNREFERENCED_PARAMETER(pVolume);
    UNREFERENCED_PARAMETER(cbInBuf);
    UNREFERENCED_PARAMETER(pOutBuf);
    UNREFERENCED_PARAMETER(cbOutBuf);
    UNREFERENCED_PARAMETER(pcbReturned);
    UNREFERENCED_PARAMETER(pOverlapped);

    if (g_fSecureRelfsd) {
        RETAILMSG(1, (TEXT("!ReleaseFSD: Error. Secure mode does not support RELFSD FsIoctls.\n")));
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }

    //
    // Overload FSCTL_REFRESH_VOLUME to mean that we should
    // reload the release directory modules list.  pInBuf can specify
    // the null-terminated filename, if NULL use the default dbglist.txt
    // 

    if ( dwIoctl == FSCTL_REFRESH_VOLUME )
    {
        bReturn = LoadDbgList((LPCWSTR)pInBuf);
    }

    return bReturn;
}

