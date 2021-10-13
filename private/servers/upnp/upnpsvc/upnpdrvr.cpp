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
#include "pch.h"
#pragma hdrstop

#include <devsvc.h>
#include <ssdpioctl.h>
#include <service.h>
#include <svsutil.hxx>
#include <upnpdcall.h>
#include <cetls.h>

extern SVSThreadPool* g_pThreadPool;

LONG  g_fState;                     // Current Service State (running, stopped, starting, shutting down, unloading)

extern CRITICAL_SECTION g_csUPNP;
extern CRITICAL_SECTION g_csSSDP;
extern LIST_ENTRY g_DevTreeList;


#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("UPNPSVC"), {
    TEXT("Misc"), TEXT("Init/Register"), TEXT("Announce"), TEXT("Events"),
    TEXT("Socket"),TEXT("Search"), TEXT("Parser"),TEXT("Timer"),
    TEXT("Cache"),TEXT("Control"),TEXT(""),TEXT(""),
    TEXT(""),TEXT(""),TEXT("Trace"),TEXT("Error") },
    0x00008000
};
#endif

// UPNP direct callable API
static UPNPApi g_UPNPApi = {0};
BOOL InitializeReceiveInvokeRequestImpl();
BOOL ReceiveInvokeRequestImplInternal(__in DWORD retCode, PBYTE pReqBuf, DWORD cbReqBuf, PDWORD pcbReqBuf);
BOOL CancelReceiveInvokeRequestImplInternal();
BOOL SetRawControlResponseImplInternal(DWORD hRequest, DWORD dwHttpStatus, PCWSTR pszResp);
BOOL UpnpAddDeviceImplInternal(HANDLE hOwner, UPNPDEVICEINFO* pDevInfo);
BOOL UpnpRemoveDeviceImplInternal(PCWSTR pszDeviceName);
void UpnpCleanUpProc(HANDLE hOwner);
BOOL UpnpPublishDeviceImplInternal(PCWSTR pszDeviceName);
BOOL UpnpUnpublishDeviceImplInternal(PCWSTR pszDeviceName);
BOOL WINAPI SsdpStartup();
VOID WINAPI SsdpCleanup();
BOOL UpnpServiceStart();
BOOL UpnpServiceStop();
BOOL UpnpGetUDNImplInternal(PCWSTR pszDeviceName, PCWSTR pszTemplateUDN, PWSTR UDNBuf, DWORD cchBuf, PDWORD pcchBuf);
BOOL UpnpGetSCPDPathImplInternal(PCWSTR pszDeviceName, PCWSTR pszUDN, PCWSTR pszServiceId, PWSTR SCPDFilePath, DWORD cchSCPDFilePath);
BOOL UpnpSubmitPropertyEventImplInternal(PCWSTR pszDeviceName, PCWSTR pszUDN, PCWSTR pszServiceId, UPNPPARAM* pArgs, DWORD cArgs);
BOOL UpnpUpdateEventedVariablesImplInternal(PCWSTR pszDeviceName, PCWSTR pszUDN, PCWSTR pszServiceId, UPNPPARAM* pArgs, DWORD cArgs);

extern "C"
BOOL
WINAPI
DllMain(IN HANDLE DllHandle,
        IN ULONG Reason,
        IN PVOID Context OPTIONAL)
{
    switch (Reason)
    {
    case DLL_PROCESS_ATTACH:

        if(!UpnpHeapCreate())
        {
            return FALSE;
        }

        if (!CeTlsProcessAttach())
        {
            UpnpHeapDestroy();
            return FALSE;
        }

        InitializeCriticalSection(&g_csUPNP);
        InitializeCriticalSection(&g_csSSDP);
        // Initialize debugging
        DEBUGREGISTER((HMODULE)DllHandle);
        g_fState = SERVICE_STATE_UNINITIALIZED;
        svsutil_Initialize();

        g_UPNPApi.lpLPSsdpStartup = SsdpStartup;
        g_UPNPApi.lpSsdpCleanup = SsdpCleanup;
        g_UPNPApi.lpUpnpServiceStart = UpnpServiceStart;
        g_UPNPApi.lpUpnpServiceStop = UpnpServiceStop;
        g_UPNPApi.lpInitializeReceiveInvokeRequestImpl = InitializeReceiveInvokeRequestImpl;
        g_UPNPApi.lpReceiveInvokeRequestImpl = ReceiveInvokeRequestImplInternal;
        g_UPNPApi.lpCancelReceiveInvokeRequestImpl = CancelReceiveInvokeRequestImplInternal;
        g_UPNPApi.lpSetRawControlResponseImpl = SetRawControlResponseImplInternal;
        g_UPNPApi.lpUpnpAddDeviceImpl = UpnpAddDeviceImplInternal;
        g_UPNPApi.lpUpnpRemoveDeviceImpl = UpnpRemoveDeviceImplInternal;
        g_UPNPApi.lpUpnpCleanUpProcImpl = UpnpCleanUpProc;
        g_UPNPApi.lpUpnpPublishDeviceImpl = UpnpPublishDeviceImplInternal;
        g_UPNPApi.lpUpnpUnpublishDeviceImpl = UpnpUnpublishDeviceImplInternal;
        g_UPNPApi.lpUpnpGetUDNImpl = UpnpGetUDNImplInternal;
        g_UPNPApi.lpUpnpGetSCPDPathImpl = UpnpGetSCPDPathImplInternal;
        g_UPNPApi.lpUpnpSubmitPropertyEventImpl = UpnpSubmitPropertyEventImplInternal;
        g_UPNPApi.lpUpnpUpdateEventedVariablesImpl = UpnpUpdateEventedVariablesImplInternal;

        SetUPNPApi(&g_UPNPApi);

        break; 

    case DLL_THREAD_ATTACH:
        CeTlsThreadAttach();
        break;

    case DLL_PROCESS_DETACH:
        CeTlsProcessDetach();
        SetUPNPApi(NULL);

        svsutil_DeInitialize();
        DeleteCriticalSection(&g_csUPNP); 
        DeleteCriticalSection(&g_csSSDP);
        UpnpHeapDestroy();
        break;

    case DLL_THREAD_DETACH:
        CeTlsThreadDetach();
        break;
    }

    return TRUE;
}



#define UPNP_OPEN_SIG      0x504E5055    // "UPNP"
#define UPNP_CLOSE_SIG    0x244E5055    // "UPN$"

typedef struct {
    DWORD  sig;
    LIST_ENTRY linkage;
} UPNP_OPEN;

static LIST_ENTRY g_UpnpOpenList;

extern "C" {
DWORD WINAPI UPP_Init(DWORD dwInfo );
BOOL WINAPI UPP_Deinit( DWORD dwData );
HANDLE WINAPI UPP_Open( DWORD dwData, DWORD dwAccessCode, DWORD dwShareMode);
BOOL WINAPI UPP_Close(DWORD dwOpenData);
DWORD WINAPI UPP_Read(DWORD dwOpenData, LPVOID pBuf, DWORD len);
DWORD WINAPI UPP_Write( DWORD dwOpenData, LPCVOID pBuf, DWORD len);
DWORD WINAPI UPP_Seek(DWORD dwOpenData, long pos, DWORD type);
void WINAPI UPP_PowerUp(DWORD dwData);
void WINAPI UPP_PowerDown(DWORD dwData);
BOOL WINAPI UPP_IOControl(
                                DWORD  dwOpenData,
                                DWORD  dwCode,
                                PBYTE  pBufIn,
                                DWORD  dwLenIn,
                                PBYTE  pBufOut,
                                DWORD  dwLenOut,
                                PDWORD pdwActualOut);
};

// declared in subs.cpp
extern BOOL  (*pfSubscribeCallback)(BOOL fSubscribe, PSTR pszUri);



/**
 *  HttpdStart:
 *
 *      - calls service start on HTP0:
 *
 */
BOOL HttpdStart()
{
    HANDLE  hFile;
    DWORD   dwBytesReturned;
    BOOL    bRet;

    hFile = CreateFile(L"HTP0:", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (INVALID_HANDLE_VALUE == hFile)
    {
        return FALSE;
    }

    bRet = DeviceIoControl(hFile, IOCTL_SERVICE_START, NULL, 0, NULL ,0, &dwBytesReturned, NULL);

    CloseHandle(hFile);

    return bRet;
}



/**
 *  UpnpServiceStart:
 *  
 *      - refresh the registry configuration data
 *      - ensure that httpd is running else we are in an invalid state
 *
 */
BOOL UpnpServiceStart()
{
    upnp_config::refresh_configuration();

    if ( g_fState == SERVICE_STATE_ON )
        return TRUE;

    if(HttpdStart())
    {
        g_fState = SERVICE_STATE_ON;
        return TRUE;
    }

    return FALSE;
}



/**
 *  UpnpServiceStop:
 *
 *      - implement service stop functionality here
 *      - currently a shell returning FALSE
 *
 */
BOOL UpnpServiceStop()
{
    return FALSE;
}



DWORD WINAPI
UPP_Init(
    DWORD dwInfo
    )
{
    TraceTag(ttidInit, "UPP_Init called\n");
    InitializeListHead(&g_UpnpOpenList);
    InitializeListHead(&g_DevTreeList);
    pfSubscribeCallback = SubscribeCallback;

    HttpdStart();

    assert(g_pThreadPool == NULL);

    g_pThreadPool = new SVSThreadPool;
    g_fState = SERVICE_STATE_ON;

    return TRUE;
}


// RETRIEVE_RECORD:  here we wish to get the address of the UPNP_OPEN record
//      since our linked list doesn't store this directly, we need to use some pointer arithmetic
#define RETRIEVE_RECORD(pointer, address, type, field) (pointer = ((type *)( \
                          (LPBYTE)(address) - \
                          (LPBYTE)(&((type *)0)->field))))

BOOL WINAPI
UPP_Deinit(
    DWORD dwData
    )
{
    TraceTag(ttidInit, "UPP_Deinit called\n");

    UPNP_OPEN *pDevice = NULL;
    PLIST_ENTRY pLink = NULL;

    g_fState = SERVICE_STATE_UNLOADING;

    // if there are devices registered then deregister them before deinitializing    
    // remove them one at a time from the front of the list
    while(!IsListEmpty(&g_UpnpOpenList))
    {
        pLink = g_UpnpOpenList.Flink;

        // remove the entry based on the same method used in linklist.h
        // using pointer offsetting
        RETRIEVE_RECORD(pDevice, pLink, UPNP_OPEN, linkage);
        DEBUGCHK(pDevice != NULL);

        // attempt to close the device deregistering it
        UPP_Close((DWORD) pDevice);
        Sleep(200);
    }

    AssertSz(g_UpnpOpenList.Flink == &g_UpnpOpenList, "UPNP Open List not empty on exit");
    AssertSz(g_DevTreeList.Flink == &g_DevTreeList, "UPNP DevTree List not empty on exit");

    g_fState = SERVICE_STATE_OFF;
    SsdpCleanup();

    delete g_pThreadPool;
    g_pThreadPool = NULL;

    return TRUE;
}


HANDLE WINAPI
UPP_Open(
    DWORD dwData,
    DWORD dwAccessCode,
    DWORD dwShareMode
    )
{
    TraceTag(ttidInit, "UPP_Open called\n");

    UPNP_OPEN *pOpen = new UPNP_OPEN;

    if (!pOpen)
    {
        SetLastError(ERROR_OUTOFMEMORY);
        return NULL;
    }

    pOpen->sig = UPNP_OPEN_SIG;

    EnterCriticalSection(&g_csUPNP);
    InsertHeadList(&g_UpnpOpenList, &pOpen->linkage);
    LeaveCriticalSection(&g_csUPNP);

    return (HANDLE) pOpen;
}


BOOL WINAPI
UPP_Close(
    DWORD dwOpenData
    )
{
    TraceTag(ttidInit, "UPP_Close called\n");

    UPNP_OPEN *pOpen = (UPNP_OPEN *)dwOpenData;
    PLIST_ENTRY pLink;
    BOOL fRet = FALSE;

    Assert(pOpen);
    AssertSz(pOpen->sig == UPNP_OPEN_SIG, "UPP_Close called with invalid handle\n");

    EnterCriticalSection(&g_csUPNP);
    for (pLink = g_UpnpOpenList.Flink; pLink != &g_UpnpOpenList; pLink = pLink->Flink)
    {
        if (pOpen == CONTAINING_RECORD(pLink,UPNP_OPEN, linkage))
        {
            break;
        }
    }

    if (pLink != &g_UpnpOpenList)
    {
        RemoveEntryList(pLink);
        pOpen->sig = UPNP_CLOSE_SIG;
        fRet = TRUE;
    }
    LeaveCriticalSection(&g_csUPNP);

    if (fRet)
    {
        UpnpCleanUpProc(pOpen);
        CleanupNotifications(pOpen);
        delete (pOpen);
    }

    return fRet;
}


DWORD WINAPI
UPP_Read(
    DWORD dwOpenData, 
    LPVOID pBuf, 
    DWORD len
    )
{
    return (ULONG) -1;
}

DWORD WINAPI
UPP_Write(
    DWORD dwOpenData, 
    LPCVOID pBuf, 
    DWORD len
    )
{
    return (ULONG) -1;
}

DWORD WINAPI
UPP_Seek(
    DWORD dwOpenData, 
    long pos, 
    DWORD type
    )
{
    return (ULONG) -1;
}

void WINAPI
UPP_PowerUp(
    DWORD dwData
    )
{
}

void WINAPI
UPP_PowerDown(
    DWORD dwData
    )
{
}

BOOL WINAPI
UPP_IOControl(
    DWORD  dwOpenData, 
    DWORD  dwCode, 
    PBYTE  pBufIn,
    DWORD  dwLenIn, 
    PBYTE  pBufOut, 
    DWORD  dwLenOut,
    PDWORD pdwActualOut
    )
{
    BOOL fRet = TRUE;
    LONG status = ERROR_SUCCESS;

    if(dwCode == SSDP_IOCTL_INVOKE)
    {
        if(!SsdpStartup())
        {
            return FALSE;
        }

        return SDP_IOControl(
                dwOpenData,
                dwCode,
                pBufIn,
                dwLenIn,
                pBufOut,
                dwLenOut,
                pdwActualOut);
    }

    switch (dwCode)
    {
        case IOCTL_SERVICE_START:
            return UpnpServiceStart();

        case IOCTL_SERVICE_REFRESH:
            UpnpServiceStop();
            return UpnpServiceStart();

        case IOCTL_SERVICE_STOP:
            return UpnpServiceStop();

        case IOCTL_SERVICE_STATUS:

            if(!pBufOut || dwLenOut < sizeof(DWORD))
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            *(DWORD *)pBufOut = g_fState;

            if (pdwActualOut)
            {
                *pdwActualOut = sizeof(DWORD);
            }

            return TRUE;

        case UPNP_IOCTL_INVOKE:
            {
                ce::psl_stub<> stub(pBufIn, dwLenIn);

                if(!SsdpStartup())
                {
                    return FALSE;
                }

                if(g_fState != SERVICE_STATE_ON)
                {
                    if(HttpdStart())
                    {
                        g_fState = SERVICE_STATE_ON;
                    }
                    else
                    {
                        return FALSE;
                    }
                }

                switch(stub.function())
                {
                    case UPNP_IOCTL_ADD_DEVICE:

                        fRet = stub.call(UpnpAddDeviceImpl, (HANDLE)dwOpenData);
                        break;

                    case UPNP_IOCTL_REMOVE_DEVICE:

                        fRet = stub.call(UpnpRemoveDeviceImpl);
                        break;

                    case UPNP_IOCTL_PUBLISH_DEVICE:

                        fRet = stub.call(UpnpPublishDeviceImpl);
                        break;

                    case UPNP_IOCTL_UNPUBLISH_DEVICE:

                        fRet = stub.call(UpnpUnpublishDeviceImpl);
                        break;

                    case UPNP_IOCTL_GET_SCPD_PATH:

                        fRet = stub.call(UpnpGetSCPDPathImpl);
                        break;

                    case UPNP_IOCTL_GET_UDN:

                        fRet = stub.call(UpnpGetUDNImpl);
                        break;

                    case UPNP_IOCTL_SUBMIT_PROPERTY_EVENT:

                        fRet = stub.call(UpnpSubmitPropertyEventImpl);
                        break;

                    case UPNP_IOCTL_UPDATE_EVENTED_VARIABLES:

                        fRet = stub.call(UpnpUpdateEventedVariablesImpl);
                        break;

                    case UPNP_IOCTL_SET_RAW_CONTROL_RESPONSE:

                        fRet = stub.call(SetRawControlResponseImpl);
                        break;

                    case UPNP_IOCTL_INIT_RECV_INVOKE_REQUEST:

                        fRet = stub.call(InitializeReceiveInvokeRequestImpl);
                        break;


                    case UPNP_IOCTL_RECV_INVOKE_REQUEST:

                        fRet = stub.call(ReceiveInvokeRequestImpl);
                        break;

                    case UPNP_IOCTL_CANCEL_RECV_INVOKE_REQUEST:

                        fRet = stub.call(CancelReceiveInvokeRequestImpl);
                        break;

                    default:
                        status = ERROR_NOT_SUPPORTED;
                        break;
                }
            }
            break;

        default:
            status = ERROR_NOT_SUPPORTED;
            break;
    }

    if(status != ERROR_SUCCESS)
    {
        SetLastError(status);
        return FALSE;
    }

    if(!fRet && ERROR_SUCCESS == GetLastError())
    {
        SetLastError(ERROR_INVALID_PARAMETER);
    }

    return fRet;
}
