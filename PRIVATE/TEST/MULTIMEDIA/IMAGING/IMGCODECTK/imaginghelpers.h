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
#include <imaging.h>
#include "ImageProvider.h"
#include <list>

namespace ImagingHelpers
{
    // Compares the image info structure with the expected values per the image descriptor.
    // If the comparison fails, VerifyImageParameters will print a failure message.
    void VerifyImageParameters(const ImageInfo & ii, const CImageDescriptor & id);

    // Create a memory buffer using the allocation method indicated by atType.
    void * GetBufferFromStream(IStream* pStream, AllocType atType, DWORD * pcbBuffer);

    // Generate a random integer in the range [iMin, iMax]
    int RandomInt(int iMin, int iMax);

    // Generate a random integer in the range [0, iMax] with a Gaussian distribution
    // centered at iCenter (with anything outside the range rerolled until it's in the range.
    int RandomGaussianInt(int iMin, int iMax, int iCenter, int iIterations);

    // Determine if the pixel format or interpolation hint is valid.
    bool IsValidPixelFormat(PixelFormat pf);
    bool IsValidInterpHint(InterpolationHint ih);

    // Generate a random guid for invalid parameter testing. The probability
    // of this being a valid guid is far out. Unfortunately, the COM API
    // CoCreateGuid is not built into the vanilla configurations.
    void CreateRandomGuid(GUID* pguid);

    // Opens the file (tszFilename) and parses through it. The guids in the file
    // are named (such as IMGFMT_JPEG) instead of listed. The map class is used
    // to map the guid names to the appropriate guids.
    HRESULT ParseGuidList(TCHAR * tszFilename, std::list<const GUID *> & lGuidList, CMapClass mcAvail);

    // Compares two ImageCodecInfo structures.
    bool ImageCodecInfoCompare(const ImageCodecInfo & ici1, const ImageCodecInfo & ici2);

    // Spits the ImageCodecInfo to the debug stream using info(DETAIL, ...).
    void DisplayCodecInfo(const ImageCodecInfo & ici);

    // Returns the difference between two values
    double GetErrorPercentage(double dfActual, double dfExpected);

    LONG HiMetricFromPixel(UINT uiPixel, double dfDpi);
    UINT PixelFromHiMetric(LONG lHM, double dfDpi);

    void GenerateRandomRect(RECT * prcBoundary, RECT * prc);
    void GenerateRandomInvalidRect(RECT * prcBoundary, RECT * prc);

    // Loads a 24bpp bitmap from a file.
    HBITMAP LoadDIBitmap(const TCHAR * tszName);

    UINT GetValidHeightFromWidth(UINT uiWidth, PixelFormat pf);

    HRESULT GetImageCodec(
        IImagingFactory * pImagingFactory, 
        const GUID * pguidImgFmt, 
        const TCHAR * tszMimeType,
        bool fBuiltin, 
        bool fEncoder,
        /*out*/ ImageCodecInfo * pici, 
        /*out*/ tstring & tsExtension);

    HRESULT GetSupportedCodecs(
        IImagingFactory* pImagingFactory,
        std::list<const GUID *> & lstDecoders, 
        std::list<const GUID *> & lstEncoders);
        
    int GetCorrectStride(UINT uiWidth, PixelFormat pf);

    void GetRandomData(void * pvBuffer, int cBytes);

    // Get and Set any pixel in a bitmap. Knows about pixel format.
    HRESULT SetBitmapPixel(UINT uix, UINT uiy, BitmapData * pbmd, unsigned __int64 ui64Value);
    HRESULT GetBitmapPixel(UINT uix, UINT uiy, BitmapData * pbmd, unsigned __int64 * pi64Value);

    // Create the path passed in, even if it means creating any paths below it as well.
    BOOL RecursiveCreateDirectory(tstring tsPath);

    HRESULT SaveStreamToFile(IStream * pStream, tstring tsFile);

    extern const int iFULLBRIGHT32;
    extern const int iFULLBRIGHT64;

    bool GetPixelFormatInformation(
        const PixelFormat     pf,
        unsigned __int64    & i64AlphaMask,
        unsigned __int64    & i64RedMask,
        unsigned __int64    & i64GreenMask,
        unsigned __int64    & i64BlueMask,
        int                 & iAlphaOffset,
        int                 & iRedOffset,
        int                 & iGreenOffset,
        int                 & iBlueOffset,
        int                 & iColorErrorThreshold,
        bool                & fPremult);

    HRESULT FillBitmapGradient(IBitmapImage* pBitmapImage);

    bool IsDesiredCodecType(IImagingFactory * pImagingFactory, const CImageDescriptor & id);
}
