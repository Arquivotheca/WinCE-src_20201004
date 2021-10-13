//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "TPartDrv.H"
#include "DiskHelp.H"

/* Store Management */

static HINSTANCE hPartDriver = NULL;
static PFN_OPENSTORE pfnOpenStore = NULL;
static PFN_CLOSESTORE pfnCloseStore = NULL;
static PFN_FORMATSTORE pfnFormatStore = NULL;
static PFN_ISSTOREFORMATTED pfnIsStoreFormatted = NULL;
static PFN_GETSTOREINFO pfnGetStoreInfo = NULL;
static PFN_CREATEPARTITION pfnCreatePartition = NULL;
static PFN_DELETEPARTITION pfnDeletePartition = NULL;
static PFN_RENAMEPARTITION pfnRenamePartition = NULL;
static PFN_SETPARTITIONATTRS pfnSetPartitionAttrs = NULL;
static PFN_FORMATPARTITION pfnFormatPartition = NULL;
static PFN_GETPARTITIONINFO pfnGetPartitionInfo = NULL;
static PFN_FINDPARTITIONSTART pfnFindPartitionStart = NULL;
static PFN_FINDPARTITIONNEXT pfnFindPartitionNext = NULL;
static PFN_FINDPARTITIONCLOSE pfnFindPartitionClose = NULL;
static PFN_OPENPARTITION pfnOpenPartition = NULL;
static PFN_CLOSEPARTITION pfnClosePartition = NULL;
static PFN_DEVICEIOCONTROL pfnDeviceIoControl = NULL;

// --------------------------------------------------------------------
BOOL LoadPartitionDriver(LPCTSTR szDriverName)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;

    hPartDriver = LoadLibrary(szDriverName);

    if(hPartDriver)
    {
        // load function pointers
        pfnOpenStore = (PFN_OPENSTORE)GetProcAddress(hPartDriver, _T("PD_OpenStore"));
        pfnCloseStore = (PFN_CLOSESTORE)GetProcAddress(hPartDriver, _T("PD_CloseStore"));
        pfnFormatStore = (PFN_FORMATSTORE)GetProcAddress(hPartDriver, _T("PD_FormatStore"));
        pfnIsStoreFormatted = (PFN_ISSTOREFORMATTED)GetProcAddress(hPartDriver,_T("PD_IsStoreFormatted"));
        pfnGetStoreInfo = (PFN_GETSTOREINFO)GetProcAddress(hPartDriver, _T("PD_GetStoreInfo"));
        pfnCreatePartition = (PFN_CREATEPARTITION)GetProcAddress(hPartDriver, _T("PD_CreatePartition"));
        pfnDeletePartition = (PFN_DELETEPARTITION)GetProcAddress(hPartDriver, _T("PD_DeletePartition"));
        pfnRenamePartition = (PFN_RENAMEPARTITION)GetProcAddress(hPartDriver, _T("PD_RenamePartition"));
        pfnSetPartitionAttrs = (PFN_SETPARTITIONATTRS)GetProcAddress(hPartDriver, _T("PD_SetPartitionAttrs"));
        pfnFormatPartition = (PFN_FORMATPARTITION)GetProcAddress(hPartDriver, _T("PD_FormatPartition"));
        pfnGetPartitionInfo = (PFN_GETPARTITIONINFO)GetProcAddress(hPartDriver, _T("PD_GetPartitionInfo"));
        pfnFindPartitionStart = (PFN_FINDPARTITIONSTART)GetProcAddress(hPartDriver, _T("PD_FindPartitionStart"));
        pfnFindPartitionNext = (PFN_FINDPARTITIONNEXT)GetProcAddress(hPartDriver, _T("PD_FindPartitionNext"));
        pfnFindPartitionClose = (PFN_FINDPARTITIONCLOSE)GetProcAddress(hPartDriver, _T("PD_FindPartitionClose"));
        pfnOpenPartition = (PFN_OPENPARTITION)GetProcAddress(hPartDriver, _T("PD_OpenPartition"));
        pfnClosePartition = (PFN_CLOSEPARTITION)GetProcAddress(hPartDriver, _T("PD_ClosePartition"));
        pfnDeviceIoControl = (PFN_DEVICEIOCONTROL)GetProcAddress(hPartDriver, _T("PD_DeviceIoControl"));

        // validate the function pointers
        if(NULL != pfnOpenStore &&
           NULL != pfnCloseStore &&
           NULL != pfnFormatStore &&
           NULL != pfnIsStoreFormatted &&
           NULL != pfnGetStoreInfo &&
           NULL != pfnCreatePartition &&
           NULL != pfnDeletePartition &&
           NULL != pfnRenamePartition &&
           NULL != pfnSetPartitionAttrs &&
           NULL != pfnFormatPartition &&
           NULL != pfnGetPartitionInfo &&
           NULL != pfnFindPartitionStart &&
           NULL != pfnFindPartitionNext &&
           NULL != pfnFindPartitionClose &&
           NULL != pfnOpenPartition &&
           NULL != pfnClosePartition &&
           NULL != pfnDeviceIoControl)
        {
            // driver loaded successfully
            fRet = TRUE;
        }
    }
    return fRet;
}

// --------------------------------------------------------------------
VOID FreePartitionDriver()
// --------------------------------------------------------------------
{
    if(hPartDriver)
    {
        // kill the function pointers
        pfnOpenStore = NULL;
        pfnCloseStore = NULL;
        pfnFormatStore = NULL;
        pfnIsStoreFormatted = NULL;
        pfnGetStoreInfo = NULL;
        pfnCreatePartition = NULL;
        pfnDeletePartition = NULL;
        pfnRenamePartition = NULL;
        pfnSetPartitionAttrs = NULL;
        pfnFormatPartition = NULL;
        pfnGetPartitionInfo = NULL;
        pfnFindPartitionStart = NULL;
        pfnFindPartitionNext = NULL;
        pfnFindPartitionClose = NULL;
        pfnOpenPartition = NULL;
        pfnClosePartition = NULL;
        pfnDeviceIoControl = NULL;

        // free the library
        FreeLibrary(hPartDriver);
        hPartDriver = NULL;
    }
   
}

// --------------------------------------------------------------------
STOREID T_OpenStore(HANDLE hDisk)
// --------------------------------------------------------------------
{   
    STOREID storeId = INVALID_STOREID;
    DWORD dwLastErr = ERROR_SUCCESS;

    if(pfnOpenStore)
    {
        dwLastErr = pfnOpenStore(hDisk, &storeId);
        SetLastError(dwLastErr);  
        if(ERROR_SUCCESS != dwLastErr)
        {
            ERR("PD_OpenStore()");
            storeId = INVALID_STOREID;
        }
    }
    return storeId;
}

// --------------------------------------------------------------------
VOID T_CloseStore(STOREID storeId)
// --------------------------------------------------------------------
{
    if(pfnCloseStore)
    {
        pfnCloseStore(storeId);
    }
}

// --------------------------------------------------------------------
BOOL T_FormatStore(STOREID storeId)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    DWORD dwLastErr = ERROR_SUCCESS;

    if(pfnFormatStore)
    {
        fRet = TRUE;
        dwLastErr = pfnFormatStore(storeId);
        SetLastError(dwLastErr);
        if(ERROR_SUCCESS != dwLastErr)
        {
            ERR("PD_FormatStore()");
            fRet = FALSE;
        }
    }
    
    return fRet;
}

// --------------------------------------------------------------------
BOOL T_IsStoreFormatted(STOREID storeId)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    DWORD dwLastErr = ERROR_SUCCESS;

    if(pfnIsStoreFormatted)
    {
        fRet = TRUE;    
        dwLastErr = pfnIsStoreFormatted(storeId);
        SetLastError(dwLastErr);
        if(ERROR_SUCCESS != dwLastErr)
        {
            ERR("PD_IsStoreFormatted()");
            fRet = FALSE;
        }    
    }
    
    return fRet;
}

// --------------------------------------------------------------------
BOOL T_GetStoreInfo(STOREID storeId, PD_STOREINFO *pInfo)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    DWORD dwLastErr = ERROR_SUCCESS;

    if(pfnGetStoreInfo)
    {
        fRet = TRUE;   
        dwLastErr = pfnGetStoreInfo(storeId, pInfo);
        SetLastError(dwLastErr);
        if(ERROR_SUCCESS != dwLastErr)
        {
            ERR("PD_GetStoreInfo()");
            fRet = FALSE;
        }
    }
    
    return fRet;
}

/* Partition management */

// --------------------------------------------------------------------
BOOL T_CreatePartition(STOREID storeId, LPCTSTR szPartName, BYTE bPartType, SECTORNUM numSectors, BOOL bAuto)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    DWORD dwLastErr = ERROR_SUCCESS;

    if(pfnCreatePartition)
    {
        fRet = TRUE;
        dwLastErr = pfnCreatePartition(storeId, szPartName, bPartType, numSectors, bAuto);
        SetLastError(dwLastErr);
        if(ERROR_SUCCESS != dwLastErr)
        {
            ERR("PD_CreatePartition()");
            fRet = FALSE;
        }
    }
    
    return fRet;
}

// --------------------------------------------------------------------
BOOL T_DeletePartition(STOREID storeId, LPCTSTR szPartName)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    DWORD dwLastErr = ERROR_SUCCESS;

    if(pfnDeletePartition)
    {
        fRet = TRUE;
        dwLastErr = pfnDeletePartition(storeId, szPartName);
        SetLastError(dwLastErr);
        if(ERROR_SUCCESS != dwLastErr)
        {
            ERR("PD_DeletePartition()");
            fRet = FALSE;
        }
    }
    
    return fRet;
}

// --------------------------------------------------------------------
BOOL T_RenamePartition(STOREID storeId, LPCTSTR szOldName, LPCTSTR szNewName)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    DWORD dwLastErr = ERROR_SUCCESS;

    if(pfnRenamePartition)
    {
        fRet = TRUE;
        dwLastErr = pfnRenamePartition(storeId, szOldName, szNewName);
        SetLastError(dwLastErr);
        if(ERROR_SUCCESS != dwLastErr)
        {
            ERR("PD_RenamePartition()");
            fRet = FALSE;
        }
    }
    
    return fRet;
}

// --------------------------------------------------------------------
BOOL T_SetPartitionAttrs(STOREID storeId, LPCTSTR szPartName, DWORD dwAttr)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    DWORD dwLastErr = ERROR_SUCCESS;

    if(pfnSetPartitionAttrs)
    {
        fRet = TRUE;
        dwLastErr = pfnSetPartitionAttrs(storeId, szPartName, dwAttr);
        SetLastError(dwLastErr);
        if(ERROR_SUCCESS != dwLastErr)
        {
            ERR("PD_SetPartitionAttrs()");
            fRet = FALSE;
        }
    }
    
    return fRet;
}

// --------------------------------------------------------------------
BOOL T_FormatPartition(STOREID storeId, LPCTSTR szPartName, BYTE bPartType, BOOL bAuto)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    DWORD dwLastErr = ERROR_SUCCESS;

    if(pfnFormatPartition)
    {
        fRet = TRUE;
        dwLastErr = pfnFormatPartition(storeId, szPartName, bPartType, bAuto);
        SetLastError(dwLastErr);
        if(ERROR_SUCCESS != dwLastErr)
        {
            ERR("PD_FormatPartition()");
            fRet = FALSE;
        }
    }
    
    return fRet;
}

// --------------------------------------------------------------------
BOOL T_GetPartitionInfo(STOREID storeId, LPCTSTR szPartName, PD_PARTINFO *pInfo)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    DWORD dwLastErr = ERROR_SUCCESS;

    if(pfnGetPartitionInfo)
    {
        fRet = TRUE;
        dwLastErr = pfnGetPartitionInfo(storeId, szPartName, pInfo);
        SetLastError(dwLastErr);
        if(ERROR_SUCCESS != dwLastErr)
        {
            ERR("PD_GetPartitionInfo()");
            fRet = FALSE;
        }
    }
    
    return fRet;
}

// --------------------------------------------------------------------
SEARCHID T_FindPartitionStart(STOREID storeId)
// --------------------------------------------------------------------
{
    SEARCHID searchId = INVALID_SEARCHID;
    DWORD dwLastErr = ERROR_SUCCESS;

    if(pfnFindPartitionStart)
    {
        dwLastErr = pfnFindPartitionStart(storeId, &searchId);
        SetLastError(dwLastErr);
        if(ERROR_SUCCESS != dwLastErr)
        {
            ERR("PD_GetPartitionInfo()");
            searchId = INVALID_SEARCHID;
        }
    }
    
    return searchId;
}

// --------------------------------------------------------------------
BOOL T_FindPartitionNext(SEARCHID searchId, PD_PARTINFO *pInfo)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    DWORD dwLastErr = ERROR_SUCCESS;

    if(pfnFindPartitionNext)
    {
        fRet = TRUE;
        dwLastErr = pfnFindPartitionNext(searchId, pInfo);
        SetLastError(dwLastErr);
        if(ERROR_SUCCESS != dwLastErr)
        {
            ERR("PD_FindPartitionNext()");
            fRet = FALSE;
        }
    }
    
    return fRet;
}

// --------------------------------------------------------------------
VOID T_FindPartitionClose(SEARCHID searchId)
// --------------------------------------------------------------------
{
    if(pfnFindPartitionClose)
    {
        pfnFindPartitionClose(searchId);
    }
}

/* Partition I/O */

// --------------------------------------------------------------------
PARTID T_OpenPartition(STOREID storeId, LPCTSTR szPartName)
// --------------------------------------------------------------------
{
    PARTID partId = INVALID_PARTID;
    DWORD dwLastErr = ERROR_SUCCESS;

    if(pfnOpenPartition)
    {
        dwLastErr = pfnOpenPartition(storeId, szPartName, &partId);
        SetLastError(dwLastErr);
        if(ERROR_SUCCESS != dwLastErr)
        {
            ERR("PD_OpenPartition()");
            partId = INVALID_PARTID;
        }
    }
    
    return partId;
}

// --------------------------------------------------------------------
VOID T_ClosePartition(PARTID partId)
// --------------------------------------------------------------------
{
    if(pfnClosePartition)
    {
        pfnClosePartition(partId);
    }
}

// --------------------------------------------------------------------
BOOL T_DeviceIoControl(PARTID partId, DWORD dwCode, PBYTE pInBuf, DWORD nInBufSize, 
    PBYTE pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    DWORD dwLastErr = ERROR_SUCCESS;

    if(pfnDeviceIoControl)
    {
        fRet = TRUE;
        dwLastErr = pfnDeviceIoControl(partId, dwCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned);
        SetLastError(dwLastErr);
        if(ERROR_SUCCESS != dwLastErr)
        {
            ERR("PD_DeviceIoControl()");
            fRet = FALSE;
        }
    }
    
    return fRet;
}
    
