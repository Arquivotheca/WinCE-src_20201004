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
#include "storeincludes.hpp"

void FileSystem_t::LoadSettings ()
{    
    LRESULT lResult;
    lResult = m_pDisk->GetRegistryFlag(L"Paging", &m_FSDFlags, FSD_FLAGS_PAGEABLE);
    if (ERROR_SUCCESS != lResult) {
        // Paging supported by default unless explicitly disabled.
        m_FSDFlags |= FSD_FLAGS_PAGEABLE;
    }
    lResult = m_pDisk->GetRegistryFlag(L"PowerNotify", &m_FSDFlags, FSD_FLAGS_POWER_NOTIFY);
    if (ERROR_SUCCESS != lResult) {
        // Power notifications are supported by default unless explicitly disabled.
        m_FSDFlags |= FSD_FLAGS_POWER_NOTIFY;
    }
    lResult = m_pDisk->GetRegistryFlag(L"FsctlTrustRequired", &m_FSDFlags, FSD_FLAGS_FSCTL_TRUST_REQUIRED);
    if (ERROR_SUCCESS != lResult) {
        // Trust required by default.
        m_FSDFlags |= FSD_FLAGS_FSCTL_TRUST_REQUIRED;
    }
    lResult = m_pDisk->GetRegistryFlag(L"LockIOBuffers", &m_FSDFlags, FSD_FLAGS_LOCK_IO_BUFFERS);
    if (ERROR_SUCCESS != lResult) {
        // Buffer locking enabled by default.
        m_FSDFlags |= FSD_FLAGS_LOCK_IO_BUFFERS;
    }
    lResult = m_pDisk->GetRegistryValue(L"LowStorageThreshold", &m_dwFreeSpaceThreshold);
    if (ERROR_SUCCESS != lResult) {
        // Low Storage notification will not fire
        m_dwFreeSpaceThreshold = 0;
    } else {
        if (m_dwFreeSpaceThreshold > 90) {
            RETAILMSG(1,(L"LowStorageThreshold key is misconfigured as %d should be lower than 90",
                m_dwFreeSpaceThreshold));
            m_dwFreeSpaceThreshold = 10;
        }
    }
}

void FileSystem_t::GetMountName(__out_ecount(elem) WCHAR *pMountName, DWORD elem)
{
    ASSERT(pMountName);
    ASSERT(m_pDisk);
    ASSERT(m_pDisk->GetVolume());

    if (!pMountName) return;
    *pMountName = 0 ;

    if (!m_pDisk) return;

    MountedVolume_t *pVol = m_pDisk->GetVolume();        
    if (!pVol) return;

    g_pMountTable->GetMountName(pVol->GetMountIndex(),pMountName,elem);
}


#ifdef UNDER_CE
LRESULT FileSystem_t::OpenSGArray (FILE_SEGMENT_ELEMENT** ppSegmentArrayLocal,
        FILE_SEGMENT_ELEMENT* pSegmentArray, DWORD BytesToTransfer, 
        BOOL fWrite, HANDLE hProc) 
{
    if ((0 == BytesToTransfer) || 
        (NULL == pSegmentArray) ||
        (0 != (BytesToTransfer % UserKInfo[KINX_PAGESIZE]))
        ) {
        return ERROR_INVALID_PARAMETER;
    }

    // Determine the number of segments we need to open and/or lock.
    DWORD SegmentCount = BytesToTransfer / UserKInfo[KINX_PAGESIZE];

    FILE_SEGMENT_ELEMENT* pSegmentArrayLocal = NULL;
    
    if (hProc != reinterpret_cast<HANDLE> (::GetCurrentProcessId ())) {
        
        // Caller is a user process, make a local copy of the segment array.
        pSegmentArrayLocal = new FILE_SEGMENT_ELEMENT[SegmentCount];
        if (!pSegmentArrayLocal) {
            return ERROR_OUTOFMEMORY;
        }

        // Initialize the local segment array to zero.
        ::memset (pSegmentArrayLocal, 0, sizeof (FILE_SEGMENT_ELEMENT) * SegmentCount);

        for (DWORD i = 0; i < SegmentCount; i ++) 
        {
            // Verify access to each embedded buffer.
            HRESULT hr = E_FAIL;
            __try {
                hr = ::CeOpenCallerBuffer (
                            &pSegmentArrayLocal[i].Buffer,
                            pSegmentArray[i].Buffer,
                            UserKInfo[KINX_PAGESIZE],
                            fWrite ? ARG_I_PTR : ARG_O_PTR,
                            FALSE
                            );
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                // Exception while accessing pSegmentArray[i].Buffer.
                hr = E_FAIL;
            }
            
            if (FAILED (hr))
            {
                // Failed opening a buffer, cleanup any already opened buffers
                // and exit.
                CloseSGArray (
                        pSegmentArrayLocal, 
                        pSegmentArray, 
                        // NOTE: We only cleanup already opened buffers:
                        (i * UserKInfo[KINX_PAGESIZE]), 
                        fWrite
                        );
                
                return ERROR_INVALID_PARAMETER;
            }

            if (FSD_FLAGS_LOCK_IO_BUFFERS & m_FSDFlags) {

                // Lock the buffer if required.

                BOOL fResult = FALSE;
                __try {
                    fResult = ::LockPages (
                            pSegmentArrayLocal[i].Buffer,
                            UserKInfo[KINX_PAGESIZE],
                            NULL,
                            fWrite ? LOCKFLAG_READ : LOCKFLAG_WRITE
                            );
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    // Exception accessing pSegmentArrayLocal[i].Buffer.
                    fResult = FALSE;
                }
                    
                if (!fResult)
                {
                    // Failed locking the buffer, cleanup any already opened
                    // and locked buffers and exit.
                    LRESULT lResult = ::FsdGetLastError ();
                    CloseSGArray (
                            pSegmentArrayLocal,
                            pSegmentArray,
                            // NOTE: We only cleanup already opened/locked buffers:
                            (i * UserKInfo[KINX_PAGESIZE]), 
                            fWrite
                            );
                    
                    return lResult;
                }
            }
        }

    } else {

        // Caller is the kernel process, use input segment array and don't
        // make a local heap copy.
        pSegmentArrayLocal = pSegmentArray;

        if (FSD_FLAGS_LOCK_IO_BUFFERS & m_FSDFlags) {

            // Lock embedded buffer pointers for appropriate read/write access.
                
            for (DWORD i = 0; i < SegmentCount; i ++)
            {
                BOOL fResult = FALSE;
                __try {
                    fResult = ::LockPages (
                            pSegmentArrayLocal[i].Buffer,
                            UserKInfo[KINX_PAGESIZE],
                            NULL,
                            fWrite ? LOCKFLAG_READ : LOCKFLAG_WRITE
                            );
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    // Exception accessing pSegmentArrayLocal[i].Buffer.
                    fResult = FALSE;
                }
                    
                if (!fResult)
                {
                    LRESULT lResult = ::FsdGetLastError ();
                    CloseSGArray (
                            pSegmentArrayLocal,
                            pSegmentArray,
                            // NOTE: We only cleanup already locked buffers:
                            (i * UserKInfo[KINX_PAGESIZE]), 
                            fWrite
                            );
                    
                    return lResult;
                }
            }
        }
    }

    // Output the local copy of the segment array. This could be a heap copy
    // or the same as the input depending on the caller's identity.
    *ppSegmentArrayLocal = pSegmentArrayLocal;

    return ERROR_SUCCESS;
}

void FileSystem_t::CloseSGArray (FILE_SEGMENT_ELEMENT* pSegmentArrayLocal,
        FILE_SEGMENT_ELEMENT* pSegmentArray, DWORD BytesToTransfer, 
        BOOL fWrite)
{
    // Determine the number of segments we need to close and/or unlock.
    DWORD SegmentCount = BytesToTransfer / UserKInfo[KINX_PAGESIZE];

    if (FSD_FLAGS_LOCK_IO_BUFFERS & m_FSDFlags) {
        
        // Unlock embedded buffer pointers.

        for (DWORD i = 0; i < SegmentCount; i ++) 
        {
            __try {
                ::UnlockPages (
                        pSegmentArrayLocal[i].Buffer,
                        UserKInfo[KINX_PAGESIZE]
                        );

            } __except (EXCEPTION_EXECUTE_HANDLER) {
                // Exception accessing pSegmentArrayLocal[i].Buffer.
            }
        }
    }

    if (pSegmentArrayLocal != pSegmentArray) {

        // Close all opened buffers.
        
        for (DWORD i = 0; i < SegmentCount; i ++)
        {
            __try {
                ::CeCloseCallerBuffer (
                        pSegmentArrayLocal[i].Buffer,
                        pSegmentArray[i].Buffer,
                        UserKInfo[KINX_PAGESIZE],
                        fWrite ? ARG_I_PTR : ARG_O_PTR
                        );
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                // Exception accessing pSegmentArray[i].Buffer.
            }
        }

        // Free the local heap copy of the segment array (allocated
        // by OpenSGArray).
        delete[] pSegmentArrayLocal;
    }
}

#else

LRESULT FileSystem_t::OpenSGArray (FILE_SEGMENT_ELEMENT** ppSegmentArrayLocal,
        FILE_SEGMENT_ELEMENT* pSegmentArray, DWORD BytesToTransfer, 
        BOOL fWrite, HANDLE hProc) 
{
    return ERROR_SUCCESS;
}

void FileSystem_t::CloseSGArray (FILE_SEGMENT_ELEMENT* pSegmentArrayLocal,
        FILE_SEGMENT_ELEMENT* pSegmentArray, DWORD BytesToTransfer, 
        BOOL fWrite)
{
    ;
}

#endif // UNDER_CE

FileSystemDriver_t::FileSystemDriver_t (MountableDisk_t* pDisk, const WCHAR* pFSDName) :
    FileSystem_t (pDisk),
    m_hFSDDll (NULL),
    m_pfnFormatVolume (NULL),
    m_pfnMountDisk (NULL),
    m_pfnUnmountDisk (NULL),
    m_pFSDName (NULL)
{
    m_ThisFileSystemType = FileSystemDriver_Type;
    ::FsdDuplicateString (&m_pFSDName, pFSDName, MAX_PATH);
}

FileSystemDriver_t::~FileSystemDriver_t ()
{
    DEBUGMSG(ZONE_INIT, (L"FSDMGR!FileSystemDriver_t::~FileSystemDriver_t: deleting FSD (%p)", this));
    if (m_pFSDName) {
        delete[] m_pFSDName;
    }
    
    if (m_hFSDDll) {
        ::FreeLibrary (m_hFSDDll);
        m_hFSDDll = NULL;
    }
}

const FileSystemDriver_t::ExportNameStubPointerPair 
    FileSystemDriver_t::FilterHookFunctions[] = {
    { L"CloseVolume", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"CreateDirectoryW", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"RemoveDirectoryW", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"GetFileAttributesW", (PFNAPI)FileSystem_t::FSStub_FileAttributes },
    { L"SetFileAttributesW", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"DeleteFileW", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"MoveFileW", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"DeleteAndRenameFileW", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"GetDiskFreeSpaceW", (PFNAPI)FileSystem_t::FSStub_Bool},
    { L"Notify", (PFNAPI)FileSystem_t::FSStub_Void },
    { L"RegisterFileSystemFunction", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"FindFirstFileW", (PFNAPI)FileSystem_t::FSStub_Handle },
    { L"FindNextFileW", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"FindClose", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"CreateFileW", (PFNAPI)FileSystem_t::FSStub_Handle },
    { L"ReadFile", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"ReadFileWithSeek", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"WriteFile", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"WriteFileWithSeek", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"SetFilePointer", (PFNAPI)FileSystem_t::FSStub_FilePointer },
    { L"GetFileSize", (PFNAPI)FileSystem_t::FSStub_FileSize },
    { L"GetFileInformationByHandle", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"FlushFileBuffers", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"GetFileTime", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"SetFileTime", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"SetEndOfFile", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"DeviceIoControl", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"CloseFile", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"RefreshVolume", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"FsIoControl", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"LockFileEx", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"UnlockFileEx", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"GetVolumeInfo", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"ReadFileScatter", (PFNAPI)FileSystem_t::FSStub_Bool },
    { L"WriteFileGather", (PFNAPI)FileSystem_t::FSStub_Bool }
};

LRESULT FileSystemDriver_t::PopulateFilterHook (HMODULE hDll, const WCHAR* pPrefix, 
    FILTERHOOK* pFilterHook)
{
    DEBUGCHK (sizeof (FILTERHOOK) == pFilterHook->cbSize);

    size_t PrefixLength;
    WCHAR ExportName[MAX_PATH];    
    if (FAILED (::StringCchCopyW (ExportName, MAX_PATH, pPrefix)) ||
        FAILED (::StringCchLengthW (ExportName, MAX_PATH, &PrefixLength))) {
        return ERROR_INVALID_PARAMETER;
    }

    WCHAR* pFunctionName = ExportName + PrefixLength;
    size_t FunctionLength = MAX_PATH - PrefixLength;
    
    PFNAPI* ppFunction = (PFNAPI*)&pFilterHook->pCloseVolume;
    DWORD FunctionCount = (sizeof (FilterHookFunctions) / sizeof (FilterHookFunctions[0]));
    for (DWORD i = 0; i < FunctionCount; i ++) {

        const WCHAR* pBaseName = FilterHookFunctions[i].pName;

        if (FAILED (::StringCchCopyW (pFunctionName, FunctionLength, FilterHookFunctions[i].pName))) {
            return ERROR_INVALID_PARAMETER;
        }

        // First try the prefixed name.
        *ppFunction  = (PFNAPI)::FsdGetProcAddress (hDll, ExportName);
        if (!*ppFunction) {
            // Next try the unprefixed, bare name.
            *ppFunction = (PFNAPI)::FsdGetProcAddress (hDll, pBaseName);
            if (!*ppFunction) {
                // No export was found, use the stub function.
                *ppFunction = FilterHookFunctions[i].pStubFunction;
            }
        }
        ppFunction ++;
    }

    return ERROR_SUCCESS;
}

LRESULT FileSystemDriver_t::GetExportPrefix (HMODULE hDll, __out_ecount(PrefixChars) WCHAR* pPrefix, size_t PrefixChars)
{
    size_t ModuleNameChars;

    if (PrefixChars < 3) {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    if (::FsdGetProcAddress(hDll, L"FSD_CreateFileW")) {
        
        // This FSD uses simply FSD_ as a function prefix.
        StringCchCopyW (pPrefix, PrefixChars, L"FSD_");

    } else {

        // Try to determine the prefix based on the module name. Note
        // that there are case sensitivity issues here: GetProcAddress
        // is case sensitive, while the name of the module is not, so
        // this is a common source of problems.
    
        ModuleNameChars = ::GetModuleFileName(hDll, pPrefix, PrefixChars);
        if (ModuleNameChars) {

            // Walk the string backwards until we find a slash.
            const WCHAR* pTmp = &pPrefix[ModuleNameChars];
            while (pTmp != pPrefix) {
                pTmp --;
                if (*pTmp == L'\\' || *pTmp == L'/') {
                    pTmp++;
                    break;
                }
            }

            // Now copy forward from the slash until we reach a dot.
            // Make sure there is at least room for '_' and final NULL char.
            size_t i = 0;
            while (*pTmp && *pTmp != '.' && i < PrefixChars - 2) {
                pPrefix[i++] = *pTmp++;
            }
        
            pPrefix[i]   = L'_';
            pPrefix[i+1] = 0;            
            
        } else {
        
            return FsdGetLastError ();
        }
    }

    DEBUGMSG (ZONE_VERBOSE, (L"FSDMGR!FileSystemDriver_t::GetExportPrefix: Using prefix \"%s\" for FSD", pPrefix));
    
    return ERROR_SUCCESS;
}

LRESULT FileSystemDriver_t::Load ()
{
    // Load settings specific to this FSD instance.
    FileSystem_t::LoadSettings ();
    
    // Load the specified FSD dll.
    m_hFSDDll = ::LoadDriver (m_pFSDName);
    if (!m_hFSDDll) {
        return FsdGetLastError (ERROR_MOD_NOT_FOUND);
    }

    // Determine the FSD prefix.
    WCHAR ExportPrefix[MAX_PATH];
    LRESULT lResult = GetExportPrefix (m_hFSDDll, ExportPrefix, MAX_PATH);
    if (ERROR_SUCCESS != lResult) {
        return lResult;
    }    

    // The FSD *must* have FSD_MountDisk and FSD_UnmountDisk exports.
    m_pfnMountDisk = (PFNMOUNTDISK)::FsdGetProcAddress (m_hFSDDll, L"FSD_MountDisk");
    m_pfnUnmountDisk = (PFNUNMOUNTDISK)::FsdGetProcAddress (m_hFSDDll, L"FSD_UnmountDisk");
    if (!m_pfnMountDisk || !m_pfnUnmountDisk) {
        DEBUGMSG (ZONE_ERRORS, (L"FSDMGR!FileSystemDriver_t::Load: FSD is missing MountDisk or UnmountDisk export\r\n"));
        ::FreeLibrary (m_hFSDDll);
        return FsdGetLastError (ERROR_PROC_NOT_FOUND);
    }

    lResult = PopulateFilterHook (m_hFSDDll, ExportPrefix, &m_FilterHook);
    if (ERROR_SUCCESS != lResult) {
        return lResult;
    }

    m_pfnFormatVolume = (PFNFORMATVOLUME)::FsdGetProcAddress (m_hFSDDll, L"FSD_FormatVolume");
    if (!m_pfnFormatVolume) {        
        // This FSD does not provide a FSD_FormatVolume export.
        PREFAST_SUPPRESS(25053, "Parameter count mismatch is acceptable since this is a stub function and is cdecl");
        m_pfnFormatVolume = reinterpret_cast<PFNFORMATVOLUME> (FSStub_LResult);
    }
    
    return ERROR_SUCCESS;
}

LRESULT FileSystemDriver_t::Unload ()
{
    // Unmount this FSD.
    if (m_FilterHook.hVolume) {
        if (Unmount ()) {
            CloseVolume ();
        }
    }
    
    // Free the library associate with this FSD.
    if (m_hFSDDll) {
        ::FreeLibrary (m_hFSDDll);
        m_hFSDDll = NULL;
    }
    return ERROR_SUCCESS;
}


FileSystemFilterDriver_t::FileSystemFilterDriver_t (MountableDisk_t* pDisk, const WCHAR* pFilterName) :    
    FileSystem_t (pDisk),
    m_hFilterDll (NULL),
    m_pfnHookVolume(NULL),
    m_pfnUnhookVolume(NULL),
    m_pNextFileSystem (NULL),
    m_pFilterName (NULL)
{
    m_ThisFileSystemType = FileSystemFilterDriver_Type;
    ::FsdDuplicateString (&m_pFilterName, pFilterName, MAX_PATH);
}

FileSystemFilterDriver_t::~FileSystemFilterDriver_t ()
{
    DEBUGMSG(ZONE_INIT, (L"FSDMGR!FileSystemFilterDriver_t::~FileSystemFilterDriver_t: deleting filter (%p)", this));

    if (m_pFilterName) {
        delete[] m_pFilterName;
    }
    
    if (m_hFilterDll) {
        ::FreeLibrary (m_hFilterDll);
        m_hFilterDll = NULL;
    }
    
    // Each filter must clean-up its own MountableDisk_t object. This will
    // always be a NullDisk_t object. Nobody else has a reference to this
    // disk object, it is owned entirely by the filter.
    DEBUGCHK (NULL != m_pDisk);
    delete m_pDisk;
    m_pDisk = NULL;
}

void FileSystemFilterDriver_t::LoadSettings ()
{    
    LRESULT lResult;

    // The filter must be hooked before we load the settings.
    DEBUGCHK (m_pNextFileSystem);

    DWORD NextFSDFlags = m_pNextFileSystem->GetFSDFlags ();

    // If any settings are not explicitly set for this filter, just use the
    // setting from the file system immediately below us in the chain.
    lResult = m_pDisk->GetRegistryFlag(L"Paging", &m_FSDFlags, FSD_FLAGS_PAGEABLE);
    if (ERROR_SUCCESS != lResult) {
        m_FSDFlags |= (FSD_FLAGS_PAGEABLE & NextFSDFlags);
    }
    lResult = m_pDisk->GetRegistryFlag(L"PowerNotify", &m_FSDFlags, FSD_FLAGS_POWER_NOTIFY);
    if (ERROR_SUCCESS != lResult) {
        m_FSDFlags |= (FSD_FLAGS_POWER_NOTIFY & NextFSDFlags);
    }
    lResult = m_pDisk->GetRegistryFlag(L"FsctlTrustRequired", &m_FSDFlags, FSD_FLAGS_FSCTL_TRUST_REQUIRED);
    if (ERROR_SUCCESS != lResult) {
        m_FSDFlags |= (FSD_FLAGS_FSCTL_TRUST_REQUIRED & NextFSDFlags);
    }
    lResult = m_pDisk->GetRegistryFlag(L"LockIOBuffers", &m_FSDFlags, FSD_FLAGS_LOCK_IO_BUFFERS);
    if (ERROR_SUCCESS != lResult) {
        m_FSDFlags |= (FSD_FLAGS_LOCK_IO_BUFFERS & NextFSDFlags);
    }

    lResult = m_pDisk->GetRegistryValue(L"LowStorageThreshold", &m_dwFreeSpaceThreshold);
    if (ERROR_SUCCESS != lResult) {
        // Low Storage notification will not fire
        m_dwFreeSpaceThreshold = 0;
    } else {
        if (m_dwFreeSpaceThreshold > 90) {
            RETAILMSG(1,(L"LowStorageThreshold key is misconfigured as %d should be lower than 90",
                m_dwFreeSpaceThreshold));
            m_dwFreeSpaceThreshold = 10;
        }
    }

    DWORD dwNextFreeSpaceThreshold = m_pNextFileSystem->GetFreeSpaceThreshold();
    if (dwNextFreeSpaceThreshold > m_dwFreeSpaceThreshold) {
        m_dwFreeSpaceThreshold = dwNextFreeSpaceThreshold;
    }
}


LRESULT FileSystemFilterDriver_t::Load ()
{
    if (!m_pFilterName) {
        // The filter name failed to be copied in the constructor.
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    
    // Load the specified filter dll.
    m_hFilterDll = ::LoadDriver (m_pFilterName);
    if (!m_hFilterDll) {
        return FsdGetLastError (ERROR_MOD_NOT_FOUND);
    }

    // Get the HookVolume and UnhookVolume exports from the filter dll.
    m_pfnHookVolume = reinterpret_cast<PFNHOOKVOLUME> (::FsdGetProcAddress (m_hFilterDll, L"FILTER_HookVolume"));
    if (!m_pfnHookVolume) {
        // Try the bare export if the FILTER_ export doesn't exist.
        m_pfnHookVolume = reinterpret_cast<PFNHOOKVOLUME> (::FsdGetProcAddress (m_hFilterDll, L"HookVolume"));
    }
    m_pfnUnhookVolume = reinterpret_cast<PFNUNHOOKVOLUME> (::FsdGetProcAddress (m_hFilterDll, L"FILTER_UnhookVolume"));
    if (!m_pfnUnhookVolume) {
        // Try the bare export if the FILTER_ export doesn't exist.
        m_pfnUnhookVolume = reinterpret_cast<PFNUNHOOKVOLUME> (::FsdGetProcAddress (m_hFilterDll, L"UnhookVolume"));
    }

    // The filter *must* have HookVolume and UnhookVolume exports or it cannot be loaded.
    if (!m_pfnHookVolume || !m_pfnUnhookVolume) {
        DEBUGMSG (ZONE_ERRORS, (L"FSDMGR!FileSystemFilterDriver_t::Load: Filter is missing HookVolume or UnhookVolume export\r\n"));
        ::FreeLibrary (m_hFilterDll);
        return FsdGetLastError (ERROR_PROC_NOT_FOUND);
    }

    // Populate the function pointers in the filter hook with the standard file system
    // driver exports using the FILTER prefix.    
    LRESULT lResult;
    lResult = FileSystemDriver_t::PopulateFilterHook (m_hFilterDll, L"FILTER_", &m_FilterHook);
    if (ERROR_SUCCESS != lResult) {
        return lResult;
    }

    return ERROR_SUCCESS;
}

LRESULT FileSystemFilterDriver_t::Unload ()
{
    // Unhook this filter.
    if (m_FilterHook.hVolume) {
        if (Unhook ()) {
            CloseVolume ();
        }
    }

    // Free the library associate with this filter.
    if (m_hFilterDll) {
        ::FreeLibrary (m_hFilterDll);
        m_hFilterDll = NULL;
    }
    
    if (m_pNextFileSystem) {
        // Each filter is responsible for unloading any filters below it.
        LRESULT lResult = m_pNextFileSystem->Unload ();
        if (ERROR_SUCCESS != lResult) {
            return lResult;
        }
        
        // Each filter is responsible for destroying the file system or filter 
        // object below itin the stack.
        delete m_pNextFileSystem;
        m_pNextFileSystem = NULL;
    }
        
    return ERROR_SUCCESS;
}

// Call the FSD's FSD_FormatVolume export.
FileSystem_t* FileSystemFilterDriver_t::Hook (FileSystem_t* pNextFileSystem)
{
    DEBUGCHK(NULL == m_pNextFileSystem);
    
    __try {
        // Pass our member mountable disk as a PDSK to the FSD_HookVolume
        // export of the file system filter driver.        
        FILTERHOOK* pHook = pNextFileSystem->GetFilterHook ();
        m_FilterHook.hVolume = m_pfnHookVolume (m_pDisk, pHook);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }

    if (!m_FilterHook.hVolume) {
        // HookVolume failed for some reason.
        return NULL;
    }

    // Successfully hooked into the filter chain.
    m_pNextFileSystem = pNextFileSystem;

    // Load our FSD flags settings. This will simply bubble-up the child
    // file system's settings if they are not explicitly set for this filter.
    LoadSettings ();

    return this;
}

