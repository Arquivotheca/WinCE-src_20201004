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
#include "ImageDecoderTests.h"
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
using namespace ConfigUty;

namespace ImageDecoderTests
{
    namespace priv_ImageDecoder
    {
        // CreateImageSink is used by the decoder tests to get a sink that can
        // be sunk to. It returns a bitmap image's sink (in the correct size and
        // pixel format).
        HRESULT CreateImageSink(IImagingFactory * pImagingFactory, const CImageDescriptor * pid, IImageSink ** ppImageSink)
        {
            IBitmapImage * pBitmapImage;
            HRESULT hr = S_OK;
            PresetPtr(pBitmapImage);
            hr = pImagingFactory->CreateNewBitmap(pid->GetWidth(), pid->GetHeight(), PixelFormat32bppARGB, &pBitmapImage);
            if (!pBitmapImage || PTRPRESET == pBitmapImage)
            {
                info(DETAIL, _FT("CreateNewBitmap failed, hr = %08x"), hr);
                return FAILED(hr) ? hr : E_FAIL;
            }
            
            PresetPtr(*ppImageSink);
            hr = pBitmapImage->QueryInterface(IID_IImageSink, (void**)ppImageSink);
            if (!*ppImageSink || PTRPRESET == *ppImageSink)
            {
                info(DETAIL, _FT("QueryInterface for ImageSink failed, hr = %08x"), hr);
            }

            pBitmapImage->Release();
            
            return hr;
        }
    }

    INT TestInitTerminateDecoder(IImagingFactory * pImagingFactory, IImageDecoder * pImageDecoder, const CImageDescriptor & id)
    {
        IStream * pStream = NULL;
        // Start with a clean slate (the decoder is passed in after being init'ed
        VerifyHRSuccess(pImageDecoder->TerminateDecoder());

        if (g_fBadParams)
        {
            // Do some bad parameter testing.
            info(ECHO | INDENT, TEXT("Testing with invalid parameters"));
            info(DETAIL, _T("Calling InitDecoder with NULL stream pointer"));
            try 
            {
                VerifyHRFailure(pImageDecoder->InitDecoder(NULL, DecoderInitFlagNone));
            } 
            catch (...)
            {
                info(FAIL, TEXT("InitDecoder excepted with NULL stream pointer"));
                info(DETAIL, _LOCATIONT);
            }
            info(UNINDENT, NULL);
        }

        info(ECHO | INDENT, TEXT("Testing with valid parameters"));

        // TerminateDecoder should not crash if the decoder hasn't been init'ed.
        // Return value in this case is unspecified.
        info(ECHO, _T("Calling TerminateDecoder when decoder is not initialized"));
        pImageDecoder->TerminateDecoder();

        // Get a stream on the image file for use with init
        pStream = id.GetStream();
        if (!pStream)
        {
            info(ABORT, TEXT("Cannot test without valid stream: \"%s\""),
                id.GetFilename().c_str());
            info(DETAIL, _LOCATIONT);
            goto LPExit;
        }
        
        // InitDecoder should succeed when not currently init'ed
        info(ECHO, _T("Calling InitDecoder with valid image stream (should succeed)"));
        VerifyHRSuccess(pImageDecoder->InitDecoder(pStream, DecoderInitFlagNone));

        // InitDecoder should AddRef the stream (bringing to 2 refs, addref again for
        // 3 refs total).
        info(DETAIL, _T("Checking stream reference count"));
        VerifyConditionFalse(pStream->AddRef() - 3);

        // InitDecoder should fail if already init'ed
        info(ECHO, _T("Calling InitDecoder a second time (should fail)"));
        VerifyHRFailure(pImageDecoder->InitDecoder(pStream, DecoderInitFlagNone));

        // The second call should not have affected the pStream (will release, bringing
        // to 2 refs total).
        info(DETAIL, _T("Checking stream reference count"));
        VerifyConditionFalse(pStream->Release() - 2);

        // Now that we're init'ed, TerminateDecoder should succeed.
        info(ECHO, _T("Calling TerminateDecoder (should succeed)"));
        VerifyHRSuccess(pImageDecoder->TerminateDecoder());

        // Make sure the TerminateDecoder call released a reference to the stream.
        // (start with 1, init to 2, addref to 3, release to 2, terminate to 1, 
        // addref to 2)
        info(DETAIL, _T("Checking stream reference count"));
        PREFAST_ASSUME(pStream);
        VerifyConditionFalse(pStream->AddRef() - 2);

        // We are not initialized now, so a second call to TerminateDecoder should
        // not crash and not release the stream again. The return value is
        // unspecified in this case.
        info(ECHO, _T("Calling TerminateDecoder a second time (nothing should happen)"));
        pImageDecoder->TerminateDecoder();

        // The second call to TerminateDecoder should in no way affect the stream.
        // (was at 2 before the second call to TerminateDecoder, release to 1)
        info(DETAIL, _T("Checking stream reference count"));
        if (!VerifyConditionFalse(pStream->Release() - 1))
        {
            // Only release a second time if the second call to TerminateDecoder
            // correctly didn't release, otherwise can cause some heap corruption
            pStream->Release();
        }
        info(UNINDENT, NULL);
        
LPExit:
        return getCode();
    }
    
    INT TestBeginEndDecode(IImagingFactory * pImagingFactory, IImageDecoder * pImageDecoder, const CImageDescriptor & id)
    {
        IImageSink * pImageSink = NULL;
        IImageSink * pImageSink2 = NULL;
        HRESULT hr;
        
        if (g_fBadParams)
        {
            info(ECHO | INDENT, TEXT("Testing with invalid parameters"));
            __try {
                VerifyHRFailure(pImageDecoder->BeginDecode(NULL, NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("BeginDecode excepted with NULL parameters"));
                info(DETAIL, _LOCATIONT);
            }
            info(UNINDENT, NULL);
        }

        info(ECHO | INDENT, TEXT("Testing with valid parameters"));

        info(ECHO, _T("Creating BitmapImage ImageSinks for decoding"));
        if (FAILED(hr = VerifyHRSuccess(priv_ImageDecoder::CreateImageSink(pImagingFactory, &id, &pImageSink))))
        {
            info(ABORT, TEXT("Cannot go on without image sink. %s"),
                E_OUTOFMEMORY == hr ? 
                    _T("More memory is required for this test.") : _T(""));
            goto LPExit;
        }

        if (FAILED(hr = VerifyHRSuccess(priv_ImageDecoder::CreateImageSink(pImagingFactory, &id, &pImageSink2))))
        {
            info(ABORT, TEXT("Cannot go on without second image sink. %s"),
                E_OUTOFMEMORY == hr ? 
                    _T("More memory is required for this test.") : _T(""));
            goto LPExit;
        }

        // EndDecode should fail if BeginDecode hasn't been called.
        info(ECHO, TEXT("Calling EndDecode before BeginDecode has been called (should fail)"));
        VerifyHRFailure(pImageDecoder->EndDecode(S_OK));

        // Decode should also fail if BeginDecode hasn't been called.
        info(ECHO, TEXT("Calling Decode before BeginDecode has been called (should fail)"));
        VerifyHRFailure(pImageDecoder->Decode());

        // BeginDecode should succeed with valid parameters
        info(ECHO, TEXT("Calling BeginDecode with a valid ImageSink (should succeed)"));
        VerifyHRSuccess(pImageDecoder->BeginDecode(pImageSink, NULL));

        // pImageSink ref should be at 2 now, addref to 3 and verify
        info(DETAIL, _T("Verifying that BeginDecode AddRef'ed the ImageSink"));
        VerifyConditionFalse(pImageSink->AddRef() - 3);

        // BeginDecode should fail if decode already begun
        info(ECHO, TEXT("Calling BeginDecode a second time with a different ImageSink (should fail)"));
        VerifyHRFailure(pImageDecoder->BeginDecode(pImageSink2, NULL));

        // pImageSink2 should only have one ref (ours)
        info(DETAIL, _T("Verifying that BeginDecode did not AddRef the ImageSink"));
        if (VerifyConditionFalse(pImageSink2->Release()))
            pImageSink2->Release();
        pImageSink2 = NULL;

        // Decode should now succeed (since we've begun)
        info(ECHO, TEXT("Decode after calling BeginDecode (should succeed)"));
        hr = pImageDecoder->Decode();
        // what should we do with this? We're not decoding with NOBLOCK set . . .
        while (E_PENDING == hr)
        {
            info(DETAIL, _T("Decode returned E_PENDING, sleeping for a second and calling again."));
            Sleep(1000);
            hr = pImageDecoder->Decode();
        }

        if (FAILED(hr))
        {
            info(FAIL, TEXT("Decode failed with hr = %08x"), hr);
            info(DETAIL, _LOCATIONT);
        }

        // EndSink should succeed.
        info(ECHO, TEXT("Calling EndDecode after decoding (should succeed)"));
        VerifyHRSuccess(pImageDecoder->EndDecode(S_OK));

        // The ImageSink should be at ref 2 now.
        info(DETAIL, _T("Verifying that EndDecode released the ImageSink"));
        VerifyConditionFalse(pImageSink->Release() - 1);

        // Decode should fail now
        info(ECHO, TEXT("Calling Decode after EndDecode was called (should fail)"));
        VerifyHRFailure(pImageDecoder->Decode());

        // EndSink called a second time should fail, without modifying the ref
        // for pImageSink
        info(ECHO, TEXT("Calling EndDecode a second time (should fail)"));
        pImageSink->AddRef();
        VerifyHRFailure(pImageDecoder->EndDecode(S_OK));
        info(DETAIL, _T("Verifying that second call to EndDecode did not release ImageSink"));
        VerifyConditionFalse(pImageSink->Release() - 1);
        SafeRelease(pImageSink);
        
        info(UNINDENT, NULL);

LPExit:
        SafeRelease(pImageSink);
        SafeRelease(pImageSink2);
        return getCode();
    }
    
    INT TestFrames(IImagingFactory * pImagingFactory, IImageDecoder * pImageDecoder, const CImageDescriptor & id)
    {
        IImageSink * pImageSink = NULL;
        UINT uiFrameDimensionsCount = 0;
        UINT uiFrameCount = 0;
        GUID * pguidFrameDimensions = NULL;
        int iTestFrameDimension = 0;
        int iExpectedFrameCount = 0;
        TCHAR tszFrameDim[25];

        info(ECHO | INDENT, _T("Checking if decoder supports frames"));
        if (E_NOTIMPL == pImageDecoder->GetFrameDimensionsCount(&uiFrameDimensionsCount))
        {
            // The decoder doesn't support frames. Make sure this agrees with the
            // config file.
            info(ECHO, TEXT("Frames not supported by decoder, making sure all APIs agree"));
            if (id.GetConfigTags().QueryInt(TESTTAG_FRAMEDIM))
            {
                info(FAIL, TEXT("Config for this image indicates a decoder that supports frames, but this decoder doesn't"));
                info(DETAIL, _LOCATIONT);
            }
            else
            {
                // Make sure all the other frame functions fail
                GUID rgguid[5];
                VerifyHRFailure(pImageDecoder->GetFrameDimensionsList(rgguid, 1));
                VerifyHRFailure(pImageDecoder->GetFrameCount(rgguid, &uiFrameCount));
                VerifyHRFailure(pImageDecoder->SelectActiveFrame(rgguid, 0));
            }
            goto LPExit;
        }
        info(UNINDENT, NULL);
        if (g_fBadParams)
        {
            info(ECHO | INDENT, TEXT("Testing with invalid parameters"));
            __try {
                VerifyHRFailure(pImageDecoder->GetFrameDimensionsCount(NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetFrameDimensionsCount excepted with NULL pointer"));
                info(DETAIL, _LOCATIONT);
            }
            // Make sure we call the next one with a valid count
            VerifyHRSuccess(pImageDecoder->GetFrameDimensionsCount(&uiFrameDimensionsCount));
            __try {
                VerifyHRFailure(pImageDecoder->GetFrameDimensionsList(NULL, uiFrameDimensionsCount));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetFrameDimensionsList excepted with NULL pointer"));
                info(DETAIL, _LOCATIONT);
            }

            // Get a valid frame dimension for the next call
            pguidFrameDimensions = new GUID [uiFrameDimensionsCount];
            if (!pguidFrameDimensions)
            {
                info(ABORT, TEXT("Out of memory!"));
                info(DETAIL, _LOCATIONT);
                goto LPExit;
            }
            VerifyHRSuccess(pImageDecoder->GetFrameDimensionsList(pguidFrameDimensions, uiFrameDimensionsCount));
            __try {
                VerifyHRFailure(pImageDecoder->GetFrameCount(NULL, NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetFrameCount excepted with NULL parameters"));
                info(DETAIL, _LOCATIONT);
            }
            __try {
                VerifyHRFailure(pImageDecoder->GetFrameCount(pguidFrameDimensions, NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetFrameCount excepted with NULL frame count pointer"));
                info(DETAIL, _LOCATIONT);
            }
            __try {
                VerifyHRFailure(pImageDecoder->GetFrameCount(NULL, &uiFrameCount));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetFrameCount excepted with NULL frame dimension pointer"));
                info(DETAIL, _LOCATIONT);
            }
            __try {
                VerifyHRFailure(pImageDecoder->SelectActiveFrame(NULL, 0));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("SelectActiveFrame excepted with NULL frame dimension pointer"));
                info(DETAIL, _LOCATIONT);
            }
            delete[] pguidFrameDimensions;
            pguidFrameDimensions = NULL;
            info(UNINDENT, NULL);
        }

        info(ECHO | INDENT, TEXT("Testing with valid parameters"));

        // Get the number of frame dimensions
        info(ECHO, _T("Getting the number of frame dimensions present in the current image"));
        VerifyHRSuccess(pImageDecoder->GetFrameDimensionsCount(&uiFrameDimensionsCount));
        pguidFrameDimensions = new GUID[uiFrameDimensionsCount];
        if (!pguidFrameDimensions)
        {
            info(ABORT, TEXT("Out of memory! Please increase available memory."));
            info(DETAIL, _LOCATIONT);
            goto LPExit;
        }

        info(ECHO, _T("Getting the list of frame dimensions for the current image"));
        VerifyHRSuccess(pImageDecoder->GetFrameDimensionsList(pguidFrameDimensions, uiFrameDimensionsCount));
        // this test currently only supports testing for one dimension
        if (uiFrameDimensionsCount > 1)
        {
            info(ECHO, TEXT("Warning: decoder has more than one frame dimension, testing only one"));
        }

        info(ECHO, _T("Selecting the frame dimension indicated by the config file"));
        // Select the framedim that's indicated in the config file (or the first one)
        // Leave some room in front for the '&' needed by the FRAMEDIM map.
        if (-1 != id.GetConfigTags().QueryString(TESTTAG_FRAMEDIM, tszFrameDim + 1, countof(tszFrameDim) - 1))
        {
            info(ECHO, TEXT("Verifying correct frame dimension"));
            const GUID * pguid = NULL;
            bool fFound = false;
            
            tszFrameDim[0] = '&';
            pguid = (const GUID *)g_mapFRAMEDIM[tszFrameDim];
            if (pguid)
            {
                for (int iCntr = 0; iCntr < uiFrameDimensionsCount; iCntr++)
                {
                    if (!memcmp(pguid, pguidFrameDimensions + iCntr, sizeof(GUID)))
                    {
                        iTestFrameDimension = iCntr;
                        fFound = true;
                        break;
                    }
                }
                if (!fFound)
                {
                    info(FAIL, TEXT("FRAMEDIM indicated in config file (%s) not supported by decoder"),
                        tszFrameDim);
                    info(DETAIL, _LOCATIONT);
                    goto LPExit;
                }
            }
            else
            {
                info(ECHO, TEXT("FRAMEDIM indicated in config file is unknown, using first dim in decoder"));
            }
        }

        info(ECHO, _T("Getting the number of frames in the selected dimension"));
        iExpectedFrameCount = id.GetConfigTags().QueryInt(TESTTAG_FRAMECOUNT);
        VerifyHRSuccess(pImageDecoder->GetFrameCount(pguidFrameDimensions + iTestFrameDimension, &uiFrameCount));
        if (iExpectedFrameCount)
        {
            info(ECHO, TEXT("Verifying correct frame count"));
            // Frame count should be provided as an integer multiplied by 10000
            if (iExpectedFrameCount > 10000)
            {
                iExpectedFrameCount /= 10000;
            }

            VerifyConditionTrue(uiFrameCount == iExpectedFrameCount);
        }

        // Now make sure we can correctly decode each frame.
        // First get a sink.
        info(ECHO | INDENT, TEXT("Decoding each frame in the image"));
        PresetPtr(pImageSink);
        info(ECHO, _T("Creating a BitmapImage ImageSink"));
        VerifyHRSuccess(priv_ImageDecoder::CreateImageSink(pImagingFactory, &id, &pImageSink));
        if (!pImageSink || PTRPRESET == pImageSink)
        {
            info(ABORT, TEXT("Cannot continue without an Image Sink"));
            goto LPExit;
        }
        for (UINT uiCurrentFrame = 0; uiCurrentFrame < uiFrameCount; uiCurrentFrame++)
        {
            HRESULT hr;
            info(ECHO, _T("Selecting frame %d"), uiCurrentFrame);
            VerifyHRSuccess(pImageDecoder->SelectActiveFrame(pguidFrameDimensions + iTestFrameDimension, uiCurrentFrame));

            info(ECHO, _T("Calling BeginDecode"));
            VerifyHRSuccess(pImageDecoder->BeginDecode(pImageSink, NULL));
            info(ECHO, _T("Calling Decode"));
            do
            {
                hr = pImageDecoder->Decode();
                Sleep(0);
            } while (E_PENDING == hr);
            if (FAILED(hr))
            {
                info(FAIL, TEXT("Could not decode frame %d of image, hr = %08x"),
                    uiCurrentFrame, hr);
                info(DETAIL, _LOCATIONT);
            }
            info(ECHO, _T("Calling EndDecode"));
            VerifyHRSuccess(pImageDecoder->EndDecode(S_OK));
        }
        info(ECHO, _T("Verifying correct reference count for the ImageSink"));
        VerifyConditionFalse(pImageSink->Release());
        
LPExit:
        if (pguidFrameDimensions)
        {
            delete[] pguidFrameDimensions;
        }
        info(UNINDENT, NULL);
        return getCode();
    }
    
    INT TestGetImageInfo(IImagingFactory * pImagingFactory, IImageDecoder * pImageDecoder, const CImageDescriptor & id)
    {
        ImageInfo ii;
        if (g_fBadParams)
        {
            info(ECHO | INDENT, TEXT("Testing with invalid parameters"));
            __try {
                VerifyHRFailure(pImageDecoder->GetImageInfo(NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetImageInfo excepted with NULL parameter"));
            }
            info(UNINDENT, NULL);
        }

        info(ECHO | INDENT, TEXT("Testing with valid parameters"));
        info(ECHO, _T("Getting the ImageInfo"));
        VerifyHRSuccess(pImageDecoder->GetImageInfo(&ii));

        // VerifyImageParameters will make sure the image info matches what's expected
        // (as indicated in the ImageDescriptor, pulled from the config files).
        info(ECHO, _T("Comparing the ImageInfo with the ImageInfo specified in the config file"));
        VerifyImageParameters(ii, id);

        info(UNINDENT, NULL);
        
        return getCode();
    }
    
    INT TestGetThumbnail(IImagingFactory * pImagingFactory, IImageDecoder * pImageDecoder, const CImageDescriptor & id)
    {
        IImage * pImage;
        HRESULT hr;
        if (g_fBadParams)
        {
            info(ECHO | INDENT, TEXT("Testing with invalid parameters"));
            info(ECHO, _T("Calling GetThumbnail with NULL image pointer pointer"));
            __try {
                VerifyHRFailure(pImageDecoder->GetThumbnail(0, 0, NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetThumbnail excepted with NULL pointer"));
            }
            info(UNINDENT, NULL);
        }

        info(ECHO | INDENT, TEXT("Testing with valid parameters"));
        if (id.GetConfigTags().QueryInt(TESTTAG_THUMBNAIL_PRESENT))
        {
            // According to the config file, there should be a thumbnail associated with this image.
            PresetPtr(pImage);

            // Since the height and width are only hints, there is no good way to test them.
            info(ECHO, TEXT("Image should have thumbnail according to config file: GetThumbnail should succeed or return E_NOTIMPL"));
            hr = pImageDecoder->GetThumbnail(0, 0, &pImage);
            // Even if GetThumbnail succeeds, make sure it set the image pointer.
            if (E_NOTIMPL == hr)
            {
                info(ECHO, _T("GetThumbnail is not supported by this decoder."));
            }
            else if (FAILED(hr) || IsPreset(pImage))
            {
                info(FAIL, TEXT("pImage pointer not set by call to GetThumbnail"));
                info(DETAIL, _LOCATIONT);
            }
            else
            {
                VerifyConditionFalse(pImage->Release());
            }
        }
        else
        {
            info(ECHO, TEXT("Image should not have thumbnail according to config file: GetThumbnail should fail"));
            PresetPtr(pImage);
            VerifyHRFailure(pImageDecoder->GetThumbnail(0, 0, &pImage));
            if (pImage && PTRPRESET != pImage)
            {
                info(FAIL, TEXT("GetThumbnail returned image when thumbnail shouldn't be present, releasing image"));
                VerifyConditionFalse(pImage->Release());
            }
        }
        info(UNINDENT, NULL);
        return getCode();
    }
    
    INT TestDecoderParams(IImagingFactory * pImagingFactory, IImageDecoder * pImageDecoder, const CImageDescriptor & id)
    {
        HRESULT hr;
        for (int iParam = 0; iParam < g_cDecoder; iParam++)
        {
            // Use a UINT buffer because that way we can know that the elements
            // will always be 4-byte aligned.
            UINT rgBuffer[3] = {0};
            int iLength = 0;
            info(ECHO | INDENT, _T("Testing parameter %s"), g_mapDECODER[*g_rgDecoder[iParam]]);
            if (&DECODER_TRANSCOLOR == g_rgDecoder[iParam] ||
                &DECODER_TRANSRANGE == g_rgDecoder[iParam])
            {
                // This is a Long Range
                iLength = 8;
                rgBuffer[0] = 0x80;
                rgBuffer[1] = 0xFF;
            }
            else if (&DECODER_OUTPUTCHANNEL == g_rgDecoder[iParam])
            {
                // This is an ASCII character (r, g, b, c, m, y, k)
                iLength = 1;
                ((BYTE*)rgBuffer)[0] = 'r';
            }
            else if (&DECODER_ICONRES == g_rgDecoder[iParam])
            {
                // This is three longs (width, height, bpp)
                iLength = 12;
                rgBuffer[0] = 16;
                rgBuffer[1] = 16;
                rgBuffer[2] = 24;
            }
            else if (&DECODER_USEICC == g_rgDecoder[iParam])
            {
                // This is a boolean
                iLength = 1;
                ((BYTE*)rgBuffer)[0] = 1;
            }
            
            hr = pImageDecoder->QueryDecoderParam(*(g_rgDecoder[iParam]));
            if (FAILED(hr))
            {
                info(ECHO, _T("Decoder does not support this parameter, calling SetDecoderParam anyway (should not crash)"));
                pImageDecoder->SetDecoderParam(*(g_rgDecoder[iParam]), iLength, (void*)rgBuffer);
            }
            else
            {
                info(ECHO, _T("Decoder does support this parameter, trying SetDecoderParam"));
                hr = pImageDecoder->SetDecoderParam(*(g_rgDecoder[iParam]), iLength, (void*)rgBuffer);
                if (FAILED(hr))
                {
                    info(ECHO, _T("SetDecoderParameter failed, is this parameter supported?"));
                }
            }
            info(UNINDENT, NULL);
        }
        return getCode();
    }

    namespace priv_GetProperties
    {
        void PrintPropertyItem(const PropertyItem * pPropItem)
        {
            // Since we're printing out a maximum number of values at a time,
            // we can limit the size of our buffer.
            TCHAR tszBuffer[70];
            int iCntr;
            int iValuesPerLine = 4;
            int iValueCount;
            // tszTemp needs to be large enough to hold a temporary value.
            // The largest to display is the signed rational, at 11 digits
            // per signed long times 2, plus room for '/' and spaces.
            TCHAR tszTemp[32];
            info(DETAIL, TEXT("ID: %s, Type: %s, Length: %d"),
                g_mapTAG[pPropItem->id], g_mapTAG_TYPE[pPropItem->type], pPropItem->length);

            tszBuffer[0] = 0;
            switch (pPropItem->type)
            {
            case PropertyTagTypeUndefined:
            case PropertyTagTypeByte:
                iValuesPerLine = 16;
                iValueCount = pPropItem->length;
                for (iCntr = 0; iCntr <= iValueCount / iValuesPerLine; iCntr++)
                {
                    tszBuffer[0] = 0;
                    for (int iInner = 0; iInner < iValuesPerLine && iInner + iCntr * iValuesPerLine < iValueCount; iInner++)
                    {
                        _stprintf(tszTemp, TEXT("%02x "), 
                            ((UNALIGNED BYTE *)(pPropItem->value))[iCntr * iValuesPerLine + iInner]);
                        _tcscat(tszBuffer, tszTemp);
                    }
                    info(DETAIL, TEXT("Value (%5d): %s"), iCntr * iValuesPerLine, tszBuffer);
                }
                break;
            case PropertyTagTypeASCII:
                iValuesPerLine = 128;
                iValueCount = pPropItem->length;
                for (iCntr = 0; iCntr <= iValueCount / iValuesPerLine; iCntr++)
                {
                    info(DETAIL, TEXT("Value (%5d): \"%.128hs\""), 
                        iCntr * iValuesPerLine, 
                        ((UNALIGNED BYTE *)(pPropItem->value)) + iCntr * iValuesPerLine);
                }
                break;
            case PropertyTagTypeShort:
                iValuesPerLine = 8;
                iValueCount = pPropItem->length / sizeof(SHORT);
                for (iCntr = 0; iCntr <= iValueCount / iValuesPerLine; iCntr++)
                {
                    tszBuffer[0] = 0;
                    for (int iInner = 0; iInner < iValuesPerLine && iInner + iCntr * iValuesPerLine < iValueCount; iInner++)
                    {
                        _stprintf(tszTemp, TEXT("%04x "), 
                            ((UNALIGNED SHORT *)(pPropItem->value))[iCntr * iValuesPerLine + iInner]);
                        _tcscat(tszBuffer, tszTemp);
                    }
                    info(DETAIL, TEXT("Value (%5d): %s"), iCntr * iValuesPerLine, tszBuffer);
                }
                break;
            case PropertyTagTypeLong:
                iValuesPerLine = 4;
                iValueCount = pPropItem->length / sizeof(LONG);
                for (iCntr = 0; iCntr <= iValueCount / iValuesPerLine; iCntr++)
                {
                    tszBuffer[0] = 0;
                    for (int iInner = 0; iInner < iValuesPerLine && iInner + iCntr * iValuesPerLine < iValueCount; iInner++)
                    {
                        _stprintf(tszTemp, TEXT("%08x "), 
                            ((UNALIGNED LONG *)(pPropItem->value))[iCntr * iValuesPerLine + iInner]);
                        _tcscat(tszBuffer, tszTemp);
                    }
                    info(DETAIL, TEXT("Value (%5d): %s"), iCntr * iValuesPerLine, tszBuffer);
                }
                break;
            case PropertyTagTypeRational:
                iValuesPerLine = 2;
                iValueCount = pPropItem->length / (sizeof(LONG) * 2);
                for (iCntr = 0; iCntr <= iValueCount / iValuesPerLine; iCntr++)
                {
                    tszBuffer[0] = 0;
                    for (int iInner = 0; iInner < iValuesPerLine && iInner + iCntr * iValuesPerLine < iValueCount; iInner++)
                    {
                        _stprintf(tszTemp, TEXT("%8x / %8x "), 
                            ((UNALIGNED LONG *)(pPropItem->value))[2 * (iCntr * iValuesPerLine + iInner)],
                            ((UNALIGNED LONG *)(pPropItem->value))[2 * (iCntr * iValuesPerLine + iInner) + 1]);
                        _tcscat(tszBuffer, tszTemp);
                    }
                    info(DETAIL, TEXT("Value (%5d): %s"), iCntr * iValuesPerLine, tszBuffer);
                }
                break;
            case PropertyTagTypeSLONG:
                iValuesPerLine = 4;
                iValueCount = pPropItem->length / sizeof(LONG);
                for (iCntr = 0; iCntr <= iValueCount / iValuesPerLine; iCntr++)
                {
                    tszBuffer[0] = 0;
                    for (int iInner = 0; iInner < iValuesPerLine && iInner + iCntr * iValuesPerLine < iValueCount; iInner++)
                    {
                        _stprintf(tszTemp, TEXT("%11d "), 
                            ((UNALIGNED LONG *)(pPropItem->value))[iCntr * iValuesPerLine + iInner]);
                        _tcscat(tszBuffer, tszTemp);
                    }
                    info(DETAIL, TEXT("Value (%5d): %s"), iCntr * iValuesPerLine, tszBuffer);
                }
                break;
            case PropertyTagTypeSRational:
                iValuesPerLine = 2;
                iValueCount = pPropItem->length / (sizeof(LONG) * 2);
                for (iCntr = 0; iCntr <= iValueCount / iValuesPerLine; iCntr++)
                {
                    tszBuffer[0] = 0;
                    for (int iInner = 0; iInner < iValuesPerLine && iInner + iCntr * iValuesPerLine < iValueCount; iInner++)
                    {
                        _stprintf(tszTemp, TEXT("%11d / %11d "), 
                            ((UNALIGNED LONG *)(pPropItem->value))[2 * (iCntr * iValuesPerLine + iInner)],
                            ((UNALIGNED LONG *)(pPropItem->value))[2 * (iCntr * iValuesPerLine + iInner) + 1]);
                        _tcscat(tszBuffer, tszTemp);
                    }
                    info(DETAIL, TEXT("Value (%5d): %s"), iCntr * iValuesPerLine, tszBuffer);
                }
                break;
            }
        }
    }
    INT TestGetProperties(IImagingFactory * pImagingFactory, IImageDecoder * pImageDecoder, const CImageDescriptor & id)
    {
        UINT uiPropertyCount;
        UINT uiPropertyCount2;
        UINT uiPropertyItemSize;
        UINT uiPropertyListSize;
        UINT uiActualListSize;
        int iIndex;
        int iIteration;
        const int c_iIters = 5;
        char * aszProperty = NULL;
        PROPID * pidList = NULL;
        PropertyItem * pPropItems = NULL;
        PropertyItem * pItem = NULL;
        if (E_NOTIMPL == pImageDecoder->GetPropertyCount(&uiPropertyCount))
        {
            PROPID propid;
            PropertyItem propitem;
            // The decoder doesn't support properties. Make sure none of the other
            // property APIs are supported
            info(ECHO | INDENT, TEXT("Frames not supported by decoder, making sure all APIs agree"));
            // Make sure all the other frame functions fail
            VerifyHRFailure(pImageDecoder->GetPropertyIdList(1, &propid));
            VerifyHRFailure(pImageDecoder->GetPropertyItemSize(propid, &uiPropertyItemSize));
            VerifyHRFailure(pImageDecoder->GetPropertySize(&uiPropertyListSize, &uiPropertyCount));
            VerifyHRFailure(pImageDecoder->GetAllPropertyItems(uiPropertyListSize, uiPropertyCount, &propitem));
            VerifyHRFailure(pImageDecoder->RemovePropertyItem(propid));
            VerifyHRFailure(pImageDecoder->SetPropertyItem(propitem));
            goto LPExit;
        }
        if (g_fBadParams)
        {
            info(ECHO | INDENT, TEXT("Testing with invalid parameters"));
            const PROPID propidInvalid = 0;
            info(ECHO, _T("Calling GetPropertyCount with NULL parameter"));
            __try {
                VerifyHRFailure(pImageDecoder->GetPropertyCount(NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetPropertyCount excepted with NULL parameter"));
                info(DETAIL, _LOCATIONT);
            }

            // Now grab the correct count for use in the rest
            VerifyHRSuccess(pImageDecoder->GetPropertyCount(&uiPropertyCount));

            pidList = new PROPID[uiPropertyCount];
            if (!pidList)
            {
                info(ABORT, TEXT("Out of memory allocating property id list"));
                info(DETAIL, _LOCATIONT);
                goto LPExit;
            }
            info(ECHO, _T("Calling GetPropertyIdList with NULL propid list pointer"));
            __try {
                VerifyHRFailure(pImageDecoder->GetPropertyIdList(uiPropertyCount, NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetPropertyIdList excepted with NULL propid list pointer"));
                info(DETAIL, _LOCATIONT);
            }

            info(ECHO, _T("Calling GetPropertyIdList with incorrect property count"));
            __try {
                VerifyHRFailure(pImageDecoder->GetPropertyIdList(uiPropertyCount - 1, pidList));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetPropertyIdList excepted with incorrect property count"));
                info(DETAIL, _LOCATIONT);
            }

            // Now grab a correct list for use in the rest
            VerifyHRSuccess(pImageDecoder->GetPropertyIdList(uiPropertyCount, pidList));

            info(ECHO, _T("Calling GetPropertyItemSize with NULL size pointer"));
            __try {
                VerifyHRFailure(pImageDecoder->GetPropertyItemSize(pidList[0], NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetPropertyItemSize excepted with NULL size pointer"));
                info(DETAIL, _LOCATIONT);
            }

            info(ECHO, _T("Calling GetPropertyItemSize with invalid property id"));
            __try {
                VerifyHRFailure(pImageDecoder->GetPropertyItemSize(propidInvalid, &uiPropertyItemSize));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetPropertyItemSize excepted with invalid property id"));
                info(DETAIL, _LOCATIONT);
            }

            // Now get the correct size for the first item
            if (uiPropertyCount)
            {
                VerifyHRSuccess(pImageDecoder->GetPropertyItemSize(pidList[0], &uiPropertyItemSize));
            }
            else
            {
                uiPropertyItemSize = 0;
            }

            // Create a property item large enough.
            pPropItems = (PropertyItem *) new BYTE[uiPropertyItemSize];
            if (!pPropItems)
            {
                info(ABORT, TEXT("Out of memory allocating property items"));
                info(DETAIL, _LOCATIONT);
            }

            info(ECHO, _T("Calling GetPropertyItem with NULL property item pointer"));
            __try {
                VerifyHRFailure(pImageDecoder->GetPropertyItem(pidList[0], uiPropertyItemSize, NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetPropertyItem excepted with NULL property item pointer"));
                info(DETAIL, _LOCATIONT);
            }

            info(ECHO, _T("Calling GetPropertyItem with invalid property ID"));
            __try {
                VerifyHRFailure(pImageDecoder->GetPropertyItem(propidInvalid, uiPropertyItemSize, pPropItems));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetPropertyItem excepted with invalid property ID"));
                info(DETAIL, _LOCATIONT);
            }

            info(ECHO, _T("Calling GetPropertyItem with incorrect property item size"));
            __try {
                VerifyHRFailure(pImageDecoder->GetPropertyItem(pidList[0], uiPropertyItemSize - 1, pPropItems));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetPropertyItem excepted with incorrect property item size"));
                info(DETAIL, _LOCATIONT);
            }

            delete[] pidList;
            pidList = NULL;
            delete[] (BYTE*) pPropItems;
            pPropItems = NULL;

            info(ECHO, _T("Calling GetPropertySize with NULL pointers"));
            __try {
                VerifyHRFailure(pImageDecoder->GetPropertySize(NULL, NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetPropertySize excepted with NULL pointers"));
                info(DETAIL, _LOCATIONT);
            }

            info(ECHO, _T("Calling GetPropertySize with NULL count pointer"));
            __try {
                VerifyHRFailure(pImageDecoder->GetPropertySize(&uiPropertyListSize, NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetPropertySize excepted with NULL property count pointer"));
                info(DETAIL, _LOCATIONT);
            }

            info(ECHO, _T("Calling GetPropertySize with NULL buffer size pointer"));
            __try {
                VerifyHRFailure(pImageDecoder->GetPropertySize(NULL, &uiPropertyCount));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetPropertySize excepted with NULL buffer size pointer"));
                info(DETAIL, _LOCATIONT);
            }

            // Get the correct buffer size and prop count for later use
            VerifyHRSuccess(pImageDecoder->GetPropertySize(&uiPropertyListSize, &uiPropertyCount));

            pPropItems = (PropertyItem *) new BYTE[uiPropertyListSize];
            if (!pPropItems)
            {
                info(ABORT, TEXT("Out of memory allocating property item buffer"));
                info(DETAIL, _LOCATIONT);
                goto LPExit;
            }

            info(ECHO, _T("Calling GetAllPropertyItems with NULL buffer pointer"));
            __try {
                VerifyHRFailure(pImageDecoder->GetAllPropertyItems(uiPropertyListSize, uiPropertyCount, NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetAllPropertyItems excepted with NULL buffer pointer"));
                info(DETAIL, _LOCATIONT);
            }

            info(ECHO, _T("Calling GetAllPropertyItems with incorrect property count"));
            __try {
                VerifyHRFailure(pImageDecoder->GetAllPropertyItems(uiPropertyListSize, uiPropertyCount - 1, pPropItems));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetAllPropertyItems excepted with incorrect property count"));
                info(DETAIL, _LOCATIONT);
            }

            info(ECHO, _T("Calling GetAllPropertyItems with incorrect property list size"));
            __try {
                VerifyHRFailure(pImageDecoder->GetAllPropertyItems(uiPropertyListSize - 1, uiPropertyCount, pPropItems));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetAllPropertyItems excepted with incorrect buffer size"));
                info(DETAIL, _LOCATIONT);
            }

            // Grab the properties for testing the SetPropertyItem
            VerifyHRSuccess(pImageDecoder->GetAllPropertyItems(uiPropertyListSize, uiPropertyCount, pPropItems));

            if (uiPropertyCount)
            {
                PropertyItem piTest;
                WORD wInvalidType = RandomInt(11, 0xffff);
                piTest = pPropItems[0];

                piTest.value = NULL;
                info(ECHO, _T("Calling SetPropertyItem with NULL item.value pointer"));
                __try {
                    VerifyHRFailure(pImageDecoder->SetPropertyItem(piTest));
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    info(FAIL, TEXT("SetPropertyItem excepted with NULL item.value"));
                    info(DETAIL, _LOCATIONT);
                }

                piTest = pPropItems[0];
                piTest.id = propidInvalid;
                info(ECHO, _T("Calling SetPropertyItem with invalid item.id"));
                __try {
                    VerifyHRFailure(pImageDecoder->SetPropertyItem(piTest));
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    info(FAIL, TEXT("SetPropertyItem excepted with invalid item.id"));
                    info(DETAIL, _LOCATIONT);
                }
                piTest = pPropItems[0];
                piTest.type = wInvalidType;
                info(ECHO, _T("Calling SetPropertyItem with invalid item.type"));
                __try {
                    VerifyHRFailure(pImageDecoder->SetPropertyItem(piTest));
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    info(FAIL, TEXT("SetPropertyItem excepted with invalid item.type"));
                    info(DETAIL, _LOCATIONT);
                }
            }

            delete[] (BYTE*) pPropItems;
            pPropItems = NULL;

            info(ECHO, _T("Calling RemovePropertyItem with invalid property ID"));
            __try {
                VerifyHRFailure(pImageDecoder->RemovePropertyItem(propidInvalid));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("RemovePropertyItem excepted with invalid propid"));
                info(DETAIL, _LOCATIONT);
            }

            info(UNINDENT, NULL);
        }

        info(ECHO | INDENT, TEXT("Testing with valid parameters"));

        // We're going to go through and first verify the properties in the image,
        // then try to change and/or remove them

        for (iIteration = 0; iIteration < c_iIters; iIteration++)
        {
            // Retrieve a list of all the properties
            info(ECHO | INDENT, _T("Checking and modifying properties, iteration %d"), iIteration);
            
            info(ECHO, _T("Getting the property list size with GetPropertySize"));
            VerifyHRSuccess(pImageDecoder->GetPropertySize(&uiPropertyListSize, &uiPropertyCount));

            info(DETAIL, _T("Allocating memory for the properties"));
            pPropItems = (PropertyItem *) new BYTE[uiPropertyListSize];
            if (!pPropItems)
            {
                info(ABORT, TEXT("Out of memory!"));
                info(DETAIL, _LOCATIONT);
                goto LPExit;
            }

            info(ECHO, _T("Getting all the property items with GetAllPropertyItems"));
            VerifyHRSuccess(pImageDecoder->GetAllPropertyItems(uiPropertyListSize, uiPropertyCount, pPropItems));

            info(ECHO, _T("Verifying that the size of the property list is correct"));
            // Verify that the buffer size is consistent with the property sizes
            uiActualListSize = sizeof(PropertyItem) * uiPropertyCount;

            VerifyConditionTrue(uiActualListSize <= uiPropertyListSize);

            for (iIndex = 0; iIndex < uiPropertyCount; iIndex++)
            {
                uiActualListSize += pPropItems[iIndex].length;
            }

            if (!iIteration)
            {
                info(ECHO, TEXT("Current image properties:"));
                for (int iCntr = 0; iCntr < uiPropertyCount; iCntr++)
                {
                    priv_GetProperties::PrintPropertyItem(pPropItems + iCntr);
                }
            }

            VerifyConditionTrue(uiActualListSize <= uiPropertyListSize);

            // Now go through and verify that the properties returned by GetPropertyItem
            // are the same as the ones returned by GetAllPropertyItems, and that those
            // values are the same as what is stored in the config file.
            info(ECHO | INDENT, _T("Verifying properties returned by GetPropertyItem match those returned by GetAllPropertyItems"));
            info(ECHO, _T("Calling GetPropertyCount (should return same count as GetPropertySize)"));
            VerifyHRSuccess(pImageDecoder->GetPropertyCount(&uiPropertyCount2));
            if (uiPropertyCount != uiPropertyCount2)
            {
                info(FAIL, TEXT("GetPropertyCount returned %d items, GetPropertySize returned %d items, cannot continue"),
                    uiPropertyCount2, uiPropertyCount);
                info(DETAIL, _LOCATIONT);
                goto LPExit;
            }

            pidList = new PROPID[uiPropertyCount];
            if (!pidList)
            {
                info(ABORT, TEXT("Out of memory allocating Property ID List for GetPropertyIdList"));
                info(DETAIL, _LOCATIONT);
                goto LPExit;
            }
            info(ECHO, _T("Calling GetPropertyIdList"));
            VerifyHRSuccess(pImageDecoder->GetPropertyIdList(uiPropertyCount, pidList));

            for (iIndex = 0; iIndex < uiPropertyCount; iIndex++)
            {
                bool fFound = false;
                double dValue;
                info(ECHO | INDENT, _T("Verifying property %d of %d"), iIndex + 1, uiPropertyCount);
                info(ECHO, _T("Calling GetPropertyItemSize"));
                VerifyHRSuccess(pImageDecoder->GetPropertyItemSize(pidList[iIndex], &uiPropertyItemSize));
                pItem = (PropertyItem*) new BYTE[uiPropertyItemSize];
                if (!pItem)
                {
                    info(ABORT, TEXT("Out of memory allocating buffer for property item"));
                    info(DETAIL, _LOCATIONT);
                    goto LPExit;
                }
                info(ECHO, _T("Calling GetPropertyItem"));
                VerifyHRSuccess(pImageDecoder->GetPropertyItem(pidList[iIndex], uiPropertyItemSize, pItem));
                for (int iIndex2 = 0; iIndex2 < uiPropertyCount; iIndex2++)
                {
                    if (pItem->id == pPropItems[iIndex2].id)
                    {
                        fFound = true;
                        if (pItem->id != pPropItems[iIndex2].id ||
                            pItem->length != pPropItems[iIndex2].length ||
                            pItem->type != pPropItems[iIndex2].type ||
                            memcmp(pItem->value, pPropItems[iIndex2].value, pItem->length))
                        {
                            info(FAIL, TEXT("Property items are different"));
                            info(DETAIL, _LOCATIONT);
                            info(DETAIL, TEXT("Single Property Item: "));
                            priv_GetProperties::PrintPropertyItem(pItem);
                            info(DETAIL, TEXT("List Property Item: "));
                            priv_GetProperties::PrintPropertyItem(pPropItems + iIndex2);
                        }
                    }
                }

                if (!fFound)
                {
                    info(FAIL, _T("Property returned by GetPropertyItem was not returned by GetAllPropertyItems"));
                    priv_GetProperties::PrintPropertyItem(pItem);
                }

                // We can only compare against the config file if we haven't
                // already changed a property
                if (!iIteration && id.GetConfigTags().QueryInt(pItem->id))
                {
                    // Need to make sure that what's stored in the config file is
                    // the same as what was in the image.
                    info(ECHO, _T("Property is listed in config file, verifying a match"));
                    int iStrLen;
                    if (-1 != (iStrLen = id.GetConfigTags().QueryString(pItem->id, NULL, 0)))
                    {
                        TCHAR * tszValue = new TCHAR[iStrLen];
                        if (!tszValue)
                        {
                            info(ABORT, TEXT("Out of Memory allocating Value string"));
                            info(DETAIL, _LOCATIONT);
                            goto LPExit;
                        }
                        id.GetConfigTags().QueryString(pItem->id, tszValue, iStrLen);
                        switch (pItem->type)
                        {
                        case PropertyTagTypeASCII:
                            for (int iCntr = 0; iCntr < iStrLen; iCntr++)
                            {
                                if (tszValue[iCntr] != ((BYTE *)pItem->value)[iCntr])
                                {
                                    info(FAIL, TEXT("Expected \"%s\" for string value"),
                                        tszValue);
                                    priv_GetProperties::PrintPropertyItem(pItem);
                                }
                            }
                        }
                        delete[] tszValue;
                    }
                    else
                    {
                        // Compare the first value in the property with the config value
                        // (only one value is supported by the test, which should work
                        // well for the most part).
                        dValue = ((double)id.GetConfigTags().QueryInt(pItem->id)) / 10000.0;
                        switch (pItem->type)
                        {
                        case PropertyTagTypeByte:
                            if (fabs(dValue - ((BYTE *)pItem->value)[0]) > 1.0 / 10000.0)
                            {
                                info(FAIL, TEXT("Expected %lf for byte value"), dValue);
                                priv_GetProperties::PrintPropertyItem(pItem);
                            }
                            break;
                        case PropertyTagTypeLong:
                            if (fabs(dValue - ((ULONG *)pItem->value)[0]) > 1.0 / 10000.0)
                            {
                                info(FAIL, TEXT("Expected %lf for unsigned long value"), dValue);
                                priv_GetProperties::PrintPropertyItem(pItem);
                            }
                            break;
                        case PropertyTagTypeShort:
                            if (fabs(dValue - ((USHORT *)pItem->value)[0]) > 1.0 / 10000.0)
                            {
                                info(FAIL, TEXT("Expected %lf for unsigned short value"), dValue);
                                priv_GetProperties::PrintPropertyItem(pItem);
                            }
                            break;
                        case PropertyTagTypeRational:
                            if (fabs(dValue - ((double)((ULONG *)pItem->value)[0])/((double)((ULONG *)pItem->value)[1])) > 1.0 / 10000.0)
                            {
                                info(FAIL, TEXT("Expected %lf for unsigned rational value"), dValue);
                                priv_GetProperties::PrintPropertyItem(pItem);
                            }
                            break;
                        case PropertyTagTypeSRational:
                            if (fabs(dValue - ((double)((LONG *)pItem->value)[0])/((double)((LONG *)pItem->value)[1])) > 1.0 / 10000.0)
                            {
                                info(FAIL, TEXT("Expected %lf for unsigned rational value"), dValue);
                                priv_GetProperties::PrintPropertyItem(pItem);
                            }
                            break;
                        default:
                            info(FAIL, TEXT("Expected %lf, received unexpected value type"), dValue);
                            priv_GetProperties::PrintPropertyItem(pItem);
                        }
                    }
                }
                delete[] (BYTE*) pItem;
                info(UNINDENT, NULL);
            }
            info(UNINDENT, NULL);
            if (uiPropertyCount)
            {
                // Choose a random property item to change
                info(ECHO, _T("Changing random property"));
                int iPropToChange = rand() % uiPropertyCount;
                if (PropertyTagTypeASCII == pPropItems[iPropToChange].type)
                {
                    pPropItems[iPropToChange].value = "This is a test string. A long test string. A very long test string.";
                    pPropItems[iPropToChange].length = strlen((char *) pPropItems[iPropToChange].value) + 1;
                }
                else
                    GetRandomData(pPropItems[iPropToChange].value, pPropItems[iPropToChange].length);

                if (rand() % 2)
                {
                    // Can't be sure the decoder supports removing properties.
                    if (SUCCEEDED(pImageDecoder->RemovePropertyItem(pPropItems[iPropToChange].id)))
                    {
                        UINT uiTemp;
                        VerifyHRFailure(pImageDecoder->GetPropertyItemSize(pPropItems[iPropToChange].id, &uiTemp));
                    }
                    else
                    {
                        UINT uiTemp;
                        VerifyHRSuccess(pImageDecoder->GetPropertyItemSize(pPropItems[iPropToChange].id, &uiTemp));
                    }
                }

                if (SUCCEEDED(pImageDecoder->SetPropertyItem(pPropItems[iPropToChange])))
                {
                    VerifyHRSuccess(pImageDecoder->GetPropertyItemSize(pPropItems[iPropToChange].id, &uiPropertyItemSize));
                }
            }
            if (pidList)
            {
                delete[] pidList;
                pidList = NULL;
            }
            if (pPropItems)
            {
                delete[] (BYTE*) pPropItems;
                pPropItems = NULL;
            }
            info(UNINDENT, NULL);
        }
LPExit:
        if (pidList)
            delete[] pidList;
        if (pPropItems)
            delete[] (BYTE*) pPropItems;
        if (pItem)
            delete[] (BYTE*) pItem;
        info(UNINDENT, NULL);
        return getCode();
    }
}
