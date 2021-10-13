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

#include "windows.h"
#include "nkintr.h"
#include "bldver.h"
#include "kernel.h"
#include "cnnclpth.h"
#include "errorrep.h"
#include "corecrtstorage.h"
#include "UsrCoredllCallbacks.h"
#include "coredll.h"

#include "cscode.c"


/*
    @doc BOTH EXTERNAL

    @func BOOL | IsAPIReady | Tells whether the specified API set has been registered
    @parm DWORD | hAPI | The predefined system handle of the desired API set (from syscall.h)
    @comm During system initialization, some of the components may rely on other
          components that are not yet loaded.  IsAPIReady can be used to check
          if the desired API set is available and thus avoid taking an exception.
*/
BOOL
IsAPIReady(
    DWORD hAPI
    )
{
    //
    // IsAPIReady still exist for Backward compatibility. As we extended the number of API sets
    // from 16 to 128, existing binaries calling IsAPIReady for API from 16-31 will have to be
    // remapped to 80-95. The only issue here is that IsAPIReady will *not* be able to query
    // actual API set between 16-31.
    //
    RETAILMSG (1, (L"IsAPIReady is being deprecated, use WaitForAPIReady instead!!\r\n"));

    // convert hAPI between 16-31 to 80-95
    if ((DWORD) (hAPI - 16) < 16) {
        hAPI += (SH_FIRST_OS_API_SET - 16);
    }
    return (WAIT_OBJECT_0 == WaitForAPIReady (hAPI, 0));
}

// @func int | GetAPIAddress | Find API function address
// @rdesc Returns the function address of the requested API (0 if not valid)
// @parm int | setId | API Set index (via QueryAPISetID)
// @parm int | iMethod | method # within the API Set
// @comm Returns the kernel trap address used to invoke the given method within
// the given API Set.

FARPROC GetAPIAddress(int setId, int iMethod)
{
    if ((WAIT_OBJECT_0 != WaitForAPIReady((DWORD)setId, 0)) || (iMethod > METHOD_MASK))
        return 0;
    return (FARPROC)IMPLICIT_CALL((DWORD)setId, (DWORD)iMethod);
}


/* Support functions which do simple things and then (usually) trap into the kernel */

/* Zero's out critical section info */

/*
    @doc BOTH EXTERNAL

    @func BOOL | GetVersionEx | Returns version information for the OS.
    @parm LPOSVERSIONINFO | lpver | address of structure to fill in.

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
BOOL GetVersionEx(LPOSVERSIONINFO lpver) {
    DEBUGCHK(lpver->dwOSVersionInfoSize >= sizeof(OSVERSIONINFO));
    lpver->dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    lpver->dwMajorVersion = CE_MAJOR_VER;
    lpver->dwMinorVersion = CE_MINOR_VER;
    lpver->dwBuildNumber = CE_BUILD_VER;
    lpver->dwPlatformId = VER_PLATFORM_WIN32_CE;
    lpver->szCSDVersion[0] = '\0';
    return TRUE;
}

BOOL CeSafeCopyMemory (LPVOID pDst, LPCVOID pSrc, DWORD cbSize)
{
    __try {
        PREFAST_SUPPRESS(419, "This is a wrapper: caller is responsible for validation");        
        memcpy (pDst, pSrc, cbSize);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }
    return TRUE;
}


extern BOOL xxx_SystemParametersInfo_GWE(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni);

BOOL SystemParametersInfoW(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni) {
    BOOL retval = FALSE;
    DWORD bytesused;
    switch (uiAction) {
        case SPI_GETPLATFORMTYPE:
            uiAction = SPI_GETPLATFORMNAME; //platform type is deprecated
            __fallthrough;
        case SPI_GETOEMINFO:
        case SPI_GETPLATFORMVERSION:
        case SPI_GETPLATFORMNAME:
        case SPI_GETPROJECTNAME:
        case SPI_GETBOOTMENAME:
        case SPI_GETUUID:
        case SPI_GETGUIDPATTERN:
        
            __try  {
                retval = KernelIoControl(IOCTL_HAL_GET_DEVICE_INFO,&uiAction,4,pvParam,uiParam,&bytesused);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                ;
            }
            break;
        default:
            if (WAIT_OBJECT_0 == WaitForAPIReady(SH_WMGR, 0))
                retval = xxx_SystemParametersInfo_GWE(uiAction, uiParam, pvParam, fWinIni);
            break;
    }
    return retval;
}



/* Thread local storage routines (just like Win32) */

/*
    @doc BOTH EXTERNAL

    @func LPVOID | TlsGetValue | Retrieves the value in the calling thread's thread local
    storage (TLS) slot for a specified TLS index. Each thread of a process has its own slot
    for each TLS index.
    @parm DWORD | dwTlsIndex | TLS index to retrieve value for

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
LPVOID WINAPI TlsGetValue (DWORD slot)
{
    LPDWORD tlsptr = UTlsPtr ();
    LPVOID lpRet = NULL;
    DEBUGCHK (tlsptr);

    if (slot >= TLS_MINIMUM_AVAILABLE) {
        SetLastError (ERROR_INVALID_PARAMETER);


    // from SDK help:
    // Note  The data stored in a TLS slot can have a value of zero. In this case,
    // the return value is zero and GetLastError returns NO_ERROR.
    } else {
    
#ifndef SHIP_BUILD
        if ((slot >= TLSSLOT_NUMRES) && (GetCurrentProcessId () != (DWORD) GetOwnerProcess ())) {
            // accessing TLS in PSL context
            DEBUGCHK (0);
            SetLastError (ERROR_INVALID_PARAMETER);
        } else
#endif
        if (!(lpRet = (LPVOID) tlsptr[slot])) {
        
            SetLastError (NO_ERROR);
        }
    }
    return lpRet;
}

/*
    @doc BOTH EXTERNAL

    @func BOOL | TlsSetValue | Stores a value in the calling thread's thread local storage
    (TLS) slot for a specified TLS index. Each thread of a process has its own slot for each
    TLS index.
    @parm DWORD | dwTlsIndex | TLS index to set value for
    @parm LPVOID | lpvTlsValue | value to be stored

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
BOOL WINAPI TlsSetValue (DWORD slot, LPVOID value)
{
    LPDWORD tlsptr = UTlsPtr ();
    DEBUGCHK (tlsptr);

    if (slot < TLS_MINIMUM_AVAILABLE) {
#ifndef SHIP_BUILD
        if ((slot >= TLSSLOT_NUMRES) && (GetCurrentProcessId () != (DWORD) GetOwnerProcess ())) {
            // accessing TLS in PSL context
            DEBUGCHK (0);
        } else
#endif
        {
            tlsptr[slot] = (DWORD)value;
            return TRUE;
        }
    }
    SetLastError (ERROR_INVALID_PARAMETER);
    return FALSE;
}


/*
    @doc BOTH EXTERNAL

    @func VOID | GlobalMemoryStatus | Gets information on the physical and virtual memory of the system
    @parm LPMEMORYSTATUS | lpmst | pointer to structure to receive information
    @comm Follows the Win32 reference description without restrictions or modifications.
*/

VOID WINAPI GlobalMemoryStatus(LPMEMORYSTATUS lpmst) {
    DWORD addr;
    MEMORY_BASIC_INFORMATION mbi;
    lpmst->dwLength = sizeof(MEMORYSTATUS);
    lpmst->dwMemoryLoad = 100 - ((UserKInfo[KINX_PAGEFREE]*100) / UserKInfo[KINX_NUMPAGES]);
    lpmst->dwTotalPhys = UserKInfo[KINX_NUMPAGES]*UserKInfo[KINX_PAGESIZE];
    lpmst->dwAvailPhys = UserKInfo[KINX_PAGEFREE]*UserKInfo[KINX_PAGESIZE];
    lpmst->dwTotalPageFile = 0;
    lpmst->dwAvailPageFile = 0;
    lpmst->dwTotalVirtual = 1024*1024*1024;
    lpmst->dwAvailVirtual = 0;
    for (addr = 0x10000; addr < 1024*1024*1024; addr += (DWORD)mbi.RegionSize) {
        if (!VirtualQuery((LPCVOID)addr,&mbi,sizeof(mbi)))
            break;
        if (mbi.State == MEM_FREE)
            lpmst->dwAvailVirtual += (mbi.RegionSize - ((~(DWORD)mbi.BaseAddress+1)&0xffff)) & 0xffff0000;
    }
}

BOOL AttachHdstub (LPCWSTR dbgname) {
    return TRUE;
}


BOOL AttachOsAxsT0 (LPCWSTR dbgname) {
    return TRUE;
}

BOOL AttachOsAxsT1 (LPCWSTR dbgname) {
    return TRUE;
}



#if defined(x86)
// Turn off FPO optimization for CaptureDumpFileOnDevice function so that Watson can correctly unwind retail x86 call stacks
#pragma optimize("y",off)
#endif

BOOL CaptureDumpFileOnDevice(DWORD dwProcessId, DWORD dwThreadId, LPCWSTR pwzExtraFilesPath)
{
    BOOL fHandled = FALSE;
    DWORD dwArguments[5];
    WCHAR wzCanonicalExtraFilesPath[MAX_PATH];
    BOOL  fReportFault = (dwProcessId == (-1)) && (dwThreadId == (-1));
    DWORD dwArg2 = 0;

    if (!fReportFault)
    {
        if (pwzExtraFilesPath)
        {
            if (!CeGetCanonicalPathNameW(pwzExtraFilesPath, wzCanonicalExtraFilesPath, ARRAY_SIZE(wzCanonicalExtraFilesPath), 0))
            {
                fHandled = FALSE;
                SetLastError(ERROR_BAD_PATHNAME);
                goto Exit;
            }
            dwArg2 = (DWORD)wzCanonicalExtraFilesPath;
        }
    }
    else
    {
        // For ReportFault this is actually the pointer to the exception
        dwArg2 = (DWORD)pwzExtraFilesPath;
    }

    dwArguments[0] = dwProcessId;
    dwArguments[1] = dwThreadId;
    dwArguments[2] = dwArg2;

    // We pass in the CurrentTrust as an extra safety check in DwDmpGen.cpp
    // DwDmpGen.cpp will do additional trust level checking.
    dwArguments[3] = CeGetCurrentTrust();
    dwArguments[4] = (DWORD)&CaptureDumpFileOnDevice;

    __try
    {
        // This exception will be handled by OsAxsT0.dll if
        // we succesfully generate a dump file. The RaisException
        // will return if handled, otherwise it will caught by the
        // the try catch block.
        RaiseException(STATUS_CRASH_DUMP,0,5,&dwArguments[0]);
        fHandled = TRUE;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        // We end up here if no dump was captured, in which case we return FALSE
        fHandled = FALSE;
        if (ERROR_SUCCESS == GetLastError())
        {
            SetLastError(ERROR_NOT_SUPPORTED);
        }
    }

Exit:

    return fHandled;
}

#if defined(x86)
// Re-Enable optimization
#pragma optimize("",on)
#endif

EFaultRepRetVal APIENTRY ReportFault(LPEXCEPTION_POINTERS pep, DWORD dwMode)
{
    if (!CaptureDumpFileOnDevice(-1,-1,(LPCWSTR)pep))
    {
        return frrvErrNoDW;
    }
    return frrvOk;
}

BOOL SetInterruptEvent(DWORD idInt) {
    long mask;
    long pend;
    long *ptrPend;

    if ((idInt < SYSINTR_DEVICES) || (idInt >= SYSINTR_MAXIMUM))
        return FALSE;
    idInt -= SYSINTR_DEVICES;
    ptrPend = (long *) UserKInfo[KINX_PENDEVENTS];

    // calculate which DWORD based on idInt
    ptrPend += idInt >> 5;  // idInt / 32
    idInt   &= 0x1f;        // idInt % 32

    mask = 1 << idInt;
    do {
        pend = *ptrPend;
        if (pend & mask)
            return TRUE;    // The bit is already set, so all done.
    } while (InterlockedTestExchange(ptrPend, pend, pend|mask) != pend);
    return TRUE;
}

// Power Handler can now call SetEvent directly
BOOL CeSetPowerOnEvent (HANDLE hEvt)
{
    return SetEvent (hEvt);
}

static CONST WCHAR szHex[] = L"0123456789ABCDEF";

UINT GetTempFileNameW(LPCWSTR lpPathName, LPCWSTR lpPrefixString, UINT uUnique, LPWSTR lpTempFileName) {
    DWORD Length, Length2, PassCount, dwAttr;
    UINT uMyUnique;
    HANDLE hFile;
    Length = wcslen(lpPathName);
    if (!Length || (Length >= MAX_PATH)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    memcpy(lpTempFileName,lpPathName,Length*sizeof(WCHAR));
    if (lpTempFileName[Length-1] != (WCHAR)'\\')
        Length++;
    lpTempFileName[Length-1] = 0;
    dwAttr = GetFileAttributesW(lpTempFileName);
    if ((dwAttr == 0xFFFFFFFF) || !(dwAttr & FILE_ATTRIBUTE_DIRECTORY)) {
        SetLastError(ERROR_DIRECTORY);
        return 0;
    }
    lpTempFileName[Length-1] = L'\\';
    PassCount = 0;
    Length2 = wcslen(lpPrefixString);
    if (Length2 > 3)
        Length2 = 3;
    memcpy(&lpTempFileName[Length],lpPrefixString,Length2*sizeof(WCHAR));
    Length += Length2;
    uUnique &= 0x0000ffff;
    if ((Length + 9) > MAX_PATH) { // 4 hex digits, .tmp,  and a null
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    lpTempFileName[Length+4] = '.';
    lpTempFileName[Length+5] = 't';
    lpTempFileName[Length+6] = 'm';
    lpTempFileName[Length+7] = 'p';
try_again:
    if (!uUnique) {
        if (!(uMyUnique = (UINT)Random() & 0x0000ffff)) {
            if (!(++PassCount & 0xffff0000))
                goto try_again;
            SetLastError(ERROR_RETRY);
            return 0;
        }
    } else
        uMyUnique = uUnique;
    lpTempFileName[Length] = szHex[(uMyUnique >> 12) & 0xf];
    lpTempFileName[Length+1] = szHex[(uMyUnique >> 8) & 0xf];
    lpTempFileName[Length+2] = szHex[(uMyUnique >> 4) & 0xf];
    lpTempFileName[Length+3] = szHex[uMyUnique & 0xf];
    lpTempFileName[Length+8] = 0;
    if (!uUnique) {
        if ((hFile = CreateFileW(lpTempFileName, GENERIC_READ, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0)) == INVALID_HANDLE_VALUE) {
            switch (GetLastError()) {
                case ERROR_FILE_EXISTS:
                case ERROR_ALREADY_EXISTS:
                    if (!(++PassCount & 0xffff0000))
                        goto try_again;
                    break;
            }
            return 0;
        } else
            CloseHandle(hFile);
    }
    return uMyUnique;
}

// On success, return length of lpCanonicalPathName.  Otherwise, return 0.
DWORD
CeGetCanonicalPathNameW(
    LPCWSTR lpPathName,
    LPWSTR lpCanonicalPathName,
    DWORD cchCanonicalPathName,
    DWORD dwReserved
    )
{
    ULONG cchPathName;           // length of lpPathName; number of remaining characters
    ULONG ulPathIndex;           // current index in path
    ULONG ulNextDirectoryIndex;  // index(next directory relative to uiPathIndex)
    ULONG ulJumpOffset;          // skip extraneous '\\'-s or '/'-s
    PATH *pPath;                   // pointer to path object
    PATH_NODE PathNode;          // current path node
    PATH_NODE_TYPE PathNodeType; // type of current path node
    HRESULT hResult;             // StringCchLength return
    DWORD dwReturn;              //function return value

    // Note: lpCanonicalPathName can be NULL
    if (!lpPathName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    // Calculate length of lpPathName; StringCchLength succeeds if lpPathName <= MAX_PATH
    hResult = StringCchLength(lpPathName, MAX_PATH, &cchPathName);
    if (FAILED(hResult)) {
        SetLastError(ERROR_FILENAME_EXCED_RANGE);
        return 0;
    }
    
    pPath = (PATH *)LocalAlloc(LPTR, sizeof(PATH));
    if(!pPath){
        SetLastError(ERROR_OUTOFMEMORY);
        return 0;
    }
        
    // Initialize path object
    InitPath(pPath);
    // Determine path type
    GetPathType(lpPathName, cchPathName, &pPath->PathType);
    // Jump to first path node; Subtract jump offset from cchPathName
    ulPathIndex = GetIndexOfNextPathNodeInPath(lpPathName, &cchPathName);

    // Process path name
    while (cchPathName > 0) {
        // Reset path node type
        PathNodeType = PNT_UNKNOWN;
        ulNextDirectoryIndex = GetIndexOfNextDirectoryInPath(&lpPathName[ulPathIndex], cchPathName);
        if (ulNextDirectoryIndex > 0) {
            // Directory
            GetPathNodeType(
                &lpPathName[ulPathIndex], ulNextDirectoryIndex, &PathNodeType);
            switch (PathNodeType) {
            case PNT_SELF:
                break;
            case PNT_PARENT:
                PopPathNode(pPath);
                break;
            case PNT_FILE:
                PathNode.ulPathIndex = ulPathIndex;
                PathNode.ulNameLength = ulNextDirectoryIndex;
                PathNode.PathNodeType = PathNodeType;
                PushPathNode(pPath, PathNode);
                break;
            default:
                // This should never happen
                DEBUGCHK(0);
            }
            // Jump over path node
            cchPathName -= (1 + ulNextDirectoryIndex);
            ulPathIndex += (1 + ulNextDirectoryIndex);
            // Ignore extraneous '\\'-s or '/'-s; Subtract jump offset from cchPathName
            ulJumpOffset = GetIndexOfNextPathNodeInPath(&lpPathName[ulPathIndex], &cchPathName);
            ulPathIndex += ulJumpOffset;
        }
        else {
            // File; Last node in path
            GetPathNodeType(&lpPathName[ulPathIndex], cchPathName, &PathNodeType);
            switch (PathNodeType) {
            case PNT_SELF:
                break;
            case PNT_PARENT:
                PopPathNode(pPath);
                break;
            case PNT_FILE:
                PathNode.ulPathIndex = ulPathIndex;
                PathNode.ulNameLength = cchPathName;
                PathNode.PathNodeType = PathNodeType;
                PushPathNode(pPath, PathNode);
                break;
            default:
                // This should never happen
                DEBUGCHK(0);
            }
            cchPathName = 0;
        }
    }

    // Calculate (cache) length of canonical path name
    cchPathName = GetPathLength(pPath);

    // If lpCanonicalPathName == NULL, then caller is only interested in the
    // length of the canonicalized version of lpPathName; Ignore cchCanonicalPathName
    if (lpCanonicalPathName == NULL) {
        LocalFree(pPath);
        return cchPathName;
    }

    // lpCanonicalPathName must be large enough to contain canonical path name
    if (cchPathName > cchCanonicalPathName) {
        LocalFree(pPath);
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return 0;
    }

    // Pop canonical path from stack into lpCanonicalPathName
    dwReturn = PopPath(pPath, lpPathName, lpCanonicalPathName);
    LocalFree(pPath);
    return dwReturn;
}

#define FS_COPYBLOCK_SIZE             (64*1024) // Perfect size for a VirtualAlloc
#define COPYFILEEX_FAIL_IF_DST_EXISTS (1 << 0)
#define COPYFILEEX_SRC_READ_ONLY      (1 << 1)
#define COPYFILEEX_QUIET              (1 << 2)
#define COPYFILEEX_RET                (1 << 3)
#define COPYFILEEX_DELETE_DST         (1 << 4)

#include <storemgr.h>
#include <fsioctl.h>

BOOL
CopyFileExW(
    LPCWSTR lpExistingFileName,
    LPCWSTR lpNewFileName,
    LPPROGRESS_ROUTINE lpProgressRoutine,
    LPVOID lpData,
    LPBOOL pbCancel,
    DWORD dwCopyFlags // NT ignores COPY_FILE_RESTARTABLE
    )
{
    // File support
    HANDLE hExistingFile, hNewFile;        // Source and destination file handles
    DWORD dwFileAttributes;                // Source file attributes
    DWORD dwDestAttributes;                // Destination file attributes
    LARGE_INTEGER liExistingFileSize;      // Source file size
    FILETIME ftCreation, ftLastAccess, ftLastWrite; // Source file times
    WCHAR lpszCExisting[MAX_PATH];         // Canonical version of lpszExisting
    WCHAR lpszCNew[MAX_PATH];              // Canonical version of lpszNew
    BOOL fNewFileExists = FALSE;           // Destination file exists
    CE_VOLUME_INFO_LEVEL InfoLevel;        // Volume info type
    CE_VOLUME_INFO VolumeInfo;             // Volume info structure

    // Copy support
    LPBYTE lpbCopyChunk = NULL;            // Copy buffer
    DWORD dwBytesRead, dwBytesWritten;     // Copy bytes read and written
    DWORD dwCopyProgressResult;            // CopyProgressRoutine result
    LARGE_INTEGER liTotalBytesTransferred; // Bytes transferred

    // Result support
    DWORD dwError;                         // Capture last error of nested calls

    // Status flags
    DWORD dwFlags = COPYFILEEX_DELETE_DST; // bit 0=fFailIfDestinationExists;
                                           // bit 1=fDestinationFileExists
                                           // bit 2=fSourceFileReadyOnly
                                           // bit 3=fQuiet (Call CopyProgressRoutine)
                                           // bit 4=fRet
                                           // bit 5=fDeleteDestinationFile

    // Canonicalize lpExistingFileName and lpNewFileName
    if (!CeGetCanonicalPathName(lpNewFileName, lpszCNew, MAX_PATH, 0)) {
        return FALSE;
    }
    if (!CeGetCanonicalPathName(lpExistingFileName, lpszCExisting, MAX_PATH, 0)) {
        return FALSE;
    }

    if (dwCopyFlags & COPY_FILE_FAIL_IF_EXISTS) dwFlags |= COPYFILEEX_FAIL_IF_DST_EXISTS;

    // Note FILE_FLAG_NO_BUFFERING: Except in rare cases where someone holds a
    // second handle to the file so we'd get benefit from reusing cached data,
    // copying through a cache will only slow us down.  Also cached data could
    // lead to flushes during CloseHandle, stomping FileTime on the new file.

    // Open source file
    hExistingFile = CreateFile(lpszCExisting, GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, 0);
    if (INVALID_HANDLE_VALUE == hExistingFile) {
        return FALSE;
    }

    if (!(dwFlags & COPYFILEEX_FAIL_IF_DST_EXISTS) &&
        (0 == _wcsnicmp(lpszCExisting, lpszCNew, MAX_PATH))) {
        // Cannot copy a file onto itself
        VERIFY(CloseHandle(hExistingFile));
        SetLastError(ERROR_SHARING_VIOLATION);
        return FALSE;
    }

    // Get source file's attributes
    dwFileAttributes = GetFileAttributes(lpszCExisting);
    if (0xFFFFFFFF == dwFileAttributes) {
        // This shouldn't happen
        ERRORMSG(1, (_T(
            "CopyFileEx: Failed to get attributes of open file %s\r\n"
            ), lpszCExisting));
        VERIFY(CloseHandle(hExistingFile));
        return FALSE;
    }
    else {
        // Remove ROM flag to/and make sure the destination file can be written to
        dwFileAttributes &= ~(FILE_ATTRIBUTE_INROM);
        if (dwFileAttributes & FILE_ATTRIBUTE_READONLY) dwFlags |= COPYFILEEX_SRC_READ_ONLY;
        dwFileAttributes = dwFileAttributes & ~FILE_ATTRIBUTE_READONLY;
    }

    if (!(dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED)) {
        // Determine whether both files are in the RAM file system (ObjectStore).
        InfoLevel = CeVolumeInfoLevelStandard;
        if (CeFsIoControlW(lpszCExisting, FSCTL_GET_VOLUME_INFO, &InfoLevel, sizeof(InfoLevel), 
                &VolumeInfo, sizeof(VolumeInfo), &dwBytesRead, NULL) &&
            (VolumeInfo.dwFlags & CE_VOLUME_FLAG_RAMFS) &&
            CeFsIoControlW(lpszCNew, FSCTL_GET_VOLUME_INFO, &InfoLevel, sizeof(InfoLevel),
                &VolumeInfo, sizeof(VolumeInfo), &dwBytesRead, NULL) &&
            (VolumeInfo.dwFlags & CE_VOLUME_FLAG_RAMFS)) {
            // Both files are in the RAM file system. The RANDOM_ACCESS flag implies
            // that the new file should not be compressed.
            dwFileAttributes |= FILE_FLAG_RANDOM_ACCESS;
        }
    }
    
    // If destination file exists and is hidden or read-only and not in ROM, fail
    dwDestAttributes = GetFileAttributes(lpszCNew);
    if ((dwDestAttributes != INVALID_FILE_ATTRIBUTES) && 
        (dwDestAttributes & (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_READONLY) &&
        !(dwDestAttributes & FILE_ATTRIBUTE_INROM))
    ) {
        SetLastError(ERROR_ACCESS_DENIED);
        VERIFY (CloseHandle(hExistingFile));
        return FALSE;
    }

    // Create the destination file.
    hNewFile = CreateFile(
        lpszCNew,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        0,
        (COPYFILEEX_FAIL_IF_DST_EXISTS & dwFlags) ? CREATE_NEW : CREATE_ALWAYS,
        dwFileAttributes | FILE_FLAG_NO_BUFFERING,
        0);
    if (INVALID_HANDLE_VALUE == hNewFile) {
        VERIFY(CloseHandle(hExistingFile));
        return FALSE;
    }

    // Per the CreateFile specification, ERROR_ALREADY_EXISTS indicates that the
    // file already existed even on success when CREATE_ALWAYS was specified.
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Destination file exists, so only delete destination file if copy
        // cancelled through CopyProgressRoutine, as per CopyFileEx specification
        dwFlags &= ~(COPYFILEEX_DELETE_DST);
    }

    // Allocate copy chunk
    lpbCopyChunk = VirtualAlloc(0, FS_COPYBLOCK_SIZE, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    if (!lpbCopyChunk) {
        SetLastError(ERROR_OUTOFMEMORY);
        goto cleanUp;
    }

    // Get size of source file
    liExistingFileSize.LowPart = GetFileSize(hExistingFile, (PDWORD)&liExistingFileSize.HighPart);
    if (0xFFFFFFFF == liExistingFileSize.LowPart) {
        // 0xFFFFFFFF is actually a valid file size for 64-bit files, so confirm with GetLastError.
        if (NO_ERROR != GetLastError()) goto cleanUp;
    }

    // Expand destination file
    if (0xFFFFFFFF == SetFilePointer(
        hNewFile,
        (LONG) liExistingFileSize.LowPart,
        &liExistingFileSize.HighPart,
        FILE_BEGIN
    )) {
        // 0xFFFFFFFF is actually a valid file offset for 64-bit files, so confirm with GetLastError.
        if (NO_ERROR != GetLastError()) goto cleanUp;
    }
    // NT pre-allocates the destination file.  If this fails, then behavior
    // deviates from NT, but correctness is preserved.
    SetEndOfFile(hNewFile);
    // Reset destination file pointer
    if (0xFFFFFFFF == SetFilePointer(hNewFile, 0, NULL, FILE_BEGIN)) {
        goto cleanUp;
    }

    // NT will call CopyProgressRoutine for initial stream switch regardless
    // of the value of *pbCancel
    if (lpProgressRoutine) {
        liTotalBytesTransferred.QuadPart = 0;
        // File streams not supported; Always report stream 1
        dwCopyProgressResult = (*lpProgressRoutine)(
            liExistingFileSize,
            liTotalBytesTransferred,
            liExistingFileSize,
            liTotalBytesTransferred,
            1,
            CALLBACK_STREAM_SWITCH,
            hExistingFile,
            hNewFile,
            lpData);
        switch (dwCopyProgressResult) {
        case PROGRESS_CONTINUE:
            break;
        case PROGRESS_CANCEL:
            dwFlags |= COPYFILEEX_DELETE_DST;
            SetLastError(ERROR_REQUEST_ABORTED);
            goto cleanUp;
        case PROGRESS_STOP:
            SetLastError(ERROR_REQUEST_ABORTED);
            goto cleanUp;
        case PROGRESS_QUIET:
            dwFlags |= COPYFILEEX_QUIET;
            break;
        default:
            ERRORMSG(1,(_T(
                "CopyFileEx: Error: Unknown CopyProgressRoutine result\r\n"
                )));
            goto cleanUp;
        }
    }

    // Perform copy
    while (1) {
        // Has the copy been cancelled?
        if (pbCancel && *pbCancel) {
            // Although the CopyFileEx specification is unclear as to whether
            // the destination file is deleted when *pbCancel is TRUE, NT
            // deletes the destination file when *pbCancel is TRUE, regardless
            // of whether the file existed before CopyFileEx was invoked
            dwFlags |= COPYFILEEX_DELETE_DST;
            goto cleanUp;
        }
        if (!ReadFile(hExistingFile, lpbCopyChunk, FS_COPYBLOCK_SIZE, &dwBytesRead, 0)) {
            goto cleanUp;
        }
        else if (
            dwBytesRead &&
            (!WriteFile(hNewFile, lpbCopyChunk, dwBytesRead, &dwBytesWritten, 0) ||
            (dwBytesWritten != dwBytesRead)
        )) {
            goto cleanUp;
        }

        // Is copy complete?
        if (dwBytesRead == 0) break;

        // Update copy progress
        liTotalBytesTransferred.QuadPart += (LONGLONG) dwBytesWritten;

        // Call CopyProgressRoutine
        if (!(dwFlags & COPYFILEEX_QUIET) && lpProgressRoutine) {
            // File streams not supported; Always report stream 1
            dwCopyProgressResult = (*lpProgressRoutine)(
                liExistingFileSize,
                liTotalBytesTransferred,
                liExistingFileSize,
                liTotalBytesTransferred,
                1,
                CALLBACK_CHUNK_FINISHED,
                hExistingFile,
                hNewFile,
                lpData);
            switch (dwCopyProgressResult) {
            case PROGRESS_CONTINUE:
                break;
            case PROGRESS_CANCEL:
                dwFlags |= COPYFILEEX_DELETE_DST;
                SetLastError(ERROR_REQUEST_ABORTED);
                goto cleanUp;
            case PROGRESS_STOP:
                SetLastError(ERROR_REQUEST_ABORTED);
                goto cleanUp;
            case PROGRESS_QUIET:
                dwFlags |= COPYFILEEX_QUIET;
                break;
            default:
                ERRORMSG(1,(_T(
                    "CopyFileEx: Error: Unknown CopyProgressRoutine result\r\n"
                    )));
                goto cleanUp;
            }
        }
    }

    // The file was copied successfully
    dwFlags |= COPYFILEEX_RET;         // Mark success
    dwFlags &= ~COPYFILEEX_DELETE_DST; // Unmark deletion of destination file

cleanUp:;

    // Preserve LastError
    dwError = GetLastError();

    VirtualFree(lpbCopyChunk, 0, MEM_RELEASE);

    if (dwFlags & COPYFILEEX_RET) {
        // Set destination file time
        GetFileTime(hExistingFile, &ftCreation, &ftLastAccess, &ftLastWrite);
        SetFileTime(hNewFile, &ftCreation, &ftLastAccess, &ftLastWrite);
    }
    
    VERIFY(CloseHandle(hExistingFile));
    VERIFY(CloseHandle(hNewFile));
    
    if ((dwFlags & COPYFILEEX_RET) &&
        (dwFlags & COPYFILEEX_SRC_READ_ONLY)) {
        // If source file read-only, make destination file read-only
        SetFileAttributes(lpszCNew, dwFileAttributes|FILE_ATTRIBUTE_READONLY);
    }

    // DeleteFile deletes a file if no open handles exist
    if (dwFlags & COPYFILEEX_DELETE_DST) {
        DeleteFile(lpszCNew);
    }

    // Set back the error code which may have occured before cleanup 
    if (dwError) {
        SetLastError(dwError);
    }

    return (dwFlags & COPYFILEEX_RET) ? TRUE : FALSE;
}

BOOL 
CopyFileW (
    LPCWSTR lpszExistingFileName,
    LPCWSTR lpszNewFileName,
    BOOL bFailIfExists
    )
{
    DWORD dwCopyFlags = bFailIfExists ? COPYFILEEX_FAIL_IF_DST_EXISTS : 0;
    return CopyFileEx (lpszExistingFileName, lpszNewFileName, NULL, NULL, NULL, dwCopyFlags);
}

BOOL GetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation) {

    LPCWSTR pTrav;
    HANDLE hFind;
    WIN32_FIND_DATA w32fd;

    for (pTrav = lpFileName; *pTrav; pTrav++) {
        if (*pTrav == '*' || *pTrav == '?') {
            SetLastError(ERROR_INVALID_NAME);
            return FALSE;
        }
    }
    if (fInfoLevelId != GetFileExInfoStandard) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if ((hFind = FindFirstFile(lpFileName,&w32fd)) == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }
    CloseHandle(hFind);
    ((WIN32_FILE_ATTRIBUTE_DATA *)lpFileInformation)->dwFileAttributes = w32fd.dwFileAttributes;
    ((WIN32_FILE_ATTRIBUTE_DATA *)lpFileInformation)->ftCreationTime = w32fd.ftCreationTime;
    ((WIN32_FILE_ATTRIBUTE_DATA *)lpFileInformation)->ftLastAccessTime = w32fd.ftLastAccessTime;
    ((WIN32_FILE_ATTRIBUTE_DATA *)lpFileInformation)->ftLastWriteTime = w32fd.ftLastWriteTime;
    ((WIN32_FILE_ATTRIBUTE_DATA *)lpFileInformation)->nFileSizeHigh = w32fd.nFileSizeHigh;
    ((WIN32_FILE_ATTRIBUTE_DATA *)lpFileInformation)->nFileSizeLow = w32fd.nFileSizeLow;
    return TRUE;
}

LPVOID WINAPI CeZeroPointer (LPVOID ptr)
{
    // deprecated
    DEBUGCHK(!DBGDEPRE);
    return ptr;
}

// Determine whether <pchTarget> matches <pchWildcardMask>; '?' and '*' are
// valid wildcards.
// The '*' quantifier is a 0 to <infinity> character wildcard.
// The '?' quantifier is a 0 or 1 character wildcard.
BOOL
MatchesWildcardMask(
    DWORD lenWildcardMask,
    PCWSTR pchWildcardMask,
    DWORD lenTarget,
    PCWSTR pchTarget
    )
{
    while (lenWildcardMask && lenTarget) {
        if (*pchWildcardMask == L'?') {
            // skip current target character
            lenTarget--;
            pchTarget++;
            lenWildcardMask--;
            pchWildcardMask++;
            continue;
        }
        if (*pchWildcardMask == L'*') {
            pchWildcardMask++;
            if (--lenWildcardMask) {
                while (lenTarget) {
                    if (MatchesWildcardMask(lenWildcardMask, pchWildcardMask, lenTarget--, pchTarget++)) {
                        return TRUE;
                    }
                }
                return FALSE;
            }
            return TRUE;
        }
        // test for case-insensitive equality
        else if (_wcsnicmp(pchWildcardMask, pchTarget, 1)) {
            return FALSE;
        }
        lenWildcardMask--;
        pchWildcardMask++;
        lenTarget--;
        pchTarget++;
    }
    // target matches wildcard mask, succeed
    if (!lenWildcardMask && !lenTarget) {
        return TRUE;
    }
    // wildcard mask has been spent and target has characters remaining, fail
    if (!lenWildcardMask) {
        return FALSE;
    }
    // target has been spent; succeed only if wildcard characters remain
    while (lenWildcardMask--) {
        if (*pchWildcardMask != L'*' && *pchWildcardMask != L'?') {
            return FALSE;
        }
        pchWildcardMask++;
    }
    return TRUE;
}


static  BOOL    s_ForcePixelDoubling;
static  BOOL    s_ForcePixelDoublingIsSet;
static  BOOL    s_ForcePixelDoublingIsChecked;


DWORD
WINAPI
ForcePixelDoubling(
        BOOL    torf
        )
{
        if ( s_ForcePixelDoublingIsSet ||
                 s_ForcePixelDoublingIsChecked )
                {
                //      OK if setting the same value.
                if ( s_ForcePixelDoubling == torf )
                        {
                        return ERROR_SUCCESS;
                        }

                //      Too late
                if ( s_ForcePixelDoublingIsChecked )
                        {
                        return ERROR_ALREADY_REGISTERED;
                        }
                //      Different value
                return ERROR_ALREADY_INITIALIZED;
                }

        s_ForcePixelDoubling = torf;
        s_ForcePixelDoublingIsSet = TRUE;
        return ERROR_SUCCESS;
}

int
WINAPI
IsForcePixelDoubling(
        void
        )
{
        if (  GetCurrentProcessId()!=(DWORD)GetOwnerProcess() )
                {
                int     IsDouble = -1;

                if( g_UsrCoredllCallbackArray && g_UsrCoredllCallbackArray[USRCOREDLLCB_IsForcePixelDoubling] )
                    {                        
                    CALLBACKINFO    cbi;
                    cbi.hProc       = GetOwnerProcess();
                    cbi.pfn         = (FARPROC) g_UsrCoredllCallbackArray[USRCOREDLLCB_IsForcePixelDoubling];
                    __try
                            {
                            IsDouble = PerformCallBack(&cbi);
                            }
                    __except (EXCEPTION_EXECUTE_HANDLER)
                            {
                            }
                    }
                
                return IsDouble;
                }

        s_ForcePixelDoublingIsChecked = TRUE;
        if ( !s_ForcePixelDoublingIsSet )
                {
                return -1;
                }
        return s_ForcePixelDoubling;
}


/* access to KData */
DWORD __GetUserKData (DWORD dwOfst)
{
    return *(LPDWORD) (PUserKData+dwOfst);
}


