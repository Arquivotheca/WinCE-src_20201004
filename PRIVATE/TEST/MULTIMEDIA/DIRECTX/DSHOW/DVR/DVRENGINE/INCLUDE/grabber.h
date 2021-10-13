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
#ifndef GRABBER_H
#define GRABBER_H

//------------------------------------------------------------------------------
// File: Grabber.h
//
// Desc: DirectShow sample code - Header file for the SampleGrabber
//       example filter
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Define new GUID and IID for the sample grabber example so that they do NOT
// conflict with the official DirectX SampleGrabber filter
//------------------------------------------------------------------------------

// {C40ADD49-D7DB-461f-8CB5-0D8C2EAD4282}
DEFINE_GUID(CLSID_GrabberSample, 
0xc40add49, 0xd7db, 0x461f, 0x8c, 0xb5, 0xd, 0x8c, 0x2e, 0xad, 0x42, 0x82);


// {FFA32E6D-8F8E-40c4-A447-5B6F9CB63C92}
DEFINE_GUID(IID_IGrabberSample, 
0xffa32e6d, 0x8f8e, 0x40c4, 0xa4, 0x47, 0x5b, 0x6f, 0x9c, 0xb6, 0x3c, 0x92);


// We define a callback typedef for this example. 
// Normally, you would make the SampleGrabber support a COM interface, 
// and in one of its methods you would pass in a pointer to a COM interface 
// used for calling back. See the DirectX documentation for the SampleGrabber
// for more information.

typedef HRESULT (*SAMPLECALLBACK) (
    IMediaSample * pSample, 
    REFERENCE_TIME * StartTime, 
    REFERENCE_TIME * StopTime,
    BOOL TypeChanged,
    AM_MEDIA_TYPE *mtype,
    LPVOID lpUser
    );

// We define the interface the app can use to program us
MIDL_INTERFACE("FFA32E6D-8F8E-40c4-A447-5B6F9CB63C92")
IGrabberSample : public IUnknown
{
    public:
        
        virtual HRESULT STDMETHODCALLTYPE SetAcceptedMediaType( 
            const CMediaType *pType) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType( 
            CMediaType *pType) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetCallback( 
            SAMPLECALLBACK Callback, LPVOID lpUser) = 0;
        
};
        

class CSampleGrabberInPin;
class CSampleGrabberOutPin;
class CSampleGrabber;

class CSampleGrabberInPin : public CTransInPlaceInputPin
{
    friend class CSampleGrabber;

    BYTE * m_pBuffer;

protected:

    CSampleGrabber * SampleGrabber( ) { return (CSampleGrabber*) m_pFilter; }

public:

    CSampleGrabberInPin( CTransInPlaceFilter * pFilter, HRESULT * pHr ) 
        : CTransInPlaceInputPin( TEXT("SampleGrabberInputPin\0"), pFilter, pHr, L"Input\0" )
        , m_pBuffer( NULL )
    {
    }

    ~CSampleGrabberInPin( )
    {
    }

    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);
    HRESULT GetMediaType( int iPosition, CMediaType *pMediaType );
    STDMETHODIMP EnumMediaTypes( IEnumMediaTypes **ppEnum );
};

class CSampleGrabberOutPin : public CTransInPlaceOutputPin
{

public:

    CSampleGrabberOutPin( CTransInPlaceFilter * pFilter, HRESULT * pHr ) 
        : CTransInPlaceOutputPin( TEXT("SampleGrabberOutputPin\0"), pFilter, pHr, L"Output\0" )
    {
    }

    ~CSampleGrabberOutPin( )
    {
    }

    HRESULT DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc);

};


//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------

class CSampleGrabber : public CTransInPlaceFilter,
                       public IGrabberSample
{
    friend class CSampleGrabberInPin;
    friend class CSampleGrabberOutPin;

protected:

    class CSampleGrabberInPin m_pGrabberInputPin;
    class CSampleGrabberInPin m_pGrabberOutputPin;
    CMediaType m_mtAccept;
    SAMPLECALLBACK m_callback;
    LPVOID m_lpUser;
    CCritSec m_Lock; // serialize access to our data
    BOOL m_bMediaTypeChanged;

    // PURE, override this to ensure we get 
    // connected with the right media type
    HRESULT CheckInputType( const CMediaType * pmt );

    // PURE, override this to callback 
    // the user when a sample is received
    HRESULT Transform( IMediaSample * pms );

    // override this so we can return S_FALSE directly. 
    // The base class CTransInPlace
    // Transform( ) method is called by it's 
    // Receive( ) method. There is no way
    // to get Transform( ) to return an S_FALSE value 
    // (which means "stop giving me data"),
    // to Receive( ) and get Receive( ) to return S_FALSE as well.

    HRESULT Receive( IMediaSample * pms );

#ifdef UNDER_CE //WINCE:  a method not exposed by our DirectShow
private:
    BOOL UsingDifferentAllocators(void) { return ((InputPin()->PeekAllocator())!=(OutputPin()->PeekAllocator())); }
#endif

public:

    static CUnknown *WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

    // Expose ISampleGrabber
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);
    DECLARE_IUNKNOWN;

    CSampleGrabber( IUnknown * pOuter, HRESULT * pHr, BOOL ModifiesData );

    // IGrabberSample
    STDMETHODIMP SetAcceptedMediaType( const CMediaType * pmt );
    STDMETHODIMP GetConnectedMediaType( CMediaType * pmt );
    STDMETHODIMP SetCallback( SAMPLECALLBACK Callback , LPVOID lpUser);

    HRESULT CompleteConnect(PIN_DIRECTION dir,IPin *pReceivePin);

#ifdef UNDER_CE
    virtual LPAMOVIESETUP_FILTER GetSetupData();
#endif

};

#endif
