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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <streams.h>
#pragma warning(disable: 4511 4512)

#include <windows.h>
//#include <winable.h>
#include <commctrl.h>
#include <olectl.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>


#include "resource.h"    // ids used in the dialog
#include "nulluids.h"    // public guids
#include "inull.h"       // private interface between property sheet and filter
#include "prop3.h"    // our own class

#define _CHR(e) {HRESULT _hr=(e); if (FAILED(_hr)) {DebugBreak(); return _hr;}}

BOOL NullIPProperties3::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
		{
			m_hWnd = hwnd;
			GetPointers();
			SetDlgItemInt(m_hWnd, IDC_TEXT_CHANNEL, GetChannel(), FALSE);
            return (LRESULT) 1;
		}

		case WM_DESTROY:
		{
			m_hWnd = NULL;
			break;
		}

        case WM_COMMAND:
        {
			GetPointers();
			if (LOWORD(wParam) == IDC_BUTTON_TUNE)
				return Tune();
        }
    }
    return CBasePropertyPage::OnReceiveMessage(hwnd,uMsg,wParam,lParam);
}

//
// CreateInstance
//
// Override CClassFactory method.
// Set lpUnk to point to an IUnknown interface on a new NullIPProperties object
// Part of the COM object instantiation mechanism
//
CUnknown * WINAPI NullIPProperties3::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
    ASSERT(phr);
    
    CUnknown *punk = new NullIPProperties3(lpunk, phr);

    if (punk == NULL) {
        if (phr)
            *phr = E_OUTOFMEMORY;
    }

    return punk;
}


//
// NullIPProperties3::Constructor
//
// Constructs and initialises a NullIPProperties object
//
NullIPProperties3::NullIPProperties3(LPUNKNOWN pUnk, HRESULT *phr)
    : CBasePropertyPage(NAME("NullIP Property Page\0"),pUnk, IDD_DIALOG3, IDS_TITLE3)
    , m_pInPin(NULL)
    , m_nIndex(0)
    , m_pINullIPP(NULL)
{
    ASSERT(phr);
} // (constructor) NullIPProperties

//
// OnConnect
//
// Override CBasePropertyPage method.
// Notification of which object this property page should display.
// We query the object for the INullIPP interface.
//
// If cObjects == 0 then we must release the interface.
// Set the member variable m_pPin to the upstream output pin connected
// to our input pin (or NULL if not connected).
//
HRESULT NullIPProperties3::OnConnect(IUnknown *pUnknown)
{
    ASSERT(m_pINullIPP == NULL);
    CheckPointer(pUnknown,E_POINTER);

    HRESULT hr = pUnknown->QueryInterface(IID_INullIPP, (void **) &m_pINullIPP);
    if (FAILED(hr))
    {
        return E_NOINTERFACE;
    }

    ASSERT(m_pINullIPP);
    ASSERT(!m_pInPin);

	return S_OK;

} // OnConnect


//
// OnDisconnect
//
// Override CBasePropertyPage method.
// Release the private interface, release the upstream pin.
//
HRESULT NullIPProperties3::OnDisconnect()
{
    // Release of Interface

    if (m_pINullIPP == NULL)
        return E_UNEXPECTED;

    m_pINullIPP->Release();
    m_pINullIPP = NULL;

	m_pInPin.Release();
	m_pUpstreamFilter.Release();

    return NOERROR;

} // OnDisconnect

HRESULT NullIPProperties3::GetPointers()
{
	m_pInPin.Release();

	CComPtr<IPin> pOut;
	_CHR(m_pINullIPP->get_IPins(&m_pInPin, &pOut));

	PIN_INFO pinInfo;

	_CHR(m_pInPin->QueryPinInfo(&pinInfo));
	m_pUpstreamFilter.Attach(pinInfo.pFilter);

	return S_OK;
}

UINT NullIPProperties3::GetChannel()
{
	CComQIPtr<IAMTVTuner> pTuner = m_pUpstreamFilter;
	if (pTuner)
	{
		long chan, x, y;
		if (FAILED(pTuner->get_Channel(&chan, &x, &y)))
			return 0;
		return chan;
	}
	return 0;
}

int NullIPProperties3::Tune()
{
	CComQIPtr<IAMTVTuner> pTuner = m_pUpstreamFilter;
	if (pTuner)
	{
		UINT iChan = GetDlgItemInt(m_hWnd, IDC_TEXT_CHANNEL, NULL, FALSE);

		_CHR(pTuner->put_Channel(	iChan,
									AMTUNER_SUBCHAN_DEFAULT,
									AMTUNER_SUBCHAN_DEFAULT));
	}
	return 1;
}
