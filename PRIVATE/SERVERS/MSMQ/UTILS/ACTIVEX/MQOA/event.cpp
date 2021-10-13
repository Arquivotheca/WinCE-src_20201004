//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// event.Cpp
//=--------------------------------------------------------------------------=
//
// the MSMQEvent object
//
//
#include "IPServer.H"

#include "LocalObj.H"
#include "dispids.h"

#include "globals.h"    // for framework globals
#include "oautil.h"
#include "event.h"
#include "msg.h"
#include "q.h"

#if defined (SDK_BUILD)
#if ! defined (GetModuleHandle)
#define GetModuleHandle(c) g_hInstance
#endif
#endif

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

#define CREATEWINDOW

// Used to coordinate user-thread queue ops and 
//  queue lookup in falcon-thread callback 
//
CRITICAL_SECTION g_csCallback;

// per-class hwnd
HWND CMSMQEvent::m_hwnd = NULL;
WNDPROC CMSMQEvent::m_lpPrevWndFunc = NULL;
UINT CMSMQEvent::m_cInstance = 0;
WNDCLASS CMSMQEvent::m_wndclass;
HANDLE g_hEvent = INVALID_HANDLE_VALUE;

// MSMQEvent event stuff...
typedef enum {
    MSMQEventEvent_Arrived = 0,
    MSMQEventEvent_ArrivedError = 1,
} MSMQEventEVENTS;

VARTYPE rgI4[] = {VT_I4};
VARTYPE rgI4I4[] = {VT_I4, VT_I4};
VARTYPE rgDISPATCHI4[] = {VT_DISPATCH, VT_I4};
VARTYPE rgDISPATCHI4I4[] = {VT_DISPATCH, VT_I4, VT_I4};

EVENTINFO g_rgMSMQEventEvents[] = {
    {DISPID_MQEVENTEVENTS_ARRIVED, 2, rgDISPATCHI4},
    {DISPID_MQEVENTEVENTS_ARRIVEDERROR, 3, rgDISPATCHI4I4}
};



//=--------------------------------------------------------------------------=
// CMSMQEvent::Create
//=--------------------------------------------------------------------------=
// creates a new MSMQMessage object.
//
// Parameters:
//    IUnknown *        - [in] controlling unkonwn
//
// Output:
//    IUnknown *        - new object.
//
// Notes:
//
IUnknown *CMSMQEvent::Create
(
    IUnknown *pUnkOuter
)
{
    // make sure we return the private unknown so that we support aggegation
    // correctly!
    //
    CMSMQEvent *pNew = new CMSMQEvent(pUnkOuter);
    return pNew ? pNew->PrivateUnknown() : NULL;
}

//=--------------------------------------------------------------------------=
// CMSMQEvent::CMSMQEvent
//=--------------------------------------------------------------------------=
// create the object and initialize the refcount
//
// Parameters:
//    IUnknown *    - [in] controlling unknown
//
// Notes:
//
#pragma warning(disable:4355)  // using 'this' in constructor
CMSMQEvent::CMSMQEvent
(
    IUnknown *pUnkOuter
)
: CAutomationObjectWEvents(pUnkOuter, OBJECT_TYPE_OBJMQEVENT, (void *)this)
{

    // TODO: initialize anything here
    HWND hwnd;

    // Register our "sub-classed" windowproc so that we can
    //  send a message from the Falcon thread to the user's VB
    //  thread in order to fire async events in the correct
    //  context.
    //
    if (m_cInstance++ == 0) {
#ifdef CREATEWINDOW
      hwnd = CreateHiddenWindow();
#else
      hwnd = GetActiveWindow();
#endif // CREATEWINDOW
      ASSERT(IsWindow(hwnd), L"should have a valid window.");
#ifndef CREATEWINDOW

      m_lpPrevWndFunc = (WNDPROC)SetWindowLong(
                                   NULL, // hwnd,
                                   GWL_WNDPROC,
                                   (LONG)CMSMQEvent_WindowProc
                                   );
#endif // CREATEWINDOW
      m_hwnd = hwnd;
      //
      // Event for inter-thread synchronization
      //
      g_hEvent = CreateEvent(NULL,
                              FALSE,  // manual reset
                              FALSE,  // not signalled
                              NULL);
      ASSERT(g_hEvent != NULL, L"CreateEvent failed!");
    } // if m_hwnd == NULL
}
#pragma warning(default:4355)  // using 'this' in constructor

//=--------------------------------------------------------------------------=
// CMSMQEvent::CMSMQEvent
//=--------------------------------------------------------------------------=
// "We all labour against our own cure, for death is the cure of all diseases"
//    - Sir Thomas Browne (1605 - 82)
//
// Notes:
//
CMSMQEvent::~CMSMQEvent ()
{
    // TODO: clean up anything here.

#ifndef CREATEWINDOW
    // Un-subclass if the sub-classed window is still valid.
    if (m_hwnd && m_lpPrevWndFunc) {
      if (IsWindow(m_hwnd)) {
        SetWindowLong(NULL, // m_hwnd,
                      GWL_WNDPROC,
                      (LONG)m_lpPrevWndFunc);
      }
    }
    m_hwnd = NULL;
    m_lpPrevWndFunc = NULL;
#else
    if (--m_cInstance == 0) {
      DestroyHiddenWindow();
      if (g_hEvent != INVALID_HANDLE_VALUE) {
        SetEvent(g_hEvent);
        CloseHandle(g_hEvent);
        g_hEvent = INVALID_HANDLE_VALUE;
      }
    }
#endif // CREATEWINDOW
}

//=--------------------------------------------------------------------------=
// CMSMQEvent::InternalQueryInterface
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
HRESULT CMSMQEvent::InternalQueryInterface
(
    REFIID riid,
    void **ppvObjOut
)
{
    CHECK_POINTER(ppvObjOut);

    // we support IMSMQEvent and ISupportErrorInfo and now IMSMQPrivateEvent
    //
    if (DO_GUIDS_MATCH(riid, IID_IMSMQEvent)) {
        *ppvObjOut = (void *)(IMSMQEvent *)this;
        AddRef();
        return S_OK;
    } else if (DO_GUIDS_MATCH(riid, IID_ISupportErrorInfo)) {
        *ppvObjOut = (void *)(ISupportErrorInfo *)this;
        AddRef();
        return S_OK;
    } else if (DO_GUIDS_MATCH(riid, IID_IMSMQPrivateEvent)) {
        *ppvObjOut = (void *)(IMSMQPrivateEvent *)this;
        AddRef();
        return S_OK;
    }

    // call the super-class version and see if it can oblige.
    //
    return CAutomationObjectWEvents::InternalQueryInterface(riid, ppvObjOut);
}


    //
    // IMSMQPrivateEvent methods
    //

//=--------------------------------------------------------------------------=
// CMSMQEvent::FireArrivedEvent
//=--------------------------------------------------------------------------=
//
// Parameters:
//
// Output:
//
// Notes:
HRESULT CMSMQEvent::FireArrivedEvent(
    IMSMQQueue __RPC_FAR *pq,
    long msgcursor)
{

    FireEvent(
      &g_rgMSMQEventEvents[MSMQEventEvent_Arrived],
      (long)pq,
      msgcursor);
    return NOERROR;
}



//=--------------------------------------------------------------------------=
// CMSMQEvent::FireArrivedErrorEvent
//=--------------------------------------------------------------------------=
//
// Parameters:
//
// Output:
//
// Notes:
HRESULT CMSMQEvent::FireArrivedErrorEvent(
    IMSMQQueue __RPC_FAR *pq,
    HRESULT hrStatus,
    long msgcursor)
{

    FireEvent(
      &g_rgMSMQEventEvents[MSMQEventEvent_ArrivedError],
      (long)pq,
      (long)hrStatus,
      (long)msgcursor);
    return NOERROR;
}

extern WNDPROC g_lpPrevWndFunc;

//=--------------------------------------------------------------------------=
// InternalReceiveCallback
//=--------------------------------------------------------------------------=
// Async callback handler.  Runs in Falcon created thread.
// We send message to user thread so that event is fired
//  in correct execution context.
//
// Parameters:
//	hrStatus,
//	hReceiveQueue,
//    	dwTimeout,
//    	dwAction,
//    	pMessageProps,
//    	lpOverlapped,
//    	hCursor
//      MQMSG_CURSOR
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//
void APIENTRY InternalReceiveCallback(
    HRESULT hrStatus,
    QUEUEHANDLE hReceiveQueue,
    DWORD dwTimeout,
    DWORD dwAction,
    MQMSGPROPS* pmsgprops,
    LPOVERLAPPED lpOverlapped,
    HANDLE hCursor,
    MQMSGCURSOR msgcursor)
{
    // FireEvent...
    // Map handle to associated queue object
    //
    CMSMQQueue *pq;
    HWND hwnd;
    VARIANT_BOOL isOpen;
    LRESULT lr;
    BOOL fInCritSect = FALSE;
    IMSMQEvent *pqevent;
    IMSMQPrivateEvent *pprivateevent = NULL;
    HRESULT hresult;

    //
    // UNDONE: 905: decrement dll refcount that we incremented
    //  when we registered the callback.
    //
    EnterCriticalSection(&g_csCallback);      // syncs other queue ops
    fInCritSect = TRUE;
    pq = CMSMQQueue::PqOfHandle(hReceiveQueue);
    
    // if no queue, then ignore the callback otherwise
    //  if the queue is open and it has a window
    //  then send message to user-thread that will
    //  trigger event firing.
    //
    if (pq) {
      hresult = pq->get_IsOpen(&isOpen);
      ASSERT(hresult == NOERROR, L"IsOpen should never fail.");
      if (isOpen) {
        pqevent = pq->Pqevent();
        if (pqevent) {
          //
          // don't get the private interface here since MTS might cache it
          //  and then we'll have ITC problems
          //
          hwnd = CMSMQEvent::Hwnd();
          if (IsWindow(hwnd)) {
            //        
            // Need to exit critsect now so that
            //  we don't deadlock with user closing queue
            //  in user thread.
            //
            LeaveCriticalSection(&g_csCallback);
            fInCritSect = FALSE;
            //
            // 1212: In principle, need to special case BUFFER_OVERFLOW
            //  by growing buffer and synchronously peeking...
            //  but since it's the user's responsibility to 
            //  receive the actual message, we'll just let our
            //  usual InternalReceive handling deal with the
            //  overflow...
            //
            //
            // 1900: pass msgcursor to event handler 
            //
            WindowsMessage *pwinmsg = new WindowsMessage;
			if (! pwinmsg) {
				RETAILMSG(1, (L"MQOA: OUT OF MEMORY - DATA WILL BE LOST!\r\n"));
				goto cleanup;
			}

            pwinmsg->m_msgcursor = msgcursor;
            if ((hrStatus == MQ_ERROR_BUFFER_OVERFLOW) || 
                  SUCCEEDED(hrStatus)) {
              //
              // Since we are in a Falcon created callback thread,
              //  send a message to the user thread to trigger
              //  event firing...
              // UNDONE: need to register unique Windows message.
              // Need to use PostMessage instead of
              //  SendMessage otherwise get RPC_E_CANCALLOUT_ININPUTSYNCCALL
              //  when attempting to call to exe (only?) server like
              //  Excel in user-defined event handler.
              // However, we would like event firing to be synchronous
              //  for ease of understanding and control so we
              //  simulate SendMessage by waiting for the event
              //  handler to signal it has completed handling.
              //  
#if 0 // Undone for now

               DWORD dwWait;
              ASSERT(g_hEvent != INVALID_HANDLE_VALUE,
                     L"should have valid event handle.");
              pwinmsg->m_hEvent = g_hEvent;
              
              lr = PostMessage(hwnd, 
                                WM_MQRECEIVE, 
                                (WPARAM)hReceiveQueue, 
                                (LPARAM)pwinmsg);
              dwWait = WaitForSingleObject(g_hEvent, INFINITE);
              ASSERT(dwWait != WAIT_FAILED, L"wait failed.");
#endif // 0
              lr = SendMessage(hwnd,WM_MQRECEIVE, 
                                (WPARAM)hReceiveQueue, 
                                (LPARAM)pwinmsg);
            }
            else {
              pwinmsg->m_hrStatus = hrStatus;
#if 0
              lr = PostMessage(hwnd, 
                                WM_MQTHROWERROR, 
                                (WPARAM)hReceiveQueue, 
                                (LPARAM)pwinmsg);
#else // 0
			  lr = SendMessage(hwnd, 
                                WM_MQTHROWERROR, 
                                (WPARAM)hReceiveQueue, 
                                (LPARAM)pwinmsg);
#endif // 1
            }
          } // if IsWindow
        } // if pqevent
      } // if isOpen
    } // pq

cleanup:
    // still in critsect?
    if (fInCritSect) {
      LeaveCriticalSection(&g_csCallback);
    }

    // Free params whose ownership we've acquired...
    CMSMQMessage::FreeMessageProps(pmsgprops);
  	RELEASE(pprivateevent);

  	// translation to local ptr
    delete pmsgprops;
    return;
}


//=--------------------------------------------------------------------------=
// ReceiveCallback, ReceiveCallbackCurrent, ReceiveCallbackNext
//=--------------------------------------------------------------------------=
// Async callback handler.  Runs in Falcon created thread.
// We send message to user thread so that event is fired
//  in correct execution context.
// NOTE: no cursor
//
// Parameters:
//	hrStatus,
//	hReceiveQueue,
//    	dwTimeout,
//    	dwAction,
//    	pMessageProps,
//    	lpOverlapped,
//    	hCursor
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//
void APIENTRY ReceiveCallback(
    HRESULT hrStatus,
    QUEUEHANDLE hReceiveQueue,
    DWORD dwTimeout,
    DWORD dwAction,
    MQMSGPROPS* pmsgprops,
    LPOVERLAPPED lpOverlapped,
    HANDLE hCursor)
{
    InternalReceiveCallback(
      hrStatus,
      hReceiveQueue,
      dwTimeout,
      dwAction,
      pmsgprops,
      lpOverlapped,
      0,               // no cursor
      MQMSG_FIRST
    );
}


void APIENTRY ReceiveCallbackCurrent(
    HRESULT hrStatus,
    QUEUEHANDLE hReceiveQueue,
    DWORD dwTimeout,
    DWORD dwAction,
    MQMSGPROPS* pmsgprops,
    LPOVERLAPPED lpOverlapped,
    HANDLE hCursor)
{
    InternalReceiveCallback(
      hrStatus,
      hReceiveQueue,
      dwTimeout,
      dwAction,
      pmsgprops,
      lpOverlapped,
      hCursor,
      MQMSG_CURRENT
    );
}


void APIENTRY ReceiveCallbackNext(
    HRESULT hrStatus,
    QUEUEHANDLE hReceiveQueue,
    DWORD dwTimeout,
    DWORD dwAction,
    MQMSGPROPS* pmsgprops,
    LPOVERLAPPED lpOverlapped,
    HANDLE hCursor)
{
    InternalReceiveCallback(
      hrStatus,
      hReceiveQueue,
      dwTimeout,
      dwAction,
      pmsgprops,
      lpOverlapped,
      hCursor,
      MQMSG_NEXT
    );
}


//=--------------------------------------------------------------------------=
// global CMSMQEvent_WindowProc
//=--------------------------------------------------------------------------=
// "derived" windowproc so that we can process our async event
//   msg.  This is a nop if notification has been disabled.
//
// Parameters:
//  hwnd
//  msg         we can handle WM_MQRECEIVE/WM_MQTHROWERROR
//  wParam      QUEUEHANDLE: hReceiveQueue
//  lParam      [lErrorCode]:
//              lErrorCode: if WM_MQTHROWERROR
//
// Output:
//    LRESULT
//
// Notes:
//
LRESULT APIENTRY CMSMQEvent_WindowProc(
    HWND hwnd, 
    UINT msg, 
    WPARAM wParam, 
    LPARAM lParam)
{
    CMSMQQueue *pq = NULL;
    VARIANT_BOOL isOpen;
    // CMSMQEvent *pqevent = NULL;
    IMSMQEvent *pqevent = NULL;
    IMSMQPrivateEvent *pprivateevent = NULL;
    WindowsMessage *pwinmsg = (WindowsMessage *)lParam;
    HRESULT hresult;

    switch (msg) {
    case WM_MQRECEIVE:
    case WM_MQTHROWERROR:

      //
      // Need to revalidate incoming hReceiveQueue by
      //  lookup up (again) in queue list -- it might
      //  have been deleted already since we previously
      //  looked it up in another thread (falcon-created
      //  callback thread).
      // Since we are now in the user-thread we don't
      //  need to critsect this lookup unlike the
      //  lookup in the callback thread.
      //
      pq = CMSMQQueue::PqOfHandle((QUEUEHANDLE)wParam);
      if (pq) { 
        hresult = pq->get_IsOpen(&isOpen);
        ASSERT(hresult == NOERROR, L"IsOpen shouldn't fail.");
        if (isOpen) {

          // does queue have an event handler?
          // note: cast is safe because we populated
          //  queue object with our implementation of IMSMQEvent.
          //
          // pqevent = (CMSMQEvent *)pq->Pqevent();
          pqevent = pq->Pqevent();
          if (pqevent) {
            MQMSGCURSOR msgcursor;
            msgcursor = pwinmsg->m_msgcursor;
            //
            // 1884: allow event handler to reenable notifications
            //
            pq->SetHasActiveEventHandler(FALSE);
            //
            // QI to the private interface
            //
            hresult = pqevent->QueryInterface(
                        IID_IMSMQPrivateEvent, 
                        (LPVOID *)&pprivateevent);
            ASSERT(hresult == NOERROR, L"QI to IMSMQPrivateEvent shouldn't fail!");
            if (msg == WM_MQRECEIVE) {
#if 1
              pprivateevent->FireArrivedEvent(
                                 pq,
                                 (long)msgcursor);
#else
              pqevent->FireEvent(&g_rgMSMQEventEvents[MSMQEventEvent_Arrived],
                                 (long)pq,
                                 (long)msgcursor);
#endif // 0
            }
            else {
              // WM_MQTHROWERROR
              HRESULT hrStatus;
              hrStatus = pwinmsg->m_hrStatus; 
#if 1
              pprivateevent->FireArrivedErrorEvent(
                                 pq,
                                 (long)hrStatus,
                                 (long)msgcursor);
#else
              pqevent->FireEvent(
                &g_rgMSMQEventEvents[MSMQEventEvent_ArrivedError],
                (long)pq,
                (long)hrStatus,
                (long)msgcursor);
#endif // 0
            }
            RELEASE(pprivateevent);
          } // pqevent
        } // isOpen
      } // pq
      if (msg == WM_MQRECEIVE) {
        //
        // signal completion event if the event is still valid
        //
        if (g_hEvent != INVALID_HANDLE_VALUE) {
          BOOL fSucceeded;
          fSucceeded = SetEvent((HANDLE)g_hEvent);
          ASSERT(fSucceeded, L"SetEvent failed!");
        }
      }
      delete pwinmsg;
      break;
    default:
#ifdef CREATEWINDOW
      return DefWindowProc(hwnd, msg, wParam, lParam);	
#else
      return CallWindowProc(
              CMSMQEvent::LpPrevWndFunc(),
              NULL, // hwnd,
              msg,
              wParam,
              lParam);	
#endif // CREATEWINDOW
    } // switch
    return 0;
}


//=--------------------------------------------------------------------------=
// CMSMQEvent::CreateHiddenWindow
//=--------------------------------------------------------------------------=
// creates a per-class hidden window that is used for inter-thread
//  messaging from the Falcon async thread and the user thread.
//
// Parameters:
//
// Output:
//
// Notes:
//
HWND CMSMQEvent::CreateHiddenWindow()
{
    HWND hwnd;
    ATOM atomWndClass;
    LPCTSTR lpClassName;

    ASSERT(m_hwnd == 0, L"shouldn't be inited yet.");
    memset(&m_wndclass, 0, sizeof(WNDCLASSA));
    m_wndclass.lpfnWndProc = CMSMQEvent_WindowProc;
    m_wndclass.lpszClassName = L"MSMQEvent";
    
    // can use ANSI version
    m_wndclass.hInstance = GetModuleHandle(NULL);
    m_wndclass.style = WS_DISABLED;
    atomWndClass = RegisterClass(&m_wndclass); // use ANSI version
    ASSERT(atomWndClass != 0, L"RegisterClass shouldn't fail.");
    lpClassName = (LPCTSTR)atomWndClass;

    // can use ANSI version
    hwnd  = CreateWindow(
              // (LPCSTR)m_wndclass.lpszClassName,
              lpClassName,
              L"EventWindow",
              m_wndclass.style,
              CW_USEDEFAULT,
              CW_USEDEFAULT,
              CW_USEDEFAULT,
              CW_USEDEFAULT,
              NULL,	// handle to parent or owner window
              NULL,	// handle to menu or child-window identifier
              m_wndclass.hInstance,
              NULL      // pointer to window-creation data
            );	
#if DEBUG
    DWORD dwErr;
    if (hwnd == 0) {
      dwErr = GetLastError();
      ASSERT(dwErr == 0, L"CreateWindow returned an error.");
    }
#endif // DEBUG
    return hwnd;
}


//=--------------------------------------------------------------------------=
// CMSMQEvent::DestroyHiddenWindow
//=--------------------------------------------------------------------------=
// destroys per-class hidden window that is used for inter-thread
//  messaging from the Falcon async thread and the user thread.
//
// Parameters:
//
// Output:
//
// Notes:
//
void CMSMQEvent::DestroyHiddenWindow()
{
    ASSERT(m_hwnd != 0, L"should have a window handle.");
    if (IsWindow(m_hwnd)) {
      BOOL fDestroyed, fUnregistered;
      fDestroyed = DestroyWindow(m_hwnd);
#if DEBUG
      DWORD dwErr;
      if (fDestroyed == FALSE) {
        dwErr = GetLastError();
        ASSERT(dwErr == 0, L"hmm... couldn't destroy window.");
      }
#endif // DEBUG
      // unregister our window class now that we've destroyed
      //  the window
      //
      fUnregistered = UnregisterClass(
                        m_wndclass.lpszClassName,
                        m_wndclass.hInstance);
#if DEBUG
      if (fUnregistered == FALSE) {
        dwErr = GetLastError();
        ASSERT(dwErr == 0, L"hmm... couldn't unregister window class.");
      }
#endif // DEBUG
    }
    m_hwnd = NULL;
}
