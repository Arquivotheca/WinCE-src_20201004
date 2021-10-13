//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
//  
//
//----------------------------------------------------------------------------
#include <windows.h>
#include <ssdppch.h>
#include <upnpdevapi.h>
#include <ssdpapi.h>

#include "sax.h"
#include "string.hxx"

struct CoGate
{
	CoGate(DWORD dwCoInit = COINIT_MULTITHREADED)
		{CoInitializeEx(NULL, dwCoInit); }

	~CoGate()
		{CoUninitialize(); }
};

extern CRITICAL_SECTION g_csUPNPAPI;

class SOAPHandler : ce::SAXContentHandler
{
    friend BOOL WINAPI UpnpSetRawControlResponse(UPNPSERVICECONTROL *pSvcCtl, DWORD dwHttpStatus, PCWSTR pszResp);
private:

    DWORD				m_hSource;			// handle to the request object in the UPNPSVC server
	PCWSTR              m_pszUDN;
    PCWSTR				m_pszServiceId;		// UPnP service id (memory not alloced)
	PCWSTR				m_pszRequestXML;	// request body  (memory not alloced)
    PWSTR				m_pszAction;		// action name 
	DWORD				m_cParams;			// number of input parameters 
	DWORD				m_cArgsMax;			// size of the m_pParams array
	UPNPPARAM			*m_pParams;			// input parameters (parsed from request body)
	PWSTR				m_pszServiceType;	// service type(from the UPNP device description)  
	BOOL				m_fResponse;		// TRUE if SetResponse has been called
	// the UPNPSERVICECONTROL struct is used for callbacks into the device implementation.
	// It contains mapped aliases to the actionName, args etc and should not be freed or generally
	// touched.
	UPNPSERVICECONTROL	m_SvcCtl;
	ce::wstring			m_strArgumentValue;
	ce::wstring			m_strArgumentElement;
	ce::wstring			m_strActionElement;
	bool				m_bParsingBody;
	bool				m_bParsingAction;
	bool				m_bParsingArgument;

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
            wcscpy(m_pszName, pszName);
    }
    ~CallbackTarget()
    {
        // should be unlinked by now
        if (m_pszName)
            delete [] m_pszName;
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

extern HANDLE g_hUPNPSVC;		// handle to SSDP service

HANDLE g_hLPCThread;
static DWORD g_dwLPCThreadId;
static UPNP_REQ *g_pReq;
static DWORD g_cbReqSize;
static UPNP_REP *g_pRep;
static DWORD g_cbRepSize;

// TODO: move to common library
static PWSTR
StrDupW(LPCWSTR pwszSource)
{
    PWSTR pwsz;
    size_t nBytes;
    if (!pwszSource)
        return NULL;
    nBytes =  wcslen(pwszSource)+1;
    if (pwsz = new WCHAR [nBytes])
		memcpy(pwsz,pwszSource,nBytes*sizeof(WCHAR));
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
            return pCallback;
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
            return pCallback;
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
    DWORD dwRet = 0;
    __try {
        dwRet = (*m_pfCallback)(cbId, m_pvUserContext, pvPara);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return dwRet;
}

#define CHECKOFFSET(offset, totalLength) ((!offset) || ((offset) >= sizeof(UPNP_REQ) && (offset) < (totalLength)))


static BOOL
UnmarshalCallbackRequest(PBYTE pbReq,DWORD cbReq, UPNP_REQ **ppUpnpReq, PWSTR *ppszUDN, PWSTR *ppszServiceId, PWSTR *ppszReqXML)
{
    DWORD nDescs;
    TAG_DESC *pTagDescs;
    DWORD i;
    if (cbReq < sizeof(DWORD) + sizeof(TAG_DESC)+sizeof(UPNP_REQ) || (nDescs = *(PDWORD)pbReq) < 1 || nDescs > 4)
        return FALSE;   // invalid request buf structure
        
    pTagDescs = (TAG_DESC *)(pbReq + sizeof(DWORD));

    for (i=0; i<nDescs;i++)
    {
        if (ppUpnpReq && pTagDescs[i].wTag == TAG_REQHEADER)
            *ppUpnpReq = (UPNP_REQ *)pTagDescs[i].pbData;
        else if (ppszServiceId && pTagDescs[i].wTag == TAG_SERVICEID)
            *ppszServiceId = (PWCHAR)pTagDescs[i].pbData;
        else if (ppszUDN && pTagDescs[i].wTag == TAG_UDN)
            *ppszUDN = (PWCHAR)pTagDescs[i].pbData;
        else if (ppszReqXML && pTagDescs[i].wTag == TAG_REQSOAP)
            *ppszReqXML =   (PWCHAR)pTagDescs[i].pbData;
    }

    return TRUE;
        
}

static DWORD CALLBACK UPNPCallback(PVOID pvContext, PBYTE pbInBuf, DWORD cbInBuf)
{
    UPNP_REQ            *pReq = NULL;
    CallbackTarget      *pCallback;
    BOOL                fRet = FALSE;
    PWSTR               pszServiceId = NULL;
    PWSTR               pszReqXML = NULL;
    PWSTR               pszUDN = NULL;

    TraceTag(ttidControl, "UPNPCallback: ");
    if (cbInBuf < sizeof(UPNP_REQ))
        return FALSE;

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

static DWORD WINAPI LPCCallbackThread(PVOID pvPara)
{
    CoGate co;

	BOOL fRet;
    DWORD cbBytesReturned;
    Assert(!g_pReq);
    g_cbReqSize = MAX_UPNP_MESSAGE_LENGTH;
    
    g_pReq = (UPNP_REQ *)VirtualAlloc(NULL,g_cbReqSize,MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    if (!g_pReq)
        return 1;

    SOAPHandler::m_pReader = new ce::SAXReader;
	
    struct ProcessCallbacksParams parms= {UPNPCallback, NULL};

	fRet = DeviceIoControl(
			g_hUPNPSVC,
			UPNP_IOCTL_PROCESS_CALLBACKS,
			&parms, sizeof(parms) , 
			g_pReq, g_cbReqSize,    // this is the buffer used for callback input params
			&cbBytesReturned, NULL);

	VirtualFree(g_pReq, g_cbReqSize, MEM_DECOMMIT|MEM_RELEASE);
	g_pReq = NULL;

    delete SOAPHandler::m_pReader;

    return fRet;
}


static BOOL
LPCInit()
{
    // create an empty list of callback targets
    InitializeListHead(&CallbackTarget::list);

    Assert(g_hLPCThread == NULL);

    // create a thread to listen on the port
    g_hLPCThread = CreateThread(NULL,0, LPCCallbackThread, NULL, 0, &g_dwLPCThreadId);

    return (g_hLPCThread != NULL);
    
}

static BOOL
CancelCallbacks()
{
    DWORD fRet;
    DWORD cbBytesReturned;
	fRet = DeviceIoControl(
			g_hUPNPSVC,
			UPNP_IOCTL_CANCEL_CALLBACKS,
			NULL, 0 , 
			NULL, 0, 
			&cbBytesReturned, NULL);
    return fRet;
}

static BOOL
LPCFinish()
{
    // wait for the listen thread exit
    if (g_hLPCThread)
    {
        DWORD dwWait;
        CancelCallbacks();
        dwWait = WaitForSingleObject(g_hLPCThread,10000);
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
    }
    // delete any remaining callbacks
    CallbackTarget::CleanupList();
    return TRUE;
    
}

BOOL
InitCallbackTarget(PCWSTR pszName, PUPNPCALLBACK pfCallback, PVOID pvContext, DWORD *phCallback)
{
    CallbackTarget *pNewCallback;
        
    if (!pszName || !pfCallback)
        return FALSE;
    
    // create a new callback target
    //
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
                   *phCallback = pNewCallback->Handle(); 
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
        SetLastError(ERROR_FILE_EXISTS);
    else
        SetLastError(ERROR_OUTOFMEMORY);
    
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
	m_bParsingArgument(false)
{
}


// Parse
BOOL SOAPHandler::Parse()
{
    HRESULT hr = E_UNEXPECTED;
    
    if(m_pReader->valid())
    {
        (*m_pReader)->putContentHandler(this);

        VARIANT vt;

        vt.vt = VT_BSTR;
        vt.bstrVal = SysAllocString(m_pszRequestXML);
        
        hr = (*m_pReader)->parse(vt);

        if(FAILED(hr))
            TraceTag(ttidError, "SAXXMLReader::parse returned error 0x%08x", hr);
        
        (*m_pReader)->putContentHandler(NULL);
        
        // avoid VariantClear to minimize dependency on oleaut32
        SysFreeString(vt.bstrVal);
    }
    else
        hr = m_pReader->Error();

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
		return false;
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
				m_cArgsMax *= 2;
			else
				m_cArgsMax = 2;
			
			if(!(pParamsTmp = new UPNPPARAM [m_cArgsMax]))
				return E_FAIL;

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
            m_pParams[m_cParams].pszName = NULL;

		// argument value to be set later
		m_pParams[m_cParams].pszValue = NULL;

		m_strArgumentValue.resize(0);
		m_bParsingArgument = true;
		m_strArgumentElement = m_strFullElementName;
	}
	
	// action element
	if(m_bParsingBody && !m_bParsingAction)
	{
		// copy service type
		Assert(!m_pszServiceType);

		if(m_pszServiceType = new WCHAR[cchNamespaceUri + 1])
		{
			wcsncpy(m_pszServiceType, pwchNamespaceUri, cchNamespaceUri);
			m_pszServiceType[cchNamespaceUri] = L'\x0';
		}

		// copy action name
		Assert(!m_pszAction);

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
		m_bParsingBody = true;

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
		m_bParsingAction = false;

	// Body element
	if(pwszBodyElement == m_strFullElementName)
		m_bParsingBody = false;

    return SAXContentHandler::endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
}


// characters
HRESULT STDMETHODCALLTYPE SOAPHandler::characters( 
    /* [in] */ const wchar_t __RPC_FAR *pwchChars,
    /* [in] */ int cchChars)
{
    if(m_strArgumentElement == m_strFullElementName)
        m_strArgumentValue.append(pwchChars, cchChars);

    return S_OK;
}


SOAPHandler::~SOAPHandler()
{
    DWORD i;
    m_SvcCtl.Reserved1 = NULL;	// this helps catch bogus callbacks after we're gone

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
    struct SetRawControlResponseParams parms= {m_hSource, dwHttpStatus, pszResp};
    DWORD cbBytesReturned;
    BOOL fRet;

	fRet = DeviceIoControl(
			g_hUPNPSVC,
			UPNP_IOCTL_SET_RAW_CONTROL_RESPONSE,
			&parms, sizeof(parms) , 
			NULL, 0,    
			&cbBytesReturned, NULL);

	m_fResponse = fRet;

	return fRet;
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
        fRet = TRUE;
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



