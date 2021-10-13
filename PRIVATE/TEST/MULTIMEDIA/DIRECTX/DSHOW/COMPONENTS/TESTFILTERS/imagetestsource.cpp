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
// File: ImageTestSource.cpp
//
//------------------------------------------------------------------------------

//#include <uuids.h>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "common.h"
#include "ImageTestSource.h"
#include "Imaging.h"
#include "ValidType.h"
#include "videoload.h"

//
// Setup data for filter registration. This does not have to specify all media types supported.
//
const AMOVIESETUP_MEDIATYPE ImageTestOutputPinTypes[] =
{
    { &MEDIATYPE_Video,
      &MEDIASUBTYPE_NULL }
};

const AMOVIESETUP_PIN ImageTestOutputPinDesc =
{ L"Output"          // strName
, FALSE              // bRendered
, TRUE               // bOutput
, FALSE              // bZero
, FALSE              // bMany
, &CLSID_NULL        // clsConnectsToFilter
, L"Input"           // strConnectsToPin
, 1                  // nTypes
, ImageTestOutputPinTypes };  // lpTypes

const AMOVIESETUP_FILTER ImageTestSourceDesc =
{ &CLSID_ImageTestSource              // clsID
, L"Image Test Source"  // strName
, MERIT_UNLIKELY                  // dwMerit
, 1                               // nPins
, &ImageTestOutputPinDesc };                    // lpPin

// RGB565 BI_BITFIELD bitmaps
DWORD RGB565ColorTable[] = {
    0xf800, 0x07e0, 0x001f
};

// These are the media sub types we support
static VideoTypeSpec SupportedMediaTypes[] = {
    // For connecting to the other filters
    {MEDIATYPE_Video, MEDIASUBTYPE_YVYU,   16, FOURCC_YVYU, 330000, 0, NULL},
//    {MEDIATYPE_Video, MEDIASUBTYPE_YUYV,   16, FOURCC_YUYV, 330000, 0, NULL},
    {MEDIATYPE_Video, MEDIASUBTYPE_YUY2,   16, FOURCC_YUY2, 330000, 0, NULL},
    {MEDIATYPE_Video, MEDIASUBTYPE_YV12,   12, FOURCC_YV12, 330000, 0, NULL},
    {MEDIATYPE_Video, MEDIASUBTYPE_YV16,   16, FOURCC_YV16, 330000, 0, NULL},
    {MEDIATYPE_Video, MEDIASUBTYPE_UYVY,   16, FOURCC_UYVY, 330000, 0, NULL},
    {MEDIATYPE_Video, MEDIASUBTYPE_RGB24,  24, BI_RGB, 330000, 0, NULL},
    {MEDIATYPE_Video, MEDIASUBTYPE_RGB32,  32, BI_RGB, 330000, 0, NULL},
    {MEDIATYPE_Video, MEDIASUBTYPE_RGB555, 16, BI_RGB, 330000},
    {MEDIATYPE_Video, MEDIASUBTYPE_RGB565, 16, BI_BITFIELDS, 330000, sizeof(RGB565ColorTable), (BYTE*)&RGB565ColorTable[0]},

    // For connecting to the encoder. These must be a last resort, because they're only compatible with a small
    // number of filters.
    {MEDIATYPE_VideoBuffered, MEDIASUBTYPE_YVYU,   16, FOURCC_YVYU, 330000, 0, NULL},
//    {MEDIATYPE_VideoBuffered, MEDIASUBTYPE_YUYV,   16, FOURCC_YUYV, 330000, 0, NULL},
    {MEDIATYPE_VideoBuffered, MEDIASUBTYPE_YUY2,   16, FOURCC_YUY2, 330000, 0, NULL},
    {MEDIATYPE_VideoBuffered, MEDIASUBTYPE_YV12,   12, FOURCC_YV12, 330000, 0, NULL},
    {MEDIATYPE_VideoBuffered, MEDIASUBTYPE_YV16,   16, FOURCC_YV16, 330000, 0, NULL},
    {MEDIATYPE_VideoBuffered, MEDIASUBTYPE_UYVY,   16, FOURCC_UYVY, 330000, 0, NULL},
    {MEDIATYPE_VideoBuffered, MEDIASUBTYPE_RGB24,  24, BI_RGB, 330000, 0, NULL},
    {MEDIATYPE_VideoBuffered, MEDIASUBTYPE_RGB32,  32, BI_RGB, 330000, 0, NULL},
    {MEDIATYPE_VideoBuffered, MEDIASUBTYPE_RGB555, 16, BI_RGB, 330000},
    {MEDIATYPE_VideoBuffered, MEDIASUBTYPE_RGB565, 16, BI_BITFIELDS, 330000, sizeof(RGB565ColorTable), (BYTE*)&RGB565ColorTable[0]},
};

static const int nSupportedMediaTypes = sizeof(SupportedMediaTypes)/sizeof(SupportedMediaTypes[0]);

static AM_MEDIA_TYPE** MediaTypeOutput;

// Filter factory method
CUnknown * WINAPI CImageTestSource::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
    ASSERT(phr);

    //  DLLEntry does the right thing with the return code and
    //  the returned value on failure

    return new CImageTestSource(NULL, pUnk, phr);
}

// Filter methods
CImageTestSource::CImageTestSource(const TCHAR* szName, LPUNKNOWN pUnk, HRESULT* phr) :
    CBaseFilter(szName ? szName : TEXT("Image Test Source"), pUnk, &m_Lock, CLSID_ImageTestSource)
{
    // Create output pin?
    m_pOutputPin = new CImageTestOutputPin(phr, this, &m_Lock);
    if (m_pOutputPin == NULL) 
    {
        *phr = E_OUTOFMEMORY;
    }

    // Currently selected media type. -1 means we accept all of our supported media types on a connection.
    m_SelectedMediaType = -1;

    // Current verifier
    m_pVerifier = NULL;

    // Data source
    m_bDataSource = NONE;

    // Init image loading variables
    m_szImageFile = NULL;

    // Default streaming Mode
    m_StreamingMode = Discrete;
    m_nUnits = -1;
    m_StreamingFlags = UntilEndOfStream;

    // Setup output media type structures
    MediaTypeOutput = new PAM_MEDIA_TYPE[nSupportedMediaTypes];

    for(int i = 0; i < nSupportedMediaTypes; i++)
    {
        AM_MEDIA_TYPE *pMediaType = new AM_MEDIA_TYPE;

        // These are standard for the YUV and RGB that we deal with
        pMediaType->bFixedSizeSamples = true;
        pMediaType->bTemporalCompression = false;
        pMediaType->pUnk = NULL;
        pMediaType->formattype = FORMAT_VideoInfo;
        pMediaType->cbFormat = sizeof(VIDEOINFOHEADER) + SupportedMediaTypes[i].colorTableSize;
        pMediaType->pbFormat = new BYTE[pMediaType->cbFormat];
        VIDEOINFOHEADER* pVIH = (VIDEOINFOHEADER*)pMediaType->pbFormat;
        pVIH->dwBitErrorRate = 0;
        pVIH->dwBitRate = 0;
        RECT rect0 = {0,0,0,0};
        pVIH->rcSource = rect0;
        pVIH->rcTarget = rect0;
        pVIH->bmiHeader.biClrImportant = 0;
        pVIH->bmiHeader.biClrUsed = 0;
        pVIH->bmiHeader.biPlanes = 1;
        pVIH->bmiHeader.biXPelsPerMeter = 0;
        pVIH->bmiHeader.biYPelsPerMeter = 0;
        pVIH->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        
        // Type specific
        pMediaType->majortype = SupportedMediaTypes[i].majortype;
        pMediaType->subtype = SupportedMediaTypes[i].subtype;
        pVIH->bmiHeader.biBitCount = SupportedMediaTypes[i].bitcount;
        pVIH->bmiHeader.biCompression = SupportedMediaTypes[i].compression;
        pVIH->AvgTimePerFrame = SupportedMediaTypes[i].rtAvgTimePerFrame;
        // If we have a color table
        if (SupportedMediaTypes[i].colorTableSize)
        {
            // Cast to a VIDEOINFO structure
            VIDEOINFO* pVI = (VIDEOINFO*)pVIH;
            memcpy(&pVI->bmiColors[0], SupportedMediaTypes[i].pColorTable, SupportedMediaTypes[i].colorTableSize);
        }

        // Set 320x240 as default supported
        pVIH->bmiHeader.biWidth = 176;
        pVIH->bmiHeader.biHeight = 144;

        // What size buffer?
        pVIH->bmiHeader.biSizeImage = AllocateBitmapBuffer(pVIH->bmiHeader.biWidth, pVIH->bmiHeader.biHeight, pVIH->bmiHeader.biBitCount, pVIH->bmiHeader.biCompression, NULL);
        pMediaType->lSampleSize = pVIH->bmiHeader.biSizeImage;

        // Set the media type
        MediaTypeOutput[i] = pMediaType;
    }

    // Set the lateness probability as 0 - all frames on time
    m_LatenessProbability = 0.0;

    // By default, we don't use our own allocator
    m_bOwnAllocator = false;

    // Our allocator
    m_pAllocator = NULL;
}

CImageTestSource::~CImageTestSource()
{
    for (int i = 0; i < nSupportedMediaTypes; i++)
    {
        // Free the format block first
        delete MediaTypeOutput[i]->pbFormat;
        delete MediaTypeOutput[i];
    }

    delete [] MediaTypeOutput;

    if (m_pOutputPin)
        delete m_pOutputPin;
}

STDMETHODIMP CImageTestSource::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
    CheckPointer(ppv,E_POINTER);

    if (riid == IID_ITestSource) {
        return GetInterface((ITestSource*)this, ppv);
    } else {
        return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
    }
    return NOERROR;
}

int CImageTestSource::GetPinCount()
{
    return 1;
}

CBasePin *CImageTestSource::GetPin(int pin)
{
    // We only support one output pin and it is numbered zero
    if(pin != 0) {
        return NULL;
    }

    return m_pOutputPin;    
}


// ITestSource Implementation
HRESULT CImageTestSource::SetVerifier(IVerifier* pVerifier)
{
    m_pVerifier = pVerifier;
    return S_OK;
}

IVerifier* CImageTestSource::GetVerifier()
{
    return m_pVerifier;
}

HRESULT CImageTestSource::BeginFlushTest()
{
    return m_pOutputPin->SetControlEvent(EvBeginFlush);
}

HRESULT CImageTestSource::EndFlushTest()
{
    return m_pOutputPin->SetControlEvent(EvEndFlush);
}

HRESULT CImageTestSource::NewSegmentTest()
{
    return m_pOutputPin->SetControlEvent(EvNewSegment);
}

HRESULT CImageTestSource::EndOfStreamTest()
{
    return m_pOutputPin->SetControlEvent(EvEndOfStream);
}

HRESULT CImageTestSource::BeginStreaming()
{
    return m_pOutputPin->SetControlEvent(EvBeginStreaming);
}

HRESULT CImageTestSource::EndStreaming()
{
    return m_pOutputPin->SetControlEvent(EvEndStreaming);
}

bool CImageTestSource::IsStreaming()
{
    return m_pOutputPin->m_bStreaming;
}

int CImageTestSource::GetSamplesSourced(int pin)
{
    if (pin == 0)
        return m_pOutputPin->m_nSamplesSourced;
    return -1;
}

HRESULT CImageTestSource::SetSampleLatenessProbability(float p)
{
    m_LatenessProbability = p;
    return S_OK;
}

float CImageTestSource::GetSampleLatenessProbability()
{
    return m_LatenessProbability;
}

// Media type related functionality
int CImageTestSource::GetNumMediaTypeCombinations()
{
    // We have only one pin
    return nSupportedMediaTypes;
}

HRESULT CImageTestSource::SelectMediaTypeCombination(int iPosition)
{
    // a requested position of -1 means use all possible media types
    if ((iPosition < -1) || (iPosition >= nSupportedMediaTypes))
        return E_INVALIDARG;
    m_SelectedMediaType = iPosition;
    return S_OK;
}

int CImageTestSource::GetSelectedMediaTypeCombination()
{
    return m_SelectedMediaType;
}

HRESULT CImageTestSource::GetMediaTypeCombination(int iPosition, AM_MEDIA_TYPE** ppMediaType)
{
    if ((iPosition < 0) || (iPosition >= nSupportedMediaTypes))
        return E_INVALIDARG;

    if (!ppMediaType || !ppMediaType[0])
        return E_POINTER;

    CopyMediaType(ppMediaType[0], MediaTypeOutput[iPosition]);
    return S_OK;
}

// Setting streaming modes
HRESULT CImageTestSource::SetStreamingMode(StreamingMode mode, int nUnits, StreamingFlags flags)
{
    m_StreamingMode = mode;
    m_nUnits = nUnits;
    m_StreamingFlags = flags;
    return S_OK;
}

// Data generation
HRESULT CImageTestSource::GenerateMediaSample(IMediaSample* pMediaSample, IPin* pPin)
{
    const TCHAR* imagefile = m_szImageFile;
    
    // We have only one output pin so no need to figure out which is which
    HRESULT hr = NOERROR;

    // Get the actual buffer to write into
    BYTE* pImageBuffer;
    hr = pMediaSample->GetPointer(&pImageBuffer);
    
    if (SUCCEEDED(hr))
    {
        // Get the format block from the connection media type
        CMediaType cmtType;
        hr = pPin->ConnectionMediaType(&cmtType);
        if (SUCCEEDED(hr))
        {
            VIDEOINFOHEADER *pVIH = reinterpret_cast<VIDEOINFOHEADER*>(cmtType.Format());
            if (!imagefile || (imagefile && IsImageFile(imagefile)))
            {
                // the video info header structure actually contains a bitmapinfo structure, not a bitmapinfoheader structure
                LoadImage(pImageBuffer, (BITMAPINFO *) &pVIH->bmiHeader, cmtType.subtype, imagefile);
            }
            else
            {
                InitVideo(imagefile);
                ReadVideo(pMediaSample);
            }
        }
    }

    return hr;
}


HRESULT CImageTestSource::SetDataSource(const TCHAR* file)
{
    if (file)
    {
        if (m_szImageFile)
            delete m_szImageFile;

        size_t len = _tcslen(file);
        m_szImageFile = new TCHAR[len + 1];
        if (m_szImageFile)
            _tcscpy(m_szImageFile, file);
        else
            return E_OUTOFMEMORY;
    }

    if (IsImageFile(file))
    {
        m_bDataSource = IMAGE;
    }
    else if (IsVideoFile(file)) 
    {
        m_bDataSource = VIDEO;
    }
    else m_bDataSource = NONE;
    return NOERROR;
}


bool CImageTestSource::SetInvalidConnectionParameters()
{
    return false;
}

bool CImageTestSource::SetInvalidAllocatorParameters()
{
    return false;
}

HRESULT CImageTestSource::UseOwnAllocator()
{
    m_bOwnAllocator = true;
    return S_OK;
}

bool CImageTestSource::IsUsingOwnAllocator()
{
    return (m_pAllocator) ? true : false;
}

HRESULT CImageTestSource::GetRefTime(LONGLONG* pCurrentTime)
{
    HRESULT hr = S_OK;


    if (!pCurrentTime)
        return E_POINTER;

    IReferenceClock* pRefClock = NULL;
    hr = GetSyncSource(&pRefClock);
    if (SUCCEEDED(hr))
    {
        hr = pRefClock->GetTime(pCurrentTime);
    }
#if 0
    else {
        *pCurrentTime = GetTickCount()*10000;
    }
#endif
    if (pRefClock)
        pRefClock->Release();
    return hr;
}

HRESULT CImageTestSource::GetStreamTime(REFERENCE_TIME* pStreamTime)
{
    // Take the lock because we check the state and don't want it to change beneath us
    CAutoLock lock(m_pLock);

    HRESULT hr = S_OK;

    if (!pStreamTime)
        return E_POINTER;

    if (m_State == State_Stopped)
        return S_FALSE;

    REFERENCE_TIME rtCurrentTime = 0;
    hr = GetRefTime(&rtCurrentTime);
    if (SUCCEEDED(hr))
    {
        REFERENCE_TIME tStart = m_tStart;

        // If we are paused and the start time is still 0, then perhaps we were never given a start time
        if ((m_State == State_Paused) && (tStart == 0))
        {
            *pStreamTime = 0;
            return S_FALSE;
        }

        // Subtract the current reference time from the starting reference time to get the stream time
        // Now the FGM when it sets the stream time can set it in advance of the current time to compensate for state change delays
        // This will cause a straight forward computation of the time difference (accounting for rollover) fail
        // BUGBUG: hardcoding values
        if (rtCurrentTime >=  tStart)
            *pStreamTime = (rtCurrentTime - tStart);
        else if (rtCurrentTime <  tStart)
            *pStreamTime = (_abs64(rtCurrentTime - tStart) > 10000000) ? (0xffffffff - tStart + rtCurrentTime) : 0;
    }
    return hr;
}

STDMETHODIMP CImageTestSource::Stop()
{
    HRESULT hr = NOERROR;
    // I cannot take the m_pLock because if I do then the thread on which I depend on to stop will not be able to take the lock. 
    // So it will wait for me to give up the lock and I will wait for it to signal me.
    // notify all pins of the state change
    if (m_State != State_Stopped) {
        // Assumption: IsConnected does not take a lock
        if (m_pOutputPin->IsConnected()) {
            HRESULT hrTemp = m_pOutputPin->Inactive();
            // Even if we fail we still have to go to Stop state
            if (FAILED(hrTemp)) {
                hr = hrTemp;
            }
        }
    }

    m_State = State_Stopped;
    return hr;
}

STDMETHODIMP CImageTestSource::Pause()
{
    // I cannot take the m_pLock because if I do then the thread on which I depend on to signal me will not be able to take the lock. 
    // So it will wait for me to give up the lock and I will wait for it to signal me.
    // notify all pins of the state change
    if (m_State == State_Stopped) {
        // Disconnected pins are not activated - this saves pins
        // worrying about this state themselves        
        if (m_pOutputPin->IsConnected()) {
            HRESULT hr = m_pOutputPin->Active();
            if (FAILED(hr)) {
                return hr;
            }
        }
    }
    m_State = State_Paused;
    return S_OK;
}

STDMETHODIMP
CImageTestSource::Run(REFERENCE_TIME tStart)
{
    // No lock taken.

    // remember the stream time offset
    m_tStart = tStart;
    
    if (CBaseFilter::m_State == State_Stopped){
        HRESULT hr = Pause();
        
        if (FAILED(hr)) {
            return hr;
        }
    }
    // notify all pins of the change to active state
    if (m_State != State_Running) {
        // Assumption: IsConnected does not take a lock
        // Notify that the pin is now in Run state. Disconnected pins are not activated - this saves pins
        // worrying about this state themselves. Not expecting this to fail.
        if (m_pOutputPin->IsConnected()) {
            m_pOutputPin->Run(tStart);
        }
    }

    m_State = State_Running;
    return S_OK;
}


// The Output Pin implementation
CImageTestOutputPin::CImageTestOutputPin(HRESULT * phr, CImageTestSource* pFilter, CCritSec* pLock) : 
      CBaseOutputPin(NAME("Image Test Output Pin"), pFilter, pLock, phr, L"Output")
{
    m_pLock = pLock;
    m_hThread = NULL;
    m_hr = NOERROR;
    m_bIsActive = false;
    m_bStreaming = false;
    m_bFlushing = false;
    m_hControlEvent = CreateEvent(NULL, false, false, TEXT("Control Event"));
    m_hEventServiced = CreateEvent(NULL, false, false, TEXT("Event Serviced"));
    m_pTestSource = pFilter;
    m_nSamplesSourced = 0;
    m_nSamplesLate = 0;
    m_rtCurrentFrame = 0;

    if (!m_hControlEvent || !m_hEventServiced)
    {
        *phr = E_OUTOFMEMORY;
        return;
    }
    m_hThread = CreateThread(NULL, 8*1024, &CImageTestOutputPin::ThreadFunction, (void*)this, 0, NULL);
}

CImageTestOutputPin::~CImageTestOutputPin()
{
    // Wait for the thread to exit before killing the control events
    if (m_hControlEvent) {
        m_Event = EvExit;
        SetEvent(m_hControlEvent);
    }
    
    if (m_hThread) {
        WaitForSingleObject(m_hThread, INFINITE);
        CloseHandle(m_hThread);
    }

    if (m_hControlEvent)
        CloseHandle(m_hControlEvent);
    if (m_hEventServiced)
        CloseHandle(m_hEventServiced);
}

STDMETHODIMP CImageTestOutputPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    return CBaseOutputPin::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CImageTestOutputPin::Notify(IBaseFilter * pSender, Quality q)
{
    UNREFERENCED_PARAMETER(q);
    UNREFERENCED_PARAMETER(pSender);
    return E_NOTIMPL;
}

HRESULT CImageTestOutputPin::GetMediaType(int iPosition, CMediaType *pMediaType)
{
    CAutoLock lock(m_pLock);

    if (!pMediaType)
        return E_POINTER;

    if (iPosition < 0) {
        return E_INVALIDARG;
    }

    // If there has been a selection just use that one
    int selected = m_pTestSource->GetSelectedMediaTypeCombination();
    int nMediaTypes = (selected == -1) ? nSupportedMediaTypes : 1;

    if (iPosition >= nMediaTypes) {
        return VFW_S_NO_MORE_ITEMS;
    }

    int iActualPosition = (selected == -1) ? iPosition : selected;

    *pMediaType = *MediaTypeOutput[iActualPosition];
    return S_OK;
}

// can we support this type?
HRESULT CImageTestOutputPin::CheckMediaType(const CMediaType *pMediaType)
{
    CheckPointer(pMediaType,E_POINTER);

    int selected = m_pTestSource->GetSelectedMediaTypeCombination();
    int nMediaTypes = (selected == -1) ? m_pTestSource->GetNumMediaTypeCombinations() : 1;

    bool accept = false;
    for (int i = 0; i < nMediaTypes; i++)
    {
        AM_MEDIA_TYPE* pSelectedMediaType = new AM_MEDIA_TYPE;
        if (!pSelectedMediaType)
            return E_OUTOFMEMORY;
        memset(pSelectedMediaType, 0, sizeof (AM_MEDIA_TYPE));

        // Since we are the output pin, we will support wildcard matches
        if (SUCCEEDED(m_pTestSource->GetMediaTypeCombination((selected == -1) ? i : selected, &pSelectedMediaType)))
        {
            // Check for major, minor, formattype and temporal and fixed size samples
            if ((*pMediaType->Type() == pSelectedMediaType->majortype) &&
                ((*pMediaType->Subtype() == GUID_NULL) || (*pMediaType->Subtype() == pSelectedMediaType->subtype)) &&
                (pMediaType->bFixedSizeSamples == pSelectedMediaType->bFixedSizeSamples) &&
                (pMediaType->bTemporalCompression == pSelectedMediaType->bTemporalCompression) &&
                (pMediaType->FormatType()) &&
                (*pMediaType->FormatType() == pSelectedMediaType->formattype)
                )
            {
                // Check the format block
                if (*pMediaType->FormatType() == FORMAT_VideoInfo) {
                    VIDEOINFOHEADER *pVIH = (VIDEOINFOHEADER*) pMediaType->Format();
                    VIDEOINFOHEADER *pRefVIH = (VIDEOINFOHEADER*) pSelectedMediaType->pbFormat;
                    if (pVIH && 
                        (pVIH->bmiHeader.biCompression == pRefVIH->bmiHeader.biCompression) &&
                        (pVIH->bmiHeader.biPlanes == pRefVIH->bmiHeader.biPlanes) &&
                        (pVIH->bmiHeader.biBitCount == pRefVIH->bmiHeader.biBitCount) &&
                        (pVIH->bmiHeader.biHeight > 0)  &&
                        (pVIH->bmiHeader.biWidth > 0) )
                    {
                        // All the checks seem ok, accept the media type
                        accept = true;
                        if (pSelectedMediaType->pbFormat)
                            delete pSelectedMediaType->pbFormat;
                        delete pSelectedMediaType;
                        pSelectedMediaType = NULL;
                        break;
                    }
                }
            }
        }
        if (pSelectedMediaType->pbFormat)
            delete pSelectedMediaType->pbFormat;
        delete pSelectedMediaType;
        pSelectedMediaType = NULL;
    }
    
    return (accept) ? S_OK : S_FALSE;
}

HRESULT CImageTestOutputPin::DecideBufferSize(IMemAllocator * pAlloc, ALLOCATOR_PROPERTIES * pProperties)
{
    CheckPointer(pAlloc,E_POINTER);
    CheckPointer(pProperties,E_POINTER);
    HRESULT hr = NOERROR;

    // Refer to the media type agreed: this is set in CBasePin::SetMediaType
    VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER *) m_mt.Format();
    if (!pVih)
        return E_FAIL;

    // The properties we get may have been filled in by the input pin. If he does not have any preferences, we will set our default preference.
    // If he does have preferences, then we will accept his preference.
    if (!pProperties->cBuffers)
        pProperties->cBuffers = 1;

    // We need to know the input dimensions when we get here.
    int cbBufferRequired = AllocateBitmapBuffer(pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, pVih->bmiHeader.biBitCount, pVih->bmiHeader.biCompression, NULL);
    if (pProperties->cbBuffer < cbBufferRequired)
        pProperties->cbBuffer = cbBufferRequired;

    if (!pProperties->cbBuffer)
        return E_FAIL;

    // Ask the allocator to reserve us some sample memory, NOTE the function
    // can succeed (that is return NOERROR) but still not have allocated the
    // memory that we requested, so we must check we got whatever we wanted
    ALLOCATOR_PROPERTIES actual;
    hr = pAlloc->SetProperties(pProperties,&actual);
    if (FAILED(hr)) 
        return hr;

    // Is this allocator unsuitable
    if (actual.cbBuffer < pProperties->cbBuffer) 
        return E_FAIL;
    if (actual.cBuffers != pProperties->cBuffers)
        return E_FAIL;

    return NOERROR;
}

HRESULT CImageTestOutputPin::DecideAllocator(IMemInputPin* pPin, IMemAllocator **ppAlloc)
{
    if (!m_pTestSource->m_bOwnAllocator)
        return CBaseOutputPin::DecideAllocator(pPin, ppAlloc);

    if (!ppAlloc)
        return E_POINTER;

    HRESULT hr = S_OK;
    IMemAllocator* pAllocator = NULL;

    *ppAlloc = NULL;
    
    // Get input pin's allocator request
    ALLOCATOR_PROPERTIES prop;
    ZeroMemory(&prop, sizeof(prop));  
    
    // We assume prop is either all zeros or he has filled it out.
    pPin->GetAllocatorRequirements(&prop);
    // If the input pin doesn't care about alignment, set it to 1
    if (prop.cbAlign == 0) {
        prop.cbAlign = 1;
    }
    
    // Create our own allocator
    hr = CreateMemoryAllocator(&pAllocator);
    if (SUCCEEDED(hr)) 
    {
        hr = DecideBufferSize(pAllocator, &prop);
        if (SUCCEEDED(hr)) {
            hr = pPin->NotifyAllocator(pAllocator, FALSE);
        }
    }
    
    // Return the allocator
    if (SUCCEEDED(hr)) {
        *ppAlloc = pAllocator;
        
        // BUGBUG: No need to addref the allocator since CreateMemoryAllocator already adds a reference?
        m_pTestSource->m_pAllocator = pAllocator;
    }
    else {
        *ppAlloc = NULL;
    }
    
    return hr;
}

HRESULT CImageTestOutputPin::SetControlEvent(ControlEvent ev)
{
    m_Event = ev;
    
    // Signal to the push thread that a flush must be scheduled
    SetEvent(m_hControlEvent);
    // Now wait for the event to be serviced
    WaitForSingleObject(m_hEventServiced, INFINITE);

    // Return the returned HRRESULT
    return m_hr;
}

HRESULT CImageTestOutputPin::Active()
{
    // Base class commits the allocator
    HRESULT hr = CBaseOutputPin::Active();
    if (FAILED(hr))
        return hr;
    SetControlEvent(EvActive);
    return S_OK;
}


HRESULT CImageTestOutputPin::Inactive()
{
    SetControlEvent(EvInactive);
    return S_OK;
}

DWORD WINAPI CImageTestOutputPin::ThreadFunction(LPVOID p)
{
    CImageTestOutputPin* pPin = reinterpret_cast<CImageTestOutputPin*>(p);
    pPin->WorkerThread();
    return 0;
}

void CImageTestOutputPin::WorkerThread()
{
    DWORD waitobj;
    while(1)
    {
        // If we are not active or streaming, check for control messages with an infinite wait
        waitobj = WaitForSingleObject(m_hControlEvent, (m_bIsActive && m_bStreaming) ? 0 : INFINITE);

        if (m_Event == EvExit)
        {
            LOG(TEXT("%S::WorkerThread exiting"), __FUNCTION__);
            SetEvent(m_hEventServiced);
            return;
        }

        // If the Active event occured
        if (waitobj == WAIT_ABANDONED)
        {
            LOG(TEXT("CImageTestOutputPin::WorkerThread wait for control event abandoned"));
        }
        else if (waitobj == WAIT_FAILED)
        {
            LOG(TEXT("CImageTestOutputPin::WorkerThread wait for control event failed"));
        }
        else if (waitobj == WAIT_OBJECT_0)
        {
            ProcessControlEvent(m_Event);
        }
        else if (waitobj == WAIT_TIMEOUT)
        {
            // Timeout: do nothing
        }
        else {
            // This should not happen 
            LOG(TEXT("CImageTestOutputPin::WorkerThread wait returned invalid result"));
            return;
        }
        // BUGBUG: currently there is no way to return the error generated by ProcessData back to the application
        // The only action we take upon receiving an error is to stop streaming, send an EOS and print a debug message
        ProcessData();
    }
}



void CImageTestOutputPin::ProcessData()
{
    HRESULT hr = NOERROR;
    IMediaSample* pMediaSample = NULL;

    if (!m_bStreaming || m_bFlushing)
        return;

    // No need to release this - no reference is added
    IVerifier* pVerifier = m_pTestSource->GetVerifier();

    // stop when we have hit nUnits
    if ((m_pTestSource->m_StreamingMode == Discrete) && (m_nSamplesSourced == m_pTestSource->m_nUnits)) {
        LOG(TEXT("CImageTestOutputPin::ProcessData sourcing End Of Stream"));
        m_bStreaming = false;
        if (pVerifier)
            pVerifier->SourceEndOfStream(0);

        hr = DeliverEndOfStream();
        return;
    }

    // for delayed we insert an artifical delay before we deliver the sample
    if (m_pTestSource->m_StreamingMode == Delayed) {
        static DWORD dwPreviousTime = 0;
        DWORD dwCurrentTime = 0;

        // compensate for the time spent delivering the previous sample,
        // by trying to better simulate a timer and not just delaying on
        // top of the delivery time.
        dwCurrentTime = GetTickCount();

        // if we encounter a rollover, deliver this sample
        // a little bit early to recover.
        if(dwPreviousTime < dwCurrentTime)
        {
            // If the DelayStreamTime hasn't been initialized, then the effect is that the
            // first frame is immedialtely delivered without delay.
            if((int)(dwCurrentTime - dwPreviousTime) < m_pTestSource->m_nUnits)
            {
                // if the time hasn't elapsed, then sleep the remaining wait time,
                // cut the time a little bit short because Sleep isn't very accurate and is 
                // guaranteed to return only _after_ the time has elapsed, not exactly when
                // it elapsed. It's accurate enough for what we're doing though.
                Sleep(m_pTestSource->m_nUnits - (dwCurrentTime - dwPreviousTime) - 1);
            }
        }

        dwPreviousTime = GetTickCount();
    }

    int selected = m_pTestSource->GetSelectedMediaTypeCombination();

    // Get the available memory
    REFERENCE_TIME rtAvgTimePerFrame = ((VIDEOINFOHEADER*)(m_mt.pbFormat))->AvgTimePerFrame;
    REFERENCE_TIME rtStartTime;
    REFERENCE_TIME rtEndTime;
    MEMORYSTATUS    memStatus;
    GlobalMemoryStatus(&memStatus );

    // BUGBUG: ad-hoc memory threshold
    if(memStatus.dwMemoryLoad >= 95) {
        // Too little memory left - stop streaming
        LOG(TEXT("CImageTestOutputPin::ProcessData memory load >= 95, %d"), memStatus.dwMemoryLoad);
        hr = E_OUTOFMEMORY;
    }

    // initialize the current frame timestamp if it hasn't been initialized yet.
    // the time stamp starts at the current stream time, and assumes
    // equally spaced samples from then on (plus or minus some lateness)
    if (SUCCEEDED(hr) && (m_rtCurrentFrame == 0 || m_pTestSource->GetSampleLatenessProbability() != 0.0f))
    {
        LONGLONG streamTime = 0;
        hr = m_pTestSource->GetStreamTime(&streamTime);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to get stream time (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);        // Get the current time
        }

        m_rtCurrentFrame = streamTime;
    }
    else m_rtCurrentFrame = m_rtCurrentFrame + rtAvgTimePerFrame;

    // Construct a time-stamped delivery buffer
    if (SUCCEEDED(hr))
    {
        // default to the current time plus 50milliseconds
        rtStartTime = m_rtCurrentFrame + 50000;

        // if we're requesting lateness, then update the start time to reflect
        // the sample lateness.
        if (m_pTestSource->GetSampleLatenessProbability() != 0.0f)
        {
            // Determine lateness of the sample
            LONGLONG rtLateness = 0;
            int randn = rand()%100;
            if (randn <  m_pTestSource->GetSampleLatenessProbability()*100)
            {
                //LOG(TEXT("%S: late frame %d"), __FUNCTION__, m_nSamplesLate);
                rtLateness = 100*rtAvgTimePerFrame;
                m_nSamplesLate++;
                rtStartTime = m_rtCurrentFrame - 10000000 - rtLateness;
            }
        }

        rtEndTime = rtStartTime + rtAvgTimePerFrame;

        // Try to get a delivery buffer. 
        hr = GetDeliveryBuffer(&pMediaSample, &rtStartTime, &rtEndTime, 0);
        if (FAILED(hr))
            LOG(TEXT("CImageTestOutputPin::ProcessData failed to get delivery buffer"));
        else {
            // If we got one, set the sample time
            hr = pMediaSample->SetTime(&rtStartTime, &rtEndTime);
            if (FAILED(hr))
                LOG(TEXT("CImageTestOutputPin::Failed to set the sample time."));
        }
    }

    // Fill the sample buffer with data and deliver it
    if (SUCCEEDED(hr) && pMediaSample)
    {
        // Fill the sample buffer with data
        hr = m_pTestSource->GenerateMediaSample(pMediaSample, this);

        if (SUCCEEDED(hr))
        {
            // the sample must be sourced into the verifier before given to the 
            // test filter. That way when the verifier recieves the output of the
            // filter under test, it knows what the input was.
            if (SUCCEEDED(hr) && (hr != S_FALSE))
            {
                if (pVerifier)
                    pVerifier->SourceSample(pMediaSample, (!m_nSamplesSourced) ? &m_mt : 0);
                m_nSamplesSourced++;
                if (m_nSamplesSourced%100 == 0)
                    LOG(TEXT("%S: delivered sample %d."), __FUNCTION__, m_nSamplesSourced);
            }

            // Call the input pin directly - this can block indefinitely            
            hr = m_pInputPin->Receive(pMediaSample);
            if (FAILED(hr) || (hr == S_FALSE))
            {
                if (hr == S_FALSE) {
                    LOG(TEXT("CImageTestOutputPin::ProcessData IMemInputPin::Receive rejected sample"), hr);
                }
                else if (FAILED(hr)) {
                    LOG(TEXT("CImageTestOutputPin::ProcessData IMemInputPin::Receive returned  0x%x"), hr);
                }
            }
        }
        else {
            LOG(TEXT("CImageTestOutputPin::ProcessData failed to generate media sample"));
        }
    }

    // Release our sample
    if (pMediaSample)
        pMediaSample->Release();

    // If we encounter an error, send an EOS and stop streaming by setting the m_bStreaming flag to false.
    if (FAILED(hr) || (hr == S_FALSE))
    {
        hr = DeliverEndOfStream();
        if (FAILED(hr))
            LOG(TEXT("CImageTestOutputPin::ProcessData DeliverEndOfStream returned failure 0x%x"), hr);
        else {
            // Signal to the verifier that we have sourced an end of stream
            if (pVerifier)
                pVerifier->SourceEndOfStream(0);
        }
        m_bStreaming = false;
    }
}

void CImageTestOutputPin::ProcessControlEvent(ControlEvent event)
{
    switch(event)
    {
    case EvActive: 
        m_bIsActive = true; 
        m_hr = NOERROR;
        break;
    case EvInactive: 
        m_bIsActive = false; 
        m_hr = NOERROR;
        break;
    case EvBeginStreaming:
        // If we are stopped, then can't stream
        if (m_pTestSource->m_State == State_Stopped)
            m_hr = E_FAIL;
        else {
            if (!m_bStreaming)
            {
                m_nSamplesSourced = 0;
                m_bStreaming = true; 
            }
            m_hr = S_OK;
        }
        break;
    case EvEndStreaming: 
        // Fall through
    case EvEndOfStream:
        if (m_bStreaming)
        {
            LOG(TEXT("%S: delivering EOS."), __FUNCTION__);
            m_bStreaming = false;
            IVerifier* pVerifier = m_pTestSource->GetVerifier();
            m_hr = DeliverEndOfStream();
            if (FAILED(m_hr))
                LOG(TEXT("CImageTestOutputPin::ProcessControlEvent DeliverEndOfStream returned failure"));
            else {
                // Signal to the verifier that we have sourced an end of stream
                if (pVerifier)
                    pVerifier->SourceEndOfStream(0);
            }
        }
        else {
            m_hr = S_FALSE;
        }
        break;
    case EvNewSegment:
        LOG(TEXT("%S: delivering NewSeg."), __FUNCTION__);
        // the parameters are just ad-hoc values right now - got to figure out the right values
        m_hr = DeliverNewSegment(0,0,100.0);
        if (FAILED(m_hr))
            LOG(TEXT("CImageTestOutputPin::ProcessControlEvent DeliverNewSegment returned failure"));
        break;
    case EvBeginFlush:
        LOG(TEXT("%S: delivering BeginFlush."), __FUNCTION__);
        m_hr = DeliverBeginFlush();
        if (FAILED(m_hr))
            LOG(TEXT("CImageTestOutputPin::ProcessControlEvent DeliverBeginFlush returned failure"));
        else {
            m_bFlushing = true;
            IVerifier* pVerifier = m_pTestSource->GetVerifier();
            if (pVerifier)
                pVerifier->SourceBeginFlush();
        }
        break;
    case EvEndFlush:
        LOG(TEXT("%S: delivering EndFlush."), __FUNCTION__);
        m_hr = DeliverEndFlush();
        if (FAILED(m_hr))
            LOG(TEXT("CImageTestOutputPin::ProcessControlEvent DeliverEndFlush returned failure"));
        else {
            IVerifier* pVerifier = m_pTestSource->GetVerifier();
            if (pVerifier)
                pVerifier->SourceEndFlush();
        }
        m_bFlushing = false;
        break;
    }
    SetEvent(m_hEventServiced);
}

