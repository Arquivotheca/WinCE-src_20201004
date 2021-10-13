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
#include "PerfTests.h"
#include <windows.h>
#include <stdlib.h>
#include "main.h"
#include "ImagingHelpers.h"
#include <string>
#include <list>
#include <exception>
#include "PerfLoggerAPI.h"

using namespace ImagingHelpers;

const int MARK_CREATEBITMAPFROMIMAGEINMEMORY = 0;
const int MARK_CREATEBITMAPFROMIMAGEWITHLOAD = 1;
const int MARK_CREATEBITMAPFROMIMAGEFROMFILE = 2;
const int MARK_CREATEBITMAPFROMIMAGECACHE    = 3;
const int MARK_ENCODEBITMAPTOMEMORY          = 4;
const int MARK_ENCODEBITMAPWITHSAVE          = 5;
const int MARK_ENCODEBITMAPTOFILE            = 6;
const int MARK_GETTHUMBNAIL                  = 7;

const TCHAR c_tszCreateBitmapFromImageInMemory[] = TEXT("CreateBitmapFromImage in memory");
const TCHAR c_tszCreateBitmapFromImageWithLoad[] = TEXT("CreateBitmapFromImage including load to memory");
const TCHAR c_tszCreateBitmapFromImageFromFile[] = TEXT("CreateBitmapFromImage directly from file");
const TCHAR c_tszCreateBitmapFromImageCache[]    = TEXT("CreateBitmapFromImage Cached");
const TCHAR c_tszEncodeBitmapToMemory[]          = TEXT("Encode bitmap to memory stream");
const TCHAR c_tszEncodeBitmapWithSave[]          = TEXT("Encode bitmap and save to file");
const TCHAR c_tszEncodeBitmapToFile[]            = TEXT("Encode bitmap to file");
const TCHAR c_tszGetThumbnail[]                  = TEXT("GetThumbnail image in memory");

const TCHAR c_tszTestName[]                      = TEXT("ImgCodecTK_perf");

const TCHAR c_tszEncodeTempName[]                = _T("perf_imaging_temp_encode.img");

// This is the largest stream that could be needed (32bpp bitmap 2048x1536)
const ULARGE_INTEGER c_uliMaxStreamSize = {5275756, 0};

namespace PerfTests
{
    void DisplayMarkerData(
        const TCHAR * tszMarkerName, 
        DWORD dwMinTime, 
        DWORD dwMaxTime, 
        UINT64 ui64TotalTime, 
        UINT64 ui64TotalTime2,
        DWORD dwIterations)
    {
        LARGE_INTEGER liFreq;
        LARGE_INTEGER liTemp;
        double dMinTime = dwMinTime;
        double dMaxTime = dwMaxTime;
        double dTotalTime = ui64TotalTime;
        double dTotalTime2 = ui64TotalTime2;
        if(QueryPerformanceFrequency(&liFreq) &&
           QueryPerformanceCounter(&liTemp))
        {
            dMinTime    /= (double)liFreq.QuadPart;
            dMaxTime    /= (double)liFreq.QuadPart;
            dTotalTime  /= (double)liFreq.QuadPart;
            dTotalTime2 /= (double)liFreq.QuadPart * (double)liFreq.QuadPart;
            dMinTime    *= 1000;
            dMaxTime    *= 1000;
            dTotalTime  *= 1000;
            dTotalTime2 *= 1000 * 1000;
        }
        double dMean = dTotalTime / (double) dwIterations;
        // The variance is approximated by (sum(x^2) - (sum(x)^2)/n) / (n - 1)
        double dVariance = dwIterations > 1 ?
            (dTotalTime2 - dTotalTime * dTotalTime / (double) dwIterations) / 
            ((double)dwIterations - 1)
            : 0.0;
        double dStdDeviation = sqrt(dVariance);
        info(DETAIL, tszMarkerName);
        info(DETAIL, _T("%12s %12s %12s %12s %10s"), 
            _T("Minimum"), 
            _T("Mean"), 
            _T("Maximum"), 
            _T("StdDev"),
            _T("Samples"));
        info(DETAIL, _T("%10.2lfms %10.2lfms %10.2lfms %10.2lfms %10d"),
            dMinTime, dMean, dMaxTime, dStdDeviation, dwIterations);
    }

    INT TestCreateBitmapFromImageInMemory(IImagingFactory* pImagingFactory, const CImageDescriptor& id)
    {
        const int c_iIters = 200;
        IBitmapImage* pBitmapImage = NULL;
        IImage * pImage;
        IStream * pStream;
        HRESULT hr;
        DWORD dwMinTime = 0xffffffff;
        DWORD dwMaxTime = 0;
        UINT64 ui64TotalTime = 0;
        UINT64 ui64TotalTime2 = 0;
        DWORD dwIterations = 0;
        DWORD dwTime = 0;
        TCHAR tszMarkerName[MAX_MARKERNAME_LEN + 1] = {0};

        PresetPtr(pStream);
        hr = CreateStreamOnFile(id.GetFilename().c_str(), &pStream);
        if (E_OUTOFMEMORY == hr)
        {
            info(DETAIL, _T("Could not create stream, out of memory"));
            return getCode();
        }
        if (FAILED(hr) || IsPreset(pStream))
        {
            info(ABORT, _T("Could not create stream, hr = %d"), hr);
            return getCode();
        }

        PresetPtr(pImage);
        hr = pImagingFactory->CreateImageFromStream(pStream, &pImage);
        pStream->Release();
        if (E_OUTOFMEMORY == hr)
        {
            info(DETAIL, _T("Could not create image from stream, out of memory"));
            return getCode();
        }
        else if (FAILED(hr) || IsPreset(pImage))
        {
            info(ABORT, _T("Could not create image from stream, hr = %08x (check for memory)"), hr);
            return getCode();
        }

        Perf_SetTestName(c_tszTestName);
        _sntprintf(tszMarkerName, MAX_MARKERNAME_LEN, _T("%s: %s"), 
            c_tszCreateBitmapFromImageInMemory,
            id.GetFilename().c_str());
        Perf_RegisterMark(MARK_CREATEBITMAPFROMIMAGEINMEMORY, tszMarkerName);

        for (int iCntr = 0; iCntr < c_iIters; iCntr++)
        {
            PresetPtr(pBitmapImage);
            Perf_MarkBegin(MARK_CREATEBITMAPFROMIMAGEINMEMORY);
            hr = pImagingFactory->CreateBitmapFromImage(pImage, 0, 0, PixelFormatDontCare, InterpolationHintDefault, &pBitmapImage);
            dwTime = Perf_MarkGetDuration(MARK_CREATEBITMAPFROMIMAGEINMEMORY);
            if (FAILED(hr) || !pBitmapImage || PTRPRESET == pBitmapImage)
            {
                info(FAIL, TEXT("CreateBitmapFromImage failed, hr = %08x, pBitmapImage = %p"),
                    hr, pBitmapImage);
                info(DETAIL, _LOCATIONT);
                break;
            }
            if (dwTime > dwMaxTime)
                dwMaxTime = dwTime;
            if (dwTime < dwMinTime)
                dwMinTime = dwTime;
            ui64TotalTime += dwTime;
            ui64TotalTime2 += dwTime * dwTime;
            dwIterations++;
            pBitmapImage->Release();
        }

        pImage->Release();

        DisplayMarkerData(tszMarkerName, dwMinTime, dwMaxTime, ui64TotalTime, ui64TotalTime2, dwIterations);
        
        return getCode();
    }

    INT TestCreateBitmapFromImageWithLoad(IImagingFactory* pImagingFactory, const CImageDescriptor& id)
    {
        const int c_iIters = 5;
        IBitmapImage* pBitmapImage = NULL;
        IImage * pImage;
        IStream * pStream;
        HRESULT hr;
        DWORD dwMinTime = 0xffffffff;
        DWORD dwMaxTime = 0;
        UINT64 ui64TotalTime = 0;
        UINT64 ui64TotalTime2 = 0;
        DWORD dwIterations = 0;
        DWORD dwTime = 0;
        TCHAR tszMarkerName[MAX_MARKERNAME_LEN + 1] = {0};

        Perf_SetTestName(c_tszTestName);
        Perf_RegisterMark(MARK_CREATEBITMAPFROMIMAGEWITHLOAD, TEXT("%s: %s"), 
            c_tszCreateBitmapFromImageWithLoad,
            id.GetFilename().c_str());

        for (int iCntr = 0; iCntr < c_iIters; iCntr++)
        {
            Perf_MarkBegin(MARK_CREATEBITMAPFROMIMAGEWITHLOAD);
            PresetPtr(pStream);
            hr = CreateStreamOnFile(id.GetFilename().c_str(), &pStream);
            if (E_OUTOFMEMORY == hr)
            {
                info(DETAIL, _T("Could not create stream, out of memory"));
                return getCode();
            }
            if (FAILED(hr) || IsPreset(pStream))
            {
                info(ABORT, _T("Could not create stream, hr = %d"), hr);
                return getCode();
            }
            
            PresetPtr(pImage);
            hr = pImagingFactory->CreateImageFromStream(pStream, &pImage);
            pStream->Release();
            if (E_OUTOFMEMORY == hr)
            {
                info(DETAIL, _T("Could not create image from stream, out of memory"));
                return getCode();
            }
            else if (FAILED(hr) || IsPreset(pImage))
            {
                info(ABORT, _T("Could not create image from stream, hr = %08x (check for memory)"), hr);
                return getCode();
            }
            PresetPtr(pBitmapImage);
            hr = pImagingFactory->CreateBitmapFromImage(pImage, 0, 0, PixelFormatDontCare, InterpolationHintDefault, &pBitmapImage);
            if (FAILED(hr) || !pBitmapImage || PTRPRESET == pBitmapImage)
            {
                info(FAIL, TEXT("CreateBitmapFromImage failed, hr = %08x, pBitmapImage = %p"),
                    hr, pBitmapImage);
                info(DETAIL, _LOCATIONT);
                break;
            }
            pBitmapImage->Release();
            pImage->Release();
            dwTime = Perf_MarkGetDuration(MARK_CREATEBITMAPFROMIMAGEWITHLOAD);
            if (dwTime > dwMaxTime)
                dwMaxTime = dwTime;
            if (dwTime < dwMinTime)
                dwMinTime = dwTime;
            ui64TotalTime += dwTime;
            ui64TotalTime2 += dwTime * dwTime;
            dwIterations++;
        }

        DisplayMarkerData(tszMarkerName, dwMinTime, dwMaxTime, ui64TotalTime, ui64TotalTime2, dwIterations);
        
        return getCode();
    }

    
    INT TestCreateBitmapFromImageFromFile(IImagingFactory* pImagingFactory, const CImageDescriptor& id)
    {
        const int c_iIters = 5;
        IBitmapImage* pBitmapImage = NULL;
        IImage * pImage;
        IStream * pStream;
        HRESULT hr;
        DWORD dwMinTime = 0xffffffff;
        DWORD dwMaxTime = 0;
        UINT64 ui64TotalTime = 0;
        UINT64 ui64TotalTime2 = 0;
        DWORD dwIterations = 0;
        DWORD dwTime = 0;
        TCHAR tszMarkerName[MAX_MARKERNAME_LEN + 1] = {0};

        
        Perf_SetTestName(c_tszTestName);
        Perf_RegisterMark(MARK_CREATEBITMAPFROMIMAGEFROMFILE, TEXT("%s: %s"), 
            c_tszCreateBitmapFromImageFromFile,
            id.GetFilename().c_str());

        for (int iCntr = 0; iCntr < c_iIters; iCntr++)
        {
            Perf_MarkBegin(MARK_CREATEBITMAPFROMIMAGEFROMFILE);
            PresetPtr(pStream);
            PresetPtr(pImage);
            hr = pImagingFactory->CreateImageFromFile(id.GetFilename().c_str(), &pImage);
            if (E_OUTOFMEMORY == hr)
            {
                info(DETAIL, _T("Could not create image from stream, out of memory"));
                return getCode();
            }
            else if (FAILED(hr) || IsPreset(pImage))
            {
                info(ABORT, _T("Could not create image from stream, hr = %08x (check for memory)"), hr);
                return getCode();
            }
            PresetPtr(pBitmapImage);
            hr = pImagingFactory->CreateBitmapFromImage(pImage, 0, 0, PixelFormatDontCare, InterpolationHintDefault, &pBitmapImage);
            if (FAILED(hr) || !pBitmapImage || PTRPRESET == pBitmapImage)
            {
                info(FAIL, TEXT("CreateBitmapFromImage failed, hr = %08x, pBitmapImage = %p"),
                    hr, pBitmapImage);
                info(DETAIL, _LOCATIONT);
                break;
            }
            pBitmapImage->Release();
            pImage->Release();
            dwTime = Perf_MarkGetDuration(MARK_CREATEBITMAPFROMIMAGEFROMFILE);
            if (dwTime > dwMaxTime)
                dwMaxTime = dwTime;
            if (dwTime < dwMinTime)
                dwMinTime = dwTime;
            ui64TotalTime += dwTime;
            ui64TotalTime2 += dwTime * dwTime;
            dwIterations++;
        }

        DisplayMarkerData(tszMarkerName, dwMinTime, dwMaxTime, ui64TotalTime, ui64TotalTime2, dwIterations);
        
        return getCode();
    }
    INT TestCreateBitmapFromImageCache(IImagingFactory* pImagingFactory, const CImageDescriptor& id)
    {
        const int c_iIters = 200;
        IBitmapImage* pBitmapImage = NULL;
        HRESULT hr;
        IImage * pImage;
        IStream * pStream;
        DWORD dwMinTime = 0xffffffff;
        DWORD dwMaxTime = 0;
        UINT64 ui64TotalTime = 0;
        UINT64 ui64TotalTime2 = 0;
        DWORD dwIterations = 0;
        DWORD dwTime = 0;
        TCHAR tszMarkerName[MAX_MARKERNAME_LEN + 1] = {0};

        PresetPtr(pStream);
        hr = CreateStreamOnFile(id.GetFilename().c_str(), &pStream);
        if (E_OUTOFMEMORY == hr)
        {
            info(DETAIL, _T("Could not create stream, out of memory"));
            return getCode();
        }
        if (FAILED(hr) || IsPreset(pStream))
        {
            info(ABORT, _T("Could not create stream, hr = %d"), hr);
            return getCode();
        }

        PresetPtr(pImage);
        hr = pImagingFactory->CreateImageFromStream(pStream, &pImage);
        pStream->Release();
        if (E_OUTOFMEMORY == hr)
        {
            info(DETAIL, _T("Could not create image from stream, out of memory"));
            return getCode();
        }
        else if (FAILED(hr) || IsPreset(pImage))
        {
            info(ABORT, _T("Could not create image from stream, hr = %08x (check for memory)"), hr);
            return getCode();
        }
        
        Perf_SetTestName(c_tszTestName);
        Perf_RegisterMark(MARK_CREATEBITMAPFROMIMAGECACHE, TEXT("%s: %s"), 
            c_tszCreateBitmapFromImageCache,
            id.GetFilename().c_str());
        pImage->SetImageFlags(ImageFlagsCaching);
        for (int iCntr = 0; iCntr < c_iIters; iCntr++)
        {
            PresetPtr(pBitmapImage);
            Perf_MarkBegin(MARK_CREATEBITMAPFROMIMAGECACHE);
            hr = pImagingFactory->CreateBitmapFromImage(pImage, 0, 0, PixelFormatDontCare, InterpolationHintDefault, &pBitmapImage);
            dwTime = Perf_MarkGetDuration(MARK_CREATEBITMAPFROMIMAGECACHE);
            if (FAILED(hr) || !pBitmapImage || PTRPRESET == pBitmapImage)
            {
                info(FAIL, TEXT("CreateBitmapFromImage failed, hr = %08x, pBitmapImage = %p"),
                    hr, pBitmapImage);
                info(DETAIL, _LOCATIONT);
                break;
            }
            SafeRelease(pBitmapImage);
            if (dwTime > dwMaxTime)
                dwMaxTime = dwTime;
            if (dwTime < dwMinTime)
                dwMinTime = dwTime;
            ui64TotalTime += dwTime;
            ui64TotalTime2 += dwTime * dwTime;
            dwIterations++;
        }

        SafeRelease(pImage);
        DisplayMarkerData(tszMarkerName, dwMinTime, dwMaxTime, ui64TotalTime, ui64TotalTime2, dwIterations);
        return getCode();
    }

    INT TestGetThumbnail(IImagingFactory* pImagingFactory, const CImageDescriptor& id)
    {
        const int c_iIters = 200;
        IImage* pImageThumb = NULL;
        IImage * pImage;
        IStream * pStream;
        HRESULT hr;
        DWORD dwMinTime = 0xffffffff;
        DWORD dwMaxTime = 0;
        UINT64 ui64TotalTime = 0;
        UINT64 ui64TotalTime2 = 0;
        DWORD dwIterations = 0;
        DWORD dwTime = 0;
        TCHAR tszMarkerName[MAX_MARKERNAME_LEN + 1] = {0};

        PresetPtr(pStream);
        hr = CreateStreamOnFile(id.GetFilename().c_str(), &pStream);
        if (E_OUTOFMEMORY == hr)
        {
            info(DETAIL, _T("Could not create stream, out of memory"));
            return getCode();
        }
        if (FAILED(hr) || IsPreset(pStream))
        {
            info(ABORT, _T("Could not create stream, hr = %d"), hr);
            return getCode();
        }

        PresetPtr(pImage);
        hr = pImagingFactory->CreateImageFromStream(pStream, &pImage);
        pStream->Release();
        if (E_OUTOFMEMORY == hr)
        {
            info(DETAIL, _T("Could not create image from stream, out of memory"));
            return getCode();
        }
        else if (FAILED(hr) || IsPreset(pImage))
        {
            info(ABORT, _T("Could not create image from stream, hr = %08x (check for memory)"), hr);
            return getCode();
        }
        
        Perf_SetTestName(c_tszTestName);
        Perf_RegisterMark(MARK_GETTHUMBNAIL, TEXT("%s: %s"), 
            c_tszGetThumbnail,
            id.GetFilename().c_str());

        for (int iCntr = 0; iCntr < c_iIters; iCntr++)
        {
            PresetPtr(pImageThumb);
            Perf_MarkBegin(MARK_GETTHUMBNAIL);
            hr = pImage->GetThumbnail(id.GetWidth() / 5 + 1, id.GetHeight() / 5 + 1, &pImageThumb);
            dwTime = Perf_MarkGetDuration(MARK_GETTHUMBNAIL);
            if (E_OUTOFMEMORY == hr)
            {
                info(DETAIL, _T("GetThumbnail returned out of memory"));
                continue;
            }
            else if (FAILED(hr) || IsPreset(pImageThumb))
            {
                info(FAIL, TEXT("GetThumbnail failed, hr = %08x, pImageThumb = %p"),
                    hr, pImageThumb);
                info(DETAIL, _LOCATIONT);
                break;
            }
            if (dwTime > dwMaxTime)
                dwMaxTime = dwTime;
            if (dwTime < dwMinTime)
                dwMinTime = dwTime;
            ui64TotalTime += dwTime;
            ui64TotalTime2 += dwTime * dwTime;
            dwIterations++;
            SafeRelease(pImageThumb);
        }

        pImage->Release();
        
        DisplayMarkerData(tszMarkerName, dwMinTime, dwMaxTime, ui64TotalTime, ui64TotalTime2, dwIterations);
        return getCode();
    }
    
    INT TestEncodeBitmapToMemory(IImagingFactory* pImagingFactory, const CImageDescriptor & id, const ImageCodecInfo * pici)
    {
        const int c_iIters = 100;
        const LARGE_INTEGER liZero = {0};
        IBitmapImage* pBitmapImage = NULL;
        IImage * pImage = NULL;
        IStream * pStream = NULL;
        IImageSink * pImageSink = NULL;
        IImageEncoder * pImageEncoder = NULL;
        HRESULT hr;
        DWORD dwMinTime = 0xffffffff;
        DWORD dwMaxTime = 0;
        UINT64 ui64TotalTime = 0;
        UINT64 ui64TotalTime2 = 0;
        DWORD dwIterations = 0;
        DWORD dwTime = 0;
        TCHAR tszMarkerName[MAX_MARKERNAME_LEN + 1] = {0};

        Perf_SetTestName(c_tszTestName);
        Perf_RegisterMark(MARK_ENCODEBITMAPTOMEMORY, TEXT("%s: %s -> %04dx%04d; %08x:%s"), 
            c_tszEncodeBitmapToMemory,
            pici->CodecName,
            id.GetWidth(), id.GetHeight(), (DWORD)id.GetPixelFormat(), g_mapPIXFMT[id.GetPixelFormat()]);

        PresetPtr(pBitmapImage);
        hr = pImagingFactory->CreateNewBitmap(id.GetWidth(), id.GetHeight(), id.GetPixelFormat(), &pBitmapImage);
        if (E_OUTOFMEMORY == hr)
        {
            info(DETAIL, _T("Could not create bitmap to encode, out of memory"));
            goto LPExit;
        }
        else if (FAILED(hr) || IsPreset(pBitmapImage))
        {
            info(ABORT, _T("Could not create bitmap to encode, hr = %08x"), hr);
            info(DETAIL, _LOCATIONT);
            goto LPExit;
        }

        PresetPtr(pImage);
        hr = pBitmapImage->QueryInterface(IID_IImage, (void**)&pImage);
        if (FAILED(hr) || IsPreset(pImage))
        {
            info(ABORT, _T("Could not convert bitmap to image to encode, hr = %08x"), hr);
            info(DETAIL, _LOCATIONT);
            goto LPExit;
        }
        SafeRelease(pBitmapImage);

        PresetPtr(pStream);
        for (int iCntr = 0; iCntr < c_iIters + 1; iCntr++)
        {

            if (IsPreset(pStream))
            {
                hr = CreateStreamOnHGlobal(NULL, TRUE, &pStream);
                if (E_OUTOFMEMORY == hr)
                {
                    info(DETAIL, _T("Could not create stream, out of memory"));
                    goto LPExit;
                }
                else if (FAILED(hr) || IsPreset(pStream))
                {
                    info(ABORT, _T("Could not create stream, hr = %d"), hr);
                    goto LPExit;
                }
                info(DETAIL, _T("Encoding first time to prepare stream"));
                hr = pStream->SetSize(c_uliMaxStreamSize);
                if (FAILED(hr))
                {
                    info(ECHO, _T("SetSize failed, hr = %08x"), hr);
                    goto LPExit;
                }
            }
            else
            {
                pStream->Seek(liZero, STREAM_SEEK_SET, NULL);
            }

            PresetPtr(pImageEncoder);
            VerifyHRSuccess(pImagingFactory->CreateImageEncoderToStream(&(pici->Clsid), pStream, &pImageEncoder));
            if (IsPreset(pImageEncoder))
            {
                info(FAIL, TEXT("ImageEncoder pointer not set by CreateImageEncoderToStream for encoder %s"),
                    pici->CodecName);
                info(DETAIL, _LOCATIONT);
                continue;
            }

            PresetPtr(pImageSink);
            hr = pImageEncoder->GetEncodeSink(&pImageSink);
            if (FAILED(hr) || IsPreset(pImageSink))
            {
                info(ABORT, _T("Could not get image sink from encoder, hr = %08x"), hr);
                info(DETAIL, _LOCATIONT);
                goto LPExit;
            }
        
            if (iCntr)
                Perf_MarkBegin(MARK_ENCODEBITMAPTOMEMORY);
            hr = pImage->PushIntoSink(pImageSink);
            if (iCntr)
            {
                dwTime = Perf_MarkGetDuration(MARK_ENCODEBITMAPTOMEMORY);
                if (dwTime > dwMaxTime)
                    dwMaxTime = dwTime;
                if (dwTime < dwMinTime)
                    dwMinTime = dwTime;
                ui64TotalTime += dwTime;
                ui64TotalTime2 += dwTime * dwTime;
                dwIterations++;
            }
            if (E_OUTOFMEMORY == hr)
            {
                info(ECHO, _T("PushIntoSink could not complete, out of memory"));
                break;
            }
            else if (FAILED(hr))
            {
                info(FAIL, TEXT("PushIntoSink failed, hr = %08x"), hr);
                info(DETAIL, _LOCATIONT);
            }
            if (!iCntr)
                info(DETAIL, _T("Stream prepared, testing encoding."));
            SafeRelease(pImageSink);
            SafeRelease(pImageEncoder);
        }

LPExit:
        SafeRelease(pImageSink);
        SafeRelease(pImageEncoder);
        SafeRelease(pImage);
        SafeRelease(pBitmapImage);
        SafeRelease(pStream);
        DisplayMarkerData(tszMarkerName, dwMinTime, dwMaxTime, ui64TotalTime, ui64TotalTime2, dwIterations);
        return getCode();
    }
    
    INT TestEncodeBitmapWithSave(IImagingFactory* pImagingFactory, const CImageDescriptor & id, const ImageCodecInfo * pici)
    {
        const int c_iIters = 5;
        IBitmapImage* pBitmapImage = NULL;
        IImage * pImage = NULL;
        IStream * pStream = NULL;
        IImageSink * pImageSink = NULL;
        IImageEncoder * pImageEncoder = NULL;
        HRESULT hr;
        const LARGE_INTEGER liZero = {0};
        DWORD dwMinTime = 0xffffffff;
        DWORD dwMaxTime = 0;
        UINT64 ui64TotalTime = 0;
        UINT64 ui64TotalTime2 = 0;
        DWORD dwIterations = 0;
        DWORD dwTime = 0;
        TCHAR tszMarkerName[MAX_MARKERNAME_LEN + 1] = {0};

        Perf_SetTestName(c_tszTestName);
        Perf_RegisterMark(MARK_ENCODEBITMAPWITHSAVE, TEXT("%s: %s -> %04dx%04d; %08x:%s"), 
            c_tszEncodeBitmapWithSave,
            pici->CodecName,
            id.GetWidth(), id.GetHeight(), (DWORD)id.GetPixelFormat(), g_mapPIXFMT[id.GetPixelFormat()]);

        PresetPtr(pBitmapImage);
        hr = pImagingFactory->CreateNewBitmap(id.GetWidth(), id.GetHeight(), id.GetPixelFormat(), &pBitmapImage);
        if (E_OUTOFMEMORY == hr)
        {
            info(DETAIL, _T("Could not create bitmap to encode, out of memory"));
            goto LPExit;
        }
        else if (FAILED(hr) || IsPreset(pBitmapImage))
        {
            info(ABORT, _T("Could not create bitmap to encode, hr = %08x"), hr);
            info(DETAIL, _LOCATIONT);
            goto LPExit;
        }

        PresetPtr(pImage);
        hr = pBitmapImage->QueryInterface(IID_IImage, (void**)&pImage);
        if (FAILED(hr) || IsPreset(pImage))
        {
            info(ABORT, _T("Could not convert bitmap to image to encode, hr = %08x"), hr);
            info(DETAIL, _LOCATIONT);
            goto LPExit;
        }
        SafeRelease(pBitmapImage);

        PresetPtr(pStream);
        for (int iCntr = 0; iCntr < c_iIters + 1; iCntr++)
        {

            if (IsPreset(pStream))
            {
                hr = CreateStreamOnHGlobal(NULL, TRUE, &pStream);
                if (E_OUTOFMEMORY == hr)
                {
                    info(DETAIL, _T("Could not create stream, out of memory"));
                    goto LPExit;
                }
                else if (FAILED(hr) || IsPreset(pStream))
                {
                    info(ABORT, _T("Could not create stream, hr = %d"), hr);
                    goto LPExit;
                }
            }
            else
            {
                pStream->Seek(liZero, STREAM_SEEK_SET, NULL);
            }

            if (iCntr)
                Perf_MarkBegin(MARK_ENCODEBITMAPWITHSAVE);
            PresetPtr(pImageEncoder);
            VerifyHRSuccess(pImagingFactory->CreateImageEncoderToStream(&(pici->Clsid), pStream, &pImageEncoder));
            if (IsPreset(pImageEncoder))
            {
                info(FAIL, TEXT("ImageEncoder pointer not set by CreateImageEncoderToStream for encoder %s"),
                    pici->CodecName);
                info(DETAIL, _LOCATIONT);
                continue;
            }

            PresetPtr(pImageSink);
            hr = pImageEncoder->GetEncodeSink(&pImageSink);
            if (FAILED(hr) || IsPreset(pImageSink))
            {
                info(ABORT, _T("Could not get image sink from encoder, hr = %08x"), hr);
                info(DETAIL, _LOCATIONT);
                goto LPExit;
            }
        
            hr = pImage->PushIntoSink(pImageSink);
            if (E_OUTOFMEMORY == hr)
            {
                info(ECHO, _T("PushIntoSink could not complete, out of memory"));
                break;
            }
            else if (FAILED(hr))
            {
                info(FAIL, TEXT("PushIntoSink failed, hr = %08x"), hr);
                info(DETAIL, _LOCATIONT);
            }
            SafeRelease(pImageSink);
            if (iCntr)
                SaveStreamToFile(pStream, (g_tsImageDest + c_tszEncodeTempName).c_str());
            SafeRelease(pImageEncoder);
            if (iCntr)
            {
                dwTime = Perf_MarkGetDuration(MARK_ENCODEBITMAPWITHSAVE);
                if (dwTime > dwMaxTime)
                    dwMaxTime = dwTime;
                if (dwTime < dwMinTime)
                    dwMinTime = dwTime;
                ui64TotalTime += dwTime;
                ui64TotalTime2 += dwTime * dwTime;
                dwIterations++;
            }
        }

LPExit:
        SafeRelease(pImageSink);
        SafeRelease(pImage);
        SafeRelease(pBitmapImage);
        SafeRelease(pStream);
        DisplayMarkerData(tszMarkerName, dwMinTime, dwMaxTime, ui64TotalTime, ui64TotalTime2, dwIterations);
        return getCode();
    }
    
    INT TestEncodeBitmapToFile(IImagingFactory* pImagingFactory, const CImageDescriptor & id, const ImageCodecInfo * pici)
    {
        const int c_iIters = 5;
        IBitmapImage* pBitmapImage = NULL;
        IImage * pImage = NULL;
        IImageSink * pImageSink = NULL;
        IImageEncoder * pImageEncoder = NULL;
        HRESULT hr;
        DWORD dwMinTime = 0xffffffff;
        DWORD dwMaxTime = 0;
        UINT64 ui64TotalTime = 0;
        UINT64 ui64TotalTime2 = 0;
        DWORD dwIterations = 0;
        DWORD dwTime = 0;
        TCHAR tszMarkerName[MAX_MARKERNAME_LEN + 1] = {0};

        Perf_SetTestName(c_tszTestName);
        Perf_RegisterMark(MARK_ENCODEBITMAPTOFILE, TEXT("%s: %s -> %04dx%04d; %08x:%s"), 
            c_tszEncodeBitmapToFile,
            pici->CodecName,
            id.GetWidth(), id.GetHeight(), (DWORD)id.GetPixelFormat(), g_mapPIXFMT[id.GetPixelFormat()]);

        PresetPtr(pBitmapImage);
        hr = pImagingFactory->CreateNewBitmap(id.GetWidth(), id.GetHeight(), id.GetPixelFormat(), &pBitmapImage);
        if (E_OUTOFMEMORY == hr)
        {
            info(DETAIL, _T("Could not create bitmap to encode, out of memory"));
            goto LPExit;
        }
        else if (FAILED(hr) || IsPreset(pBitmapImage))
        {
            info(ABORT, _T("Could not create bitmap to encode, hr = %08x"), hr);
            info(DETAIL, _LOCATIONT);
            goto LPExit;
        }

        PresetPtr(pImage);
        hr = pBitmapImage->QueryInterface(IID_IImage, (void**)&pImage);
        if (FAILED(hr) || IsPreset(pImage))
        {
            info(ABORT, _T("Could not convert bitmap to image to encode, hr = %08x"), hr);
            info(DETAIL, _LOCATIONT);
            goto LPExit;
        }
        SafeRelease(pBitmapImage);

        for (int iCntr = 0; iCntr < c_iIters; iCntr++)
        {
            PresetPtr(pImageEncoder);
            if (!DeleteFile((g_tsImageDest + c_tszEncodeTempName).c_str()))
            {
                info(DETAIL, _T("Could not delete the temporary encode file, gle = %d"), GetLastError());
            }
            VerifyHRSuccess(pImagingFactory->CreateImageEncoderToFile(&(pici->Clsid), (g_tsImageSource + c_tszEncodeTempName).c_str(), &pImageEncoder));
            if (IsPreset(pImageEncoder))
            {
                info(FAIL, TEXT("ImageEncoder pointer not set by CreateImageEncoderToStream for encoder %s"),
                    pici->CodecName);
                info(DETAIL, _LOCATIONT);
                continue;
            }

            PresetPtr(pImageSink);
            hr = pImageEncoder->GetEncodeSink(&pImageSink);
            if (FAILED(hr) || IsPreset(pImageSink))
            {
                info(ABORT, _T("Could not get image sink from encoder, hr = %08x"), hr);
                info(DETAIL, _LOCATIONT);
                goto LPExit;
            }
        
            Perf_MarkBegin(MARK_ENCODEBITMAPTOFILE);
            hr = pImage->PushIntoSink(pImageSink);
            dwTime = Perf_MarkGetDuration(MARK_ENCODEBITMAPTOFILE);
            if (E_OUTOFMEMORY == hr)
            {
                info(ECHO, _T("PushIntoSink could not complete, out of memory"));
                break;
            }
            else if (FAILED(hr))
            {
                info(FAIL, TEXT("PushIntoSink failed, hr = %08x"), hr);
                info(DETAIL, _LOCATIONT);
            }
            SafeRelease(pImageSink);
            SafeRelease(pImageEncoder);
            if (dwTime > dwMaxTime)
                dwMaxTime = dwTime;
            if (dwTime < dwMinTime)
                dwMinTime = dwTime;
            ui64TotalTime += dwTime;
            ui64TotalTime2 += dwTime * dwTime;
            dwIterations++;
        }

LPExit:
        SafeRelease(pImageSink);
        SafeRelease(pImage);
        SafeRelease(pBitmapImage);
        SafeRelease(pImageEncoder);
        DisplayMarkerData(tszMarkerName, dwMinTime, dwMaxTime, ui64TotalTime, ui64TotalTime2, dwIterations);
        return getCode();
    }
}

