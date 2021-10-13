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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef BOOL (*LPReceiveInvokeRequestImpl)(__in DWORD retCode, PBYTE pReqBuf, DWORD cbReqBuf, PDWORD pcbReqBuf);
typedef BOOL (*LPInitializeReceiveInvokeRequestImpl)();
typedef BOOL (*LPCancelReceiveInvokeRequestImpl)();
typedef BOOL (*LPSetRawControlResponseImpl)(DWORD hRequest, DWORD dwHttpStatus, PCWSTR pszResp);
typedef BOOL (*LPUpnpAddDeviceImpl)(HANDLE hOwner, UPNPDEVICEINFO* pDevInfo);
typedef BOOL (*LPUpnpRemoveDeviceImpl)(PCWSTR pszDeviceName);
typedef void (*LPUpnpCleanUpProcImpl)(HANDLE hOwner);
typedef BOOL (*LPUpnpPublishDeviceImpl)(PCWSTR pszDeviceName);
typedef BOOL (*LPUpnpUnpublishDeviceImpl)(PCWSTR pszDeviceName);
typedef BOOL (*LPSsdpStartup)();
typedef VOID (*LPSsdpCleanup)();
typedef BOOL (*LPUpnpServiceStart)();
typedef BOOL (*LPUpnpServiceStop)();
typedef BOOL (*LPUpnpGetUDNImpl)(PCWSTR pszDeviceName, PCWSTR pszTemplateUDN, PWSTR UDNBuf, DWORD cchBuf, PDWORD pcchBuf);
typedef BOOL (*LPUpnpGetSCPDPathImpl)(PCWSTR pszDeviceName, PCWSTR pszUDN, PCWSTR pszServiceId, PWSTR SCPDFilePath, DWORD cchSCPDFilePath);
typedef BOOL (*LPUpnpSubmitPropertyEventImpl)(PCWSTR pszDeviceName, PCWSTR pszUDN, PCWSTR pszServiceId, UPNPPARAM* pArgs, DWORD cArgs);
typedef BOOL (*LPUpnpUpdateEventedVariablesImpl)(PCWSTR pszDeviceName, PCWSTR pszUDN, PCWSTR pszServiceId, UPNPPARAM* pArgs, DWORD cArgs);

struct UPNPApi
{
    LPSsdpStartup                           lpLPSsdpStartup;
    LPSsdpCleanup                           lpSsdpCleanup;
    LPUpnpServiceStart                      lpUpnpServiceStart;
    LPUpnpServiceStop                       lpUpnpServiceStop;
    LPInitializeReceiveInvokeRequestImpl    lpInitializeReceiveInvokeRequestImpl;
    LPReceiveInvokeRequestImpl              lpReceiveInvokeRequestImpl;
    LPCancelReceiveInvokeRequestImpl        lpCancelReceiveInvokeRequestImpl;
    LPSetRawControlResponseImpl             lpSetRawControlResponseImpl;
    LPUpnpAddDeviceImpl                     lpUpnpAddDeviceImpl;
    LPUpnpRemoveDeviceImpl                  lpUpnpRemoveDeviceImpl;
    LPUpnpCleanUpProcImpl                   lpUpnpCleanUpProcImpl;
    LPUpnpPublishDeviceImpl                 lpUpnpPublishDeviceImpl;
    LPUpnpUnpublishDeviceImpl               lpUpnpUnpublishDeviceImpl;
    LPUpnpGetUDNImpl                        lpUpnpGetUDNImpl;
    LPUpnpGetSCPDPathImpl                   lpUpnpGetSCPDPathImpl;
    LPUpnpSubmitPropertyEventImpl           lpUpnpSubmitPropertyEventImpl;
    LPUpnpUpdateEventedVariablesImpl        lpUpnpUpdateEventedVariablesImpl;
};

void SetUPNPApi(UPNPApi *pApi);
UPNPApi *GetUPNPApi();

#ifdef __cplusplus
}
#endif

