//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "pch.h"
#pragma hdrstop
#include <ssdpioctl.h>
#include <service.h>
#include <svsutil.hxx>

extern SVSThreadPool* g_pThreadPool;

LONG  g_fState;                     // Current Service State (running, stopped, starting, shutting down, unloading)

extern CRITICAL_SECTION g_csUPNP;
extern CRITICAL_SECTION g_csSSDP;
extern LIST_ENTRY g_DevTreeList;

extern BOOL UpnpAddDeviceImpl(IN HANDLE hOwner, IN UPNPDEVICEINFO *pDevInfo);
extern BOOL UpnpSubmitPropertyEventImpl(PCWSTR pszDeviceName, PCWSTR pszUDN, PCWSTR pszServiceId, UPNPPARAMS Args);
extern BOOL UpnpUpdateEventedVariablesImpl(PCWSTR pszDeviceName, PCWSTR pszUDN, PCWSTR pszServiceId, UPNPPARAMS Args);

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

        InitializeCriticalSection(&g_csUPNP);
        InitializeCriticalSection(&g_csSSDP);
        // Initialize debugging
        DEBUGREGISTER((HMODULE)DllHandle);
        // We don't need to receive thread attach and detach
        // notifications, so disable them to help application
        // performance.
        DisableThreadLibraryCalls((HMODULE)DllHandle);
        g_fState = SERVICE_STATE_UNINITIALIZED;
        svsutil_Initialize();
        break; 

    case DLL_THREAD_ATTACH:
        break;

    case DLL_PROCESS_DETACH:

        DeleteCriticalSection(&g_csUPNP); 
        DeleteCriticalSection(&g_csSSDP);
        break;

    case DLL_THREAD_DETACH:
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
void WINAPI UPP_PowerDown( DWORD dwData);
BOOL WINAPI UPP_IOControl( DWORD  dwOpenData, DWORD  dwCode, PBYTE  pBufIn, 
DWORD  dwLenIn,
                            PBYTE  pBufOut,DWORD  dwLenOut, PDWORD pdwActualOut);

};

// declared in subs.cpp
extern BOOL  (*pfSubscribeCallback)(BOOL fSubscribe, PSTR pszUri);


BOOL HttpdStart()
{
    HANDLE  hFile;
    DWORD   dwBytesReturned;
    BOOL    bRet;
    
    hFile = CreateFile(L"HTP0:", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (INVALID_HANDLE_VALUE == hFile)
        return FALSE;

    bRet = DeviceIoControl(hFile, IOCTL_SERVICE_START, NULL, 0, NULL ,0, &dwBytesReturned, NULL);

    CloseHandle(hFile);

    return bRet;
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

BOOL WINAPI
UPP_Deinit(
    DWORD dwData
    )
{
    TraceTag(ttidInit, "UPP_Deinit called\n");
    AssertSz(g_UpnpOpenList.Flink == &g_UpnpOpenList, "UPNP Open List not empty on exit");
    AssertSz(g_DevTreeList.Flink == &g_DevTreeList, "UPNP DevTree List not empty on exit");
    
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
    
    AssertSz(pOpen->sig == UPNP_OPEN_SIG, "UPP_Close called with invalid handle\n");

    EnterCriticalSection(&g_csUPNP);
    for (pLink = g_UpnpOpenList.Flink; pLink != &g_UpnpOpenList; pLink = pLink->Flink)
    {
        if (pOpen == CONTAINING_RECORD(pLink,UPNP_OPEN, linkage))
            break;
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
            return FALSE;
            
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
            
            // make sure httpd is running
            if(HttpdStart())
            {
                g_fState = SERVICE_STATE_ON;
            
                return TRUE;
            }
            else
                return FALSE;

        case IOCTL_SERVICE_STOP:
            
            if(g_UpnpOpenList.Flink == &g_UpnpOpenList)
            {
                g_fState = SERVICE_STATE_OFF;
                SsdpCleanup();
                return TRUE;
            }
            else
            {
                SetLastError(ERROR_BUSY);
                return FALSE;
            }
            break;

        case IOCTL_SERVICE_QUERY_CAN_DEINIT:

            if(g_UpnpOpenList.Flink == &g_UpnpOpenList)
            {
                g_fState = SERVICE_STATE_OFF;
                SsdpCleanup();
            }
            else
            {
                // don't unload me!
                memset(pBufOut, 0, dwLenOut);
            }
            return TRUE;

        case IOCTL_SERVICE_STATUS:
            
            if(!pBufOut || dwLenOut < sizeof(DWORD))
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            *(DWORD *)pBufOut = g_fState;
            
            if (pdwActualOut)
                *pdwActualOut = sizeof(DWORD);
                
            return TRUE;

        case UPNP_IOCTL_INVOKE:
            {
                ce::psl_stub<> stub(pBufIn, dwLenIn);
            
                if(!SsdpStartup())
                    return FALSE;
                    
                if(g_fState != SERVICE_STATE_ON)
                {
                    if(HttpdStart())
                        g_fState = SERVICE_STATE_ON;
                    else
                        return FALSE;
                }
                    
                switch(stub.function())
                {
                    case UPNP_IOCTL_ADD_DEVICE:
                    
                        fRet = stub.call(UpnpAddDeviceImpl, (HANDLE)dwOpenData);
                        break;
                    
                    case UPNP_IOCTL_REMOVE_DEVICE:
                    
                        fRet = stub.call(UpnpRemoveDevice);
                        break;
                    
                    case UPNP_IOCTL_PUBLISH_DEVICE:
                            
                        fRet = stub.call(UpnpPublishDevice);
                        break;
                    
                    case UPNP_IOCTL_UNPUBLISH_DEVICE:
                    
                        fRet = stub.call(UpnpUnpublishDevice);
                        break;
                    
                    case UPNP_IOCTL_GET_SCPD_PATH:
                    
                        fRet = stub.call(UpnpGetSCPDPath);
                        break;
                        
                    case UPNP_IOCTL_GET_UDN:
                        
                        fRet = stub.call(UpnpGetUDN);
                        break;
                    
                    case UPNP_IOCTL_SUBMIT_PROPERTY_EVENT:
                    
                        fRet = stub.call(UpnpSubmitPropertyEventImpl);
                        break;
                        
                    case UPNP_IOCTL_UPDATE_EVENTED_VARIABLES:
                    
                        fRet = stub.call(UpnpUpdateEventedVariablesImpl);
                        break;
                            
                    case UPNP_IOCTL_SET_RAW_CONTROL_RESPONSE:
                        
                        fRet = stub.call(SetRawControlResponse);
                        break;

                    case UPNP_IOCTL_PROCESS_CALLBACKS:
                        
                        fRet = stub.call(ProcessCallbacks);
                        break;
                        
                    case UPNP_IOCTL_CANCEL_CALLBACKS:
                        
                        fRet = stub.call(CancelCallbacks);
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
