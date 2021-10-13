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
#pragma warning (disable : 4509)

BOOL BeginSSDPforDLNA(void);
BOOL EndSSDPforDLNA(void);

BOOL RegisterUpnpServiceImpl(PSSDP_MESSAGE pSsdpMessage, DWORD flags, HANDLE* phService);

#define MAX_STRING 256

//
// string dup utility functions that use new instead of malloc as the allocator
//
PSTR
StrDupWtoA(LPCWSTR pwszSource)
 {
    PSTR psza;
    size_t nBytes;
    if (!pwszSource)
    {
        return NULL;
    }
    nBytes =  wcslen(pwszSource)+1;
    if (psza = new CHAR [nBytes])
    {
        wcstombs(psza,pwszSource,nBytes);
    }
    return psza;
    
}

PWSTR
StrDupAtoW(LPCSTR pszaSource)
{
    size_t nBytes;
    PWSTR pwsz;
    if (!pszaSource)
    {
        return NULL;
    }
    nBytes = strlen(pszaSource)+1;
    if (pwsz = new WCHAR [nBytes])
    {
        mbstowcs(pwsz,pszaSource, nBytes);
    }
    return pwsz;
}

PWSTR
StrDupW(LPCWSTR pwszSource)
{
    PWSTR pwsz;
    size_t nBytes;
    if (!pwszSource)
    {
        return NULL;
    }
    nBytes =  wcslen(pwszSource)+1;
    if (pwsz = new WCHAR [nBytes])
    {
        memcpy(pwsz,pwszSource,nBytes*sizeof(WCHAR));
    }
    return pwsz;
}


PWSTR
CreateUDN(void)
{
    PWSTR pszUDN;
    LONGLONG uuid64 = GenerateUUID64();
    // GUID guid;
    //CeGenerateGUID(&guid);
    pszUDN = new WCHAR [42];
    if (!pszUDN) 
    {
        return NULL;
    }

    // put guid64 into string.  Next byte contains variant 10xxxxxxb. 
    wsprintfW(pszUDN, L"uuid:%04x%04x-%04x-%04x-9000-000000000000", (WORD)uuid64, *((WORD*)&uuid64 + 1), *((WORD*)&uuid64 + 2), *((WORD*)&uuid64 + 3));

    return pszUDN;
}


HANDLE RegisterSSDPService(
  DWORD dwLifeTime,
  PCWSTR pStrDescUrl,
  PCWSTR pStrUDN,
  PCWSTR pStrType) 
{

  HANDLE hReturn = NULL;

  SSDP_MESSAGE    msg;
  
  CHAR            aStrBuffer  [MAX_STRING];
  CHAR            aStrUdn     [MAX_STRING];
  CHAR            aStrType    [MAX_STRING];
  CHAR            aStrDescUrl [MAX_STRING];
  
  memset(&msg, 0, sizeof(msg));

  wcstombs ( aStrDescUrl ,pStrDescUrl, MAX_STRING);
  wcstombs (aStrUdn, pStrUDN, MAX_STRING);
  wcstombs (aStrType, pStrType, MAX_STRING);

  if (pStrUDN == pStrType)
  {
    // uuid:deviceUUID
    strcpy(aStrBuffer, aStrUdn);
  }
  else
  {
    // one of the following :-
    // uuid:deviceUUID::upnp:rootDevice
    // uuid:deviceUUID::urn:schemas-upnp-org:device:deviceType:v
    // uuid:deviceUUID::urn:schemas-upnp-org:service:serviceType:v 
    sprintf(aStrBuffer, "%s::%s", aStrUdn, aStrType);
  }

  msg.iLifeTime     = dwLifeTime;
  msg.szLocHeader   = aStrDescUrl;
  msg.szType        = aStrType;
  msg.szUSN         = aStrBuffer;

  RegisterUpnpServiceImpl(&msg, 0, &hReturn);
  
  return hReturn;
}


#if UNDER_CE
//
// hack to do Initialize in a worker thread,
// because OLE mucks with TLS
// 
// static 
DWORD 
HostedDeviceTree::Initialize2(void *param)
{
    struct HDTInitInfo *pHDTI = (struct HDTInitInfo *)(param);
    
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    pHDTI->fRet = pHDTI->pHDT->Initialize(&pHDTI->devInfo, pHDTI->hOwnerProc, pHDTI->hOwner);
    CoUninitialize();
    
    return 0;
}
#endif

BOOL
HostedDeviceTree::Initialize(UPNPDEVICEINFO *pDevInfo, HANDLE hOwnerProc, HANDLE hOwner)
{
    BOOL fRet = TRUE;
    
    if (pDevInfo->pszUDN && pDevInfo->pszUDN[0])
    {
        // UDN was specified -> use it
        m_pszUDN = StrDupW(pDevInfo->pszUDN);
    }
    else
    {
        // generate a UDN
        m_pszUDN = ::CreateUDN();
    }

    m_pfCallback = pDevInfo->pfCallback;
    m_pvCallbackContext = pDevInfo->pvUserDevContext;
    m_hOwner = hOwner;  
    m_hOwnerProc = hOwnerProc;  
    m_pszName = StrDupW(pDevInfo->pszDeviceName);

    if(pDevInfo->cachecontrol != 0)
    {
        m_dwLifeTime = max(pDevInfo->cachecontrol, MINIMUM_PUBLISH_INTERVAL);
    }

    INT nChars;
    Assert(!m_pszDescFileName);
    // conjure up a suitable filename for the published device description file
    // "\windows\upnp\" "MyDevice" ".xml"
    nChars = celems(c_szLocalWebRootDir)+wcslen(m_pszName)+5;
    m_pszDescFileName = new WCHAR [nChars];
    if (!m_pszDescFileName)
    {
        fRet = FALSE;
    }
    else 
    {
        nChars -= wsprintfW(m_pszDescFileName, L"%s%s.xml",c_szLocalWebRootDir, m_pszName );
        Assert(nChars >=0);
    }

    if (fRet)
    {
        // parse device description
        fRet = Parse(pDevInfo->pszDeviceDescription);
    }

    if (fRet)
    {
        m_state = DevTreeInitialized;
    }

    return fRet;
}



BOOL
HostedDeviceTree::Publish()
{
    BOOL fRet;
    TraceTag(ttidPublish, "%s: [%08x] Publish\n", __FUNCTION__, this);

    if (m_state >= DevTreePublished)
    {
        return FALSE;   // already published
    }

    if (m_state < DevTreeInitialized)
    {
        return FALSE;   // not yet initialized
    }

    // determine the device description URL
    if (!m_pszDescURL)
    {
        int nChars;
        // skip past the web root path e.g) \windows\upnp
        PCWSTR pch = m_pszDescFileName+celems(c_szLocalWebRootDir)-1;
        nChars = wcslen(pch) + celems(c_szHttpPrefix) + celems(c_szReplaceAddrGuid) + celems(c_szURLPrefix) + 1;
        m_pszDescURL = new WCHAR [nChars];

        if (!m_pszDescURL)
        {
            return FALSE;
        }

        nChars -= wsprintfW(m_pszDescURL, L"%s%S%s%s", c_szHttpPrefix, c_szReplaceAddrGuid, c_szURLPrefix, pch);

        Assert(nChars > 0);

    }
    fRet = FALSE;

    Lock();

    // Tell SSDP that we are a DLNA type service
    BeginSSDPforDLNA();

    // publish the rootdevice
    m_hSSDPRoot = RegisterSSDPService(m_dwLifeTime, m_pszDescURL, UDN(), L"upnp:rootdevice");

    if (VALIDSSDPH(m_hSSDPRoot))
    {
        // Register each of the devices and services with SSDP in order to send out the announcements
        fRet = m_pRootDevice->SsdpRegister(m_dwLifeTime, m_pszDescURL);

        if (!fRet)
        {
            DeregisterUpnpService(m_hSSDPRoot, TRUE);
            m_hSSDPRoot = INVALID_HANDLE_VALUE;
        }
    }

    if (fRet)
    {
        m_state = DevTreePublished;
    }

    // complete the SSDP job
    EndSSDPforDLNA();

    Unlock();

    TraceTag(ttidPublish, "%s: [%08x] Published\n", __FUNCTION__, this);
    return fRet;
}

BOOL
HostedDeviceTree::Unpublish()
{
    BOOL fRet = FALSE;
    TraceTag(ttidPublish, "%s: [%08x] Unpublish\n", __FUNCTION__, this);

    Lock();

    if (m_state >= DevTreePublished)
    {
        if (VALIDSSDPH(m_hSSDPRoot))
        {
            fRet = DeregisterUpnpService(m_hSSDPRoot,TRUE);
            m_hSSDPRoot = INVALID_HANDLE_VALUE;
        }
        fRet &= m_pRootDevice->SsdpUnregister();

        m_state = DevTreeInitialized;
    }

    Unlock();

    TraceTag(ttidPublish, "%s: [%08x] Unpublished\n", __FUNCTION__, this);
    return fRet;
}

BOOL
HostedDeviceTree::SubmitPropertyEvent(
    PCWSTR szUDN,
    PCWSTR szServiceId,
    DWORD dwFlags,
    DWORD nArgs,
    UPNPPARAM *rgArgs)
{
    HostedService *pService;
    BOOL fRet = FALSE;

    TraceTag(ttidTrace, "%s: [%08x] Submit Property Event UDN[%S] ServiceID[%S]\n", __FUNCTION__, this, szUDN, szServiceId);
    Lock();

    if (m_state >= DevTreeInitialized)
    {
        pService = FindService(szUDN, szServiceId);
        if (pService)
        {
            fRet = pService->SubmitPropertyEvent(dwFlags, nArgs, rgArgs);
        }
    }

    Unlock();
    TraceTag(ttidTrace, "%s: [%08x] Submited Property Event UDN[%S] ServiceID[%S]\n", __FUNCTION__, this, szUDN, szServiceId);
    return fRet;
}

HostedDeviceTree::~HostedDeviceTree()
{
    delete m_pRootDevice;
    delete m_pTempDevice;
    delete m_pszURLBase;
    delete m_pszDescURL;
    delete m_pszDescFileName;
    delete m_pszName;
    delete m_pszUDN;

    DeleteCriticalSection(&m_cs);
}

BOOL HostedDeviceTree::Shutdown(DWORD dwFlags)
{
    DWORD dwStatus;
    TraceTag(ttidTrace, "%s: [%08x] Shutdown\n", __FUNCTION__, this);

    Lock();

    Unpublish();
    m_state = DevTreeInvalid;
    m_hShutdownEvent = CreateEvent(NULL,FALSE,FALSE,NULL);

    Unlock();

    if (! (dwFlags & HDT_NO_CALLBACKS))
    {
        // notify device owner that his device is going away
        DoDeviceCallback(NULL, UPNPCB_SHUTDOWN, NULL, NULL, NULL);
    }

    DecRef();   // normally decrements to zero
    dwStatus = WaitForSingleObject(m_hShutdownEvent, 15000);

    AssertSz(dwStatus == WAIT_OBJECT_0, "HostedDeviceTree not released on shutdown!");

    CloseHandle(m_hShutdownEvent);
    m_hShutdownEvent = NULL;

    DeleteFile(m_pszDescFileName);

    TraceTag(ttidTrace, "%s: [%08x] Shutdown Complete\n", __FUNCTION__, this);
    return TRUE;
}


// CreateUDN
void HostedDeviceTree::CreateUDN(LPWSTR *ppUDN)
{
    Assert(ppUDN);
    Assert(!*ppUDN);
    
    if(!m_nUDNSuffix)
    {
        // root device
        *ppUDN = StrDupW(m_pszUDN);
    }
    else
    {
        // embedded device
        int cchUDN = wcslen(m_pszUDN) + 11; // max size of integer string

        *ppUDN = new WCHAR [cchUDN];

        if (*ppUDN)
        {
            cchUDN -= wsprintfW(*ppUDN, L"%.37s%04x", m_pszUDN, m_nUDNSuffix);
            Assert(cchUDN > 0);
        }
    }

    m_nUDNSuffix++;
}


HostedDevice::~HostedDevice()
{
    delete m_pNextDev;
    delete m_pFirstChildDev;
    delete m_pFirstService;
    delete m_pszUDN;
    delete m_pszOrigUDN;
    delete m_pszDeviceType;
}


BOOL HostedDevice::SsdpRegister(DWORD dwLifeTime, PCWSTR pszDescURL )
{
    BOOL fRet = TRUE;
    // publish the UDN
    m_hSSDPUDN = RegisterSSDPService(dwLifeTime, pszDescURL, m_pszUDN, m_pszUDN);
    // publish the device Type
    m_hSSDPDevType = RegisterSSDPService(dwLifeTime, pszDescURL, m_pszUDN, m_pszDeviceType);

    if (VALIDSSDPH(m_hSSDPUDN) && VALIDSSDPH(m_hSSDPDevType))
    {
        HostedService *pService;
        fRet = TRUE;
        for (pService=m_pFirstService;pService;pService=pService->NextService())
        {
            fRet = pService->SsdpRegister(dwLifeTime, pszDescURL, m_pszUDN);
        }
        
    }
    // recurse down
    if (fRet && m_pFirstChildDev)
    {
        fRet = m_pFirstChildDev->SsdpRegister(dwLifeTime, pszDescURL);
    }

    // recurse right
    if (fRet && m_pNextDev)
    {
        fRet = m_pNextDev->SsdpRegister(dwLifeTime, pszDescURL);
    }

    if (!fRet)
    {
        SsdpUnregister();
    }
    return fRet;
}

BOOL HostedDevice::SsdpUnregister(void)
{
    BOOL fRet = TRUE;
    HostedService *pService;
    if (m_pFirstChildDev)
        fRet &= m_pFirstChildDev->SsdpUnregister();

    if (m_pNextDev)
        fRet &= m_pNextDev->SsdpUnregister();

    if (VALIDSSDPH(m_hSSDPUDN))
    {
        fRet &= DeregisterUpnpService(m_hSSDPUDN, TRUE);
    }
    m_hSSDPUDN = INVALID_HANDLE_VALUE;
    
    if (VALIDSSDPH(m_hSSDPDevType))
    {
        fRet &= DeregisterUpnpService(m_hSSDPDevType, TRUE);
    }
    m_hSSDPDevType = INVALID_HANDLE_VALUE;

    for (pService=m_pFirstService;pService;pService=pService->NextService())
    {
        fRet &= pService->SsdpUnregister();
    }
    
    return fRet;
    
}

BOOL
HostedDeviceTree::DoDeviceCallback(ControlRequest *pControlReq, UPNPCB_ID id, PCWSTR pszUDN, PCWSTR pszServiceId, PCWSTR pszReqXML)
{
    BOOL fRet = FALSE;
    DispatchGate *pGate = NULL;

    TraceTag(ttidControl, "%s: [%08x] Perform Callback [%08x]\n", __FUNCTION__, this, m_pfCallback);

    if (!m_pfCallback)
    {
        return FALSE;
    }

    pGate = DispatchGate::FindDispatchGateForProc(m_hOwnerProc);
    if (!pGate)
    {
        return FALSE;
    }

    fRet = pGate->EnterGate();
    if (!fRet)
    {
        pGate->DecRef();     // ensure that the gate reference is released
        return FALSE;           // we are unable to initiate a callback
    }

    fRet = FALSE;
    UpnpInvokeRequestContainer_t requestContainer;

    __try {

            if(requestContainer.CreateUpnpInvokeRequest(
                                                    pszReqXML, 
                                                    pszServiceId, 
                                                    pszUDN, 
                                                    (DWORD) m_pszName, 
                                                    (DWORD) m_pvCallbackContext, 
                                                    id) == FALSE) 
            {
                    TraceTag(ttidError,"%s: [%08x] Exception packing invoke request in DoDeviceCallback id= %d", __FUNCTION__, this, id);
                    goto Finish;
            }

            m_pCurControlReq = pControlReq;
            TraceTag(ttidControl, "%s: [%08x] Setting Current Control Request [%08x]\n", __FUNCTION__, this, m_pCurControlReq);
            fRet = pGate->DoCallback(requestContainer); // this will block
            m_pCurControlReq = NULL;
            TraceTag(ttidControl, "%s: [%08x] Unsetting Current Control Request [%08x]\n", __FUNCTION__, this, m_pCurControlReq);

    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        TraceTag(ttidError,"Exception in DoDeviceCallback id= %d", id);
    }

Finish:

    pGate->LeaveGate();
    pGate->DecRef();

    return fRet;
}


BOOL
HostedDeviceTree::Control(ControlRequest *pCReq)
{
    HostedService *pService;
    BOOL fRet = FALSE;
    TraceTag(ttidControl, "%s: [%08x] Control Request for UDN[%S]\n", __FUNCTION__, this, m_pszName);
    TraceTag(ttidControl, "%s: [%08x] Control Request for ServiceID[%S]\n", __FUNCTION__, this, pCReq->ServiceId());


    Lock();

    Assert(m_pRootDevice);
    pService = m_pRootDevice->FindService(pCReq->UDN(), pCReq->ServiceId());
    TraceTag(ttidControl, "%s: [%08x] Control Request Service [%08x]\n", __FUNCTION__, this, pService);

    if (m_state >= DevTreePublished && pService)
    {
        // assume its okay to control
        if (m_state < DevTreeRunning)
        {
            // the device is being invoked for the first time
            m_state = DevTreeRunning;

            // Send the startup notification to the device implementation
            TraceTag(ttidControl, "%s: [%08x] UPNPCB_INIT\n", __FUNCTION__, this);

            Unlock();
            fRet = DoDeviceCallback(NULL, UPNPCB_INIT, NULL, NULL, NULL);
            Lock();

            if (!fRet && m_state > DevTreePublished)
            {
                m_state = DevTreePublished; // uh-oh Init failed
                TraceTag(ttidControl, "%s: [%08x] failed init\n", __FUNCTION__, this);
            }
        }

        // since we may have changed state in the previous Unlock(), check again
        if (m_state >= DevTreeRunning)
        {
            TraceTag(ttidControl, "%s: [%08x] UPNPCB_CONTROL\n", __FUNCTION__, this);

            Unlock();
            fRet = DoDeviceCallback(pCReq, UPNPCB_CONTROL, pCReq->UDN(), pCReq->ServiceId(), pCReq->RequestXML());
            Lock();
        }
    }

    Unlock();

    TraceTag(ttidControl, "%s: [%08x] Completed Control Request [%S]\n", __FUNCTION__, this, fRet ? L"TRUE" : L"FALSE");
    return fRet;
}

BOOL
HostedDeviceTree::Subscribe(PCWSTR szUDN, PCWSTR pszServiceId)
{
    HostedService *pService;
    BOOL fRet = FALSE;

    TraceTag(ttidTrace, "%s: [%08x] Subscribing UDN[%S] ServiceID[%S]\n", __FUNCTION__, this, szUDN, pszServiceId);

    Lock();

    Assert(m_pRootDevice);
    pService = m_pRootDevice->FindService(szUDN, pszServiceId);
    
    if (m_state >= DevTreePublished && pService)
    {
        // assume its okay to subscribe
        if (m_state < DevTreeRunning)
        {
            // the device is being invoked for the first time
            m_state = DevTreeRunning;

            // Send the startup notification to the device implementation

            Unlock();
            fRet = DoDeviceCallback(NULL, UPNPCB_INIT, NULL, NULL, NULL);
            Lock();

            if (!fRet && m_state > DevTreePublished)
            {
                m_state = DevTreePublished; // uh-oh Init failed
            }
        }

        // since we may have changed state in the previous Unlock(), check again
        if (m_state >= DevTreeRunning)
        {
            Unlock();
            fRet = DoDeviceCallback(NULL, UPNPCB_SUBSCRIBING, szUDN, pszServiceId, NULL);
            Lock();
        }
    }

    Unlock();

    TraceTag(ttidTrace, "%s: [%08x] Subscribed UDN[%S] ServiceID[%S]\n", __FUNCTION__, this, szUDN, pszServiceId);
    return fRet;
}

BOOL
HostedDeviceTree::Unsubscribe(PCWSTR szUDN, PCWSTR pszServiceId)
{
    TraceTag(ttidTrace, "%s: [%08x] Unsubscribing UDN[%S] ServiceID[%S]\n", __FUNCTION__, this, szUDN, pszServiceId);
    DoDeviceCallback(NULL, UPNPCB_UNSUBSCRIBING, szUDN, pszServiceId, NULL);

    TraceTag(ttidTrace, "%s: [%08x] Unsubscribed UDN[%S] ServiceID[%S]\n", __FUNCTION__, this, szUDN, pszServiceId);
    return TRUE;
}

BOOL
HostedDeviceTree::SetControlResponse(DWORD dwHttpStatus, PCWSTR pszResp)
{
    BOOL fRet = FALSE;

    Lock();

    if (m_pCurControlReq)
    {
        fRet = m_pCurControlReq->SetResponse(dwHttpStatus, pszResp);
    }
    else
    {
        return FALSE;
    }

    Unlock();
    TraceTag(ttidTrace, "%s: [%08x] Set Control Response [%S]\n", __FUNCTION__, this, fRet ? L"TRUE" : L"FALSE");

    return fRet;
}

static void FreeUPNP_PROPERTY(DWORD nArgs, UPNP_PROPERTY *rgUpnpProps)
{
    DWORD i;
    for (i=0;i<nArgs;i++)
    {
        if(rgUpnpProps[i].szName)
        {
            free(rgUpnpProps[i].szName);
        }

        if(rgUpnpProps[i].szValue)
        {
            free(rgUpnpProps[i].szValue);
        }
    }

    delete [] rgUpnpProps;
}

static UPNP_PROPERTY *CopyUPNPPARAMtoPROPERTY(DWORD nArgs, UPNPPARAM *rgArgs)
{
    UPNP_PROPERTY *rgUpnpProps = new UPNP_PROPERTY [nArgs];
    DWORD i;
    if (rgUpnpProps)
    {
        __try 
        {
            for (i=0;i<nArgs;i++)
            {
                rgUpnpProps[i].szName = _wcsdup(rgArgs[i].pszName);
                rgUpnpProps[i].szValue = _wcsdup(rgArgs[i].pszValue);
                rgUpnpProps[i].dwFlags = 0;
                if (!rgUpnpProps[i].szName || !rgUpnpProps[i].szValue)
                {
                    break;
                }
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
        }
        if (i<nArgs)
        {
            FreeUPNP_PROPERTY(i, rgUpnpProps);
            rgUpnpProps = NULL;
        }
    }
    return rgUpnpProps;
}


HostedService::~HostedService()
{
    delete m_pNextService;
    delete m_pszServiceId;
    delete m_pszServiceType;
    delete m_pszSCPDPath;

    if (m_pszaUri)
    {
        if (m_fEventing)
        {
            DeregisterUpnpEventSource(m_pszaUri);
        }
        delete m_pszaUri;
    }
}


BOOL
HostedService::SsdpRegister(DWORD dwLifeTime, PCWSTR pszDescURL, PCWSTR pszUDN )
{
    // publish the service type:
    // 
    m_hSSDPSvc = RegisterSSDPService(dwLifeTime, pszDescURL, pszUDN, m_pszServiceType);

    // Register this service as an event source
    if (VALIDSSDPH(m_hSSDPSvc))
    {
        m_fEventing = RegisterUpnpEventSource(m_pszaUri, 0, NULL);
    }
    

    return (VALIDSSDPH(m_hSSDPSvc) && m_fEventing);
}

BOOL
HostedService::SsdpUnregister()
{
    BOOL fRet = TRUE;
    if (VALIDSSDPH(m_hSSDPSvc))
    {
        fRet = DeregisterUpnpService(m_hSSDPSvc, TRUE);
    }
    m_hSSDPSvc = INVALID_HANDLE_VALUE;
    if (m_fEventing && m_pszaUri)
    {
        DeregisterUpnpEventSource(m_pszaUri);
        m_fEventing = FALSE;
    }
    return fRet;
}

BOOL
HostedService::SubmitPropertyEvent(
    DWORD dwFlags,
    DWORD nArgs,
    UPNPPARAM *rgArgs)
{
    BOOL fRet;
    UPNP_PROPERTY *rgUpnpProps;

    // convert to ascii parameter array, understood by eventing subsystem
    rgUpnpProps = CopyUPNPPARAMtoPROPERTY(nArgs, rgArgs);

    if (!rgUpnpProps)
    {
        return FALSE;
    }
        
    fRet = FALSE;
    if (m_fEventing)
    {
        fRet = SubmitUpnpPropertyEvent(m_pszaUri, dwFlags, nArgs, rgUpnpProps);
    }

    FreeUPNP_PROPERTY(nArgs, rgUpnpProps);

    return fRet;
}


HostedDevice *
HostedDevice::FindDevice(PCWSTR pszUDN)
{
    HostedDevice *pDevice = NULL;
    if (wcscmp(pszUDN, m_pszUDN) == 0)
    {
            return this; 
    }
    // recurse down
    if (m_pFirstChildDev && (pDevice = m_pFirstChildDev->FindDevice(pszUDN)))
    {
        return pDevice;
    }
    // recurse left
    if (m_pNextDev)
    {
        pDevice = m_pNextDev->FindDevice(pszUDN);
    }
    return pDevice;
    
}

HostedDevice *
HostedDevice::FindByOrigUDN(PCWSTR pszTemplateUDN)
{
    HostedDevice *pDevice = NULL;
    if (m_pszOrigUDN && wcscmp(pszTemplateUDN, m_pszOrigUDN) == 0)
    {
        return this; 
    }
    // recurse down
    if (m_pFirstChildDev && (pDevice = m_pFirstChildDev->FindByOrigUDN(pszTemplateUDN)))
    {
        return pDevice;
    }
    // recurse left
    if (m_pNextDev)
    {
        pDevice = m_pNextDev->FindByOrigUDN(pszTemplateUDN);
    }
        
    return pDevice;
    
}


HostedService *
HostedDevice::FindService(PCWSTR pszUDN, PCWSTR pszServiceId)
{
    HostedService *pService = NULL;

    if(0 == wcscmp(pszUDN, m_pszUDN))
    {
        for (pService = m_pFirstService; pService; pService = pService->NextService())
        {
            if (wcscmp(pszServiceId, pService->ServiceId()) == 0)
            {
                return pService;
            }
        }
    }

    // recurse down
    if (m_pFirstChildDev && (pService = m_pFirstChildDev->FindService(pszUDN, pszServiceId)))
    {
        return pService;
    }

    // recurse left
    if (m_pNextDev)
    {
        pService = m_pNextDev->FindService(pszUDN, pszServiceId);
    }

    return pService;
}



