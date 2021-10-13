//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <storemain.h>

BOOL IsValidHandle(STOREHANDLE *pStoreHandle)
{
    BOOL fValid = FALSE;
    __try {
        if ((pStoreHandle->dwSig == STORE_HANDLE_SIG) || (pStoreHandle->dwSig == PART_HANDLE_SIG)){
            if (pStoreHandle->dwFlags & STOREHANDLE_TYPE_INTERNAL) {
                fValid = TRUE;
            } else {    
                fValid = IsValidStore( pStoreHandle->pStore);
            }    
        }    
    } __except(EXCEPTION_EXECUTE_HANDLER) {
    }
    return fValid;
}

HANDLE WINAPI STG_FindFirstStore(STOREINFO *pInfo, HANDLE hProc)
{
    HANDLE hFindStore = INVALID_HANDLE_VALUE;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_FindFirstStore()\r\n"));
    LockStoreMgr();
    __try {
        if (pInfo->cbSize == sizeof(STOREINFO)) {
            CStore *pStore = g_pStoreRoot;
            while(pStore) {
                if (!(pStore->m_dwFlags & STORE_FLAG_DETACHED)) {
                    break;
                }
                pStore = pStore->m_pNextStore;
            }
            if (pStore) {
                SEARCHSTORE *pSearch = new SEARCHSTORE;
                if (pSearch) {
                    pSearch->pStore = pStore;
                    pSearch->pStore->GetStoreInfo( pInfo);
                    pSearch->pPartition = INVALID_PARTITION;
                    pSearch->pNext = NULL;
                    pSearch->dwFlags = STOREHANDLE_TYPE_SEARCH;
                    pSearch->hProc = hProc;
                    pSearch->dwSig = STORE_HANDLE_SIG;
                    hFindStore = CreateAPIHandle(g_hFindStoreApi, pSearch);
                    if (hFindStore != INVALID_HANDLE_VALUE) {
                        AddHandleToList(&g_pRootHandle, pSearch);
                    }
                }
            } else {
                SetLastError(ERROR_NO_MORE_ITEMS);
            }    
        }  else {
            SetLastError(ERROR_BAD_ARGUMENTS);
        }    
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_BAD_ARGUMENTS);
    }
    UnlockStoreMgr();
    return hFindStore;
}

BOOL WINAPI STG_FindNextStore(SEARCHSTORE *pSearch, STOREINFO *pInfo)
{
    BOOL bRet = FALSE;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_FindNextStore(0x%08X)\r\n", pSearch));
    LockStoreMgr();
    __try {
        if (pInfo->cbSize == sizeof(STOREINFO)) {
            if (IsValidHandle(pSearch)) {
                if (pSearch->dwFlags & STOREHANDLE_TYPE_CURRENT) {
                    pSearch->dwFlags &= ~STOREHANDLE_TYPE_CURRENT;
                } else {
                    CStore *pStore = pSearch->pStore->m_pNextStore;
                    while(pStore) {
                        if (!(pStore->m_dwFlags & STORE_FLAG_DETACHED)) {
                            break;
                        }
                        pStore = pStore->m_pNextStore;
                    }
                    pSearch->pStore = pStore;
                }    
                if (pSearch->pStore && (pSearch->pStore != INVALID_STORE)) {
                    pSearch->pStore->GetStoreInfo( pInfo);
                    bRet = TRUE;
                } else {
                    SetLastError(ERROR_NO_MORE_ITEMS);
                }    
            } 
        }  else {
            SetLastError( ERROR_BAD_ARGUMENTS);
        }    
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_BAD_ARGUMENTS);
        bRet = FALSE;
    }
    UnlockStoreMgr();
    return bRet;
}

BOOL WINAPI STG_FindCloseStore(SEARCHSTORE *pSearch)
{
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_FindCloseStore(0x%08X)\r\n", pSearch));
    BOOL bRet = TRUE;
    LockStoreMgr();
    __try {
        if (DeleteHandleFromList(&g_pRootHandle, pSearch)) {
            delete pSearch;
        } else {
            bRet = FALSE;
        }   
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_BAD_ARGUMENTS);
        bRet = FALSE;
    }
    UnlockStoreMgr();
    return bRet;
}

HANDLE WINAPI STG_OpenStore(LPCTSTR szDeviceName, HANDLE hProc)
{
    HANDLE hStore = INVALID_HANDLE_VALUE;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_OpenStore(%s)\r\n", szDeviceName));
    STOREHANDLE *pStoreHandle = new STOREHANDLE;
    if (pStoreHandle) {
        LockStoreMgr();
        pStoreHandle->pStore = g_pStoreRoot;
        pStoreHandle->pPartition = INVALID_PARTITION;
        pStoreHandle->pNext = NULL;
        pStoreHandle->dwFlags = 0;
        pStoreHandle->hProc = hProc;
        pStoreHandle->dwSig = STORE_HANDLE_SIG;
        __try {
            while(pStoreHandle->pStore && (pStoreHandle->pStore != INVALID_STORE)) {
                if (wcsicmp( szDeviceName, pStoreHandle->pStore->m_szDeviceName) == 0) {
                    break;
                }    
                pStoreHandle->pStore = pStoreHandle->pStore->m_pNextStore;
            }
            if (pStoreHandle->pStore && (pStoreHandle->pStore != INVALID_STORE)) {
                hStore = CreateAPIHandle(g_hStoreApi, pStoreHandle);
                if (hStore != INVALID_HANDLE_VALUE) {
                    AddHandleToList(&g_pRootHandle, pStoreHandle);
                 } else {
                    delete pStoreHandle;
                    pStoreHandle = NULL;
                }   
            } else {
                SetLastError(ERROR_DEVICE_NOT_AVAILABLE);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_BAD_ARGUMENTS);
        }
        UnlockStoreMgr();
    }    
    if ((hStore == INVALID_HANDLE_VALUE) && pStoreHandle){
        delete pStoreHandle;
    }   
    return hStore;
}

BOOL WINAPI STG_CloseHandle(PSTOREHANDLE pStoreHandle)
{
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_CloseHandle(0x%08X)\r\n", pStoreHandle));
    BOOL bRet = TRUE;
    LockStoreMgr();
    __try {
        if (pStoreHandle->pStore && (pStoreHandle->dwSig == PART_HANDLE_SIG)) {
            CStore *pStore = pStoreHandle->pStore;
            if (pStore && (pStore != INVALID_STORE)) {
                pStore->Lock();
                if (DeleteHandleFromList(&pStore->m_pRootHandle, pStoreHandle)) {
                    delete pStoreHandle;
                } else {
                    bRet = FALSE;
                }   
                pStore->Unlock();
            }   
        } else {
            if (DeleteHandleFromList( &g_pRootHandle, pStoreHandle)) {
                delete pStoreHandle;
            }    
        }
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_BAD_ARGUMENTS);
        bRet = FALSE;
    }
    UnlockStoreMgr();
    return bRet;
}    

BOOL WINAPI STG_GetStoreInfo(PSTOREHANDLE pStoreHandle, PSTOREINFO pInfo)
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_GetStoreInfo(0x%08X)\r\n", pStoreHandle));
    __try {
        if (pInfo->cbSize == sizeof(STOREINFO)) {
            if (IsValidHandle(pStoreHandle)) {
                if (!pStoreHandle->pStore->GetStoreInfo( pInfo)) {
                    dwError = ERROR_GEN_FAILURE;
                }    
            } else {
                dwError = ERROR_BAD_ARGUMENTS;
            }
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }   
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_BAD_ARGUMENTS;
    }
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }    
    return (dwError == ERROR_SUCCESS);
}

BOOL WINAPI STG_DismountStore(PSTOREHANDLE pStoreHandle)
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_DismountStore(0x%08X)\r\n", pStoreHandle));
    __try {    
        if (IsValidHandle(pStoreHandle)) {
            CStore *pStore = pStoreHandle->pStore;
            pStore->Lock();
            if (!pStore->UnmountStore(FALSE)) {
                dwError = ERROR_GEN_FAILURE;
            }    
            pStore->Unlock();
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_BAD_ARGUMENTS;
    }
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }    
    return (dwError == ERROR_SUCCESS);
}

BOOL WINAPI STG_FormatStore(PSTOREHANDLE pStoreHandle)
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_FormatStore(0x%08X)\r\n", pStoreHandle));
    __try {    
        if (IsValidHandle(pStoreHandle)) {
            dwError = pStoreHandle->pStore->FormatStore();
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_BAD_ARGUMENTS;
    }
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }    
    return (dwError == ERROR_SUCCESS);
}

BOOL WINAPI STG_CreatePartition(PSTOREHANDLE pStoreHandle, LPCTSTR szPartitionName, DWORD dwPartType, DWORD dwHighSec, DWORD dwLowSec, BOOL bAuto)
{
    DWORD dwError = ERROR_SUCCESS;
    SECTORNUM snNumSectors = ((SECTORNUM)dwHighSec << 32) | dwLowSec;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_CreatePartition(0x%08X) %s PartType=%ld %ld(0x%08X%08X) Auto=%s\r\n",
              pStoreHandle,
              szPartitionName,
              dwPartType,
              dwLowSec,
              (DWORD)(snNumSectors >> 32),
              (DWORD)snNumSectors,
              bAuto ? L"TRUE" : L"FALSE"));
    __try {          
        if (IsValidHandle(pStoreHandle)) {
            dwError = pStoreHandle->pStore->CreatePartition( szPartitionName, (BYTE)dwPartType, snNumSectors, bAuto);
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_BAD_ARGUMENTS;
    }
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }    
    return (dwError == ERROR_SUCCESS);
}


BOOL WINAPI STG_DeletePartition(PSTOREHANDLE pStoreHandle, LPCTSTR szPartitionName)
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_DeletePartition(0x%08X) Name=%s\r\n", pStoreHandle,szPartitionName));
    __try {
        if (IsValidHandle(pStoreHandle)) {
            dwError = pStoreHandle->pStore->DeletePartition( szPartitionName);
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_BAD_ARGUMENTS;
    }
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }    
    return (dwError == ERROR_SUCCESS);
}

HANDLE WINAPI STG_OpenPartition(PSTOREHANDLE pStoreHandle, LPCTSTR szPartitionName)
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_OpenPartition(0x%08X) Name=%s\r\n", pStoreHandle,szPartitionName));
    HANDLE hPartition = INVALID_HANDLE_VALUE;
    __try {
        if (IsValidHandle(pStoreHandle)) {
            dwError = pStoreHandle->pStore->OpenPartition( szPartitionName, &hPartition, pStoreHandle->hProc);
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_BAD_ARGUMENTS;
    }
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }    
    return hPartition;
}



BOOL WINAPI STG_MountPartition(PSTOREHANDLE pStoreHandle)
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_MountPartition(0x%08X)\r\n", pStoreHandle));
    __try {
        if (IsValidHandle(pStoreHandle)) {
            dwError = pStoreHandle->pStore->MountPartition( pStoreHandle->pPartition);
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_BAD_ARGUMENTS;
    }
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }    
    return (dwError == ERROR_SUCCESS);
}

BOOL WINAPI STG_DismountPartition(PSTOREHANDLE pStoreHandle)
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_DismountPartition(0x%08X)\r\n", pStoreHandle));
    __try {
        if (IsValidHandle(pStoreHandle)) {
            dwError = pStoreHandle->pStore->UnmountPartition( pStoreHandle->pPartition);
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_BAD_ARGUMENTS;
    }
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }    
    return (dwError == ERROR_SUCCESS);
}

BOOL WINAPI STG_RenamePartition(PSTOREHANDLE pStoreHandle, LPCTSTR szNewName)
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_RenamePartition(0x%08X) Name=%s\r\n", pStoreHandle, szNewName));
    __try {
        if (IsValidHandle(pStoreHandle)) {
            dwError = pStoreHandle->pStore->RenameParttion(pStoreHandle->pPartition, szNewName);
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_BAD_ARGUMENTS;
    }
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }    
    return (dwError == ERROR_SUCCESS);
}


BOOL WINAPI STG_SetPartitionAttributes(PSTOREHANDLE pStoreHandle, DWORD dwAttrs)
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_SetPartitionAttributes(0x%08X) Attrs=%08X\r\n", pStoreHandle, dwAttrs));
    __try {
        if (IsValidHandle(pStoreHandle)) {
            dwError = pStoreHandle->pStore->SetPartitionAttributes(pStoreHandle->pPartition, dwAttrs);
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_BAD_ARGUMENTS;
    }
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }    
    return (dwError == ERROR_SUCCESS);
}

BOOL   WINAPI STG_DeviceIoControl(PSTOREHANDLE pStoreHandle, DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned, OVERLAPPED *pOverlapped)
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_DeviceIoControl(0x%08X) IoCode=%ld\r\n", pStoreHandle, dwIoControlCode));
    __try {
        if (IsValidHandle(pStoreHandle) && !(pStoreHandle->dwFlags & STOREHANDLE_TYPE_SEARCH)) {
            dwError = pStoreHandle->pStore->DeviceIoControl(pStoreHandle->pPartition, dwIoControlCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned);
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_BAD_ARGUMENTS;
    }
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }    
    return (dwError == ERROR_SUCCESS);
}


BOOL WINAPI STG_GetPartitionInfo(PSTOREHANDLE pStoreHandle, PPARTINFO pInfo)
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_GetPartitionInfo(0x%08X)\r\n", pStoreHandle));
    __try {
        if (pInfo->cbSize == sizeof(PARTINFO)) {
            if (IsValidHandle(pStoreHandle)) {
                dwError = pStoreHandle->pStore->GetPartitionInfo(pStoreHandle->pPartition, pInfo);
            } else {
                dwError = ERROR_BAD_ARGUMENTS;
            }
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }   
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_BAD_ARGUMENTS;
    }
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }    
    return (dwError == ERROR_SUCCESS);
}


BOOL WINAPI STG_FormatPartition(PSTOREHANDLE pStoreHandle, DWORD bPartType, BOOL bAuto)
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_FormatPartition(0x%08X) PartType=%ld Auto=%s\r\n", pStoreHandle, bPartType, bAuto ? L"TRUE" : L"FALSE"));
    __try {
        if (IsValidHandle(pStoreHandle)) {
            dwError = pStoreHandle->pStore->FormatPartition(pStoreHandle->pPartition, (BYTE)bPartType, bAuto);
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }
        if (dwError != ERROR_SUCCESS) {
            SetLastError(dwError);
        }    
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_BAD_ARGUMENTS;
    }
    return (dwError == ERROR_SUCCESS);
}

HANDLE WINAPI STG_FindFirstPartition(PSTOREHANDLE pStoreHandle, PPARTINFO pInfo)
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_FindFirstPartition(0x%08X)\r\n", pStoreHandle));
    HANDLE hSearch = INVALID_HANDLE_VALUE;
    __try {
        if (pInfo->cbSize == sizeof(PARTINFO)) {
            if (IsValidHandle(pStoreHandle)) {
                dwError = pStoreHandle->pStore->FindFirstPartition(pInfo, &hSearch, pStoreHandle->hProc);
            } else {
                dwError = ERROR_BAD_ARGUMENTS;
            }
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }   
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_BAD_ARGUMENTS;
    }
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }    
    return hSearch;
}


BOOL WINAPI STG_FindNextPartition(SEARCHPARTITION *pSearch, PPARTINFO pInfo)
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_FindNextPartition(0x%08X)\r\n", pSearch));
    __try {
        if (pInfo->cbSize == sizeof(PARTINFO)) {
            if (IsValidHandle(pSearch)) {
                dwError = pSearch->pStore->FindNextPartition(pSearch, pInfo);
            } else {
                dwError = ERROR_BAD_ARGUMENTS;
            }
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }   
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_BAD_ARGUMENTS;
    }
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }    
    return (dwError == ERROR_SUCCESS);
}

BOOL WINAPI STG_FindClosePartition(SEARCHPARTITION *pSearch)
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, (L"FSDMGR:STG_FindClosePartition(0x%08X)\r\n", pSearch));
    __try {
        if (IsValidHandle(pSearch)) {
            dwError = pSearch->pStore->FindClosePartition(pSearch);
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_BAD_ARGUMENTS;
    }
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }
    return (ERROR_SUCCESS == dwError);
}

