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

#include <windowsx.h>
#include <commctrl.h>
#include <olectl.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>

#include "resource.h"    // ids used in the dialog
#include "nulluids.h"    // public guids
#include "inull.h"       // private interface between property sheet and filter
#include "nullprop.h"    // our own class

#define _CHR(e) {HRESULT _hr=(e); if (FAILED(_hr)) {DebugBreak(); return _hr;}}

BOOL NullIPProperties::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
		{
			m_hWnd = hwnd;
            return (LRESULT) 1;
		}

        case WM_COMMAND:
        {
			GetPointers();
			if (LOWORD(wParam) == IDC_BEGIN_PERM)
				return BeginPermRecording();
			if (LOWORD(wParam) == IDC_BEGIN_TEMP)
				return BeginTempRecording();
			if (LOWORD(wParam) == IDC_GETCURFILENAME)
				return GetCurFileName();
			if (LOWORD(wParam) == IDC_SETPATH)
				return SetRecordingPath();
            if (LOWORD(wParam) == IDC_CONVERT)
				return ConvertToTemp();
            if (LOWORD(wParam) == IDC_MUTEVID)
				return MuteStream(0);
            if (LOWORD(wParam) == IDC_MUTEAUD)
				return MuteStream(1);
            if (LOWORD(wParam) == IDC_DELORPHANS)
				return DeleteOrphans();
            if (LOWORD(wParam) == IDC_DELRECORDING)
				return DeleteRecording();
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
CUnknown * WINAPI NullIPProperties::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
    ASSERT(phr);

    CUnknown *punk = new NullIPProperties(lpunk, phr);

    if (punk == NULL) {
        if (phr)
            *phr = E_OUTOFMEMORY;
    }

    return punk;
}


//
// NullIPProperties::Constructor
//
// Constructs and initialises a NullIPProperties object
//
NullIPProperties::NullIPProperties(LPUNKNOWN pUnk, HRESULT *phr)
    : CBasePropertyPage(NAME("NullIP Property Page\0"),pUnk, IDD_DIALOG1, IDS_TITLE)
    , m_pInPin(NULL)
	, m_pOutPin(NULL)
    , m_nIndex(0)
    , m_pINullIPP(NULL)
{
    ASSERT(phr);
	memset(m_enabled, 1, sizeof(m_enabled));

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
HRESULT NullIPProperties::OnConnect(IUnknown *pUnknown)
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
HRESULT NullIPProperties::OnDisconnect()
{
    // Release of Interface

    if (m_pINullIPP == NULL)
        return E_UNEXPECTED;

    m_pINullIPP->Release();
    m_pINullIPP = NULL;

	m_pInPin.Release();
	m_pOutPin.Release();
	m_pDownstreamFilter.Release();

    return NOERROR;

} // OnDisconnect

HRESULT NullIPProperties::GetPointers()
{
	m_pInPin.Release();
	m_pOutPin.Release();
	m_pDownstreamFilter.Release();

	_CHR(m_pINullIPP->get_IPins(&m_pInPin, &m_pOutPin));

	PIN_INFO pinInfo;
 	_CHR(m_pOutPin->QueryPinInfo(&pinInfo));

	m_pDownstreamFilter.Attach(pinInfo.pFilter);

	return S_OK;
}

int NullIPProperties::GetCurFileName()
{
	WCHAR * strFileName;
	WCHAR * strPath;

	CComQIPtr<IFileSinkFilter> pFileSink = m_pDownstreamFilter;
	if (!pFileSink)
		return E_NOINTERFACE;
	CComQIPtr<IStreamBufferCapture> pSBC = m_pDownstreamFilter;
	if (!pSBC)
		return E_NOINTERFACE;

	_CHR(pFileSink->GetCurFile(&strFileName, NULL));
	_CHR(pSBC->GetRecordingPath(&strPath));

	SetDlgItemTextW(m_hWnd, IDC_TEXTCURFILE, strFileName);

	CoTaskMemFree(strFileName);
	CoTaskMemFree(strPath);
	return 1;
}

int NullIPProperties::SetRecordingPath()
{
	CComQIPtr<IStreamBufferCapture> pSBC = m_pDownstreamFilter;
	if (!pSBC)
		return E_NOINTERFACE;

	_CHR(pSBC->SetRecordingPath(L"C:\\TEMP"));
	GetCurFileName();
	return 1;
}

int NullIPProperties::BeginTempRecording()
{
	CComQIPtr<IStreamBufferCapture> pSBC = m_pDownstreamFilter;
	if (!pSBC)
		return E_NOINTERFACE;

	LONGLONG l = GetDlgItemInt(m_hWnd, IDC_TEMP_REC_LEN, NULL, FALSE);
	_CHR(pSBC->BeginTemporaryRecording(l*1000));
	GetCurFileName();
	return 1;
}

int NullIPProperties::BeginPermRecording()
{
	CComQIPtr<IStreamBufferCapture> pSBC = m_pDownstreamFilter;
	if (!pSBC)
		return E_NOINTERFACE;

	LONGLONG l = GetDlgItemInt(m_hWnd, IDC_PERM_REC_LEN, NULL, FALSE);
	_CHR(pSBC->BeginPermanentRecording(l*1000));
	GetCurFileName();
	return 1;
}

int NullIPProperties::ConvertToTemp()
{
	CComQIPtr<IStreamBufferCapture> pSBC = m_pDownstreamFilter;
	if (!pSBC)
		return E_NOINTERFACE;

	WCHAR wsz[1000];
	GetDlgItemTextW(m_hWnd, IDC_TEXTCURFILE, wsz, 999);
	_CHR(pSBC->ConvertToTemporaryRecording(wsz));
	return 1;
}

int NullIPProperties::MuteStream(int id)
{
	CComQIPtr<IAMStreamSelect> pSS = m_pDownstreamFilter;
	if (!pSS)
		return E_NOINTERFACE;


	AM_MEDIA_TYPE *pmt;
	WCHAR *pwsz;
	DWORD dwNumStreams, dwFlags, dwGroup;
	_CHR(pSS->Count(&dwNumStreams));
	for (DWORD i = 0; i < dwNumStreams; i++)
	{
		_CHR(pSS->Info(i, &pmt, &dwFlags, NULL, &dwGroup, &pwsz, NULL, NULL));
		CoTaskMemFree(pwsz);
		DeleteMediaType(pmt);
	}

	m_enabled[id] = !m_enabled[id];
	_CHR(pSS->Enable(id, m_enabled[id] ? AMSTREAMSELECTENABLE_ENABLE : 0));

	return 1;
}

int NullIPProperties::DeleteRecording()
{
	CComQIPtr<IDVREngineHelpers> pHelp = m_pDownstreamFilter;
	if (!pHelp)
		return E_NOINTERFACE;

	WCHAR wsz[1000];
	GetDlgItemTextW(m_hWnd, IDC_TEXTCURFILE, wsz, 999);
	_CHR(pHelp->DeleteRecording(wsz));

	return 1;
}

int NullIPProperties::DeleteOrphans()
{
	CComQIPtr<IDVREngineHelpers> pHelp = m_pDownstreamFilter;
	if (!pHelp)
		return E_NOINTERFACE;

	WCHAR wsz[1000];
	GetDlgItemTextW(m_hWnd, IDC_TEXTCURFILE, wsz, 999);
	pHelp->CleanupOrphanedRecordings(wsz);

	return 1;
}

