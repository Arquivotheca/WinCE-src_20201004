//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <msgqueue.h>
#include <pnp.h>

// --------------------------------------------------------------------
// --------------------------------------------------------------------
typedef struct _DEV_DETECT_INFO
{
    HANDLE hMsgQueue;
    HANDLE hNotify;
    
} DEV_DETECT_INFO, *PDEV_DETECT_INFO;

// --------------------------------------------------------------------
// --------------------------------------------------------------------
BOOL DEV_DetectNextDevice (
        IN HANDLE hDetect, 
        OUT WCHAR *pszDevName, 
        IN DWORD cchDevName
        )
{
    DWORD cbRead, dwFlags;
    BYTE rgbData[1024];
    PDEVDETAIL pDevDetail = (PDEVDETAIL)rgbData;
    PDEV_DETECT_INFO pInfo = (PDEV_DETECT_INFO)hDetect;

    ASSERT(pInfo);
    ASSERT(pInfo->hNotify);
    ASSERT(pInfo->hMsgQueue);
    
    if(NULL == pInfo || NULL == pInfo->hNotify || NULL == pInfo->hMsgQueue)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto fail;
    }

    for(;;)
    {
        if(!ReadMsgQueue(pInfo->hMsgQueue, rgbData, sizeof(rgbData), &cbRead, 0, &dwFlags))
        {
            SetLastError(ERROR_NO_MORE_ITEMS);
            goto fail;
        }
        
        if(pDevDetail->fAttached)
        {            
            // found an attached device of the specified guid, copy the name of the
            // device to the output buffer and exit
            if (FAILED(StringCchCopy(pszDevName, cchDevName, pDevDetail->szName)))
            {
                goto fail;
            }
            break;
        }
    }

    return TRUE;

fail:
    return FALSE;
}


// --------------------------------------------------------------------
// --------------------------------------------------------------------
HANDLE DEV_DetectFirstDevice (
        IN const GUID *devclass, 
        OUT WCHAR *pszDevName, 
        IN DWORD cchDevName
        )
{
    MSGQUEUEOPTIONS msgqopts = {0};    
    PDEV_DETECT_INFO pInfo = NULL;
    
    if(NULL == pszDevName || 0 == cchDevName)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto fail;
    }

    // allocate detect info struct
    pInfo = (PDEV_DETECT_INFO)LocalAlloc(LMEM_FIXED, sizeof(DEV_DETECT_INFO));
    if(!pInfo)
        goto fail;

    
    msgqopts.dwSize = sizeof(MSGQUEUEOPTIONS);
    msgqopts.dwFlags = 0;
    msgqopts.cbMaxMessage = 1024;
    msgqopts.bReadAccess = TRUE;
    
    pInfo->hMsgQueue = CreateMsgQueue(NULL, &msgqopts);
    if(NULL == pInfo->hMsgQueue)
        goto fail;

    pInfo->hNotify = RequestDeviceNotifications(devclass, pInfo->hMsgQueue, TRUE);
    if(NULL == pInfo->hNotify)
        goto fail;

    if(!DEV_DetectNextDevice((HANDLE)pInfo, pszDevName, cchDevName))
        goto fail;
    
    return (HANDLE)pInfo;

fail:
    if(pInfo)
        VERIFY(NULL == LocalFree((HLOCAL)pInfo));
    return NULL;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
void DEV_DetectClose (
        IN HANDLE hDetect
        )
{
    PDEV_DETECT_INFO pInfo = (PDEV_DETECT_INFO)hDetect;

    ASSERT(pInfo);
    ASSERT(pInfo->hNotify);
    ASSERT(pInfo->hMsgQueue);

    if(pInfo)
    {
        if(pInfo->hNotify)
            VERIFY(StopDeviceNotifications(pInfo->hNotify));

        if(pInfo->hMsgQueue)
            VERIFY(CloseMsgQueue(pInfo->hMsgQueue));

        VERIFY(NULL == LocalFree((HLOCAL)pInfo));
    }
}

