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
#include "ImageEncoderTests.h"
#include <windows.h>
#include <stdlib.h>
#include "main.h"
#include "ImagingHelpers.h"
#include <string>
#include <list>
#include <exception>
#include <vector>
#include <gdiplusenums.h>

using namespace ImagingHelpers;

namespace ImageEncoderTests
{
    INT TestInitTerminateEncoder(IImagingFactory* pImagingFactory, IImageEncoder* pImageEncoder, ImageCodecInfo* pici)
    {
        UNREFERENCED_PARAMETER(pImagingFactory);
        IStream* pStream;
        HRESULT hr;
        
        // encoder is initialized by the CreateImageEncoderToStream method
        VerifyHRSuccess(pImageEncoder->TerminateEncoder());
        if (g_fBadParams)
        {
            info(ECHO | INDENT, TEXT("Testing with invalid parameters"));
            __try {
                VerifyHRFailure(pImageEncoder->InitEncoder(NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("InitEncoder excepted with NULL parameter"));
                info(DETAIL, _LOCATIONT);
            }
            info(UNINDENT, NULL);
        }

        // Verify that Init and Terminate work and that they handle the stream
        // correctly.
        info(ECHO | INDENT, TEXT("Testing with valid parameters"));
        info(ECHO, _T("Creating stream"));
        PresetPtr(pStream);
        if (FAILED(hr = CreateStreamOnHGlobal(NULL, TRUE, &pStream)) || IsPreset(pStream))
        {
            info(ABORT, _T("CreateStreamOnHGlobal returned hr = 0x%08x"), hr);
            info(DETAIL, _LOCATIONT);
            return getCode();
        }
        VerifyHRSuccess(pStream->AddRef());

        info(ECHO, _T("Initializing Encoder with valid stream (should succeed)"));
        VerifyHRSuccess(pImageEncoder->InitEncoder(pStream));
        info(DETAIL, _T("Verifying that InitEncoder AddRef'ed the stream"));
        VerifyConditionFalse(pStream->Release() - 2);
        info(ECHO, _T("Terminating Encoder (should succeed)"));
        VerifyHRSuccess(pImageEncoder->TerminateEncoder());

        // Make sure successive calls to terminate encoder do not release the stream again
        info(DETAIL, _T("Verifying that TerminateEncoder Released the stream"));
        VerifyConditionFalse(pStream->AddRef() - 2);
        
        // Return value is unspecified in this case.
        info(ECHO, _T("Calling TerminateEncoder a second time (should not crash)"));
        pImageEncoder->TerminateEncoder();

        info(DETAIL, _T("Verifying that second call to TerminateEncoder did not change stream's reference count"));
        VerifyConditionFalse(pStream->Release() - 1);
        VerifyConditionFalse(pStream->Release());

        
        info(UNINDENT, NULL);
        
        return getCode();
    }
    INT TestGetEncodeSink(IImagingFactory* pImagingFactory, IImageEncoder* pImageEncoder, ImageCodecInfo* pici)
    {
        UNREFERENCED_PARAMETER(pImagingFactory);
        IImageSink* pImageSink;
        if (g_fBadParams)
        {
            info(ECHO | INDENT, TEXT("Testing with invalid parameters"));
            __try {
                VerifyHRFailure(pImageEncoder->GetEncodeSink(NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetEncodeSink excepted with NULL parameter"));
                info(DETAIL, _LOCATIONT);
            }
            info(UNINDENT, NULL);
        }

        // The ImageSink tests handle testing the image sink, so just make sure we get a sink back.
        PresetPtr(pImageSink);
        info(ECHO, _T("Calling GetEncodeSink (should succeed)"));
        VerifyHRSuccess(pImageEncoder->GetEncodeSink(&pImageSink));
        if (!pImageSink || PTRPRESET == pImageSink)
        {
            info(FAIL, TEXT("GetEncodeSink did not return a valid pointer"));
            info(DETAIL, _LOCATIONT);
        }
        else
        {
            info(DETAIL, _T("Verifying GetEncodeSink AddRef'ed"));
            if (VerifyConditionFalse(pImageSink->Release() - 1))
            {
                pImageSink->Release();
            }
        }
        return getCode();
    }
    INT TestSetFrameDimension(IImagingFactory* pImagingFactory, IImageEncoder* pImageEncoder, ImageCodecInfo* pici)
    {
        UNREFERENCED_PARAMETER(pImagingFactory);
        if (g_fBadParams)
        {
            GUID guidInvalid;
            HRESULT hr;
            info(ECHO | INDENT, TEXT("Testing with invalid parameters"));

            for (int iCntr = 0; iCntr < 100; iCntr++)
            {
                GetRandomData((void*) &guidInvalid, sizeof(GUID));
                hr = pImageEncoder->SetFrameDimension(&guidInvalid);
                if (SUCCEEDED(hr) && !g_mapFRAMEDIM[guidInvalid])
                {
                    info(FAIL, TEXT("SetFrameDimension failed with invalid guid: ") _GUIDT, _GUIDEXP(guidInvalid));
                    info(DETAIL, _LOCATIONT);
                }
            }
            info(UNINDENT, NULL);
        }

        info(ECHO | INDENT, _T("Testing with valid parameters"));

        if (IMGFMT_TIFF == pici->FormatID)
        {
            // Any TIFF encoder needs to support frames.
            info(ECHO, _T("TIFF encoder needs to support PAGE dimension"));
            VerifyHRSuccess(pImageEncoder->SetFrameDimension(&FRAMEDIM_PAGE));
        }

        info(ECHO, _T("Calling SetFrameDimension with FRAMEDIM_TIME (should not crash)"));
        pImageEncoder->SetFrameDimension(&FRAMEDIM_TIME);
        info(ECHO, _T("Calling SetFrameDimension with FRAMEDIM_PAGE (should not crash)"));
        pImageEncoder->SetFrameDimension(&FRAMEDIM_PAGE);
        info(ECHO, _T("Calling SetFrameDimension with FRAMEDIM_RESOLUTION (should not crash)"));
        pImageEncoder->SetFrameDimension(&FRAMEDIM_RESOLUTION);
        
        info(UNINDENT, NULL);
        return getCode();
    }

    namespace priv_EncoderParameters
    {
        HRESULT GetBitmapImage(IImagingFactory* pImagingFactory, IImage** ppImage)
        {
            IBitmapImage* pBitmapImage;
            HRESULT hr;
            hr = pImagingFactory->CreateNewBitmap(50, 50, PixelFormat32bppARGB, &pBitmapImage);
            if (FAILED(hr))
            {
                return hr;
            }

            hr = pBitmapImage->QueryInterface(IID_IImage, (void**)ppImage);
            pBitmapImage->Release();
            return hr;
        }
        
        int GetValueSize(int iValueType)
        {
            int iTypeSize = -1;
            switch (iValueType)
            {
            case EncoderParameterValueTypeASCII:
            case EncoderParameterValueTypeByte:
            case EncoderParameterValueTypeUndefined:
                iTypeSize = sizeof(BYTE);
                break;
            case EncoderParameterValueTypeShort:
                iTypeSize = sizeof(SHORT);
                break;
            case EncoderParameterValueTypeLong:
                iTypeSize = sizeof(LONG);
                break;
            case EncoderParameterValueTypeRational:
            case EncoderParameterValueTypeLongRange:
                iTypeSize = 2 * sizeof(LONG);
                break;
            case EncoderParameterValueTypeRationalRange:
                iTypeSize = 4 * sizeof(LONG);
                break;
            }
            return iTypeSize;
        }
        HRESULT RecursiveSetAllParameters(
            IImageEncoder* pImageEncoder, 
            EncoderParameter* pParams, 
            int iParamsLeft, 
            IImage* pImage,
            std::vector<EncoderParameter>& vectParams)
        {
            int iValueSize;
            int iCntr;
            HRESULT hr = S_OK;
            if (iParamsLeft)
            {
                EncoderParameter param;
                param.Guid = pParams->Guid;

                iValueSize = GetValueSize(pParams->Type);
                switch(pParams->Type)
                {
                case EncoderParameterValueTypeByte:
                case EncoderParameterValueTypeLong:
                case EncoderParameterValueTypeShort:
                case EncoderParameterValueTypeRational:
                    param.NumberOfValues = 1;
                    param.Type = pParams->Type;
                    
                    for (iCntr = 0; iCntr < pParams->NumberOfValues && SUCCEEDED(hr); iCntr++)
                    {
                        param.Value = ((BYTE*)pParams->Value) + iCntr * iValueSize;

                        vectParams.push_back(param);
                        hr = RecursiveSetAllParameters(
                            pImageEncoder, 
                            pParams + 1, 
                            iParamsLeft - 1,
                            pImage,
                            vectParams);
                        vectParams.pop_back();
                    }
                    break;
                case EncoderParameterValueTypeASCII:
                    param.NumberOfValues = 1;
                    param.Type = pParams->Type;
                    param.Value = (void*) "test string";
                    vectParams.push_back(param);
                    hr = RecursiveSetAllParameters(
                        pImageEncoder,
                        pParams + 1,
                        iParamsLeft - 1,
                        pImage,
                        vectParams);
                    vectParams.pop_back();
                    break;
                case EncoderParameterValueTypeLongRange:
                    param.NumberOfValues = 1;
                    param.Type = EncoderParameterValueTypeLong;
                    for (iCntr = 0; iCntr < pParams->NumberOfValues && SUCCEEDED(hr); iCntr++)
                    {
                        for (int iRandIters = 0; iRandIters < 10 && SUCCEEDED(hr); iRandIters++)
                        {
                            int iRand;
                            LONG* pRange = ((LONG*)pParams->Value) + iCntr;
                            iRand = RandomInt(pRange[0], pRange[1]);
                            param.Value = &iRand;
                            vectParams.push_back(param);
                            hr = RecursiveSetAllParameters(
                                pImageEncoder,
                                pParams + 1,
                                iParamsLeft - 1,
                                pImage,
                                vectParams);
                            vectParams.pop_back();
                        }
                    }
                    break;
                }

                if (FAILED(hr))
                {
                    info(DETAIL, TEXT("GUID: ") _GUIDT TEXT("; Type (%d); Number of Values (%d); Value (%d)"),
                    _GUIDEXP(param.Guid), param.Type, param.NumberOfValues, *((LONG*)param.Value));
                    
                }
                
                // And call without setting the parameter.
                if (SUCCEEDED(hr))
                {
                    hr = RecursiveSetAllParameters(
                        pImageEncoder,
                        pParams + 1,
                        iParamsLeft - 1,
                        pImage,
                        vectParams);
                }
            }
            else
            {
                IStream* pStream;
                IImageSink* pImageSink;
                EncoderParameters* pParametersToSet = NULL;
                pParametersToSet = (EncoderParameters*)CoTaskMemAlloc(sizeof(UINT) + vectParams.size() * sizeof(EncoderParameter));
                if (!pParametersToSet)
                {
                    info(ABORT, TEXT("Out of memory allocating parameters to set"));
                    info(DETAIL, _LOCATIONT);
                    return E_OUTOFMEMORY;
                }
                pParametersToSet->Count = vectParams.size();
                memcpy(pParametersToSet->Parameter, &*(vectParams.begin()), vectParams.size() * sizeof(EncoderParameter));
                hr = VerifyHRSuccess(CreateStreamOnHGlobal(NULL, TRUE, &pStream));
                if (!pStream)
                {
                    info(ABORT, TEXT("Cannot create stream on which to test (hr = %08x)!"), hr);
                    info(DETAIL, _LOCATIONT);
                    return hr;
                }

                // The encoder is always in the initialized state when passed to us.
                // keep it that way.
                VerifyHRSuccess(pImageEncoder->TerminateEncoder());
                VerifyHRSuccess(pImageEncoder->InitEncoder(pStream));
                VerifyConditionFalse(pStream->Release() - 1);
                if (pParametersToSet->Count)
                    hr = VerifyHRSuccess(pImageEncoder->SetEncoderParameters(pParametersToSet));
                pImageEncoder->SetFrameDimension(&FRAMEDIM_PAGE);
                VerifyHRSuccess(pImageEncoder->GetEncodeSink(&pImageSink));

                // There are too many special cases where the pixel format of the
                // sinking image is related to the parameters, so don't check
                // the result of this call.
                pImage->PushIntoSink(pImageSink);

                VerifyConditionFalse(pImageSink->Release() - 1);
                CoTaskMemFree(pParametersToSet);
            }
            return hr;
        }
    }

    namespace priv_EncoderParameters
    {
        void TestBadParams(IImageEncoder* pImageEncoder)
        {
        }
    }
    INT TestEncoderParameters(IImagingFactory* pImagingFactory, IImageEncoder* pImageEncoder, ImageCodecInfo* pici)
    {
        UINT uiListSize;
        UINT uiListSizeReturned;
        EncoderParameters* pParams = NULL;
        HRESULT hr;
        int iCntr;
        IImage* pImage;
        std::vector<EncoderParameter> vectParams;

        if (g_fBadParams)
        {
            try 
            {
                VerifyHRFailure(pImageEncoder->GetEncoderParameterListSize(NULL));
            } 
            catch (...)
            {
                info(FAIL, TEXT("GetEncoderParameterListSize excepted with NULL parameter"));
                info(DETAIL, _LOCATIONT);
            }
            
            VerifyHRSuccess(pImageEncoder->GetEncoderParameterListSize(&uiListSize));
            pParams = (EncoderParameters*) CoTaskMemAlloc(uiListSize);
            if (!pParams)
            {
                info(ABORT, TEXT("Out of memory: uiListSize = %d"));
                info(DETAIL, _LOCATIONT);
                goto LPExit;
            }
            
            try 
            {
                VerifyHRFailure(pImageEncoder->GetEncoderParameterList(uiListSize, NULL));
            } 
            catch (...)
            {
                info(FAIL, TEXT("GetEncoderParameterListSize excepted with NULL parameter"));
                info(DETAIL, _LOCATIONT);
            }
            
            CoTaskMemFree(pParams);
            pParams = NULL;
            
            try 
            {
                VerifyHRFailure(pImageEncoder->SetEncoderParameters(NULL));
            } 
            catch (...)
            {
                info(FAIL, TEXT("GetEncoderParameterListSize excepted with NULL parameter"));
                info(DETAIL, _LOCATIONT);
            }
        }

        info(ECHO | INDENT, TEXT("Testing with valid parameters."));
        info(ECHO, _T("Calling GetEncoderParameterListSize"));
        hr = pImageEncoder->GetEncoderParameterListSize(&uiListSize);
        if (0 == uiListSize || E_NOTIMPL == hr)
        {
            info(ECHO, TEXT("No parameters supported"));
            goto LPExit;
        }
        else if (FAILED(hr))
        {
            info(FAIL, TEXT("GetEncoderParameterListSize failed, hr = %08x"), hr);
            info(DETAIL, _LOCATIONT);
            goto LPExit;
        }

        pParams = (EncoderParameters*) CoTaskMemAlloc(uiListSize);
        if (!pParams)
        {
            info(ABORT, TEXT("Out of memory: uiListSize = %d"));
            info(DETAIL, _LOCATIONT);
            goto LPExit;
        }

        info(ECHO, _T("Calling GetEncoderParameterList"));
        VerifyHRSuccess(pImageEncoder->GetEncoderParameterList(uiListSize, pParams));

        // Determine the parameter list's size based on the parameters it has.
        info(ECHO, _T("Verifying the size returned by GetEncoderParameterListSize matches the parameter list's actual size"));
        uiListSizeReturned = sizeof(UINT) + pParams->Count * sizeof(EncoderParameter);
        for (iCntr = 0; iCntr < pParams->Count; iCntr++)
        {
            int iTypeSize;
            VerifyConditionFalse(-1 == (iTypeSize = priv_EncoderParameters::GetValueSize(pParams->Parameter[iCntr].Type)));
            uiListSizeReturned += iTypeSize * pParams->Parameter[iCntr].NumberOfValues;
        }

        if (uiListSizeReturned > uiListSize)
        {
            info(FAIL, TEXT("GetEncoderParameterList returned %d bytes of information instead of the indicated %d"),
                uiListSizeReturned, uiListSize);
        }

        info(ECHO, _T("Getting a BitmapImage to encode"));
        VerifyHRSuccess(priv_EncoderParameters::GetBitmapImage(pImagingFactory, &pImage));

        info(ECHO, _T("Setting all of the parameters recursively"));
        priv_EncoderParameters::RecursiveSetAllParameters(
            pImageEncoder,
            pParams->Parameter,
            pParams->Count,
            pImage,
            vectParams);

        info(DETAIL, _T("Verifying correct reference count on image that was being encoded"));
        VerifyConditionFalse(pImage->Release());
LPExit:
        if (pParams)
            CoTaskMemFree(pParams);
        info(UNINDENT, NULL);
        return getCode();
    }
}
