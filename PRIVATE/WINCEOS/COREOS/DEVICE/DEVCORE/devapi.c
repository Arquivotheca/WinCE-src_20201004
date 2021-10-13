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

#include <windows.h>
#include <devload.h>
#include <console.h>

#include "devmgrp.h"
#include "celogdev.h"
#include "devmgrif.h"
#include <fsioctl.h>
#include "pmif.h"
#include "devzones.h"
// This routine is hit when some one call unsupported API set.
BOOL WINAPI DM_Unsupported()
{
    DEBUGMSG(ZONE_ACTIVE, (TEXT("DEVICE!DM_Unsupported!!!\r\n")));
    SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
    ASSERT(FALSE);
    return FALSE;
}
// This routine exposes AdvertiseInterface() to device drivers that link directly
// with the device manager DLL.  The hDevice parameter is the device handle provided
// to the driver in its Init() entry point.  The other parameters are exactly equivalent
// to those of AdvertiseInterface().  This routine returns ERROR_SUCCESS if 
// the interface is successfully advertised, or a Win32 error code if it is not.
DWORD WINAPI 
DmAdvertiseInterface(HANDLE hDevice, const GUID *devclass, LPCWSTR name, BOOL fAdd)
{
    return I_AdvertiseDeviceInterface((fsdev_t *) hDevice, devclass, name, fAdd);
}

// This routine handles RegisterDevice() calls and routes them internally 
// to I_RegisterDeviceEx().
HANDLE
DM_RegisterDevice(
    LPCWSTR lpszType,
    DWORD   dwIndex,
    LPCWSTR lpszLib,
    DWORD   dwInfo
    )
{
    return I_RegisterDeviceEx(
                lpszType,
                dwIndex,
                lpszLib,
                dwInfo
                );
}


//      @func   BOOL | DeregisterDevice | Deregister a registered device
//  @parm       HANDLE | hDevice | handle to registered device, from RegisterDevice
//      @rdesc  Returns TRUE for success, FALSE for failure
//      @comm   DeregisterDevice can be used if a device is removed from the system or
//                      is being shut down.  An example would be:<nl>
//                      <tab>DeregisterDevice(h1);<nl>
//                      where h1 was returned from a call to RegisterDevice.
//      @xref <f RegisterDevice> <l Overview.WinCE Device Drivers>

BOOL DM_DeregisterDevice(HANDLE hDevice)
{
    BOOL retval = I_DeregisterDevice(hDevice);
    return retval;
}


//
//  @func   HANDLE | ActivateDevice | Register a device and add it to the active list under HKEY_LOCAL_MACHINE\Drivers\Active.
//  @parm   LPCWSTR | lpszDevKey | The driver's device key under HKEY_LOCAL_MACHINE\Drivers (for example "Drivers\PCMCIA\Modem")
//  @parm   DWORD | dwClientInfo | Instance information to be stored in the device's active key.
//  @rdesc  Returns a handle to a device, or 0 for failure.  This handle should
//          only be passed to DeactivateDevice
//  @comm   ActivateDevice will RegisterDevice the specified device and
//  create an active key in HKEY_LOCAL_MACHINE\Drivers\Active for the new device.
//  Also the device's creation is announced to the system via a
//  WM_DEVICECHANGE message and through the application notification system.
//
//  An example would be:<nl>
//  <tab>hnd = ActivateDevice(lpszDevKey, dwClientInfo);<nl>
//  where hnd is a HANDLE that can be used for DeactivateDevice<nl>
//  lpszDevKey is the registry path in HKEY_LOCAL_MACHINE to the device
//    driver's device key which has values that specify the driver name and
//    device prefix.<nl>
//  dwClientInfo is a DWORD value that will be stored in the device's active
//    key.<nl>
//  @xref <f DeactivateDevice> <f RegisterDevice> <f DeregisterDevice> <l Overview.WinCE Device Drivers>
//
HANDLE DM_ActivateDeviceEx(
    LPCTSTR lpszDevKey,
    LPCREGINI lpReg,
    DWORD cReg,
    LPVOID lpvParam
    )
{
    DEBUGMSG(ZONE_ACTIVE, (TEXT("DEVICE!ActivateDeviceEx(%s) entered\r\n"), lpszDevKey));
    CELOG_ActivateDevice (lpszDevKey);
    return I_ActivateDeviceEx(
               lpszDevKey,
               lpReg, cReg, lpvParam);
}
BOOL CreateInitReg(LPREGINI pDestReg,LPCREGINI pSrcReg, int nCount )
{
    BOOL fOk = FALSE;
    __try {
        if (nCount && pSrcReg && pSrcReg->lpszVal && pSrcReg->pData  && pDestReg ) { 
            DWORD dwLen;
            DWORD dwTotalLen;
            LPCREGINI pCurReg;
            BOOL fContinue = TRUE;
            for (pCurReg = pSrcReg ; nCount!=0 && pCurReg!=NULL && fContinue ; pCurReg++, pDestReg++, nCount --) {
                LPTSTR lpMarsheledString = NULL;
                LPBYTE lpMarsheledData = NULL;
                const DWORD dwTCHARperDWORD = sizeof(DWORD)/sizeof(TCHAR);
                if (!SUCCEEDED(CeOpenCallerBuffer(&lpMarsheledString,(PVOID)pCurReg->lpszVal,0,ARG_I_WSTR,FALSE))) {
                    lpMarsheledString = NULL;
                }
                if (!SUCCEEDED(CeOpenCallerBuffer(&lpMarsheledData,(PVOID)pCurReg->pData,pCurReg->dwLen ,ARG_I_PTR,FALSE))) {
                    lpMarsheledData = NULL;
                }
                if (lpMarsheledString && lpMarsheledData) {
                    __try {
                        dwLen= (_tcslen( lpMarsheledString ) + 1)*sizeof(TCHAR) ;
                        dwLen = ((dwLen + sizeof(DWORD)-1)/sizeof(DWORD))*sizeof(DWORD); // 32-bit alligment.
                        dwTotalLen = dwLen + pCurReg->dwLen ; // Size is BYTE.
                        pDestReg->lpszVal = malloc(dwTotalLen);
                        if (pDestReg->lpszVal && dwTotalLen >= dwLen ) { // we do it when alloc success and no integer overflow
                            HRESULT hResult = StringCbCopy( (LPTSTR)(pDestReg->lpszVal), dwLen, lpMarsheledString);
                            ASSERT(SUCCEEDED(hResult));
                            pDestReg->pData=((PBYTE)pDestReg->lpszVal) + dwLen;
                            memcpy(pDestReg->pData,lpMarsheledData,pCurReg->dwLen);
                            pDestReg->dwType = pCurReg->dwType;
                            pDestReg->dwLen = pCurReg->dwLen;
                        }
                        else
                            fContinue = FALSE;
                    }
                    __except(EXCEPTION_EXECUTE_HANDLER) {
                        DEBUGMSG(ZONE_ERROR,(TEXT("DEVICE! Exception CreateInitReg for\r\n")));
                        fContinue = FALSE;
                    }
                }
                else
                    fContinue = FALSE;
                if (lpMarsheledString) {
                    HRESULT hResult = CeCloseCallerBuffer(lpMarsheledString,(PVOID)pCurReg->lpszVal, 0 , ARG_I_WSTR);
                    ASSERT(SUCCEEDED(hResult));
                }
                if (lpMarsheledData) {
                    HRESULT hResult = CeCloseCallerBuffer(lpMarsheledData,(PVOID)pCurReg->pData,pCurReg->dwLen,ARG_I_PTR);
                    ASSERT(SUCCEEDED(hResult));
                }
            }
            fOk = fContinue;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_ERROR,(TEXT("DEVICE! Exception CreateInitReg for\r\n")));
        DEBUGCHK(FALSE);
        fOk = FALSE;
    }
    return (fOk);
}
BOOL DeleteInitReg(LPREGINI pDestReg,int nCount )
{
    while (pDestReg && nCount) {
        if (pDestReg->lpszVal!=NULL) {
            free((PVOID)pDestReg->lpszVal);
            pDestReg->lpszVal = NULL;
            pDestReg->pData = NULL;
        }
        pDestReg ++ ;
        nCount--;
    }
    return TRUE;
}

HANDLE EX_DM_ActivateDeviceEx(
    LPCTSTR lpszDevKey,
    LPCREGINI lpReg,
    DWORD cReg,
    LPVOID lpvParam
    )
{
    LPREGINI lpRegLocal = NULL;
    LPREGINI lpMarsheldReg = NULL;
    HANDLE rtHandle = NULL;
    DWORD status ;
    TCHAR iDevKey[MAX_PATH];
    CELOG_ActivateDevice (lpszDevKey);
    DEBUGMSG(ZONE_ACTIVE, (TEXT("DEVICE!ActivateDeviceEx(%s) entered\r\n"), lpszDevKey));
    if (lpszDevKey) {
        StringCchCopy(iDevKey,MAX_PATH,lpszDevKey);
    }
    else {
        SetLastError(ERROR_BAD_CONFIGURATION);
        return NULL;
    }
    if (CeGetCallerTrust() != OEM_CERTIFY_TRUST) { // Untrusted caller will do following.
        HKEY hActiveKey;
        if (lpReg!=NULL && cReg!=0) { // Those parameter only can be set by Trusted API.
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
        if ((status =RegOpenKeyEx( HKEY_LOCAL_MACHINE, iDevKey, 0, 0, &hActiveKey)) == ERROR_SUCCESS ) { // It is registry.
            // We need check Registry is in secure location.
            CE_REGISTRY_INFO regInfo;
            status = ERROR_ACCESS_DENIED;
            memset(&regInfo,0,sizeof(regInfo));
            regInfo.cbSize = sizeof(CE_REGISTRY_INFO);
            if (CeFsIoControl(NULL, FSCTL_GET_REGISTRY_INFO, &hActiveKey, sizeof(HKEY), &regInfo, sizeof(CE_REGISTRY_INFO), NULL, NULL)) { // Succeed
                if (regInfo.dwFlags & CE_REG_INFO_FLAG_TRUST_PROTECTED) {
                    status = ERROR_SUCCESS;
                }
            }
            RegCloseKey( hActiveKey ); 
        }
        if (status!=ERROR_SUCCESS) {
            SetLastError(status);
            return FALSE ;
        }
    }
    
    // The lpszDevKey has been verified by kernel. We need varify the lpReg .
    // Don't know how to varify lpvParm. So, every driver has to varify it if they use it.
    if (cReg!=0 && lpReg!=NULL && cReg < MAXDWORD/sizeof( REGINI)) {
        // Create Local Copy.
        DWORD dwCopiedSize = cReg*sizeof( REGINI);
        HRESULT hResult= CeOpenCallerBuffer( &lpMarsheldReg,(PVOID)lpReg,dwCopiedSize ,ARG_I_PTR, TRUE);
        lpRegLocal = (LPREGINI)malloc(dwCopiedSize) ;
        if (SUCCEEDED(hResult)&& lpMarsheldReg && lpRegLocal) {
            memset((PVOID)lpRegLocal,0,sizeof(REGINI)*cReg);
            if (CreateInitReg(lpRegLocal,lpMarsheldReg,cReg ))
                rtHandle = I_ActivateDeviceEx(iDevKey, lpRegLocal, cReg, lpvParam) ;
            else 
                SetLastError(ERROR_INVALID_PARAMETER);
            DeleteInitReg(lpRegLocal,cReg);
        }
        else 
            SetLastError(ERROR_INVALID_PARAMETER);
        
        if (lpRegLocal)
            free((PVOID)lpRegLocal);
        if (SUCCEEDED(hResult) && lpMarsheldReg) {
            hResult = CeCloseCallerBuffer(lpMarsheldReg,(PVOID)lpReg, dwCopiedSize , ARG_I_PTR);
            ASSERT(SUCCEEDED(hResult));
        }
        
    }
    else
        rtHandle = I_ActivateDeviceEx( lpszDevKey!=NULL?iDevKey:NULL, NULL, 0 , lpvParam) ;
    return rtHandle ;
        
}
//
//  @func   BOOL | DeactivateDevice | Deregister a registered device and remove
//                                    it from the active list.
//  @parm   HANDLE | hDevice | handle to registered device, from ActivateDevice
//  @rdesc  Returns TRUE for success, FALSE for failure
//  @comm   DeactivateDevice will DeregisterDevice the specified device and
//  delete the device's active key from HKEY_LOCAL_MACHINE\Drivers\Active.
//  Also the device's removal is announced to the system via a
//  WM_DEVICECHANGE message and through the application notification system.
//          An example would be:<nl>
//                      <tab>DeactivateDevice(h1);<nl>
//                      where h1 was returned from a call to ActivateDevice.
//      @xref <f ActivateDevice> <f RegisterDevice> <f DeregisterDevice> <l Overview.WinCE Device Drivers>
//

BOOL DM_DeactivateDevice(HANDLE hDevice)
{
    return I_DeactivateDevice(hDevice);
}

BOOL DM_GetDeviceInformationByDeviceHandle(HANDLE hDevice, PDEVMGR_DEVICE_INFORMATION pdi)
{
    DWORD dwStatus = ERROR_INVALID_PARAMETER;
    
    // shall we call our internal routine to get the information?
    if(pdi != NULL) {
        dwStatus = I_GetDeviceInformation((fsdev_t *) hDevice, pdi);
    }

    if(dwStatus != ERROR_SUCCESS) {
        SetLastError(dwStatus);
        return FALSE;
    } else {
        return TRUE;
    }
        
}
BOOL EX_DM_GetDeviceInformationByDeviceHandle(HANDLE hDevice, PDEVMGR_DEVICE_INFORMATION pdi)
{
    DWORD dwStatus = ERROR_INVALID_PARAMETER;
    
    // shall we call our internal routine to get the information?
    __try {
        if (pdi!=NULL && pdi->dwSize >= sizeof(DEVMGR_DEVICE_INFORMATION)) {
            DEVMGR_DEVICE_INFORMATION di;
            di.dwSize = sizeof(di);
            dwStatus = I_GetDeviceInformation((fsdev_t *) hDevice, &di);
            if (dwStatus == ERROR_SUCCESS)
                *pdi = di;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_ERROR,(TEXT("DEVICE! Exception EX_DM_GetDeviceInformationByDeviceHandle\r\n")));
        dwStatus = ERROR_INVALID_PARAMETER;
        DEBUGCHK(FALSE);
    }
    

    if(dwStatus != ERROR_SUCCESS) {
        SetLastError(dwStatus);
        return FALSE;
    } else {
        return TRUE;
    }
        
}
// This routine handles power callbacks for all registered devices.
// On suspend, callbacks are invoked in reverse order of driver loading
// so that the last driver loaded (which may depend on an earlier
// driver) gets called first.  On resume, callbacks are loaded in the
// same order as driver load.  The Power Manager is notified last on 
// suspend and first on resume.
void DevMgrPowerOffHandler(BOOL bOff)
{
    fsdev_t *lpdev;

    if (bOff) {
        // Notify drivers of power off in reverse order from that in which
        // they were loaded.
        for (lpdev = (fsdev_t *)g_DevChain.Blink;
             lpdev != (fsdev_t *)&g_DevChain;
             lpdev = (fsdev_t *)lpdev->list.Blink) {
            if (lpdev->fnPowerdn) {
                DEBUGMSG(ZONE_PNP,(TEXT("Calling \'%s\' PowerDown at 0x%x\r\n"), 
                    lpdev->pszDeviceName != NULL ? lpdev->pszDeviceName : L"<Unnamed>", lpdev->fnPowerdn));
                __try {
                    lpdev->fnPowerdn(lpdev->dwData);
                }
                __except(EXCEPTION_EXECUTE_HANDLER) {
                    DEBUGMSG(ZONE_ERROR,(TEXT("DEVICE! Exception calling PowerDown for '%s'\r\n"), lpdev->pszDeviceName != NULL ? lpdev->pszDeviceName : L"<Unnamed>"));
                    DEBUGCHK(FALSE);
                }
            }
        }
        DEBUGMSG(ZONE_PNP,(TEXT("Calling PmPowerHandler Off \r\n")));
        PmPowerHandler(bOff);
    } else {
        // Notify drivers of power on according in the same order as they
        // were loaded.
        DEBUGMSG(ZONE_PNP,(TEXT("Calling PmPowerHandler On \r\n")));
        PmPowerHandler(bOff);
        for (lpdev = (fsdev_t *)g_DevChain.Flink;
             lpdev != (fsdev_t *)&g_DevChain;
             lpdev = (fsdev_t *)lpdev->list.Flink) {
            if (lpdev->fnPowerup) {
                DEBUGMSG(ZONE_PNP,(TEXT("Calling \'%s\' PowerUp at 0x%x\r\n"), 
                    lpdev->pszDeviceName != NULL ? lpdev->pszDeviceName : L"<Unnamed>", lpdev->fnPowerdn));
                __try {
                    lpdev->fnPowerup(lpdev->dwData);
                }
                __except(EXCEPTION_EXECUTE_HANDLER) {
                    DEBUGMSG(ZONE_ERROR,(TEXT("DEVICE! Exception calling PowerUp for '%s'\r\n"), lpdev->pszDeviceName != NULL ? lpdev->pszDeviceName : L"<Unnamed>"));
                    DEBUGCHK(FALSE);
                }
            }
        }
    }
}

// This routine allows the caller to enumerate through loaded devices by
// calling FindFirstFileEx/FindNextFile (using the FindExSearchLimitToDevices 
// extended search flag).  It loads the lpFindFileData structure with 
// the name of the device (e.g., "COM1:").
BOOL DM_GetDeviceByIndex(DWORD dwIndex, LPWIN32_FIND_DATA lpFindFileData) {
    fsdev_t *lpdev;
    BOOL bRet = FALSE;

    // sanity check parameters
    if(lpFindFileData == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    EnterCriticalSection(&g_devcs);
    __try {
        lpdev = (fsdev_t *)g_DevChain.Flink;
        while (lpdev != (fsdev_t *)&g_DevChain) {
            if(lpdev->pszLegacyName != NULL) {
                if(dwIndex == 0) {
                    break;
                } else {
                    dwIndex--;
                }
            }
            lpdev = (fsdev_t *)lpdev->list.Flink;
        }
        if (lpdev != (fsdev_t *)&g_DevChain) {
            lpFindFileData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
            *(__int64 *)&lpFindFileData->ftCreationTime = 0;
            *(__int64 *)&lpFindFileData->ftLastAccessTime = 0;
            *(__int64 *)&lpFindFileData->ftLastWriteTime = 0;
            lpFindFileData->nFileSizeHigh = 0;
            lpFindFileData->nFileSizeLow = 0;
            lpFindFileData->dwOID = 0xffffffff;
            wcsncpy(lpFindFileData->cFileName, lpdev->pszLegacyName, ARRAYSIZE(lpFindFileData->cFileName));
            bRet = TRUE;
        }
    } 
    __except(EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    LeaveCriticalSection(&g_devcs);
    return bRet;
}

// This routine allows the caller to enumerate through the list of
// interfaces associated with a particular device.  The dwIndex parameter
// is used to return the Nth interface on the list, so the caller should
// start at 0 and keep going until no more interfaces are available (in which
// case the routine returns FALSE with an error status of ERROR_NO_MORE_ITEMS.
// For each discovered interface, pClass is filled in with the interface GUID,
// pszNameBuf with the name, and lpdwNameBufSize with the number of bytes
// written into pszNameBuf.  If the buffer size is too small, the routine returns
// FALSE with an error status of ERROR_INSUFFICIENT_BUFFER but fills in lpdwNameBufSize.
// If the pszNameBuf is NULL and lpdwNameBufSize is not, the routine returns
// its normal status but doesn't fill in a name.  Finally, both pszNameBuf and 
// lpdwNameBuf size can be NULL, in which case the routine only returns interface class
// information.
BOOL 
DM_EnumDeviceInterfaces(HANDLE h, DWORD dwIndex, GUID *pClass, LPWSTR pszNameBuf, LPDWORD lpdwNameBufSize)
{
    DWORD dwStatus;
    fsdev_t *lpdev = (fsdev_t *) h;

    if(pClass == NULL || (pszNameBuf != NULL && lpdwNameBufSize == NULL)) {
        dwStatus = ERROR_INVALID_PARAMETER;
        goto done;
    }
    
    EnterCriticalSection(&g_devcs);
    if(!FindDeviceByHandle(lpdev)) {
        dwStatus = ERROR_INVALID_HANDLE;
    } else {
        fsinterface_t *pif = lpdev->pInterfaces;
        while(pif != NULL && dwIndex > 0) {
            pif = pif->pNext;
            dwIndex--;
        }

        // did we find the interface at the requested index?
        if(dwIndex != 0 || pif == NULL) {
            dwStatus = ERROR_NO_MORE_ITEMS;
        } else {
            dwStatus = ERROR_SUCCESS;
            __try {
                DEBUGCHK(pClass != NULL);
                DEBUGCHK(pif->pszName != NULL);
                *pClass = pif->guidClass;
                if(lpdwNameBufSize != NULL) {
                    DWORD dwSize = (wcslen(pif->pszName) + 1) * sizeof(pif->pszName[0]);
                    if(*lpdwNameBufSize < dwSize) {
                        dwStatus = ERROR_INSUFFICIENT_BUFFER;
                    } 
                    else if(pszNameBuf != NULL) {
                        wcscpy(pszNameBuf, pif->pszName);
                    }
                    *lpdwNameBufSize = dwSize;
                }
            } 
            __except(EXCEPTION_EXECUTE_HANDLER) {
                dwStatus = ERROR_INVALID_PARAMETER;
            }
        }
    }
    LeaveCriticalSection(&g_devcs);

done:
    if(dwStatus != ERROR_SUCCESS) {
        SetLastError(dwStatus);
    }
    return(dwStatus == ERROR_SUCCESS ? TRUE : FALSE);
}
BOOL 
EX_DM_EnumDeviceInterfaces(HANDLE h, DWORD dwIndex, GUID *pClass, LPWSTR pszNameBuf, LPDWORD lpdwNameBufSize)
{
    GUID guid;
    LPWSTR ipszNameBuf = NULL;
    DWORD dwBufferSize = 0; 
    BOOL    fStatus = FALSE ;
    if(!(pClass == NULL || (pszNameBuf != NULL && lpdwNameBufSize == NULL))) {
        __try {
            guid = *pClass;
            if (pszNameBuf ) {
                dwBufferSize = *lpdwNameBufSize ;
                dwBufferSize = sizeof(WCHAR) * ( dwBufferSize/sizeof(WCHAR));
                if (dwBufferSize)
                    ipszNameBuf = (LPWSTR)malloc( dwBufferSize );
            }
            fStatus = DM_EnumDeviceInterfaces(h,dwIndex,&guid, ipszNameBuf, &dwBufferSize ) ;
            *pClass = guid ;
            if (dwBufferSize!=0 && ipszNameBuf!=NULL  && pszNameBuf!=NULL ) {
                memcpy(pszNameBuf,ipszNameBuf,dwBufferSize);
                *lpdwNameBufSize = dwBufferSize ;
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            fStatus = FALSE;
            SetLastError(ERROR_INVALID_PARAMETER) ;
        } 
    }
    if (ipszNameBuf)
        free((PVOID) ipszNameBuf ) ;
    return fStatus;
}
// This routine notifies all devices that have open handles owned
// by a particular process that a PSL event is taking place.  The
// PSL event is given by the flags parameter, and currently this
// routine is only called with the DLL_PROCESS_EXITING value.  
// Drivers are notified via IOCTL_PSL_NOTIFY that contains a DEVICE_PSL_NOTIFY 
// structure in the input buffer.
void OpenAddRef(fsopendev_t *fsodev) ;
void OpenDelRef(fsopendev_t *fsodev) ;
static void DevPSLNotify (DWORD flags, HPROCESS proc, HTHREAD thread) {
    fsopendev_t *fsodev;
    int         icalls, i;
    fsopendev_t *h[20];
    fsopendev_t **ph;

    DEVICE_PSL_NOTIFY pslPacket;

    EnterCriticalSection(&g_devcs);

    fsodev = g_lpOpenDevs;
    icalls = 0;

    // figure out how many handles this process owns
    while (fsodev) {
        if (fsodev->hProc == proc)
            ++icalls;
        fsodev = fsodev->nextptr;
    }

    // Do we need to allocate a handle buffer?  We can manage
    // up to ARRAYSIZE(h) handles without allocation.
    if (icalls < sizeof(h) / sizeof(h[0]))
        ph = h;
    else {
        // allocate an array
        ph = (fsopendev_t **)LocalAlloc (LMEM_FIXED, icalls * sizeof(fsopendev_t *));
        if (ph == NULL) { // Run out memory. Do as much as we can
            ph = h;
            icalls = sizeof(h) / sizeof(h[0]);
        };
    };

    // now copy each device's KHandle -- we'll use this to make the 
    // IOCTL call
    i = 0;
    fsodev = g_lpOpenDevs;
    while (fsodev && i < icalls) {
        if (fsodev->hProc == proc) {
            ph[i++] = fsodev;
            OpenAddRef(fsodev);
        }
        fsodev = fsodev->nextptr;
    }

    LeaveCriticalSection (&g_devcs);

    // format the notification packet
    pslPacket.dwSize  = sizeof(pslPacket);
    pslPacket.dwFlags = flags;
    pslPacket.hProc   = proc;
    pslPacket.hThread = thread;

    // Now call all the devices using their cached KHandles.  If the
    // handle is no longer valid the kernel will reject our call.
    for (i = 0 ; i < icalls ; ++i) {
        DM_DevDeviceIoControl(ph[i], IOCTL_PSL_NOTIFY, (LPVOID)&pslPacket, sizeof(pslPacket), NULL, 0, NULL, NULL);
        OpenDelRef(ph[i]);
    }

    // free the handle buffer if it was one we allocated
    if (ph != h)
        LocalFree (ph);
}

// This routine receives PSL notifications for the Device Manager and 
// Power Manager PSLs.
void DM_DevProcNotify(DWORD flags, HPROCESS proc, HTHREAD thread) {
    switch (flags) {
        case DLL_MEMORY_LOW:        // memory is getting low
            CompactAllHeaps ();
            return;
        case DLL_PROCESS_EXITING:   // process appears to have threads blocked in a PSL call
            if ((OEM_CERTIFY_TRUST != PSLGetCallerTrust()) && (proc != GetCallerProcess())) {
                return;
            }
            DevPSLNotify (flags, proc, thread);
            break;
        case DLL_SYSTEM_STARTED:    // all applications launched at system boot time
            if (OEM_CERTIFY_TRUST != PSLGetCallerTrust()) {
                return;
            }
            // Eventually we'll want to signal all drivers that the system has
            // initialized.  For now do what's necessary.
            DevloadPostInit();
            break;
    }

    // notify the power manager
    PM_Notify(flags, proc, thread);

    return;
}

//
//  @func   BOOL | CeResyncFilesys | Cause a file system driver to remount a device because of media change.
//  @parm   HANDLE | hDevice | handle to registered device from RegisterDevice
//  @rdesc  Returns TRUE for success, FALSE for failure
//  @comm   CeResyncFilesys is used by a device driver to indicate to its associated file
//                  system driver that a new volume/media has been inserted. The handle can be retrieved from
//                  the device's active key in the registry or from the p_hDevice field of the POST_INIT_BUF
//                  structure passed to the post-initialization IOCTL.
//                  
//
BOOL DM_CeResyncFilesys(fsdev_t *lpdev)
{
    DEVMGR_DEVICE_INFORMATION di;

    // get device name information using GetDeviceInformationByDeviceHandle()
    memset(&di, 0, sizeof(di));
    di.dwSize = sizeof(di);
    if(I_GetDeviceInformation(lpdev, &di) != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ERROR,(L"DEVICE: CeResyncFilesys - invalid handle\n"));
        return FALSE;   // Invalid device handle
    }

    if ((lpdev->pszLegacyName != NULL) &&
        (GetFileAttributesW( L"\\StoreMgr") != -1) &&
        (GetFileAttributesW( L"\\StoreMgr") & FILE_ATTRIBUTE_TEMPORARY)) {
            WCHAR szFileName[MAX_PATH];
            HRESULT hr;
            
            hr = StringCchCopyW(szFileName, ARRAYSIZE(szFileName), L"\\StoreMgr\\");
            hr = StringCchCatW(szFileName, ARRAYSIZE(szFileName), lpdev->pszLegacyName);
            return(MoveFile( szFileName, szFileName));
    }    
    
    return FALSE;
}


