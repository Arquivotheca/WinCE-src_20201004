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

#include "PerfListTests.h"
#include "EventHandler.h"


////////////////////////////////////////////////////////////////////////////////
//
//  EventHandler
//
//   Tracks event callbacks
//
////////////////////////////////////////////////////////////////////////////////

// class which handles event notifications
//
EventHandler::EventHandler()
{
    m_hLayoutEvent = NULL;
}

EventHandler::~EventHandler()
{
    CloseLayoutEventHandle();
}


HRESULT EventHandler::OnLayoutUpdatedHandler( IXRDependencyObject* pSender, XREventArgs* pArgs )
{
    if( m_hLayoutEvent )
    {
        SetEvent( m_hLayoutEvent );
    }

    return S_OK;
}

HRESULT EventHandler::CreateLayoutEventHandle()
{
    if( m_hLayoutEvent == NULL )
    {
        m_hLayoutEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    }

    return m_hLayoutEvent==NULL ? E_FAIL : S_OK;
}

HRESULT EventHandler::WaitForLayoutEvent()
{
    if( WaitForSingleObject(m_hLayoutEvent, 10000) != WAIT_OBJECT_0 )  //wait up to 10 seconds
    {
        return E_FAIL;
    }
    Sleep(10); //it appears the slower systems need just a little more time

    return S_OK;
}

HRESULT EventHandler::CloseLayoutEventHandle()
{
    if( m_hLayoutEvent )
    {
        CloseHandle( m_hLayoutEvent );
        m_hLayoutEvent = NULL;
    }

    return S_OK;
}


