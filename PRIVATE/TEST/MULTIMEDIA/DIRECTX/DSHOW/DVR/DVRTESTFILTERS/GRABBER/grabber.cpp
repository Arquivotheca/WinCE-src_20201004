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
//------------------------------------------------------------------------------
// File: Grabber.cpp
//
// Desc: DirectShow sample code - Implementation file for the SampleGrabber
//       example filter
//
//------------------------------------------------------------------------------

#include <streams.h>     // Active Movie (includes windows.h)
#include <initguid.h>    // declares DEFINE_GUID to declare an EXTERN_C const.

#ifndef UNDER_CE  //WINCE:  Unknown File
#include "qedit.h"
#endif //UNDER_CE
#include "grabber.h"

#pragma warning(disable: 4800)

const AMOVIESETUP_PIN psudSampleGrabberPins[] =
{ { L"Input"            // strName
  , FALSE               // bRendered
  , FALSE               // bOutput
  , FALSE               // bZero
  , FALSE               // bMany
  , &CLSID_NULL         // clsConnectsToFilter
  , L""                 // strConnectsToPin
  , 0                   // nTypes
  , NULL                // lpTypes
  }
, { L"Output"           // strName
  , FALSE               // bRendered
  , TRUE                // bOutput
  , FALSE               // bZero
  , FALSE               // bMany
  , &CLSID_NULL         // clsConnectsToFilter
  , L""                 // strConnectsToPin
  , 0                   // nTypes
  , NULL                // lpTypes
  }
};

const AMOVIESETUP_FILTER sudSampleGrabber =
{ &CLSID_GrabberSample            // clsID
, L"DVR Sample Grabber filter"        // strName
, MERIT_DO_NOT_USE                // dwMerit
, 2                               // nPins
, psudSampleGrabberPins };        // lpPin


// Needed for the CreateInstance mechanism
CFactoryTemplate g_Templates[]=
{
    { L"DVR Sample Grabber filter"
        , &CLSID_GrabberSample
        , CSampleGrabber::CreateInstance
        , NULL
        , &sudSampleGrabber }

};

int g_cTemplates = sizeof(g_Templates)/sizeof(g_Templates[0]);

#ifdef UNDER_CE
LPAMOVIESETUP_FILTER CSampleGrabber::GetSetupData(){ return (LPAMOVIESETUP_FILTER)&sudSampleGrabber; }
#endif

////////////////////////////////////////////////////////////////////////
//
// Exported entry points for registration and unregistration 
// (in this case they only call through to default implementations).
//
////////////////////////////////////////////////////////////////////////

STDAPI DllRegisterServer() 
{
    return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer() 
{
    return AMovieDllRegisterServer2(FALSE);
}

//
// DllEntryPoint
//
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, 
                      DWORD  dwReason, 
                      LPVOID lpReserved)
{
    return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

//
// CreateInstance
//
// Provide the way for COM to create a CSampleGrabber object
//
CUnknown * WINAPI CSampleGrabber::CreateInstance(LPUNKNOWN punk, HRESULT *phr) 
{
    ASSERT(phr);
    
    // assuming we don't want to modify the data
    CSampleGrabber *pNewObject = new CSampleGrabber(punk, phr, FALSE);

    if(pNewObject == NULL) {
        if (phr)
            *phr = E_OUTOFMEMORY;
    }

    return pNewObject;   

} // CreateInstance


#pragma warning(disable:4355)

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------

CSampleGrabber::CSampleGrabber( IUnknown * pOuter, HRESULT * phr, BOOL ModifiesData ) :
    CTransInPlaceFilter( TEXT("SampleGrabber"), (IUnknown*) pOuter, CLSID_GrabberSample, phr),
    m_callback( NULL ), m_lpUser (NULL), m_pGrabberInputPin(this,phr), m_pGrabberOutputPin(this,phr),
    m_bMediaTypeChanged( FALSE )

{
    // this is used to override the input pin with our own   
    m_pInput = (CTransInPlaceInputPin*) new CSampleGrabberInPin( this, phr );
    if( !m_pInput )
    {
        if (phr)
            *phr = E_OUTOFMEMORY;
    }

    m_pOutput = (CTransInPlaceOutputPin*) new CSampleGrabberOutPin( this, phr );
    if( !m_pOutput )
    {
        if (phr)
            *phr = E_OUTOFMEMORY;
    }
}


STDMETHODIMP CSampleGrabber::NonDelegatingQueryInterface( REFIID riid, void ** ppv) 
{
    CheckPointer(ppv,E_POINTER);

    if(riid == IID_IGrabberSample) {                
        return GetInterface((IGrabberSample *) this, ppv);
    }
    else {
        return CTransInPlaceFilter::NonDelegatingQueryInterface(riid, ppv);
    }
}


//----------------------------------------------------------------------------
// This is where you force the sample grabber to connect with one type
// or the other. What you do here is crucial to what type of data your
// app will be dealing with in the sample grabber's callback. For instance,
// if you don't enforce right-side-up video in this call, you may not get
// right-side-up video in your callback. It all depends on what you do here.
//----------------------------------------------------------------------------

HRESULT CSampleGrabber::CheckInputType( const CMediaType * pmt )
{
    CheckPointer(pmt,E_POINTER);
    CAutoLock lock( &m_Lock );

    // if the major type is not set, then accept anything

    GUID g = *m_mtAccept.Type( );
    if( g == GUID_NULL )
    {
        return NOERROR;
    }

    // if the major type is set, don't accept anything else

    if( g != *pmt->Type( ) )
    {
        return VFW_E_INVALID_MEDIA_TYPE;
    }

    // subtypes must match, if set. if not set, accept anything

    g = *m_mtAccept.Subtype( );
    if( g == GUID_NULL )
    {
        return NOERROR;
    }
    if( g != *pmt->Subtype( ) )
    {
        return VFW_E_INVALID_MEDIA_TYPE;
    }

    // format types must match, if one is set

    g = *m_mtAccept.FormatType( );
    if( g == GUID_NULL )
    {
        return NOERROR;
    }
    if( g != *pmt->FormatType( ) )
    {
        return VFW_E_INVALID_MEDIA_TYPE;
    }

    // at this point, for this sample code, this is good enough,
    // but you may want to make it more strict

    return NOERROR;
}


//----------------------------------------------------------------------------
// This bit is almost straight out of the base classes.
// We override this so we can handle Transform( )'s error
// result differently.
//----------------------------------------------------------------------------

HRESULT CSampleGrabber::Receive( IMediaSample * pms )
{
    CheckPointer(pms,E_POINTER);

    HRESULT hr;
    AM_SAMPLE2_PROPERTIES * const pProps = m_pInput->SampleProps();

    // we assume the media type didn't change, later we set this flag when we determine
    // it did.
    m_bMediaTypeChanged = FALSE;

    if (pProps->dwStreamId != AM_STREAM_MEDIA) 
    {
        if( m_pOutput->IsConnected() )
            return m_pOutput->Deliver(pms);
        else
            return NOERROR;
    }

    if (UsingDifferentAllocators()) 
    {
        // We have to copy the data.

        pms = Copy(pms);

        if (pms == NULL) 
        {
            return E_UNEXPECTED;
        }
    }

    AM_MEDIA_TYPE *pMediaType = NULL;
    if(SUCCEEDED(pms->GetMediaType(&pMediaType)))
    {
        if(pMediaType != NULL)
        {
            CMediaType cmt(*pMediaType);


            // BUGBUG: the media excel decoder doesn't set the media type correctly, so when we recieve
            // a sample with the type change from it we want to override it with what we know it outputs.
            // for other filters, everything should work as expected.
            {
                PIN_INFO pInfo;
                IPin *pPin = NULL;

                if(m_pInput->ConnectedTo(&pPin) && pPin)
                {
                    if(SUCCEEDED(pPin->QueryPinInfo(&pInfo)) && pInfo.pFilter)
                    {
                        FILTER_INFO fInfo;
                        pInfo.pFilter->QueryFilterInfo(&fInfo);

                        if(!_tcscmp(fInfo.achName, TEXT("Video Decoder")))
                        {
                            IEnumMediaTypes *pEnum;
                            AM_MEDIA_TYPE *pEnumeratedMediaType;
                            ULONG Fetched = 0;
                            
                            pPin->EnumMediaTypes(&pEnum);

                            pEnum->Next(1, &pEnumeratedMediaType, &Fetched);
                            
                            cmt = *pEnumeratedMediaType;

                            pEnum->Release();
                            DeleteMediaType(pEnumeratedMediaType);
                        }

                        pInfo.pFilter->Release();
                    }
                    pPin->Release();
                }
            }

            m_pInput->SetMediaType(&cmt);
            m_bMediaTypeChanged = TRUE;

            DeleteMediaType(pMediaType);
        }
    }
    
    // have the derived class transform the data
    hr = Transform(pms);

    if (FAILED(hr)) 
    {
        DbgLog((LOG_TRACE, 1, TEXT("Error from TransInPlace")));
        if (UsingDifferentAllocators()) 
        {
            pms->Release();
        }
        return hr;
    }

    if (hr == NOERROR) 
    {
        hr = m_pOutput->Deliver(pms);
    }
    
    // release the output buffer. If the connected pin still needs it,
    // it will have addrefed it itself.
    if (UsingDifferentAllocators()) 
    {
        pms->Release();
    }

    return hr;
}


//----------------------------------------------------------------------------
// Transform
//----------------------------------------------------------------------------

HRESULT CSampleGrabber::Transform ( IMediaSample * pms )
{
    CheckPointer(pms,E_POINTER);
    CAutoLock lock( &m_Lock );

    if( m_callback )
    {
        REFERENCE_TIME StartTime, StopTime;
        pms->GetTime( &StartTime, &StopTime);
        AM_MEDIA_TYPE pmt;

        m_pInput->ConnectionMediaType( &pmt );

        StartTime += m_pInput->CurrentStartTime( );
        StopTime  += m_pInput->CurrentStartTime( );

        HRESULT hr = m_callback( pms, &StartTime, &StopTime, m_bMediaTypeChanged, &pmt, m_lpUser );

        FreeMediaType(pmt);

        return hr;
    }

    return NOERROR;
}


//----------------------------------------------------------------------------
// SetAcceptedMediaType
//----------------------------------------------------------------------------

STDMETHODIMP CSampleGrabber::SetAcceptedMediaType( const CMediaType * pmt )
{
    CAutoLock lock( &m_Lock );

    if( !pmt )
    {
        m_mtAccept = CMediaType( );
        return NOERROR;        
    }

    HRESULT hr;
#ifndef UNDER_CE //WINCE:  CopyMediaType has no return value under Windows CE .net
    hr = CopyMediaType( &m_mtAccept, pmt );
#else
    hr=NOERROR;
    CopyMediaType( &m_mtAccept, pmt );
#endif

    return hr;
}

//----------------------------------------------------------------------------
// GetAcceptedMediaType
//----------------------------------------------------------------------------

STDMETHODIMP CSampleGrabber::GetConnectedMediaType( CMediaType * pmt )
{
    if( !m_pInput || !m_pInput->IsConnected( ) )
    {
        return VFW_E_NOT_CONNECTED;
    }

    return m_pInput->ConnectionMediaType( pmt );
}


//----------------------------------------------------------------------------
// SetCallback
//----------------------------------------------------------------------------

STDMETHODIMP CSampleGrabber::SetCallback( SAMPLECALLBACK Callback, LPVOID lpUser )
{
    CAutoLock lock( &m_Lock );

    m_callback = Callback;
    m_lpUser = lpUser;

    return NOERROR;
}

HRESULT CSampleGrabber:: CompleteConnect(PIN_DIRECTION dir,IPin *pReceivePin)
{
    return CTransInPlaceFilter::CompleteConnect(dir, pReceivePin);
}

STDMETHODIMP CSampleGrabberInPin::NonDelegatingQueryInterface( REFIID riid, void ** ppv) 
{
    CheckPointer(ppv,E_POINTER);

    if(riid == IID_IKsPropertySet) {                
        return ((CSampleGrabber*)m_pTIPFilter)->OutputPin( )->GetConnected()->QueryInterface(riid, ppv);
    }
    else {
        return CTransInPlaceInputPin::NonDelegatingQueryInterface(riid, ppv);
    }
}

//----------------------------------------------------------------------------
// used to help speed input pin connection times. We return a partially
// specified media type - only the main type is specified. If we return
// anything BUT a major type, some codecs written improperly will crash
//----------------------------------------------------------------------------

HRESULT CSampleGrabberInPin::GetMediaType( int iPosition, CMediaType * pMediaType )
{
    CheckPointer(pMediaType,E_POINTER);

    if (iPosition < 0) {
        return E_INVALIDARG;
    }
    if (iPosition > 0) {
        return VFW_S_NO_MORE_ITEMS;
    }

    *pMediaType = CMediaType( );
    pMediaType->SetType( ((CSampleGrabber*)m_pFilter)->m_mtAccept.Type( ) );

    return S_OK;
}


//----------------------------------------------------------------------------
// override the CTransInPlaceInputPin's method, and return a new enumerator
// if the input pin is disconnected. This will allow GetMediaType to be
// called. If we didn't do this, EnumMediaTypes returns a failure code
// and GetMediaType is never called. 
//----------------------------------------------------------------------------

STDMETHODIMP CSampleGrabberInPin::EnumMediaTypes( IEnumMediaTypes **ppEnum )
{
    CheckPointer(ppEnum,E_POINTER);
    ValidateReadWritePtr(ppEnum,sizeof(IEnumMediaTypes *));

    // if the output pin isn't connected yet, offer the possibly 
    // partially specified media type that has been set by the user

    if( !((CSampleGrabber*)m_pTIPFilter)->OutputPin( )->IsConnected() )
    {
        // Create a new reference counted enumerator

        *ppEnum = new CEnumMediaTypes( this, NULL );

        return (*ppEnum) ? NOERROR : E_OUTOFMEMORY;
    }

    return ((CSampleGrabber*)m_pTIPFilter)->OutputPin( )->GetConnected()->EnumMediaTypes( ppEnum );
}

HRESULT
CSampleGrabberOutPin::DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc)
{
    HRESULT hr = NOERROR;

    *ppAlloc = NULL;

    bool fNeedToConfigureAllocator = false;

    // if the output pin is connected, retrieve the downstream allocator
    // and set the flag to indicate that the allocator needs to be configured.
    if ( IsConnected() ) {
        // Get an addreffed allocator from the downstream input pin.
        hr = m_pInputPin->GetAllocator( ppAlloc );
    }

    // if we still don't have an allocator, try out the input pin
    if(*ppAlloc == NULL)
    {
        // if the input pin is connected and not read only, then set the allocator
        if (((CSampleGrabber*)m_pTIPFilter)->InputPin()) {
            if (!((CSampleGrabber*)m_pTIPFilter)->InputPin()->ReadOnly()) {
                *ppAlloc = ((CSampleGrabber*)m_pTIPFilter)->InputPin()->PeekAllocator();

                // we succeeded setting the allocator, so addref it.
                if (*ppAlloc!=NULL) {
                    // don't need to configure allocator -- upstream filter has
                    // already configured it
                    (*ppAlloc)->AddRef();
                }
            }
        }
    }

    if (*ppAlloc==NULL) {
        // Can't get one from upstream or downstream, so must use our own.

        hr = InitAllocator(ppAlloc);
        fNeedToConfigureAllocator = true;
    }

    if(FAILED(hr))
        return hr;

    if(*ppAlloc == NULL )
        return E_FAIL;

    // if we're using our own allocator, then it needs to be configured
    if (fNeedToConfigureAllocator) {

        ALLOCATOR_PROPERTIES prop;
        ZeroMemory(&prop, sizeof(prop));

        // Try to get requirements from downstream
        pPin->GetAllocatorRequirements(&prop);

        // if he doesn't care about alignment, then set it to 1
        if (prop.cbAlign == 0) {
            prop.cbAlign = 1;
        }

        hr = DecideBufferSize(*ppAlloc, &prop);

        if (FAILED(hr)) {
            (*ppAlloc)->Release();
            *ppAlloc = NULL;
        }
    }    

    // Tell the downstream input pin
    return pPin->NotifyAllocator(*ppAlloc, FALSE);

} // DecideAllocator

