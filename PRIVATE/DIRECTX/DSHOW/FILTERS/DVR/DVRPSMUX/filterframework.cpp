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

#include <objbase.h>
#include <initguid.h>
#include "filterframework.hpp"

using namespace DvrPsMux;

#define DISCONTINUITY_LATENCY (100 * 10000)        // arbitrary offset of 100ms

//
// File-scope variables needed by DirectShow base classes for
// filter implementation
//

const AMOVIESETUP_MEDIATYPE f_sudAudioInputType[] =
{
    {
        &MEDIATYPE_Audio,
        &MEDIASUBTYPE_MPEG2_AUDIO
    },

    {
        &MEDIATYPE_Audio,
        &MEDIASUBTYPE_DOLBY_AC3
    }
};

const AMOVIESETUP_MEDIATYPE f_sudVideoInputType[] =
{
    {
        &MEDIATYPE_Video,
        &MEDIASUBTYPE_MPEG2_VIDEO
    }
};

const AMOVIESETUP_MEDIATYPE f_sudOutputType[] =
{
    {
        &MEDIATYPE_Stream,
        &MEDIASUBTYPE_MPEG2_PROGRAM
    }
};

const AMOVIESETUP_PIN f_sudPins[] =
{
    {
        AUDIO_INPUT_PIN_NAME,
        FALSE,                          // bRendered
        FALSE,                          // bOutput
        FALSE,                          // bZero
        FALSE,                          // bMany
        &CLSID_NULL,                    // clsConnectsToFilter
        NULL,                           // ConnectsToPin
        NUMELMS(f_sudAudioInputType),   // Number of media types
        f_sudAudioInputType
    },
    {
        VIDEO_INPUT_PIN_NAME,
        FALSE,                          // bRendered
        FALSE,                          // bOutput
        FALSE,                          // bZero
        FALSE,                          // bMany
        &CLSID_NULL,                    // clsConnectsToFilter
        NULL,                           // ConnectsToPin
        NUMELMS(f_sudVideoInputType),   // Number of media types
        f_sudVideoInputType
    },
    {
        OUTPUT_PIN_NAME,
        FALSE,                          // bRendered
        TRUE,                           // bOutput
        FALSE,                          // bZero
        FALSE,                          // bMany
        &CLSID_NULL,                    // clsConnectsToFilter
        NULL,                           // ConnectsToPin
        NUMELMS(f_sudOutputType),       // Number of media types
        f_sudOutputType
    }
};

AMOVIESETUP_FILTER f_sudFilter =
{
    & CLSID_MPEG2PSMuxFilter,
    FILTER_NAME,
    MERIT_DO_NOT_USE,
    NUMELMS(f_sudPins),
    f_sudPins
} ;

// Global template array. AMovieDllRegisterServer2 in the base
// class uses this to determine which filters to register.
CFactoryTemplate g_Templates[] =
{
    {
        FILTER_NAME,
        & CLSID_MPEG2PSMuxFilter,
        Filter_t::CreateInstance,
        NULL,
        &f_sudFilter
    }
};

// Global count of # of templates. The base class uses this to
// figure out the # of entries in g_Templates
int g_cTemplates = NUMELMS(g_Templates);


//
// DLL functions
//

extern "C"
BOOL
WINAPI
DllEntryPoint(
    HINSTANCE,
    ULONG,
    LPVOID
    );

BOOL
APIENTRY
DllMain(
    HANDLE hModule,
    DWORD  dwReason,
    LPVOID lpReserved
    )
{
    switch (dwReason)
        {
        case DLL_PROCESS_ATTACH:
            wcsncpy(dpCurSettings.lpszName, L"dvrpsmux", 32);
            svsutil_Initialize();
            break;
        }

    // Need to call base class' DllEntryPoint to init global
    // variables. If we don't do this, lots of stuff breaks
    // (e.g. AMovieDllRegisterServer2)
    BOOL Succeeded = DllEntryPoint(
        static_cast<HINSTANCE>(hModule),
        dwReason,
        lpReserved
        );

    if (!Succeeded)
        {
        return FALSE;
        }

    switch (dwReason)
        {
        case DLL_PROCESS_DETACH:
            svsutil_DeInitialize();
            break;
        }

    return TRUE;
}

STDAPI
DllRegisterServer()
{
    return AMovieDllRegisterServer2(TRUE);
}

STDAPI
DllUnregisterServer()
{
    return AMovieDllRegisterServer2(FALSE);
}


//
// --------------- Filter_t ---------------
//

STDMETHODIMP
Filter_t::
NonDelegatingQueryInterface(
    REFIID riid,
    void ** ppv
    )
{
    if(riid == IID_IPropertyBag)
        {
        *ppv = dynamic_cast<IPropertyBag*>(this);
        ASSERT(*ppv);
        AddRef();
        }
    else
        {
        return
            CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
        }

    return S_OK;
}

CUnknown*
Filter_t::
CreateInstance(
    IUnknown* pUnk,
    HRESULT*  pHr
    )
{
    *pHr = S_OK;

    Filter_t* pFilter = new Filter_t(
        L"DvrPsMux::Filter_t",
        pUnk,
        pHr
        );
    if (!pFilter)
        {
        *pHr = E_OUTOFMEMORY;
        }

    if (FAILED(*pHr))
        {
        delete pFilter;
        pFilter = NULL;
        }

    return pFilter;
}

Filter_t::
Filter_t(
    wchar_t*  pName,
    IUnknown* pUnk,
    HRESULT*  pHr
    ) :
        CBaseFilter(pName, pUnk, &m_Lock, CLSID_MPEG2PSMuxFilter),
        m_pAudioInput(NULL),
        m_pVideoInput(NULL),
        m_pOutput(NULL)
{
    m_pOutput = new OutputPin_t(
        L"DvrPsMux::OutputPin_t",
        this,
        &m_Lock,
        pHr,
        OUTPUT_PIN_NAME
        );

    if (m_pOutput && SUCCEEDED(*pHr))
        {
        m_pMux = new Mux_t(this, m_pOutput);

        if (m_pMux)
            {
            m_pAudioInput = new InputPin_t(
                L"DvrPsMux::InputPin_t[Audio]",
                this,
                &m_Lock,
                pHr,
                AUDIO_INPUT_PIN_NAME,
                &f_sudPins[0],
                dynamic_cast<IMuxInput*>(m_pMux)
                );

            if (m_pAudioInput && SUCCEEDED(*pHr))
                {
                m_pVideoInput = new InputPin_t(
                    L"DvrPsMux::InputPin_t[Video]",
                    this,
                    &m_Lock,
                    pHr,
                    VIDEO_INPUT_PIN_NAME,
                    &f_sudPins[1],
                    dynamic_cast<IMuxInput*>(m_pMux)
                    );

                if (m_pVideoInput && SUCCEEDED(*pHr))
                    {
                    return;
                    }
                }
            }
        }

    if (m_pVideoInput)
        {
        delete m_pVideoInput;
        m_pVideoInput = NULL;
        }

    if (m_pAudioInput)
        {
        delete m_pAudioInput;
        m_pAudioInput = NULL;
        }

    if (m_pMux)
        {
        delete m_pMux;
        m_pMux = NULL;
        }

    if (m_pOutput)
        {
        delete m_pOutput;
        m_pOutput = NULL;
        }

    if (SUCCEEDED(*pHr))
        {
        *pHr = E_OUTOFMEMORY;
        }
}

Filter_t::
~Filter_t()
{
    if (m_pVideoInput)
        {
        delete m_pVideoInput;
        m_pVideoInput = NULL;
        }

    if (m_pAudioInput)
        {
        delete m_pAudioInput;
        m_pAudioInput = NULL;
        }

    if (m_pMux)
        {
        delete m_pMux;
        m_pMux = NULL;
        }

    if (m_pOutput)
        {
        delete m_pOutput;
        m_pOutput = NULL;
        }
}

CBasePin *
Filter_t::
GetPin(
    int Index
    )
{
    switch (Index)
    {
    case 0:
        return m_pAudioInput;
        break;

    case 1:
        return m_pVideoInput;
        break;

    case 2:
        return m_pOutput;
        break;
    };

    return NULL;
}

int
Filter_t::
GetPinCount()
{
    return NUMELMS(f_sudPins);
}

AMOVIESETUP_FILTER*
Filter_t::
GetSetupData()
{
    return &f_sudFilter;
}

STDMETHODIMP
Filter_t::
Run(
    REFERENCE_TIME tStart
    )
{
    CAutoLock Lock(&m_Lock);
    if ( !m_pMux->OnStart(tStart) )
        {
        return E_FAIL;
        }

    return CBaseFilter::Run(tStart);
}

// TODO: On Stop(), send MPEG_program_end_code
STDMETHODIMP
Filter_t::
Stop()
{
    CAutoLock Lock(&m_Lock);
    m_pMux->OnStop();
    return CBaseFilter::Stop();
}

//
// IPropertyBag impl
//

STDMETHODIMP
Filter_t::
Read(
    LPCOLESTR Name,
    VARIANT* pValue,
    IErrorLog* pErrorLog
    )
{
    if (!Name || !pValue)
        {
        return E_POINTER;
        }

    CAutoLock Lock(&m_Lock);

    return m_pMux->GetProperty(Name, pValue);
}


STDMETHODIMP
Filter_t::
Write(
    LPCOLESTR Name,
    VARIANT* pValue
    )
{
    if (!Name || !pValue)
        {
        return E_POINTER;
        }

    CAutoLock Lock(&m_Lock);

    return m_pMux->SetProperty(Name, pValue);
}


//
// -------------- InputPin_t --------------
//

HRESULT
InputPin_t::
CheckMediaType(
    const CMediaType* pMediaType
    )
{
    if (!pMediaType)
        {
        ASSERT(false);
        return E_POINTER;
        }

    for (UINT Index = 0; Index < m_pPinInfo->nMediaTypes; Index++)
        {
        const CLSID MajorType =
            *m_pPinInfo->lpMediaType[Index].clsMajorType;
        const CLSID MinorType =
            *m_pPinInfo->lpMediaType[Index].clsMinorType;

        if ( (*pMediaType->Type() == MajorType) &&
             (*pMediaType->Subtype() == MinorType) )
            {
            return S_OK;
            }
        }

    return S_FALSE;
}

HRESULT
InputPin_t::
CompleteConnect(
    IPin *pReceivePin
    )
{
    AM_MEDIA_TYPE MediaType;
    HRESULT hr = ConnectionMediaType(&MediaType);
    if (FAILED(hr))
        {
        return hr;
        }

    if (MediaType.subtype == MEDIASUBTYPE_MPEG2_VIDEO)
        {
        m_StreamType = Mpeg2Video;
        }
    else if (MediaType.subtype == MEDIASUBTYPE_MPEG2_AUDIO)
        {
        m_StreamType = Mpeg2Audio;
        }
    else if (MediaType.subtype == MEDIASUBTYPE_DOLBY_AC3)
        {
        m_StreamType = Ac3Audio;
        }
    else
        {
        ASSERT(false);
        return E_FAIL;
        }

    return S_OK;
}

HRESULT
InputPin_t::
GetMediaType(
    int         Position,
    CMediaType* pMediaType
    )
{
    if (!pMediaType)
        {
        ASSERT(false);
        return E_POINTER;
        }

    if ( static_cast<unsigned int>(Position) <
         m_pPinInfo->nMediaTypes )
        {
        pMediaType->InitMediaType();
        pMediaType->SetType(
            m_pPinInfo->lpMediaType[Position].clsMajorType
            );
        pMediaType->SetSubtype(
            m_pPinInfo->lpMediaType[Position].clsMinorType
            );

        return S_OK;
        }

    return VFW_S_NO_MORE_ITEMS;
}

HRESULT
InputPin_t::
Receive(
    IMediaSample* pSample
    )
{
#ifdef DEBUG
    {
    REFERENCE_TIME rtStart, rtEnd;
    HRESULT hr = pSample->GetTime(&rtStart, &rtEnd);
    if (S_OK == hr)
        {
        DEBUGMSG(
            ZONE_INFO_MAX,
            ( L"rtStart[%s] %ims %s\r\n",
              Mpeg2Video == m_StreamType ? L"V" : L"A",
              static_cast<long>(rtStart/10000),
              S_OK == pSample->IsDiscontinuity()
                ? L"- Discontinuity"
                : L"" )
            );
        }
    else if (VFW_E_MEDIA_TIME_NOT_SET == hr)
        {
        DEBUGMSG(
            ZONE_INFO_MAX,
            ( L"rtStart[%s] NOT SET %s\r\n",
              Mpeg2Video == m_StreamType ? L"V" : L"A",
              S_OK == pSample->IsDiscontinuity()
                ? L"- Discontinuity"
                : L"" )
            );
        }
    }
#endif

    // Note CBaseInputPin::Receive validates pSample
    HRESULT hr = CBaseInputPin::Receive(pSample);
    if (FAILED(hr))
        {
        return hr;
        }

    DEBUGMSG(
        ZONE_INFO_MAX,
        ( L"DvrPsMux::InputPin_t::Receive --> new %s sample"
          L"\r\n",
          Mpeg2Video == m_StreamType ? L"video" : L"audio" )
        );

#if 1

    // AddRef the sample here. Remember to Release if putting it in the queue fails
    pSample->AddRef();

    SampleEntry_t Entry = { m_StreamType, pSample };

    bool Succeeded = m_pMuxInput->Append(Entry);
    if (!Succeeded)
        {
        pSample->Release();
        return E_FAIL;
        }

#else
    DWORD Tick = GetTickCount();

    if (Mpeg2Video == m_StreamType)
        {
        static DWORD s_LastVideoTick = Tick;
        static DWORD s_MaxVideoLatency = 0;

        DWORD Latency = Tick - s_LastVideoTick;
        RETAILMSG(0, (L"@@@ Video Latency = %u\r\n", Latency) );

        if (Latency > s_MaxVideoLatency)
            {
            s_MaxVideoLatency = Latency;
            RETAILMSG(1, (L"@@@ Max Video Latency = %u\r\n", s_MaxVideoLatency) );
            }

        s_LastVideoTick = Tick;
        }
    else
        {
        static DWORD s_LastAudioTick = Tick;
        static DWORD s_MaxAudioLatency = 0;

        DWORD Latency = Tick - s_LastAudioTick;
        RETAILMSG(0, (L"@@@ Audio Latency = %u\r\n", Latency) );

        if (Latency > s_MaxAudioLatency)
            {
            s_MaxAudioLatency = Latency;
            RETAILMSG(1, (L"@@@ Max Audio Latency = %u\r\n", s_MaxAudioLatency) );
            }

        s_LastAudioTick = Tick;
        }
#endif

    return S_OK;
}

STDMETHODIMP
InputPin_t::
NewSegment(
    __int64 StartTime,
    __int64 StopTime,
    double Rate
    )
{
    HRESULT hr = CBasePin::NewSegment(
        StartTime,
        StopTime,
        Rate
        );
    if (FAILED(hr))
        {
        ASSERT(false);
        return hr;
        }

    return m_pMuxInput->NewSegment(
        StartTime,
        StopTime,
        Rate
        );
}


//
// ------------- OutputPin_t --------------
//

HRESULT
OutputPin_t::
CheckMediaType(
    const CMediaType* pMediaType
    )
{
    if (!pMediaType)
        {
        ASSERT(false);
        return E_POINTER;
        }

    if ( (*pMediaType->Type() == MEDIATYPE_Stream) &&
         (*pMediaType->Subtype() == MEDIASUBTYPE_MPEG2_PROGRAM) )
        {
        return S_OK;
        }

    return S_FALSE;
}

HRESULT
OutputPin_t::
GetMediaType(
    int         iPosition,
    CMediaType* pMediaType
    )
{
    if (!pMediaType)
        {
        ASSERT(false);
        return E_POINTER;
        }

    if (iPosition == 0)
        {
        pMediaType->InitMediaType();
        pMediaType->SetType(&MEDIATYPE_Stream);
        pMediaType->SetSubtype(&MEDIASUBTYPE_MPEG2_PROGRAM);

        return S_OK;
        }

    return VFW_S_NO_MORE_ITEMS;
}

HRESULT
OutputPin_t::
CompleteConnect(
    IPin *pReceivePin
    )
{
    HRESULT hr = CBaseOutputPin::CompleteConnect(pReceivePin);
    if (FAILED(hr))
        {
        return hr;
        }

    // If the downstream filter implements IStream, then it
    // is a file writer and we have to set timestamps a
    // little differently.
    IStream* pStream;
    hr = pReceivePin->QueryInterface(
        IID_IStream,
        reinterpret_cast<void**>(&pStream)
        );
    m_SinkIsFileWriter = SUCCEEDED(hr);
    if (pStream)
        {
        pStream->Release();
        }

    return S_OK;
}

HRESULT
OutputPin_t::
DecideBufferSize(
        IMemAllocator* pAlloc,
        ALLOCATOR_PROPERTIES* pPropInputRequest
        )
{
    if (!pAlloc || !pPropInputRequest)
        {
        ASSERT(false);
        return E_POINTER;
        }

    ALLOCATOR_PROPERTIES PropRequest;
    ALLOCATOR_PROPERTIES PropActual;
    PropRequest = *pPropInputRequest;

    // grab buffer count & size from the registry (use defaults
    // if registry keys don't exist)
    long BufferCount;
    long BufferSize;
    DWORD Size = sizeof(long);
    DWORD Type = 0;

    BufferCount = 10;              // default buffer count
    BufferSize = f_PackSize * 64;  // default buffer size

    HKEY hKey;
    long ReturnCode = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Dvr\\DvrPsMux",
        0,
        0,
        &hKey);
    if (ReturnCode == ERROR_SUCCESS && hKey != NULL)
        {
        ReturnCode = RegQueryValueEx(
            hKey,
            L"OutputBufferCount",
            NULL,
            &Type,
            reinterpret_cast<BYTE*>(&BufferCount),
            &Size
            );
        ASSERT( ERROR_SUCCESS == ReturnCode ||
                ERROR_FILE_NOT_FOUND == ReturnCode );

        if ( ERROR_SUCCESS != ReturnCode ||
             ( REG_DWORD != Type ||
               sizeof(DWORD) != Size ) )
            {
            DEBUGMSG(
                ZONE_INFO_MAX,
                ( L"Using default buffer count %d\n",\
                  BufferCount )
                );
            }

        Size = sizeof(long);
        Type = 0;

        ReturnCode = RegQueryValueEx(
            hKey,
            L"OutputBufferSize",
            NULL,
            &Type,
            reinterpret_cast<BYTE*>(&BufferSize),
            &Size
            );
        ASSERT( ERROR_SUCCESS == ReturnCode ||
                ERROR_FILE_NOT_FOUND == ReturnCode );

        if ( ERROR_SUCCESS != ReturnCode ||
             ( REG_DWORD != Type ||
               sizeof(DWORD) != Size ) )
            {
            DEBUGMSG(
                ZONE_INFO_MAX,
                ( L"Using default buffer size %d\n",
                  BufferSize )
                );
            }

        RegCloseKey(hKey);
        }

    // make sure the buffer size is a multiple of the pack size
    if (0 != BufferSize % f_PackSize)
        {
        // did we set the registry value incorrectly?
        ASSERT(false);
        BufferSize += ( f_PackSize - (BufferSize % f_PackSize) );
        }

    PropRequest.cBuffers = 
        max(PropRequest.cBuffers, BufferCount);
    PropRequest.cbBuffer =
        max(PropRequest.cbBuffer, BufferSize);

    DEBUGMSG(
        ZONE_INFO_MAX,
        ( L"DvrPsMux::OutputPin_t::DecideBufferSize --> "
          L"BufferSize = %d, BufferCount = %d\r\n",
          PropRequest.cBuffers,
          PropRequest.cbBuffer )
        );

    return pAlloc->SetProperties(&PropRequest, &PropActual);
}

