//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <list.h>

#define VALIDSSDPH(handle)			(handle != INVALID_HANDLE_VALUE)
#define DEFAULT_PUBLISH_INTERVAL    1800 // default recommended by UPnP spec
#define MINIMUM_PUBLISH_INTERVAL    150  // this is the minimum allowed
#define HDT_NO_CALLBACKS			1    // flag to Shutdown()

#include "sax.h"
#include "SAXWriter.h"

class HostedDeviceTree;
class HostedDevice;
class ControlRequest;

class HostedService
{
private:
    PWSTR			m_pszServiceId;
    PWSTR			m_pszServiceType;
    PWSTR			m_pszSCPDPath;
    PSTR			m_pszaUri;
    BOOL			m_fEventing;
    HANDLE			m_hSSDPSvc;
    HostedDevice	*m_pDevice;
    HostedService	*m_pNextService;

	friend class HostedDevice;

public:
    HostedService(HostedDevice *pOwnerDevice)
        : m_pNextService(NULL),
          m_pDevice(pOwnerDevice),
          m_pszServiceId(NULL),
          m_pszServiceType(NULL),
          m_pszSCPDPath(NULL),
          m_pszaUri(NULL),
          m_fEventing(FALSE),
          m_hSSDPSvc(INVALID_HANDLE_VALUE)
    {
    }

    ~HostedService();

    HostedService * NextService(void)
    {
        return m_pNextService;
    }
    void LinkService(class HostedService *pNext)
    {
        m_pNextService = pNext;
    }
    PCWSTR ServiceId()
    {
        return m_pszServiceId;
    }
    PCWSTR SCPDPath()
    {
        return m_pszSCPDPath;
    }
    BOOL SubmitPropertyEvent(DWORD nArgs, UPNPPARAM *rgArgs);
    BOOL SsdpRegister(DWORD dwLifeTime, PCWSTR pszDescURL, PCWSTR pszUDN);
    BOOL SsdpUnregister(void);
};

class HostedDevice : ce::SAXContentHandler
{
private:
    PWSTR				m_pszUDN;
    PWSTR				m_pszOrigUDN;
    PWSTR				m_pszDeviceType;
    HANDLE				m_hSSDPUDN;
    HANDLE				m_hSSDPDevType;
    HostedDevice		*m_pTempDevice;
	HostedDevice		*m_pNextDev;
    HostedDevice		*m_pFirstChildDev;
	HostedDevice		**m_ppNextChildDev;
    HostedDeviceTree	*m_pRoot;
    HostedService		*m_pFirstService;
	HostedService		**m_ppNextService;
    // private methods
    
	// used during parsing
	ce::wstring			m_strServiceType;
	ce::wstring			m_strServiceId;
	ce::wstring			m_strServiceDescriptionURL;
	ce::wstring			m_strServiceControlURL;
	ce::wstring			m_strServiceEventSubURL;
	ce::wstring			m_strIconURL;
	ce::wstring			m_strDeviceType;
	ce::wstring			m_strUDN;
	ce::wstring			m_strPresentationURL;

	friend class HostedDeviceTree;

public:
    // public methods
    HostedDevice( HostedDeviceTree *pRoot)
		: m_pszUDN(NULL),
		  m_pszOrigUDN(NULL),
          m_pszDeviceType(NULL),
          m_hSSDPUDN(INVALID_HANDLE_VALUE),
          m_hSSDPDevType(INVALID_HANDLE_VALUE),
		  m_pTempDevice(NULL),
          m_pNextDev(NULL),
          m_pFirstChildDev(NULL),
          m_pRoot(pRoot),
          m_pFirstService(NULL),
		  m_ppNextChildDev(&m_pFirstChildDev),
		  m_ppNextService(&m_pFirstService)
    {}


    ~HostedDevice();

    PCWSTR UDN() { return m_pszUDN;}
    BOOL SetUDN(PCWSTR pszUDN);
    HostedDeviceTree *Root() {return m_pRoot;}
    HostedDevice *FindDevice(PCWSTR pszUDN);
    HostedDevice *FindByOrigUDN(PCWSTR pszTemplateUDN);
    HostedService *FindService(PCWSTR pszUDN, PCWSTR pszServiceId);
    BOOL CreateUDN();
    BOOL FileToURL(LPCWSTR pwszFile, ce::wstring* pstrURL);
    BOOL SsdpRegister(DWORD dwLifeTime, PCWSTR pszDescURL );
    BOOL SsdpUnregister(void);

// ISAXContentHandler
private:
    virtual HRESULT STDMETHODCALLTYPE startDocument(void);

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

	virtual HRESULT STDMETHODCALLTYPE endDocument(void);
};

// used only in Initialize2
struct HDTInitInfo {
    class HostedDeviceTree *pHDT;
    UPNPDEVICEINFO devInfo;
    HANDLE hOwner;
    HANDLE hOwnerProc;
    BOOL fRet;
};

typedef enum 
{
    DevTreeInvalid = 0,
    DevTreeInitialized,
    DevTreePublished,
    DevTreeRunning
} DevTreeState;

class HostedDeviceTree : ce::SAXContentHandler
{
private:
    // data
    DevTreeState	m_state;
    DWORD			m_dwLifeTime;
    LONG			m_cRef;
    PWSTR			m_pszURLBase;
    PWSTR			m_pszDescURL;
    PWSTR			m_pszDescFileName;
    PWSTR			m_pszName;
	PWSTR			m_pszUDN;
	ce::wstring		m_strSpecVersionMajor;
	ce::wstring		m_strSpecVersionMinor;
	int				m_nUDNSuffix;

    HANDLE          m_hOwner;               // opaque handle identifying owner of the device tree
    HANDLE			m_hOwnerProc;           // process into which to do callbacks
    PUPNPCALLBACK	m_pfCallback;           // callback address in target process
    PVOID			m_pvCallbackContext;    // opaque callback context
	ControlRequest	*m_pCurControlReq;      // currently outstanding control action
	
    HostedDevice	*m_pRootDevice;
	HostedDevice	*m_pTempDevice;

    HANDLE			m_hSSDPRoot;
    CRITICAL_SECTION m_cs;
    HANDLE			m_hShutdownEvent;    // don't free
    HANDLE			m_hInitEvent;        // don't free

	ce::SAXWriter	m_SAXWriter;

    // private methods
    void Lock() {EnterCriticalSection(&m_cs);}
    void Unlock() {LeaveCriticalSection(&m_cs);}

public:
    // public data 
    LIST_ENTRY m_link;
    // public methods
    HostedDeviceTree() :
        m_state(DevTreeInvalid),
        m_dwLifeTime(DEFAULT_PUBLISH_INTERVAL),
        m_cRef(0),
        m_pszDescURL(NULL),
        m_pszURLBase(NULL),
        m_pRootDevice(NULL),
		m_pTempDevice(NULL),
        m_hSSDPRoot(INVALID_HANDLE_VALUE),
        m_hShutdownEvent(NULL),
		m_hInitEvent(NULL),
        m_pszName(NULL),
        m_pfCallback(NULL),
        m_pszDescFileName(NULL),
		m_pszUDN(NULL),
		m_nUDNSuffix(0),
		m_pvCallbackContext(NULL),
		m_pCurControlReq(NULL),
		m_hOwnerProc(NULL),
        m_hOwner(NULL),
		m_SAXWriter(CP_UTF8)
    {
        InitializeListHead(&m_link);
        InitializeCriticalSection(&m_cs);
    }

    ~HostedDeviceTree();

	BOOL Parse(LPCWSTR pwszDeviceDescription);
	void CreateUDN(LPWSTR *ppUDN);

    LONG IncRef()
    {
        return InterlockedIncrement(&m_cRef);
    }
    
	LONG DecRef()
    {
        LONG cRef =  InterlockedDecrement(&m_cRef);
        if (cRef == 0 && m_hShutdownEvent)
            SetEvent(m_hShutdownEvent);
        return cRef;
    }
    
	void AddToList(LIST_ENTRY *pList)
    {
	    InsertHeadList(pList, &m_link);
    }
    
	void RemoveFromList()
    {
        RemoveEntryList(&m_link);
    }
    
	PCWSTR UDN() 
    {
        return m_pszUDN;
    }

    int UDNSuffix()
    {
        return m_nUDNSuffix;
    }
    
	PCWSTR Name()
    {
        return m_pszName;
    }
    
	PCWSTR DescFileName()
    {
        return m_pszDescFileName;
    }
    
	HANDLE OwnerProc()
    {
        return m_hOwnerProc;
    }

    HANDLE Owner()
    {
        return m_hOwner;
    }
    
	ce::SAXWriter* SAXWriter()
	{
		return &m_SAXWriter;
	}	
	
	HostedService *FindService(PCWSTR szUDN, PCWSTR pszServiceId)
    {
        return m_pRootDevice ? m_pRootDevice->FindService(szUDN, pszServiceId) : NULL;
    }
    
    HostedDevice *FindDevice(PCWSTR pszUDN)
    {
        return m_pRootDevice ? m_pRootDevice->FindDevice(pszUDN) : NULL;
    }
    
	HostedDevice *FindDeviceByOrigUDN(PCWSTR pszTemplateUDN)
    {
        if (!m_pRootDevice)
            return NULL;
        // NULL pszTemplateUDN matches the root device
        return pszTemplateUDN ? m_pRootDevice->FindByOrigUDN(pszTemplateUDN) : m_pRootDevice;
    }
    
	BOOL Initialize(UPNPDEVICEINFO *pDevInfo, HANDLE hOwnerProc, HANDLE hOwner);
    static DWORD Initialize2(void *pHDTI);
    
    BOOL Publish();
    BOOL Unpublish();
    BOOL Subscribe(PCWSTR szUDN, PCWSTR szServiceId);
	BOOL Control(ControlRequest *);
	BOOL SetControlResponse(DWORD dwHttpStatus, PCWSTR pszResponse);
    BOOL Unsubscribe(PCWSTR szUDN, PCWSTR szServiceId);
    BOOL DoDeviceCallback(ControlRequest *pControlReq, UPNPCB_ID id, PCWSTR pszUDN, PCWSTR pszServiceId, PCWSTR pszReqXML);
    BOOL Shutdown(DWORD flags = 0);
    BOOL SubmitPropertyEvent(PCWSTR szUDN, PCWSTR szServiceId, DWORD nArgs, UPNPPARAM *rgArgs);

// ISAXContentHandler
private:
    virtual HRESULT STDMETHODCALLTYPE startDocument(void);

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

	virtual HRESULT STDMETHODCALLTYPE endDocument(void);
};

#define SIMPLE_BUFFER_DEFAULT_GRANULARITY 256

//
// This class represents an incoming HTTP/UPNP control request
// It is instantiated on each HttpExtensionProc invocation.
// Parses the request and provides methods to construct the response before it is 
// returned to the control point.
//
class ControlRequest
{
	//friend BOOL SetRawControlResponse(UPNPSERVICECONTROL *pSvcCtl, DWORD dwHttpStatus,PCWSTR pszResp);

private:
	LPEXTENSION_CONTROL_BLOCK m_pecb;
	DWORD m_dwHttpStatus;	// the status that we respond with (initialized to 200 OK)
	HostedDeviceTree *m_pDevTree; // device that is hosting the invoked service
	PWSTR m_pszServiceId;	// service Id  (from the UPNP device description)
    PWSTR m_pszUDN;
	PWSTR m_pszServiceType;	// service type(from the UPNP device description)  
	PSTR m_pszResponse;		// response body, if any
	PWSTR m_pszRequestXML;	// request body
	// the UPNPSERVICECONTROL struct is used for callbacks into the device implementation.
	// It contains mapped aliases to the actionName, args etc and should not be freed or generally
	// touched.
	UPNPSERVICECONTROL m_SvcCtl;
	
public:
	ControlRequest(LPEXTENSION_CONTROL_BLOCK pecb);
	~ControlRequest();
	BOOL ParseRequest();
	BOOL ForwardRequest();
	BOOL SetResponse(DWORD dwHttpStatus, PCWSTR pszResp);
	BOOL SendResponse();
	BOOL StatusOk() { return m_dwHttpStatus == HTTP_STATUS_OK;}
	PCWSTR ServiceId(void) { return m_pszServiceId;}
    PCWSTR UDN(void) { return m_pszUDN;}
	PCWSTR RequestXML(void) { return m_pszRequestXML;}
};

const WCHAR c_szHttpPrefix[] = L"http://";
const WCHAR c_szURLPrefix[] = L"/upnp/";
const WCHAR c_szLocalWebRootDir[] = L"\\windows\\upnp\\";

PSTR StrDupWtoA(LPCWSTR pwszSource);
PWSTR StrDupAtoW(LPCSTR pszaSource);
PWSTR StrDupW(LPCWSTR pwszSource);
BOOL SubscribeCallback(BOOL fSubscribe, PSTR pszUri);
HostedDeviceTree *FindDevTreeAndServiceIdFromUri(PCSTR pszaUri, PWSTR *ppszUDN, PWSTR *ppszSid);
BOOL SetRawControlResponse(DWORD hRequest, DWORD dwHttpStatus,PCWSTR pszResp);
void UpnpCleanUpProc(HANDLE hProc);

#if 1
BOOL ProcessCallbacks(PFUPNPCALLBACK pfCallback, PVOID pvContext, PBYTE pReqBuf, DWORD cbReqBuf);
BOOL CancelCallbacks();
#endif

class DispatchGate
{
    /*
        The state sequence is:
        INVALID -> INITED -> READY -> ENTERED -> REQUEST_WAITING -> REQUEST_HANDLING
        -> REQUEST_DONE -> READY
    */
    typedef enum {
        CBGATE_INVALID,
        CBGATE_INITED,
        CBGATE_READY,
        CBGATE_ENTERED,
        CBGATE_REQUEST_WAITING,
        CBGATE_REQUEST_HANDLING,
        CBGATE_REQUEST_DONE
    } CBGateState;
    private:
    CBGateState  m_state;
    BOOL        m_fShuttingDown; // set by CancelCallbacks
    LONG    m_RefCount;      // number of threads accessing the object
    HANDLE  m_hMutexRequestor;    // serialize entry to the gate by requestors
    HANDLE  m_hEvtRequest;  // signaled when new request is created
    HANDLE  m_hEvtRequestDone;   // signaled when request is completed
    HANDLE  m_hTargetProc;  // process in which to dispatch callbacks
    CRITICAL_SECTION m_cs;  // protect access to internal state
    PFUPNPCALLBACK m_pfCallback;    //callback address in process
    PVOID   m_pvContext;            // opaque parameter to callback
    PBYTE   m_pbReqBuf;      // request buffer in target proc
    //PBYTE   m_pbRespBuf;     // response buffer in target proc
    DWORD  m_cbReqBufSize;  // max size of target request buffer
    DWORD  m_cbReqBufUsed; // request data length
    DWORD  m_dwReturnCode;	// return code from request
    // DWORD  m_cbRespBufSize; // max size of responze buffer
    //DWORD  m_cbRespBufUsed;// response data length
    DWORD	m_nDescs;			// number of requestor input data descriptors
    TAG_DESC *m_pDescs;			// array of requestor input data descriptors

    public:
    DispatchGate(HANDLE hTargetProc,
        PFUPNPCALLBACK pfCallback,
        PVOID pvContext,
        PBYTE pbReq,    //input buffer in Target proc 
        DWORD cbReqSize
        );
    ~DispatchGate();
    LONG IncRef() {return InterlockedIncrement(&m_RefCount);}
    LONG DecRef() {return InterlockedDecrement(&m_RefCount);}
    HANDLE GetTargetProc() { return m_hTargetProc; }
    BOOL WaitForRequests(DWORD dwTime = INFINITE);
    BOOL DispatchRequest(void);
    BOOL EnterGate(DWORD dwTimeout = INFINITE);
    BOOL LeaveGate(void);
    //PBYTE GetReqBuf(DWORD cbBufSize);
    BOOL DoCallback(DWORD nInDescs, TAG_DESC *pInDescs, DWORD dwTimeout = INFINITE);
    BOOL Shutdown(void);
};

DispatchGate * FindDispatchGateForProc(HANDLE hProc);

extern CRITICAL_SECTION g_csUPNP;

#define MAP(x) MapPtrToProcess(x, hMappedProc)



