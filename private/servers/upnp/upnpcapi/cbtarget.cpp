//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  File:       CALLBACK.CPP
//
//  Contents:   Application LPC callback processing for UPnP device notifications
//
//  Notes:
//      Plays the role of a LPC server, although we are running in client (application) space.
//      The LPC client in this case is the UPNPSVC service.
//
//  Author:     
//
//----------------------------------------------------------------------------
#include <windows.h>
#include <ssdppch.h>
#include <upnpdevapi.h>
#include <ssdpapi.h>
#include <sax.h>
#include <string.hxx>
#include <psl_marshaler.hxx>
#include <UpnpInvokeRequest.hpp>
#include <upnpdcall.h>

extern CRITICAL_SECTION g_csUPNPAPI;

class SOAPHandler : ce::SAXContentHandler
{
    friend BOOL WINAPI UpnpSetRawControlResponse(UPNPSERVICECONTROL *pSvcCtl, DWORD dwHttpStatus, PCWSTR pszResp);
private:

    DWORD               m_hSource;          // handle to the request object in the UPNPSVC server
    PCWSTR              m_pszUDN;
    PCWSTR              m_pszServiceId;     // UPnP service id (memory not alloced)
    PCWSTR              m_pszRequestXML;    // request body  (memory not alloced)
    PWSTR               m_pszAction;        // action name 
    DWORD               m_cParams;          // number of input parameters 
    DWORD               m_cArgsMax;         // size of the m_pParams array
    UPNPPARAM           *m_pParams;         // input parameters (parsed from request body)
    PWSTR               m_pszServiceType;   // service type(from the UPNP device description)  
    BOOL                m_fResponse;        // TRUE if SetResponse has been called
    // the UPNPSERVICECONTROL struct is used for callbacks into the device implementation.
    // It contains mapped aliases to the actionName, args etc and should not be freed or generally
    // touched.
    UPNPSERVICECONTROL  m_SvcCtl;
    ce::wstring         m_strArgumentValue;
    ce::wstring         m_strArgumentElement;
    ce::wstring         m_strActionElement;
    bool                m_bParsingBody;
    bool                m_bParsingAction;
    bool                m_bParsingArgument;
    bool                m_bParsingActionComplete;

public:
    SOAPHandler(DWORD hSource, PCWSTR pszUDN, PCWSTR pszServiceId, PCWSTR pszRequestXML);
    ~SOAPHandler();

    BOOL Parse();

    UPNPSERVICECONTROL *GetServiceControl() { return &m_SvcCtl; }
    BOOL SetResponse(DWORD dwHttpStatus, PCWSTR pszResp);
    BOOL Done(BOOL fRet);

    static ce::SAXReader* m_pReader;

// ISAXContentHandler
private:
    virtual HRESULT STDMETHODCALLTYPE startElement(
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName,
        /* [in] */ ISAXAttributes __RPC_FAR *pAttributes);
    
    virtual HRESULT STDMETHODCALLTYPE endElement( 
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName);
    
    virtual HRESULT STDMETHODCALLTYPE characters( 
        /* [in] */ const wchar_t __RPC_FAR *pwchChars,
        /* [in] */ int cchChars);
};


ce::SAXReader* SOAPHandler::m_pReader;

class CallbackTarget
{
    private:
    LIST_ENTRY m_link;
    PWSTR m_pszName;
    PUPNPCALLBACK m_pfCallback;
    PVOID m_pvUserContext;
    DWORD m_Id;

    public:

    CallbackTarget(PCWSTR pszName, PUPNPCALLBACK pfCallback, PVOID pvUserContext)
        : m_pfCallback(pfCallback),
          m_pvUserContext(pvUserContext)
    {
        InitializeListHead(&m_link);
        m_pszName = new WCHAR [wcslen(pszName)+1];
        m_Id = InterlockedIncrement(&lastId);
        if (m_pszName)
        {
            wcscpy(m_pszName, pszName);
        }
    }
    ~CallbackTarget()
    {
        // should be unlinked by now
        if (m_pszName)
        {
            delete [] m_pszName;
        }
    }

    PCWSTR Name() { return m_pszName; }
    DWORD  Handle() { return m_Id; }

    DWORD DoCallback(UPNPCB_ID, PVOID pvPara);
    // since there is only one list, the list manipulation methods are added to this
    // class as statics.
    static LIST_ENTRY list;
    static LONG lastId;
    static CallbackTarget *SearchByName(PCWSTR pszName);
    static CallbackTarget *SearchByHandle(DWORD handle);
    static void  Link(CallbackTarget *pCallback) // add to list
    {
            InsertHeadList(&list, &pCallback->m_link);     
    }
    static void Unlink(CallbackTarget *pCallback)    // remove from list
    {
            RemoveEntryList(&pCallback->m_link);
    }
        
    static void LockList(void)
    {
        EnterCriticalSection(&g_csUPNPAPI);
    }
    static void UnlockList(void)
    {
        LeaveCriticalSection(&g_csUPNPAPI);
    }
    static BOOL CleanupList();
    
};

extern ce::psl_proxy<> proxy;

HANDLE g_hLPCThread;
static BOOL g_Stopped;
static DWORD g_dwLPCThreadId;
static UpnpInvokeRequest *g_pReq;
static DWORD g_cbReqSize;


// TODO: move to common library
static PWSTR
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


LIST_ENTRY CallbackTarget::list = {&CallbackTarget::list, &CallbackTarget::list};
LONG CallbackTarget::lastId;

CallbackTarget *
CallbackTarget::SearchByName(PCWSTR pszName)
{
    LIST_ENTRY *pLink = CallbackTarget::list.Flink;
    CallbackTarget *pCallback;
    // assume lock is held
    while (pLink != &CallbackTarget::list)
    {
        pCallback = CONTAINING_RECORD(pLink, CallbackTarget, m_link);
        if (wcscmp(pszName, pCallback->m_pszName) == 0)
        {
            return pCallback;
        }
        pLink = pLink->Flink;
    }
    return NULL;
}

CallbackTarget *
CallbackTarget::SearchByHandle(DWORD handle)
{
    LIST_ENTRY *pLink = CallbackTarget::list.Flink;
    CallbackTarget *pCallback;
    // assume lock is held
    while (pLink != &CallbackTarget::list)
    {
        pCallback = CONTAINING_RECORD(pLink, CallbackTarget, m_link);
        if (pCallback->Handle() == handle)
        {
            return pCallback;
        }
        pLink = pLink->Flink;
    }
    return NULL;
}

BOOL
CallbackTarget::CleanupList()
{
    
    CallbackTarget *pCallback;
    while (CallbackTarget::list.Flink != &CallbackTarget::list)
    {
        pCallback = CONTAINING_RECORD(CallbackTarget::list.Flink, CallbackTarget, m_link);
        if (pCallback)
        {
            CallbackTarget::Unlink(pCallback);  // remove from list
            delete pCallback;
        }
    }
    return TRUE;
}
    
DWORD
CallbackTarget::DoCallback(UPNPCB_ID cbId, PVOID pvPara)
{
    DWORD dwRet = ERROR_GEN_FAILURE;
    PVOID pvUserContext = m_pvUserContext;
    
    if (cbId == UPNPCB_SHUTDOWN)
    {
        // prevent double-deletion in DeviceProxy::ShutdownCallback
        // This is safe to do as DoCallback is already inside a
        // LockList/UnlockList block
        m_pvUserContext = NULL;
    }
    
    __try
    {
        if (pvUserContext)
        {
            dwRet = (*m_pfCallback)(cbId, pvUserContext, pvPara);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        dwRet = ERROR_GEN_FAILURE;
    }
    
    return dwRet;
}


static BOOL
UnmarshalCallbackRequest(PBYTE pbReq, DWORD cbReq, UpnpRequest **ppUpnpReq, PWSTR *ppszUDN, PWSTR *ppszServiceId, PWSTR *ppszReqXML)
{
    UpnpInvokeRequestContainer_t upnpInvokeRequestContainer;

    if(upnpInvokeRequestContainer.CreateUpnpInvokeRequest(pbReq, cbReq) == FALSE) 
    {
        return FALSE;
    }

    *ppUpnpReq = upnpInvokeRequestContainer.GetUpnpRequest();
    *ppszServiceId = static_cast<PWSTR>(const_cast<WCHAR*>(upnpInvokeRequestContainer.GetServiceID()));
    *ppszUDN = static_cast<PWSTR>(const_cast<WCHAR*>(upnpInvokeRequestContainer.GetUDN()));
    *ppszReqXML = static_cast<PWSTR>(const_cast<WCHAR*>(upnpInvokeRequestContainer.GetRequestXML()));

    return TRUE;        
}


static DWORD CALLBACK UPNPCallback(PVOID pvContext, PBYTE pbInBuf, DWORD cbInBuf)
{
    UpnpRequest *pReq = NULL;
    CallbackTarget *pCallback;
    BOOL fRet = FALSE;
    PWSTR pszServiceId = NULL;
    PWSTR pszReqXML = NULL;
    PWSTR pszUDN = NULL;

    TraceTag(ttidControl, "UPNPCallback: ");
    if (cbInBuf < sizeof(UpnpRequest))
    {
        return FALSE;
    }

    if (UnmarshalCallbackRequest( pbInBuf, cbInBuf, &pReq, &pszUDN, &pszServiceId, &pszReqXML) && pReq)
    {
        TraceTag(ttidControl, "UPNPCallback: CB_ID(%d), service %S, hTarget(%x)", pReq->cbId, (pszServiceId ? pszServiceId : L"NULL"), pReq->hTarget);
        // which of the callback targets is this msg directed to?
        CallbackTarget::LockList();
        pCallback = CallbackTarget::SearchByHandle(pReq->hTarget);
        if (pCallback)
        {
            switch (pReq->cbId)
            {
                case UPNPCB_INIT:
                case UPNPCB_SHUTDOWN:
                    
                    fRet = pCallback->DoCallback(pReq->cbId, NULL);
                    break;

                case UPNPCB_SUBSCRIBING:
                case UPNPCB_UNSUBSCRIBING:
                    
                    {
                        UPNPSUBSCRIPTION UPnPSubscription;
                        UPnPSubscription.pszSID = pszServiceId;
                        UPnPSubscription.pszUDN = pszUDN;
                    
                        fRet = pCallback->DoCallback(pReq->cbId, &UPnPSubscription);
                    }
                    break;

                case UPNPCB_CONTROL:
                {
                    SOAPHandler soapHandler(pReq->hSource, pszUDN, pszServiceId, pszReqXML);
                    if (soapHandler.Parse())
                    {
                        fRet = pCallback->DoCallback(pReq->cbId, soapHandler.GetServiceControl());
                        soapHandler.Done(fRet);
                    }
                    break;
                }
            }
        }
        CallbackTarget::UnlockList();
        
    }
    return fRet;
}



static DWORD WINAPI LPCInvokeRequestHandlerThread(PVOID pvPara)
{
    BOOL fRet = TRUE;
    DWORD cbWrittenReqSize;
    DWORD errCode = ERROR_SUCCESS;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    TraceTag(ttidControl, "%s: Beginning Invoke Request Handler Thread\n", __FUNCTION__);
    Assert(!g_pReq);
    g_cbReqSize = MAXIMUM_UPNP_MESSAGE_LENGTH;
    g_pReq = static_cast<UpnpInvokeRequest*>(VirtualAlloc(NULL,g_cbReqSize,MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE));

    if(!g_pReq)
    {
        errCode = ERROR_OUTOFMEMORY;
        TraceTag(ttidError, "%s: OOM allocating request buffer [%d]\n", __FUNCTION__, errCode);
        goto Finish;
    }

    SOAPHandler::m_pReader = new ce::SAXReader;
    if(!SOAPHandler::m_pReader)
    {
        errCode = ERROR_OUTOFMEMORY;
        TraceTag(ttidError, "%s: Invoke Request Handler Thread error OOM m_pReader [%d]\n", __FUNCTION__, errCode);
        goto Finish;
    }
    
    if (FAILED(SOAPHandler::m_pReader->Init()))
    {
        errCode = ERROR_OUTOFMEMORY;
        TraceTag(ttidError, "%s: Invoke Request Handler Thread error OOM m_pReader [%d]\n", __FUNCTION__, errCode);
        goto Finish;
    }

    {
        while(g_Stopped == FALSE)
        {
            SetSentinel(g_pReq);
            UPNPApi *pUpnpApi = GetUPNPApi();
            if ( !pUpnpApi || !pUpnpApi->lpReceiveInvokeRequestImpl )
            {
                fRet = (ERROR_SUCCESS == proxy.call(UPNP_IOCTL_RECV_INVOKE_REQUEST, errCode, ce::psl_buffer(g_pReq, g_cbReqSize), &cbWrittenReqSize));
            }
            else
            {
                fRet = pUpnpApi->lpReceiveInvokeRequestImpl(errCode, (LPBYTE)g_pReq, g_cbReqSize, &cbWrittenReqSize);
            }

            errCode = ERROR_SUCCESS;
            TraceTag(ttidControl, "%s: Received Next Invoke Request [%S]\n", __FUNCTION__, fRet ? L"TRUE" : L"FALSE");
            Assert((!SentinelSet(g_pReq) && fRet) || (SentinelSet(g_pReq) && !fRet));

            if(g_Stopped == TRUE)
            {
                break;
            }

            if(fRet)
            {
                fRet = UPNPCallback(NULL, (PBYTE) g_pReq, cbWrittenReqSize);
                if(!fRet)
                {
                    errCode = GetLastError();
                    if(errCode == ERROR_SUCCESS) 
                    {
                        errCode = E_FAIL;
                    }
                    TraceTag(ttidError,"%s: Error processing handler Callback[%d]\n", __FUNCTION__, errCode);
                }
            }
            else
            {
                errCode = GetLastError();
                if(errCode == ERROR_SUCCESS)
                {
                    errCode = E_FAIL;
                }
                TraceTag(ttidError,"%s: Error Receiving Next Invoke Request [%d]\n", __FUNCTION__, errCode);
            }
        }

    }

Finish:
    if(SOAPHandler::m_pReader)
    {
        delete SOAPHandler::m_pReader;
        SOAPHandler::m_pReader = NULL;
    }

    if(g_pReq)
    {
        VirtualFree(g_pReq, g_cbReqSize, MEM_DECOMMIT);
        VirtualFree(g_pReq, 0, MEM_RELEASE);
        g_pReq = NULL;
    }

    TraceTag(ttidError,"%s: Exiting Receive Invoke Request Thread [%d]\n", __FUNCTION__, errCode);
    if(errCode != ERROR_SUCCESS)
    {
        SetLastError(errCode);
    }
    CoUninitialize();
    return fRet;
}


static BOOL
LPCInit()
{
    BOOL fRet = TRUE;
    TraceTag(ttidDevice,"%s: Initializing LPCInvokeRequestHandler Thread\n", __FUNCTION__);

    // create an empty list of callback targets
    InitializeListHead(&CallbackTarget::list);

    Assert(g_hLPCThread == NULL);

    g_Stopped = FALSE;

    // initialize the service for UPNP message requests
    UPNPApi *pUpnpApi = GetUPNPApi();
    if ( !pUpnpApi || !pUpnpApi->lpInitializeReceiveInvokeRequestImpl )
    {
        fRet = (ERROR_SUCCESS == proxy.call(UPNP_IOCTL_INIT_RECV_INVOKE_REQUEST));
    }
    else
    {
        fRet = pUpnpApi->lpInitializeReceiveInvokeRequestImpl();
    }
    Assert(fRet);

    // create a thread to listen on the port
    g_hLPCThread = CreateThread(NULL,0, LPCInvokeRequestHandlerThread, NULL, 0, &g_dwLPCThreadId);

    return ((g_hLPCThread != NULL) && fRet);
}


static BOOL
CancelCallbacks()
{
    TraceTag(ttidDevice,"%s: Cancelling UPnP Service Callbacks\n", __FUNCTION__);
    UPNPApi *pUpnpApi = GetUPNPApi();
    if ( !pUpnpApi || !pUpnpApi->lpCancelReceiveInvokeRequestImpl )
    {
        return (ERROR_SUCCESS == proxy.call(UPNP_IOCTL_CANCEL_RECV_INVOKE_REQUEST));
    }
    else
    {
        return pUpnpApi->lpCancelReceiveInvokeRequestImpl();
    }
}


static BOOL
LPCFinish()
{
    // signal the stopped flag
    g_Stopped = TRUE;
    TraceTag(ttidDevice,"%s: Stopping LPCInvokeRequestHandler Thread\n", __FUNCTION__);

    // wait for the listen thread exit
    if (g_hLPCThread)
    {
        DWORD dwWait;
        CancelCallbacks();
        dwWait = WaitForSingleObject(g_hLPCThread, 10000);
        if (dwWait == WAIT_TIMEOUT)
        {
            // This is to handle a couple of possible race conditions:
            // 1- The callback thread is blocked in LockList() while processing a callback
            // 2- CancelCallbacks() was called too early, before the callback thread had a chance to 
            //    call into the UPNPSVC process.
            // No harm done in either case, apart from the 10 second delay
            TraceTag(ttidError,"CallbackThread did not exit after 10 secs, abandoning..\n");
            CancelCallbacks();
        }
        CloseHandle(g_hLPCThread);
        g_hLPCThread = NULL;
        TraceTag(ttidDevice,"%s: Thread Handle Closed and Shutdown Signalled\n", __FUNCTION__);
    }

    // delete any remaining callbacks
    CallbackTarget::CleanupList();
    return TRUE;
    
}


BOOL
InitCallbackTarget(PCWSTR pszName, PUPNPCALLBACK pfCallback, PVOID pvContext, DWORD *phCallback)
{
    CallbackTarget *pNewCallback;
    TraceTag(ttidDevice, "%s: Adding Callback for %S\n", __FUNCTION__, pszName);

    if (!pszName || !pfCallback)
    {
        return FALSE;
    }


    // create a new callback target
    pNewCallback = NULL;
    CallbackTarget::LockList();
    if (!g_hLPCThread)
    {
        LPCInit();
    }
    if (g_hLPCThread && CallbackTarget::SearchByName(pszName) == NULL)
    {
        pNewCallback = new CallbackTarget(pszName, pfCallback, pvContext);
        if (pNewCallback)
        {
            if (pNewCallback->Name())
            {
                CallbackTarget::Link(pNewCallback);
                if (phCallback)
                {
                   *phCallback = pNewCallback->Handle(); 
                }
            }
            else
            {
                // init error
                delete pNewCallback;
                pNewCallback = NULL;
            }
        }
    }
    else if (g_hLPCThread)
    {
        SetLastError(ERROR_FILE_EXISTS);
    }
    else
    {
        SetLastError(ERROR_OUTOFMEMORY);
    }

    if (g_hLPCThread && IsListEmpty(&CallbackTarget::list))
    {
        // error case
        // there are no other targets on the list, so clean up the thread
        LPCFinish();
    }
    CallbackTarget::UnlockList();
    return (pNewCallback != NULL);
}


BOOL
StopCallbackTarget(PCWSTR pszName)
{
    CallbackTarget *pCallback;
    TraceTag(ttidDevice, "%s: Removing Callback for %S\n", __FUNCTION__, pszName);

    // delete the callback target
    CallbackTarget::LockList();
    pCallback = CallbackTarget::SearchByName(pszName);
    if (pCallback)
    {
        CallbackTarget::Unlink(pCallback);  // remove from list
        delete pCallback;
    }
    if (g_hLPCThread && IsListEmpty(&CallbackTarget::list))
    {
        // if this was the last target, clean up the callback thread
        LPCFinish();
    }
    CallbackTarget::UnlockList();
    return (pCallback != NULL);
}



const WCHAR c_szDefaultSOAPFault [] =
      L"<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\">\r\n"
      L"  <SOAP-ENV:Body>\r\n"
      L"    <SOAP-ENV:Fault>\r\n"
      L"      <faultcode>SOAP-ENV:Client</faultcode>\r\n"
      L"      <faultstring>UPnPError</faultstring>\r\n"
      L"      <detail>\r\n"
      L"        <UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\">\r\n"
      L"          <errorCode>501</errorCode>\r\n"
      L"          <errorDescription>Action Failed</errorDescription>\r\n"
      L"        </UPnPError>\r\n"
      L"      </detail>\r\n"
      L"    </SOAP-ENV:Fault>\r\n"
      L"  </SOAP-ENV:Body>\r\n"
      L"</SOAP-ENV:Envelope>";
      
const WCHAR c_szSOAPRespOK[] = L"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" \r\n"
                            L"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
                            L" <s:Body>\r\n"
                            L"   <u:%sResponse xmlns:u=\"%s\"/>\r\n"
                            L" </s:Body>\r\n"
                            L"</s:Envelope>\r\n";
                            
SOAPHandler::SOAPHandler(DWORD hSource, PCWSTR pszUDN, PCWSTR pszServiceId, PCWSTR pszRequestXML)
    :   
    m_hSource(hSource),
    m_pszServiceType(NULL),
    m_pszUDN(pszUDN),
    m_pszServiceId(pszServiceId),
    m_pszRequestXML(pszRequestXML),
    m_pszAction(NULL),
    m_cParams(0),
    m_cArgsMax(0),
    m_pParams(NULL),
    m_fResponse(FALSE),
    m_bParsingBody(false),
    m_bParsingAction(false),
    m_bParsingActionComplete(false),
    m_bParsingArgument(false)
{
}

// Parse
BOOL SOAPHandler::Parse()
{
    HRESULT hr = E_UNEXPECTED;
    ISAXXMLReader *pXMLReader = NULL;
    VARIANT vt;
    ce::auto_bstr bstrXML;
    
    ChkBool(m_pReader, E_FAIL);
    
    pXMLReader = m_pReader->GetXMLReader();
    ChkBool(pXMLReader, E_FAIL);
    
    Chk(pXMLReader->putContentHandler(this));

    bstrXML = SysAllocString(m_pszRequestXML);
    ChkBool(bstrXML != NULL, E_OUTOFMEMORY);
    
    vt.vt = VT_BSTR;
    vt.bstrVal = bstrXML;
    
    Chk(pXMLReader->parse(vt));
    
    Chk(pXMLReader->putContentHandler(NULL));
    

Cleanup:
    if(SUCCEEDED(hr))
    {
        // map the UPNPSERVICECONTROL pointers
        m_SvcCtl.pszRequestXML  = m_pszRequestXML;
        m_SvcCtl.pszUDN         = m_pszUDN;      
        m_SvcCtl.pszSID         = m_pszServiceId;
        m_SvcCtl.pszServiceType = m_pszServiceType;
        m_SvcCtl.pszAction      = m_pszAction;
        m_SvcCtl.cInArgs        = m_cParams;
        m_SvcCtl.pInArgs        = m_pParams;
        // back pointer for locating the ControlRequest from the SvcCtl
        m_SvcCtl.Reserved1 = this;      
        
        return true;
    }
    else
    {
        TraceTag(ttidError, "SOAPHandler::Parse returning false - error is : 0x%08x", hr);
        
        return false;
    }
}


static wchar_t* pwszEnvelopeElement =
    L"<http://schemas.xmlsoap.org/soap/envelope/>"
    L"<Envelope>";

static wchar_t* pwszBodyElement =
    L"<http://schemas.xmlsoap.org/soap/envelope/>"
    L"<Envelope>"
    L"<http://schemas.xmlsoap.org/soap/envelope/>"
    L"<Body>";

static wchar_t* pwszControlNamespace =
    L"urn:schemas-upnp-org:control-1-0";

// startElement
HRESULT STDMETHODCALLTYPE SOAPHandler::startElement( 
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName,
    /* [in] */ ISAXAttributes __RPC_FAR *pAttributes)
{
    SAXContentHandler::startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);
    
    // Envelope element
    if(pwszEnvelopeElement == m_strFullElementName)
    {
        // verify that we understand the encoding scheme
        const wchar_t*  pwchEncodingStyle;
        int             cbEncodingStyle;

        if(FAILED(pAttributes->getValueFromName(pwchNamespaceUri, cchNamespaceUri, L"encodingStyle", 13, &pwchEncodingStyle, &cbEncodingStyle)) ||
           0 != wcsncmp(L"http://schemas.xmlsoap.org/soap/encoding/", pwchEncodingStyle, cbEncodingStyle))
        {
            return E_FAIL;
        }
    }

    // argument element - either no namespace (actions) or "control" namespace (QueryStateVariable)
    if(m_bParsingAction && !m_bParsingArgument && 0 == wcsncmp(pwchNamespaceUri, pwszControlNamespace, cchNamespaceUri))
    {
        // reallocate arguments array if needed
        if (m_cParams == m_cArgsMax)
        {
            UPNPPARAM *pParamsTmp;

            if(m_cArgsMax)
            {
                m_cArgsMax *= 2;
            }
            else
            {
                m_cArgsMax = 2;
            }
            
            if(!(pParamsTmp = new UPNPPARAM [m_cArgsMax]))
            {
                return ERROR_OUTOFMEMORY;
            }

            memcpy(pParamsTmp, m_pParams, m_cParams * sizeof(UPNPPARAM));

            delete [] m_pParams;
            m_pParams = pParamsTmp;
        }

        // copy argument name
        if(LPWSTR pszName = new WCHAR[cchLocalName + 1])
        {
            wcsncpy(pszName, pwchLocalName, cchLocalName);
            pszName[cchLocalName] = L'\x0';

            m_pParams[m_cParams].pszName = pszName;
        }
        else
        {
            m_pParams[m_cParams].pszName = NULL;
        }

        // argument value to be set later
        m_pParams[m_cParams].pszValue = NULL;

        m_strArgumentValue.resize(0);
        m_bParsingArgument = true;
        m_strArgumentElement = m_strFullElementName;
    }
    
    // action element
    if(m_bParsingBody && !m_bParsingAction && !m_bParsingActionComplete)
    {
        // copy service type
        if (m_pszServiceType != NULL)
        {
            delete [] m_pszServiceType;
        }
        if(m_pszServiceType = new WCHAR[cchNamespaceUri + 1])
        {
            wcsncpy(m_pszServiceType, pwchNamespaceUri, cchNamespaceUri);
            m_pszServiceType[cchNamespaceUri] = L'\x0';
        }

        // copy action name
        if (m_pszAction != NULL)
        {
            delete [] m_pszAction;
        }
        if(m_pszAction = new WCHAR[cchLocalName + 1])
        {
            wcsncpy(m_pszAction, pwchLocalName, cchLocalName);
            m_pszAction[cchLocalName] = L'\x0';
        }
        
        m_bParsingAction = true;
        m_strActionElement = m_strFullElementName;
    }
    
    // Body element
    if(pwszBodyElement == m_strFullElementName)
    {
        m_bParsingBody = true;
        m_bParsingActionComplete = false;
    }

    return S_OK;
}


// endElement
HRESULT STDMETHODCALLTYPE SOAPHandler::endElement( 
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName)
{
    // argument element
    if(m_strArgumentElement == m_strFullElementName)
    {
        // trim white spaces
        m_strArgumentValue.trim(L"\n\r\t ");
        
        // set argument value
        m_pParams[m_cParams].pszValue = StrDupW(m_strArgumentValue);
        
        // increment arguments count
        m_cParams++;
        
        m_bParsingArgument = false;
    }

    // action element
    if(m_strActionElement == m_strFullElementName)
    {
        m_bParsingActionComplete = true;
        m_bParsingAction = false;
    }

    // Body element
    if(pwszBodyElement == m_strFullElementName)
    {
        m_bParsingBody = false;
    }

    return SAXContentHandler::endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
}


// characters
HRESULT STDMETHODCALLTYPE SOAPHandler::characters( 
    /* [in] */ const wchar_t __RPC_FAR *pwchChars,
    /* [in] */ int cchChars)
{
    if(m_strArgumentElement == m_strFullElementName)
    {
        m_strArgumentValue.append(pwchChars, cchChars);
    }

    return S_OK;
}


SOAPHandler::~SOAPHandler()
{
    DWORD i;
    m_SvcCtl.Reserved1 = NULL;  // this helps catch bogus callbacks after we're gone

    delete [] m_pszServiceType;
    delete [] m_pszAction;

    for (i=0; i < m_cParams; i++)
    {
        delete [] const_cast<LPWSTR>(m_pParams[i].pszName);
        delete [] const_cast<LPWSTR>(m_pParams[i].pszValue);
    }

    delete [] m_pParams;
}

BOOL
SOAPHandler::SetResponse(DWORD dwHttpStatus, PCWSTR pszResp)
{
    UPNPApi *pUpnpApi = GetUPNPApi();
    if ( !pUpnpApi || !pUpnpApi->lpSetRawControlResponseImpl )
    {
        return (m_fResponse = (ERROR_SUCCESS == proxy.call(UPNP_IOCTL_SET_RAW_CONTROL_RESPONSE, m_hSource, dwHttpStatus, pszResp)));
    }
    else
    {
        return (m_fResponse = pUpnpApi->lpSetRawControlResponseImpl(m_hSource, dwHttpStatus, pszResp));
    }
}

//
// This method is called at the end of request handling to send a default response, if necessary
BOOL
SOAPHandler::Done(BOOL fRet)
{
    //
    // only set the response if the device implementation has not already called SetRawControlResponse
    //
    if (!m_fResponse)
    {
        if (fRet)
        {
            // default success response
            int cch = wcslen(m_pszAction) + wcslen(m_pszServiceType) + celems(c_szSOAPRespOK);
            PWCHAR pszRawResp = new WCHAR [cch];
            if (pszRawResp)
            {
                cch -= wsprintfW(pszRawResp,c_szSOAPRespOK,m_pszAction, m_pszServiceType);
                Assert(cch > 0);
                fRet = SetResponse(HTTP_STATUS_OK, pszRawResp);
                delete[] pszRawResp;
            }
            
        }
        else
        {
            // default failure response
            fRet = SetResponse(HTTP_STATUS_SERVER_ERROR, c_szDefaultSOAPFault);   
        }
        m_fResponse = TRUE;
    }
    else
    {
        fRet = TRUE;
    }

  return fRet;
}


//
// Called from the device implementation, while processing the UPnP control request
// Locate the owning ControlRequest from the UPNPSERVICECONTROL structure and
// set the response bytes that are to be returned to the control point.
//
BOOL
WINAPI
UpnpSetRawControlResponse(UPNPSERVICECONTROL *pSvcCtl, DWORD dwHttpStatus, PCWSTR pszResp)
{
    SOAPHandler *pSoapReq;
    BOOL fRet;
    __try {
        pSoapReq = (SOAPHandler *)pSvcCtl->Reserved1;
        // check that this is an actual ControlRequest
        if (&pSoapReq->m_SvcCtl != pSvcCtl)
        {
            // failed the sanity check
            Assert(FALSE);
            fRet = FALSE;
            __leave;
        }
        fRet = pSoapReq->SetResponse( dwHttpStatus, pszResp);
        
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        fRet = FALSE;
    }
    return fRet;
}
