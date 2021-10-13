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

static LRESULT RegisterFileSystem (int Index, HANDLE hAPISet, DWORD Context, 
    DWORD Version, DWORD MountFlags, HANDLE hOwnerProcess)
{       
    if (AFS_VERSION != Version) {
        // Invalid AFS version.
        return ERROR_INVALID_PARAMETER;
    }
    
    // Register the volume using the caller process as the owner. This must be
    // the same process that called RegisterAFSName or this call will fail.
    return g_pMountTable->RegisterVolume (Index, hAPISet, Context, hOwnerProcess,
        MountFlags);    
}

static LRESULT DeregisterFileSystem (int Index, HANDLE hOwnerProcess)
{
    if (!MountTable_t::IsValidIndex (Index)) {
        // Invalid AFS index.
        return ERROR_INVALID_PARAMETER;
    }

    return  g_pMountTable->DeregisterVolume (Index, hOwnerProcess);
}

EXTERN_C void FS_ProcessCleanupVolumes (HANDLE hProcess)
{
    g_pMountTable->LockTable ();

    // Walk the existing mounted volumes looking for any that
    // are owned by the exiting process.
    int MaxIndex = g_pMountTable->GetHighMountIndex ();
    for (int i = 0; i <= MaxIndex; i ++) {       

        MountInfo* pMountInfo = (*g_pMountTable)[i];

        if (pMountInfo && pMountInfo->hOwnerProcess == hProcess) {

            // Deregister this volume on behalf of the exiting
            // process.

            DEBUGMSG (ZONE_ERRORS, (L"FSDMGR!FS_ProcessCleanupVolumes: WARNING: process 0x%x leaked fs volume \"%s\"", 
                hProcess, pMountInfo->MountName));

            g_pMountTable->DeregisterVolume (i, hProcess, TRUE);
            g_pMountTable->DeregisterVolumeName (i, hProcess);
        }
    }

    g_pMountTable->UnlockTable ();
}



// FS_RegisterAFSName
//
// Exported as a file system API.
//
EXTERN_C int FSINT_RegisterAFSName (const WCHAR* pName)
{
    int index = INVALID_MOUNT_INDEX;
    HANDLE hProcess = reinterpret_cast<HANDLE> (GetCurrentProcessId ());
    LRESULT lRet = g_pMountTable->RegisterVolumeName (pName, &index, hProcess);
    SetLastError (lRet);
    return index; 
}

// FS_RegisterAFSName
//
// Exported as a file system API.
//
EXTERN_C int FSEXT_RegisterAFSName (const WCHAR* pName)
{
    // Make a local heap copy of the input string buffer.
    WCHAR* pLocalName = NULL;
    if (pName && FAILED (CeAllocDuplicateBuffer (reinterpret_cast<PVOID*>(&pLocalName),
        const_cast<WCHAR*>(pName), 0, ARG_I_WSTR))) {
        SetLastError (ERROR_OUTOFMEMORY);
        return INVALID_MOUNT_INDEX;
    }

    int index = INVALID_MOUNT_INDEX;
    HANDLE hProcess = reinterpret_cast<HANDLE> (GetCallerVMProcessId ());
    LRESULT lRet = g_pMountTable->RegisterVolumeName (pLocalName, &index, hProcess);

    // Free local heap copy of string.
    if (pLocalName) {
        VERIFY (SUCCEEDED (CeFreeDuplicateBuffer (pLocalName, const_cast<WCHAR*>(pName),
            0, ARG_I_WSTR)));
    }

    SetLastError (lRet);
    return index; 
}

// FS_RegisterAFSEx
//
// Exported as a file system API.
// 

EXTERN_C BOOL FSINT_RegisterAFSEx (int Index, HANDLE hAPISet, DWORD Context, 
    DWORD Version, DWORD Flags)
{
    LRESULT lResult;
    HANDLE hProcess = reinterpret_cast<HANDLE> (GetCurrentProcessId ());
    lResult = RegisterFileSystem (Index, hAPISet, Context, Version, Flags, hProcess);
    SetLastError (lResult);
    return (ERROR_SUCCESS == lResult);
}

EXTERN_C BOOL FSEXT_RegisterAFSEx (int Index, HANDLE hAPISet, DWORD Context, 
    DWORD Version, DWORD Flags)
{
    // Make a duplicate of the caller's API set in case the API set owner is not
    // the kernel process. This is required to call CreateAPIHandle using the
    // API set handle.
    if (!FsdDuplicateHandle (reinterpret_cast<HANDLE> (GetCallerVMProcessId ()), 
        hAPISet, reinterpret_cast<HANDLE> (GetCurrentProcessId ()), &hAPISet, 0, FALSE, 
        DUPLICATE_SAME_ACCESS)) {
        return FALSE;
    }
    
    LRESULT lResult;
    HANDLE hProcess = reinterpret_cast<HANDLE> (GetCallerVMProcessId ());
    lResult = RegisterFileSystem (Index, hAPISet, Context, Version, Flags, hProcess);
    
    // Close the duplicated API set handle.
    VERIFY(CloseHandle(hAPISet));

    SetLastError (lResult);
    return (ERROR_SUCCESS == lResult);
}

// FS_DeregisterAFS
//
// Exported as a file system API.
// 

EXTERN_C BOOL FSINT_DeregisterAFS (int Index)
{
    LRESULT lResult;
    HANDLE hProcess = reinterpret_cast<HANDLE> (GetCurrentProcessId ());
    lResult = DeregisterFileSystem (Index, hProcess);
    SetLastError (lResult);
    return (ERROR_SUCCESS == lResult);
}

EXTERN_C BOOL FSEXT_DeregisterAFS (int Index)
{
    LRESULT lResult;
    HANDLE hProcess = reinterpret_cast<HANDLE> (GetCallerVMProcessId ());
    lResult = DeregisterFileSystem (Index, hProcess);
    SetLastError (lResult);
    return (ERROR_SUCCESS == lResult);
}

// FS_DeregisterAFSName
// 
// Exported as a file system API.
//
EXTERN_C BOOL FSINT_DeregisterAFSName (int Index)
{
    HANDLE hProcess = reinterpret_cast<HANDLE> (GetCurrentProcessId ());
    LRESULT lRet = g_pMountTable->DeregisterVolumeName (Index, hProcess);
    SetLastError (lRet);
    return (ERROR_SUCCESS == lRet); 
}

EXTERN_C BOOL FSEXT_DeregisterAFSName (int Index)
{
    HANDLE hProcess = reinterpret_cast<HANDLE> (GetCallerVMProcessId ());
    LRESULT lRet = g_pMountTable->DeregisterVolumeName (Index, hProcess);
    SetLastError (lRet);
    return (ERROR_SUCCESS == lRet); 
}

