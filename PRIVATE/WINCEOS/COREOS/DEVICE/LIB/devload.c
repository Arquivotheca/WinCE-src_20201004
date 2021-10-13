//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/* File:    devload.c
 *
 * Purpose: WinCE device manager initialization and built-in device management
 *
 */
#include <windows.h>
#include <types.h>
#include <winreg.h>
#include <tchar.h>
#include "device.h"
#include <devload.h>
#include "devloadp.h"
#include "pnp.h"
#include <dbt.h>
#include <tapi.h>
#include <tspi.h>
#include <tapicomn.h>
#include <notify.h>
#include <windev.h>

#ifndef ARRAYSIZE
#define ARRAYSIZE(a)            (sizeof(a)/sizeof(a[0]))
#endif

//
// Functions:
//  DevicePostInit
//  CallTapiDeviceChange
//  AnnounceNewDevice
//  NotifyDevice
//  StartOneDriver
//  InitDevices
//  DevloadInit
//

// these structures are passed to the notification thread in a queue
typedef struct _DEVICE_CHANGE_CONTEXT {
	LIST_ENTRY link;
    TCHAR DevName[DEVNAME_LEN];
    BOOL bNew;
} DEVICE_CHANGE_CONTEXT, * PDEVICE_CHANGE_CONTEXT;

typedef struct _DEVICE_CHANGE_THREAD_INIT_INFO {
	LIST_ENTRY ContextChain;		// queue of devices
	CRITICAL_SECTION csNotify;		// serializes access to the queue
	HANDLE hevNewEntries;			// signaled when the queue is updated
} DEVICE_CHANGE_THREAD_INIT_INFO, *PDEVICE_CHANGE_THREAD_INIT_INFO;

extern CRITICAL_SECTION g_devcs;    // device.c
extern LIST_ENTRY g_DevChain;       // device.c
extern LIST_ENTRY g_CandidateDevs;  // device.c
extern DWORD g_BootPhase;           // device.c

int v_NextDeviceNum;            // Used to create active device list
HMODULE v_hTapiDLL;
HMODULE v_hCoreDLL;
BOOL g_bSystemInitd;
DEVICE_CHANGE_THREAD_INIT_INFO g_NotifyThreadState;

const TCHAR s_DllName_ValName[] = DEVLOAD_DLLNAME_VALNAME;
const TCHAR s_ActiveKey[] = DEVLOAD_ACTIVE_KEY;
const TCHAR s_BuiltInKey[] = DEVLOAD_BUILT_IN_KEY;

#ifdef DEBUG

//
// These defines must match the ZONE_* defines in devloadp.h
//
#define DBG_ERROR      1
#define DBG_WARNING    2
#define DBG_FUNCTION   4
#define DBG_INIT       8
#define DBG_BUILTIN    16
#define DBG_PCMCIA     32
#define DBG_ACTIVE     64
#define DBG_RESMGR     128
#define DBG_FSD        256
#define DBG_DYING      512
#define DBG_BOOTSEQ 1024
#define DBG_PNP         2048

DBGPARAM dpCurSettings = {
    TEXT("DEVLOAD"), {
    TEXT("Errors"),TEXT("Warnings"),TEXT("Functions"),TEXT("Initialization"),
    TEXT("Built-In Devices"),TEXT("PCMCIA Devices"),TEXT("Active Devices"),TEXT("Resource Manager"),
    TEXT("File System Drvr"),TEXT("Dying Devs"),TEXT("Boot Sequence"),TEXT("PnP"),
    TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined") },
    DBG_ERROR | DBG_WARNING | DBG_INIT
};
#endif  // DEBUG

//
// Function to call a newly registered device's post initialization
// device I/O control function.
//
static VOID
DevicePostInit(
    LPTSTR DevName,
    DWORD  dwIoControlCode,
    DWORD  Handle,          // Handle from RegisterDevice
    HKEY   hDevKey
    )
{
    HANDLE hDev;
    BOOL ret;
    POST_INIT_BUF PBuf;

    DEBUGMSG(ZONE_ACTIVE,
        (TEXT("DEVICE!DevicePostInit calling CreateFile(%s)\r\n"), DevName));
    hDev = CreateFile(
                DevName,
                GENERIC_READ|GENERIC_WRITE,
                0,
                NULL,
                OPEN_EXISTING,
                0,
                NULL);
    if (hDev == INVALID_HANDLE_VALUE) {
        DEBUGMSG(ZONE_ACTIVE,
            (TEXT("DEVICE!DevicePostInit CreateFile(%s) failed %d\r\n"),
            DevName, GetLastError()));
        return;
    }

    DEBUGMSG(ZONE_ACTIVE,
        (TEXT("DEVICE!DevicePostInit calling DeviceIoControl(%s, %d)\r\n"),
         DevName, dwIoControlCode));
    PBuf.p_hDevice = (HANDLE)Handle;
    PBuf.p_hDeviceKey = hDevKey;
    ret = DeviceIoControl(
              hDev,
              dwIoControlCode,
              &PBuf,
              sizeof(PBuf),
              NULL,
              0,
              NULL,
              NULL);
    DEBUGMSG(ZONE_ACTIVE,
        (TEXT("DEVICE!DevicePostInit DeviceIoControl(%s, %d) "),
         DevName, dwIoControlCode));
    if (ret == TRUE) {
        DEBUGMSG(ZONE_ACTIVE, (TEXT("succeeded\r\n")));
    } else {
        DEBUGMSG(ZONE_ACTIVE, (TEXT("failed\r\n")));
    }
    CloseHandle(hDev);
}   // DevicePostInit



#if 0
//
// Function to notify TAPI of a device change. A TAPI device has a key which
// contains a Tsp value listing the TAPI Service Provider.
//
// Returns TRUE if the device has a TAPI association.
//
BOOL
CallTapiDeviceChange(
    HKEY hActiveKey,
    LPCTSTR DevName,
    BOOL bDelete
    )
{
    DWORD ValType;
    DWORD ValLen;
    DWORD status;
    HKEY DevKey;
    TCHAR DevKeyPath[REG_PATH_LEN];
    BOOL bTapiDevice;

    typedef BOOL (WINAPI *PFN_TapiDeviceChange) (HKEY DevKey, LPCWSTR lpszDevName, BOOL bDelete);
    PFN_TapiDeviceChange pfnTapiDeviceChange;

    //
    // TAPI.DLL must be loaded first.
    //
    if (v_hTapiDLL == NULL) {
        DEBUGMSG(ZONE_TAPI|ZONE_ERROR,
            (TEXT("DEVICE!CallTapiDeviceChange TAPI.DLL not loaded yet.\r\n")));
        return FALSE;
    }

    //
    // It's a TAPI device if a subkey of the device key contains a TSP value.
    //
    ValLen = sizeof(DevKeyPath);
    status = RegQueryValueEx(            // Read Device Key Path
                hActiveKey,
                DEVLOAD_DEVKEY_VALNAME,
                NULL,
                &ValType,
                (LPBYTE)DevKeyPath,
                &ValLen);
    if (status) {
        DEBUGMSG(ZONE_INIT,
            (TEXT("DEVICE!CallTapiDeviceChange RegQueryValueEx(%s) failed %d\r\n"),
            DEVLOAD_DEVKEY_VALNAME, status));
            return FALSE;
    }

    status = RegOpenKeyEx(             // Open the Device Key
                HKEY_LOCAL_MACHINE,
                DevKeyPath,
                0,
                0,
                &DevKey);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
            (TEXT("DEVICE!CallTapiDeviceChange: RegOpenKeyEx(%s) returned %d\r\n"),
            DevKeyPath, status));
        return FALSE;
    }


    // Try to find the pointer to TapiDeviceChange
    pfnTapiDeviceChange = (PFN_TapiDeviceChange)GetProcAddress (v_hTapiDLL, TEXT("TapiDeviceChange"));
    if (pfnTapiDeviceChange == NULL) {
        DEBUGMSG(ZONE_TAPI,
                 (TEXT("DEVICE!CallTapiDeviceChange can't get pointer to TapiDeviceChange.\r\n")));
        return FALSE;
    }

    bTapiDevice = pfnTapiDeviceChange(DevKey, DevName, bDelete);
    RegCloseKey(DevKey);
    return bTapiDevice;
}   // CallTapiDeviceChange
#endif


//
// Function to format and send a Windows broadcast message announcing the arrival
// or removal of a device in the system.
//
static VOID
BroadcastDeviceChange(
    LPTSTR DevName,
    BOOL bNew
    )
{
    PDEV_BROADCAST_PORT pBCast;
    DWORD len;
    LPTSTR str;
    typedef BOOL (WINAPI *PFN_SendNotifyMessageW)(HWND hWnd, UINT Msg,
                                                  WPARAM wParam,
                                                  LPARAM lParam);
    PFN_SendNotifyMessageW pfnSendNotifyMessageW;

    if (v_hCoreDLL == NULL) {
        v_hCoreDLL = (HMODULE)LoadLibrary(TEXT("COREDLL.DLL"));
        if (v_hCoreDLL == NULL) {
            DEBUGMSG(ZONE_ERROR,
                     (TEXT("DEVICE!BroadcastDeviceChange unable to load CoreDLL.\r\n")));
            return;
        }
    }

    pfnSendNotifyMessageW = (PFN_SendNotifyMessageW)GetProcAddress(v_hCoreDLL, TEXT("SendNotifyMessageW"));
    if (pfnSendNotifyMessageW == NULL) {
        DEBUGMSG (ZONE_PCMCIA, (TEXT("Can't find SendNotifyMessage\r\n")));
        return;
    }
    //
    // Don't use GWE API functions if not there
    //
    if (IsAPIReady(SH_WMGR) == FALSE) {
        return;
    }

    len = sizeof(DEV_BROADCAST_HDR) + (_tcslen(DevName) + 1)*sizeof(TCHAR);

    pBCast = LocalAlloc(LPTR, len);
    if (pBCast == NULL) {
        return;
    }

    



    pBCast->dbcp_devicetype = DBT_DEVTYP_PORT;
    pBCast->dbcp_reserved = 0;
    str = (LPTSTR)&(pBCast->dbcp_name[0]);
    _tcscpy(str, DevName);
    pBCast->dbcp_size = len;

    DEBUGMSG(ZONE_PCMCIA,
             (TEXT("DEVICE!BroadcastDeviceChange Calling SendNotifyMessage for device %s\r\n"), DevName));

    // Call the function
    pfnSendNotifyMessageW(
        HWND_BROADCAST,
        WM_DEVICECHANGE,
        (bNew) ? DBT_DEVICEARRIVAL : DBT_DEVICEREMOVECOMPLETE,
        (LPARAM)pBCast);

    LocalFree(pBCast);
}   // BroadcastDeviceChange


//
// Function to signal the app notification system of a device change
//
static VOID
NotifyDevice(
    LPTSTR DevName,
    LPTSTR Op
    )
{
    DWORD len;
    LPTSTR str;

    //
    // First check if shell functions can be called.
    //
    if (IsAPIReady(SH_WMGR) == FALSE) {
        DEBUGMSG(ZONE_PCMCIA,
            (TEXT("DEVICE!NotifyDevice IsAPIReady(SH_WMGR i.e. GWES) returned FALSE, not calling CeEventHasOccurred(%s %s)\r\n"),
            Op, DevName));
        return;
    }

    len = (_tcslen(Op) + _tcslen(DevName) + 2)*sizeof(TCHAR);

    str = LocalAlloc(LPTR, len);
    if (str == NULL) {
        return;
    }

    //
    // Format the end of the command line
    //
    _tcscpy(str, Op);
    _tcscat(str, TEXT(" "));
    _tcscat(str, DevName);

    DEBUGMSG(ZONE_PCMCIA,
        (TEXT("DEVICE!NotifyDevice Calling CeEventHasOccurred(%s)\r\n"), str));
    CeEventHasOccurred(NOTIFICATION_EVENT_DEVICE_CHANGE, str);
    LocalFree(str);
}   // NotifyDevice


//
// Thread function to call CeEventHasOccurred and SendNotifyMessage for a device
// that has been recently installed or removed. Another thread is needed because
// in the context in which the device changes occur, the gwes and the filesystem
// critical sections are taken.
//
static DWORD
DeviceNotifyThread(
   IN PVOID ThreadContext
   )
{
    PDEVICE_CHANGE_THREAD_INIT_INFO pNotifyThreadState = 
        (PDEVICE_CHANGE_THREAD_INIT_INFO) ThreadContext;
    HKEY hk;
    DWORD dwStatus;
    
    DEBUGCHK(pNotifyThreadState != NULL);
    DEBUGCHK(pNotifyThreadState->hevNewEntries != NULL);
    
    // get our thread priority
    dwStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, DEVLOAD_DRIVERS_KEY, 0, 0, &hk);
    if(dwStatus == ERROR_SUCCESS) {
        DWORD dwType, dwValue;
        DWORD dwSize = sizeof(dwValue);
        dwStatus = RegQueryValueEx(hk, _T("NotifyPriority256"), NULL, &dwType, 
            (LPBYTE) &dwValue, &dwSize);
        if(dwStatus == ERROR_SUCCESS && dwType == REG_DWORD) {
            CeSetThreadPriority(GetCurrentThread(), (INT) dwValue);
            DEBUGMSG(ZONE_INIT, (_T("DEVICE!DeviceNotifyThread: priority is %u\r\n"),
                dwStatus));
        }
        RegCloseKey(hk);
    }
    
    
    while(TRUE) {
        dwStatus = WaitForSingleObject(pNotifyThreadState->hevNewEntries, INFINITE);
        if(dwStatus == WAIT_OBJECT_0) {
            BOOL fDone = FALSE;
            
            // process all queued device notifications in order
            while(!fDone) {
                PDEVICE_CHANGE_CONTEXT pdcc = NULL;
                
                // get the next notification, if one is present
                EnterCriticalSection(&pNotifyThreadState->csNotify);
                if(!IsListEmpty(&pNotifyThreadState->ContextChain)) {
                    PLIST_ENTRY pLink = RemoveTailList(&pNotifyThreadState->ContextChain);
                    DEBUGCHK(pLink != NULL);
                    pdcc = CONTAINING_RECORD(pLink, DEVICE_CHANGE_CONTEXT, link);
                    DEBUGCHK(pdcc != NULL);
                }
                LeaveCriticalSection(&pNotifyThreadState->csNotify);
                
                // if we found a queue entry, process it
                if(pdcc != NULL) {
                    BroadcastDeviceChange(pdcc->DevName, pdcc->bNew);
                    NotifyDevice(pdcc->DevName, (pdcc->bNew) ? NOTIFY_DEVICE_ADD : NOTIFY_DEVICE_REMOVE);
                    LocalFree(pdcc);
                } else {
                    fDone = TRUE;
                }
            }
        } else {
            DEBUGMSG(ZONE_ERROR, 
                (_T("DEVICE!DeviceNotifyThread: WaitForSingleObject() failed %d\r\n"),
                GetLastError()));
        }
    }
    
    return 0;
}   // DeviceNotifyThread


// This routine launches the thread that handles device notification broadcasts.
// It is called once at boot time and returns TRUE if successful, FALSE if
// there's a problem.
BOOL StartDeviceNotifyThread(void)
{
    BOOL fOk = FALSE;
    HANDLE htNotify;
    
    // set up the notification structure
    g_NotifyThreadState.hevNewEntries = CreateEvent(NULL, FALSE, FALSE, NULL);
    InitializeCriticalSection(&g_NotifyThreadState.csNotify);
    InitializeListHead(&g_NotifyThreadState.ContextChain);
    
    // make sure we have everything we need
    if(g_NotifyThreadState.hevNewEntries == NULL) {
        DEBUGMSG(ZONE_WARNING, 
            (_T("DEVICE!StartDeviceNotify: CreateEvent() failed\r\n")));
    } else {
        htNotify = CreateThread(NULL, 0,
            (LPTHREAD_START_ROUTINE)DeviceNotifyThread,
            (LPVOID) &g_NotifyThreadState, 0, NULL);
        if (htNotify == NULL) {
            DEBUGMSG(ZONE_WARNING, 
                (_T("DEVICE!StartDeviceNotify: CreateThread() failed\r\n")));
        } else {
            // close the handle, since we're not waiting on the thread -- but
            // keep the value to record that the thread is running.
            CloseHandle(htNotify);
            fOk = TRUE;
        }
    }
    
    // if there was an error, free resources
    if(!fOk) {
        if(g_NotifyThreadState.hevNewEntries != NULL) {
            CloseHandle(g_NotifyThreadState.hevNewEntries);
            g_NotifyThreadState.hevNewEntries = NULL;
        }
        DeleteCriticalSection(&g_NotifyThreadState.csNotify);
    }

    DEBUGCHK(fOk);

    return fOk;
}

//
// Function to start a thread that signals a device change via the application
// notification system and via a broadcast windows message.
//
VOID
StartDeviceNotify(
    LPTSTR DevName,
    BOOL bNew
    )
{
    PDEVICE_CHANGE_CONTEXT pdcc;

    DEBUGCHK(g_NotifyThreadState.hevNewEntries != NULL);
    if(g_NotifyThreadState.hevNewEntries != NULL) {
        pdcc = LocalAlloc(LPTR, sizeof(DEVICE_CHANGE_CONTEXT));
        if (pdcc == NULL) {
            DEBUGMSG(ZONE_WARNING, (_T("DEVICE!StartDeviceNotify: LocalAlloc() failed\r\n")));
            return;
        }
        
        pdcc->bNew = bNew;
        memcpy(pdcc->DevName, DevName, DEVNAME_LEN*sizeof(TCHAR));
        
        pdcc->DevName[DEVNAME_LEN-1] = '\0';        // Sentinel
        
        // queue the notification if the thread is not running
        EnterCriticalSection(&g_NotifyThreadState.csNotify);
        InsertHeadList(&g_NotifyThreadState.ContextChain, (PLIST_ENTRY) pdcc);
        LeaveCriticalSection(&g_NotifyThreadState.csNotify);
        SetEvent(g_NotifyThreadState.hevNewEntries);
    }
}   // StartDeviceNotify

//
// Function to delete all the values under an active key that we were unable
// to delete (most likely because someone else has it opened).
//
static VOID
DeleteActiveValues(
    LPTSTR ActivePath
    )
{
    HKEY hActiveKey;
    DWORD status;
    
    //
    // Open active key
    //
    status = RegOpenKeyEx(
                HKEY_LOCAL_MACHINE,
                ActivePath,
                0,
                0,
                &hActiveKey);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
           (TEXT("DEVICE!DeleteActiveValues: RegOpenKeyEx(%s) returned %d\r\n"),
           ActivePath, status));
        return;
    }

    //
    // Delete some values so we no longer mistake it for a valid active key.
    //
    RegDeleteValue(hActiveKey, DEVLOAD_CLIENTINFO_VALNAME);
    RegDeleteValue(hActiveKey, DEVLOAD_HANDLE_VALNAME);
    RegDeleteValue(hActiveKey, DEVLOAD_DEVNAME_VALNAME);
    RegDeleteValue(hActiveKey, DEVLOAD_DEVKEY_VALNAME);
    RegDeleteValue(hActiveKey, DEVLOAD_PNPID_VALNAME);
    RegDeleteValue(hActiveKey, DEVLOAD_SOCKET_VALNAME);
    RegCloseKey(hActiveKey);
}   // DeleteActiveValues

//
// Function to DeregisterDevice a device and remove its active key and announce
// the device's removal to the system.
//
BOOL
I_DeactivateDevice(
    HANDLE hDevice,
    LPTSTR ActivePath
    )
{
    HKEY hActiveKey;
    DWORD status;
    DWORD ValLen;
    DWORD ValType;
    TCHAR DevName[DEVNAME_LEN];

    //
    // Open its active key in the registry
    //
    status = RegOpenKeyEx(
                HKEY_LOCAL_MACHINE,
                ActivePath,
                0,
                0,
                &hActiveKey);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
           (TEXT("DEVICE!I_DeactivateDevice: RegOpenKeyEx(%s) returned %d\r\n"),
           ActivePath, status));
    } else {
        // We're going to need the name
        ValLen = sizeof(DevName);
        status = RegQueryValueEx(
                     hActiveKey,
                     DEVLOAD_DEVNAME_VALNAME,
                     NULL,
                     &ValType,
                     (PUCHAR)DevName,
                     &ValLen);
        if (status != ERROR_SUCCESS) {
            DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
               (TEXT("DEVICE!I_DeactivateDevice: RegQueryValueEx(DevName) returned %d\r\n"),
               status));
            RegCloseKey(hActiveKey);
        }

//        CallTapiDeviceChange(hActiveKey, DevName, TRUE); // bDelete == TRUE
        RegCloseKey(hActiveKey);
    }
    DeregisterDevice((HANDLE)hDevice);

    PnpProcessInterfaces(ActivePath, FALSE);  // defined in pnp.c
    
    status = RegDeleteKey(HKEY_LOCAL_MACHINE, ActivePath);
    if (status) {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,(TEXT("DEVICE!I_DeactivateDevice: RegDeleteKey failed %d\n"), status));
        DeleteActiveValues(ActivePath);
    }

    return TRUE;
}

LPCTSTR ValidateString (LPCTSTR szFromCaller)
{
    LPCTSTR p = MapCallerPtr((LPVOID)szFromCaller, 256); // magic - will be fixed later
    return p;
}

// create a structure to keep track of device prefix/index combinations while we install
// the device and add it to the list of candidate devices.  Make sure to hold g_devcs 
// when this routine is called.  Return a pointer to the structure if successful or NULL
// if there's a problem.
fscandidatedev_t *
CreateCandidateDev(WCHAR *pszPrefix, DWORD dwPrefixLen, DWORD dwIndex)
{
    fscandidatedev_t *pdc = NULL;

    // sanity check parameters
    if(dwPrefixLen <= sizeof(pdc->szPrefix)) {
        // allocate a structure
        pdc = LocalAlloc(LPTR, sizeof(*pdc));
        if(pdc != NULL) {
            // init structure members
            memset(pdc, 0, sizeof(*pdc));
            memcpy(pdc->szPrefix, pszPrefix, dwPrefixLen);
            pdc->dwPrefixLen = dwPrefixLen;
            pdc->dwIndex = dwIndex;

            // add this structure to the list
            InsertTailList(&g_CandidateDevs, (PLIST_ENTRY) pdc)
        }
    }

    return pdc;
}

// Look for a member of the candidate device list that matches this one.  Return a pointer
// to the matching entry or NULL if no match is found.  Make sure to hold g_devcs when 
// calling this routine.
fscandidatedev_t *
FindCandidateDev(WCHAR *pszPrefix, DWORD dwPrefixLen, DWORD dwIndex)
{
    fscandidatedev_t *pdc;

    // look for a match
    for(pdc = (fscandidatedev_t *) g_CandidateDevs.Flink;
    pdc != (fscandidatedev_t *) &g_CandidateDevs;
    pdc = (fscandidatedev_t *) pdc->list.Flink) {
        if(pdc->dwPrefixLen == dwPrefixLen 
            && pdc->dwIndex == dwIndex 
            && memcmp(pdc->szPrefix, pszPrefix, dwPrefixLen) == 0) {
            // found a match, get out
            break;
        }            
    }

    // did we find a match?
    if(pdc == (fscandidatedev_t *) &g_CandidateDevs) {
        pdc = NULL;                 // no, return NULL
    }

    return pdc;
}

// remove an entry from the list of candidate device structures and free its associated
// memory.  Make sure to hold g_devcs when calling this function.
void
DeleteCandidateDev(fscandidatedev_t * pdc)
{
    // sanity check
    if(pdc != NULL) {
        RemoveEntryList((PLIST_ENTRY) pdc);
        LocalFree(pdc);
    }
}

//
// Function to RegisterDevice a device driver and add it to the active device list
// in HLM\Drivers\Active and then signal the system that a new device is available.
//
// Return the HANDLE from RegisterDevice.
//
HANDLE
StartOneDriver(
    LPCTSTR   RegPath,
    DWORD     LoadOrder,
    LPCREGINI lpReg,
    DWORD     cReg,
    LPVOID    lpvParam
    )
{
    BOOL bUseContext;
    BOOL bFoundIndex;
    BOOL bHasPrefix;
    HKEY ActiveKey;
    HKEY DevKey;
    DWORD Context;
    DWORD Disposition;
    DWORD Handle;
    DWORD Index;
    DWORD Flags;
    DWORD status;
    DWORD ValLen;
    DWORD ValType;
    DWORD ActiveId;
    LPTSTR str;
    TCHAR ActivePath[REG_PATH_LEN];
    TCHAR DevDll[DEVDLL_LEN];
    TCHAR DevName[DEVNAME_LEN];
    TCHAR Prefix[DEVPREFIX_LEN];
    fsdev_t * lpdev;
    fscandidatedev_t * lpcandidatedev = NULL;;

    DEBUGMSG(ZONE_ACTIVE,
             (TEXT("DEVICE!StartOneDriver starting HLM\\%s.\r\n"), RegPath));

    //
    // Get the required (dll) and optional (prefix, index, flags, and context) values.
    //
    status = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        RegPath,
        0,
        0,
        &DevKey);
    if (status) {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                 (TEXT("DEVICE!StartOneDriver RegOpenKeyEx(%s) returned %d.\r\n"),
                  RegPath, status));
        return NULL;
    }

    // Read Flags value first (if it exists).  If DEVFLAGS_NOLOAD or not in correct boot phase,
    // return w/o checking other parameters.
    ValLen = sizeof(Flags);
    status = RegQueryValueEx(
        DevKey,
        DEVLOAD_FLAGS_VALNAME,
        NULL,
        &ValType,
        (PUCHAR)&Flags,
        &ValLen);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE,
                 (TEXT("DEVICE!StartOneDriver RegQueryValueEx(%s\\Flags) returned %d\r\n"),
                  RegPath, status));
        Flags = DEVFLAGS_NONE;  // default is no flags set
    }
    if (Flags & DEVFLAGS_NOLOAD) {
        DEBUGMSG(ZONE_ACTIVE,
                 (TEXT("DEVICE!StartOneDriver not loading %s with DEVFLAGS_NOLOAD\n"),
                  RegPath));
        RegCloseKey(DevKey);
        return NULL;    // Really success, but we cannot distinguish success at
        // deliberately not loading from a failure to load.
    }
    if ((Flags & DEVFLAGS_BOOTPHASE_1) && (g_BootPhase > 1)) { // BOOTPHASE_1=loaded in phase 1 and never again
        DEBUGMSG(ZONE_ACTIVE,
                 (TEXT("DEVICE!StartOneDriver skipping %s in BootPhase %d\n"),
                  RegPath, g_BootPhase));
        RegCloseKey(DevKey);
        return NULL;    // Same caveat as above.
    }


    // Read DLL name
    ValLen = sizeof(DevDll);
    status = RegQueryValueEx(
        DevKey,
        s_DllName_ValName,
        NULL,
        &ValType,
        (PUCHAR)DevDll,
        &ValLen);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                 (TEXT("DEVICE!StartOneDriver RegQueryValueEx(%s\\DllName) returned %d\r\n"),
                  RegPath, status));
        RegCloseKey(DevKey);
        return NULL; // dll name is required
    }


    // Read prefix value, if it exists.
    bHasPrefix = TRUE;
    ValLen = sizeof(Prefix);
    status = RegQueryValueEx(
        DevKey,
        DEVLOAD_PREFIX_VALNAME,
        NULL,
        &ValType,
        (PUCHAR)Prefix,
        &ValLen);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE,
                 (TEXT("DEVICE!StartOneDriver RegQueryValueEx(%s\\Prefix) returned %d\r\n"),
                  RegPath, status));
        bHasPrefix = FALSE;
    }

    //
    // Read the optional index and context values
    //
    ValLen = sizeof(Index);
    status = RegQueryValueEx(
        DevKey,
        DEVLOAD_INDEX_VALNAME,
        NULL,
        &ValType,
        (PUCHAR)&Index,
        &ValLen);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE,
                 (TEXT("DEVICE!StartOneDriver RegQueryValueEx(%s\\Index) returned %d\r\n"),
                  RegPath, status));
        Index = (DWORD)-1;     // devload will find an index to use
    }

    bUseContext = TRUE;
    ValLen = sizeof(Context);
    status = RegQueryValueEx(
        DevKey,
        DEVLOAD_CONTEXT_VALNAME,
        NULL,
        &ValType,
        (PUCHAR)&Context,
        &ValLen);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE,
                 (TEXT("DEVICE!StartOneDriver RegQueryValueEx(%s\\Context) returned %d\r\n"),
                  RegPath, status));
        bUseContext = FALSE;    // context is pointer to active reg path string
    }

    //
    // Format the key's registry path (HLM\Drivers\Active\nnnn)
    //
    ActiveId = InterlockedIncrement(&v_NextDeviceNum) - 1;  // sub 1 to get the pre-increment value
    wsprintf(ActivePath, TEXT("%s\\%02d"), s_ActiveKey, ActiveId);

    DEBUGMSG(ZONE_ACTIVE,
             (TEXT("DEVICE!StartOneDriver Adding HLM\\%s.\r\n"), ActivePath));

    //
    // Create a new key in the active list
    //
    status = RegCreateKeyEx(
        HKEY_LOCAL_MACHINE,
        ActivePath,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        0,
        NULL,
        &ActiveKey,     // HKEY result
        &Disposition);
    if (status) {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                 (TEXT("DEVICE!StartOneDriver RegCreateKeyEx(%s) returned %d.\r\n"),
                  ActivePath, status));
        RegCloseKey(DevKey);
        return NULL;
    }

    //
    // Default context is registry path
    //
    if (bUseContext == FALSE) {
        Context = (DWORD)ActivePath;
    }


    //
    // Install requested values in the device's active registry key.
    // (handle and device name are added after RegisterDevice())
    //
    lpReg = MapCallerPtr((LPVOID)lpReg, sizeof(REGINI) * cReg);
    if (cReg != 0 && lpReg == NULL) {
        DEBUGMSG(ZONE_WARNING|ZONE_ERROR,
                 (TEXT("DEVICE!StartOneDriver - invalid registry initializer; ignoring.\n")));
        cReg = 0;  // cause all entries to be ignored
    }
    while (cReg--) {
        __try {
            status = RegSetValueEx(
                ActiveKey,
                ValidateString(lpReg[cReg].lpszVal),
                0,
                lpReg[cReg].dwType,
                MapCallerPtr(lpReg[cReg].pData, lpReg[cReg].dwLen), // really only need READ access
                lpReg[cReg].dwLen);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status = ERROR_INVALID_ACCESS;
        }
        if (status) {
            DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                     (TEXT("DEVICE!StartOneDriver RegSetValueEx(0x%08x) returned %d.\r\n"),
                      lpReg[cReg].lpszVal, status));
        }
    }

    //
    // Registry path of the device driver (from HLM\Drivers\BuiltIn or HLM\Drivers\PCMCIA)
    //
    if (RegPath != NULL) {    
        status = RegSetValueEx(
            ActiveKey,
            DEVLOAD_DEVKEY_VALNAME,
            0,
            DEVLOAD_DEVKEY_VALTYPE,
            (PBYTE)RegPath,
            sizeof(TCHAR)*(_tcslen(RegPath) + 1));
        if (status) {
            DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                     (TEXT("DEVICE!StartOneDriver RegSetValueEx(%s) returned %d.\r\n"),
                      DEVLOAD_DEVKEY_VALNAME, status));
        }
    }

    // Non-stream drivers must specify the index explicitly in order to load
    // multiple instances of the same driver. This is to prevent accidents because
    // non-stream drivers typically represent interfaces that multiplex functionality
    // over many physical devices.
    if (!bHasPrefix && Index == -1) {
        Index = 0;
    }

    if (Index == -1) {
        //
        // Find the first available index for this device prefix.
        //
        bFoundIndex = FALSE;
        Index = 1;  // device index (run it through 1-9 and then 0)
        EnterCriticalSection(&g_devcs);
        while (!bFoundIndex) {
            bUseContext = FALSE; // reuse this flag for this loop.

            // look for a conflict in the list of existing devices
            for (lpdev = (fsdev_t *)g_DevChain.Flink;
                 lpdev != (fsdev_t *)&g_DevChain;
                 lpdev = (fsdev_t *)lpdev->list.Flink) {
                if (!memcmp(Prefix, lpdev->type, sizeof(lpdev->type))) {
                    if (lpdev->index == Index) {
                        bUseContext = TRUE;
                        break;
                    }
                }
            }

            // does this entry seem to be available?
            if(!bUseContext) {
                // look for a conflict in the list of candidate devices (those
                // currently being created by another thread)
                if (FindCandidateDev(Prefix, sizeof(lpdev->type), Index) != NULL) {
                    // this device name may be used in the near future, don't use it now
                    bUseContext = TRUE;
                }
            }
            
            // if this device name/index combination is in use, bUseContext 
            // will be TRUE
            if (!bUseContext) {
                //
                // No other devices of this prefix are using this index.
                //
                bFoundIndex = TRUE;
                DEBUGMSG(ZONE_ACTIVE,
                         (TEXT("DEVICE:StartOneDriver using index %d for new %s device\r\n"),
                          Index, Prefix));

                // make sure nobody else uses this index while we are installing this device
                lpcandidatedev = CreateCandidateDev(Prefix, sizeof(lpdev->type), Index);
                if(lpcandidatedev == NULL) {
                    DEBUGMSG(ZONE_ACTIVE | ZONE_ERROR, 
                             (_T("DEVICE:StartOneDriver couldn't allocate candidate device structure\r\n")));
                    bFoundIndex = FALSE;
                }
                break;
            }
            if (Index == 0) {   // There are no more indexes to try after 0
                break;
            }

            Index++;
            if (Index == 10) {
                Index = 0;       // Try 0 as the last index
            }
        }   // while (trying device indexes)
        LeaveCriticalSection(&g_devcs);
    } else {
        bFoundIndex = TRUE;
    }

    if (bFoundIndex) {
        if (bHasPrefix) {
            //
            // Format device name from prefix and index and write it to registry
            //
            _tcsncpy(DevName, Prefix, ARRAYSIZE(DevName)-3);
            str = DevName + _tcslen(DevName);
            str[0] = (TCHAR)Index + (TCHAR)'0';
            str[1] = (TCHAR)':';
            str[2] = (TCHAR)0;
            status = RegSetValueEx(ActiveKey,
                                   DEVLOAD_DEVNAME_VALNAME,
                                   0,
                                   DEVLOAD_DEVNAME_VALTYPE,
                                   (PBYTE)DevName,
                                   min(sizeof(TCHAR)*(_tcslen(DevName) + 1),sizeof(DevName)));
            if (status) {
                DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                         (TEXT("DEVICE!StartOneDriver RegSetValueEx(%s) returned %d.\r\n"),
                          DEVLOAD_DEVNAME_VALNAME, status));
            }
        }
    
        Handle = (DWORD)RegisterDeviceEx(
            bHasPrefix ? Prefix : NULL,
            Index,
            DevDll,
            Context,
            LoadOrder,
            Flags,
            ActiveId,
            lpvParam
            );
    } else {
        Handle = 0;
    }

    // if we've allocated a candidate device structure we should free it now.  The device
    // has either been instantiated or it hasn't.
    if(lpcandidatedev != NULL) {
        EnterCriticalSection(&g_devcs);
        DeleteCandidateDev(lpcandidatedev);
        LeaveCriticalSection(&g_devcs);
    }
    
    if (Handle == 0) {
        //
        // RegisterDevice failed
        //
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                 (TEXT("DEVICE!StartOneDriver RegisterDevice(%s, %d, %s, 0x%x) failed\r\n"),
                  bHasPrefix ? Prefix : TEXT("<nil>"), Index, DevDll, Context));
        RegCloseKey(DevKey);
        RegCloseKey(ActiveKey);
        RegDeleteKey(HKEY_LOCAL_MACHINE, ActivePath);
        return NULL;
    }

    //
    // Add handle from RegisterDevice()
    //
    status = RegSetValueEx(
        ActiveKey,
        DEVLOAD_HANDLE_VALNAME,
        0,
        DEVLOAD_HANDLE_VALTYPE,
        (PBYTE)&Handle,
        sizeof(Handle));
    if (status) {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                 (TEXT("DEVICE!StartOneDriver RegSetValueEx(%s) returned %d.\r\n"),
                  DEVLOAD_HANDLE_VALNAME, status));
    }

    //
    // Only stream interfaces can use TAPI services or receive ioctls.
    //
    if (bHasPrefix) {
//        CallTapiDeviceChange(ActiveKey, DevName, FALSE); // bDelete == FALSE

        //
        // Determine whether this device wants a post init ioctl
        //
        ValLen = sizeof(Context);
        status = RegQueryValueEx(
            DevKey,
            DEVLOAD_INITCODE_VALNAME,
            NULL,
            &ValType,
            (PUCHAR)&Context,
            &ValLen);
        if (status != ERROR_SUCCESS) {
            DEBUGMSG(ZONE_ACTIVE,
                     (TEXT("DEVICE!StartOneDriver RegQueryValueEx(%s\\InitCode) returned %d\r\n"),
                      RegPath, status));
        } else {
            //
            // Call the new device's IOCTL_DEVICE_INITIALIZED 
            //
            DevicePostInit(DevName, Context, Handle, DevKey);
        }
    }

    PnpProcessInterfaces(ActivePath, TRUE);  // defined in pnp.c

    RegCloseKey(DevKey);
    RegCloseKey(ActiveKey);

    return (HANDLE)Handle;
}   // StartOneDriver

//
// Function to load device drivers for built-in devices.
//
VOID
InitDevices(VOID)
{
    HKEY RootKey;
    DWORD status;
    DWORD ValType;
    DWORD ValLen;
    TCHAR RootKeyPath[REG_PATH_LEN];

    //
    // Open HLM\Drivers key
    //
    status = RegOpenKeyEx(
                HKEY_LOCAL_MACHINE,
                DEVLOAD_DRIVERS_KEY,
                0,
                0,
                &RootKey);
                
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ROOT|ZONE_ERROR,
            (TEXT("DEVICE!InitDevices RegOpenKeyEx(%s) returned %d.\r\n"),
            DEVLOAD_DRIVERS_KEY, status));
        return;
    }

    //
    // Look for root key value; if not found use current Root Key as default,
    // otherwise open new root key
    //
    ValLen = sizeof(RootKeyPath);
    status = RegQueryValueEx(
                RootKey,
                DEVLOAD_ROOTKEY_VALNAME,
                NULL,
                &ValType,
                (PUCHAR)RootKeyPath,
                &ValLen);
                
    if (status != ERROR_SUCCESS) {
        // Root key value not found, thus root key is Drivers
        _tcscpy(RootKeyPath, DEVLOAD_DRIVERS_KEY);
    }

    // Close previous root key
    RegCloseKey(RootKey);

    DEBUGMSG(1,
        (L"DEVICE!InitDevices: Root Key is %s.\r\n",
        RootKeyPath));

    // Someday we'll want to track the handle returned by ActivateDevice so that
    // we can potentially deactivate it later. But since this code refers only to
    // builtin (ie, static) devices, we can just throw away the key.
    (void) ActivateDevice(RootKeyPath, 0);

}   // InitDevices


//
// Called from device.c after it initializes.
//
void DevloadInit(void)
{
    DEBUGMSG(ZONE_INIT, (TEXT("DEVICE!DevloadInit\r\n")));
    //
    // Delete the HLM\Drivers\Active key since there are no active devices at
    // init time.
    //
    RegDeleteKey(HKEY_LOCAL_MACHINE, s_ActiveKey);

    v_NextDeviceNum = 1;
    v_hTapiDLL = NULL;
    v_hCoreDLL = NULL;
    g_bSystemInitd = FALSE;
    InitDevices();
}


//
// Called from device.c after the system has initialized.
//
void DevloadPostInit(void)
{
    if (g_bSystemInitd) {
        DEBUGMSG(ZONE_INIT, (TEXT("DEVICE!DevloadPostInit: multiple DLL_SYSTEM_STARTED msgs!!!\r\n")));
        return;
    }
    g_bSystemInitd = TRUE;

    DEBUGMSG(ZONE_INIT, (TEXT("-DEVICE!DevloadPostInit\r\n")));
}
