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

#include "ncbase.h"
#include "upnp.h"

#include "auto_xxx.hxx"
#include "variant.h"
#include "url_verifier.h"
#include "upnp_config.h"

#include "main.h"
#include "finder.h"
#include "ssdpapi.h"
#include "device.h"
#include "DeviceDescription.h"

#define FINDERCBSIG		'UPNP'

extern url_verifier* g_pURL_Verifier;

void ServiceCallback(SSDP_CALLBACK_TYPE cbType, CONST SSDP_MESSAGE *pSsdpService, void *pContext);


// StateVariableChanged
void FinderCallback::StateVariableChanged(LPCWSTR pwszName, LPCWSTR pwszValue)
{
    Assert(FALSE);
}


// ServiceInstanceDied
void FinderCallback::ServiceInstanceDied(LPCWSTR pszUSN)
{
    SSDP_MESSAGE    SsdpService = {0};
    ce::string      strUSN;

    if(!ce::WideCharToMultiByte(CP_ACP, pszUSN, -1, &strUSN))
        return;
    
    // check if device has already been reported
    for(ce::list<FinderCallback::Device>::iterator it = listReported.begin(), itEnd = listReported.end(); it != itEnd; ++it)
        if(it->strUSN == strUSN)
        {
            listReported.erase(it);
            break;
        }
    
    SsdpService.szUSN = const_cast<LPSTR>(static_cast<LPCSTR>(strUSN));

    ServiceCallback(SSDP_BYEBYE, &SsdpService, this);
}


// AliveNotification
void FinderCallback::AliveNotification(LPCWSTR pszUSN, LPCWSTR pszLocation, LPCWSTR pszNls, DWORD dwLifeTime)
{
    SSDP_MESSAGE    SsdpService = {0};
    ce::string      strUSN;
    ce::string      strLocation;
    ce::string      strNls;

    if(!ce::WideCharToMultiByte(CP_ACP, pszUSN, -1, &strUSN))
        return;
    
    if(!ce::WideCharToMultiByte(CP_ACP, pszLocation, -1, &strLocation))
        return;
        
    ce::WideCharToMultiByte(CP_ACP, pszNls, -1, &strNls);

    SsdpService.szUSN = const_cast<LPSTR>(static_cast<LPCSTR>(strUSN));
    SsdpService.szLocHeader = const_cast<LPSTR>(static_cast<LPCSTR>(strLocation));
    SsdpService.szNls = const_cast<LPSTR>(static_cast<LPCSTR>(strNls));
    SsdpService.iLifeTime = dwLifeTime;

    ServiceCallback(SSDP_ALIVE, &SsdpService, this);
}


// DumpSSDPMessage
void DumpSSDPMessage(PSSDP_MESSAGE pMsg)
{
	WCHAR szBuf[512];

	DEBUGMSG(ZONE_TRACE, (L"SSDP Message\n"));
	
	MultiByteToWideChar(CP_ACP, 0, pMsg->szType, -1, szBuf, 512);
	DEBUGMSG(ZONE_TRACE, (L"szType = %s\n", szBuf));

	MultiByteToWideChar(CP_ACP, 0, pMsg->szLocHeader, -1, szBuf, 512);
	DEBUGMSG(ZONE_TRACE, (L"szLocHeader = %s\n", szBuf));

	MultiByteToWideChar(CP_ACP, 0, pMsg->szAltHeaders, -1, szBuf, 512);
	DEBUGMSG(ZONE_TRACE, (L"szAltHeaders = %s\n", szBuf));
	
	MultiByteToWideChar(CP_ACP, 0, pMsg->szNls, -1, szBuf, 512);
	DEBUGMSG(ZONE_TRACE, (L"szNls = %s\n", szBuf));

	MultiByteToWideChar(CP_ACP, 0, pMsg->szUSN, -1, szBuf, 512);
	DEBUGMSG(ZONE_TRACE, (L"szUSN = %s\n", szBuf));

	MultiByteToWideChar(CP_ACP, 0, pMsg->szSid, -1, szBuf, 512);
	DEBUGMSG(ZONE_TRACE, (L"szSid = %s\n", szBuf));
	
	DEBUGMSG(ZONE_TRACE, (L"iSeq = %d\n", pMsg->iSeq));
	DEBUGMSG(ZONE_TRACE, (L"iLifeTime = %d\n\n", pMsg->iLifeTime));
}


// GetUDN
static void GetUDN(LPCSTR pszUSN, ce::string* pstr)
{
	LPCSTR psz = strstr(pszUSN, "::");

	if(psz)
		pstr->assign(pszUSN, psz - pszUSN);
	else
		pstr->assign(pszUSN);
}


// GetUPnPDevice
HRESULT GetUPnPDevice(LPCSTR pszUSN, LPCSTR pszUrl, UINT nLifeTime, IUPnPDevice **ppUPnPDevice)
{
	ASSERT(pszUSN && pszUrl && ppUPnPDevice && *pszUSN && *pszUrl);

	ce::wstring	wstrURL;
	ce::wstring	wstrUDN;
	ce::string	strUDN;
	
	// ANSI code page for UDN
	ce::MultiByteToWideChar(CP_ACP, pszUrl, -1, &wstrURL);
	
	if(!g_pURL_Verifier->is_url_ok(pszUrl))
	{
	    TraceTag(ttidError, "GetUPnPDevice: invalid device description URL");
	    return E_FAIL;
	}
	
	// find UDN from USN
	GetUDN(pszUSN, &strUDN);
	
	// ANSI code page for UDN
	ce::MultiByteToWideChar(CP_ACP, strUDN, -1, &wstrUDN);
	
	if(DeviceDescription* pDescription = new DeviceDescription(nLifeTime))
    {
	    // hold reference to device description object for the time of parsing
	    pDescription->AddRef();

	    // parse device description
	    pDescription->Load(const_cast<BSTR>(static_cast<LPCWSTR>(wstrURL)));
    	
	    // find device with given UDN
	    pDescription->DeviceByUDN(const_cast<BSTR>(static_cast<LPCWSTR>(wstrUDN)), ppUPnPDevice);

	    // release the reference
	    // if device with given UDN was found it will hold reference to device description
	    // otherwise device description will be deleted at this moment
	    pDescription->Release();
	}
	
	return *ppUPnPDevice ? S_OK : E_FAIL;
}


// ServiceCallback
void ServiceCallback(SSDP_CALLBACK_TYPE cbType, CONST SSDP_MESSAGE *pSsdpService, void *pContext)
{
	IUPnPDevice*	pUPnPDevice = NULL;
	ce::string 		strUDN;
	ce::auto_bstr	bstrUDN;

	ASSERT((cbType == SSDP_DONE) || (pSsdpService != NULL));

	FinderCallback *pfcb = reinterpret_cast<FinderCallback*>(pContext);

	if ((pfcb == NULL) || (pfcb->dwSig != FINDERCBSIG))
		return;

    pfcb->AddRef();
   
    HRESULT hr = S_OK;

	if (cbType == SSDP_FOUND || cbType == SSDP_ALIVE)
	{
		// check if device has already been reported
        for(ce::list<FinderCallback::Device>::iterator it = pfcb->listReported.begin(), itEnd = pfcb->listReported.end(); it != itEnd; ++it)
            if(it->strUSN == pSsdpService->szUSN)
            {
                if(it->strLocation == pSsdpService->szLocHeader || 
                   pSsdpService->szNls != NULL && it->strNls == pSsdpService->szNls)
                {
                    if(pSsdpService->szNls != NULL)
                        it->strNls = pSsdpService->szNls;
                    
                    pfcb->Release();
                    return;
                }

                pfcb->listReported.erase(it);
                ServiceCallback(SSDP_BYEBYE, pSsdpService, pContext);
                break;
            }
        
        if(cbType == SSDP_ALIVE)
        {
            // delay processing of unsolicited announcement by random number of seconds
            int nDelay = (10 * rand()) % upnp_config::max_control_point_delay();
            
            DEBUGMSG(ZONE_CALLBACK, (L"UPNP:ServiceCallback delaying control point response by %d ms\n", nDelay));
            
            Sleep(nDelay);
        }
        
        // get found device
		if(FAILED(hr = GetUPnPDevice(pSsdpService->szUSN, pSsdpService->szLocHeader, pSsdpService->iLifeTime, &pUPnPDevice)))
        {
			pfcb->Release();
            return;
        }
    
        pfcb->listReported.push_front(FinderCallback::Device(pSsdpService->szUSN, pSsdpService->szLocHeader, pSsdpService->szNls));

		ASSERT(pUPnPDevice);
		DEBUGMSG(ZONE_CALLBACK, (L"UPNP:ServiceCallback-Found device :%a of type %a at %a.\n", pSsdpService->szUSN, pSsdpService->szType, pSsdpService->szLocHeader));

		// get device UDN
		pUPnPDevice->get_UniqueDeviceName(&bstrUDN);
	}
	else if (cbType == SSDP_BYEBYE)
	{
		ce::wstring	wstrUDN;
		ce::string	strUDN;

		GetUDN(pSsdpService->szUSN, &strUDN);
		
		// ANSI code page for UDN
		ce::MultiByteToWideChar(CP_ACP, strUDN, -1, &wstrUDN);
		
		DEBUGMSG(ZONE_CALLBACK, (L"UPNP:ServiceCallback-Device (%s) going away.\n", static_cast<LPCWSTR>(wstrUDN)));

		bstrUDN = SysAllocString(wstrUDN);
	}

	if (pfcb->fDispatch)
	{
		ce::variant	varResult;
		ce::variant	avarArgs[3];

		avarArgs[0].vt = VT_I4;
		
		switch(cbType)
		{
			case SSDP_ALIVE:
			case SSDP_FOUND:
				
				// 1st argument is pointer to device object
				avarArgs[2].vt = VT_DISPATCH;
				pUPnPDevice->QueryInterface(IID_IDispatch, (LPVOID *)(&avarArgs[2].pdispVal));
				
				// 2nd argument is device UDN
				avarArgs[1] = bstrUDN;
				
				// 3rd argument is type of callback
				V_I4(&avarArgs[0]) = 0;
				break;

			case SSDP_BYEBYE:
				// 1st argument not used for "device removed" callback
				
				// 2nd argument is device UDN
				avarArgs[1] = bstrUDN;

				// 3rd argument is type of callback - string "1" indicates "device removed"
				V_I4(&avarArgs[0]) = 1;
				break;

			case SSDP_DONE:
				// 1st argument not used for "search complete" callback
				
				// 2nd argument not used for "search complete" callback

				// 3rd argument is type of callback - string "2" indicates "search complete"
				V_I4(&avarArgs[0]) = 2;

				DEBUGMSG(ZONE_CALLBACK, (L"UPNP:ServiceCallback-Search complete.\n"));
				break;

			default:
				ASSERT(FALSE);
				break;
		}

	    DISPPARAMS dispParams = {0};
		
		dispParams.rgvarg = &avarArgs[0];
		dispParams.cArgs = 3;

		hr = pfcb->cb.pDispatch->Invoke(DISPID_VALUE, IID_NULL, 0, DISPATCH_METHOD, &dispParams, &varResult, NULL, NULL);
		
		if (hr != S_OK)
		{
			DEBUGMSG(ZONE_ERROR, (L"UPNP:Invoke on DeviceFinderCallback returns 0x%08x\n", hr));
		}
	}
	else
	{
		switch(cbType)
		{
			case SSDP_ALIVE:
			case SSDP_FOUND:
				hr = pfcb->cb.pCallback->DeviceAdded((LONG)pfcb, pUPnPDevice);
				break;

			case SSDP_BYEBYE:
				hr = pfcb->cb.pCallback->DeviceRemoved((LONG)pfcb, bstrUDN);
				break;

			case SSDP_DONE:
				hr = pfcb->cb.pCallback->SearchComplete((LONG)pfcb);

				DEBUGMSG(ZONE_CALLBACK, (L"UPNP:ServiceCallback-Search complete.\n"));
				break;

			default:
				ASSERT(FALSE);
				break;
		}
	}

	if (pUPnPDevice)
		pUPnPDevice->Release();

    pfcb->Release();
}


// Find
HRESULT CUPnPDeviceFinder::Find(BSTR bstrSearchTarget, IUPnPDevices **ppDevices, IUPnPDevice **ppDevice)
{
	ASSERT((ppDevices != NULL) ^ (ppDevice != NULL));	//exactly one of them has to be NULL

	HRESULT																	hr = E_FAIL;
	ce::auto_xxx<HANDLE, BOOL (__stdcall *)(HANDLE), FindServicesClose, -1>	hFind;
	ce::string																strSearchTarget;

	// ANSI code page for URI
	ce::WideCharToMultiByte(CP_ACP, bstrSearchTarget, -1, &strSearchTarget);
	
	// find services
	hFind = FindServices(strSearchTarget, NULL, TRUE);

	if(!hFind.valid())
	{
		DEBUGMSG(ZONE_ERROR, (L"FindServices returns INVALID_HANDLE_VALUE\n"));
		
		return HrFromLastWin32Error();
	}

	Devices* pDevices = NULL;
	
	if(ppDevices)
	{
		// create output collection
		pDevices = new Devices();

		if(!pDevices)
			return E_OUTOFMEMORY;
	}
	
	PSSDP_MESSAGE pService = NULL;

	// iterate through found services 
	for(BOOL bContinue = GetFirstService(hFind, &pService); bContinue && pService; bContinue = GetNextService(hFind, &pService))
	{
		IFDBG(DumpSSDPMessage(pService));
		
		IUPnPDevice *pUPnPDevice = NULL;

		// get device for the service
		if(SUCCEEDED(hr = GetUPnPDevice(pService->szUSN, pService->szLocHeader, pService->iLifeTime, &pUPnPDevice)))
		{
			ASSERT(pUPnPDevice);

			if(ppDevice)
			{
				// searching by UDN - there should be just one result
				// return as soon as we find one
				*ppDevice = pUPnPDevice;
				break;
			}
			else
			{
				ASSERT(pDevices);
				// searching by type - there might be multiply results
				// add found device to collection
				ce::auto_bstr bstrUDN;

				pUPnPDevice->get_UniqueDeviceName(&bstrUDN);
				pDevices->AddItem(bstrUDN, pUPnPDevice);
			}

			pUPnPDevice->Release();
		}
	}

	if(pDevices)
		if(SUCCEEDED(hr))
			hr = pDevices->QueryInterface(IID_IUPnPDevices, (void**)ppDevices);
		else
			delete pDevices;

	return hr;
}
	

// FindByType
STDMETHODIMP
CUPnPDeviceFinder::FindByType(	/* [in] */			BSTR bstrTypeURI,
								/* [in] */			DWORD dwFlags,
								/* [out, retval] */	IUPnPDevices ** ppDevices)
{
	DEBUGMSG(ZONE_FINDER, (L"IUPnPDeviceFinder::FindByType(bstrTypeURI=%s, dwFlags=0x%08x).\n", bstrTypeURI, dwFlags));
	
	return Find(bstrTypeURI, ppDevices, NULL);
}


// FindByUDN
STDMETHODIMP
CUPnPDeviceFinder::FindByUDN(	/*[in]*/ BSTR bstrUDN,
								/*[out, retval]*/ IUPnPDevice ** ppDevice)
{
	DEBUGMSG(ZONE_FINDER, (L"IUPnPDeviceFinder::FindByUDN(bstrTypeUDN=%s).\n", bstrUDN));
	
	return Find(bstrUDN, NULL, ppDevice);
}


// CreateAsyncFind
STDMETHODIMP
CUPnPDeviceFinder::CreateAsyncFind(	/* [in] */			BSTR bstrTypeURI,
									/* [in] */			DWORD dwFlags,
									/* [in] */			IUnknown __RPC_FAR *punkDeviceFinderCallback,
									/* [retval][out] */ LONG __RPC_FAR *plFindData)
{
	CHECK_POINTER(plFindData);
	CHECK_POINTER(punkDeviceFinderCallback);
	CHECK_POINTER(bstrTypeURI);

	FinderCallback *pfcb = new FinderCallback;
	if (!pfcb)
		return E_OUTOFMEMORY;

	pfcb->dwSig = FINDERCBSIG;
	pfcb->bstrSearchTarget = SysAllocString(bstrTypeURI);
	pfcb->dwFlags = dwFlags;
	pfcb->fDispatch = FALSE;
	
	if (FAILED(punkDeviceFinderCallback->QueryInterface(IID_IUPnPDeviceFinderCallback, (LPVOID *)&(pfcb->cb.pCallback))))
	{
		pfcb->fDispatch = TRUE;
		if (FAILED(punkDeviceFinderCallback->QueryInterface(IID_IDispatch, (LPVOID *)&(pfcb->cb.pDispatch))))
		{
			DEBUGMSG(ZONE_ERROR|ZONE_FINDER, (L"IUPnPDeviceFinder::CreateAsyncFind:punkDeviceFinderCallback supports neither IDispatch nor IUPnPDeviceFinderCallback.\n"));
			pfcb->Release();
			return E_FAIL;
		}
	}

	pfcb->hFind = INVALID_HANDLE_VALUE;
	
	Lock();

    pfcb->pNext = _pfcbFirst;
	_pfcbFirst = pfcb;

	Unlock();

	*plFindData = (LONG)pfcb;

	return S_OK;
}


// StartAsyncFind
STDMETHODIMP
CUPnPDeviceFinder::StartAsyncFind(/* [in] */ LONG lFindData)
{
	Lock();

	FinderCallback *pfcb = _pfcbFirst;
	FinderCallback *pfcbPrev = NULL;

	// look for the async find object
	while (pfcb && (lFindData != (LONG )pfcb))
	{
		pfcbPrev = pfcb;
		pfcb = pfcb->pNext;
	}

	if (!pfcb)
	{
		DEBUGMSG(ZONE_ERROR|ZONE_FINDER, (L"IUPnPDeviceFinder::StartAsyncFind:Invalid cookie-%d.\n", lFindData));
		Unlock();
		return E_INVALIDARG;
	}

	ce::string	strSearchTarget;

	// ANSI code page for search target
	ce::WideCharToMultiByte(CP_ACP, pfcb->bstrSearchTarget, -1, &strSearchTarget);

	pfcb->start_listening();
    
    pfcb->hFind = FindServicesCallback(strSearchTarget, NULL, TRUE, ServiceCallback, pfcb);

	DEBUGMSG(ZONE_FINDER, (L"IUPnPDeviceFinder::StartAsyncFind:FindServicesCallback returns handle=0x%08x.\n", pfcb->hFind));

	//  The find is cancelled if StartAsyncFind fails
	HRESULT hr = S_OK;
	
	if (pfcb->hFind == INVALID_HANDLE_VALUE)
	{
		hr = HrFromLastWin32Error();

		if (!pfcbPrev)
			_pfcbFirst = pfcb->pNext;
		else
			pfcbPrev->pNext = pfcb->pNext;
		
		pfcb->Release();
	}

	Unlock();
	
	return hr;
}


// CancelAsyncFind
STDMETHODIMP
CUPnPDeviceFinder::CancelAsyncFind(/* [in] */ LONG lFindData)
{
	Lock();

	FinderCallback *pfcb = _pfcbFirst;
	FinderCallback *pfcbPrev = NULL;

	// look for the async find object
	while (pfcb && (lFindData != (LONG )pfcb))
	{
		pfcbPrev = pfcb;
		pfcb = pfcb->pNext;
	}

	if (!pfcb)
	{
		DEBUGMSG(ZONE_ERROR|ZONE_FINDER, (L"IUPnPDeviceFinder::CancelAsyncFind:Invalid cookie-%d.\n", lFindData));
		Unlock();
		return E_INVALIDARG;
	}
	
	// release find object resources
	if (pfcb->hFind != INVALID_HANDLE_VALUE)
		FindServicesClose(pfcb->hFind);

    pfcb->stop_listening();

	// remove find object from the list
	if (!pfcbPrev)
		_pfcbFirst = pfcb->pNext;
	else
		pfcbPrev->pNext = pfcb->pNext;
	
	// release find object
	pfcb->Release();

	Unlock();

	return S_OK;
}


