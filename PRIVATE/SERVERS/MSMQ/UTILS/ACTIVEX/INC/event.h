//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// event.H
//=--------------------------------------------------------------------------=
//
// the MSMQEvent object.
//
//
#ifndef _MSMQEvent_H_

#include "AutoObj.H"
#include "lookupx.h"    // UNDONE; to define admin stuff...
#include "mqoa.H"
#include "mq.h"

#include "oautil.h"

// forwards
class CMSMQEvent;

LRESULT APIENTRY CMSMQEvent_WindowProc(
                     HWND hwnd, 
                     UINT msg, 
                     WPARAM wParam, 
                     LPARAM lParam);

class CMSMQEvent : 
     public IMSMQEvent, 
     public IMSMQPrivateEvent,
     public CAutomationObjectWEvents, ISupportErrorInfo {

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

    CMSMQEvent(IUnknown *);
    virtual ~CMSMQEvent();

    // IMSMQEvent methods
    // TODO: copy over the interface methods for IMSMQEvent from
    //       mqInterfaces.H here.

     // IMSMQPrivateEvent methods
     virtual HRESULT STDMETHODCALLTYPE FireArrivedEvent( 
         IMSMQQueue __RPC_FAR *pq,
         long msgcursor);
     
     virtual HRESULT STDMETHODCALLTYPE FireArrivedErrorEvent( 
         IMSMQQueue __RPC_FAR *pq,
         HRESULT hrStatus,
         long msgcursor);

    // creation method
    //
    static IUnknown *Create(IUnknown *);

    // introduced methods
    
  protected:
    virtual HRESULT InternalQueryInterface(REFIID riid, void **ppvObjOut);

  private:
    // member variables that nobody else gets to look at.
    // TODO: add your member variables and private functions here.

    // per-class hwnd
    static HWND m_hwnd;
    static WNDPROC m_lpPrevWndFunc;
    static UINT m_cInstance;      // instance count
    // used by register class/create window
    static WNDCLASS m_wndclass;   

public:
    static WNDPROC LpPrevWndFunc() { 
      return m_lpPrevWndFunc;
    }
    static HWND Hwnd() { 
      return m_hwnd;
    }
    static HWND CreateHiddenWindow();
    static void DestroyHiddenWindow();
};

// TODO: modify anything appropriate in this structure, such as the helpfile
//       name, the version number, etc.
//
DEFINE_AUTOMATIONOBJECTWEVENTS(MSMQEvent,
    &CLSID_MSMQEvent,
    L"MSMQEvent",
    CMSMQEvent::Create,
    1,
    &IID_IMSMQEvent,
    &DIID__DMSMQEventEvents,
    L"MSMQEvent.Hlp");


#define _MSMQEvent_H_
#endif // _MSMQEvent_H_
