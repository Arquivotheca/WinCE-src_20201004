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
#include <notify.h>
#include <notifdev.h>

static HANDLE hNotifyDriver = INVALID_HANDLE_VALUE;

int InitNotifyThunks (void) 
{
    if (INVALID_HANDLE_VALUE ==  hNotifyDriver) {

        // wait for coredll to load and device mgr APIs to be ready
        if ((WAIT_OBJECT_0 == WaitForAPIReady(SH_DEVMGR_APIS, 0))
            && IsNamedEventSignaled (COREDLL_READY_EVENT, 0)) {
        
            // see if the driver is already loaded by device manager
            HANDLE h = CreateFile (L"NFY0:", 0, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (h == INVALID_HANDLE_VALUE) {
                // device manager hasn't loaded this driver; try to activate the device
                ActivateDeviceEx_Trap(L"Notify", NULL, 0, NULL);
                h = CreateFile (L"NFY0:", 0, 0, NULL, OPEN_EXISTING, 0, NULL);
            }
            
            if ((INVALID_HANDLE_VALUE != h)
                && (INVALID_HANDLE_VALUE != (HANDLE)InterlockedCompareExchange ((LONG*)&hNotifyDriver, (LONG)h, (LONG)INVALID_HANDLE_VALUE))) {
                // some other thread has activated the driver and updated global variable
                CloseHandle (h);            
            }
        }
    }

    return hNotifyDriver != INVALID_HANDLE_VALUE;
}

//
// Flatten the user notification structure and notification trigger structure
// so that embedded pointers in these structures can be marshaled properly
// to the notification driver.
//
// Embedded pointers are just offsets to the actual start of the data from
// the beginning of the data (*ppOut). All the embedded pointers have to
// be passed this way as the calls could be coming from kernel threads in
// which case the notification driver cannot access the pointers (even if 
// we use marshaling helper functions CeOpenCallerBuffer/CeCloseCallerBuffer).
//
// Layout of the (*ppOut) is the following:
// {
//      copy of CE_NOTIFICATION_INFO_HEADER {};
//  --> start of CE_NOTIFICATION_TRIGGER structure contents
//  ...
//  --> end
//  --> start lpszApplication (flattened string data)
//  ...
//  --> end
//  --> start lpszArguments
//  ...
//  --> end
//  --> start of CE_USER_NOTIFICATION structure contents
//  ...
//  --> end
//  --> start pwszDialogTitle
//  ...
//  --> end
//  --> start pwszDialogText
//  ...
//  --> end
//  --> start pwszSound
//  ...
//  --> end (this is special since the length of this is nMaxSound rather than
//           the lenght of the pszSound string)
//  }
//
BOOL FlattenCeUserNotificationAndTrigger(
    PCE_NOTIFICATION_TRIGGER pcent,
    PCE_USER_NOTIFICATION pceun,
    DWORD dwVal, // could be HWND/HANDLE
    PBYTE*  ppOut,
    LPDWORD pdwSizeOfBufReturned
    )
{
    BOOL fRet = FALSE;
    DWORD ccbDialogTitle = 0;
    DWORD ccbDialogText = 0;
    DWORD ccbSound = 0;
    DWORD ccbApplication = 0;
    DWORD ccbArguments = 0;
    DWORD dwOffset = 0;
    DWORD dwFlatStructureSize = sizeof(CE_NOTIFICATION_INFO_HEADER);

    DEBUGCHK(ppOut);

    __try {
        // first compute the total size required

        if (pcent) {
            dwFlatStructureSize +=  sizeof(CE_NOTIFICATION_TRIGGER);

            if (pcent->lpszApplication) {
                ccbApplication = StrLenDWAligned(pcent->lpszApplication);
                dwFlatStructureSize += ccbApplication;
            }
            if (pcent->lpszArguments) {
                ccbArguments = StrLenDWAligned(pcent->lpszArguments);
                dwFlatStructureSize += ccbArguments;
            }
        }

        if (pceun) {
            dwFlatStructureSize +=  sizeof(CE_USER_NOTIFICATION);

            if (pceun->pwszDialogTitle) {
                ccbDialogTitle = StrLenDWAligned(pceun->pwszDialogTitle);
                dwFlatStructureSize += ccbDialogTitle;
            }
            if (pceun->pwszDialogText) {
                ccbDialogText = StrLenDWAligned(pceun->pwszDialogText);
                dwFlatStructureSize += ccbDialogText;
            }
            if (pceun->pwszSound) {
                // sound buffer is a special case; total length of the buffer is
                // given by pceun->nMaxSound whereas the string size could be
                // less than that. Hence we need to copy only string length bytes
                // but allocate space for nMaxSound in the flat structure.
                // no need to DWORD aligned the size as this is always the last 
                // entry in the buffer and nothing else is copied after this
                ccbSound = (wcslen(pceun->pwszSound) + 1) * sizeof(WCHAR);
                
                if (pceun->nMaxSound && (ccbSound > pceun->nMaxSound)) {
                    // invalid length specified
                    DEBUGCHK(0);
                    return FALSE;
                    
                } else if (!pceun->nMaxSound) {
                    // nMaxSound is optional for CeSetUserNotification(Ex) APIs
                    dwFlatStructureSize += ccbSound;
                    
                } else {
                    // nMaxSound is specified and ccbSound is < nMaxSound
                    dwFlatStructureSize += pceun->nMaxSound;
                }
            }
        }

        // allocate the byte stream to hold the entire flat structure
        *ppOut = new BYTE[dwFlatStructureSize];
        if (!*ppOut) {
            return FALSE;
        }

        // start copying data to the flattened buffer
        PCE_NOTIFICATION_INFO_HEADER pHeader = (PCE_NOTIFICATION_INFO_HEADER) *ppOut;
        memset(pHeader, 0, sizeof(CE_NOTIFICATION_INFO_HEADER));
        pHeader->hNotification = (HANDLE)dwVal;
        dwOffset = sizeof(CE_NOTIFICATION_INFO_HEADER);

        // copy the notification trigger
        if (pcent) {
            pHeader->pcent = (PCE_NOTIFICATION_TRIGGER) ((LPBYTE)pHeader + dwOffset);
            memcpy(pHeader->pcent, pcent, sizeof(CE_NOTIFICATION_TRIGGER));
            dwOffset += sizeof(CE_NOTIFICATION_TRIGGER);
            COPY_STRING_AND_UPDATE_OFFSET(pHeader, dwOffset, ccbApplication, pHeader->pcent->lpszApplication, pcent->lpszApplication);
            COPY_STRING_AND_UPDATE_OFFSET(pHeader, dwOffset, ccbArguments, pHeader->pcent->lpszArguments, pcent->lpszArguments);
            // don't refer to pHeader->pcent after this line as this is now an offset
            pHeader->pcent = (PCE_NOTIFICATION_TRIGGER) ((DWORD) pHeader->pcent - (DWORD) pHeader); 
        }

        // copy the user notification
        if (pceun) {    
            pHeader->pceun = (PCE_USER_NOTIFICATION) ((LPBYTE)pHeader + dwOffset);
            memcpy(pHeader->pceun, pceun, sizeof(CE_USER_NOTIFICATION));
            dwOffset += sizeof(CE_USER_NOTIFICATION);
            COPY_STRING_AND_UPDATE_OFFSET(pHeader, dwOffset, ccbDialogTitle, pHeader->pceun->pwszDialogTitle, pceun->pwszDialogTitle);
            COPY_STRING_AND_UPDATE_OFFSET(pHeader, dwOffset, ccbDialogText, pHeader->pceun->pwszDialogText, pceun->pwszDialogText);
            COPY_STRING_AND_UPDATE_OFFSET(pHeader, dwOffset, ccbSound, pHeader->pceun->pwszSound, pceun->pwszSound);
            // don't refer to pHeader->pceun after this line as this is now an offset
            pHeader->pceun = (PCE_USER_NOTIFICATION) ((DWORD) pHeader->pceun - (DWORD) pHeader); 
        }

        *pdwSizeOfBufReturned = dwFlatStructureSize;
        fRet = TRUE;

    } __except(EXCEPTION_EXECUTE_HANDLER) {
        *pdwSizeOfBufReturned = 0;
        fRet = FALSE;
    }

    return fRet;
}

// SDK exports
extern "C"
HANDLE
CeSetUserNotification(
    HANDLE h, // optional; 0 will create new notification
    WCHAR* lpszAppName,
    SYSTEMTIME* pstTime,
    CE_USER_NOTIFICATION* pceun // optional; could be NULL
    )
{
    HANDLE hRet = NULL;
    PBYTE pInBuf = NULL;
    DWORD dwSize = 0;
    CE_NOTIFICATION_TRIGGER cent = {0};

    if (! InitNotifyThunks ()) {
        SetLastError (ERROR_SERVICE_NOT_ACTIVE);
        return hRet;
    }

    if (!pstTime) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    cent.dwSize          = sizeof(cent);
    cent.dwType          = CNT_CLASSICTIME;
    cent.lpszApplication = lpszAppName;
    cent.stStartTime     = *pstTime;

    if (FlattenCeUserNotificationAndTrigger(&cent, pceun, (DWORD)h, &pInBuf, &dwSize)) {
        DeviceIoControl (hNotifyDriver, NOTFIDEV_IOCTL_CeSetUserNotificationEx, pInBuf, dwSize, &hRet, sizeof(hRet), NULL, NULL);        
    }

    if (pInBuf) {
        delete[] pInBuf;
    }

    return hRet;
}

extern "C"
HANDLE
CeSetUserNotificationEx(
    HANDLE h, // optional; 0 will create new notification
    CE_NOTIFICATION_TRIGGER* pcent,
    CE_USER_NOTIFICATION* pceun // optional; could be NULL
    )
{
    HANDLE hRet = NULL;
    PBYTE pInBuf = NULL;
    DWORD dwSize = 0;

    if (! InitNotifyThunks ()) {
        SetLastError (ERROR_SERVICE_NOT_ACTIVE);
        return hRet;
    }

    if (!pcent) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return hRet;
    }

    if (FlattenCeUserNotificationAndTrigger(pcent, pceun, (DWORD)h, &pInBuf, &dwSize)) {
        DeviceIoControl (hNotifyDriver, NOTFIDEV_IOCTL_CeSetUserNotificationEx, pInBuf, dwSize, &hRet, sizeof(hRet), NULL, NULL);
    }

    if (pInBuf) {
        delete[] pInBuf;
    }

    return hRet;
}


extern "C"
BOOL
CeClearUserNotification(
    HANDLE hNotification
    )
{
    if (! InitNotifyThunks ()) {
        SetLastError (ERROR_SERVICE_NOT_ACTIVE);
        return FALSE;
    }

    if (!hNotification) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    NotifyArgBuffer nab;
    memset (&nab, 0, sizeof(nab));

    nab.CeClearUserNotification_p.h = hNotification;

    return DeviceIoControl (hNotifyDriver, NOTFIDEV_IOCTL_CeClearUserNotification, &nab, sizeof(nab), NULL, 0, NULL, NULL);
}

extern "C"
BOOL
CeRunAppAtTime(
    WCHAR* lpszAppName,
    SYSTEMTIME* pstTime // optional; could be NULL
    )
{
    if (! InitNotifyThunks ()) {
        SetLastError (ERROR_SERVICE_NOT_ACTIVE);
        return FALSE;
    }

    if (!lpszAppName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    NotifyArgBuffer nab;
    memset (&nab, 0, sizeof(nab));

    if (SUCCEEDED(StringCchCopy(nab.CeRunAppAtTime_p.lpszAppName, MAX_PATH, lpszAppName))) {
        if (pstTime) {
            memcpy(&nab.CeRunAppAtTime_p.stTime, pstTime, sizeof(SYSTEMTIME));
        } else {
            nab.CeRunAppAtTime_p.fRemove = 1; // NULL pstTime implies remove any current notifications set for this app
        }
        
        return DeviceIoControl (hNotifyDriver, NOTFIDEV_IOCTL_CeRunAppAtTime, &nab, sizeof(nab), NULL, 0, NULL, NULL);

    }

    return FALSE;
}

extern "C"
BOOL
CeRunAppAtEvent(
    WCHAR* lpszAppName,
    LONG lWhichEvent
    )
{
    if (! InitNotifyThunks ()) {
        SetLastError (ERROR_SERVICE_NOT_ACTIVE);
        return FALSE;
    }

    if (!lpszAppName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    NotifyArgBuffer nab;
    memset (&nab, 0, sizeof(nab));

    nab.CeRunAppAtEvent_p.lWhichEvent = lWhichEvent;
    if (SUCCEEDED(StringCchCopy(nab.CeRunAppAtEvent_p.lpszAppName, MAX_PATH, lpszAppName))) {
        return DeviceIoControl (hNotifyDriver, NOTFIDEV_IOCTL_CeRunAppAtEvent, &nab, sizeof(nab), NULL, 0, NULL, NULL);
    }

    return FALSE;
}

extern "C"
BOOL
CeHandleAppNotifications(
    WCHAR* pwszAppName
    )
{
    if (! InitNotifyThunks ()) {
        SetLastError (ERROR_SERVICE_NOT_ACTIVE);
        return FALSE;
    }

    if (!pwszAppName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    NotifyArgBuffer nab;
    memset (&nab, 0, sizeof(nab));

    if (SUCCEEDED(StringCchCopy(nab.CeHandleAppNotifications_p.lpszAppName, MAX_PATH, pwszAppName))) {
        return DeviceIoControl (hNotifyDriver, NOTFIDEV_IOCTL_CeHandleAppNotifications, &nab, sizeof(nab), NULL, 0, NULL, NULL);
    }

    return FALSE;
}

extern "C"
BOOL
CeGetUserNotificationPreferences(
    HWND hWndParent, // optional; could be NULL
    CE_USER_NOTIFICATION *pceun // [in/out]
    )
{
    BOOL fRet = FALSE;

    if (! InitNotifyThunks ()) {
        SetLastError (ERROR_SERVICE_NOT_ACTIVE);
        return fRet;
    }

    if (!pceun) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return fRet;
    }

    PBYTE pInBuf = NULL;
    DWORD dwSizeOfBuf = 0;

    NOTIFY_RET_PREFERENCES_BUF ret;
    memset(&ret, 0, sizeof(NOTIFY_RET_PREFERENCES_BUF));

    if (FlattenCeUserNotificationAndTrigger(NULL, pceun, (DWORD)hWndParent, &pInBuf, &dwSizeOfBuf)) {
        if(DeviceIoControl (hNotifyDriver, NOTFIDEV_IOCTL_CeGetUserNotificationPreferences, pInBuf, dwSizeOfBuf, &ret, sizeof(ret), NULL, NULL)) {
            // copy the out arguments
            fRet = TRUE;
            pceun->ActionFlags = ret.ActionFlags;
            if (pceun->pwszSound && pceun->nMaxSound) {
                if (!CeSafeCopyMemory(pceun->pwszSound, ret.wszSound, pceun->nMaxSound))
                    fRet = FALSE;
            }            
        }
    }

    if (pInBuf) {
        delete[] pInBuf;
    }

    return fRet;
}

extern "C"
BOOL
CeEventHasOccurred(
    LONG lWhichEvent,
    WCHAR* pwszEndOfCommandLine // optional; could be NULL
    )
{
    if (! InitNotifyThunks ()) {
        SetLastError (ERROR_SERVICE_NOT_ACTIVE);
        return FALSE;
    }

    NotifyArgBuffer nab;
    memset (&nab, 0, sizeof(nab));
    
    if (pwszEndOfCommandLine) {
        StringCchCopy(nab.CeEventHasOccurred_p.pwszEndOfCommandLine, MAX_PATH, pwszEndOfCommandLine);
    } else {
        nab.CeEventHasOccurred_p.pwszEndOfCommandLine[0] = 0;
    }

    nab.CeEventHasOccurred_p.lWhichEvent = lWhichEvent;

    return DeviceIoControl (hNotifyDriver, NOTFIDEV_IOCTL_CeEventHasOccurred, &nab, sizeof(nab), NULL, 0, NULL, NULL);
}
      
extern "C"
BOOL
CeGetUserNotificationHandles(
    HANDLE* rghNotifications,
    DWORD cHandleCount,
    DWORD* cHandlesNeeded
    )
{
    if (! InitNotifyThunks ()) {
        SetLastError (ERROR_SERVICE_NOT_ACTIVE);
        return FALSE;
    }

    return DeviceIoControl (hNotifyDriver, NOTFIDEV_IOCTL_CeGetUserNotificationHandles, rghNotifications, sizeof(HANDLE) * cHandleCount, rghNotifications, sizeof(HANDLE) * cHandleCount, cHandlesNeeded, NULL);
}

extern "C"
BOOL
CeGetUserNotification(
    HANDLE hNotification,
    DWORD cBytes,
    DWORD* pcBytesNeeded,
    BYTE* pBuffer
    )
{
    if (! InitNotifyThunks ()) {
        SetLastError (ERROR_SERVICE_NOT_ACTIVE);
        return FALSE;
    }

    if(DeviceIoControl (hNotifyDriver, NOTFIDEV_IOCTL_CeGetUserNotification, &hNotification, sizeof(hNotification), pBuffer, cBytes, pcBytesNeeded, NULL)) {
        // need to re-adjust embedded pointers within the pBuffer structure
        PCE_NOTIFICATION_INFO_HEADER pHeader = (PCE_NOTIFICATION_INFO_HEADER)pBuffer;
        if (pHeader->pcent) {
            UPDATE_BASED_ON_OFFSET(pHeader->pcent, PCE_NOTIFICATION_TRIGGER, pBuffer);
            UPDATE_BASED_ON_OFFSET(pHeader->pcent->lpszApplication, LPTSTR, pBuffer);
            UPDATE_BASED_ON_OFFSET(pHeader->pcent->lpszArguments, LPTSTR, pBuffer);
        }
        if (pHeader->pceun) {
            UPDATE_BASED_ON_OFFSET(pHeader->pceun, PCE_USER_NOTIFICATION, pBuffer);
            UPDATE_BASED_ON_OFFSET(pHeader->pceun->pwszDialogTitle, LPTSTR, pBuffer);
            UPDATE_BASED_ON_OFFSET(pHeader->pceun->pwszDialogText, LPTSTR, pBuffer);
            UPDATE_BASED_ON_OFFSET(pHeader->pceun->pwszSound, LPTSTR, pBuffer);
            UPDATE_BASED_ON_OFFSET(pHeader->pceun->pExpansion, LPVOID, pBuffer);
        }
        return TRUE;
    }

    return FALSE;
}

extern "C"
DWORD
CeGetNotificationThreadId (void)
{
    if (! InitNotifyThunks ()) {
        SetLastError (ERROR_SERVICE_NOT_ACTIVE);
        return 0;
    }

    DWORD dw = 0;

    if (DeviceIoControl (hNotifyDriver, NOTFIDEV_IOCTL_CeGetNotificationThreadId, NULL, 0, &dw, sizeof(dw), NULL, NULL))
        return dw;

    return 0;
}
