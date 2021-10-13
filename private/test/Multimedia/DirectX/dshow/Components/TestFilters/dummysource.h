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
#ifndef _DUMMY_SOURCE_H
#define _DUMMY_SOURCE_H

#define MAX_PINS 4

class CTypedOutputPin:
    public CBaseOutputPin
{
public:
    CTypedOutputPin( CBaseFilter* pFilter, CCritSec* pLoc, HRESULT* phr, WCHAR* wzPinName );
    ~CTypedOutputPin();

    // COM Compliance
    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFGUID riid, void **ppv);

    // check that we can support this output type
    HRESULT CheckMediaType(const CMediaType* pmt);          
    HRESULT SetMediaType(const CMediaType *pmt);            
    HRESULT GetMediaType(int iPosition,CMediaType *pMediaType);

	HRESULT DecideBufferSize(IMemAllocator * pAlloc, ALLOCATOR_PROPERTIES * pProperties);

    HRESULT SetSupportedTypes(AM_MEDIA_TYPE* mtArray, int nMediaTypes, bool bWildCard);

private:
    CMediaType* m_pMediaTypeArray;
    int m_nMediaTypes;
	BOOL m_bWildCard;
};


class CDummySource :
    public CBaseFilter
{
public:
    CDummySource( HRESULT *phr, int nPins );
    ~CDummySource();

    // COM Compliance
    DECLARE_IUNKNOWN;
    STDMETHOD(NonDelegatingQueryInterface)( REFIID riid, void ** ppv );
    static CUnknown* WINAPI CreateInstance( LPUNKNOWN pUnkOuter, HRESULT* phr );
    
    // Let's expose the pins
    int           GetPinCount();
    CBasePin*     GetPin(int n);

	HRESULT SetSupportedTypes(int pin, AM_MEDIA_TYPE* mtArray, int nMediaTypes, bool bWildCard);	
private:
	int m_nPins;
    CTypedOutputPin *m_pOutputPin[MAX_PINS];
    CCritSec            m_csFilter;
};

#endif
