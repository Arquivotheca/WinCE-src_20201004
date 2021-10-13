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



#include <windows.h>
#include <guiddef.h> // For LPGUID needed by winsock2.h
#include <winsock2.h>
#include <iphlpapi.h>
#include <devload.h> // needed to map ActivateDevice
#include <cedrv_guid.h>
#include <cesvsbus.h>

#include <service.h>
#include <svsutil.hxx>
#include <mservice.h>

//
// Services.exe external exports.  The implementations of many
// of these functions is now processed in the kernel mode device
// driver handler for both services.exe and udevice.exe.
// 


// Coredll.dll call that implements GetDeviceInformationBy*Handle.  Call
// directly rather than going through import table due to various compile-time
// dependencies in the GetDeviceInformationBy*Handle_Trap.
extern "C" BOOL xxx_GetDeviceInformationByFileHandle(HANDLE hDevice, PDEVMGR_DEVICE_INFORMATION pdi);
extern "C" BOOL xxx_GetDeviceInformationByDeviceHandle(HANDLE hDevice, PDEVMGR_DEVICE_INFORMATION pdi);

const WCHAR g_servicesRegKey[]  = L"services\\";
const DWORD g_servicesRegKeyLen = SVSUTIL_CONSTSTRLEN(g_servicesRegKey);

HANDLE OpenServiceBusHandle()
{
    HANDLE hOpenServieBusHandle = NULL;
    DEVMGR_DEVICE_INFORMATION di = {0};
    GUID Guid;
    HANDLE hServices = NULL;
    di.dwSize = sizeof(di);
    if (svsutil_ReadGuidW(CE_SERVICE_BUS_GUID,&Guid,TRUE)) {
        hServices= FindFirstDevice(DeviceSearchByGuid,&Guid,&di);
    }
    if (hServices && hServices!=INVALID_HANDLE_VALUE) {
        LPCTSTR lpServiceName = (di.szBusName[0]!=0? di.szBusName: 
            (di.szDeviceName[0]!=0? di.szDeviceName: di.szLegacyName));
        if (*lpServiceName) { 
            hOpenServieBusHandle = CreateFile(lpServiceName,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_WRITE,
                NULL,OPEN_EXISTING,FILE_ATTRIBUTE_SYSTEM,NULL);
        }
        FindClose(hServices);
    }
    return hOpenServieBusHandle;

}
extern "C"
HANDLE xxx_ActivateService(LPCWSTR lpszDevKey, DWORD dwClientInfo) {
    // Acts exactly like ActivateDevice, except we have to fixup the 
    // base registry key at this layer.  ActivateService(L"HTTPD",...)
    // expects HKLM\Services\HTTPD to be activated.

    BUS_SVS_ACTIVATE_SERVICE busServ = {
        sizeof(BUS_SVS_ACTIVATE_SERVICE),dwClientInfo                     
    };
    if (FAILED(StringCchPrintf(busServ.sRegPath,_countof(busServ.sRegPath),TEXT("services\\%s"),lpszDevKey))){
        SetLastError(ERROR_BUFFER_OVERFLOW);
        return NULL;
    }

    // Find First Servise Bus Driver.
    HANDLE hReturnHandle = NULL;
    HANDLE hOpenService = OpenServiceBusHandle();
    if (hOpenService) {
        if (!DeviceIoControl(hOpenService, IOCTL_BUS_SVS_ACTIVATE, &busServ,sizeof(busServ),&hReturnHandle,sizeof(hReturnHandle),NULL,NULL)) {
            hReturnHandle = NULL;
        }
        CloseHandle(hOpenService);
    }
    return hReturnHandle;
    //return ActivateDeviceEx_Trap(serviceName, (LPCVOID)dwClientInfo, 0, NULL);
}

extern "C"
HANDLE xxx_RegisterService(LPCWSTR lpszType, DWORD dwIndex, LPCWSTR lpszLib, DWORD dwInfo) {
    // No longer supported.  Use ActivateDeviceEx instead
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return NULL;
}

extern "C"
BOOL xxx_DeregisterService(HANDLE hDevice) {
    // Since ActivateService maps to ActivateDevice, do same mapping here.
    // DeregisterService always could take ActivateService AND RegisterService
    // returned handles, so no need for a DeactivateService API.
    BOOL fReturn = FALSE;    
    HANDLE hOpenService = OpenServiceBusHandle();
    if (hOpenService) {
        fReturn = DeviceIoControl(hOpenService, IOCTL_BUS_SVS_DEACTIVATE, &hDevice,sizeof(hDevice),NULL,0,NULL,NULL);
        DWORD dwFirstError = GetLastError();
        if (!fReturn) { // This shouldn't be activation handle, try file handle.
            // Legacy apps to unload a service will do h=GetServiceHandle("...");
            // and then DeregisterService(h);  In CE 6.0 and above, GetServiceHandle 
            // no longer returns an ActivateService() style handle but instead a filesys
            // handle.  This small change will support BC with those apps, since
            // we can get the service handle from the filesys handle.

            DEVMGR_DEVICE_INFORMATION devInfo;
            devInfo.dwSize = sizeof(devInfo);

            if (xxx_GetDeviceInformationByFileHandle(hDevice,&devInfo)) {
                if (devInfo.szBusName[0]!=0) {
                    fReturn = DeviceIoControl(hOpenService, IOCTL_BUS_SVS_DEACTIVATE_BY_NAME, devInfo.szBusName,sizeof(devInfo.szBusName),NULL,0,NULL,NULL);
                }
                else
                    SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
            }
            else {
                // some code used to pass a device handle to DeregisterServer in CE6, 
                // for BC we will still handle this case
                if (xxx_GetDeviceInformationByDeviceHandle(hDevice,&devInfo)) {
                    if (devInfo.szBusName[0]!=0) {
                        RETAILMSG(1, (L"WARNING: Passing a device handle to DeregisterService() is deprecated, use a file handle from CreateFile() instead!!"));
                        fReturn = DeviceIoControl(hOpenService, IOCTL_BUS_SVS_DEACTIVATE_BY_NAME, devInfo.szBusName,sizeof(devInfo.szBusName),NULL,0,NULL,NULL);
                    }
                    else
                        SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
                } else
                    SetLastError(dwFirstError);
            }
        }
        CloseHandle(hOpenService);
    }
    return fReturn;
}

extern "C"
void xxx_CloseAllServiceHandles(HPROCESS proc) {
    // No longer supported. 
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
}

extern "C"
HANDLE xxx_CreateServiceHandle(LPCWSTR lpNew, DWORD dwAccess, DWORD dwShareMode, HPROCESS hProc) {
    // No longer supported. 
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

extern "C"
BOOL xxx_GetServiceByIndex(DWORD dwIndex, LPWIN32_FIND_DATA lpFindFileData) {
    // No longer supported. 
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

extern "C"
BOOL xxx_ServiceIoControl(HANDLE hService, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) {
    // Service handles are no longer exposed to apps - apps always deal with
    // filesystem based handles now.  See xxx_GetServiceHandle for details.
    // So use DeviceIoControl rather than pre CE 6.0 ServiceIoControl call.
    RETAILMSG (1, (L"ServiceIoControl is deprecated, use DeviceIoControl instead!!\r\n"));
    return DeviceIoControl(hService, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
}

extern "C"
BOOL xxx_ServiceAddPort(HANDLE hService, SOCKADDR *pSockAddr, int cbSockAddr, int iProtocol,WCHAR *szRegistryPath) {
    ServicesExeAddPort servAdd;

    if (szRegistryPath) {
        // The registry write path is no longer supported in CE 6.0 and above
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    servAdd.pSockAddr  = pSockAddr;
    servAdd.cbSockAddr = cbSockAddr;
    servAdd.iProtocol  = iProtocol;

    return DeviceIoControl(hService, IOCTL_SERVICE_INTERNAL_SERVICE_ADD_PORT, &servAdd, sizeof(servAdd), NULL, 0, NULL, NULL);
}

extern "C"
BOOL xxx_ServiceUnbindPorts(HANDLE hService) {
    return DeviceIoControl(hService, IOCTL_SERVICE_INTERNAL_SERVICE_UNBIND_PORTS, NULL, 0, NULL, 0, NULL, NULL);
}

extern "C"
BOOL xxx_EnumServices(PBYTE pBuffer, DWORD *pdwServiceEntries, DWORD *pdwBufferLen) {
    BOOL fReturn = FALSE;
    HANDLE hOpenService = OpenServiceBusHandle();
    if (hOpenService) {
        ServicesExeEnumServicesIntrnl enumServInfo;
        enumServInfo.pBuffer = pBuffer;
        enumServInfo.pdwBufferLen = pdwBufferLen;
        enumServInfo.pdwServiceEntries = pdwServiceEntries;

        fReturn = DeviceIoControl(hOpenService, IOCTL_BUS_SVS_ENUM_SERVICES,NULL,0,&enumServInfo,sizeof(enumServInfo),NULL,NULL);
        CloseHandle(hOpenService);
    }
    return fReturn;
}

extern "C"
HANDLE xxx_GetServiceHandle(LPWSTR szPrefix, LPWSTR szDllName, DWORD *pdwDllBuf) {
    // In pre-CE 6.0 kernel, there was the concept of a service handle which
    // was a non-ref counted reference to a service itself (didn't involve
    // filesystem).  In CE 6.0 and above, the service handle concept has been 
    // removed because of kernel rework.  In order to maintain back-compat with
    // older apps, do CreateFile() to get the service handle.

    // Since there is no CloseServiceHandle() (service handles weren't ref-counted)
    // this will cause a leak of the handle when old apps use this.  This means
    // that the service will never be able to be unloaded.  However since
    // most services are stopped & restarted via IOCTL_SERVICE_START/IOCTL_SERVICE_STOP,
    // this won't affect them.
    if (szDllName || pdwDllBuf) {
        // Reading the DLL name directly from this API call is no longer supported.
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    RETAILMSG (1, (L"GetServiceHandle is deprecated, new code should not use this!!\r\n"));
    return CreateFile(szPrefix, GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
}

extern "C"
BOOL xxx_ServiceClosePort(HANDLE hService, SOCKADDR *pSockAddr, int cbSockAddr, int iProtocol, BOOL fRemoveFromRegistry) {
    ServicesExeClosePort servClose;

    if (fRemoveFromRegistry) {
        // Registry writing/deleting is no longer supported.
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    servClose.pSockAddr  = pSockAddr;
    servClose.iProtocol  = iProtocol;
    servClose.cbSockAddr = cbSockAddr;

    return DeviceIoControl(hService, IOCTL_SERVICE_INTERNAL_SERVICE_CLOSE_PORT, &servClose, sizeof(servClose), NULL, 0, NULL, NULL);
}
