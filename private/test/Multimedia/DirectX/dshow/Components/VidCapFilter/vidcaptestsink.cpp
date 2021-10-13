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
//------------------------------------------------------------------------------
// File: VidCapTestSink.cpp
//
//------------------------------------------------------------------------------

#include <streams.h>
#include <tux.h>
#include "mmImaging.h"
#include "VidCapTestSink.h"
#include "ValidType.h"

// Since we can't include TuxMain.h, which would cause a circular inclusion - define these as external
extern void LOG(LPWSTR szFormat, ...);
extern void Debug(LPCTSTR szFormat, ...);

//
// Setup data for filter registration. This does not have to specify all media types supported.
//
const AMOVIESETUP_MEDIATYPE VidCapTestInputPinTypes[] =
{
    { &MEDIATYPE_Video,
      &MEDIASUBTYPE_NULL }
};

// RGB565 BI_BITFIELD bitmaps
DWORD ColorTable_RGB565[] = {
    0xf800, 0x07e0, 0x001f
};

// These are the media sub types we support
static VideoTypeSpec SupportedMediaTypes[] = {
    {MEDIATYPE_Video, MEDIASUBTYPE_YVYU,   16, FOURCC_YVYU, 330000, 0, NULL},
//    {MEDIATYPE_Video, MEDIASUBTYPE_YUYV,   16, FOURCC_YUYV, 330000, 0, NULL},
    {MEDIATYPE_Video, MEDIASUBTYPE_YUY2,   16, FOURCC_YUY2, 330000, 0, NULL},
    {MEDIATYPE_Video, MEDIASUBTYPE_YV12,   12, FOURCC_YV12, 330000, 0, NULL},
    {MEDIATYPE_Video, MEDIASUBTYPE_YV16,   16, FOURCC_YV16, 330000, 0, NULL},
    {MEDIATYPE_Video, MEDIASUBTYPE_UYVY,   16, FOURCC_UYVY, 330000, 0, NULL},
    {MEDIATYPE_Video, MEDIASUBTYPE_RGB24,  24, BI_RGB, 330000, 0, NULL},
    {MEDIATYPE_Video, MEDIASUBTYPE_RGB32,  32, BI_RGB, 330000, 0, NULL},
    {MEDIATYPE_Video, MEDIASUBTYPE_RGB555, 16, BI_RGB, 330000},
    {MEDIATYPE_Video, MEDIASUBTYPE_RGB565, 16, BI_BITFIELDS, 330000, sizeof(ColorTable_RGB565), (BYTE*)&ColorTable_RGB565[0]},
    {MEDIATYPE_Video, MEDIASUBTYPE_IJPG, 24, FOURCC_JPEG, 330000},
};

static const int nSupportedMediaTypes = sizeof(SupportedMediaTypes)/sizeof(SupportedMediaTypes[0]);

static AM_MEDIA_TYPE** MediaTypeInput;

const AMOVIESETUP_PIN VidCapTestInputPinDesc =
{ L"Input"          // strName
, FALSE              // bRendered
, FALSE               // bOutput
, FALSE              // bZero
, FALSE              // bMany
, &CLSID_NULL        // clsConnectsToFilter
, L"Input"           // strConnectsToPin
, 1                  // nTypes
, VidCapTestInputPinTypes };  // lpTypes

const AMOVIESETUP_FILTER VidCapTestSinkDesc =
{ &CLSID_VidCapTestSink              // clsID
, L"VidCap Test Sink"  // strName
, MERIT_UNLIKELY                  // dwMerit
, MAX_VID_SINK_PINS                               // nPins
, &VidCapTestInputPinDesc };                    // lpPin

// Filter factory method
CUnknown * WINAPI CVidCapTestSink::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
    ASSERT(phr);

    //  DLLEntry does the right thing with the return code and
    //  the returned value on failure

    return new CVidCapTestSink(pUnk, phr, 2);
}

// Filter methods
CVidCapTestSink::CVidCapTestSink(LPUNKNOWN pUnk, HRESULT* phr, int nInputs) :
    CBaseFilter(TEXT("VidCap Test Sink"), pUnk, &m_Lock, CLSID_VidCapTestSink)
{
    // Init to NULL
    m_pVerifier = NULL;

    // Create input pins
    m_nInputPins = nInputs;

    memset(m_pInputPin, 0, sizeof(m_pInputPin));
    for(int i = 0; i < m_nInputPins; i++) {
        m_pInputPin[i] = new CVidCapTestInputPin(phr, this, &m_Lock, i);
        if (!m_pInputPin[i]) {
            *phr = E_OUTOFMEMORY;
            break;
        }
    }

    if (FAILED(*phr)) {
        LOG(TEXT("CVidCapTestSink:: failed to create input pins."));
        for(int i = 0; i < m_nInputPins; i++) {
            if (m_pInputPin[i])
                delete m_pInputPin[i];
        }
        return;
    }

    m_SelectedMediaTypeCombination = -1;
    for(int i = 0; i < m_nInputPins; i++)
        m_SelectedMediaType[i] = -1;

    // Setup input media type combinations
    MediaTypeInput = new PAM_MEDIA_TYPE[nSupportedMediaTypes];

    for(int i = 0; i < nSupportedMediaTypes; i++)
    {
        AM_MEDIA_TYPE *pMediaType = new AM_MEDIA_TYPE;

        // These are standard for the YUV and RGB that we deal with
        pMediaType->bFixedSizeSamples = true;
        pMediaType->bTemporalCompression = false;
        pMediaType->pUnk = NULL;
        pMediaType->formattype = FORMAT_VideoInfo;
        pMediaType->pbFormat = (BYTE*) new VIDEOINFOHEADER;
        pMediaType->cbFormat = sizeof(VIDEOINFOHEADER);
        
        VIDEOINFOHEADER* pVIH = (VIDEOINFOHEADER*)pMediaType->pbFormat;
        pVIH->AvgTimePerFrame = 33000; //~30 frames per sec
        pVIH->dwBitErrorRate = 0;
        pVIH->dwBitRate = 0;
        RECT rect0 = {0,0,0,0};
        pVIH->rcSource = rect0;
        pVIH->rcTarget = rect0;
        
        pVIH->bmiHeader.biClrImportant = 0;
        pVIH->bmiHeader.biClrUsed = 0;
        if(SupportedMediaTypes[i].subtype == MEDIASUBTYPE_YV12 || SupportedMediaTypes[i].subtype == MEDIASUBTYPE_YV16)
            pVIH->bmiHeader.biPlanes = 3;
        else        
            pVIH->bmiHeader.biPlanes = 1;
        pVIH->bmiHeader.biXPelsPerMeter = 0;
        pVIH->bmiHeader.biYPelsPerMeter = 0;
        pVIH->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    
        // Type specific
        pMediaType->majortype = SupportedMediaTypes[i].majortype;
        pMediaType->subtype = SupportedMediaTypes[i].subtype;
        pVIH->bmiHeader.biBitCount = SupportedMediaTypes[i].bitcount;
        pVIH->bmiHeader.biCompression = SupportedMediaTypes[i].compression;

        // Not sure what we can set for the bitmap width and height and sample size
        pVIH->bmiHeader.biWidth = 320;
        pVIH->bmiHeader.biHeight = 240;

        // What size buffer?
        pVIH->bmiHeader.biSizeImage = AllocateBitmapBuffer(pVIH->bmiHeader.biWidth, pVIH->bmiHeader.biHeight, pVIH->bmiHeader.biBitCount, pVIH->bmiHeader.biCompression, NULL);
        pMediaType->lSampleSize = pVIH->bmiHeader.biSizeImage;

        // Set the media type
        MediaTypeInput[i] = pMediaType;
    }
}

CVidCapTestSink::~CVidCapTestSink()
{
    if (MediaTypeInput)
    {
        for (int i = 0; i < nSupportedMediaTypes; i++)
        {
            // Free the format block first
            if (MediaTypeInput[i]) {
                if (MediaTypeInput[i]->pbFormat)
                    delete MediaTypeInput[i]->pbFormat;
                delete MediaTypeInput[i];
            }
        }

        delete [] MediaTypeInput;
    }

    for(int i = 0; i < m_nInputPins; i++)
    {
        if (m_pInputPin[i])
            delete m_pInputPin[i];
    }
}

STDMETHODIMP CVidCapTestSink::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
    CheckPointer(ppv,E_POINTER);

    if(riid == IID_IVidCapTestSink)
        return GetInterface((IVidCapTestSink*)this, ppv);
    else if (riid == IID_ITestSink)
        return GetInterface((ITestSink*)this, ppv);
    else if( riid == IID_IAMFilterMiscFlags )
        return GetInterface((IAMFilterMiscFlags*)this, ppv);
    else
        return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
}

int CVidCapTestSink::GetPinCount()
{
    return m_nInputPins;
}

CBasePin *CVidCapTestSink::GetPin(int n)
{
    if ((n < 0) || (n >= m_nInputPins))
        return NULL;

    return m_pInputPin[n];    
}

bool CVidCapTestSink::SetInvalidConnectionParameters()
{
    return false;
}

bool CVidCapTestSink::SetInvalidAllocatorParameters()
{
    return false;
}

// ITestSink Implementation

// Media type related functionality
int CVidCapTestSink::GetNumMediaTypeCombinations()
{
    // We have more than 1 pin but we assume that all of them will have the same type - 
    // of course they dont have to but this seems like a good starting point
    // If they can have different media types then the number of combinations will explode - N^3
    return nSupportedMediaTypes;
}

HRESULT CVidCapTestSink::SelectMediaTypeCombination(int iPosition)
{
    // a requested position of -1 means use all possible media types
    if ((iPosition < -1) || (iPosition >= nSupportedMediaTypes))
        return E_INVALIDARG;
    m_SelectedMediaTypeCombination = iPosition;
    return S_OK;
}

int CVidCapTestSink::GetSelectedMediaTypeCombination()
{
    return m_SelectedMediaTypeCombination;
}

HRESULT CVidCapTestSink::GetMediaTypeCombination(int iPosition, AM_MEDIA_TYPE** ppMediaType)
{
    return S_FALSE;
#if 0
    if ((iPosition < 0) || (iPosition >= nSupportedMediaTypes))
        return E_INVALIDARG;

    if (!ppMediaType)
        return E_POINTER;

    for(int i = 0; i < m_nInputPins; i++) {
        if (!ppMediaType[i])
            return E_POINTER;
        CopyMediaType(ppMediaType[i], MediaTypeInput[iPosition]);
    }
    return S_OK;
#endif
}

HRESULT CVidCapTestSink::SetVerifier(IVerifier* pVerifier)
{
    // the verifier passed in must always be a vidcapverifier, not an iverifier.
    m_pVerifier = (CVidCapVerifier *)pVerifier;
    return S_OK;
}

IVerifier* CVidCapTestSink::GetVerifier()
{
    return m_pVerifier;
}

CVidCapVerifier *CVidCapTestSink::GetVidCapVerifier()
{
    return m_pVerifier;
}


ULONG
CVidCapTestSink::GetMiscFlags( void )
{
    return AM_FILTER_MISC_FLAGS_IS_RENDERER;
}

// The Input Pin implementation
CVidCapTestInputPin::CVidCapTestInputPin(HRESULT * phr, CVidCapTestSink* pFilter, CCritSec* pLock, int index) : 
      CBaseInputPin(NAME("VidCap Test Input Pin"), pFilter, pLock, phr, L"Input")
{
    m_pLock = pLock;
    m_pTestSink = pFilter;
    m_Index = index;
    m_bFirstSampleDelivered = FALSE;
}

CVidCapTestInputPin::~CVidCapTestInputPin()
{
}

STDMETHODIMP CVidCapTestInputPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    return CBaseInputPin::NonDelegatingQueryInterface(riid, ppv);
}


HRESULT CVidCapTestInputPin::GetMediaType(int iPosition, CMediaType *pMediaType)
{
    return S_FALSE;
#if 0
    CAutoLock lock(m_pLock);
    if (!pMediaType)
        return E_POINTER;

    if (iPosition < 0) {
        return E_INVALIDARG;
    }

    // If there has been a selection just use that one
    int selected = m_pTestSink->GetSelectedMediaTypeCombination();
    int nMediaTypes = (selected == -1) ? nSupportedMediaTypes : 1;

    if (iPosition >= nMediaTypes) {
        return VFW_S_NO_MORE_ITEMS;
    }

    int iActualPosition = (selected == -1) ? iPosition : selected;

    *pMediaType = *MediaTypeInput[iActualPosition];
    return S_OK;
#endif
}

HRESULT CVidCapTestInputPin::CheckMediaType(const CMediaType *pMediaType)
{
    CheckPointer(pMediaType,E_POINTER);

    // Even though there are three pins, the simplifying assumption is that in each selected combination, 
    // each of the three pins would have the same media type. This is limiting but ok for now.
    int selected = m_pTestSink->GetSelectedMediaTypeCombination();

    bool accept = false;

    for(int i = 0; i < nSupportedMediaTypes; i++)
    {
        if ((selected != i) && (selected != -1))
            continue;
        
        if (*pMediaType->Type() != MediaTypeInput[i]->majortype)
        {
            OutputDebugString(TEXT("CVidCapTestInputPin::CheckMediaType: Major type did not agree."));
        }
        else if (*pMediaType->Subtype() != MediaTypeInput[i]->subtype)
        {
            OutputDebugString(TEXT("CVidCapTestInputPin::CheckMediaType: Sub type did not agree."));
        }
        else if (*pMediaType->FormatType() != MediaTypeInput[i]->formattype)
        {
            OutputDebugString(TEXT("CVidCapTestInputPin::CheckMediaType: Format type did not agree."));
        }
        else {
            // So far the Major/Minor and Format types are ok - check the format block
            VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER*) pMediaType->Format();
            VIDEOINFOHEADER *pRefVih = (VIDEOINFOHEADER*)MediaTypeInput[i]->pbFormat;
            if (pVih && pRefVih) {
                if (!IsValidVideoInfoHeader(pVih) ||
                    !IsEqualVideoInfoHeaderNonStrict(pVih, pRefVih))
                {
                    OutputDebugString(TEXT("CVidCapTestInputPin::CheckMediaType: Format block did not agree."));
                }
                else {
                    OutputDebugString(TEXT("CVidCapTestInputPin::CheckMediaType: agreed on a media type."));
                    accept = true;
                    break;
                }
            }
        }
    }
            
    return (accept) ? S_OK : S_FALSE;
}


STDMETHODIMP CVidCapTestInputPin::Receive(IMediaSample* pSample)
{
    IVerifier* pVerifier = m_pTestSink->GetVerifier();
    if (pVerifier && m_pTestSink->IsActive())
        pVerifier->SinkSample(pSample, (m_bFirstSampleDelivered) ? NULL: &m_mt, m_Index);

    m_bFirstSampleDelivered = true;
    return NOERROR;
}

STDMETHODIMP CVidCapTestInputPin::EndOfStream()
{
    IVerifier* pVerifier = m_pTestSink->GetVerifier();
    if (pVerifier)
        pVerifier->SinkEndOfStream(m_Index);
    return NOERROR;
}

STDMETHODIMP CVidCapTestInputPin::BeginFlush(void)
{
    IVerifier* pVerifier = m_pTestSink->GetVerifier();
    if (pVerifier)
        pVerifier->SinkBeginFlush(m_Index);
    return NOERROR;
}

STDMETHODIMP CVidCapTestInputPin::EndFlush(void)
{
    IVerifier* pVerifier = m_pTestSink->GetVerifier();
    if (pVerifier)
        pVerifier->SinkEndFlush(m_Index);
    return NOERROR;
}

