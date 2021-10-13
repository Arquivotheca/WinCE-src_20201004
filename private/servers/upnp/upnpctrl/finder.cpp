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

#include "ncbase.h"
#include "upnp.h"

#include "auto_xxx.hxx"
#include "variant.h"
#include "url_verifier.h"
#include "upnp_config.h"

#include "main.h"
#include "finder.h"
#include "device.h"

#define FINDERCBSIG     'UPNP'

extern url_verifier* g_pURL_Verifier;

void ServiceCallback(SSDP_CALLBACK_TYPE cbType, CONST SSDP_MESSAGE *pSsdpService, void *pContext);
static void GetUDN(LPCSTR pszUSN, ce::string* pstr);

// StateVariableChanged
void FinderCallback::StateVariableChanged(LPCWSTR pwszName, LPCWSTR pwszValue)
{
    Assert(FALSE);
}


// ServiceInstanceDied
void FinderCallback::ServiceInstanceDied(LPCWSTR pszUSN)
{
    SSDP_MESSAGE    SsdpService;
    ce::string      strUSN;

    memset(&SsdpService, 0, sizeof(SsdpService));

    if(!ce::WideCharToMultiByte(CP_ACP, pszUSN, -1, &strUSN))
    {
        return;
    }

    RemoveReportedDevice(strUSN);

    SsdpService.szUSN = const_cast<LPSTR>(static_cast<LPCSTR>(strUSN));

    ServiceCallback(SSDP_BYEBYE, &SsdpService, this);
}


// AliveNotification
void FinderCallback::AliveNotification(LPCWSTR pszUSN, LPCWSTR pszLocation, LPCWSTR pszNls, DWORD dwLifeTime)
{
    SSDP_MESSAGE    SsdpService;
    ce::string      strUSN;
    ce::string      strLocation;
    ce::string      strNls;

    memset(&SsdpService, 0, sizeof(SsdpService));

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

void FinderCallback::GetUPnPDeviceAsync(LPCSTR pszUSN, LPCSTR pszUrl, LPCSTR pszNls, UINT nLifeTime)
{
    ASSERT(pszUSN && pszUrl && *pszUSN && *pszUrl);

    //
    // Validate URL
    //
    if(!g_pURL_Verifier->is_url_ok(pszUrl))
    {
        TraceTag(ttidError, "GetUPnPDeviceAsync: invalid device description URL");
        return;
    }

    //
    // ANSI code page for UDN
    //
    ce::wstring wstrURL;
    ce::MultiByteToWideChar(CP_ACP, pszUrl, -1, &wstrURL);

    if(AsyncDeviceLoader* pAsyncDeviceLoader = new AsyncDeviceLoader(this, pszUSN, pszUrl, pszNls, nLifeTime))
    {
        HRESULT hr;

        // hold reference to device loader object for the time of loading
        pAsyncDeviceLoader->AddRef();

        // parse device description
        if(FAILED(hr = pAsyncDeviceLoader->LoadAsync(wstrURL)))
        {
            DEBUGMSG(ZONE_ERROR, (L"UPNP: LoadAsync failed 0x%x\n", hr));

            pAsyncDeviceLoader->Release();

            RemoveReportedDevice(pszUSN, pszUrl);
        }
    }
    else
    {
        DEBUGMSG(ZONE_ERROR, (L"UPNP: OOM creating AsyncDeviceLoader\n"));

        RemoveReportedDevice(pszUSN, pszUrl);
    }
}


HRESULT STDMETHODCALLTYPE FinderCallback::AsyncDeviceLoader::LoadComplete(/* [in] */HRESULT hrLoadResult)
{
    if(SUCCEEDED(hrLoadResult))
    {
        IUPnPDevice     *pUPnPDevice = NULL;
        ce::wstring     wstrUDN;
        ce::string      strUDN;
        ce::auto_bstr   bstrUDN;
        HRESULT         hr;

        // find UDN from USN
        GetUDN(m_strUSN, &strUDN);

        // ANSI code page for UDN
        ce::MultiByteToWideChar(CP_ACP, strUDN, -1, &wstrUDN);

        bstrUDN = SysAllocString(wstrUDN);

        ASSERT(m_pDescription);

        // find device with given UDN
        if(SUCCEEDED(hr = m_pDescription->DeviceByUDN(bstrUDN, &pUPnPDevice)))
        {
            ASSERT(pUPnPDevice);

            m_pfcb->Lock();

            m_pfcb->PerformCallback(SSDP_ALIVE, bstrUDN, pUPnPDevice);

            m_pfcb->Unlock();

            pUPnPDevice->Release();
            pUPnPDevice = NULL;
        }
        else
        {
            DEBUGMSG(ZONE_ERROR, (L"UPNP: Error 0x%x getting device by UDN %a\n", hr, static_cast<LPCSTR>(strUDN)));

            m_pfcb->RemoveReportedDevice(m_strUSN, m_strLocHeader);
        }

        ASSERT(!pUPnPDevice);
    }
    else
    {
        DEBUGMSG(ZONE_ERROR, (L"UPNP: Error 0x%x loading device description from %a\n", hrLoadResult, static_cast<LPCSTR>(m_strLocHeader)));

        m_pfcb->RemoveReportedDevice(m_strUSN, m_strLocHeader);
    }

    // release the reference
    Release();

    return S_OK;
}


// DumpSSDPMessage
void DumpSSDPMessageItem(PSSDP_MESSAGE_ITEM pMsgItem)
{
    WCHAR szBuf[512];

    PSSDP_MESSAGE pMsg = NULL;
    if (pMsgItem)
    {
        pMsg = pMsgItem->pSsdpMessage;
    }
    if (!pMsg)
    {
        return;
    }

    DEBUGMSG(ZONE_TRACE, (L"SSDP Message\n"));

    MultiByteToWideChar(CP_ACP, 0, pMsg->szType, -1, szBuf, 512);
    DEBUGMSG(ZONE_TRACE, (L"szType = %s\n", szBuf));

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

    for (int i = 0; i < _countof(pMsgItem->rgszLocations); i++)
    {
        if (pMsgItem->rgszLocations[i])
        {
            MultiByteToWideChar(CP_ACP, 0, pMsgItem->rgszLocations[i], -1, szBuf, 512);
            DEBUGMSG(ZONE_TRACE, (L"szLocation[%d] = %s\n", i, szBuf));
        }
    }
}


// GetUDN
static void GetUDN(LPCSTR pszUSN, ce::string* pstr)
{
    LPCSTR psz = strstr(pszUSN, "::");

    if(psz)
    {
        pstr->assign(pszUSN, psz - pszUSN);
    }
    else
    {
        pstr->assign(pszUSN);
    }
}


// GetUPnPDevice
HRESULT GetUPnPDevice(LPCSTR pszUSN, LPCSTR pszUrl, UINT nLifeTime, IUPnPDevice **ppUPnPDevice)
{
    ASSERT(pszUSN && pszUrl && ppUPnPDevice && *pszUSN && *pszUrl);

    ce::wstring wstrURL;
    ce::wstring wstrUDN;
    ce::string  strUDN;

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
    ce::string      strUDN;
    ce::auto_bstr   bstrUDN;

    ASSERT((cbType == SSDP_DONE) || (pSsdpService != NULL));

    FinderCallback *pfcb = reinterpret_cast<FinderCallback*>(pContext);

    if ((pfcb == NULL) || (pfcb->dwSig != FINDERCBSIG))
    {
        return;
    }

    pfcb->AddRef();
    pfcb->Lock();

    HRESULT hr = S_OK;
    boolean bIsAlt = false;
    boolean bIsDup = false;

    if (cbType == SSDP_FOUND || cbType == SSDP_ALIVE)
    {
        // check if device has already been reported
        for(ce::list<FinderCallback::Device>::iterator it = pfcb->listReported.begin(), itEnd = pfcb->listReported.end(); it != itEnd; ++it)
        {
            if(it->strUSN == pSsdpService->szUSN)
            {
                for (int i = 0; i < _countof(it->rgstrLocations); i++)
                {
                    if (it->rgstrLocations[i] == "")
                    {
                        bIsAlt = true;
                        it->rgstrLocations[i] = pSsdpService->szLocHeader;
                        it->rgstrNls[i] = pSsdpService->szNls;
                        break;
                    }
                    else if (it->rgstrLocations[i] == pSsdpService->szLocHeader)
                    {
                        // This is a duplicate
                        bIsDup = true;
                        it->rgstrNls[i] = pSsdpService->szNls;
                        break;
                    }
                }

            /*
                pfcb->listReported.erase(it);
                pfcb->Unlock();
                ServiceCallback(SSDP_BYEBYE, pSsdpService, pContext);
                pfcb->Lock();
            */
                break;
            }
        }

        if (!bIsDup && !bIsAlt)
        {
            pfcb->listReported.push_front(FinderCallback::Device(pSsdpService->szUSN, pSsdpService->szLocHeader, pSsdpService->szNls));
        }

        if (!bIsDup)
        {
            if(cbType == SSDP_ALIVE)
            {
                // delay processing of unsolicited announcement by random number of seconds
                LARGE_INTEGER now;
                QueryPerformanceCounter(&now);
                int nDelay = (10 * (DWORD)now.QuadPart) % upnp_config::max_control_point_delay();
                DEBUGMSG(ZONE_CALLBACK, (L"UPNP:ServiceCallback delaying control point response by %d ms\n", nDelay));
                Sleep(nDelay);
            }

            DEBUGMSG(ZONE_CALLBACK, (L"UPNP:ServiceCallback-Found device :%a of type %a at %a.\n", pSsdpService->szUSN, pSsdpService->szType, pSsdpService->szLocHeader));
            DEBUGMSG(ZONE_CALLBACK, (L"UPNP:ServiceCallback-Attempting to load device description\n"));

            pfcb->GetUPnPDeviceAsync(pSsdpService->szUSN, pSsdpService->szLocHeader, pSsdpService->szNls, pSsdpService->iLifeTime);
        }
    }
    else if (cbType == SSDP_BYEBYE)
    {
        ce::wstring wstrUDN;
        ce::string  strUDN;

        GetUDN(pSsdpService->szUSN, &strUDN);

        // ANSI code page for UDN
        ce::MultiByteToWideChar(CP_ACP, strUDN, -1, &wstrUDN);

        DEBUGMSG(ZONE_CALLBACK, (L"UPNP:ServiceCallback-Device (%s) going away.\n", static_cast<LPCWSTR>(wstrUDN)));

        bstrUDN = SysAllocString(wstrUDN);

        pfcb->Unlock();
        // we must not hold the lock when calling into the users callback
        pfcb->PerformCallback(cbType, bstrUDN, NULL);
        pfcb->Lock();
    }
    else if (cbType == SSDP_DONE)
    {
        pfcb->Unlock();
        // we must not hold the lock when calling into the users callback
        pfcb->PerformCallback(cbType, NULL, NULL);
        pfcb->Lock();
    }
    else
        ASSERT(FALSE);

    pfcb->Unlock();
    pfcb->Release();
}


void FinderCallback::PerformCallback(SSDP_CALLBACK_TYPE cbType, BSTR bstrUDN, IUPnPDevice* pUPnPDevice)
{
    HRESULT hr;

    if(fDispatch)
    {
        ce::variant varResult;
        ce::variant avarArgs[3];

        avarArgs[0].vt = VT_I4;

        switch(cbType)
        {
            case SSDP_ALIVE:
            case SSDP_FOUND:

                ASSERT(pUPnPDevice);

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

        hr = cb.pDispatch->Invoke(DISPID_VALUE, IID_NULL, 0, DISPATCH_METHOD, &dispParams, &varResult, NULL, NULL);

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
                ASSERT(pUPnPDevice);
                hr = cb.pCallback->DeviceAdded((LONG)this, pUPnPDevice);
                break;

            case SSDP_BYEBYE:
                hr = cb.pCallback->DeviceRemoved((LONG)this, bstrUDN);
                break;

            case SSDP_DONE:
                hr = cb.pCallback->SearchComplete((LONG)this);

                DEBUGMSG(ZONE_CALLBACK, (L"UPNP:ServiceCallback-Search complete.\n"));
                break;

            default:
                ASSERT(FALSE);
                break;
        }
    }
}


// Find
HRESULT CUPnPDeviceFinder::Find(BSTR bstrSearchTarget, IUPnPDevices **ppDevices, IUPnPDevice **ppDevice)
{
    ASSERT((ppDevices != NULL) ^ (ppDevice != NULL));   //exactly one of them has to be NULL

    HRESULT                                                                                     hr = E_FAIL;
    ce::auto_xxx<HANDLE, BOOL (__stdcall *)(HANDLE), FindServicesClose, INVALID_HANDLE_VALUE>   hFind;
    ce::string                                                                                  strSearchTarget;
    bool bFoundOne = false;

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

    if(ppDevices)    // searching by service - multiple results possible
    {
        // create output collection
        pDevices = new Devices();

        if(!pDevices)
        {
            return E_OUTOFMEMORY;
        }
    }

    PSSDP_MESSAGE_ITEM pService = NULL;

    // iterate through found services 
    for(BOOL bContinue = GetFirstService(hFind, &pService); bContinue && pService; bContinue = GetNextService(hFind, &pService))
    {
        IFDBG(DumpSSDPMessageItem(pService));

        IUPnPDevice *pUPnPDevice = NULL;
        PSSDP_MESSAGE pMsg = pService->pSsdpMessage;

        // get device for the service - try all available addresses
        for (int i = 0; i < _countof(pService->rgszLocations); i++)
        {
            if (pMsg && pService->rgszLocations[i])
            {
                hr = GetUPnPDevice(pMsg->szUSN, pService->rgszLocations[i], pMsg->iLifeTime, &pUPnPDevice);
                if(SUCCEEDED(hr))
                {
                    break;
                }
            }
        }

        if(SUCCEEDED(hr))
        {
            if (!pUPnPDevice)
            {
                ASSERT(0);
                continue;
            }

            // Note that we have found at least one device
            bFoundOne = true;

            if(ppDevices)     // searching by services - multiple results possible
            {
                // searching by type - there might be multiply results
                // add found device to collection
                ce::auto_bstr bstrUDN;

                pUPnPDevice->get_UniqueDeviceName(&bstrUDN);
                pDevices->AddItem(bstrUDN, pUPnPDevice);
            }
            if (ppDevice)   // Searching by UDN - only one device expected
            {
                if (! *ppDevice)
                {
                    // Save a reference to the first found device
                    *ppDevice = pUPnPDevice;
                    pUPnPDevice->AddRef();
                }
            }

            pUPnPDevice->Release();
        }
    }

    if(ppDevices)     // Searching by services - multiple results possible
    {
        if(bFoundOne)
        {
            hr = pDevices->QueryInterface(IID_IUPnPDevices, (void**)ppDevices);
            if (FAILED(hr))
                {
                    delete pDevices;
                }
        }
        else
        {
            delete pDevices;
        }
    }

    return hr;
}


// FindByType
STDMETHODIMP
CUPnPDeviceFinder::FindByType(  /* [in] */          BSTR bstrTypeURI,
                                /* [in] */          DWORD dwFlags,
                                /* [out, retval] */ IUPnPDevices ** ppDevices)
{
    DEBUGMSG(ZONE_FINDER, (L"IUPnPDeviceFinder::FindByType(bstrTypeURI=%s, dwFlags=0x%08x).\n", bstrTypeURI, dwFlags));

    return Find(bstrTypeURI, ppDevices, NULL);
}


// FindByUDN
STDMETHODIMP
CUPnPDeviceFinder::FindByUDN(   /*[in]*/ BSTR bstrUDN,
                                /*[out, retval]*/ IUPnPDevice ** ppDevice)
{
    DEBUGMSG(ZONE_FINDER, (L"IUPnPDeviceFinder::FindByUDN(bstrTypeUDN=%s).\n", bstrUDN));

    return Find(bstrUDN, NULL, ppDevice);
}


// CreateAsyncFind
STDMETHODIMP
CUPnPDeviceFinder::CreateAsyncFind( /* [in] */          BSTR bstrTypeURI,
                                    /* [in] */          DWORD dwFlags,
                                    /* [in] */          IUnknown __RPC_FAR *punkDeviceFinderCallback,
                                    /* [retval][out] */ LONG __RPC_FAR *plFindData)
{
    CHECK_POINTER(plFindData);
    CHECK_POINTER(punkDeviceFinderCallback);
    CHECK_POINTER(bstrTypeURI);

    FinderCallback *pfcb = new FinderCallback;
    if (!pfcb)
    {
        return E_OUTOFMEMORY;
    }

    pfcb->dwSig = FINDERCBSIG;
    pfcb->bstrSearchTarget = SysAllocString(bstrTypeURI);
    pfcb->dwFlags = dwFlags;
    pfcb->fDispatch = FALSE;

    if(!pfcb->Valid())
    {
        pfcb->Release();
        return E_FAIL;
    }

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

    ce::string  strSearchTarget;

    // ANSI code page for search target
    ce::WideCharToMultiByte(CP_ACP, pfcb->bstrSearchTarget, -1, &strSearchTarget);

    pfcb->start_listening();

    pfcb->hFind = FindServicesCallback(strSearchTarget, NULL, TRUE, ServiceCallback, pfcb);

    DEBUGMSG(ZONE_FINDER, (L"IUPnPDeviceFinder::StartAsyncFind:FindServicesCallback returns handle=0x%08x.\n", pfcb->hFind));

    //  The find is cancelled if StartAsyncFind fails
    HRESULT hr = S_OK;

    if (pfcb->hFind == INVALID_HANDLE_VALUE)
    {
        pfcb->stop_listening();

        hr = HrFromLastWin32Error();
        if (!pfcbPrev)
        {
            _pfcbFirst = pfcb->pNext;
        }
        else
        {
            pfcbPrev->pNext = pfcb->pNext;
        }

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
    {
        FindServicesClose(pfcb->hFind);
    }

    // remove find object from the list
    if (!pfcbPrev)
    {
        _pfcbFirst = pfcb->pNext;
    }
    else
    {
        pfcbPrev->pNext = pfcb->pNext;
    }

    Unlock();

    // release find object
    pfcb->FinalRelease();

    return S_OK;
}


