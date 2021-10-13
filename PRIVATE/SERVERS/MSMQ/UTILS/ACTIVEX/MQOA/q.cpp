//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// MSMQQueueObj.Cpp
//=--------------------------------------------------------------------------=
//
// the MSMQQueue object
//
//
#include "IPServer.H"

#include "LocalObj.H"
#include "dispids.h"


#include "oautil.h"
#include "q.H"
#include "msg.h"
#include "qinfo.h"
// #include "txdtc.h"             // transaction support.
// #include "xact.h"

extern UINT OrdinalOfPropId(MQMSGPROPS *pmsgprops, MSGPROPID propid);
// extern HRESULT GetCurrentViperTransaction(ITransaction **pptransaction);


// for ASSERT and FAIL
//
SZTHISFILE

// debug...
#define new DEBUG_NEW
#if DEBUG
#define SysAllocString DebSysAllocString
#define SysReAllocString DebSysReAllocString
#define SysFreeString DebSysFreeString
#endif // DEBUG

// Used to coordinate user-thread queue ops and 
//  queue lookup in falcon-thread callback 
//
extern CRITICAL_SECTION g_csCallback;

// Global list of MSMQQueue instances
// Used to map queue handle to queue object
//
QueueNode *CMSMQQueue::m_pqnodeFirst = NULL;

// Global list of callback params
CallbackNode *CMSMQQueue::m_pcallbacknodeFirst = NULL;

// defined in MSMQMessage
extern PROPREC g_rgmsgproprec[];
extern ULONG g_cPropRec;
#if 0
extern PROPREC g_rgproprecAsyncReceive[];
extern ULONG g_cPropRecAsyncReceive;
#endif // 0

/* helper: provided by msg.cpp
extern HRESULT GetOptionalTransaction(
    VARIANT *pvarTransaction,
    ITransaction **pptransaction,
    BOOL *pisRealXact);
*/

//=--------------------------------------------------------------------------=
// HELPER::InitMessageProps
//=--------------------------------------------------------------------------=
// Inits MQMSGPROPS struct.  
//
// Parameters:
//
// Output:
//
// Notes:
//
void InitMessageProps(MQMSGPROPS *pmsgprops)
{
    pmsgprops->aPropID = NULL;
    pmsgprops->aPropVar = NULL;
    pmsgprops->aStatus = NULL;
}

//=--------------------------------------------------------------------------=
// CMSMQQueue::AddQueue, RemQueue, PqOfHandle
//=--------------------------------------------------------------------------=
// Static instance list manipulation methods
//
// Parameters:
//    pq    [in]  queue to be added to list
//
// Output:
//
// Notes:
//    pq is not addref'ed.
//    We critsect queue closure/removal and the callback.
//
HRESULT CMSMQQueue::AddQueue(CMSMQQueue *pq)
{
    QueueNode *pqnode;
    HRESULT hresult = NOERROR;


    pqnode = new QueueNode;
    if (pqnode == NULL) {
      hresult = E_OUTOFMEMORY;
    }
    else {
      EnterCriticalSection(&g_csCallback);    // synchs falcon callback queue lookup
      // cons
      pqnode->m_pq = pq;
      pqnode->m_pqnodeNext = m_pqnodeFirst;
      m_pqnodeFirst = pqnode;
      LeaveCriticalSection(&g_csCallback);    // synchs falcon callback queue lookup
    }

    return hresult;
}


HRESULT CMSMQQueue::RemQueue(CMSMQQueue *pq)
{
    EnterCriticalSection(&g_csCallback);    // synchs falcon callback queue lookup
    QueueNode *pqnodeCur = m_pqnodeFirst;
    QueueNode *pqnodePrev = NULL;
    HRESULT hresult = NOERROR;


    while (pqnodeCur) {
      if (pqnodeCur->m_pq == pq) {

        // remove
        if (pqnodePrev) {
          pqnodePrev->m_pqnodeNext = pqnodeCur->m_pqnodeNext;
        }
        else {
          m_pqnodeFirst = pqnodeCur->m_pqnodeNext;
        }
        delete pqnodeCur;
        break;
      }
      pqnodePrev = pqnodeCur;
      pqnodeCur = pqnodeCur->m_pqnodeNext;
    } // while

    LeaveCriticalSection(&g_csCallback);    // synchs falcon callback queue lookup
    return hresult;
}


CMSMQQueue *CMSMQQueue::PqOfHandle(QUEUEHANDLE lHandle)
{
    QueueNode *pqnodeCur = m_pqnodeFirst;
    CMSMQQueue *pq = NULL;

    while (pqnodeCur) {
      if (pqnodeCur->m_pq->m_lHandle == lHandle) {
        pq = pqnodeCur->m_pq;
        break;
      }
      pqnodeCur = pqnodeCur->m_pqnodeNext;
    } // while

    return pq;
}


// helpers
// helper: free body buffer
void FreeBodyBuffer(
    MQMSGPROPS* pmsgprops, 
    UINT iBody)
{
    HGLOBAL hMem = NULL;
    CAUB *pcau = &pmsgprops->aPropVar[iBody].caub;

    if (pcau->pElems) {
      hMem = GlobalHandle(pcau->pElems);
      ASSERT(hMem, L"bad handle.");
      GLOBALFREE(hMem);
      pcau->pElems = NULL;
    }

}

// helper: (re)allocate body buffer
HGLOBAL AllocateBodyBuffer(
    MQMSGPROPS* pmsgprops, 
    UINT iBody, 
    DWORD dwBodySize)
{
    HGLOBAL hMem = NULL;
    CAUB *pcau;

    // 
    // free current body if any
    //
    FreeBodyBuffer(pmsgprops, iBody);
    //
    // Allocating buffer
    //
    pmsgprops->aPropVar[iBody].caub.cElems = dwBodySize;
    pcau = &pmsgprops->aPropVar[iBody].caub;
    IfNullGo(hMem = GLOBALALLOC_MOVEABLE_NONDISCARD(pcau->cElems));
    pcau->pElems = (UCHAR *)GlobalLock(hMem);
    GLOBALUNLOCK(hMem);

    // fall through...

Error:
    return hMem;
}

//
// HELPER: get optional timeout param
//  defaults to INFINITE
//
HRESULT GetOptionalReceiveTimeout(
    VARIANT *pvarReceiveTimeout,
    long *plReceiveTimeout)
{
    long lReceiveTimeout = INFINITE;
    HRESULT hresult = NOERROR;

    if (pvarReceiveTimeout) {
      if (V_VT(pvarReceiveTimeout) != VT_ERROR) {
        IfFailRet(VariantChangeType(pvarReceiveTimeout, 
                                    pvarReceiveTimeout, 
                                    0, 
                                    VT_I4));
        lReceiveTimeout = V_I4(pvarReceiveTimeout);
      }
    }
    *plReceiveTimeout = lReceiveTimeout;

    return hresult;
}


// forwards decls...
void APIENTRY ReceiveCallback(
    HRESULT hrStatus,
    QUEUEHANDLE hReceiveQueue,
    DWORD dwTimeout,
    DWORD dwAction,
    MQMSGPROPS* pmsgprops,
    LPOVERLAPPED lpOverlapped,
    HANDLE hCursor);
void APIENTRY ReceiveCallbackCurrent(
    HRESULT hrStatus,
    QUEUEHANDLE hReceiveQueue,
    DWORD dwTimeout,
    DWORD dwAction,
    MQMSGPROPS* pmsgprops,
    LPOVERLAPPED lpOverlapped,
    HANDLE hCursor);
void APIENTRY ReceiveCallbackNext(
    HRESULT hrStatus,
    QUEUEHANDLE hReceiveQueue,
    DWORD dwTimeout,
    DWORD dwAction,
    MQMSGPROPS* pmsgprops,
    LPOVERLAPPED lpOverlapped,
    HANDLE hCursor);


//=--------------------------------------------------------------------------=
// CMSMQQueue::Create
//=--------------------------------------------------------------------------=
// creates a new MSMQQueue object.
//
// Parameters:
//    IUnknown *        - [in] controlling unkonwn
//
// Output:
//    IUnknown *        - new object.
//
// Notes:
//
IUnknown *CMSMQQueue::Create
(
    IUnknown *pUnkOuter
)
{
    // make sure we return the private unknown so that we support aggegation
    // correctly!
    //
    CMSMQQueue *pNew = new CMSMQQueue(pUnkOuter);
    return pNew ? pNew->PrivateUnknown() : NULL;
}

//=--------------------------------------------------------------------------=
// CMSMQQueue::CMSMQQueue
//=--------------------------------------------------------------------------=
// create the object and initialize the refcount
//
// Parameters:
//    IUnknown *    - [in] controlling unknown
//
// Notes:
//
#pragma warning(disable:4355)  // using 'this' in constructor
CMSMQQueue::CMSMQQueue
(
    IUnknown *pUnkOuter
)
: CAutomationObject(pUnkOuter, OBJECT_TYPE_OBJMQQUEUE, (void *)this)
{

    // TODO: initialize anything here
    m_lAccess = 0;                  
    m_lShareMode = MQ_DENY_NONE;               
    m_pqinfo = NULL;
    m_lHandle = INVALID_HANDLE_VALUE;
    m_hCursor = 0;
    m_pqevent = NULL;
    m_hasActiveEventHandler = FALSE;
}
#pragma warning(default:4355)  // using 'this' in constructor

//=--------------------------------------------------------------------------=
// CMSMQQueue::CMSMQQueue
//=--------------------------------------------------------------------------=
// "We all labour against our own cure, for death is the cure of all diseases"
//    - Sir Thomas Browne (1605 - 82)
//
// Notes:
//
CMSMQQueue::~CMSMQQueue ()
{
    // TODO: clean up anything here.
    HRESULT hresult;

    // remove from global instance list
    RemQueue(this);
    RELEASE(m_pqinfo);
    RELEASE(m_pqevent);
    hresult = Close();
    ASSERT(SUCCEEDED(hresult), L"whoops! can't handle errors here.");
}


//=--------------------------------------------------------------------------=
// CMSMQQueue::InternalQueryInterface
//=--------------------------------------------------------------------------=
// the controlling unknown will call this for us in the case where they're
// looking for a specific interface.
//
// Parameters:
//    REFIID        - [in]  interface they want
//    void **       - [out] where they want to put the resulting object ptr.
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//
HRESULT CMSMQQueue::InternalQueryInterface
(
    REFIID riid,
    void **ppvObjOut
)
{
    CHECK_POINTER(ppvObjOut);

    // we support IMSMQQueue and ISupportErrorInfo
    //
    if (DO_GUIDS_MATCH(riid, IID_IMSMQQueue)) {
        *ppvObjOut = (void *)(IMSMQQueue *)this;
        AddRef();
        return S_OK;
    } else if (DO_GUIDS_MATCH(riid, IID_ISupportErrorInfo)) {
        *ppvObjOut = (void *)(ISupportErrorInfo *)this;
        AddRef();
        return S_OK;
    }

    // call the super-class version and see if it can oblige.
    //
    return CAutomationObject::InternalQueryInterface(riid, ppvObjOut);
}



// TODO: implement your interface methods and property exchange functions
//       here.


//=--------------------------------------------------------------------------=
// CMSMQQueue::get_Access
//=--------------------------------------------------------------------------=
// Gets access
//
// Parameters:
//    plAccess [out] 
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueue::get_Access(long FAR* plAccess)
{
    *plAccess = m_lAccess;
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQQueue::get_ShareMode
//=--------------------------------------------------------------------------=
// Gets sharemode
//
// Parameters:
//    plShareMode [out] 
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueue::get_ShareMode(long FAR* plShareMode)
{
    *plShareMode = m_lShareMode;
    return NOERROR;
}

//=--------------------------------------------------------------------------=
// CMSMQQueue::get_queueinfo
//=--------------------------------------------------------------------------=
// Gets defining queueinfo
//
// Parameters:
//    ppqinfo [out] 
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueue::get_QueueInfo(IMSMQQueueInfo FAR* FAR* ppqinfo)
{
    ASSERT(m_pqinfo, L"null.");
    *ppqinfo = m_pqinfo;
    ADDREF(*ppqinfo);
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQQueue::get_Handle
//=--------------------------------------------------------------------------=
// Gets queue handle
//
// Parameters:
//    plHandle [out] 
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueue::get_Handle(long FAR* plHandle)
{
    // could be -1 if closed.
    *plHandle = (long)m_lHandle;
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQQueue::get_IsOpen
//=--------------------------------------------------------------------------=
// Tests if queue is open, i.e. has a valid handle
//
// Parameters:
//    pisOpen [out] 
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueue::get_IsOpen(VARIANT_BOOL FAR* pisOpen)
{
  *pisOpen = (m_lHandle != INVALID_HANDLE_VALUE);
  return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQQueue::Close
//=--------------------------------------------------------------------------=
// Closes queue if open.
//
// Parameters:
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueue::Close()
{
    HRESULT hresult = NOERROR;

    EnterCriticalSection(&g_csCallback);
    if (m_hCursor) {
      hresult = MQCloseCursor(m_hCursor);
      m_hCursor = 0;      
    }
    if (m_lHandle != INVALID_HANDLE_VALUE) {
      hresult = MQCloseQueue(m_lHandle);
      m_lHandle = INVALID_HANDLE_VALUE;
    }
    // REVIEW: should it be an error to attempt to close
    //  an already closed queue?
    //
    LeaveCriticalSection(&g_csCallback);


    return CreateErrorHelper(hresult, m_ObjectType);
}                   


//=--------------------------------------------------------------------------=
// CMSMQQueue::InternalReceive
//=--------------------------------------------------------------------------=
// Synchronously receive or peek next, matching msg.
//
// Parameters:
//  dwAction      [in]
//  hCursor       [in]
//  ptransaction  [in]
//  wantDestQueue [in]  if missing -> FALSE
//  wantBody      [in]  if missing -> TRUE
//  lReceiveTimeout [in]
//  ppmsg         [out] pointer to pointer to received message.  
//                      NULL if don't want msg.
//
// Output:
//  Returns NULL in *ppmsg if no received msg.
//
// Notes:
//  Synchronously receive a message.  
//  Execution is blocked until either a matching message arrives 
//   or ReceiveTimeout expires.
//
HRESULT CMSMQQueue::InternalReceive(
    DWORD dwAction, 
    HANDLE hCursor,
    VARIANT *pvarTransaction,
    VARIANT *wantDestQueue,
    VARIANT *wantBody,
    VARIANT *pvarReceiveTimeout,
    IMSMQMessage FAR* FAR* ppmsg)
{
    MQMSGPROPS msgprops;
    CMSMQMessage *pmsg = NULL;
    DWORD dwBodySize;
    UINT iBody = (DWORD)-1, iBodySize = (DWORD)-1;

#if 0  // no transactions on CE
    ITransaction *ptransaction = NULL;
    BOOL isRealXact = FALSE;
#endif // 0

    BOOL fWantDestQueue = FALSE;
    BOOL fWantBody = TRUE;
    long lReceiveTimeout = INFINITE;
#if DEBUG
    UINT iLoop = 0;
#endif // DEBUG
    HRESULT hresult = NOERROR;


    if (ppmsg == NULL) {
      return E_INVALIDARG;
    }
    *ppmsg = NULL;     
    //
    // process optional params
    //
    if (V_VT(wantDestQueue) != VT_ERROR ) {
      fWantDestQueue = GetBool(wantDestQueue);
    }
    if (V_VT(wantBody) != VT_ERROR ) {
      fWantBody = GetBool(wantBody);
    }
    IfFailRet(GetOptionalReceiveTimeout(
                pvarReceiveTimeout,
                &lReceiveTimeout));
    IfNullRet(pmsg = new CMSMQMessage(NULL));
    IfFailGo(pmsg->CreateReceiveMessageProps(
              fWantDestQueue,
              fWantBody,
              &msgprops));
    //
    // get optional transaction...
    //
#if 0 // no transactions on CE
    IfFailGo(GetOptionalTransaction(
               pvarTransaction,
               &ptransaction,
               &isRealXact));
#endif

    do {
#if DEBUG
      //
      // we should only ever perform this loop at most twice
      //  per-message
      //
      ASSERT(iLoop < 2, L"possible infinite recursion?");
#endif // DEBUG
      //
      // 1694: need to special-case retrying PeekNext 
      //  after a buffer overflow by using vanilla
      //  Peek on retries since otherwise Falcon will
      //  advance the peek cursor unnecessarily.
      //
      if (hresult == MQ_ERROR_BUFFER_OVERFLOW) {
        if (dwAction == MQ_ACTION_PEEK_NEXT) {
          dwAction = MQ_ACTION_PEEK_CURRENT;
        }  
      }
     
      
      hresult = MQReceiveMessage(m_lHandle, 
                                 lReceiveTimeout,
                                 dwAction,
                                 (MQMSGPROPS *)&msgprops,
                                 0,   // dwAppDefined
                                 0,   // fnRcvClbk
                                 hCursor,
                                 NULL);  

      
      if (hresult == MQ_ERROR_BUFFER_OVERFLOW) {
        // grow buffer...
        iBodySize = OrdinalOfPropId(&msgprops, PROPID_M_BODY_SIZE);
        iBody = OrdinalOfPropId(&msgprops, PROPID_M_BODY);
        ASSERT((iBodySize != -1) && (iBody != -1),
               L"msgprops lookup failed.");
        dwBodySize = msgprops.aPropVar[iBodySize].lVal;
        IfNullGo(AllocateBodyBuffer(&msgprops, iBody, dwBodySize));
      } 
#if DEBUG
      iLoop++;
#endif // DEBUG

    } while (hresult == MQ_ERROR_BUFFER_OVERFLOW);
    if (SUCCEEDED(hresult)) {
      // set message props
      IfFailGo(pmsg->SetMessageProps(&msgprops));
      *ppmsg = pmsg;
    }
    //
    // fall through...
    //

Error:
    if (FAILED(hresult)) {
      delete pmsg;
      ASSERT(*ppmsg == NULL, L"msg should be NULL.");
      if (hresult == MQ_ERROR_IO_TIMEOUT) {
        //
        // map time-out error to NULL msg return
        //
        hresult = NOERROR;
      }
      	//
      	// 2209: only free msg buf if body was requested
      	//
      if (fWantBody) {
        //
        // 2099: need to free message buffer since it has
        //  no owner.
        //
        if (iBody == -1) {
          iBody = OrdinalOfPropId(&msgprops, PROPID_M_BODY);
          ASSERT(iBody != -1, L"msgprops lookup failed.");
        }
        FreeBodyBuffer(&msgprops, iBody);
      }
    }
    CMSMQMessage::FreeMessageProps(&msgprops);
    
#if 0 // no transactions on CE
    if (isRealXact) {
      RELEASE(ptransaction);
    }
#endif

    return CreateErrorHelper(hresult, m_ObjectType);
}


//=--------------------------------------------------------------------------=
// CMSMQQueue::Receive
//=--------------------------------------------------------------------------=
// Synchronously receives a message.
//
// Parameters:
//  ptransaction  [in, optional]
//  ppmsg         [out] pointer to pointer to received message.
//
// Output:
//  Returns NULL in *ppmsg if no received msg.
//
// Notes:
//  Synchronously receive a message.  
//  Execution is blocked until either a matching message arrives 
//   or ReceiveTimeout expires.
//
HRESULT CMSMQQueue::Receive(
    VARIANT *ptransaction,
    VARIANT *wantDestQueue,
    VARIANT *wantBody,
    VARIANT *lReceiveTimeout,
    IMSMQMessage FAR* FAR* ppmsg)
{
    return InternalReceive(MQ_ACTION_RECEIVE, 
                           0, // no cursor
                           NULL,
                           wantDestQueue,
                           wantBody,
                           lReceiveTimeout,
                           ppmsg);
}


//=--------------------------------------------------------------------------=
// CMSMQQueue::Peek
//=--------------------------------------------------------------------------=
// Synchronously peeks at a message.
//
// Parameters:
//  ppmsg     [out] pointer to pointer to peeked message.
//
// Output:
//  Returns NULL in *ppmsg if no received msg.
//
// Notes:
//  Synchronously peek at a message.  
//  Execution is blocked until either a matching message arrives 
//   or ReceiveTimeout expires.
//
HRESULT CMSMQQueue::Peek(
    VARIANT *wantDestQueue,
    VARIANT *wantBody,
    VARIANT *lReceiveTimeout,
    IMSMQMessage FAR* FAR* ppmsg)
{
    return InternalReceive(MQ_ACTION_PEEK_CURRENT, 
                           0,             
                           NULL,          // no transaction
                           wantDestQueue,
                           wantBody,
                           lReceiveTimeout,
                           ppmsg);
}


//=--------------------------------------------------------------------------=
// CMSMQQueue::PeekCurrent
//=--------------------------------------------------------------------------=
// Synchronously peeks at a message with cursor.
//
// Parameters:
//  ppmsg     [out] pointer to pointer to peeked message.
//
// Output:
//  Returns NULL in *ppmsg if no received msg.
//
// Notes:
//  Synchronously peek at a message.  
//  Execution is blocked until either a matching message arrives 
//   or ReceiveTimeout expires.
//
HRESULT CMSMQQueue::PeekCurrent(
    VARIANT *wantDestQueue,
    VARIANT *wantBody,
    VARIANT *lReceiveTimeout,
    IMSMQMessage FAR* FAR* ppmsg)
{
    return InternalReceive(MQ_ACTION_PEEK_CURRENT, 
                           m_hCursor,     
                           NULL,          // no transaction
                           wantDestQueue,
                           wantBody,
                           lReceiveTimeout,
                           ppmsg);
}


//=--------------------------------------------------------------------------=
// CMSMQQueue::ReceiveCurrent
//=--------------------------------------------------------------------------=
// Synchronously receive next matching message.
//
// Parameters:
//  ppmsg     [out] pointer to pointer to received message.
//
// Output:
//  Returns NULL in *ppmsg if no received msg.
//
// Notes:
//  Synchronously receive a message.  
//  Execution is blocked until either a matching message arrives 
//   or ReceiveTimeout expires.
//
HRESULT CMSMQQueue::ReceiveCurrent(
    VARIANT *ptransaction,
    VARIANT *wantDestQueue,
    VARIANT *wantBody,
    VARIANT *lReceiveTimeout,
    IMSMQMessage FAR* FAR* ppmsg)
{
    return InternalReceive(MQ_ACTION_RECEIVE, 
                           m_hCursor,
                           ptransaction,
                           wantDestQueue,
                           wantBody,
                           lReceiveTimeout,
                           ppmsg);
}


//=--------------------------------------------------------------------------=
// CMSMQQueue::PeekNext
//=--------------------------------------------------------------------------=
// Synchronously peek at next matching message.
//
// Parameters:
//  ppmsg     [out] pointer to pointer to peeked message.
//
// Output:
//  Returns NULL in *ppmsg if no peekable msg.
//
// Notes:
//  Synchronously peek at a message.  
//  Execution is blocked until either a matching message arrives 
//   or ReceiveTimeout expires.
//
HRESULT CMSMQQueue::PeekNext(
    VARIANT *wantDestQueue,
    VARIANT *wantBody,
    VARIANT *lReceiveTimeout,
    IMSMQMessage FAR* FAR* ppmsg)
{
    return InternalReceive(MQ_ACTION_PEEK_NEXT, 
                           m_hCursor,
                           NULL,   // no transaction
                           wantDestQueue,
                           wantBody,
                           lReceiveTimeout,
                           ppmsg);
}


//=--------------------------------------------------------------------------=
// CMSMQQueue::EnableNotification
//=--------------------------------------------------------------------------=
// Enables async message arrival notification
//
// Parameters:
//  pqvent          [in]  queue's event handler
//  msgcursor       [in]  indicates whether they want
//                        to wait on first, current or next.
//                        Default: MQMSG_FIRST
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueue::EnableNotification(
    IMSMQEvent *pqevent,
    VARIANT *pvarMsgCursor,
    VARIANT *pvarReceiveTimeout)
{
    // Callback params -- ownership is transferred to
    //  callback.
    // UNDONE: what if callback isn't invoked? mem will leak.
    //  
    MQMSGPROPS *pmsgprops = NULL;
    MQMSGCURSOR msgcursor = MQMSG_FIRST;
    long lReceiveTimeout = INFINITE;
    static BOOL fWeirdLoadLibraryWorkaround = FALSE;
    HRESULT hresult;
    //
    // 1884: error if queue still has outstanding event
    //  handler. 
    // UNDONE: need specific error
    //
    if (m_hasActiveEventHandler) {
      return E_INVALIDARG;
    }
    if (pqevent == NULL) {
      return E_INVALIDARG;
    }
    if (pvarMsgCursor) {
      if (V_VT(pvarMsgCursor) != VT_ERROR) {
        IfFailRet(VariantChangeType(pvarMsgCursor, 
                                    pvarMsgCursor, 
                                    0, 
                                    VT_I4));
        msgcursor = (MQMSGCURSOR)(V_I4(pvarMsgCursor));
        if ((msgcursor != MQMSG_FIRST) &&
            (msgcursor != MQMSG_CURRENT) &&
            (msgcursor != MQMSG_NEXT)) {
          return E_INVALIDARG;
        }
      }
    }
    IfFailRet(GetOptionalReceiveTimeout(
                pvarReceiveTimeout,
                &lReceiveTimeout));
    IfNullRet(pmsgprops = new MQMSGPROPS); 
    InitMessageProps(pmsgprops);

	// sync to make sure the reference counts are accurate
    EnterCriticalSection(&m_CSlocal);
    	RELEASE(m_pqevent);
    	m_pqevent = pqevent;
    	ADDREF(m_pqevent);
	LeaveCriticalSection(&m_CSlocal);
	
	IfFailGo(CMSMQMessage::CreateAsyncReceiveMessageProps(pmsgprops));
	CMSMQMessage::FreeMessageProps(pmsgprops);
	IfFailGo(CMSMQMessage::CreateAsyncReceiveMessageProps(pmsgprops));
	
    // UNDONE: need to load mqoa.dll one extra time to workaround
    //  case in which Falcon rt calls to ActiveX
    //  ReceiveCallback after its dl, i.e. this one,
    //  has been unloaded.  This might happen if it wants
    //  to report that the callback has been canceled!
    // Note: this means that once you've made an async
    //  receive request then this dll will remain loaded.
    //
    if (!fWeirdLoadLibraryWorkaround) {
      LoadLibrary(L"mqoa.dll");
      fWeirdLoadLibraryWorkaround = TRUE;
    }
    //
    // register callback for the async notification.
    //
    // 2016: workaround VC5.0 codegen: operator ?: doesn't
    /// work correctly?
    //
    DWORD dwAction;
    PMQRECEIVECALLBACK fnReceiveCallback;
    HANDLE hCursor;
    
    switch (msgcursor) {
    case MQMSG_FIRST:
      dwAction = MQ_ACTION_PEEK_CURRENT;
      fnReceiveCallback = ReceiveCallback;
      hCursor = 0;
      break;
    case MQMSG_CURRENT:
      dwAction = MQ_ACTION_PEEK_CURRENT;
      fnReceiveCallback = ReceiveCallbackCurrent;
      hCursor = m_hCursor;
      break;
    case MQMSG_NEXT:
      dwAction = MQ_ACTION_PEEK_NEXT;
      fnReceiveCallback = ReceiveCallbackNext;
      hCursor = m_hCursor;
      break;
    default:
      ASSERT(0, L"bad msgcursor!");
      break;
    } // switch

    hresult = MQReceiveMessage(
                m_lHandle, 
                lReceiveTimeout,
                dwAction,
                pmsgprops,
                0,                            // overlapped
                fnReceiveCallback,
                hCursor,
                NULL               // no transaction
              );
              
    if (SUCCEEDED(hresult)) {
      //
      // 1884: queue now has active event handler,
      //  this is consumed by the callback.
      //
      m_hasActiveEventHandler = TRUE;
    }
    //
    // 1212: ignore BUFFER_OVERFLOW errors -- they will be 
    //  handled later by InternalReceive.
    //
    if (hresult == MQ_ERROR_BUFFER_OVERFLOW) {
      return NOERROR;
    }
Error:
    if (FAILED(hresult)) {
      CMSMQMessage::FreeMessageProps(pmsgprops);
    }
    return CreateErrorHelper(hresult, m_ObjectType);
}

//=--------------------------------------------------------------------------=
// CMSMQQueue::Reset
//=--------------------------------------------------------------------------=
// Resets message queue
//
// Parameters:
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueue::Reset()
{
    HRESULT hresult = NOERROR;

    if (m_hCursor) {
      hresult = MQCloseCursor(m_hCursor);
    }
    m_hCursor = 0;

    if ((m_lAccess & (MQ_RECEIVE_ACCESS | MQ_PEEK_ACCESS)) && SUCCEEDED(hresult)) {
      hresult = MQCreateCursor(m_lHandle, &m_hCursor);
    }

    return CreateErrorHelper(hresult, m_ObjectType);

}


//=--------------------------------------------------------------------------=
// CMSMQQueue::Init
//=--------------------------------------------------------------------------=
// Inits new instance with handle and creating MSMQQueueInfo instance.
//
// Parameters:
//    pqinfo       [in]  
//    lHandle     [in] 
//    lAccess     [in]
//    lShareMode  [in]
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//  Dtor must release.
//
HRESULT CMSMQQueue::Init(
    IMSMQQueueInfo *pqinfo, 
    QUEUEHANDLE lHandle,
    long lAccess,
    long lShareMode)
{
    BSTR bstrFormatName = NULL;
    HRESULT hresult = NOERROR;

	// Syncronization: This method is called only when this object is 
	// initialized by it's parent. There should not be more than one caller
	
    m_lHandle = lHandle;
    m_lAccess = lAccess;
    m_lShareMode = lShareMode;

    // Need to copy incoming pqinfo since we need
    //  to snapshot it otherwise it might change
    //  under our feet.
    // We do this by creating a new qinfo and initing
    //  it with the formatname of the incoming qinfo
    //  and then finally refreshing it.
    //
    // m_pqinfo released in dtor
    //
    IfNullRet(m_pqinfo = new CMSMQQueueInfo(NULL));     
    IfFailGo(pqinfo->get_FormatName(&bstrFormatName));
    
    //
    // ownership is not transferred -- the format name is copied.
    // note: cast is safe cos we just created the object
    //  and we know its true type.
    //
    IfFailGoTo(((CMSMQQueueInfo *)m_pqinfo)->Init(bstrFormatName), Error2); 
    SysFreeString(bstrFormatName);      // UNDONE: needs to be freed in a single location
    
    //
    // 2536: only attempt to use the DS when
    //  the first prop is accessed... or Refresh
    //  is called explicitly.
    //
#if 0
    // Might fail if no DS or if DIRECT so we ignore
    //  the error...
    //
    hresult = m_pqinfo->Refresh();
#endif // 0

    IfFailGo(AddQueue(this));              // add to global list
    return Reset();

Error2:
    SysFreeString(bstrFormatName);
    // fall through...

Error:
    RELEASE(m_pqinfo);
    return hresult;
}


