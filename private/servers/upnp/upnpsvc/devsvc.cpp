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
#include <httpext.h>



CRITICAL_SECTION g_csUPNP;
LIST_ENTRY g_DevTreeList;

static HostedDeviceTree *FindDevTreeByName(PCWSTR pszDevTreeName)
{
    LIST_ENTRY *pLink;
    for (pLink = g_DevTreeList.Flink; pLink != &g_DevTreeList; pLink = pLink->Flink)
    {
        HostedDeviceTree *pDevTree;
        pDevTree = CONTAINING_RECORD(pLink, HostedDeviceTree, m_link);
        if (wcscmp(pszDevTreeName, pDevTree->Name()) == 0)
        {
            return pDevTree;
        }
    }
    return NULL;
}

static HostedDeviceTree *FindDevTreeSafe(PCWSTR pszDevName)
{
    HostedDeviceTree *pDevTree = NULL;
    EnterCriticalSection(&g_csUPNP);
    __try {
        pDevTree = FindDevTreeByName(pszDevName);

        if (!pDevTree)
        {
            SetLastError(ERROR_FILE_NOT_FOUND);
            __leave;
        }
        pDevTree->IncRef();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    LeaveCriticalSection(&g_csUPNP);

    return pDevTree;
}

// find HostedDevTree by UDN. If found, increment refcount and return pointer

static HostedDeviceTree *FindDevTreeByUDN(PCWSTR pszUDN)
{
    HostedDeviceTree    *pDevTree = NULL;
    LIST_ENTRY          *pLink;

    EnterCriticalSection(&g_csUPNP);
    for (pLink = g_DevTreeList.Flink; pLink != &g_DevTreeList; pLink = pLink->Flink)
    {
        pDevTree = CONTAINING_RECORD(pLink, HostedDeviceTree, m_link);

        if(NULL != pDevTree->FindDevice(pszUDN))
        {
            pDevTree->IncRef();
            break;
        }
    }

    LeaveCriticalSection(&g_csUPNP);

    if(pLink == &g_DevTreeList)
    {
        return NULL;
    }

    return pDevTree;
}

HostedDeviceTree *FindDevTreeAndServiceIdFromUri(PCSTR pszaUri, PWSTR *ppszUDN, PWSTR *ppszSid)
{
    // szaUri is  "UDN+ServiceID"
    const CHAR         *pszaSid;
    HostedDeviceTree   *pDevTree = NULL;
    PWSTR               pszUDN;

    *ppszUDN = NULL;
    *ppszSid = NULL;
    
    pszaSid = strchr(pszaUri, '+');

    if (!pszaSid)
    {
        return FALSE;
    }

    pszUDN = new WCHAR [pszaSid - pszaUri + 1];

    if (pszUDN) 
    {
        mbstowcs(pszUDN, pszaUri, pszaSid - pszaUri);
        pszUDN[pszaSid - pszaUri] = 0;  // null terminate;
        pDevTree = FindDevTreeByUDN(pszUDN);

        if(pDevTree)
        {
            *ppszUDN = pszUDN;
        }
        else        
        {
            delete [] pszUDN;
        }
    }

    pszaSid++;

    if (pDevTree)
    {
        if (ppszSid)
        {
            *ppszSid = StrDupAtoW(pszaSid);
            if (!*ppszSid)
            {
                pDevTree->DecRef();
                pDevTree = NULL;

                delete *ppszUDN;
                *ppszUDN = NULL;
            }
        }
    }

    return pDevTree;
}



/*
 *  UPnP Service Interface
 *
 *      - upnpsvc interface routines
 *      - parameter checking delegates
 *
 */


// FORWARD Declarations
BOOL UpnpAddDeviceImpl(__in HANDLE hOwner, __in UPNPDEVICEINFO* pDevInfo);
BOOL UpnpRemoveDeviceImpl(__in PCWSTR pszDeviceName);
BOOL UpnpPublishDeviceImpl(__in PCWSTR pszDeviceName);
BOOL UpnpUnpublishDeviceImpl(__in PCWSTR pszDeviceName); 
BOOL UpnpGetUDNImpl(__in PCWSTR pszDeviceName, __in PCWSTR pszTemplateUDN, __out ce::psl_buffer_wrapper<PWSTR>& UDNBuf, __out PDWORD pcchBuf);
BOOL UpnpGetSCPDPathImpl(__in PCWSTR pszDeviceName, __in PCWSTR pszUDN, __in PCWSTR pszServiceId, __out ce::psl_buffer_wrapper<PWSTR>& SCPDFilePath);
BOOL SetRawControlResponseImpl(__in DWORD hRequest, __in DWORD dwHttpStatus, __in PCWSTR pszResp);
BOOL UpnpSubmitPropertyEventImpl(__in PCWSTR pszDeviceName, __in PCWSTR pszUDN, __in PCWSTR pszServiceId, __in ce::psl_buffer_wrapper<UPNPPARAM*> Args);
BOOL UpnpUpdateEventedVariablesImpl(PCWSTR pszDeviceName, PCWSTR pszUDN, PCWSTR pszServiceId, ce::psl_buffer_wrapper<UPNPPARAM*> Args);
BOOL ReceiveInvokeRequestImpl(__in DWORD retCode, __out ce::psl_buffer_wrapper<PBYTE>& pReqBuf, __out PDWORD pcbReqBuf);



/*
 *  UpnpAddDeviceImpl:
 *
 *      - perform parameter checking then differ to delegate
 *
 */
BOOL UpnpAddDeviceImpl(__in HANDLE hOwner, __in ce::marshal_arg<ce::copy_in, UPNPDEVICEINFO*> pDevInfo)
{
    if (!pDevInfo || 
        pDevInfo->cbStruct != sizeof(UPNPDEVICEINFO) || 
        !pDevInfo->pszDeviceDescription || 
        !pDevInfo->pszDeviceName)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return UpnpAddDeviceImpl(hOwner, static_cast<UPNPDEVICEINFO*>(pDevInfo));
}



/*
 *  UpnpAddDeviceImpl:
 *
 *      - create a new HostedDeviceTree object representing the upnp device
 *
 */
BOOL UpnpAddDeviceImplInternal(HANDLE hOwner, UPNPDEVICEINFO* pDevInfo)
{
    DWORD error = NO_ERROR;
    BOOL fRet = FALSE;
    HostedDeviceTree *pDevTree;

    EnterCriticalSection(&g_csUPNP);

    pDevTree = FindDevTreeByName(pDevInfo->pszDeviceName);
    if (pDevTree)
    {
        error = ERROR_ALREADY_EXISTS;
        goto Finish;
    }

    pDevTree = new HostedDeviceTree();
    if (!pDevTree)
    {
        error = ERROR_OUTOFMEMORY;
        goto Finish;
    }

    // because we can't load MSXML in a borrowed thread without sideeffects
    // the following hack is to do the initialization in another thread

#pragma warning(push)
#pragma warning(disable:4068)  // warning C4068: unknown pragma
#pragma prefast(disable:11)    // prefast bug - doesn't understand variable declaration in a conditional

    if(struct HDTInitInfo *pinitInfo = new struct HDTInitInfo)
    {
        pinitInfo->pHDT = pDevTree;
        pinitInfo->devInfo.cbStruct = sizeof(pinitInfo->devInfo);
        pinitInfo->devInfo.pszDeviceDescription = StrDupW(pDevInfo->pszDeviceDescription);
        pinitInfo->devInfo.pszDeviceName = StrDupW(pDevInfo->pszDeviceName);
        pinitInfo->devInfo.pszUDN = StrDupW(pDevInfo->pszUDN);
        pinitInfo->devInfo.cachecontrol = pDevInfo->cachecontrol;
        pinitInfo->devInfo.pfCallback = pDevInfo->pfCallback;
        pinitInfo->devInfo.pvUserDevContext = pDevInfo->pvUserDevContext;
        pinitInfo->hOwner = hOwner;
        pinitInfo->hOwnerProc = GetCallingProcess();
        pinitInfo->fRet = FALSE;

        HANDLE h = CreateThread(NULL, 0, HostedDeviceTree::Initialize2, pinitInfo, 0, NULL);

        if(h)
        {
            DWORD dwWait;
            dwWait = WaitForSingleObject(h, INFINITE);
            Assert(dwWait == WAIT_OBJECT_0);
            CloseHandle(h);
        }

        fRet = pinitInfo->fRet;
        delete pinitInfo->devInfo.pszDeviceDescription;
        delete pinitInfo->devInfo.pszDeviceName;
        delete pinitInfo->devInfo.pszUDN;
        delete pinitInfo;
    }
#pragma prefast(enable:11)
#pragma warning(pop)


    if (!fRet)
    {
        delete pDevTree;
        pDevTree = NULL;
    }
    else
    {
        pDevTree->IncRef();     // set the refcount to 1
        pDevTree->AddToList(&g_DevTreeList);
    }

Finish:
    LeaveCriticalSection(&g_csUPNP);

    if (error)
    {
        fRet = FALSE;
        SetLastError(error);
    }
    return fRet;
}

BOOL UpnpAddDeviceImpl(__in HANDLE hOwner, __in UPNPDEVICEINFO* pDevInfo)
{
    return UpnpAddDeviceImplInternal(hOwner, pDevInfo);
}



/*
 *  UpnpRemoveDeviceImpl:
 *
 *      - perform parameter checking and differ to delegate
 *      - pszDeviceName must be non-null and not the empty-string
 *
 */
BOOL UpnpRemoveDeviceImpl(__in ce::marshal_arg<ce::copy_in, PCWSTR> pszDeviceName)
{
    if(!pszDeviceName || !*pszDeviceName)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return UpnpRemoveDeviceImpl((PCWSTR) pszDeviceName);
}



/*
 *  UpnpRemoveDeviceImpl:
 *
 *      - find the HostedDeviceTree from the device name
 *      - remove the HostedDeviceTree from the device list
 *      - shutdown the device and cleanup resource allocations
 *
 */
BOOL UpnpRemoveDeviceImplInternal(PCWSTR pszDeviceName)
{
    HostedDeviceTree *pDevTree = NULL;
    EnterCriticalSection(&g_csUPNP);

    pDevTree = FindDevTreeByName(pszDeviceName);

    if (!pDevTree)
    {
        SetLastError(ERROR_FILE_NOT_FOUND);
        goto Finish;
    }

    pDevTree->RemoveFromList();

Finish:
    LeaveCriticalSection(&g_csUPNP);

    if (pDevTree)
    {
        pDevTree->Shutdown();
        delete pDevTree;
        return TRUE;
    }

    return FALSE;
}

BOOL UpnpRemoveDeviceImpl(__in PCWSTR pszDeviceName)
{
    return UpnpRemoveDeviceImplInternal(pszDeviceName);
}



/*
 *  UpnpPublishDeviceImpl:
 *
 *      - perform parameter checking and differ to delegate
 *      - pszDeviceName must be non-null and not the empty-string
 *
 */
BOOL UpnpPublishDeviceImpl(__in ce::marshal_arg<ce::copy_in, PCWSTR> pszDeviceName)
{
    if(!pszDeviceName || !*pszDeviceName)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return UpnpPublishDeviceImpl((PCWSTR) pszDeviceName);
}



/*
 *  UpnpPublishDeviceImpl:
 *
 *      - publish the specified device
 *
 */
BOOL UpnpPublishDeviceImplInternal(PCWSTR pszDeviceName)
{
    BOOL fRet;
    HostedDeviceTree *pDevTree;

    pDevTree = FindDevTreeSafe(pszDeviceName);

    fRet = FALSE;
    if (pDevTree)
    {
        fRet = pDevTree->Publish();
        pDevTree->DecRef();
    }
    return fRet;
}
BOOL UpnpPublishDeviceImpl(__in PCWSTR pszDeviceName)
{
    return UpnpPublishDeviceImplInternal(pszDeviceName);
}



/*
 *  UpnpUnpublishDeviceImpl:
 * 
 *      - perform parameter checking and differ to delegate
 *      - pszDeviceName must be non-null and not the empty-string
 *
 */
BOOL UpnpUnpublishDeviceImpl(__in ce::marshal_arg<ce::copy_in, PCWSTR> pszDeviceName)
{
    if(!pszDeviceName || !*pszDeviceName)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return UpnpUnpublishDeviceImpl((PCWSTR) pszDeviceName);
}


/*
 *  UpnpUnpublishDeviceImpl:
 *
 *      - unpublish the specified device
 *
 */
BOOL UpnpUnpublishDeviceImplInternal(PCWSTR pszDeviceName)
{
    BOOL fRet;
    HostedDeviceTree *pDevTree;

    pDevTree = FindDevTreeSafe(pszDeviceName);

    fRet = FALSE;
    if (pDevTree)
    {
        fRet = pDevTree->Unpublish();
        pDevTree->DecRef();
    }
    return fRet;
}

BOOL UpnpUnpublishDeviceImpl(__in PCWSTR pszDeviceName)
{
    return UpnpUnpublishDeviceImplInternal(pszDeviceName);
}



/*
 *  UpnpGetUDNImpl:
 *
 *      - ensure that the device name is not null or empty string
 *      - ensure that the UDN buffer count is not 0
 *      - ensure that the UDN buffer size pointer is not null
 *
 */
BOOL
UpnpGetUDNImpl(
    __in ce::marshal_arg<ce::copy_in, PCWSTR>                            pszDeviceName,      // [in] local device name
    __in ce::marshal_arg<ce::copy_in, PCWSTR>                            pszTemplateUDN,     // [in, optional] the UDN element in the original device description
    __out ce::marshal_arg<ce::copy_out, ce::psl_buffer_wrapper<PWSTR> >  UDNBuf,             // [out] UDN buffer
    __out ce::marshal_arg<ce::copy_out, PDWORD>                          pcchBuf)            // [out] size of buffer/ length filled (in WCHARs)
{
    if(!pszDeviceName || !*pszDeviceName)
    {
        goto Finish;
    }

    if(!(UDNBuf.count()) || !pcchBuf)
    {
        goto Finish;
    }

    return UpnpGetUDNImpl((PCWSTR) pszDeviceName,
                          static_cast<PCWSTR>(pszTemplateUDN),
                          static_cast<ce::psl_buffer_wrapper<PWSTR> >(UDNBuf),
                          static_cast<PDWORD>(pcchBuf));

Finish:
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
}



/*
 *  UpnpGetUDNImpl:
 *
 *      - get device UDN based on device name
 *      - copy UDN into the UDN buffer and indicate the size in pcchBuf
 *
 */
BOOL UpnpGetUDNImplInternal(PCWSTR pszDeviceName, PCWSTR pszTemplateUDN, PWSTR UDNBuf, DWORD cchBuf, PDWORD pcchBuf)
{
    BOOL fRet = FALSE;
    HostedDeviceTree *pDevTree;

    pDevTree = FindDevTreeSafe(pszDeviceName);

    if (pDevTree)
    {
        HostedDevice *pDev;
        pDev = pDevTree->FindDeviceByOrigUDN(pszTemplateUDN);
        if (pDev && pDev->UDN())
        {
            DWORD cch = wcslen(pDev->UDN())+1;
            if (cchBuf >= cch)
            {
                wcscpy(UDNBuf, pDev->UDN());
                fRet = TRUE;
            }
            else
            {
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
            }

            *pcchBuf = cch; // set the required length in any case
        }
        else
        {
            SetLastError(ERROR_NOT_FOUND);
        }

        pDevTree->DecRef();
    }

    return fRet;
}

BOOL UpnpGetUDNImpl(__in PCWSTR pszDeviceName, __in PCWSTR pszTemplateUDN, __out ce::psl_buffer_wrapper<PWSTR>& UDNBuf, __out PDWORD pcchBuf)
{
    return UpnpGetUDNImplInternal(pszDeviceName, pszTemplateUDN, UDNBuf.buffer(),UDNBuf.count(), pcchBuf);
}


/*
 *  UpnpGetSCPDPathImpl:
 *
 *      - device name must not be null or empty string to retrieve a device record
 *      - udn and service id must not be null or empty string to retrieve a service record
 *      - the service control point description count must not be zero to get the path
 *
 */
BOOL
UpnpGetSCPDPathImpl(
    __in ce::marshal_arg<ce::copy_in, PCWSTR>                            pszDeviceName,   // [in] local device name
    __in ce::marshal_arg<ce::copy_in, PCWSTR>                            pszUDN,          // [in] device UDN
    __in ce::marshal_arg<ce::copy_in, PCWSTR>                            pszServiceId,    // [in] serviceId (from device description)
    __out ce::marshal_arg<ce::copy_out, ce::psl_buffer_wrapper<PWSTR> >  SCPDFilePath)    // [out] service control point description file path
{
    if(!pszDeviceName || !*pszDeviceName)
    {
        goto Finish;
    }

    if(!pszUDN || !*pszUDN)
    {
        goto Finish;
    }

    if(!pszServiceId || !*pszServiceId)
    {
        goto Finish;
    }

    if(!(SCPDFilePath.count()))
    {
        goto Finish;
    }

    return UpnpGetSCPDPathImpl((PCWSTR) pszDeviceName, (PCWSTR) pszUDN, (PCWSTR) pszServiceId, static_cast<ce::psl_buffer_wrapper<PWSTR> >(SCPDFilePath));

Finish:
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
}



/*
 *  UpnpGetSCPDPathImpl:
 *
 *      - get the upnp service control point description path
 *
 */
BOOL UpnpGetSCPDPathImplInternal(PCWSTR pszDeviceName, PCWSTR pszUDN, PCWSTR pszServiceId, PWSTR SCPDFilePath, DWORD cchSCPDFilePath)
{
    BOOL fRet = FALSE;
    HostedDeviceTree *pDevTree;
    HostedService *pService;

    pDevTree = FindDevTreeSafe(pszDeviceName);

    if (pDevTree)
    {
        pService = pDevTree->FindService(pszUDN, pszServiceId);

        if (pService)
        {
            PCWSTR pszPath;
            if (pszPath = pService->SCPDPath())
            {
                DWORD cch;
                cch = wcslen(pszPath)+1;
                if (*pszPath != '\\')
                {
                    // the path is not absolute. Prepend the upnp resource path.
                    cch += celems(c_szLocalWebRootDir) - 1;
                }

                if (cchSCPDFilePath >= cch)
                {
                    PWSTR pszSCPDFilePath = SCPDFilePath;

                    *pszSCPDFilePath = 0;
                    if (*pszPath != '\\')
                    {
                        wcscpy(pszSCPDFilePath, c_szLocalWebRootDir);
                    }
                    wcscat(pszSCPDFilePath, pszPath);
                    fRet = TRUE;
                }
                else
                {
                    SetLastError(ERROR_INSUFFICIENT_BUFFER);
                }
            }
            else
            {
                SetLastError(ERROR_NOT_FOUND);
            }
        }
        else
        {
            SetLastError(ERROR_NO_MATCH);  // error ?
        }

        pDevTree->DecRef();
    }

    return fRet;
}

BOOL UpnpGetSCPDPathImpl(__in PCWSTR pszDeviceName, __in PCWSTR pszUDN, __in PCWSTR pszServiceId, __out ce::psl_buffer_wrapper<PWSTR>& SCPDFilePath)
{
    return UpnpGetSCPDPathImplInternal(pszDeviceName, pszUDN, pszServiceId, SCPDFilePath.buffer(), SCPDFilePath.count());
}


/*
 *  SetRawControlResponseImpl:
 *
 *      - ensure that the response string is not null or empty string
 *
 */
BOOL SetRawControlResponseImpl(__in DWORD hRequest, __in DWORD dwHttpStatus, __in ce::marshal_arg<ce::copy_in, PCWSTR> pszResp)
{
    if(!pszResp || !*pszResp)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return SetRawControlResponseImpl(hRequest, dwHttpStatus, (PCWSTR) pszResp);
}


/*
 *  SetRawControlResponseImpl:
 *
 *  - called from the device implementation, while processing the UPnP control request
 *  - locate the owning ControlRequest from the UPNPSERVICECONTROL structure and
 *  - set the response bytes that are to be returned to the control point
 *
 */
BOOL SetRawControlResponseImplInternal(DWORD hRequest, DWORD dwHttpStatus, PCWSTR pszResp)
{
    HostedDeviceTree *pDevTree;
    BOOL fRet = FALSE;

    pDevTree = FindDevTreeSafe((PCWSTR)hRequest);
    if (pDevTree)
    {
        fRet = pDevTree->SetControlResponse(dwHttpStatus, pszResp);
        pDevTree->DecRef();
    }

    return fRet;
}
BOOL SetRawControlResponseImpl(__in DWORD hRequest, __in DWORD dwHttpStatus, __in PCWSTR pszResp)
{
    return SetRawControlResponseImplInternal(hRequest, dwHttpStatus, pszResp);
}



/*
 * UpnpSubmitPropertyEventImpl:
 *
 *      - device name must be non-null and not empty
 *      - udn and service id must be non-null and not the empty string
 *      - the number of UPNPPARAMS must not be zero
 *
 */
BOOL
UpnpSubmitPropertyEventImpl(
    __in ce::marshal_arg<ce::copy_in, PCWSTR>    pszDeviceName,
    __in ce::marshal_arg<ce::copy_in, PCWSTR>    pszUDN,
    __in ce::marshal_arg<ce::copy_in, PCWSTR>    pszServiceId,
    __in ce::marshal_arg<ce::copy_array_in, ce::psl_buffer_wrapper<UPNPPARAM*> > Args)
{
    if(!pszDeviceName || !*pszDeviceName)
    {
        goto Finish;
    }

    if(!pszUDN || !*pszUDN)
    {
        goto Finish;
    }

    if(!pszServiceId || !*pszServiceId)
    {
        goto Finish;
    }

    if(!Args.count())
    {
        goto Finish;
    }

    return UpnpSubmitPropertyEventImpl((PCWSTR) pszDeviceName, (PCWSTR) pszUDN, (PCWSTR) pszServiceId, static_cast<ce::psl_buffer_wrapper<UPNPPARAM*> >(Args));

Finish:
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
}



/*
 *  UpnpSubmitPropertyEventImpl:
 *
 *      - notifies the target service of parameter eventing
 *
 */
BOOL UpnpSubmitPropertyEventImplInternal(PCWSTR pszDeviceName, PCWSTR pszUDN, PCWSTR pszServiceId, UPNPPARAM* pArgs, DWORD cArgs)
{
    BOOL fRet = FALSE;
    HostedDeviceTree *pDevTree = NULL;

    pDevTree = FindDevTreeSafe(pszDeviceName);
    if (pDevTree)
    {
        fRet = pDevTree->SubmitPropertyEvent(pszUDN,
                                             pszServiceId,
                                             F_MARK_MODIFIED,
                                             cArgs,
                                             pArgs);
        pDevTree->DecRef();
    }

    return fRet;
}
BOOL UpnpSubmitPropertyEventImpl(__in PCWSTR pszDeviceName, __in PCWSTR pszUDN, __in PCWSTR pszServiceId, __in ce::psl_buffer_wrapper<UPNPPARAM*> Args)
{
    return UpnpSubmitPropertyEventImplInternal(pszDeviceName, pszUDN, pszServiceId, Args.buffer(), Args.count());
}



/*
 *  UpnpUpdateEventedVariablesImpl:
 *
 *      - device name must be non-null and not empty
 *      - udn and service id must be non-null and not the empty string
 *      - the number of UPNPPARAMS must not be zero
 *
 */
BOOL
UpnpUpdateEventedVariablesImpl(
    __in ce::marshal_arg<ce::copy_in, PCWSTR>    pszDeviceName,
    __in ce::marshal_arg<ce::copy_in, PCWSTR>    pszUDN,
    __in ce::marshal_arg<ce::copy_in, PCWSTR>    pszServiceId,
    __in ce::marshal_arg<ce::copy_array_in, ce::psl_buffer_wrapper<UPNPPARAM*> > Args)
{
    if(!pszDeviceName || !*pszDeviceName)
    {
        goto Finish;
    }

    if(!pszUDN || !*pszUDN)
    {
        goto Finish;
    }

    if(!pszServiceId || !*pszServiceId)
    {
        goto Finish;
    }

    if(!Args.count())
    {
        goto Finish;
    }

    return UpnpUpdateEventedVariablesImpl((PCWSTR) pszDeviceName, (PCWSTR) pszUDN, (PCWSTR) pszServiceId, static_cast<ce::psl_buffer_wrapper<UPNPPARAM*> >(Args));

Finish:
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
}



/*
 *  UpnpUpdateEventedVariablesImpl:
 *
 *
 */
BOOL UpnpUpdateEventedVariablesImplInternal(PCWSTR pszDeviceName, PCWSTR pszUDN, PCWSTR pszServiceId, UPNPPARAM* pArgs, DWORD cArgs)
{
    BOOL fRet = FALSE;
    HostedDeviceTree *pDevTree = NULL;

    pDevTree = FindDevTreeSafe(pszDeviceName);
    if (pDevTree)
        {
            fRet = pDevTree->SubmitPropertyEvent(pszUDN,
                                                 pszServiceId,
                                                 0,
                                                 cArgs,
                                                 pArgs);
            pDevTree->DecRef();
        }

    return fRet;
}

BOOL UpnpUpdateEventedVariablesImpl(__in PCWSTR pszDeviceName, __in PCWSTR pszUDN, __in PCWSTR pszServiceId, __in ce::psl_buffer_wrapper<UPNPPARAM*> Args)
{
    return UpnpUpdateEventedVariablesImplInternal(pszDeviceName, pszUDN, pszServiceId, Args.buffer(), Args.count());
}



/*
 *  InitializeReceiveInvokeRequestImpl:
 *
 *      - initializes the environment for the calling user-mode process of the service
 *      - creates a dispatch gate
 *      - if the dispatch gate is already in the system then this routine returns false
 *
 */
BOOL 
InitializeReceiveInvokeRequestImplInternal()
{
    return DispatchGate::AddDispatchGateForProc(GetCallingProcess());
}

BOOL 
InitializeReceiveInvokeRequestImpl()
{
    return InitializeReceiveInvokeRequestImplInternal();
}



/*
 *  ReceiveInvokeRequestImpl:
 *
 *      - ensure that the invoke request buffer is valid
 *      - ensure that the invoke request size buffer is valid
 *
 */
BOOL 
ReceiveInvokeRequestImpl(
    __in DWORD retCode, 
    __out ce::marshal_arg<ce::copy_out, ce::psl_buffer_wrapper<PBYTE> > pReqBuf, 
    __out ce::marshal_arg<ce::copy_out, PDWORD> pcbReqBuf)
{
    if(!pReqBuf.count() || !pcbReqBuf)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return ReceiveInvokeRequestImpl(retCode, static_cast<ce::psl_buffer_wrapper<PBYTE> >(pReqBuf), static_cast<PDWORD>(pcbReqBuf));
}



/**
 *  ReceiveInvokeRequestImpl:
 *
 *      - blocks waiting to receive a UPNP message for the calling usermode process
 *      - reuses an existing dispatch gate for the process or creates a new one
 *      - returns to the caller once a new UPNP message has been received
 *      - copies the new UPNP message into the users buffer
 *
 */
BOOL ReceiveInvokeRequestImplInternal(__in DWORD retCode, PBYTE pReqBuf, DWORD cbReqBuf, PDWORD pcbReqBuf)
{
    HANDLE hCallerProc = GetCallerProcess();
    DispatchGate *pCBGate = NULL;
    BOOL fRet = TRUE;

    if ( hCallerProc == NULL ) 
        hCallerProc = (HANDLE)GetCurrentProcessId();  // assume call is from current process

    TraceTag(ttidTrace, "%s: Receive Next Invoke Request for Process[%08x]\n", __FUNCTION__, hCallerProc);

    // get the dispatch gate for the calling process
    pCBGate = DispatchGate::FindDispatchGateForProc(hCallerProc);

    if (!pCBGate)
    {
        // this can occur on shutdown of the request handler thread
        TraceTag(ttidError, "%s: Could not find DispatchGate for Process[%08x]\n", __FUNCTION__, hCallerProc);
        return FALSE;
    }


    // if we should notify the invoke request thread
    pCBGate->CheckResetMutexRequestor(retCode);


    TraceTag(ttidControl, "%s: get UPNP message for Process [%08x]\n", __FUNCTION__, hCallerProc);
    pCBGate->SetCallerBuffer(pReqBuf, cbReqBuf, pcbReqBuf);
    fRet = pCBGate->WaitForRequests(INFINITE);
    if (!fRet)
    {
        // exit if there is an error may be a shutdown request
        TraceTag(ttidControl, "%s: failure waiting for incoming request for Process [%08x]\n", __FUNCTION__, hCallerProc);
        goto Finish;
    }

    fRet = pCBGate->DispatchRequest();
    if (!fRet)
    {
        TraceTag(ttidControl, "%s: failure dispatching incoming request for Proc %x\n", __FUNCTION__, hCallerProc);
    }

Finish:
    TraceTag(ttidControl, "%s: returning received UPNP message for Process[%08x] [%S]\n", __FUNCTION__, hCallerProc, fRet ? L"TRUE" : L"FALSE");
    pCBGate->DecRef();
    return fRet;
}

BOOL ReceiveInvokeRequestImpl(__in DWORD retCode, __out ce::psl_buffer_wrapper<PBYTE>& pReqBuf, __out PDWORD pcbReqBuf)
{
    return ReceiveInvokeRequestImplInternal(retCode, pReqBuf.buffer(), pReqBuf.count(), pcbReqBuf);
}



/**
 *  CancelReceiveInvokeRequestImpl:
 *
 *
 *
 */
BOOL 
CancelReceiveInvokeRequestImplInternal()
{
    DispatchGate *pCBGate = NULL;
    HANDLE hProc = GetCallingProcess();
    TraceTag(ttidControl, "%s:  entered for Process[%08x]\n", __FUNCTION__, hProc);

    pCBGate = DispatchGate::RemoveDispatchGateForProc(hProc);
    if (pCBGate)
    {
        BOOL fRet;
        EnterCriticalSection(&g_csUPNP);
        fRet =  pCBGate->Shutdown();    // mark the thread as shutting down
        LeaveCriticalSection(&g_csUPNP);
        pCBGate->DecRef();  // release this reference
        return fRet;
    }

    return FALSE;
}

BOOL 
CancelReceiveInvokeRequestImpl()
{
    return CancelReceiveInvokeRequestImplInternal();
}



BOOL SubscribeCallback(BOOL fSubscribe, PSTR pszUri)
{
    HostedDeviceTree    *pDevTree;
    LPWSTR              pszServiceId;
    LPWSTR              pszUDN;
    BOOL                fRet = FALSE;

    pDevTree = FindDevTreeAndServiceIdFromUri(pszUri, &pszUDN, &pszServiceId);

    if (pDevTree)
    {
        Assert(pszServiceId);
        Assert(pszUDN);

        fRet = fSubscribe ? pDevTree->Subscribe(pszUDN, pszServiceId) : pDevTree->Unsubscribe(pszUDN, pszServiceId);
        pDevTree->DecRef();

        delete [] pszServiceId;
        delete [] pszUDN;
    }

    return fRet;
}

//
// Called when a process is going away.
// There is an assumption that only one handle is opened by a process
// We have to remove all the device trees hosted by the going-away process
//
void
UpnpCleanUpProc(
    HANDLE hOwner
)
{
    LIST_ENTRY *pLink;
    HostedDeviceTree *pDevTree = NULL;
    while (1)
    {
        EnterCriticalSection(&g_csUPNP);
        for (pLink = g_DevTreeList.Flink; pLink != &g_DevTreeList; pLink = pLink->Flink)
        {
            pDevTree = CONTAINING_RECORD(pLink, HostedDeviceTree, m_link);
            if (pDevTree->Owner() == hOwner)
            {
                pDevTree->RemoveFromList();
                break;
            }
        }
        LeaveCriticalSection(&g_csUPNP);
        if (pLink == &g_DevTreeList)
        {
            break;  // no more devices hosted in the specified process
        }

        pDevTree->Shutdown(HDT_NO_CALLBACKS);
        delete pDevTree;

    }
}
