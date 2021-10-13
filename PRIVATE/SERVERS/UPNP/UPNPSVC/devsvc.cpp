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
			return pDevTree;
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
		return NULL;
	
	return pDevTree;
}

HostedDeviceTree *FindDevTreeAndServiceIdFromUri(PCSTR pszaUri, PWSTR *ppszUDN, PWSTR *ppszSid)
{
	// szaUri is  "UDN+ServiceID"
	CHAR                *pszaSid;
	HostedDeviceTree    *pDevTree = NULL;
	PWSTR               pszUDN;
	
    *ppszUDN = NULL;
    *ppszSid = NULL;
    
    pszaSid = strchr(pszaUri, '+');
	
    if (!pszaSid)
		return FALSE;
	
	pszUDN = new WCHAR [pszaSid - pszaUri + 1];

	if (pszUDN) {
		mbstowcs(pszUDN, pszaUri, pszaSid - pszaUri);
		pszUDN[pszaSid - pszaUri] = 0;	// null terminate;
		pDevTree = FindDevTreeByUDN(pszUDN);
		
        if(pDevTree)
            *ppszUDN = pszUDN;
        else        
            delete [] pszUDN;
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
    Device creation
 */
BOOL
WINAPI
UpnpAddDeviceImpl(IN HANDLE hOwner,  IN UPNPDEVICEINFO *pDevInfo)
{
	DWORD error = NO_ERROR;
	BOOL fRet = FALSE;
	HostedDeviceTree *pDevTree;
	EnterCriticalSection(&g_csUPNP);
	__try {
		if (!pDevInfo || pDevInfo->cbStruct != sizeof(UPNPDEVICEINFO)
			|| !pDevInfo->pszDeviceDescription
			|| !pDevInfo->pszDeviceName
			)
		{
			error = ERROR_INVALID_PARAMETER;
			__leave;
		}
		pDevTree = FindDevTreeByName(pDevInfo->pszDeviceName);
		if (pDevTree)
		{
			error = ERROR_ALREADY_EXISTS;
			__leave;
		}
		pDevTree = new HostedDeviceTree();
		if (!pDevTree)
		{
			error = ERROR_OUTOFMEMORY;
			__leave;
		}
#if UNDER_CE
		// Because we can't load MSXML in a borrowed thread without sideeffects
		// the following hack is to do the initialization in another thread
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
            pinitInfo->hOwnerProc = GetCallerProcess();
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
		    if (pinitInfo->devInfo.pszDeviceDescription)
			    delete pinitInfo->devInfo.pszDeviceDescription;
		    if (pinitInfo->devInfo.pszDeviceName)
			    delete pinitInfo->devInfo.pszDeviceName;
		    if (pinitInfo->devInfo.pszUDN)
			    delete pinitInfo->devInfo.pszUDN;
		    delete pinitInfo;
        }
#else
		fRet = pDevTree->Initialize(pDevInfo);
#endif
		
		if (!fRet)
		{
			delete pDevTree;
			pDevTree = NULL;
		}
		else
		{
			pDevTree->IncRef();		// set the refcount to 1
			pDevTree->AddToList(&g_DevTreeList);
		}
		
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		error = ERROR_INVALID_PARAMETER;
	}
	LeaveCriticalSection(&g_csUPNP);

	if (error)
	{
		fRet = FALSE;
		SetLastError(error);
	}
	return fRet;
}

BOOL
WINAPI
UpnpRemoveDevice(
    PCWSTR pszDeviceName)
{
	HostedDeviceTree *pDevTree = NULL;
	EnterCriticalSection(&g_csUPNP);
	__try {
		pDevTree = FindDevTreeByName(pszDeviceName);

		if (!pDevTree)
		{
			SetLastError(ERROR_FILE_NOT_FOUND);
			__leave;
		}

		pDevTree->RemoveFromList();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
	}
	LeaveCriticalSection(&g_csUPNP);

	if (pDevTree)
	{
		pDevTree->Shutdown();
		delete pDevTree;
		return TRUE;
	}

	return FALSE;
		
}

/*
  Publication control
*/


BOOL
WINAPI
UpnpPublishDevice(
    PCWSTR pszDeviceName)
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

BOOL
WINAPI
UpnpUnpublishDevice(
    PCWSTR pszDeviceName)
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

/*
	Device description info
*/
BOOL
WINAPI
UpnpGetUDN(
    IN PCWSTR pszDeviceName,    // [in] local device name
    IN PCWSTR pszTemplateUDN,   // [in, optional] the UDN element in the original device description
    OUT PWSTR pszUDNBuf,        // [in] buffer to hold the assigned UDN
    IN OUT PDWORD pchBuf)       // [in,out] size of buffer/ length filled (in WCHARs)
{
	BOOL fRet = FALSE;
	HostedDeviceTree *pDevTree;

	pDevTree = FindDevTreeSafe(pszDeviceName);
	
	if (pDevTree)
	{
		__try {
			HostedDevice *pDev;
			pDev = pDevTree->FindDeviceByOrigUDN(pszTemplateUDN);
			if (pDev && pDev->UDN())
			{
				DWORD cch = wcslen(pDev->UDN())+1;
				if (*pchBuf >= cch && pszUDNBuf)
				{	
					wcscpy(pszUDNBuf, pDev->UDN());
					fRet = TRUE;
				}
				else
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
				*pchBuf = cch;	// set the required length in any case
			}
			else
				SetLastError(ERROR_NOT_FOUND);
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			SetLastError(ERROR_INVALID_PARAMETER);
		}
		pDevTree->DecRef();
	}

	return fRet;
}
    
BOOL
WINAPI
UpnpGetSCPDPath(
    IN PCWSTR pszDeviceName,   // [in] local device name
    IN PCWSTR pszUDN,          // [in] device UDN
    IN PCWSTR pszServiceId,    // [in] serviceId (from device description)
    OUT PWSTR  pszSCPDFilePath,// [out] file path to SCPD
    IN DWORD  cchFilePath)     // [in] size of  pszSCPDFilePath in WCHARs
{
	BOOL fRet = FALSE;
	HostedDeviceTree *pDevTree;
	HostedService *pService;

	pDevTree = FindDevTreeSafe(pszDeviceName);
	
	if (pDevTree)
	{
		__try {
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
					if (cchFilePath >= cch)
					{
						*pszSCPDFilePath = 0;
						if (*pszPath != '\\')
							wcscpy(pszSCPDFilePath, c_szLocalWebRootDir);
						wcscat(pszSCPDFilePath, pszPath);
						fRet = TRUE;
					}
					else
						SetLastError(ERROR_INSUFFICIENT_BUFFER);
				}
				else
					SetLastError(ERROR_NOT_FOUND);
			}
			else
				SetLastError(ERROR_NO_MATCH);	// error ?
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			SetLastError(ERROR_INVALID_PARAMETER);
		}
		pDevTree->DecRef();
	}

	return fRet;
}
/*
    Control
*/
//
// Called from the device implementation, while processing the UPnP control request
// Locate the owning ControlRequest from the UPNPSERVICECONTROL structure and
// set the response bytes that are to be returned to the control point.
//
BOOL
SetRawControlResponse(DWORD hRequest, DWORD dwHttpStatus, PCWSTR pszResp)
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

/*
  Eventing

*/
UpnpSubmitPropertyEvent(
    PCWSTR pszDeviceName,
    PCWSTR pszUDN,
    PCWSTR pszServiceId,
    DWORD nArgs,
    UPNPPARAM *rgArgs)
{
	BOOL fRet;
	HostedDeviceTree *pDevTree;

	pDevTree = FindDevTreeSafe(pszDeviceName);

	fRet = FALSE;
	if (pDevTree)
	{
		__try {
			fRet = pDevTree->SubmitPropertyEvent(
					pszUDN,
                    pszServiceId,
					nArgs,
					rgArgs);
			
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			SetLastError(ERROR_INVALID_PARAMETER);
		}
		pDevTree->DecRef();
	}
	return fRet;	
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
			break;	// no more devices hosted in the specified process

		pDevTree->Shutdown(HDT_NO_CALLBACKS);
		delete pDevTree;

	}
}
