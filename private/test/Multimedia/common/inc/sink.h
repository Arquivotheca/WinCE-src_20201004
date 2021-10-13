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

#pragma once


class CSinkInputPin:
    public CBasePin
{
    friend class CSinkFilter;

public:    
    CSinkInputPin( CBaseFilter* pFilter, CCritSec* pLoc, HRESULT* phr, WCHAR* wzPinName );
    ~CSinkInputPin();

    // COM Compliance
    DECLARE_IUNKNOWN

    STDMETHOD( BeginFlush )();
    STDMETHOD( EndFlush )();

    // check that we can support this output type
    HRESULT CheckMediaType( const CMediaType* pmt );          
    HRESULT SetMediaType( const CMediaType *pmt );            
    HRESULT GetMediaType( int iPosition,CMediaType *pMediaType );
    HRESULT CompleteConnect( IPin *pReceivePin );

private:
    CComPtr<IMemAllocator> m_pAllocator;
};

class CSinkFilter:
    public CBaseFilter,
    public IMediaSeeking
{
public:
    CSinkFilter();
    ~CSinkFilter();

    DECLARE_IUNKNOWN;
    STDMETHOD( NonDelegatingQueryInterface )( REFIID riid, void ** ppv );
    static CUnknown* WINAPI CreateInstance( LPUNKNOWN pUnkOuter, HRESULT* phr );

    STDMETHOD(  Stop )();
    STDMETHOD( Pause )();
    STDMETHOD( Run )( REFERENCE_TIME tStart );


    static DWORD ThreadProc( LPVOID dwParameter );
    HRESULT RunThread();

    // IMediaSeeking
    STDMETHOD( CheckCapabilities )( DWORD * pCapabilities );
    STDMETHOD( ConvertTimeFormat )( LONGLONG* pTarget, const GUID* pTargetFormat, LONGLONG Source, const GUID* pSourceFormat );
    STDMETHOD( GetAvailable )( LONGLONG* pEarliest, LONGLONG* pLatest );
    STDMETHOD( GetCapabilities )( DWORD* pCapabilities );
    STDMETHOD( GetCurrentPosition )( LONGLONG* pCurrent );
    STDMETHOD( GetDuration )( LONGLONG* pDuration );
    STDMETHOD( GetPositions )( LONGLONG* pCurrent, LONGLONG* pStop );
    STDMETHOD( GetPreroll )( LONGLONG* pllPreroll );
    STDMETHOD( GetRate)( double* dRate );
    STDMETHOD( GetStopPosition )( LONGLONG* pStop );
    STDMETHOD( GetTimeFormat )( GUID* pFormat );
    STDMETHOD( IsFormatSupported )( const GUID* pFormat );
    STDMETHOD( IsUsingTimeFormat )( const GUID* pFormat );
    STDMETHOD( QueryPreferredFormat )( GUID* pFormat );
    STDMETHOD( SetPositions )( LONGLONG* pCurrent, DWORD dwCurrentFlags, LONGLONG* pStop, DWORD dwStopFlags );
    STDMETHOD( SetRate )( double dRate );
    STDMETHOD( SetTimeFormat )( const GUID* pFormat );

    int           GetPinCount();
    CBasePin*     GetPin( int n );

private:
    CSinkInputPin       *m_pInputPin;
    CCritSec            m_csFilter;
    DWORD               m_dwLocation;
    DWORD               m_dwNext;
    DWORD               m_dwBitrate;

    HANDLE              m_hExitThread, m_hThread;

    HRESULT RequestNewBuffer( BOOL *pfContinue );
};
