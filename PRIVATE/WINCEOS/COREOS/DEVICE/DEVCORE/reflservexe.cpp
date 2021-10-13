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
#include <winsock.h>
#include <mservice.h>

#include <promgr.hpp>
#include <reflector.h>
#include <devzones.h>

// Enumerates all running services in services.exe ($services name space)
extern "C" BOOL DM_EnumServices(ServicesExeEnumServicesIntrnl *pServiceEnumInfo) {
    if (! pServiceEnumInfo) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // FindUserProcByID does an AddRef() if the process is found.
    UserDriverProcessor *pServicesExe = g_pUDPContainer->FindUserProcByID(SERVICEDS_EXE_DEFAULT_PROCESSOR_ID, FALSE);
    if (! pServicesExe) {
        // Services.exe is not running.
        SetLastError(ERROR_SERVICE_NOT_ACTIVE);
        return FALSE;
    }

    PVOID pServiceEnumInfoMapped = 
         VirtualAllocCopyEx(GetCallerProcess(),(HANDLE)pServicesExe->GetUserDriverPorcessorInfo().dwProcessId,
                    pServiceEnumInfo,sizeof(*pServiceEnumInfo),PAGE_READWRITE);

    if (! pServiceEnumInfoMapped) {
        DEBUGMSG(ZONE_ERROR,(L"VirtualAllocCopyEx failed on EnumServices ptr 0x%08x, error = 0x%08x\r\n",
                               pServiceEnumInfo, GetLastError()));
        pServicesExe->DeRef();
        return 0;
    }

    FNIOCTL_PARAM fnIoctlParam;
    memset(&fnIoctlParam,0,sizeof(fnIoctlParam));
    fnIoctlParam.dwCallerProcessId = GetCallerProcessId();
    fnIoctlParam.dwIoControlCode = IOCTL_SERVICE_INTERNAL_SERVICE_ENUM;
    fnIoctlParam.nInBufSize = sizeof(*pServiceEnumInfo);
    fnIoctlParam.lpInBuf = pServiceEnumInfoMapped;

    BOOL ret = pServicesExe->SendIoControl(IOCTL_SERVICES_IOCTL, &fnIoctlParam, sizeof(fnIoctlParam), NULL, 0, NULL);

#ifdef ARM
    if (IsVirtualTaggedCache () && (DWORD)pServiceEnumInfo >= VM_SHARED_HEAP_BASE ) {  // We only need flush it when address is in kernel. 
        CacheRangeFlush (pServiceEnumInfo, sizeof(*pServiceEnumInfo), CACHE_SYNC_DISCARD);
    }
#endif

    VirtualFreeEx((HANDLE)pServicesExe->GetUserDriverPorcessorInfo().dwProcessId,
                (LPVOID)((DWORD)pServiceEnumInfoMapped & ~VM_BLOCK_OFST_MASK),0, MEM_RELEASE) ;    

    pServicesExe->DeRef();
    return ret;
}


extern "C" BOOL EX_DM_EnumServices(ServicesExeEnumServicesIntrnl *pServiceEnumInfo) {
    return DM_EnumServices(pServiceEnumInfo);
}

