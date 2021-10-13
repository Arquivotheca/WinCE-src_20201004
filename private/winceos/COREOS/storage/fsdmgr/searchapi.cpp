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
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include "storeincludes.hpp"

#ifdef UNDER_CE
const PFNVOID SearchAPIMethods[NUM_FIND_APIS] = {
    (PFNVOID)FSDMGR_FindClose,
    (PFNVOID)0,
    (PFNVOID)FSDMGR_FindNextFileW
};

const ULONGLONG SearchAPISigs[NUM_FIND_APIS] = {
    FNSIG1(DW),                 // FindClose
    FNSIG0(),                   // unused
    FNSIG3(DW,O_PTR,DW)         // FindNextFile
};
#endif // UNDER_CE

HANDLE hSearchAPI;

LRESULT InitializeSearchPI ()
{
#ifdef UNDER_CE
    // Initialize the handle-based file API.
    hSearchAPI = CreateAPISet (const_cast<CHAR*> ("FFHN"), NUM_FIND_APIS, SearchAPIMethods, SearchAPISigs);
    RegisterAPISet (hSearchAPI, HT_FIND | REGISTER_APISET_TYPE);
#endif
    return ERROR_SUCCESS;
}

EXTERN_C BOOL FSDMGR_FindNextFileW (FileSystemHandle_t* pHandle, WIN32_FIND_DATAW* pFindData,
    DWORD SizeOfFindData)
{
#ifdef UNDER_CE
    if (sizeof (WIN32_FIND_DATAW) != SizeOfFindData) {
        DEBUGCHK (0); // FindNextFileW_Trap macro was called directly w/out proper size.
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }
#endif // UNDER_CE

    BOOL fRet = FALSE;
    
    MountedVolume_t* pVolume;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);
    
    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        DWORD HandleContext = pHandle->GetHandleContext ();
        fRet = pFileSystem->FindNextFileW (HandleContext, pFindData);

        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C BOOL FSDMGR_FindClose (FileSystemHandle_t* pHandle)
{
    MountedVolume_t* pVolume;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);
    if (ERROR_SUCCESS == lResult) {

        DEBUGCHK (pVolume);

        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        DWORD HandleContext = pHandle->GetHandleContext ();

        // A file system should never fail FindClose because there is
        // no way of propagating failure back up to the caller of 
        // CloseHandle at this point.
        pFileSystem->FindClose (HandleContext);

        // The FileSystemHandle_t destructor will remove the handle from the owning
        // volume's handle list.
        delete pHandle;
        
        pVolume->Exit ();

    } else {

        // The volume cannot be entered, just delete the handle to finalize
        // all cleanup. At this point, the volume object will still be exist
        // because there is at least one (this) handle, though it will be 
        // orphaned and cannot be entered.
        delete pHandle;
    }

    return TRUE;
}

