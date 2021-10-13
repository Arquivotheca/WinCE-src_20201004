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
#include "sinkfiltergraphEx.h"
#include "Helper.h"
#include "logger.h"
#include <atlbase.h>


// the following is actually the clsid of asf source filter
//const CLSID CLSID_EM10_FILTER     = {0xba46b749, 0x97d8, 0x4811, {0xbd, 0x99, 0x56, 0x9b, 0xcd, 0xc4, 0x7e, 0xd2}};
//#ifdef USE_ASF_LOOPER
//const CLSID CLSID_ASF_LOOPER      = {0xe270e43c, 0xd1b0, 0x4222, {0x9c, 0x70, 0x67, 0x30, 0x0b, 0x2d, 0x7a, 0x0a}};
//#endif
//const CLSID CLSID_DVR_SINK_FILTER = {0x8255F4E7, 0x1EC2, 0X4EE0, {0XB9, 0XD5, 0X5B, 0X57, 0X69, 0XF5, 0X7C, 0X30}};

bool CSinkFilterGraphEx::Ex_flag;
UINT32 CSinkFilterGraphEx::Ex_iCount;

CSinkFilterGraphEx::CSinkFilterGraphEx()
{
    Ex_flag = true;
    Ex_iCount = 0;
}

CSinkFilterGraphEx::~CSinkFilterGraphEx(void)
{
}

HRESULT CSinkFilterGraphEx::SequentialStamper(IMediaSample *pSample, REFERENCE_TIME *StartTime, REFERENCE_TIME *StopTime, BOOL TypeChanged, AM_MEDIA_TYPE *mtype, LPVOID lpUser)
{
    BYTE * pdata = NULL;
    int actlen = pSample->GetActualDataLength();
    pSample->GetPointer(&pdata);
    if(pdata == NULL)
    {
        return E_FAIL;
    }
    *((UINT32*)(pdata+actlen-4)) = Ex_iCount;
    Ex_iCount++;
    return S_OK;
}

HRESULT CSinkFilterGraphEx::SimpleCountDisp(IMediaSample *pSample, REFERENCE_TIME *StartTime, REFERENCE_TIME *StopTime, BOOL TypeChanged, AM_MEDIA_TYPE *mtype, LPVOID lpUser)
{
    Ex_iCount++;
    if(Ex_iCount%30 == 0)
        LogText(__LINE__, TEXT("CSinkFilterGraphEx::SimpleCountDisp"), TEXT("Frame %d written"), Ex_iCount);
    return S_OK;
}


HRESULT CSinkFilterGraphEx::SetGrabberCallback(SAMPLECALLBACK callback)
{
    CComPtr<IGrabberSample> pGrabber;
    HRESULT hr = this->FindInterface(__uuidof(IGrabberSample), (void **)&pGrabber);
    if(SUCCEEDED(hr))
    {
        hr = pGrabber->SetCallback(callback, NULL);
    }
    return hr;
}

