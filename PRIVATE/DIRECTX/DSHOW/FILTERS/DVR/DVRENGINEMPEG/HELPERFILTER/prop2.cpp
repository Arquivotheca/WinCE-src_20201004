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
#include "prop2.h"    // our own class

#define _CHR(e) {HRESULT _hr=(e); if (FAILED(_hr)) {DebugBreak(); return _hr;}}

BOOL NullIPProperties2::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
		{
			m_hWnd = hwnd;
//			m_hThread = CreateThread(NULL, 0, NullIPProperties2::ThreadProc, this, 0, NULL);
			SendMessage(GetDlgItem(m_hWnd, IDC_RATE1), BM_SETCHECK, BST_CHECKED, 0);
            return (LRESULT) 1;
		}

		case WM_DESTROY:
		{
			m_hWnd = NULL;
			while (m_hThread)
				Sleep(100);
			break;
		}

        case WM_COMMAND:
        {
			GetPointers();
			if (LOWORD(wParam) == IDC_RATE0)
				return SetRate(0);
			if (LOWORD(wParam) == IDC_RATE1)
				return SetRate(1);
			if (LOWORD(wParam) == IDC_RATE_1_2)
				return SetRate(-0.5);
			if (LOWORD(wParam) == IDC_RATE1_2)
				return SetRate(.5);
			if (LOWORD(wParam) == IDC_RATE1_2)
				return SetRate(0.5);
//			Refresh();
			/*
			if (LOWORD(wParam) == IDC_BEGIN_PERM)78
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
				return DeleteRecording();*/
        }
    }
    return CBasePropertyPage::OnReceiveMessage(hwnd,uMsg,wParam,lParam);
}

DWORD WINAPI NullIPProperties2::ThreadProc(void *p)
{
	NullIPProperties2 *pPage = (NullIPProperties2 *) p;
	HWND hWnd = pPage->m_hWnd;
	while (hWnd)
	{
		pPage->Refresh();
		Sleep(500);
		hWnd = pPage->m_hWnd;
	}
	CloseHandle(pPage->m_hThread);
	pPage->m_hThread = NULL;
	return 0;
}

WCHAR *l64tow(__int64 i, WCHAR *wsz)
{
	if (i == 0)
	{
		wcscpy(wsz, L"0");
		return wsz;
	}

	bool fNegative = false;
	if (i < 0)
	{
		i *= -1;
		fNegative = true;
	}

	WCHAR wszTemp[100];
	WCHAR *p = wszTemp;
	while (i)
	{
		*(p++) = '0' + (WCHAR) (i % 10);
		i /= 10;
	}
	if (fNegative)
		*(p++) = '-';

	int iIndex = 0;
	do
	{
		wsz[iIndex++] = *(--p);
	} while(p > wszTemp);
	wsz[iIndex] = 0;
	return wsz;
}

//
// CreateInstance
//
// Override CClassFactory method.
// Set lpUnk to point to an IUnknown interface on a new NullIPProperties object
// Part of the COM object instantiation mechanism
//
CUnknown * WINAPI NullIPProperties2::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
    ASSERT(phr);

    CUnknown *punk = new NullIPProperties2(lpunk, phr);

    if (punk == NULL) {
        if (phr)
            *phr = E_OUTOFMEMORY;
    }

    return punk;
}


//
// NullIPProperties2::Constructor
//
// Constructs and initialises a NullIPProperties object
//
NullIPProperties2::NullIPProperties2(LPUNKNOWN pUnk, HRESULT *phr)
    : CBasePropertyPage(NAME("NullIP Property Page\0"),pUnk, IDD_DIALOG2, IDS_TITLE2)
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
HRESULT NullIPProperties2::OnConnect(IUnknown *pUnknown)
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
HRESULT NullIPProperties2::OnDisconnect()
{
    // Release of Interface

    if (m_pINullIPP == NULL)
        return E_UNEXPECTED;

    m_pINullIPP->Release();
    m_pINullIPP = NULL;

	m_pInPin.Release();
	m_pOutPin.Release();
	m_pDownstreamFilter.Release();
	m_pUpstreamFilter.Release();

    return NOERROR;

} // OnDisconnect

HRESULT NullIPProperties2::GetPointers()
{
	m_pInPin.Release();
	m_pOutPin.Release();
	m_pDownstreamFilter.Release();

	_CHR(m_pINullIPP->get_IPins(&m_pInPin, &m_pOutPin));

	PIN_INFO pinInfo;
 	_CHR(m_pOutPin->QueryPinInfo(&pinInfo));
	m_pDownstreamFilter.Attach(pinInfo.pFilter);

 	_CHR(m_pInPin->QueryPinInfo(&pinInfo));
	m_pUpstreamFilter.Attach(pinInfo.pFilter);

	return S_OK;
}

int NullIPProperties2::SetRecordingPath()
{
	CComQIPtr<IStreamBufferCapture> pSBC = m_pDownstreamFilter;
	if (!pSBC)
		return E_NOINTERFACE;

	_CHR(pSBC->SetRecordingPath(L"C:\\TEMP"));
//	GetCurFileName();
	return 1;
}

int NullIPProperties2::BeginTempRecording()
{
	CComQIPtr<IStreamBufferCapture> pSBC = m_pDownstreamFilter;
	if (!pSBC)
		return E_NOINTERFACE;

	LONGLONG l = GetDlgItemInt(m_hWnd, IDC_TEMP_REC_LEN, NULL, FALSE);
	_CHR(pSBC->BeginTemporaryRecording(l*1000));
//	GetCurFileName();
	return 1;
}

int NullIPProperties2::ConvertToTemp()
{
	CComQIPtr<IStreamBufferCapture> pSBC = m_pDownstreamFilter;
	if (!pSBC)
		return E_NOINTERFACE;

	WCHAR wsz[1000];
	GetDlgItemTextW(m_hWnd, IDC_TEXTCURFILE, wsz, 999);
	_CHR(pSBC->ConvertToTemporaryRecording(wsz));
	return 1;
}

int NullIPProperties2::MuteStream(int id)
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

int NullIPProperties2::DeleteRecording()
{
	CComQIPtr<IDVREngineHelpers> pHelp = m_pDownstreamFilter;
	if (!pHelp)
		return E_NOINTERFACE;

	WCHAR wsz[1000];
	GetDlgItemTextW(m_hWnd, IDC_TEXTCURFILE, wsz, 999);
	_CHR(pHelp->DeleteRecording(wsz));

	return 1;
}

int NullIPProperties2::DeleteOrphans()
{
	CComQIPtr<IDVREngineHelpers> pHelp = m_pDownstreamFilter;
	if (!pHelp)
		return E_NOINTERFACE;

	WCHAR wsz[1000];
	GetDlgItemTextW(m_hWnd, IDC_TEXTCURFILE, wsz, 999);
	pHelp->CleanupOrphanedRecordings(wsz);

	return 1;
}

int NullIPProperties2::Refresh()
{
	GetCurSeekPos();
	return GetCurRate();
}

int NullIPProperties2::GetCurSeekPos()
{
	CComQIPtr<IStreamBufferMediaSeeking> pMS = m_pUpstreamFilter;
	if (pMS)
	{
		WCHAR wsz[1000];
		LONGLONG curPos;
		if (SUCCEEDED(pMS->GetCurrentPosition(&curPos)))
		{
			SetDlgItemTextW(m_hWnd, IDC_CURSEEKPOS, l64tow(curPos, wsz));
			return 1;
		}
	}
	SetDlgItemTextW(m_hWnd, IDC_CURSEEKPOS, L"<Error>");
	return 1;
}

int NullIPProperties2::GetCurRate()
{
	CComQIPtr<IStreamBufferMediaSeeking> pMS = m_pUpstreamFilter;
	if (pMS)
	{
		WCHAR wsz[1000];
		double dRate;
		if (SUCCEEDED(pMS->GetRate(&dRate)))
		{
			swprintf(wsz, L"%.2f", dRate);
			SetDlgItemTextW(m_hWnd, IDC_CURRATE, wsz);
			return 1;
		}
	}
	SetDlgItemTextW(m_hWnd, IDC_CURSEEKPOS, L"<Error>");
	return 1;
}

int NullIPProperties2::SetRate(double dRate)
{
	CComQIPtr<IStreamBufferMediaSeeking> pMS = m_pUpstreamFilter;
	if (pMS)
		pMS->SetRate(dRate);
	return 1;
}




