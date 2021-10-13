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
#include "binfs.h"



extern "C" HANDLE FSD_CreateFileW(BinVolume *pVolume, HANDLE hProc, LPCWSTR lpFileName, DWORD dwAccess, DWORD dwShareMode, 
                            LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreate, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    DWORD dwError =ERROR_SUCCESS;
    BinDirList *pDirectory = pVolume->pDirectory;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    
    DEBUGMSG(ZONE_API, 
             (L"BINFS: FSD_CreateFileW lpFileName=%s dwAccess=%08X dwShare=%08X dwCreate=%08X dwFlags=%08X\n", 
             lpFileName, dwAccess, dwShareMode, dwCreate, dwFlagsAndAttributes));

    __try {
        if (!lpFileName || (wcslen(lpFileName) > MAX_PATH)) {
            dwError = ERROR_INVALID_PARAMETER;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
    }
    if (ERROR_SUCCESS == dwError) {
        while ((*lpFileName == L'\\') || (*lpFileName == L'/'))
            lpFileName++;

        if (wcslen(lpFileName) > SYSTEM_DIR_LEN) {
            if ((wcsnicmp( lpFileName, SYSTEM_DIR, SYSTEM_DIR_LEN) == 0) && ((*(lpFileName+SYSTEM_DIR_LEN) == L'\\') || (*(lpFileName+SYSTEM_DIR_LEN) == L'/'))){
                lpFileName += SYSTEM_DIR_LEN;
                while ((*lpFileName == L'\\') || (*lpFileName == L'/')) {
                    lpFileName++;
                }
            }
        }

        while(pDirectory) {
            if (wcsicmp( pDirectory->szFileName, lpFileName) == 0) 
                break;
            pDirectory = pDirectory->pNext;
        }
        if (pDirectory) {
            if ((dwAccess & GENERIC_WRITE) || 
                (dwCreate == CREATE_NEW) || 
                (dwCreate == CREATE_ALWAYS) || 
                (dwCreate == TRUNCATE_EXISTING))
            {
                dwError = ERROR_ACCESS_DENIED;

            } else if (pDirectory->pe32 && (pDirectory->pe32->e32_objcnt > MAX_REGIONS)) {

                // this module has too many sections, fail the CreateFile call
                dwError = ERROR_BAD_FORMAT;
                RETAILMSG(1, (L"BINFS!FSD_CreateFileW: ERROR! module \"%s\" has %u o32 sections (max sections = %u)\r\n", pDirectory->szFileName, pDirectory->pe32->e32_objcnt, MAX_O32_SECTIONS));
                
            } else {
                FileHandle *pFile = new FileHandle;
                if (pFile) {
                    pFile->pDirectory = pDirectory;
                    pFile->dwShareMode = dwShareMode;
                    pFile->dwPosition = 0;
                    pFile->pVolume = pVolume;
                    hFile = FSDMGR_CreateFileHandle(pVolume->hVolume, hProc, (PFILE)pFile);
                } else {
                    dwError = GetLastError();
                }    
            }    
        } else {
            dwError = ERROR_FILE_NOT_FOUND;
        }    
    }    
            
    if (ERROR_SUCCESS != dwError) {
        SetLastError( dwError);
    }
    return hFile;
}


extern "C" BOOL FSD_CloseFile(FileHandle *pFile)
{
    delete pFile;
    return TRUE;
}

extern "C" BOOL FSD_ReadFile(FileHandle *pFile, BYTE *pBuffer, DWORD dwBytesToRead, LPDWORD lpNumBytesRead, LPOVERLAPPED lpOverlapped)
{
    DWORD dwError = ERROR_SUCCESS;
    DWORD dwRead = 0;

    if (dwBytesToRead+pFile->dwPosition > pFile->pDirectory->dwRealFileSize)
        dwBytesToRead = pFile->pDirectory->dwRealFileSize-pFile->dwPosition;

    if (dwBytesToRead) {
        if (!pFile->pDirectory->po32) {
            dwRead = GetFileData(pFile->pVolume, pFile->pDirectory, pFile->dwPosition, pBuffer, dwBytesToRead);
            pFile->dwPosition += dwRead;
        } else {
            dwError = ERROR_ACCESS_DENIED;
        }    
    }
    
    if (lpNumBytesRead) {
        *lpNumBytesRead = dwRead;
    }    

    if (ERROR_SUCCESS != dwError) {
        SetLastError( dwError);
    }
    return (dwError == ERROR_SUCCESS);
}



extern "C" BOOL FSD_ReadFileWithSeek( FileHandle *pFile, BYTE *pBuffer, DWORD dwBytesToRead, LPDWORD lpNumBytesRead, LPOVERLAPPED lpOverlapped,
                                 DWORD dwLowOffset, DWORD dwHighOffset)
{
    DWORD dwError = ERROR_SUCCESS;
    BOOL fSuccess = FALSE;
    DWORD dwRead = 0;

    if (dwLowOffset + dwBytesToRead < dwLowOffset) {
        // catch integer overflow
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (dwBytesToRead) {
        if (!pFile->pDirectory->po32) {
            if (dwBytesToRead+dwLowOffset > pFile->pDirectory->dwRealFileSize)
                dwBytesToRead = pFile->pDirectory->dwRealFileSize-dwLowOffset;
            dwRead = GetFileData(pFile->pVolume, pFile->pDirectory, dwLowOffset, pBuffer, dwBytesToRead);
        } else {
            // BUGBUG: Temporary until we know this all works correctly
            BinDirList Directory;
            o32_rom *po32;
            if ( dwLowOffset >> 28 < pFile->pDirectory->pe32->e32_objcnt) {
                po32 = &(pFile->pDirectory->po32[dwLowOffset >> 28]);
                Directory.dwRegion = pFile->pDirectory->dwRegion;
                EnterCriticalSection( &pFile->pVolume->csComp);    
                pFile->pVolume->pCurCompDir = NULL;
                Directory.dwAddress = po32->o32_dataptr;
                Directory.dwRealFileSize= po32->o32_psize;
                Directory.dwCompFileSize = po32->o32_psize;
                if (po32->o32_flags & IMAGE_SCN_COMPRESSED) {
                    Directory.dwRealFileSize= po32->o32_vsize;
                } 
                dwRead = GetFileData( pFile->pVolume, &Directory, dwLowOffset & 0x0fffffff, pBuffer, dwBytesToRead);
                LeaveCriticalSection(&pFile->pVolume->csComp);
#if 0            
                if (i == pFile->pDirectory->pe32->e32_objcnt) {
                dwError = ERROR_NOT_FOUND;
                }    
#endif            
            } else {
                // the offset refers to an invalid section
                dwError = ERROR_INVALID_PARAMETER;
            }
        }    
    }    
    if (lpNumBytesRead) {
        *lpNumBytesRead = dwRead;
    }    
    
    if (ERROR_SUCCESS != dwError) {
        SetLastError( dwError);
    }
    return (dwError == ERROR_SUCCESS);
}

extern "C" BOOL FSD_WriteFile( FileHandle *pFile, LPCVOID buffer, DWORD dwBytesToWrite, LPDWORD lpNumBytesWritten, LPOVERLAPPED lpOverlapped)
{
    if (lpNumBytesWritten)
        *lpNumBytesWritten = 0;
    SetLastError( ERROR_ACCESS_DENIED);
    return FALSE;
}



extern "C" BOOL FSD_WriteFileWithSeek( FileHandle *pFile, LPCVOID buffer, DWORD dwBytesToWrite, LPDWORD lpNumBytesWritten, LPOVERLAPPED lpOverlapped,
                                  DWORD dwLowOffset, DWORD dwHighOffset)
{
    if (lpNumBytesWritten)
        *lpNumBytesWritten = 0;
        
    SetLastError( ERROR_ACCESS_DENIED);
    return FALSE;
}

extern "C" DWORD FSD_SetFilePointer( FileHandle *pFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
    switch( dwMoveMethod) {
        case FILE_BEGIN:
            pFile->dwPosition = lDistanceToMove;
            break;
        case FILE_CURRENT:
            pFile->dwPosition += lDistanceToMove;
            break;
        case FILE_END:
            pFile->dwPosition = pFile->pDirectory->dwRealFileSize - lDistanceToMove;
            break;
        default:
            break;
    }
    return pFile->dwPosition;
}


extern "C" DWORD FSD_GetFileSize(FileHandle *pFile, LPDWORD lpFileSizeHigh)
{
    if (lpFileSizeHigh)
        *lpFileSizeHigh = 0;
        
    return pFile->pDirectory->dwRealFileSize;
}


extern "C" BOOL FSD_GetFileInformationByHandle(FileHandle *pFile, LPBY_HANDLE_FILE_INFORMATION lpFileInfo)
{
    if (lpFileInfo) {
        if (pFile->pDirectory->dwAttributes & FILE_ATTRIBUTE_ROMMODULE) {
            lpFileInfo->dwOID = 0; /* pFile->pDirectory->dwAddress; */
        } else {
            lpFileInfo->dwOID = (DWORD)INVALID_HANDLE_VALUE;
        }
        lpFileInfo->dwFileAttributes = pFile->pDirectory->dwAttributes;
        lpFileInfo->dwVolumeSerialNumber = ROM_SIGNATURE;
        lpFileInfo->ftCreationTime = pFile->pDirectory->ft;
        lpFileInfo->ftLastAccessTime = pFile->pDirectory->ft;
        lpFileInfo->ftLastWriteTime = pFile->pDirectory->ft;
        lpFileInfo->nFileIndexHigh = 0;
        lpFileInfo->nFileIndexLow = (DWORD)pFile->pDirectory;
        lpFileInfo->nFileSizeHigh = 0;
        lpFileInfo->nFileSizeLow = pFile->pDirectory->dwRealFileSize;
        lpFileInfo->nNumberOfLinks = 0;
    }
    return TRUE;
}


extern "C" BOOL FSD_FlushFileBuffers(FileHandle *pFile)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}


extern "C" BOOL FSD_GetFileTime( FileHandle *pFile, LPFILETIME lpCreation, LPFILETIME lpLastAccess, LPFILETIME lpLastWrite)
{
    DEBUGMSG(ZONE_API, (L"BINFS: FSD_GetFileTime\n"));
    if (lpCreation)
        memcpy( lpCreation, &pFile->pDirectory->ft, sizeof(FILETIME));
    if (lpLastAccess)
        memcpy( lpLastAccess, &pFile->pDirectory->ft, sizeof(FILETIME));
    if (lpLastWrite)
        memcpy( lpLastWrite,&pFile->pDirectory->ft, sizeof(FILETIME));
        
    return TRUE;
}


extern "C" BOOL FSD_SetFileTime( FileHandle *pFile, CONST FILETIME *lpCreation, CONST FILETIME *lpLastAccess, CONST FILETIME *lpLastWrite)
{
    SetLastError( ERROR_ACCESS_DENIED);
    return FALSE;
}


extern "C" BOOL FSD_SetEndOfFile(FileHandle *pFile)
{
    SetLastError( ERROR_ACCESS_DENIED);
    return FALSE;
}

DWORD AccessFromFlags(    DWORD flags) 
{
    if (flags & IMAGE_SCN_MEM_WRITE)
        return (flags & IMAGE_SCN_MEM_EXECUTE ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
    else
        return (flags & IMAGE_SCN_MEM_EXECUTE ? PAGE_EXECUTE_READ : PAGE_READONLY);
}

BOOL SectionSupportsXIP(FileHandle *pFile, DWORD dwSection)
{
#ifdef DEBUG
    if (!(IMAGE_SCN_MEM_WRITE & pFile->pDirectory->po32[dwSection].o32_flags)
        && !(IMAGE_SCN_COMPRESSED & pFile->pDirectory->po32[dwSection].o32_flags)
        && 0 != (pFile->pDirectory->po32[dwSection].o32_dataptr & (UserKInfo[KINX_PAGESIZE]-1))) {
        DEBUGCHK(0);
    }
#endif // DEBUG

    // only read-only, uncompressed, page-aligned sections will support XIP   
    return (!(IMAGE_SCN_MEM_WRITE & pFile->pDirectory->po32[dwSection].o32_flags)
        && !(IMAGE_SCN_COMPRESSED & pFile->pDirectory->po32[dwSection].o32_flags)
        && (0 == (pFile->pDirectory->po32[dwSection].o32_dataptr & (UserKInfo[KINX_PAGESIZE]-1))));
}

DWORD GetXIPPages(FileHandle *pFile, DWORD dwSection, LPDWORD lpOutputList, DWORD cbOut, LPDWORD lpcbRet)
{
    DWORD dwError = ERROR_GEN_FAILURE;
    DWORD cbRet = 0;

    ASSERT(pFile);    
    
    if (!pFile || !pFile->pDirectory || !pFile->pDirectory->pe32 || !pFile->pDirectory->po32
        || (dwSection >= pFile->pDirectory->pe32->e32_objcnt)) {
        dwError = ERROR_INVALID_PARAMETER;

    } else if (!(VOL_FLAG_XIP & pFile->pVolume->dwVolFlags)
        || !SectionSupportsXIP(pFile, dwSection)) {
        // either the media or section does not support XIP
        dwError = ERROR_NOT_SUPPORTED;

    } else {

        DWORD dwPageCount = (pFile->pDirectory->po32[dwSection].o32_psize + (UserKInfo[KINX_PAGESIZE]-1)) / UserKInfo[KINX_PAGESIZE];
        ASSERT(dwPageCount > 0);

        cbRet = sizeof(DWORD) * dwPageCount;
        
        if (NULL == lpOutputList) { 
            // allow NULL output buffer to query the required buffer size
            dwError = ERROR_SUCCESS;

        } else if ((cbOut < cbRet) || (dwPageCount != cbRet / sizeof(DWORD))) { // check for integer overflow on the above multiplication
            // buffer is too small, output size will be required bytes
            dwError = ERROR_INSUFFICIENT_BUFFER;
            
        } else {
            // validated parameters
            DWORD *pdwPageList = new DWORD[dwPageCount];
            if (pdwPageList) {

                ASSERT(UserKInfo[KINX_PAGESIZE] == pFile->pVolume->diskInfo.di_bytes_per_sect);

                // build a list of pages for the section, we know they are contiguous sectors
                pdwPageList[0] = pFile->pDirectory->po32[dwSection].o32_dataptr / pFile->pVolume->diskInfo.di_bytes_per_sect;
                for ( DWORD i=1; i < dwPageCount; i++) {
                    pdwPageList[i] = 1 + pdwPageList[i-1];
                }

                // try to get XIP pages from the block driver
                dwError = ERROR_SUCCESS;
                if (!FSDMGR_DiskIoControl(pFile->pVolume->hDsk, IOCTL_DISK_GET_SECTOR_ADDR, pdwPageList, cbRet, (LPBYTE)lpOutputList, cbOut, lpcbRet, NULL)) {
                    dwError = GetLastError();
                    if (ERROR_SUCCESS == dwError) {
                        dwError = ERROR_GEN_FAILURE;
                    }
                }

#ifdef DEBUG
                // verify that the block driver returned all valid addresses
                for ( i=1; i < dwPageCount; i++) {
                    if ((DWORD)-1 == lpOutputList[i]) {
                        DEBUGMSG(ZONE_ERROR, (L"BINFS: IOCTL_DISK_GET_SECTOR_ADDR returned invalid address for page %u of module \"%s\" section %u\n",
                            i, pFile->pDirectory->szFileName, dwSection));
                        DEBUGCHK(0);
                        dwError = ERROR_SECTOR_NOT_FOUND;
                    }
                }
#endif // DEBUG

                delete[] pdwPageList;
            }
        }
    }

    if (ERROR_SUCCESS == dwError && lpcbRet) {
        *lpcbRet = cbRet;
    }
    return dwError;
}

extern "C" BOOL FSD_DeviceIoControl( FileHandle *pFile, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize,
                               LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    DWORD dwError =ERROR_INVALID_PARAMETER;
    DWORD dwBytesRet = 0;

    if (dwIoControlCode == IOCTL_BIN_GET_E32) {
        if (pFile->pDirectory->pe32 && lpOutBuf) {
            if (nOutBufSize == sizeof(e32_rom)) {
                dwBytesRet = sizeof(e32_rom);
#if 0                
                ((e32_lite *)lpOutBuf)->e32_objcnt = pFile->pDirectory->pe32->e32_objcnt;     /* Number of memory objects            */
                ((e32_lite *)lpOutBuf)->e32_cevermajor = (BYTE)pFile->pDirectory->pe32->e32_subsysmajor; /* version of CE built for             */
                ((e32_lite *)lpOutBuf)->e32_ceverminor = (BYTE)pFile->pDirectory->pe32->e32_subsysminor; /* version of CE built for             */
                ((e32_lite *)lpOutBuf)->e32_stackmax = pFile->pDirectory->pe32->e32_stackmax;   /* Maximum stack size                  */
                ((e32_lite *)lpOutBuf)->e32_vbase = pFile->pDirectory->pe32->e32_vbase;      /* Virtual base address of module      */
                ((e32_lite *)lpOutBuf)->e32_vsize = pFile->pDirectory->pe32->e32_vsize;      /* Virtual size of the entire image    */
                ((e32_lite *)lpOutBuf)->e32_sect14rva = pFile->pDirectory->pe32->e32_sect14rva;  /* section 14 rva */
                ((e32_lite *)lpOutBuf)->e32_sect14size = pFile->pDirectory->pe32->e32_sect14size; /* section 14 size */
                memcpy( &(((e32_lite *)lpOutBuf)->e32_unit[0]), 
                        &(pFile->pDirectory->pe32->e32_unit[0]), 
                        sizeof(struct info)*LITE_EXTRA); /* Array of extra info units     */                dwError = ERROR_SUCCESS;
#endif                        
                memcpy( lpOutBuf, pFile->pDirectory->pe32, sizeof(e32_rom));

                if (VOL_FLAG_XIP & pFile->pVolume->dwVolFlags) {
                    ((e32_rom *)lpOutBuf)->e32_imageflags |= IMAGE_FILE_XIP;
                }
                dwError = ERROR_SUCCESS;
            } else {
                dwError = ERROR_INSUFFICIENT_BUFFER;
            }
        }
    }
    if (dwIoControlCode == IOCTL_BIN_GET_O32) {
        if (pFile->pDirectory->pe32 && pFile->pDirectory->po32 && lpOutBuf) {
            if (nOutBufSize == (sizeof(o32_lite) * pFile->pDirectory->pe32->e32_objcnt)) {
                dwBytesRet = sizeof(o32_lite) * pFile->pDirectory->pe32->e32_objcnt;
                for ( DWORD i=0; i < pFile->pDirectory->pe32->e32_objcnt; i++) {
                    ((o32_lite *)lpOutBuf)[i].o32_vsize = pFile->pDirectory->po32[i].o32_vsize;
                    ((o32_lite *)lpOutBuf)[i].o32_rva = pFile->pDirectory->po32[i].o32_rva;
                    ((o32_lite *)lpOutBuf)[i].o32_realaddr = pFile->pDirectory->po32[i].o32_realaddr;
                    ((o32_lite *)lpOutBuf)[i].o32_access = AccessFromFlags(pFile->pDirectory->po32[i].o32_flags);
                    ((o32_lite *)lpOutBuf)[i].o32_flags = pFile->pDirectory->po32[i].o32_flags;
                    ((o32_lite *)lpOutBuf)[i].o32_psize = (pFile->pDirectory->po32[i].o32_flags & IMAGE_SCN_COMPRESSED)
                                                        ? pFile->pDirectory->po32[i].o32_vsize // Report back vsize as psize to the kernel to make behavior identical with IMGFS
                                                        : pFile->pDirectory->po32[i].o32_psize; 
                    ((o32_lite *)lpOutBuf)[i].o32_dataptr = i << 28;               
                }
                dwError = ERROR_SUCCESS;
            } else {
                dwError = ERROR_INSUFFICIENT_BUFFER;
            }
        }    
    }

    if (dwIoControlCode == IOCTL_BIN_GET_XIP_PAGES) {
        if  (lpInBuf && sizeof(DWORD) == nInBufSize) {
            dwError = GetXIPPages(pFile, *(LPDWORD)lpInBuf, (LPDWORD)lpOutBuf, nOutBufSize, &dwBytesRet);
        }
    }
        
    if (ERROR_SUCCESS != dwError) {
        SetLastError( dwError);
        dwBytesRet = 0;
    }
    if (lpBytesReturned)
        *lpBytesReturned = dwBytesRet;
    return (dwError == ERROR_SUCCESS);
}
