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
#include "StdAfx.h"
#include "sourcefiltergraphEx.h"
#include "SinkfiltergraphEx.h"
#include "Helper.h"
#include "logger.h"
#include <atlbase.h>

bool CSourceFilterGraphEx::Ex_flag;
UINT32 CSourceFilterGraphEx::Ex_iCount;

CSourceFilterGraphEx::CSourceFilterGraphEx()
{
    Ex_flag = true;
    Ex_iCount = 0;
}

CSourceFilterGraphEx::~CSourceFilterGraphEx(void)
{
}

HRESULT CSourceFilterGraphEx::SequentialStampDisp(IMediaSample *pSample, REFERENCE_TIME *StartTime, REFERENCE_TIME *StopTime, BOOL TypeChanged, AM_MEDIA_TYPE *mtype, LPVOID lpUser)
{
    const TCHAR * pszFnName = TEXT("CSourceFilterGraphEx::SequentialStampDisp");

    //CComPtr<IMediaSample> pComSample = pSample;
    int actlen = pSample->GetActualDataLength();
    BYTE * pdata = NULL;
    UINT32 iThisStamp = 0;
    // Code
    pSample->GetPointer(&pdata);
    if(pdata == NULL)
    {
        LogError(__LINE__, pszFnName, TEXT("Failed to get Sample Pointer. fail"));
        return E_FAIL;
    }
    iThisStamp = *((UINT32*)(pdata + actlen - 4));
    CSourceFilterGraphEx::Ex_iCount = iThisStamp;
    LogText(__LINE__, pszFnName, TEXT("Source Frame Stamp is %d; Sink at Frame %d"), iThisStamp, CSinkFilterGraphEx::Ex_iCount);
    return S_OK;
}


HRESULT CSourceFilterGraphEx::GetCurrentFrameStamp(UINT32 * uiStamp)
{
    *uiStamp = CSourceFilterGraphEx::Ex_iCount;
    return S_OK;
}


HRESULT CSourceFilterGraphEx::SequentialTempStampChecker(IMediaSample *pSample, REFERENCE_TIME *StartTime, REFERENCE_TIME *StopTime, BOOL TypeChanged, AM_MEDIA_TYPE *mtype, LPVOID lpUser)
{
    const TCHAR * pszFnName = TEXT("CSourceFilterGraphEx::SequentialTempStampChecker");
    static UINT32 iLastStamp;
    static UINT32 iThisStamp;
    static bool firsttime = true;

    //CComPtr<IMediaSample> pComSample = pSample;
    int actlen = pSample->GetActualDataLength();
    BYTE * pdata = NULL;
    pSample->GetPointer(&pdata);
    if(pdata == NULL)
    {
        LogError(__LINE__, pszFnName, TEXT("Failed to get Sample Pointer. fail"));
        return E_FAIL;
    }
    iThisStamp = *((UINT32*)(pdata + actlen - 4));
    Ex_iCount = iThisStamp;
    if(firsttime)
    {
        firsttime = false;
        LogText(__LINE__, pszFnName, TEXT("First Frame start from %d"), iThisStamp);
        iLastStamp = iThisStamp;
    }
    else
    {
        if(iThisStamp != iLastStamp + 1)
        {
            Ex_flag = false;
            LogWarning(__LINE__, pszFnName, TEXT("Stamp break. This stamp is %d, Last stamp is %d"), iThisStamp, iLastStamp);
            iLastStamp = iThisStamp;
        }
        else
        {
            iLastStamp = iThisStamp;
            if(iThisStamp % 30 == 0)
                LogText(__LINE__, pszFnName, TEXT("Stamp till %d are verified and valid"), iThisStamp);
        }
    }
    return S_OK;
}


HRESULT CSourceFilterGraphEx::SequentialStampNullCounter(IMediaSample *pSample, REFERENCE_TIME *StartTime, REFERENCE_TIME *StopTime, BOOL TypeChanged, AM_MEDIA_TYPE *mtype, LPVOID lpUser)
{
    const TCHAR * pszFnName = TEXT("CSourceFilterGraphEx::SequentialStampNullCounter");

    //CComPtr<IMediaSample> pComSample = pSample;
    BYTE * pdata = NULL;
    int actlen = pSample->GetActualDataLength();
    pSample->GetPointer(&pdata);
    if(pdata == NULL)
    {
        LogError(__LINE__, pszFnName, TEXT("Failed to get Sample Pointer. fail"));
        return E_FAIL;
    }
    CSourceFilterGraphEx::Ex_iCount = *((UINT32*)(pdata + actlen - 4));
    return S_OK;
}


HRESULT CSourceFilterGraphEx::NullCounterNoStamp(IMediaSample *pSample, REFERENCE_TIME *StartTime, REFERENCE_TIME *StopTime, BOOL TypeChanged, AM_MEDIA_TYPE *mtype, LPVOID lpUser)
{
    CSourceFilterGraphEx::Ex_iCount += 1;
    return S_OK;
}


HRESULT CSourceFilterGraphEx::SequentialPermStampChecker(IMediaSample *pSample, REFERENCE_TIME *StartTime, REFERENCE_TIME *StopTime, BOOL TypeChanged, AM_MEDIA_TYPE *mtype, LPVOID lpUser)
{
    const TCHAR * pszFnName = TEXT("CSourceFilterGraphEx::SequentialPermStampChecker");
    static UINT32 iLastStamp;
    static UINT32 iThisStamp;
    static bool firsttime = true;

    //CComPtr<IMediaSample> pComSample = pSample;
    int actlen = pSample->GetActualDataLength();
    BYTE * pdata = NULL;
    pSample->GetPointer(&pdata);
    if(pdata == NULL)
    {
        LogError(__LINE__, pszFnName, TEXT("Failed to get Sample Pointer. fail"));
        return E_FAIL;
    }
    iThisStamp = *((UINT32*)(pdata + actlen - 4));
    Ex_iCount = iThisStamp;
    if(firsttime)
    {
        firsttime = false;
        LogText(__LINE__, pszFnName, TEXT("First Frame start from %d"), iThisStamp);
        if(iThisStamp > 5)
        {
            LogError(__LINE__, pszFnName, TEXT("First Frame does not start from beginning, wrong"));
            Ex_flag = false;
        }
        iLastStamp = iThisStamp;
    }
    else
    {
        if(iThisStamp != iLastStamp + 1)
        {
            Ex_flag = false;
            LogWarning(__LINE__, pszFnName, TEXT("Stamp break. This stamp is %d, Last stamp is %d"), iThisStamp, iLastStamp);
            iLastStamp = iThisStamp;
        }
        else
        {
            iLastStamp = iThisStamp;
            if(iThisStamp % 30 == 0)
                LogText(__LINE__, pszFnName, TEXT("Stamp till %d are verified and valid"), iThisStamp);
        }
    }
    return S_OK;
}

HRESULT CSourceFilterGraphEx::SetupDemuxDirectPlayBackFromFile(LPCOLESTR pszWMVFileName)
{
    const TCHAR * pszFnName = TEXT("CSourceFilterGraphEx::SetupDemuxDirectPlayBackFromFile");
    CComPtr<IBaseFilter> pBaseFilterDemux = NULL;
    CComPtr<IBaseFilter> pBaseFilterEM10 = NULL;
    CComPtr<IBaseFilter> pBaseFilterGrabber = NULL;
    CComPtr<IFileSourceFilter> pFileSource = NULL;

    CComPtr<IPin> pPinAudio = NULL;
    CComPtr<IPin> pPinVideo = NULL;

    HRESULT hr = this->AddFilterByCLSID(CLSID_Babylon_EM10, L"EM10 source filter", &pBaseFilterEM10);
    if(SUCCEEDED(hr))
    {
        if(pszWMVFileName != NULL)
        {
            hr = this->FindInterface(IID_IFileSourceFilter, (void **)&pFileSource);
            if(SUCCEEDED(hr))
            {
                hr = pFileSource->Load(pszWMVFileName, NULL);
                if(!SUCCEEDED(hr))
                {
                    LogError(__LINE__, pszFnName, TEXT("Failed at load file %s."), pszWMVFileName);
                    return hr;
                }

            }
            else
            {
                LogError(__LINE__, pszFnName, TEXT("Failed at Find IFileSource Interface."));
                return hr;
            }
        }

        hr = this->AddFilterByCLSID(CLSID_myGrabberSample, TEXT("Grabber Filter"), &pBaseFilterGrabber);
        if(SUCCEEDED(hr))
        {

            hr = this->AddFilterByCLSID(CLSID_Demux, L"Demux filter", &pBaseFilterDemux);

            if(SUCCEEDED(hr))
            {
                hr = this->ConnectFilters(pBaseFilterEM10, pBaseFilterGrabber);
                if(SUCCEEDED(hr))
                {
                    hr = this->ConnectFilters(pBaseFilterGrabber, pBaseFilterDemux);

                    if(SUCCEEDED(hr))
                    {
                        pPinAudio = this->GetPinByName(pBaseFilterDemux, L"Audio");
                        pPinVideo = this->GetPinByName(pBaseFilterDemux, L"Video");
                        if(pPinAudio == NULL || pPinVideo == NULL)
                        {
                            LogError(__LINE__, pszFnName, TEXT("Failed to Get Demux Audio and Video Pins"));
                            return E_FAIL;
                        }
                        else
                        {
                            hr = this->RenderPin(pPinAudio);
                            if(SUCCEEDED(hr))
                            {
                                hr = this->RenderPin(pPinVideo);
                                if(SUCCEEDED(hr))
                                {
                                    return hr;
                                }
                                else
                                {
                                    LogError(__LINE__, pszFnName, TEXT("Failed to render video Pin"));
                                    return hr;
                                }

                            }
                            else
                            {
                                LogError(__LINE__, pszFnName, TEXT("Failed to render Audio Pin"));
                                return hr;
                            }
                        }
                    }
                    else
                    {
                        LogError(__LINE__, pszFnName, TEXT("Failed at connecting Grabber and Demux"));
                        return hr;
                    }
                }
                else
                {
                    LogError(__LINE__, pszFnName, TEXT("Failed connect Source and Grabber"));
                    return hr;
                }
            }
            else
            {
                LogError(__LINE__, pszFnName, TEXT("Failed at inserting Demux"));
                return hr;
            }
        }
        else
        {
            LogError(__LINE__, pszFnName, TEXT("Failed at inserting Grabber"));
            return hr;
        }

    }
    else
    {
        LogError(__LINE__, pszFnName, TEXT("Failed at Add Em10 Filter."));
        return hr;
    }
}


HRESULT CSourceFilterGraphEx::SetGrabberCallback(SAMPLECALLBACK callback)
{
    CComPtr<IGrabberSample> pGrabber;
    HRESULT hr = this->FindInterface(__uuidof(IGrabberSample), (void **)&pGrabber);
    if(SUCCEEDED(hr))
    {
        if(callback != NULL)
        {
            hr = pGrabber->SetCallback(callback, NULL);
        }
    }
    return hr;
}

