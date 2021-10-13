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

// Abstract: Location framework user-mode DLL - marshalls API calls
// into location framework service in services.exe.


#include <windows.h>
#include <lfApi.h>
#include <svsutil.hxx>
#include <..\inc\locIoctl.hpp>

ce::psl_proxy<>* pProxy;

extern "C" HLOCATION LocationOpen(DWORD version, VOID *pReserved, DWORD flags) {
    HLOCATION hRet = 0;

    // PSL marshaller class returns GetLastError() code, so add the return
    // HANDLE as an extra paramater for internal calls.
    DWORD err = pProxy->call(IOCTL_LF_OPEN,version,pReserved,flags,&hRet);
    if (err == ERROR_SUCCESS) {
        ASSERT(hRet != NULL);
        return hRet;
    }

    // PSL marshaller already has SetLastError to err code, so don't repeat here.
    return NULL;
}

extern "C" DWORD LocationClose(HLOCATION hLocation) {
    return pProxy->call(IOCTL_LF_CLOSE,hLocation);
}

extern "C" DWORD LocationRegisterForReport(HLOCATION hLocation, HANDLE hNewLocationReport,
                                 HANDLE hStateChangeEvent, REFGUID reportType, 
                                 DWORD flags)
{
    return pProxy->call(IOCTL_LF_REGISTER_FOR_REPORT,hLocation, hNewLocationReport,
                                 hStateChangeEvent, &reportType, flags);
}

extern "C" DWORD LocationUnRegisterForReport(HLOCATION hLocation, REFGUID reportType, DWORD flags) 
{
    return pProxy->call(IOCTL_LF_UN_REGISTER_FOR_REPORT, hLocation, &reportType, flags);
}

// If report appears to have been generated 10 seconds or more in the future,
// then clean up its filetime.  If report is farther ahead in future than this
// then assume it's not the result of clock skew
const __int64 g_maxFutureSkew = (10 * 1000 * SVS_FILETIME_TO_MILLISECONDS);

// Potentially fix up creationTime of report.  The WinCE real-time clock
// has the strange behavior that different processes calling GetCurrentFT
// to retrieve its value may get unexpectedly different values.  In particular,
// it is possible that the time the report was generated as marked in
// services.exe appears to this user-mode process's clock to have been 
// generated in the future!  If the report is a little in the future, using
// this app's clock put the time to be reasonable.
void FixupFileTimeIfNeeded(FILETIME *pReportTime) {
    __int64 currentTime;
    GetCurrentFT((FILETIME*)&currentTime);
    __int64 reportAge = currentTime - *((__int64*)(pReportTime));

    if ((reportAge < 0) && (g_maxFutureSkew + reportAge >= 0)) {
        // The report's creationTime is 10 seconds or less ahead in the future 
        // compared to this process's RTC.  Use this proc's RTC as creationTime
        // in this case so as not to confuse application
        memcpy(pReportTime,&currentTime,sizeof(FILETIME));
    }
}

extern "C" DWORD LocationGetReport(HLOCATION hLocation, REFGUID reportType, DWORD maximumAge, LOCATION_REPORT *pLocationReport,
                        DWORD *pcbLocationReport, DWORD flags)
{
    if (NULL == pcbLocationReport)
        return ERROR_INVALID_PARAMETER;

    DWORD err = pProxy->call(IOCTL_LF_GET_REPORT,hLocation,&reportType, maximumAge, 
                             ce::psl_buffer(pLocationReport,*pcbLocationReport),
                             pcbLocationReport, flags);

    if (err != ERROR_SUCCESS)
        return err;

    LOCATION_REPORT_BASE *pReportBase = (LOCATION_REPORT_BASE*)pLocationReport;
    FixupFileTimeIfNeeded(&pReportBase->creationTime);
    return ERROR_SUCCESS;
}

extern "C" DWORD LocationGetServiceState(HLOCATION hLocation, LOCATION_SERVICE_STATE *pServiceState) 
{
    return pProxy->call(IOCTL_LF_GET_SERVICE_STATE,hLocation, pServiceState);
}

extern "C" DWORD LocationGetPluginInfoForReport(HLOCATION hLocation, REFGUID reportType, PLUGIN_STATE *pPluginState, GUID *pPluginGuid)
{
    return pProxy->call(IOCTL_LF_GET_PLUGIN_INFO_FOR_REPORT,hLocation, &reportType, pPluginState,pPluginGuid);
}

extern "C" DWORD LocationGetProvidersInfo(HLOCATION hLocation, PROVIDER_INFORMATION *pProviders, DWORD *pcbBuffer)
{
    if (pcbBuffer == NULL)
        return ERROR_INVALID_PARAMETER;

    DWORD err = pProxy->call(IOCTL_LF_GET_PROVIDERS_INFO, hLocation, 
                        ce::psl_buffer(pProviders, *pcbBuffer), pcbBuffer);

    if (err != ERROR_SUCCESS)
        return err;

    // Number of bytes returned should be even multiple of PROVIDER_INFORMATION
    ASSERT((*pcbBuffer) % sizeof(PROVIDER_INFORMATION) == 0);
    DWORD numProviders = (*pcbBuffer) / sizeof(PROVIDER_INFORMATION);

    for (DWORD i = 0; i < numProviders; i++)
        FixupFileTimeIfNeeded(&pProviders[i].pluginInfo.lastUpdate);

    return ERROR_SUCCESS;
}

extern "C" DWORD LocationGetResolversInfo(HLOCATION hLocation, RESOLVER_INFORMATION *pResolvers, DWORD *pcbBuffer)
{
    if (pcbBuffer == NULL)
        return ERROR_INVALID_PARAMETER;

    DWORD err = pProxy->call(IOCTL_LF_GET_RESOLVERS_INFO, hLocation, 
                        ce::psl_buffer(pResolvers, *pcbBuffer),pcbBuffer);

    if (err != ERROR_SUCCESS)
        return err;

    // Number of bytes returned should be even multiple of RESOLVER_INFORMATION
    ASSERT((*pcbBuffer) % sizeof(RESOLVER_INFORMATION) == 0);
    DWORD numResolvers = (*pcbBuffer) / sizeof(RESOLVER_INFORMATION);

    for (DWORD i = 0; i < numResolvers; i++)
        FixupFileTimeIfNeeded(&pResolvers[i].pluginInfo.lastUpdate);

    return ERROR_SUCCESS;
}

extern "C" HLOCATIONPLUGIN LocationPluginOpen(HLOCATION hLocation, REFGUID pluginGuid) {
    HLOCATIONPLUGIN hRet = 0;

    // PSL marshaller class returns GetLastError() code, so add the return
    // HANDLE as an extra paramater for internal calls.
    DWORD err = pProxy->call(IOCTL_LF_PLUGIN_OPEN,hLocation,&pluginGuid,&hRet);
    if (err == ERROR_SUCCESS) {
        ASSERT(hRet != NULL);
        return hRet;
    }

    return NULL;
}

extern "C" DWORD LocationPluginIOCTL(HLOCATION hLocation, HLOCATIONPLUGIN hPlugin, 
                                     DWORD dwCode, BYTE *pbIn, DWORD cbIn, 
                                     BYTE *pbOut, DWORD *pcbOut) 
{
    
    return pProxy->call(IOCTL_LF_PLUGIN_IOCTL,hLocation,hPlugin,dwCode,
                        ce::psl_buffer(pbIn,cbIn),
                        ce::psl_buffer(pbOut, pcbOut ? *pcbOut : 0),
                        pcbOut);
}

extern "C" DWORD LocationPluginClose(HLOCATION hLocation, HLOCATIONPLUGIN hPlugin) {
    return pProxy->call(IOCTL_LF_PLUGIN_CLOSE,hLocation,hPlugin);
}

extern "C" BOOL WINAPI DllEntry(HANDLE hInstDll, DWORD fdwReason, LPVOID lpvReserved)
{
    switch(fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls((HMODULE)hInstDll);
            pProxy = new ce::psl_proxy<>(LOC_DEV_NAME, IOCTL_LF_INVOKE, NULL);
            return (pProxy != NULL);
        break;

        case DLL_PROCESS_DETACH:
            if (pProxy)
                delete pProxy;
        break;
    }

    return TRUE;
}


