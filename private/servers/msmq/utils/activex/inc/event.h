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
