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
#include <devload.h>
#include <guiddef.h>
#include <notify.h>
#include <dbt.h>
#include <console.h>
#include <sid.h>
#include <reflector.h>
#include "devmgrp.h"
#include "devzones.h"

#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

typedef struct _DEVICE_CHANGE_THREAD_INIT_INFO {
    LIST_ENTRY ContextChain;        // queue of devices
    CRITICAL_SECTION csNotify;      // serializes access to the queue
    HANDLE hevNewEntries;           // signaled when the queue is updated
} DEVICE_CHANGE_THREAD_INIT_INFO, *PDEVICE_CHANGE_THREAD_INIT_INFO;

DEVICE_CHANGE_THREAD_INIT_INFO g_NotifyThreadState;

// these structures are passed to the notification thread in a queue
typedef struct _DEVICE_CHANGE_CONTEXT {
    LIST_ENTRY link;
    TCHAR DevName[DEVNAME_LEN];
    BOOL bNew;
} DEVICE_CHANGE_CONTEXT, * PDEVICE_CHANGE_CONTEXT;

typedef BOOL (WINAPI *PFN_SendNotifyMessageW)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

static PFN_SendNotifyMessageW gpfnSendNotifyMessageW;

// {6F40791D-300E-44e4-BC38-E0E63CA8375C} -- see DMCLASS_PROTECTEDBUSNAMESPACE in devload.h
static const GUID gDmClassProtectedBusNamespace = { 0x6f40791d, 0x300e, 0x44e4, { 0xbc, 0x38, 0xe0, 0xe6, 0x3c, 0xa8, 0x37, 0x5c } };

extern BOOL AdvertiseInterfaceEx(const GUID* devclass,LPCWSTR name,DWORD dwId, BOOL fAdd);

//
// Function to format and send a Windows broadcast message announcing the arrival
// or removal of a device in the system.
//
static VOID
BroadcastDeviceChange(
    LPCTSTR DevName,
    BOOL bNew
    )
{
    PDEV_BROADCAST_PORT pBCast;
    DWORD len;
    LPTSTR str;

    DEBUGCHK(gpfnSendNotifyMessageW != NULL);

    len = sizeof(DEV_BROADCAST_HDR) + (_tcslen(DevName) + 1)*sizeof(TCHAR);

    pBCast = LocalAlloc(LPTR, len);
    if (pBCast == NULL) {
        return;
    }

    //
    // Broadcast the arrival of a new device in the system.
    //
    pBCast->dbcp_devicetype = DBT_DEVTYP_PORT;
    pBCast->dbcp_reserved = 0;
    str = (LPTSTR)&(pBCast->dbcp_name[0]);
    VERIFY(SUCCEEDED(StringCbCopy(str, len - sizeof(DEV_BROADCAST_HDR), DevName)));
    pBCast->dbcp_size = len;

    DEBUGMSG(ZONE_PNP,
             (TEXT("DEVICE!BroadcastDeviceChange Calling SendNotifyMessage for device %s\r\n"), DevName));

    // Call the function
    gpfnSendNotifyMessageW(
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
    LPCTSTR DevName,
    LPCTSTR Op
    )
{
    DWORD len;
    LPTSTR str;


    len = (_tcslen(Op) + _tcslen(DevName) + 2)*sizeof(TCHAR);

    str = LocalAlloc(LPTR, len);
    if (str == NULL) {
        return;
    }

    //
    // Format the end of the command line
    //
    VERIFY(SUCCEEDED(StringCbCopy(str, len, Op)));
    VERIFY(SUCCEEDED(StringCbCat(str, len, TEXT(" "))));
    VERIFY(SUCCEEDED(StringCbCat(str, len, DevName)));

    DEBUGMSG(ZONE_PNP,
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
#pragma warning(push)
#pragma warning(disable:4702) // unreachable code
static DWORD
DeviceNotifyThread(
   IN PVOID ThreadContext
   )
{
    PDEVICE_CHANGE_THREAD_INIT_INFO pNotifyThreadState = 
        (PDEVICE_CHANGE_THREAD_INIT_INFO) ThreadContext;
    HKEY hk;
    DWORD dwStatus;
    HMODULE hmCoreDll;
    
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
    

    // get DLL entry points for notification
    hmCoreDll = (HMODULE)LoadLibrary(TEXT("COREDLL.DLL"));
    if(hmCoreDll != NULL) {
        gpfnSendNotifyMessageW = (PFN_SendNotifyMessageW)GetProcAddress(hmCoreDll, TEXT("SendNotifyMessageW"));
        FreeLibrary(hmCoreDll);     // we're explicitly linked with coredll so this is safe
    }

    for (;;) {
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
                    if(gpfnSendNotifyMessageW) {
                        if(WaitForAPIReady(SH_WMGR,0) == WAIT_OBJECT_0 ) {
                            BroadcastDeviceChange(pdcc->DevName, pdcc->bNew);
                        }
                    }
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
#pragma warning(pop)

// This routine launches the thread that handles device notification broadcasts.
// It is called once at boot time and returns TRUE if successful, FALSE if
// there's a problem.
static BOOL 
StartDeviceNotifyThread(void)
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
    LPCTSTR DevName,
    BOOL bNew
    )
{
    PDEVICE_CHANGE_CONTEXT pdcc;

    DEBUGCHK(g_NotifyThreadState.hevNewEntries != NULL);
    DEBUGCHK(_tcslen(DevName) < DEVNAME_LEN);

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

BOOL 
ConvertGuidToString (__out_ecount(cchGuid) LPTSTR pszGuid, int cchGuid, const GUID *pGuid)
{
    BOOL fOk = FALSE;
    
    // format the class name
    fOk = SUCCEEDED(StringCchPrintf(pszGuid, cchGuid, _T("{%08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x}"), 
                        pGuid->Data1, pGuid->Data2, pGuid->Data3,
                        (pGuid->Data4[0] << 8) + pGuid->Data4[1], pGuid->Data4[2], pGuid->Data4[3], 
                        pGuid->Data4[4], pGuid->Data4[5], pGuid->Data4[6], pGuid->Data4[7]));

    return fOk;
}


static BOOL IsInRange(WCHAR id, WCHAR idFirst, WCHAR idLast)
{
    return ((UINT)((id)-(idFirst)) <= (UINT)((idLast)-(idFirst)));
}

// scan psz for a number of hex digits (at most 8); update psz, return
// value in Value; check for chDelim; return TRUE for success.
static BOOL HexStringToDword(__inout LPCWSTR * ppsz, __out DWORD* pdwValue, int cDigits, WCHAR wchDelim)
{
    int ich;
    LPCWSTR psz = *ppsz;
    DWORD Value = 0;
    BOOL fRet = TRUE;

    for (ich = 0; ich < cDigits; ich++)
    {
        WCHAR ch = psz[ich];

        if (IsInRange(ch, L'0', L'9'))
        {
            Value = (Value << 4) + ch - L'0';
        }
        else if (IsInRange( (ch |= (L'a' - L'A')), L'a', L'f') )
        {
            Value = (Value << 4) + ch - L'a' + 10;
        }
        else
        {
            return FALSE;
        }
    }

    if (wchDelim)
    {
        fRet = (psz[ich++] == wchDelim);
    }

    *pdwValue = Value;
    *ppsz = psz + ich;

    return fRet;
}

BOOL 
ConvertStringToGuid(__in LPCWSTR psz, __out GUID *pguid)
{
    DWORD dw = 0;

    if (*psz++ != L'{')
    {
        return FALSE;
    }

    if (!HexStringToDword(&psz, &pguid->Data1, sizeof(DWORD)*2, L'-'))
    {
        return FALSE;
    }

    if (!HexStringToDword(&psz, &dw, sizeof(WORD)*2, L'-'))
    {
        return FALSE;
    }

    pguid->Data2 = (WORD)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(WORD)*2, L'-'))
    {
        return FALSE;
    }

    pguid->Data3 = (WORD)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(BYTE)*2, 0))
    {
        return FALSE;
    }

    pguid->Data4[0] = (BYTE)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(BYTE)*2, L'-'))
    {
        return FALSE;
    }

    pguid->Data4[1] = (BYTE)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(BYTE)*2, 0))
    {
        return FALSE;
    }

    pguid->Data4[2] = (BYTE)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(BYTE)*2, 0))
    {
        return FALSE;
    }

    pguid->Data4[3] = (BYTE)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(BYTE)*2, 0))
    {
        return FALSE;
    }

    pguid->Data4[4] = (BYTE)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(BYTE)*2, 0))
    {
        return FALSE;
    }

    pguid->Data4[5] = (BYTE)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(BYTE)*2, 0))
    {
        return FALSE;
    }

    pguid->Data4[6] = (BYTE)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(BYTE)*2, L'}'))
    {
        return FALSE;
    }

    pguid->Data4[7] = (BYTE)dw;

    return !(*psz); // should have reached the end of the string
}

// This routine looks for an interface advertised by a device driver. If it
// finds a match, it returns a pointer to its state structure.  Otherwise,
// it returns NULL.  The caller of this routine must hold the device manager
// critical section.
fsinterface_t *
FindDeviceInterface(fsdev_t *lpdev, const GUID *pClass)
{
    fsinterface_t *pif;

    DEBUGCHK(lpdev != NULL);
    DEBUGCHK(pClass != NULL);
    
    for(pif = lpdev->pInterfaces; pif != NULL; pif = pif->pNext) {
        if(IsEqualGUID(&pif->guidClass, pClass)) {
            break;
        }
    }

    return pif;
}

// This routine advertises or deadvertises an interface that is associated
// with a particular device driver.  If successful it will allocate or
// free a structure describing the interface.  Its intrface is modeled
// after AdvertiseInterface(), with an extra parameter for the device state
// structure.  Unlike AdvertiseInterface(), it returns a status DWORD, with
// ERROR_SUCCESS indicating successful completion or a Win32 error code
// indicating a problem.
DWORD
I_AdvertiseDeviceInterface(fsdev_t *lpdev, LPCGUID pClass, LPCWSTR pszName, BOOL fAdd)
{
    DWORD dwStatus = ERROR_SUCCESS;
#ifdef DEBUG
    LPCWSTR pszFname = L"I_AdvertiseDeviceInterface";
#endif  // DEBUG

    DEBUGCHK(lpdev != NULL);
    DEBUGCHK(pClass != NULL);
    DEBUGCHK(pszName != NULL);

    if(fAdd) {
        // allocate a structure to keep track of the interface
        fsinterface_t *pif;
        DWORD cbSizeOfNamePlusNull = (wcslen(pszName) + 1) * sizeof(pszName[0]);   // Determin the buffer space needed for the string
        pif = (fsinterface_t *) LocalAlloc(0, sizeof(*pif) + cbSizeOfNamePlusNull);
        if(pif == NULL) {
            DEBUGMSG(ZONE_WARNING, 
                (_T("%s: not enough memory to advertise '%s' for device 0x%08x\r\n"),
                pszFname, pszFname, lpdev));
            dwStatus = ERROR_OUTOFMEMORY;
        } else {
            fsinterface_t *pifPrev,*pifRemove;

            LPWSTR pszTemp = (LPWSTR) ((LPBYTE) pif + sizeof(*pif));
            
            // initialize the structure
            StringCbCopy(pszTemp, cbSizeOfNamePlusNull, pszName); // Copy the string into the buffer space allocated for it.
            pif->pszName = pszTemp;
            pif->guidClass = *pClass;

            // add to the list of interfaces (must happen before we advertise)
            EnterCriticalSection(&g_devcs);
            pif->pNext = lpdev->pInterfaces;
            lpdev->pInterfaces = pif;
            LeaveCriticalSection(&g_devcs);

            // now try to advertise the interface
            if(!AdvertiseInterfaceEx(&pif->guidClass, pif->pszName, lpdev->dwId, fAdd)) {
                dwStatus = GetLastError();

                // failed to advertise interface - remove it from the list
                pifPrev = NULL;
                EnterCriticalSection(&g_devcs);
                pifRemove = lpdev->pInterfaces;
                while (pifRemove != pif)
                {
                    pifPrev = pifRemove;
                    pifRemove = pifRemove->pNext;
                    if (!pifRemove)
                        break;
                }
                if (pifRemove)
                {
                    if (pifPrev)
                        pifPrev->pNext = pif->pNext;
                    else
                        lpdev->pInterfaces = pif->pNext;
                }
                LeaveCriticalSection(&g_devcs);

                DEBUGMSG(ZONE_WARNING, 
                    (_T("%s: AdvertiseInterface({%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}, '%s', %d) failed %d\r\n"), 
                    pszFname,
                    pif->guidClass.Data1, pif->guidClass.Data2, pif->guidClass.Data3, 
                    pif->guidClass.Data4[0], pif->guidClass.Data4[1], pif->guidClass.Data4[2], pif->guidClass.Data4[3], 
                    pif->guidClass.Data4[4], pif->guidClass.Data4[5], pif->guidClass.Data4[6], pif->guidClass.Data4[7], 
                    pif->pszName, fAdd, dwStatus));
                DEBUGCHK(dwStatus != ERROR_SUCCESS);
            } else {
                DEBUGMSG(ZONE_PNP, 
                    (L"PNP interface class {%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x} (%s) ATTACH\n", 
                    pif->guidClass.Data1, pif->guidClass.Data2, pif->guidClass.Data3, 
                    pif->guidClass.Data4[0], pif->guidClass.Data4[1], pif->guidClass.Data4[2], pif->guidClass.Data4[3], 
                    pif->guidClass.Data4[4], pif->guidClass.Data4[5], pif->guidClass.Data4[6], pif->guidClass.Data4[7], 
                    pif->pszName));
            }

            // did we add the interface successfully?
            if(dwStatus != ERROR_SUCCESS) {
                // no, release resources
                LocalFree(pif);
            }
        }
    } else {
        // shut down interfaces and free their associated data
        fsinterface_t *pif, *pifPrev;

        EnterCriticalSection(&g_devcs);

        // find the interface
        pifPrev = NULL;
        for(pif = lpdev->pInterfaces; pif != NULL; pif = pif->pNext) {
            if(IsEqualGUID(&pif->guidClass, pClass) && wcscmp(pif->pszName, pszName) == 0) {
                break;
            }
            pifPrev = pif;
        }

        // did we find it?
        if(pif == NULL) {
            DEBUGMSG(ZONE_WARNING, 
                (L"%s: can't find {%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x} (%s)\n", 
                pszFname,
                pClass->Data1, pClass->Data2, pClass->Data3, 
                pClass->Data4[0], pClass->Data4[1], pClass->Data4[2], pClass->Data4[3], 
                pClass->Data4[4], pClass->Data4[5], pClass->Data4[6], pClass->Data4[7], 
                pszName));
            dwStatus = ERROR_FILE_NOT_FOUND;
            LeaveCriticalSection(&g_devcs);
        } else {
            BOOL fOk;

            // remove this entry from the list
            if(pifPrev != NULL) {
                pifPrev->pNext = pif->pNext;
            } else {
                lpdev->pInterfaces = pif->pNext;
            }

            // can't keep g_devcs before calling an external API
            LeaveCriticalSection(&g_devcs);

            // de-advertise the interface
            DEBUGMSG(ZONE_PNP, 
                (L"PNP interface class {%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x} (%s) DETACH\n", 
                pif->guidClass.Data1, pif->guidClass.Data2, pif->guidClass.Data3, 
                pif->guidClass.Data4[0], pif->guidClass.Data4[1], pif->guidClass.Data4[2], pif->guidClass.Data4[3], 
                pif->guidClass.Data4[4], pif->guidClass.Data4[5], pif->guidClass.Data4[6], pif->guidClass.Data4[7], 
                pif->pszName));
            fOk = AdvertiseInterfaceEx(&pif->guidClass, pif->pszName, lpdev->dwId, fAdd);
            if(!fOk) {
                dwStatus = GetLastError();
                DEBUGMSG(ZONE_WARNING, 
                    (_T("%s: AdvertiseInterface({%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}, '%s', %d) failed %d\r\n"), 
                    pszFname,
                    pif->guidClass.Data1, pif->guidClass.Data2, pif->guidClass.Data3, 
                    pif->guidClass.Data4[0], pif->guidClass.Data4[1], pif->guidClass.Data4[2], pif->guidClass.Data4[3], 
                    pif->guidClass.Data4[4], pif->guidClass.Data4[5], pif->guidClass.Data4[6], pif->guidClass.Data4[7], 
                    pif->pszName, fAdd, dwStatus));
                DEBUGCHK(dwStatus != ERROR_SUCCESS);
            }

            // release resources
            LocalFree(pif);
        }


    }

    // update the device driver's state structure if necessary
    if(lpdev->pszBusName != NULL && lpdev->fnOpen != NULL) {
        // are we advertising support for protected namespaces and associating it with our bus name?
        if(IsEqualGUID(&gDmClassProtectedBusNamespace, pClass)) {
            WCHAR szNameBuf[MAX_PATH];
            HRESULT hr = StringCchCopyW(szNameBuf, _countof(szNameBuf), L"$bus\\");
            hr = StringCchCatW(szNameBuf, _countof(szNameBuf), lpdev->pszBusName);
            if(wcscmp(szNameBuf, pszName) == 0) {
                EnterCriticalSection(&g_devcs);
                if(fAdd) {
                    lpdev->wFlags |= DF_ENABLE_BUSNAMES;
                } else {
                    lpdev->wFlags &= ~DF_ENABLE_BUSNAMES;
                }
                LeaveCriticalSection(&g_devcs);
            }
        }
    }

    return dwStatus;
}

// Check to see if there is an "IClass" value in the device's active key;
// if not, check for one in the device key.
// If still not, then if there's a "Prefix" value, use the DEVCLASS_STREAM guid;
// otherwise issue a warning - this driver is not PnP-CE compatible.
//
// For each guid in the IClass value (assume 1 if we defaulted):
//   it should be of the form "GUID[=name]" where GUID is in the standard
//   registry form - {8-4-4-4-12} - and name is a string representing the
//   the device address as advertised over the interface designated by the GUID.
//   If the "=name" component is not present then the driver must have specified
//   a Prefix and the name will default to the name of the stream interface.
void 
PnpAdvertiseInterfaces (fsdev_t *lpdev)
{
    HKEY hk = NULL;
    DWORD dwStatus, cbSize = 0;
    LPCWSTR pszStreamIClass = L"{f8a6ba98-087a-43ac-a9d8-b7f13c5bae31}\0";   // can't include '=' because we modify the string
    WCHAR szNameBuf[MAX_PATH];
    LPCWSTR pszIClass = NULL;
    LPCWSTR pszClass = NULL;
    LPWSTR pszTempIClass = NULL;
#ifdef DEBUG
    LPCWSTR pszFname = _T("Device!PnpAdvertiseInterfaces");
#endif  // DEBUG

    DEBUGCHK(lpdev != NULL);
    DEBUGCHK(lpdev->pszDeviceKey != NULL);

    // format the active key path, We sure szNameBuf is big enough to hold the formatted string.
    VERIFY(SUCCEEDED(StringCchPrintf(szNameBuf,MAX_PATH, L"%s\\%02u", s_ActiveKey, lpdev->dwId)));

    // open the active key
    dwStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szNameBuf, 0, 0, &hk);
    if(dwStatus != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_WARNING, 
            (_T("%s: can't open active key '%s' for device 0x%08x\r\n"), 
            pszFname, szNameBuf, lpdev));
    } else {
        dwStatus = RegQueryValueEx(hk, DEVLOAD_ICLASS_VALNAME, NULL, NULL, NULL, &cbSize);
        if(dwStatus != ERROR_SUCCESS) {
            RegCloseKey(hk);
        }
    } 

    // have we found an IClass?
    if(dwStatus != ERROR_SUCCESS) {
        // no, check the device key
        dwStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpdev->pszDeviceKey, 0, 0, &hk);
        if(dwStatus != ERROR_SUCCESS) {
            DEBUGMSG(ZONE_WARNING, 
                (_T("%s: can't open device key '%s' for device 0x%08x\r\n"), 
                pszFname, lpdev->pszDeviceKey, lpdev));
        } else {
            dwStatus = RegQueryValueEx(hk, DEVLOAD_ICLASS_VALNAME, NULL, NULL, NULL, &cbSize);
            if(dwStatus != ERROR_SUCCESS) {
                RegCloseKey(hk);
            }
        }
    }

    // have we found an IClass?
    if(dwStatus != ERROR_SUCCESS) {
        // no, use the default stream class
        pszIClass = pszStreamIClass;
        dwStatus = ERROR_SUCCESS;
    } else {
        // yes, validate the size
        if((cbSize % sizeof(WCHAR)) != 0) {
            DEBUGMSG(ZONE_WARNING, 
                (_T("%s: IClass size %u for device 0x%08x is not a multiple of WCHAR\r\n"), 
                pszFname, cbSize, lpdev));
            RegCloseKey(hk);
            return;
        }
        if (cbSize + sizeof(WCHAR)<cbSize) { // overflow.
            cbSize -= sizeof(WCHAR);
        }
        
        // try to read the IClass
        __try {
            pszTempIClass = malloc(cbSize + sizeof(WCHAR));
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            dwStatus = ERROR_OUTOFMEMORY;
            pszTempIClass = NULL;
        }
        if(pszTempIClass != NULL) {
            DWORD cchSize;
            dwStatus = RegQueryValueEx(hk, DEVLOAD_ICLASS_VALNAME, NULL, NULL, (LPBYTE) pszTempIClass, &cbSize);
            cchSize = cbSize / sizeof(pszTempIClass[0]);
            pszTempIClass[cchSize - 1] = 0;  // enforce null termination
            pszTempIClass[cchSize] = 0;      // enforce multi-sz double termination
            pszIClass = pszTempIClass;
        }
        RegCloseKey(hk);
    }

    // have we read the IClass?
    if(dwStatus != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_WARNING, (_T("%s: can't get IClass for device 0x%08x\r\n"), pszFname, lpdev));
        return;
    }

    // send legacy CeEventHasOccurred and WM_DEVICECHANGE notifications
    if(lpdev->pszLegacyName != NULL) {
        StartDeviceNotify(lpdev->pszLegacyName, TRUE);
    }

    // Parse the IClass data, which consists of multiple null-terminated strings
    pszClass = pszIClass;
#pragma prefast(disable: 394, "Pre-NULL terminated string.")
    while(*pszClass != 0) {
        LPWSTR pszName;
        DWORD dwLen = wcslen(pszClass);

        // decide what name to associate with the GUID
        pszName = wcschr(pszClass, L'=');
        if(pszName != NULL) {
            *pszName = 0;       // null terminate the class
            pszName++;          // get the rest of the name
            if(*pszName == 0) {
                DEBUGMSG(ZONE_WARNING, (_T("%s: empty name associated with IClass '%s' for device 0x%08x\r\n"), 
                    pszFname, pszClass, lpdev));
                pszClass += dwLen + 1;
                continue;
            }
        }

        // was a name specified?
        if(pszName == NULL) {
            if(lpdev->pszLegacyName != NULL) {
                pszName = lpdev->pszLegacyName;
            } else if(lpdev->pszDeviceName != NULL) {
                VERIFY(SUCCEEDED(StringCchCopyW(szNameBuf, _countof(szNameBuf), L"$device\\")));
                VERIFY(SUCCEEDED(StringCchCatW(szNameBuf, _countof(szNameBuf), lpdev->pszDeviceName)));
                pszName = szNameBuf;
            } else {
                DEBUGMSG(ZONE_PNP, 
                    (_T("%s: can't advertise interfaces from unnamed device 0x%08x\r\n"),
                    pszFname, lpdev));
            }
        }

        // is the name actually a namespace specifier?
        if(pszName != NULL) {
            if(wcsicmp(pszName, L"%d") == 0) {
                if(lpdev->pszDeviceName == NULL) {
                    DEBUGMSG(ZONE_WARNING, 
                        (_T("%s: device 0x%08x has no $device name to advertise\r\n"), pszFname, lpdev));
                    pszName = NULL;
                } else {
                    VERIFY(SUCCEEDED(StringCchCopyW(szNameBuf, _countof(szNameBuf), L"$device\\")));
                    VERIFY(SUCCEEDED(StringCchCatW(szNameBuf, _countof(szNameBuf), lpdev->pszDeviceName)));
                    pszName = szNameBuf;
                }
            } else if(wcsicmp(pszName, L"%b") == 0) {
                if(lpdev->pszBusName == NULL || lpdev->fnOpen == NULL) {
                    DEBUGMSG(ZONE_WARNING, 
                        (_T("%s: device 0x%08x does not support handle access to $bus names\r\n"), pszFname, lpdev));
                    pszName = NULL;
                } else {
                    VERIFY(SUCCEEDED(StringCchCopyW(szNameBuf, _countof(szNameBuf), L"$bus\\")));
                    VERIFY(SUCCEEDED(StringCchCatW(szNameBuf, _countof(szNameBuf), lpdev->pszBusName)));
                    pszName = szNameBuf;
                }
            } else if(wcsicmp(pszName, L"%l") == 0) {
                if(lpdev->pszLegacyName == NULL) {
                    DEBUGMSG(ZONE_WARNING, 
                        (_T("%s: device 0x%08x has no legacy name to advertise\r\n"), pszFname, lpdev));
                    pszName = NULL;
                } else {
                    VERIFY(SUCCEEDED(StringCchCopyW(szNameBuf, _countof(szNameBuf), lpdev->pszLegacyName)));
                    pszName = szNameBuf;
                }
            }
        }

        // advertise the interface if we have a name associated with it
        if(pszName != NULL) {
            GUID guidClass;

            if (ConvertStringToGuid(pszClass, &guidClass) == FALSE) {
                // WARN! bad guid syntax
                DEBUGMSG(ZONE_WARNING, (_T("%s: invalid class GUID '%s'\r\n"), pszFname, pszClass));
            } else {
                I_AdvertiseDeviceInterface(lpdev, &guidClass, pszName, TRUE);
            }
        }
        pszClass += dwLen + 1;
    }
#pragma prefast(pop)
    if (pszTempIClass)
        free(pszTempIClass);
}

void 
PnpDeadvertiseInterfaces (fsdev_t *lpdev)
{
    DEBUGCHK(lpdev != NULL);

    // send legacy CeEventHasOccurred and WM_DEVICECHANGE notifications
    if(lpdev->pszLegacyName != NULL) {
        StartDeviceNotify(lpdev->pszLegacyName, FALSE);
    }

    // shut down interfaces and free their associated data
    // do not need a critical section here. Device is already marked 
    // as being deregistered
    while(lpdev->pInterfaces != NULL) {
        // I_AdvertiseDeviceInterface will free lpdev->pInterfaces, so copy args to local variables.
        LPWSTR pwszName;
        GUID guidClass = lpdev->pInterfaces->guidClass;
        DWORD cChars = wcslen(lpdev->pInterfaces->pszName) + 1;
        DWORD cbSize = cChars * sizeof(pwszName[0]);
        
        pwszName = (LPWSTR) LocalAlloc(0, cbSize);

        ASSERT(pwszName);

        if (pwszName != NULL)
        {
            wcscpy_s(pwszName, cChars, lpdev->pInterfaces->pszName);
            I_AdvertiseDeviceInterface(lpdev, &guidClass, pwszName, FALSE);
            LocalFree(pwszName);
        }
        else
        {
            DEBUGMSG(ZONE_ERROR, (_T("PnpDeadvertiseInterfaces: LocalAlloc failed for pwszName\r\n")));
        }
                                      
    }
}

// this routine sets up global data structures and starts the notification
// thread
void 
InitializePnPNotifications (void)
{
    StartDeviceNotifyThread();
}

