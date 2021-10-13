//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// MSMQQueueObj.H
//=--------------------------------------------------------------------------=
//
// the MSMQQueue object.
//
//
#ifndef _MSMQQueue_H_

#include "AutoObj.H"
#include "lookupx.h"    // UNDONE; to define admin stuff...
#include "mqoa.H"
#include "mq.h"

#include "oautil.h"

// forwards
struct IMSMQQueueInfo;
class CMSMQQueue;
struct IMSMQEvent;

// helper struct for queue list
struct QueueNode
{
    CMSMQQueue *m_pq;
    QueueNode *m_pqnodeNext;
    QueueNode() { m_pq = NULL; m_pqnodeNext = NULL; }
};


// helper struct for callback param list
struct CallbackNode
{
    MQMSGPROPS m_msgprops;
    BSTR m_bstrResponseQueue;
    ULONG m_uiFormatNameLen;
    CallbackNode *m_pcallbacknodeNext;

    CallbackNode() { 
      m_bstrResponseQueue = NULL;
      m_uiFormatNameLen = FORMAT_NAME_INIT_BUFFER;
      m_pcallbacknodeNext = NULL;
      // UNDONE: free m_msgprops
    }

    ~CallbackNode() {
      SysFreeString(m_bstrResponseQueue);
    }
};


//
// helper struct for inter-thread communication:
//  used by PostMessage to pass info from
//  callback to user-defined event handler
//
struct WindowsMessage
{
    union {
      HANDLE m_hEvent;
      HRESULT m_hrStatus;
    };
    MQMSGCURSOR m_msgcursor;
    WindowsMessage() {
      m_hEvent = INVALID_HANDLE_VALUE;
      m_msgcursor = MQMSG_FIRST;
    }
};


LRESULT APIENTRY CMSMQQueue_WindowProc(
                     HWND hwnd, 
                     UINT msg, 
                     WPARAM wParam, 
                     LPARAM lParam);

class CMSMQQueue : public IMSMQQueue, public CAutomationObject, ISupportErrorInfo {

  public:
    // IUnknown methods
    //
    DECLARE_STANDARD_UNKNOWN();

    // IDispatch methods
    //
    DECLARE_STANDARD_DISPATCH();

    //  ISupportErrorInfo methods
    //
    DECLARE_STANDARD_SUPPORTERRORINFO();

    CMSMQQueue(IUnknown *);
    virtual ~CMSMQQueue();

    // IMSMQQueue methods
    // TODO: copy over the interface methods for IMSMQQueue from
    //       mqInterfaces.H here.
    /* IMSMQQueue methods */
    STDMETHOD(get_Access)(THIS_ long FAR* plAccess);
    STDMETHOD(get_ShareMode)(THIS_ long FAR* plShareMode);
    STDMETHOD(get_QueueInfo)(THIS_ IMSMQQueueInfo FAR* FAR* ppqinfo);
    STDMETHOD(get_Handle)(THIS_ long FAR* plHandle);
    STDMETHOD(get_IsOpen)(THIS_ VARIANT_BOOL FAR* pisOpen);
    STDMETHOD(Close)(THIS);
    STDMETHOD(Receive)(THIS_ VARIANT FAR* ptransaction, VARIANT FAR* wantDestQueue, VARIANT FAR* wantBody, VARIANT FAR* lReceiveTimeout, IMSMQMessage FAR* FAR* ppmsg);
    STDMETHOD(Peek)(THIS_ VARIANT FAR* wantDestQueue, VARIANT FAR* wantBody, VARIANT FAR* lReceiveTimeout, IMSMQMessage FAR* FAR* ppmsg);
    STDMETHOD(EnableNotification)(THIS_ IMSMQEvent FAR* pqevent, VARIANT FAR* msgcursor, VARIANT FAR* lReceiveTimeout);
    STDMETHOD(Reset)(THIS);
    STDMETHOD(ReceiveCurrent)(THIS_ VARIANT FAR* ptransaction, VARIANT FAR* wantDestQueue, VARIANT FAR* wantBody, VARIANT FAR* lReceiveTimeout, IMSMQMessage FAR* FAR* ppmsg);
    STDMETHOD(PeekNext)(THIS_ VARIANT FAR* wantDestQueue, VARIANT FAR* wantBody, VARIANT FAR* lReceiveTimeout, IMSMQMessage FAR* FAR* ppmsg);
    STDMETHOD(PeekCurrent)(THIS_ VARIANT FAR* wantDestQueue, VARIANT FAR* wantBody, VARIANT FAR* lReceiveTimeout, IMSMQMessage FAR* FAR* ppmsg);

    // creation method
    //
    static IUnknown *Create(IUnknown *);

    // introduced methods
    HRESULT Init(
      IMSMQQueueInfo *pqinfo, 
      QUEUEHANDLE lHandle,
      long lAccess,
      long lShareMode);
    HRESULT InternalReceive(
      DWORD dwAction, 
      HANDLE hCursor,
      VARIANT *pvarTransaction,
      VARIANT *wantDestQueue,
      VARIANT *wantBody,
      VARIANT *pvarReceiveTimeout,
      IMSMQMessage FAR* FAR* ppmsg);
    IMSMQEvent *Pqevent() const { return m_pqevent; }
    BOOL HasActiveEventHandler() const { 
      return m_hasActiveEventHandler; 
    }
    void SetHasActiveEventHandler(BOOL hasActiveEventHandler) {
      m_hasActiveEventHandler = hasActiveEventHandler; 
    }

  protected:
    virtual HRESULT InternalQueryInterface(REFIID riid, void **ppvObjOut);

  private:
    // member variables that nobody else gets to look at.
    // TODO: add your member variables and private functions here.
    long m_lReceiveTimeout;
    long m_lAccess;
    long m_lShareMode;
    QUEUEHANDLE m_lHandle;
    IMSMQQueueInfo *m_pqinfo;

    // current cursor position in queue
    HANDLE m_hCursor;

    // queue's event handler
    IMSMQEvent *m_pqevent;
    
    //
    // 1884: indicates whether queue has outstanding
    //  event handler
    //
    BOOL m_hasActiveEventHandler;

    // global instance list
    static QueueNode *m_pqnodeFirst;

    // global callback param list
    static CallbackNode *m_pcallbacknodeFirst;

    // static methods to manipulate instance list
private:
    static HRESULT AddQueue(CMSMQQueue *pq);
    static HRESULT RemQueue(CMSMQQueue *pq);

public:
    static CMSMQQueue *PqOfHandle(
                     QUEUEHANDLE lHandle);
};

// TODO: modify anything appropriate in this structure, such as the helpfile
//       name, the version number, etc.
//
DEFINE_AUTOMATIONOBJECT(MSMQQueue,
    &CLSID_MSMQQueue,
    L"MSMQQueue",
    CMSMQQueue::Create,
    1,
    &IID_IMSMQQueue,
    L"MSMQQueue.Hlp");

#define _MSMQQueue_H_
#endif // _MSMQQueue_H_
