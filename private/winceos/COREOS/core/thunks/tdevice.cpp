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
#define BUILDING_FS_STUBS
#define BUILDING_USER_STUBS
#include <windows.h>
#include <devload.h> // needed to map ActivateDevice
#include <guiddef.h>


// Device external exports

extern "C"
HANDLE xxx_RegisterDevice(LPCWSTR lpszname, DWORD index, LPCWSTR lpszLib,
              DWORD dwInfo) {
    return RegisterDevice_Trap(lpszname,index,lpszLib,dwInfo);
}

extern "C"
BOOL xxx_DeregisterDevice(HANDLE hDevice) {
    return DeregisterDevice_Trap(hDevice);
}

extern "C"
BOOL xxx_LoadFSD(HANDLE hDevice, LPCWSTR lpFSDName) {
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

extern "C"
BOOL xxx_LoadFSDEx(HANDLE hDevice, LPCWSTR lpFSDName, DWORD dwFlag) {
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

extern "C"
HANDLE xxx_ActivateDevice(LPCWSTR lpszDevKey, DWORD dwClientInfo) {
    // Add this to the registry to maintain compatibility with the older API;
    // it needs to be here so that the memory access checks can succeed.
    REGINI reg;
    reg.lpszVal = DEVLOAD_CLIENTINFO_VALNAME;
    reg.dwType = DEVLOAD_CLIENTINFO_VALTYPE;
    reg.pData = (LPBYTE) &dwClientInfo;
    reg.dwLen = sizeof(dwClientInfo);
    return ActivateDeviceEx_Trap(lpszDevKey, &reg, 1, NULL);
}

extern "C"
HANDLE xxx_ActivateDeviceEx(LPCWSTR lpszDevKey, LPCVOID lpRegEnts, DWORD cRegEnts, LPVOID lpvParam) {
    return ActivateDeviceEx_Trap(lpszDevKey,lpRegEnts,cRegEnts,lpvParam);
}

extern "C"
BOOL xxx_REL_UDriverProcIoControl(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned) {
    return REL_UDriverProcIoControl_Trap( dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf,nOutBufSize, lpBytesReturned) ;
}
extern "C"
HANDLE xxx_CreateAsyncIoHandle(HANDLE hIoRef, LPVOID * lpInBuf ,LPVOID * lpOutBuf) {
#ifdef KCOREDLL
    return CreateAsyncIoHandle_Trap(hIoRef, lpInBuf , lpOutBuf) ;
#else
    REF_CREATE_ASYNC_IO_HANDLE refCreateAsyncIoHandle = {
         hIoRef,NULL,NULL
    };
    BOOL hResult = REL_UDriverProcIoControl_Trap(IOCTL_REF_CREATE_ASYNC_IO_HANDLE,NULL,0,&refCreateAsyncIoHandle,sizeof(refCreateAsyncIoHandle),NULL);
    if (hResult) {
         if (lpInBuf)
              *lpInBuf = refCreateAsyncIoHandle.lpInBuf;
         if (lpOutBuf)
              *lpOutBuf = refCreateAsyncIoHandle.lpOutBuf;
         return (refCreateAsyncIoHandle.hCompletionHandle);
    }
    return (NULL);
#endif
}

extern "C"
BOOL xxx_SetIoProgress(HANDLE ioHandle, DWORD dwBytesTransfered)
{
    return SetIoProgress_Trap(ioHandle,dwBytesTransfered);
}
extern "C"
BOOL xxx_CompleteAsyncIo(HANDLE ioHandle, DWORD dwCompletedByte,DWORD dwFinalStatus)
{
    return CompleteAsyncIo_Trap(ioHandle,dwCompletedByte,dwFinalStatus);
}
extern "C"
BOOL xxx_EnableDeviceDriver(HANDLE hDevice, DWORD dwMilliseconds)
{
    return EnableDeviceDriver_Trap(hDevice,dwMilliseconds);
}
extern "C"
BOOL xxx_DisableDeviceDriver(HANDLE hDevice, DWORD dwMilliseconds)
{
    return DisableDeviceDriver_Trap (hDevice,dwMilliseconds);
}
extern "C"
BOOL xxx_GetDeviceDriverStatus(HANDLE hDevice, LPDWORD lpdResult)
{
    return GetDeviceDriverStatus_Trap (hDevice,lpdResult);
}


extern "C"
BOOL xxx_DeactivateDevice(HANDLE hDevice) {
    return DeactivateDevice_Trap(hDevice);
}

extern "C"
BOOL xxx_CeResyncFilesys(HANDLE hDevice) {
    return CeResyncFilesys_Trap(hDevice);
}

extern "C"
HANDLE GetDeviceHandleFromContext(LPCWSTR pszContext)
{
    DWORD dwStatus;
    HKEY hk;
    HANDLE hDevice = NULL;
    
    dwStatus = RegOpenKeyExW_Trap(HKEY_LOCAL_MACHINE, pszContext, 0, 0, &hk);
    if (dwStatus == ERROR_SUCCESS) {
        DWORD dwType, dwValue;
        DWORD dwSize = sizeof(dwValue);
        dwStatus = RegQueryValueExW_Trap(hk, DEVLOAD_HANDLE_VALNAME, NULL, &dwType, (LPBYTE) &dwValue, dwSize, &dwSize);
    
        if(dwStatus == ERROR_SUCCESS && dwType == REG_DWORD) {
            hDevice = (HANDLE) dwValue;
        }
        RegCloseKey_Trap(hk);
    }

    return hDevice;
}

extern "C"
HKEY xxx_RegOpenProcessKey(DWORD k) {
    TCHAR szActivePath[256];
    HKEY hkDevice;
    
    StringCchPrintf(szActivePath, _countof(szActivePath), TEXT("%s\\%02u"), DEVLOAD_ACTIVE_KEY, k);
    hkDevice = OpenDeviceKey(szActivePath);
    return hkDevice;
}

extern "C"
BOOL xxx_ResourceCreateList (DWORD dwResId, DWORD dwMinimum, DWORD dwCount)
{
    return ResourceCreateList_Trap(dwResId, dwMinimum, dwCount);
}

extern "C"
BOOL xxx_ResourceDestroyList (DWORD dwResId)
{
    return ResourceDestroyList_Trap(dwResId);
}

extern "C"
BOOL xxx_ResourceRequest (DWORD dwResId, DWORD dwId, DWORD dwLen)
{
    return ResourceRequestEx_Trap(dwResId, dwId, dwLen, 0);
}

extern "C"
BOOL xxx_ResourceRequestEx (DWORD dwResId, DWORD dwId, DWORD dwLen, DWORD dwFlags)
{
    return ResourceRequestEx_Trap(dwResId, dwId, dwLen, dwFlags);
}

extern "C"
BOOL xxx_ResourceRelease (DWORD dwResId, DWORD dwId, DWORD dwLen)
{
    return ResourceAdjust_Trap(dwResId, dwId, dwLen, FALSE);
}

extern "C"
BOOL xxx_ResourceMarkAsShareable (DWORD dwResId, DWORD dwId, DWORD dwLen, BOOL fShared)
{
    return ResourceMarkAsShareable_Trap(dwResId, dwId, dwLen, fShared);
}

extern "C"
BOOL xxx_GetDeviceByIndex(DWORD dwIndex, LPWIN32_FIND_DATA lpFindFileData) {
    return GetDeviceByIndex_Trap(dwIndex, lpFindFileData);
}

extern "C"
BOOL xxx_GetDeviceInformationByDeviceHandle(HANDLE hDevice, PDEVMGR_DEVICE_INFORMATION pdi)
{
    return GetDeviceInformationByDeviceHandle_Trap(hDevice, pdi);
}

extern "C"
BOOL xxx_GetDeviceInformationByFileHandle(HANDLE hDevice, PDEVMGR_DEVICE_INFORMATION pdi)
{
    return GetDeviceInformationByFileHandle_Trap(hDevice, pdi);
}

extern "C"
HANDLE
FindFirstDevice(DeviceSearchType searchType, LPCVOID pvSearchParam, PDEVMGR_DEVICE_INFORMATION pdi)
{
    LPCWSTR pszNamespace = L"$device\\";
    LPCWSTR pszSearchType = NULL;
    WCHAR   _localBuffer[MAX_PATH];
    LPWSTR pszBuf = _localBuffer;
    DWORD dwLen;
    
    // get the number of characters we need
    dwLen = 0;
    if(searchType == DeviceSearchByLegacyName || searchType == DeviceSearchByDeviceName || searchType == DeviceSearchByBusName) {
        dwLen += wcslen((LPCWSTR) pvSearchParam) + 1;
        if(searchType == DeviceSearchByLegacyName) {
            pszSearchType = L"byLegacyName";
        } else if(searchType == DeviceSearchByDeviceName) {
            pszSearchType = L"byDeviceName";
        } else {
            DEBUGCHK(searchType == DeviceSearchByBusName);
            pszSearchType = L"byBusName";
        }
    } 
    else if(searchType == DeviceSearchByParent) {
        dwLen += 9;                 // 8 hex chars + null
        pszSearchType = L"byParent";
    }
    else if(searchType == DeviceSearchByGuid) {
        dwLen += 32 + 4 + 2 + 1;    // 32 digits, 4 hyphens, two braces, and a null
        pszSearchType = L"byInterfaceClass";
    }
    else {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    // is the size of the parameter too large
    if(dwLen >= MAX_PATH) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    // add other parts of the string
    DEBUGCHK(pszSearchType != NULL);
    dwLen += wcslen(pszNamespace) + 1;      // room for namespace and null
    dwLen += wcslen(pszSearchType) + 1;     // length of search type string
    dwLen += 3;                             // plus two embedded nulls and a guard character

    // allocate the buffer if needed
    if (dwLen > MAX_PATH) {
        pszBuf = (LPWSTR) LocalAlloc (LMEM_FIXED, dwLen * sizeof (WCHAR));
        if (!pszBuf) {
            // last error already set if allocation failed.
            return INVALID_HANDLE_VALUE;
        }
    }

    // convert to bytes
    dwLen *= sizeof(WCHAR);

#ifdef DEBUG
    memset(pszBuf, 0, dwLen);
#endif  // DEBUG

    // format the parameter structure
    if(searchType == DeviceSearchByBusName || searchType == DeviceSearchByLegacyName || searchType == DeviceSearchByDeviceName) {
        StringCbPrintf(pszBuf, dwLen, L"%s\\%s+%s", pszNamespace, pszSearchType, (LPCWSTR) pvSearchParam);
    }
    else if(searchType == DeviceSearchByGuid) {
        const GUID *pGuid = (const GUID *) pvSearchParam;
        StringCbPrintf(pszBuf, dwLen, L"%s\\%s+{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
            pszNamespace, pszSearchType,
            pGuid->Data1, pGuid->Data2, pGuid->Data3, 
            pGuid->Data4[0], pGuid->Data4[1], pGuid->Data4[2], pGuid->Data4[3], 
            pGuid->Data4[4], pGuid->Data4[5], pGuid->Data4[6], pGuid->Data4[7]);
    } 
    else if(searchType == DeviceSearchByParent) {
        StringCbPrintf(pszBuf, dwLen, L"%s\\%s+%08x", pszNamespace, pszSearchType, (DWORD) pvSearchParam);
    } else {
        DEBUGCHK(FALSE);
    }
    DEBUGCHK(pszBuf[(dwLen / sizeof(pszBuf[0])) - 1] == 0);
    
    HANDLE hFind = FindFirstFileW_Trap (pszBuf, (LPWIN32_FIND_DATA)pdi, sizeof (DEVMGR_DEVICE_INFORMATION));

    if (_localBuffer != pszBuf) {
        LocalFree (pszBuf);
    }

    return hFind;
}

extern "C"
BOOL
FindNextDevice(HANDLE h, PDEVMGR_DEVICE_INFORMATION pdi)
{
    BOOL fOk = FindNextFileW_Trap(h, (PWIN32_FIND_DATA) pdi, sizeof(DEVMGR_DEVICE_INFORMATION));
    return fOk;
}

extern "C"
BOOL 
xxx_EnumDeviceInterfaces(HANDLE h, DWORD dwIndex, GUID *pClass, LPWSTR pszNameBuf, LPDWORD lpdwNameBufSize)
{
    return EnumDeviceInterfaces_Trap(h, dwIndex, pClass, pszNameBuf, lpdwNameBufSize);
}

