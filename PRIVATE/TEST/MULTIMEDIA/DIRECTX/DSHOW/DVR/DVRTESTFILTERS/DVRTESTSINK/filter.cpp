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
#include <windows.h>
#include <streams.h>
#include <initguid.h>
#include "dvrtestsink.h"

// {70569FB3-BC50-4b4d-A0EB-149DC0D4868B}
DEFINE_GUID(CLSID_DVRTestSink, 
0x70569fb3, 0xbc50, 0x4b4d, 0xa0, 0xeb, 0x14, 0x9d, 0xc0, 0xd4, 0x86, 0x8b);

const AMOVIESETUP_MEDIATYPE sudVideoPinTypes[] =
{
    {
        &MEDIATYPE_DVD_ENCRYPTED_PACK,            // Major type
        &MEDIASUBTYPE_MPEG2_VIDEO          // Minor type
    }
};

const AMOVIESETUP_MEDIATYPE sudAudioPinTypes[] =
{
    {
        &MEDIATYPE_DVD_ENCRYPTED_PACK,            // Major type
        &MEDIASUBTYPE_DOLBY_AC3          // Minor type
    },
    {
        &MEDIATYPE_DVD_ENCRYPTED_PACK,            // Major type
        &MEDIASUBTYPE_MPEG2_AUDIO          // Minor type
    },
    {
        &MEDIATYPE_DVD_ENCRYPTED_PACK,            // Major type
        &MEDIASUBTYPE_DVD_LPCM_AUDIO          // Minor type
    }
};

const AMOVIESETUP_PIN sudPins[] =
{
    {
        L"Video",                     // Pin string name
        FALSE,                      // Is it rendered
        FALSE,                      // Is it an output
        FALSE,                      // Allowed none
        FALSE,                      // Likewise many
        &CLSID_NULL,                // Connects to filter
        NULL,                        // Connects to pin
        NUMELMS(sudVideoPinTypes),                          // Number of types
        sudVideoPinTypes                // Pin information
    },
    {
        L"Audio",                     // Pin string name
        FALSE,                      // Is it rendered
        FALSE,                      // Is it an output
        FALSE,                      // Allowed none
        FALSE,                      // Likewise many
        &CLSID_NULL,                // Connects to filter
        NULL,                        // Connects to pin
        NUMELMS(sudVideoPinTypes),                          // Number of types
        sudAudioPinTypes               // Pin information
    }
};

const AMOVIESETUP_FILTER sudDVRTestSink =
{
    &CLSID_DVRTestSink,     // Filter CLSID
    L"DVR test sink",        // String name
    MERIT_DO_NOT_USE,           // Filter merit
    NUMELMS(sudPins),       // Number pins
    sudPins                         // Pin details
};


//
//  Object creation stuff
//
CFactoryTemplate g_Templates[]= {
    L"DVR test sink", &CLSID_DVRTestSink, CDVRTestSinkFilter::CreateInstance, NULL, &sudDVRTestSink
};
int g_cTemplates = 1;

//  CDVRTestSinkFilter constructor.  Create the filter and pins.
CDVRTestSinkFilter::CDVRTestSinkFilter(LPUNKNOWN pUnk, HRESULT *phr) :
    CBaseFilter(NAME("CDVRTestSinkFilter"), pUnk, &m_Lock, CLSID_DVRTestSink),
    m_pVideoInputPin(NULL),
    m_pAudioInputPin(NULL)
{
    m_pVideoInputPin = new CVideoInputPin(this, GetOwner(), &m_Lock, phr);
    if (!m_pVideoInputPin)
    {
        *phr = E_OUTOFMEMORY;
        return;
    }

    m_pAudioInputPin = new CAudioInputPin(this, GetOwner(), &m_Lock, phr);
    if (!m_pAudioInputPin)
    {
        *phr = E_OUTOFMEMORY;
        return;
    }
}

// Destructor
CDVRTestSinkFilter::~CDVRTestSinkFilter()
{
    if(m_pVideoInputPin)
        delete m_pVideoInputPin;

    if(m_pAudioInputPin)
        delete m_pAudioInputPin;
}

// Retrieves one of our pins.
CBasePin * CDVRTestSinkFilter::GetPin(int n)
{
    if(0 == n)
        return m_pVideoInputPin;
    else if(1 == n)
        return m_pAudioInputPin;
    else return NULL;
}

int CDVRTestSinkFilter::GetPinCount()
{
    return NUMELMS(sudPins); 
}

// Provide the way for COM to create the NULL filter
CUnknown * WINAPI CDVRTestSinkFilter::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
    HRESULT hr;
    if (!phr)
        phr = &hr;
    
    CDVRTestSinkFilter *pNewObject = new CDVRTestSinkFilter(punk, phr);
    if (pNewObject == NULL)
        *phr = E_OUTOFMEMORY;

    return pNewObject;
}

STDMETHODIMP CDVRTestSinkFilter::NonDelegatingQueryInterface(REFIID riid, void ** ppv)
{
    CheckPointer(ppv, E_POINTER);

    return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP CDVRTestSinkFilter::Stop()
{
    m_pVideoInputPin->SetDiscontinuity();
    return CBaseFilter::Stop();
}

STDMETHODIMP CDVRTestSinkFilter::Run(REFERENCE_TIME rt)
{
    m_pVideoInputPin->SetDiscontinuity();
    return CBaseFilter::Run(rt);
}

// AMovieDllRegisterServer will call back into this to get pin info.
AMOVIESETUP_FILTER *
CDVRTestSinkFilter::GetSetupData()
{
    return (AMOVIESETUP_FILTER *)&sudDVRTestSink;
}

////////////////////////////////////////////////////////////////////////
//
// Exported entry points for registration and unregistration 
// (in this case they only call through to default implementations).
//
////////////////////////////////////////////////////////////////////////

//
// DllRegisterSever
//
// Handle the registration of this filter
//
STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2( TRUE );
} // DllRegisterServer


//
// DllUnregisterServer
//
STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2( FALSE );
} // DllUnregisterServer


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

