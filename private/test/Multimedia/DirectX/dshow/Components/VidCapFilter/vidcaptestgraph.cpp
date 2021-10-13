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
#include <streams.h>
#include <dmodshow.h>
#include <camera.h>
#include <tux.h>

#include "VidCapTestGraph.h"
#include "VidCapTestSink.h"
#include "VidCapTestUids.h"
#include "VidCapVerifier.h"
#include "Verifier.h"
#include "ValidType.h"
#include "CamDriverLibrary.h"
#include "PropertyBag.h"

// Since we can't include TuxMain.h, which would cause a circular inclusion - define these as external
extern void LOG(LPWSTR szFormat, ...);
extern void Debug(LPCTSTR szFormat, ...);

#define rect0 {0, 0, 0, 0}

// RGB565 bitmaps - these need a color table which is essentially a set of bitmasks 
// - so use the VIDEOINFO structure to provide it and specify BI_BITFIELDS in biCompression instead of BI_RGB
static BITMAPINFOHEADER ValidBitmapInfo = {sizeof(BITMAPINFOHEADER), 320, 240, 1, 16, BI_BITFIELDS, 320*240*2, 0, 0, 0, 0};
static VIDEOINFO ValidVideoInfo = {rect0, rect0, 128000, 10, 330000, ValidBitmapInfo, {(BYTE) 0xf800, (BYTE) 0x7e00, (BYTE) 0x001f}};
//BUGBUG: need to make a correct invalid bitmap info structure - setting the dimensions to 0 does not seem to be invalid enough
//static BITMAPINFOHEADER InvalidBitmapInfo = {sizeof(BITMAPINFOHEADER), 320, 240, 1, 16, BI_BITFIELDS, 320*240*2, 0, 0, 0, 0};
//static VIDEOINFO InvalidVideoInfo = {rect0, rect0, 128000, 10, 330000, InvalidBitmapInfo, {0xf800, 0x7e00, 0x001f, 0x0000}};

// Combinations are tuples of <majortype, minortype, formattype, formatsize, formatblockbits> - temporal compression, samplesize, fixedsizsamples are ignored
// These should work for both 2 pin and 3 pin drivers - since they all export the same media type
// For instance, if 2 pin use the Output00 and Output01. If 3 pin use Output02 as well

// invalid
#define Output00 {MEDIATYPE_Video, MEDIASUBTYPE_CLJR, TRUE, FALSE, 0, FORMAT_VideoInfo, NULL, sizeof(VIDEOINFOHEADER), (BYTE*)&ValidVideoInfo}
#define Output01 {MEDIATYPE_Video, MEDIASUBTYPE_CLJR, TRUE, FALSE, 0, FORMAT_VideoInfo, NULL, sizeof(VIDEOINFOHEADER), (BYTE*)&ValidVideoInfo}
#define Output02 {MEDIATYPE_Video, MEDIASUBTYPE_CLJR, TRUE, FALSE, 0, FORMAT_VideoInfo, NULL, sizeof(VIDEOINFOHEADER), (BYTE*)&ValidVideoInfo}

// Invalid - majortype Audio
#define Output10 {MEDIATYPE_Audio, MEDIASUBTYPE_RGB565, TRUE, FALSE, 0, FORMAT_VideoInfo, NULL, sizeof(VIDEOINFOHEADER), (BYTE*)&ValidVideoInfo}
#define Output11 {MEDIATYPE_Audio, MEDIASUBTYPE_RGB565, TRUE, FALSE, 0, FORMAT_VideoInfo, NULL, sizeof(VIDEOINFOHEADER), (BYTE*)&ValidVideoInfo}
#define Output12 {MEDIATYPE_Audio, MEDIASUBTYPE_RGB565, TRUE, FALSE, 0, FORMAT_VideoInfo, NULL, sizeof(VIDEOINFOHEADER), (BYTE*)&ValidVideoInfo}

// Invalid - invalid cbFormat and pbFormat
#define Output20 {MEDIATYPE_Video, MEDIASUBTYPE_RGB565, TRUE, FALSE, 0, FORMAT_VideoInfo, NULL, 0, 0}
#define Output21 {MEDIATYPE_Video, MEDIASUBTYPE_RGB565, TRUE, FALSE, 0, FORMAT_VideoInfo, NULL, 0, 0}
#define Output22 {MEDIATYPE_Video, MEDIASUBTYPE_RGB565, TRUE, FALSE, 0, FORMAT_VideoInfo, NULL, 0, 0}

static AM_MEDIA_TYPE Output[NCOMBINATIONS][NOUTPUTS] = {
    {Output00, Output01, Output02},
    {Output10, Output11, Output12},
    {Output20, Output21, Output22}
};

CTestGraph* CreateVidCapTestGraph()
{
    return new CVidCapTestGraph();
}

CTestGraph* CreateVidCapTestGraphStatic()
{
    static CTestGraph* pTestGraph = NULL;
    if (!pTestGraph)
        pTestGraph = new CVidCapTestGraph();
    return pTestGraph;
}

CVidCapTestGraph::CVidCapTestGraph()
: CTestGraph(GUID_NULL, CLSID_VideoCapture, CLSID_VidCapTestSink, CLSID_VideoCapture)
{
    m_nOutputs = -1;
    IBaseFilter* pVidCapFilter = NULL;

    // Create the filter and query for pin count
    HRESULT hr = CreateVideoCaptureFilter(IID_IBaseFilter, (void**)&pVidCapFilter);
    if (FAILED(hr) || !pVidCapFilter)
        return;
    else {
        // Query the filter for its pin count - use the base class method
        GetPinArray(pVidCapFilter, NULL, &m_nOutputs, PINDIR_OUTPUT);
        // Release the filter we dont need it until it's specifically requested to be created
        pVidCapFilter->Release();

        // Create the media type combinations
        CreateMediaTypeCombinations();
    }
}
    
CVidCapTestGraph::~CVidCapTestGraph()
{
    for(int i = 0; i < NCOMBINATIONS; i++)
    {
        if (m_mtc[i].mtInput)
            delete m_mtc[i].mtInput;
        if (m_mtc[i].mtOutput)
            delete m_mtc[i].mtOutput;
    }
}

HRESULT CVidCapTestGraph::CreateVideoCaptureFilter(REFIID riid, void** ppInterface)
{
    // The Video capture filter needs a driver for it to work
    CAMDRIVERLIBRARY camDriverUtils;
    HRESULT hr = S_OK;

    hr = camDriverUtils.Init();
    if(FAILED(hr))
    {
        LOG(TEXT("CreateVideoCaptureFilter: Failed setting up the camera driver."));
        return hr;
    }
    
    // If the NULL driver is present, use that
    TCHAR* camName = NULL;

    // Get the name of the device
    camName = camDriverUtils.GetDeviceName(0);
    LOG(TEXT("CreateVideoCaptureFilter: using %s as camera device."), camName);

    IBaseFilter* pVidCapFilter = NULL;
    // Create the filter object first
    hr = CoCreateInstance(CLSID_VideoCapture, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&pVidCapFilter);
    if (SUCCEEDED(hr)) {
        // We already have the name of the driver, set it in the property bag
        IPersistPropertyBag* pCapPropertyBag = NULL;
        IPropertyBag * pPropBag=NULL;

        if(SUCCEEDED(hr = CoCreateInstance( CLSID_PropertyBag, NULL, 
                CLSCTX_INPROC_SERVER, IID_IPropertyBag, (LPVOID *)&pPropBag ))) {
            hr = pVidCapFilter->QueryInterface(IID_IPersistPropertyBag, (void**)&pCapPropertyBag);
            if (SUCCEEDED(hr))     {
                VARIANT varCamName;
                VariantInit(&varCamName);
                VariantClear(&varCamName);
                varCamName.vt = VT_BSTR;
                varCamName.bstrVal = SysAllocString(camName);
                // Property for the driver name
                BSTR pszPropName = SysAllocString(TEXT("VCapName"));
                if (!varCamName.bstrVal || !pszPropName)
                    hr = E_OUTOFMEMORY;
                else {
                    if (SUCCEEDED(hr = pPropBag->Write(pszPropName, &varCamName)))
                    {
                        // Now load the property bag and note the result
                        hr = pCapPropertyBag->Load(pPropBag, NULL);
                        if (FAILED(hr))
                            LOG(TEXT("CVidCapTestGraph::CreateVideoCaptureFilter: couldn't load property bag."));
                    }
                    else {
                        LOG(TEXT("CVidCapTestGraph::CreateVideoCaptureFilter: couldn't write to the property bag."));
                    }
                }
                if (pszPropName)
                    SysFreeString(pszPropName);
                if (varCamName.bstrVal)
                    SysFreeString(varCamName.bstrVal);
            }
            else {
                LOG(TEXT("CVidCapTestGraph::CreateVideoCaptureFilter: couldn't get a property bag interface."));
            }
        }
        else {
            LOG(TEXT("CVidCapTestGraph::CreateVideoCaptureFilter: couldn't create the property bag interface."));
            }
        if (pCapPropertyBag)
            pCapPropertyBag->Release();
        if (pPropBag)
            pPropBag->Release();
    }
    else {
        LOG(TEXT("CVidCapTestGraph::CreateVideoCaptureFilter: couldn't creat the video capture filter."));
    }

    
    // If we succeeded loading the property bag, get the interface that was asked for
    if (SUCCEEDED(hr))
        hr = pVidCapFilter->QueryInterface(riid, (void**)ppInterface);

    // Release the video capture filter
    if (pVidCapFilter)
        pVidCapFilter->Release();
    
    return hr;
}

HRESULT CVidCapTestGraph::CreateVidCapTestSink(REFIID riid, void** ppInterface)
{
    // Specify the number of input pins which should be the same as the number of output pins queried from the filter
    HRESULT hr = NO_ERROR;
    CVidCapTestSink* pVidCapTestSink = new CVidCapTestSink(NULL, &hr, m_nOutputs);
    if (pVidCapTestSink)
    {
        hr = pVidCapTestSink->QueryInterface(riid, (void**)ppInterface);
    }
    return hr;
}

HRESULT CVidCapTestGraph::CreateFilter(const GUID clsid, REFIID riid, void** ppInterface)
{
    HRESULT hr = NOERROR;
    if (!ppInterface)
        return E_INVALIDARG;

    if (clsid == CLSID_VidCapTestSink)
    {
        return CreateVidCapTestSink(riid, ppInterface);
    }

    // Override the creation of the Video Capture filter to set the right driver
    else if (clsid == CLSID_VideoCapture)
        return CreateVideoCaptureFilter(riid, ppInterface);
    else {
        // We dont know how to handle the clsid just use the generic CoCreate
        hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, riid, (void**)ppInterface);
    }
    
    return hr;
}

HRESULT CVidCapTestGraph::CreateVerifier(const GUID clsid, IVerifier** ppVerifier)
{
    if (!ppVerifier)
        return E_INVALIDARG;
    if (clsid == CLSID_VideoCapture) {
        *ppVerifier = new CVidCapVerifier();
    }
    else {
        *ppVerifier = new CVerifier();
    }
    return (*ppVerifier) ? S_OK : E_FAIL;
}


int CVidCapTestGraph::GetNumFilterInputPins()
{
    return 0;
}

int CVidCapTestGraph::GetNumFilterOutputPins()
{
    return m_nOutputs;
}

ConnectionOrder CVidCapTestGraph::GetConnectionOrder()
{
    return OutputFirst;
}

void CVidCapTestGraph::CreateMediaTypeCombinations()
{
    // Iterate over media type combinations
    for(int i = 0; i < NCOMBINATIONS; i++)
    {
        m_mtc[i].nInputs = 0;
        m_mtc[i].mtInput = NULL;
        
        m_mtc[i].nOutputs = m_nOutputs;
        m_mtc[i].mtOutput = new AM_MEDIA_TYPE[m_nOutputs];

        if (m_mtc[i].mtOutput) {
            for(int j = 0; j < m_nOutputs; j++)
                m_mtc[i].mtOutput[j] = Output[i][j];
        }
    }
}

MediaTypeCombination* CVidCapTestGraph::GetMediaTypeCombination(int enumerator, bool &bValidCombination)
{
    // BUGBUG: Media type combinations currently not set correctly - because the capture filter expects a color table
    //return NULL;
    if ((enumerator < 0) || (enumerator >= NCOMBINATIONS))
        return NULL;
    
    bValidCombination = (enumerator >= 0) ? false : true;
    return &m_mtc[enumerator];
}

int CVidCapTestGraph::GetNumMediaTypeCombinations()
{
    // BUGBUG: Media type combinations currently not set correctly - because the capture filter expects a color table
    return NCOMBINATIONS;
}

ConnectionValidation CVidCapTestGraph::ValidateMediaTypeCombination(AM_MEDIA_TYPE** ppSourceMediaType, AM_MEDIA_TYPE** ppSinkMediaType)
{
    // ppSinkMediaType needs to be present with 3 entries on the sink
    if (m_nOutputs >= 2)
        ASSERT(!ppSourceMediaType && ppSinkMediaType && ppSinkMediaType[0] && ppSinkMediaType[1]);
    if (m_nOutputs == 3)
        ASSERT(ppSinkMediaType[2]);

    ConnectionValidation validation = Unsupported;
    // The condition for this to succeed is that
    // a. for all pins input is video - YUY2, RGB565, RGB555, YVYU, YV12, RGB24, RGB32 with correct 4cc, bitcount, subtype and majortypes
    
    if (IsValidUncompressedVideoType(ppSinkMediaType[0]) && 
        IsValidUncompressedVideoType(ppSinkMediaType[1]))
    {
        if (m_nOutputs > 2)
            validation = (IsValidUncompressedVideoType(ppSinkMediaType[2])) ? Supported : Unsupported;
        else validation = Supported;
    }
    return validation;
}


int CVidCapTestGraph::GetNumStreamingModes()
{
    return 1;
}

bool CVidCapTestGraph::GetStreamingMode(int enumerator, StreamingMode& mode, int& nUnits, StreamingFlags& flags, TCHAR** datasource)
{
    if (enumerator != 0)
        return false;
    mode = Continuous;
    nUnits = 10000;
    flags = ForSpecifiedTime;
    return true;
}
