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
#define INITGUID
#include <windows.h>
#include "main.h"
//#include <imaging.h>
#undef INITGUID
#include "ft.h"

// shell and log vars
SPS_SHELL_INFO g_spsShellInfo;
CKato  *aLog = NULL;
PFNLoggerFunction g_pfnLogger = NULL;

HINSTANCE g_hInst = NULL;

TCHAR  *
CMapClass::operator[] (const GUID index)
{
    // initialize it to be null (this way we can test it in an if statement
    TCHAR * tcTmp = NULL;

    for (int i = 0; NULL != m_funcEntryStruct[i].szName; i++)
    {
        //info(DETAIL, _FT("Comparing index ") _GUIDT TEXT(" with ") _GUIDT,
        //    _GUIDEXP(index), _GUIDEXP(*((const GUID*)m_funcEntryStruct[i].pValue)));
        if (!IsBadReadPtr((const GUID *)m_funcEntryStruct[i].pValue, sizeof(GUID))
            && *((const GUID*)m_funcEntryStruct[i].pValue) == index)
        {
            //info(DETAIL, _FT("Success!"));
            tcTmp = m_funcEntryStruct[i].szName;
        }
    }

    return tcTmp;
}

TCHAR  *
CMapClass::operator[] (const LONG index)
{
    // initialize it to be null (this way we can test it in an if statement
    TCHAR * tcTmp = NULL;

    for (int i = 0; NULL != m_funcEntryStruct[i].szName; i++)
        if (m_funcEntryStruct[i].pValue == index)
            tcTmp = m_funcEntryStruct[i].szName;

    return tcTmp;
}

LONG
CMapClass::operator[](const TCHAR * index)
{
    LONG lVal = 0;
    
    for (int i = 0; NULL != m_funcEntryStruct[i].szName; i++)
    {
        //info(DETAIL, _FT("Comparing index (%s) with (%s)"), index, m_funcEntryStruct[i].szName);
        if (!_tcscmp(m_funcEntryStruct[i].szName, index))
        {
            lVal = m_funcEntryStruct[i].pValue;
        }
    }
    return lVal;
}

BEGINNAMEVALMAP(g_nvpIMGFMT)
    NAMEVALENTRY(&IMGFMT_MEMORYBMP)
    NAMEVALENTRY(&IMGFMT_JPEG)
    NAMEVALENTRY(&IMGFMT_BMP)
    NAMEVALENTRY(&IMGFMT_PNG)
    NAMEVALENTRY(&IMGFMT_TIFF)
    NAMEVALENTRY(&IMGFMT_ICO)
    NAMEVALENTRY(&IMGFMT_GIF)
ENDNAMEVALMAP()

CMapClass g_mapIMGFMT(g_nvpIMGFMT);

const GUID * g_rgImgFmt[] = {
    &IMGFMT_MEMORYBMP,
    &IMGFMT_JPEG,
    &IMGFMT_BMP,
    &IMGFMT_PNG,
    &IMGFMT_TIFF,
    &IMGFMT_ICO,
    &IMGFMT_GIF
};
const int g_cImgFmt = countof(g_rgImgFmt);

BEGINNAMEVALMAP(g_nvpDECODER)
    NAMEVALENTRY(&DECODER_TRANSCOLOR)
    NAMEVALENTRY(&DECODER_TRANSRANGE)
    NAMEVALENTRY(&DECODER_OUTPUTCHANNEL)
    NAMEVALENTRY(&DECODER_ICONRES)
    NAMEVALENTRY(&DECODER_USEICC)
ENDNAMEVALMAP()

CMapClass g_mapDECODER(g_nvpDECODER);

const GUID * g_rgDecoder[] = {
    &DECODER_TRANSCOLOR,
    &DECODER_TRANSRANGE,
    &DECODER_OUTPUTCHANNEL,
    &DECODER_ICONRES,
    &DECODER_USEICC,
};
const int g_cDecoder = countof(g_rgDecoder);

BEGINNAMEVALMAP(g_nvpPIXFMT)
    NAMEVALENTRY(PixelFormatIndexed)
    NAMEVALENTRY(PixelFormatGDI)
    NAMEVALENTRY(PixelFormatAlpha)
    NAMEVALENTRY(PixelFormatPAlpha)
    NAMEVALENTRY(PixelFormatExtended)
    NAMEVALENTRY(PixelFormatCanonical)
    NAMEVALENTRY(PixelFormatUndefined)
    NAMEVALENTRY(PixelFormatDontCare)
    NAMEVALENTRY(PixelFormat1bppIndexed)
    NAMEVALENTRY(PixelFormat4bppIndexed)
    NAMEVALENTRY(PixelFormat8bppIndexed)
    NAMEVALENTRY(PixelFormat16bppRGB555)
    NAMEVALENTRY(PixelFormat16bppRGB565)
    NAMEVALENTRY(PixelFormat16bppARGB1555)
    NAMEVALENTRY(PixelFormat24bppRGB)
    NAMEVALENTRY(PixelFormat32bppRGB)
    NAMEVALENTRY(PixelFormat32bppARGB)
    NAMEVALENTRY(PixelFormat32bppPARGB)
    NAMEVALENTRY(PixelFormat48bppRGB)
    NAMEVALENTRY(PixelFormat64bppARGB)
    NAMEVALENTRY(PixelFormat64bppPARGB)
ENDNAMEVALMAP()

CMapClass g_mapPIXFMT(g_nvpPIXFMT);

const PixelFormat g_rgPixelFormats[] = {
    PixelFormat1bppIndexed,
    PixelFormat4bppIndexed,
    PixelFormat8bppIndexed,
    PixelFormat16bppRGB555,
    PixelFormat16bppRGB565,
    PixelFormat16bppARGB1555,
    PixelFormat24bppRGB,
    PixelFormat32bppRGB,
    PixelFormat32bppARGB,
    PixelFormat32bppPARGB,
    PixelFormat48bppRGB,
    PixelFormat64bppARGB,
    PixelFormat64bppPARGB,
    //PixelFormat24bppBGR - Not a valid pixel format for creating bitmap images.
};

const int g_cPixelFormats = countof(g_rgPixelFormats);

BEGINNAMEVALMAP(g_nvpINTERP)
    NAMEVALENTRY(InterpolationHintDefault)
    NAMEVALENTRY(InterpolationHintNearestNeighbor)
    NAMEVALENTRY(InterpolationHintBilinear)
    NAMEVALENTRY(InterpolationHintAveraging)
    NAMEVALENTRY(InterpolationHintBicubic)
ENDNAMEVALMAP()

CMapClass g_mapINTERP(g_nvpINTERP);

const InterpolationHint g_rgInterpHints[] = {
    InterpolationHintDefault,
    InterpolationHintNearestNeighbor,
    InterpolationHintBilinear,
    InterpolationHintAveraging,
    InterpolationHintBicubic
};

const int g_cInterpHints = countof(g_rgInterpHints);

BEGINNAMEVALMAP(g_nvpFRAMEDIM)
    NAMEVALENTRY(&FRAMEDIM_PAGE)
    NAMEVALENTRY(&FRAMEDIM_TIME)
    NAMEVALENTRY(&FRAMEDIM_RESOLUTION)
ENDNAMEVALMAP()

CMapClass g_mapFRAMEDIM(g_nvpFRAMEDIM);

BEGINNAMEVALMAP(g_nvpDECODERINIT)
    NAMEVALENTRY(DecoderInitFlagNoBlock)
    NAMEVALENTRY(DecoderInitFlagBuiltIn1st)
    NAMEVALENTRY(DecoderInitFlagNone)
ENDNAMEVALMAP()

CMapClass g_mapDECODERINIT(g_nvpDECODERINIT);

BEGINNAMEVALMAP(g_nvpAllocType)
    NAMEVALENTRY(atGLOBAL)
    NAMEVALENTRY(atCTM)
    NAMEVALENTRY(atFM)
ENDNAMEVALMAP()

CMapClass g_mapAllocType(g_nvpAllocType);

BEGINNAMEVALMAP(g_nvpImgFlag)
    NAMEVALENTRY(ImageFlagsNone)
    NAMEVALENTRY(ImageFlagsScalable)
    NAMEVALENTRY(ImageFlagsHasAlpha)
    NAMEVALENTRY(ImageFlagsHasTranslucent)
    NAMEVALENTRY(ImageFlagsPartiallyScalable)
    NAMEVALENTRY(ImageFlagsColorSpaceRGB)
    NAMEVALENTRY(ImageFlagsColorSpaceCMYK)
    NAMEVALENTRY(ImageFlagsColorSpaceGRAY)
    NAMEVALENTRY(ImageFlagsColorSpaceYCBCR)
    NAMEVALENTRY(ImageFlagsColorSpaceYCCK)
    NAMEVALENTRY(ImageFlagsHasRealDPI)
    NAMEVALENTRY(ImageFlagsHasRealPixelSize)
    NAMEVALENTRY(ImageFlagsReadOnly)
    NAMEVALENTRY(ImageFlagsCaching)
ENDNAMEVALMAP()

CMapClass g_mapImgFlag(g_nvpImgFlag);

BEGINNAMEVALMAP(g_nvpTAG_TYPE)
    NAMEVALENTRY(PropertyTagTypeByte)
    NAMEVALENTRY(PropertyTagTypeASCII)
    NAMEVALENTRY(PropertyTagTypeShort)
    NAMEVALENTRY(PropertyTagTypeLong)
    NAMEVALENTRY(PropertyTagTypeRational)
    NAMEVALENTRY(PropertyTagTypeUndefined)
    NAMEVALENTRY(PropertyTagTypeSLONG)
    NAMEVALENTRY(PropertyTagTypeSRational)
ENDNAMEVALMAP()

CMapClass g_mapTAG_TYPE(g_nvpTAG_TYPE);

BEGINNAMEVALMAP(g_nvpTAG)
    NAMEVALENTRY(PropertyTagExifIFD)
    NAMEVALENTRY(PropertyTagGpsIFD)

    NAMEVALENTRY(PropertyTagNewSubfileType)
    NAMEVALENTRY(PropertyTagSubfileType)
    NAMEVALENTRY(PropertyTagImageWidth)
    NAMEVALENTRY(PropertyTagImageHeight)
    NAMEVALENTRY(PropertyTagBitsPerSample)
    NAMEVALENTRY(PropertyTagCompression)
    NAMEVALENTRY(PropertyTagPhotometricInterp)
    NAMEVALENTRY(PropertyTagThreshHolding)
    NAMEVALENTRY(PropertyTagCellWidth)
    NAMEVALENTRY(PropertyTagCellHeight)
    NAMEVALENTRY(PropertyTagFillOrder)
    NAMEVALENTRY(PropertyTagDocumentName)
    NAMEVALENTRY(PropertyTagImageDescription)
    NAMEVALENTRY(PropertyTagEquipMake)
    NAMEVALENTRY(PropertyTagEquipModel)
    NAMEVALENTRY(PropertyTagStripOffsets)
    NAMEVALENTRY(PropertyTagOrientation)
    NAMEVALENTRY(PropertyTagSamplesPerPixel)
    NAMEVALENTRY(PropertyTagRowsPerStrip)
    NAMEVALENTRY(PropertyTagStripBytesCount)
    NAMEVALENTRY(PropertyTagMinSampleValue)
    NAMEVALENTRY(PropertyTagMaxSampleValue)
    NAMEVALENTRY(PropertyTagXResolution)
    NAMEVALENTRY(PropertyTagYResolution)
    NAMEVALENTRY(PropertyTagPlanarConfig)
    NAMEVALENTRY(PropertyTagPageName)
    NAMEVALENTRY(PropertyTagXPosition)
    NAMEVALENTRY(PropertyTagYPosition)
    NAMEVALENTRY(PropertyTagFreeOffset)
    NAMEVALENTRY(PropertyTagFreeByteCounts)
    NAMEVALENTRY(PropertyTagGrayResponseUnit)
    NAMEVALENTRY(PropertyTagGrayResponseCurve)
    NAMEVALENTRY(PropertyTagT4Option)
    NAMEVALENTRY(PropertyTagT6Option)
    NAMEVALENTRY(PropertyTagResolutionUnit)
    NAMEVALENTRY(PropertyTagPageNumber)
    NAMEVALENTRY(PropertyTagTransferFuncition)
    NAMEVALENTRY(PropertyTagSoftwareUsed)
    NAMEVALENTRY(PropertyTagDateTime)
    NAMEVALENTRY(PropertyTagArtist)
    NAMEVALENTRY(PropertyTagHostComputer)
    NAMEVALENTRY(PropertyTagPredictor)
    NAMEVALENTRY(PropertyTagWhitePoint)
    NAMEVALENTRY(PropertyTagPrimaryChromaticities)
    NAMEVALENTRY(PropertyTagColorMap)
    NAMEVALENTRY(PropertyTagHalftoneHints)
    NAMEVALENTRY(PropertyTagTileWidth)
    NAMEVALENTRY(PropertyTagTileLength)
    NAMEVALENTRY(PropertyTagTileOffset)
    NAMEVALENTRY(PropertyTagTileByteCounts)
    NAMEVALENTRY(PropertyTagInkSet)
    NAMEVALENTRY(PropertyTagInkNames)
    NAMEVALENTRY(PropertyTagNumberOfInks)
    NAMEVALENTRY(PropertyTagDotRange)
    NAMEVALENTRY(PropertyTagTargetPrinter)
    NAMEVALENTRY(PropertyTagExtraSamples)
    NAMEVALENTRY(PropertyTagSampleFormat)
    NAMEVALENTRY(PropertyTagSMinSampleValue)
    NAMEVALENTRY(PropertyTagSMaxSampleValue)
    NAMEVALENTRY(PropertyTagTransferRange)

    NAMEVALENTRY(PropertyTagJPEGProc)
    NAMEVALENTRY(PropertyTagJPEGInterFormat)
    NAMEVALENTRY(PropertyTagJPEGInterLength)
    NAMEVALENTRY(PropertyTagJPEGRestartInterval)
    NAMEVALENTRY(PropertyTagJPEGLosslessPredictors)
    NAMEVALENTRY(PropertyTagJPEGPointTransforms)
    NAMEVALENTRY(PropertyTagJPEGQTables)
    NAMEVALENTRY(PropertyTagJPEGDCTables)
    NAMEVALENTRY(PropertyTagJPEGACTables)

    NAMEVALENTRY(PropertyTagYCbCrCoefficients)
    NAMEVALENTRY(PropertyTagYCbCrSubsampling)
    NAMEVALENTRY(PropertyTagYCbCrPositioning)
    NAMEVALENTRY(PropertyTagREFBlackWhite)

    NAMEVALENTRY(PropertyTagICCProfile)
    NAMEVALENTRY(PropertyTagGamma)
    NAMEVALENTRY(PropertyTagICCProfileDescriptor)
    NAMEVALENTRY(PropertyTagSRGBRenderingIntent)

    NAMEVALENTRY(PropertyTagImageTitle)
    NAMEVALENTRY(PropertyTagCopyright)

    NAMEVALENTRY(PropertyTagResolutionXUnit)
    NAMEVALENTRY(PropertyTagResolutionYUnit)
    NAMEVALENTRY(PropertyTagResolutionXLengthUnit)
    NAMEVALENTRY(PropertyTagResolutionYLengthUnit)
    NAMEVALENTRY(PropertyTagPrintFlags)
    NAMEVALENTRY(PropertyTagPrintFlagsVersion)
    NAMEVALENTRY(PropertyTagPrintFlagsCrop)
    NAMEVALENTRY(PropertyTagPrintFlagsBleedWidth)
    NAMEVALENTRY(PropertyTagPrintFlagsBleedWidthScale)
    NAMEVALENTRY(PropertyTagHalftoneLPI)
    NAMEVALENTRY(PropertyTagHalftoneLPIUnit)
    NAMEVALENTRY(PropertyTagHalftoneDegree)
    NAMEVALENTRY(PropertyTagHalftoneShape)
    NAMEVALENTRY(PropertyTagHalftoneMisc)
    NAMEVALENTRY(PropertyTagHalftoneScreen)
    NAMEVALENTRY(PropertyTagJPEGQuality)
    NAMEVALENTRY(PropertyTagGridSize)
    NAMEVALENTRY(PropertyTagThumbnailFormat)
    NAMEVALENTRY(PropertyTagThumbnailWidth)
    NAMEVALENTRY(PropertyTagThumbnailHeight)
    NAMEVALENTRY(PropertyTagThumbnailColorDepth)
    NAMEVALENTRY(PropertyTagThumbnailPlanes)
    NAMEVALENTRY(PropertyTagThumbnailRawBytes)
    NAMEVALENTRY(PropertyTagThumbnailSize)
    NAMEVALENTRY(PropertyTagThumbnailCompressedSize)
    NAMEVALENTRY(PropertyTagColorTransferFunction)
    NAMEVALENTRY(PropertyTagThumbnailData)
                                                
    NAMEVALENTRY(PropertyTagThumbnailImageWidth)
    NAMEVALENTRY(PropertyTagThumbnailImageHeight)
    NAMEVALENTRY(PropertyTagThumbnailBitsPerSample)
    NAMEVALENTRY(PropertyTagThumbnailCompression)
    NAMEVALENTRY(PropertyTagThumbnailPhotometricInterp)
    NAMEVALENTRY(PropertyTagThumbnailImageDescription)
    NAMEVALENTRY(PropertyTagThumbnailEquipMake)
    NAMEVALENTRY(PropertyTagThumbnailEquipModel)
    NAMEVALENTRY(PropertyTagThumbnailStripOffsets)
    NAMEVALENTRY(PropertyTagThumbnailOrientation)
    NAMEVALENTRY(PropertyTagThumbnailSamplesPerPixel)
    NAMEVALENTRY(PropertyTagThumbnailRowsPerStrip)
    NAMEVALENTRY(PropertyTagThumbnailStripBytesCount)
    NAMEVALENTRY(PropertyTagThumbnailResolutionX)
    NAMEVALENTRY(PropertyTagThumbnailResolutionY)
    NAMEVALENTRY(PropertyTagThumbnailPlanarConfig)
    NAMEVALENTRY(PropertyTagThumbnailResolutionUnit)
    NAMEVALENTRY(PropertyTagThumbnailTransferFunction)
    NAMEVALENTRY(PropertyTagThumbnailSoftwareUsed)
    NAMEVALENTRY(PropertyTagThumbnailDateTime)
    NAMEVALENTRY(PropertyTagThumbnailArtist)
    NAMEVALENTRY(PropertyTagThumbnailWhitePoint)
    NAMEVALENTRY(PropertyTagThumbnailPrimaryChromaticities)
    NAMEVALENTRY(PropertyTagThumbnailYCbCrCoefficients)
    NAMEVALENTRY(PropertyTagThumbnailYCbCrSubsampling)
    NAMEVALENTRY(PropertyTagThumbnailYCbCrPositioning)
    NAMEVALENTRY(PropertyTagThumbnailRefBlackWhite)
    NAMEVALENTRY(PropertyTagThumbnailCopyRight)

    NAMEVALENTRY(PropertyTagLuminanceTable)
    NAMEVALENTRY(PropertyTagChrominanceTable)

    NAMEVALENTRY(PropertyTagFrameDelay)
    NAMEVALENTRY(PropertyTagLoopCount)

    NAMEVALENTRY(PropertyTagPixelUnit)
    NAMEVALENTRY(PropertyTagPixelPerUnitX)
    NAMEVALENTRY(PropertyTagPixelPerUnitY)
    NAMEVALENTRY(PropertyTagPaletteHistogram)

    NAMEVALENTRY(PropertyTagExifExposureTime)
    NAMEVALENTRY(PropertyTagExifFNumber)

    NAMEVALENTRY(PropertyTagExifExposureProg)
    NAMEVALENTRY(PropertyTagExifSpectralSense)
    NAMEVALENTRY(PropertyTagExifISOSpeed)
    NAMEVALENTRY(PropertyTagExifOECF)

    NAMEVALENTRY(PropertyTagExifVer)
    NAMEVALENTRY(PropertyTagExifDTOrig)
    NAMEVALENTRY(PropertyTagExifDTDigitized)

    NAMEVALENTRY(PropertyTagExifCompConfig)
    NAMEVALENTRY(PropertyTagExifCompBPP)

    NAMEVALENTRY(PropertyTagExifShutterSpeed)
    NAMEVALENTRY(PropertyTagExifAperture)
    NAMEVALENTRY(PropertyTagExifBrightness)
    NAMEVALENTRY(PropertyTagExifExposureBias)
    NAMEVALENTRY(PropertyTagExifMaxAperture)
    NAMEVALENTRY(PropertyTagExifSubjectDist)
    NAMEVALENTRY(PropertyTagExifMeteringMode)
    NAMEVALENTRY(PropertyTagExifLightSource)
    NAMEVALENTRY(PropertyTagExifFlash)
    NAMEVALENTRY(PropertyTagExifFocalLength)
    NAMEVALENTRY(PropertyTagExifMakerNote)
    NAMEVALENTRY(PropertyTagExifUserComment)
    NAMEVALENTRY(PropertyTagExifDTSubsec)
    NAMEVALENTRY(PropertyTagExifDTOrigSS)
    NAMEVALENTRY(PropertyTagExifDTDigSS)

    NAMEVALENTRY(PropertyTagExifFPXVer)
    NAMEVALENTRY(PropertyTagExifColorSpace)
    NAMEVALENTRY(PropertyTagExifPixXDim)
    NAMEVALENTRY(PropertyTagExifPixYDim)
    NAMEVALENTRY(PropertyTagExifRelatedWav)
    NAMEVALENTRY(PropertyTagExifInterop)
    NAMEVALENTRY(PropertyTagExifFlashEnergy)
    NAMEVALENTRY(PropertyTagExifSpatialFR)
    NAMEVALENTRY(PropertyTagExifFocalXRes)
    NAMEVALENTRY(PropertyTagExifFocalYRes)
    NAMEVALENTRY(PropertyTagExifFocalResUnit)
    NAMEVALENTRY(PropertyTagExifSubjectLoc)
    NAMEVALENTRY(PropertyTagExifExposureIndex)
    NAMEVALENTRY(PropertyTagExifSensingMethod)
    NAMEVALENTRY(PropertyTagExifFileSource)
    NAMEVALENTRY(PropertyTagExifSceneType)
    NAMEVALENTRY(PropertyTagExifCfaPattern)

    NAMEVALENTRY(PropertyTagGpsVer)
    NAMEVALENTRY(PropertyTagGpsLatitudeRef)
    NAMEVALENTRY(PropertyTagGpsLatitude)
    NAMEVALENTRY(PropertyTagGpsLongitudeRef)
    NAMEVALENTRY(PropertyTagGpsLongitude)
    NAMEVALENTRY(PropertyTagGpsAltitudeRef)
    NAMEVALENTRY(PropertyTagGpsAltitude)
    NAMEVALENTRY(PropertyTagGpsGpsTime)
    NAMEVALENTRY(PropertyTagGpsGpsSatellites)
    NAMEVALENTRY(PropertyTagGpsGpsStatus)
    NAMEVALENTRY(PropertyTagGpsGpsMeasureMode)
    NAMEVALENTRY(PropertyTagGpsGpsDop)
    NAMEVALENTRY(PropertyTagGpsSpeedRef)
    NAMEVALENTRY(PropertyTagGpsSpeed)
    NAMEVALENTRY(PropertyTagGpsTrackRef)
    NAMEVALENTRY(PropertyTagGpsTrack)
    NAMEVALENTRY(PropertyTagGpsImgDirRef)
    NAMEVALENTRY(PropertyTagGpsImgDir)
    NAMEVALENTRY(PropertyTagGpsMapDatum)
    NAMEVALENTRY(PropertyTagGpsDestLatRef)
    NAMEVALENTRY(PropertyTagGpsDestLat)
    NAMEVALENTRY(PropertyTagGpsDestLongRef)
    NAMEVALENTRY(PropertyTagGpsDestLong)
    NAMEVALENTRY(PropertyTagGpsDestBearRef)
    NAMEVALENTRY(PropertyTagGpsDestBear)
    NAMEVALENTRY(PropertyTagGpsDestDistRef)
    NAMEVALENTRY(PropertyTagGpsDestDist)
ENDNAMEVALMAP()

CMapClass g_mapTAG(g_nvpTAG);

// All the Imaging-defined TAGs.
const UINT g_rgPropertyTags[] = {
    //---------------------------------------------------------------------------
    // Image property ID tags
    //---------------------------------------------------------------------------
    
    PropertyTagExifIFD,
    PropertyTagGpsIFD,
    
    PropertyTagNewSubfileType,
    PropertyTagSubfileType,
    PropertyTagImageWidth,
    PropertyTagImageHeight,
    PropertyTagBitsPerSample,
    PropertyTagCompression,
    PropertyTagPhotometricInterp,
    PropertyTagThreshHolding,
    PropertyTagCellWidth,
    PropertyTagCellHeight,
    PropertyTagFillOrder,
    PropertyTagDocumentName,
    PropertyTagImageDescription,
    PropertyTagEquipMake,
    PropertyTagEquipModel,
    PropertyTagStripOffsets,
    PropertyTagOrientation,
    PropertyTagSamplesPerPixel,
    PropertyTagRowsPerStrip,
    PropertyTagStripBytesCount,
    PropertyTagMinSampleValue,
    PropertyTagMaxSampleValue,
    PropertyTagXResolution,
    PropertyTagYResolution,
    PropertyTagPlanarConfig,
    PropertyTagPageName,
    PropertyTagXPosition,
    PropertyTagYPosition,
    PropertyTagFreeOffset,
    PropertyTagFreeByteCounts,
    PropertyTagGrayResponseUnit,
    PropertyTagGrayResponseCurve,
    PropertyTagT4Option,
    PropertyTagT6Option,
    PropertyTagResolutionUnit,
    PropertyTagPageNumber,
    PropertyTagTransferFuncition,
    PropertyTagSoftwareUsed,
    PropertyTagDateTime,
    PropertyTagArtist,
    PropertyTagHostComputer,
    PropertyTagPredictor,
    PropertyTagWhitePoint,
    PropertyTagPrimaryChromaticities,
    PropertyTagColorMap,
    PropertyTagHalftoneHints,
    PropertyTagTileWidth,
    PropertyTagTileLength,
    PropertyTagTileOffset,
    PropertyTagTileByteCounts,
    PropertyTagInkSet,
    PropertyTagInkNames,
    PropertyTagNumberOfInks,
    PropertyTagDotRange,
    PropertyTagTargetPrinter,
    PropertyTagExtraSamples,
    PropertyTagSampleFormat,
    PropertyTagSMinSampleValue,
    PropertyTagSMaxSampleValue,
    PropertyTagTransferRange,
    
    PropertyTagJPEGProc,
    PropertyTagJPEGInterFormat,
    PropertyTagJPEGInterLength,
    PropertyTagJPEGRestartInterval,
    PropertyTagJPEGLosslessPredictors,
    PropertyTagJPEGPointTransforms,
    PropertyTagJPEGQTables,
    PropertyTagJPEGDCTables,
    PropertyTagJPEGACTables,
    
    PropertyTagYCbCrCoefficients,
    PropertyTagYCbCrSubsampling,
    PropertyTagYCbCrPositioning,
    PropertyTagREFBlackWhite,
    
    PropertyTagICCProfile,
    PropertyTagGamma,
    PropertyTagICCProfileDescriptor,
    PropertyTagSRGBRenderingIntent,
    
    PropertyTagImageTitle,
    PropertyTagCopyright,
    
    // Extra TAGs (Like Adobe Image Information tags etc.)
    
    PropertyTagResolutionXUnit,
    PropertyTagResolutionYUnit,
    PropertyTagResolutionXLengthUnit,
    PropertyTagResolutionYLengthUnit,
    PropertyTagPrintFlags,
    PropertyTagPrintFlagsVersion,
    PropertyTagPrintFlagsCrop,
    PropertyTagPrintFlagsBleedWidth,
    PropertyTagPrintFlagsBleedWidthScale,
    PropertyTagHalftoneLPI,
    PropertyTagHalftoneLPIUnit,
    PropertyTagHalftoneDegree,
    PropertyTagHalftoneShape,
    PropertyTagHalftoneMisc,
    PropertyTagHalftoneScreen,
    PropertyTagJPEGQuality,
    PropertyTagGridSize,
    PropertyTagThumbnailFormat,
    PropertyTagThumbnailWidth,
    PropertyTagThumbnailHeight,
    PropertyTagThumbnailColorDepth,
    PropertyTagThumbnailPlanes,
    PropertyTagThumbnailRawBytes,
    PropertyTagThumbnailSize,
    PropertyTagThumbnailCompressedSize,
    PropertyTagColorTransferFunction,
    PropertyTagThumbnailData,

    // Thumbnail related TAGs
                                                    
    PropertyTagThumbnailImageWidth,
    PropertyTagThumbnailImageHeight,
    PropertyTagThumbnailBitsPerSample,
    PropertyTagThumbnailCompression,
    PropertyTagThumbnailPhotometricInterp,
    PropertyTagThumbnailImageDescription,
    PropertyTagThumbnailEquipMake,
    PropertyTagThumbnailEquipModel,
    PropertyTagThumbnailStripOffsets,
    PropertyTagThumbnailOrientation,
    PropertyTagThumbnailSamplesPerPixel,
    PropertyTagThumbnailRowsPerStrip,
    PropertyTagThumbnailStripBytesCount,
    PropertyTagThumbnailResolutionX,
    PropertyTagThumbnailResolutionY,
    PropertyTagThumbnailPlanarConfig,
    PropertyTagThumbnailResolutionUnit,
    PropertyTagThumbnailTransferFunction,
    PropertyTagThumbnailSoftwareUsed,
    PropertyTagThumbnailDateTime,
    PropertyTagThumbnailArtist,
    PropertyTagThumbnailWhitePoint,
    PropertyTagThumbnailPrimaryChromaticities,
    PropertyTagThumbnailYCbCrCoefficients,
    PropertyTagThumbnailYCbCrSubsampling,
    PropertyTagThumbnailYCbCrPositioning,
    PropertyTagThumbnailRefBlackWhite,
    PropertyTagThumbnailCopyRight,
    PropertyTagLuminanceTable,
    PropertyTagChrominanceTable,
    PropertyTagFrameDelay,
    PropertyTagLoopCount,
    PropertyTagPixelUnit,
    PropertyTagPixelPerUnitX,
    PropertyTagPixelPerUnitY,
    PropertyTagPaletteHistogram,
    
    // EXIF specific tag
    
    PropertyTagExifExposureTime,
    PropertyTagExifFNumber,
    
    PropertyTagExifExposureProg,
    PropertyTagExifSpectralSense,
    PropertyTagExifISOSpeed,
    PropertyTagExifOECF,
    
    PropertyTagExifVer,
    PropertyTagExifDTOrig,
    PropertyTagExifDTDigitized,
    
    PropertyTagExifCompConfig,
    PropertyTagExifCompBPP,
    
    PropertyTagExifShutterSpeed,
    PropertyTagExifAperture,
    PropertyTagExifBrightness,
    PropertyTagExifExposureBias,
    PropertyTagExifMaxAperture,
    PropertyTagExifSubjectDist,
    PropertyTagExifMeteringMode,
    PropertyTagExifLightSource,
    PropertyTagExifFlash,
    PropertyTagExifFocalLength,
    PropertyTagExifMakerNote,
    PropertyTagExifUserComment,
    PropertyTagExifDTSubsec,
    PropertyTagExifDTOrigSS,
    PropertyTagExifDTDigSS,
    
    PropertyTagExifFPXVer,
    PropertyTagExifColorSpace,
    PropertyTagExifPixXDim,
    PropertyTagExifPixYDim,
    PropertyTagExifRelatedWav,
    PropertyTagExifInterop,
    PropertyTagExifFlashEnergy,
    PropertyTagExifSpatialFR,
    PropertyTagExifFocalXRes,
    PropertyTagExifFocalYRes,
    PropertyTagExifFocalResUnit,
    PropertyTagExifSubjectLoc,
    PropertyTagExifExposureIndex,
    PropertyTagExifSensingMethod,
    PropertyTagExifFileSource,
    PropertyTagExifSceneType,
    PropertyTagExifCfaPattern,
    
    PropertyTagGpsVer,
    PropertyTagGpsLatitudeRef,
    PropertyTagGpsLatitude,
    PropertyTagGpsLongitudeRef,
    PropertyTagGpsLongitude,
    PropertyTagGpsAltitudeRef,
    PropertyTagGpsAltitude,
    PropertyTagGpsGpsTime,
    PropertyTagGpsGpsSatellites,
    PropertyTagGpsGpsStatus,
    PropertyTagGpsGpsMeasureMode,
    PropertyTagGpsGpsDop,
    PropertyTagGpsSpeedRef,
    PropertyTagGpsSpeed,
    PropertyTagGpsTrackRef,
    PropertyTagGpsTrack,
    PropertyTagGpsImgDirRef,
    PropertyTagGpsImgDir,
    PropertyTagGpsMapDatum,
    PropertyTagGpsDestLatRef,
    PropertyTagGpsDestLat,
    PropertyTagGpsDestLongRef,
    PropertyTagGpsDestLong,
    PropertyTagGpsDestBearRef,
    PropertyTagGpsDestBear,
    PropertyTagGpsDestDistRef,
    PropertyTagGpsDestDist,
};

const int g_cPropertyTags = countof(g_rgPropertyTags);

const UINT g_rgrgInvalidSizes[][2] = {
    // These are unreasonably large, but not overflowing.
    {0xffff, 0xffff},
    {0x1, 0x3ffffff},
    {0x3ffffff, 0x1},
    {0x23c6, 0x23c6},
    // These are unreasonably large, but overflow to reasonable sizes.
    {0x10001, 0x10001},
    {0x10, 0x0200000a},
    {0x0200000a, 0x10},
    // Zero width/height should fail.
    {0, 32},
    {32, 0},
    {0, 5000},
    {5000, 0},
    {0, 0}
};

const int g_cInvalidSizes = countof(g_rgrgInvalidSizes);

const UINT g_rgrgValidSizes[][2] = {
    // Try some typical values
    {1, 1},
    // If 0, use a random number for a few times
    {1, 0},
    {0, 1},
    {1600, 1200},
    {1024, 768},
    {0,0}
};

const int g_cValidSizes = countof(g_rgrgValidSizes);


/***********************************************************************************
***
***   Global parameters
***
************************************************************************************/

CClParse *  g_pclpParameters = NULL;
tstring     g_tsImageSource = TEXT("\\");
tstring     g_tsImageDest = TEXT("\\");
tstring     g_tsCompServ = TEXT("");
tstring     g_tsCodecType = TEXT("");
BOOL        g_fBuiltinFirst = TRUE;
BOOL        g_fNoBuiltin = FALSE;
BOOL        g_fNoUser = FALSE;
BOOL        g_fLeaveOutput = FALSE;

UINT        g_uiCompPort = 4321;
BOOL        g_fBadParams = false;

/***********************************************************************************
***
***   Log Definitions
***
************************************************************************************/

#define MY_EXCEPTION          0
#define MY_FAIL               2
#define MY_ABORT              4
#define MY_SKIP               6
#define MY_NOT_IMPLEMENTED    8
#define MY_PASS              10

int GetModulePath(HMODULE hMod, TCHAR * tszPath, DWORD dwSize)
{
    int iLen = GetModuleFileName(hMod, tszPath, dwSize);

    while (iLen > 0 && tszPath[iLen - 1] != _T('\\'))
        --iLen;

    // Changing the item at iLen to '\0' will leave a trailing \ on the path.
    if (iLen >= 0)
    {
        tszPath[iLen] = 0;
    }
    return iLen;
}

BOOL    WINAPI
DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved)
{
    g_hInst = (HINSTANCE) hInstance;
    return 1;
}

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam)
{
    SPS_BEGIN_TEST *pBeginTestInfo;
    TCHAR tszModulePath[MAX_PATH + 1] = _T("");
    
    switch (uMsg)
    {

        case SPM_REGISTER:
            ((LPSPS_REGISTER) spParam)->lpFunctionTable = g_lpFTE;
#ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
#else // UNICODE
            return SPR_HANDLED;
#endif // UNICODE

        case SPM_LOAD_DLL:
            aLog = (CKato *) KatoGetDefaultObject();
            info(DETAIL, TEXT("Initializing COM"));
            CoInitializeEx(NULL,0);
#ifdef UNICODE
            ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif
            return SPR_HANDLED;
        case SPM_UNLOAD_DLL:
            info(DETAIL, TEXT("Uninitializing COM"));
            CoUninitialize();
            if (g_pclpParameters)
            {
                delete g_pclpParameters;
                g_pclpParameters = NULL;
            }
        case SPM_START_SCRIPT:
        case SPM_STOP_SCRIPT:
            return SPR_HANDLED;

        case SPM_SHELL_INFO:
            g_spsShellInfo = *(LPSPS_SHELL_INFO) spParam;
            g_hInst = g_spsShellInfo.hLib;

            GetModulePath(g_hInst, tszModulePath, countof(tszModulePath)); 
            g_tsImageSource = tszModulePath;
            g_tsImageDest = tszModulePath;
            
            if (!g_pclpParameters)
            {
                g_pclpParameters = new CClParse(g_spsShellInfo.szDllCmdLine);
            }
            if (g_pclpParameters)
            {
                if (!UseCommandLineParameters(g_pclpParameters))
                {
                    // UseCommandLineParameters returns false if the user asked
                    // for usage info (/?)
                    info(ECHO, TEXT("Returning SPR_FAIL to abort test execution."));
                    return SPR_FAIL;
                }
            }
            return SPR_HANDLED;

        case SPM_BEGIN_GROUP:
            return SPR_HANDLED;

        case SPM_END_GROUP:
            return SPR_HANDLED;

        case SPM_BEGIN_TEST:
            pBeginTestInfo = reinterpret_cast<SPS_BEGIN_TEST *>(spParam);
            srand(pBeginTestInfo->dwRandomSeed);
            aLog->BeginLevel(((LPSPS_BEGIN_TEST) spParam)->lpFTE->dwUniqueID, TEXT("BEGIN TEST: <%s>, Thds=%u,"),
                             ((LPSPS_BEGIN_TEST) spParam)->lpFTE->lpDescription, ((LPSPS_BEGIN_TEST) spParam)->dwThreadCount);
            return SPR_HANDLED;

        case SPM_END_TEST:
            aLog->EndLevel(TEXT("END TEST: <%s>"), ((LPSPS_END_TEST) spParam)->lpFTE->lpDescription);
            return SPR_HANDLED;

        case SPM_EXCEPTION:
            aLog->Log(MY_EXCEPTION, TEXT("Exception occurred!"));
            return SPR_HANDLED;
    }
    return SPR_NOT_HANDLED;
}

bool UseCommandLineParameters(CClParse * pclpParameters)
{
    TCHAR tszOption[1024];
    if (pclpParameters->GetOptString(TEXT("Source"), tszOption, countof(tszOption)))
    {
        g_tsImageSource = tszOption;
        if (g_tsImageSource.find_last_of(_T('\\')) != g_tsImageSource.size() - 1)
        {
            g_tsImageSource += _T('\\');
        }
    }
    if (pclpParameters->GetOptString(TEXT("Dest"), tszOption, countof(tszOption)))
    {
        srand(GetTickCount());
        g_tsImageDest = tszOption;
        
        if (g_tsImageDest.find_last_of(_T('\\')) != g_tsImageDest.size() - 1)
        {
            g_tsImageDest += _T('\\');
        }
        g_tsImageDest += _itot(rand(), tszOption, 10);
        g_tsImageDest += _T('\\');
    }
    if (pclpParameters->GetOptString(TEXT("CompServ"), tszOption, countof(tszOption)))
    {
        g_tsCompServ = tszOption;
    }
    if (pclpParameters->GetOptString(TEXT("CodecType"), tszOption, countof(tszOption)))
    {
        g_tsCodecType = tszOption;
        g_tsCodecType = _T("image/") + g_tsCodecType;
    }
    pclpParameters->GetOptVal(TEXT("CompPort"), (DWORD*) &g_uiCompPort);
    g_fBadParams    = pclpParameters->GetOpt(TEXT("Badparams"));
    g_fBuiltinFirst = pclpParameters->GetOpt(TEXT("BuiltinFirst"));
    g_fNoBuiltin    = pclpParameters->GetOpt(TEXT("NoBuiltin"));
    g_fNoUser       = pclpParameters->GetOpt(TEXT("NoUser"));
    g_fLeaveOutput  = pclpParameters->GetOpt(TEXT("LeaveOutput"));

    if (pclpParameters->GetOpt(TEXT("?")) || 
        pclpParameters->GetOpt(TEXT("h")) || 
        pclpParameters->GetOpt(TEXT("Help")))
    {
        DisplayUsage();
        return false;
    }
    
    info(DETAIL, _T("Command Line Parameters are available for this test, use /? to display"));
    info(DETAIL, _T("(for example: tux -o -d ImgCodecTK -c \"/?\")"));

    return true;
}

void DisplayUsage()
{
    info(DETAIL, _T("Usage for ImgCodecTK test suite:"));
    info(DETAIL, _T("    tux <tux parameters> -d ImgCodecTK [-x<test-cases>] [-c \"<options>\"]"));
    info(DETAIL, _T("The options available are (encoder and decoder parameters can be used together):"));
    info(DETAIL, _T("  /Source <image source>    : The directory from which to use images for testing."));
    info(DETAIL, _T("                              default: \\release"));
    info(DETAIL, _T("  /Dest <image dest>        : The directory to be used when saving images."));
    info(DETAIL, _T("                              default: \\release"));
    info(DETAIL, _T("  /CodecType <mimesubtype>  : Test only codecs that support the given mimesubtype."));
    info(DETAIL, _T("                              Use the last part of the image/<subtype> mimetype"));
    info(DETAIL, _T("                              (for example, \"/CodecType jpeg\")"));
    info(DETAIL, _T("  Decoder test options (only used for the decoder tests)"));
    info(DETAIL, _T("    /BuiltinFirst           : When choosing test decoder, prefer builtin over user"));
    info(DETAIL, _T("                              decoder if both are available. Without this parameter"));
    info(DETAIL, _T("                              the tests will default to a user decoder."));
    info(DETAIL, _T("  Encoder test options (only used for the Encoder tests)"));
    info(DETAIL, _T("    /NoBuiltin              : Do not test builtin encoders. Test will skip if"));
    info(DETAIL, _T("                              there are no user encoders or if both /NoBuiltin and"));
    info(DETAIL, _T("                              /NoUser options are specified."));
    info(DETAIL, _T("    /NoUser                 : Do not test user encoders."));
}

//******************************************************************************
void
info(int iType, LPCTSTR szFormat, ...)
{
    TCHAR   szBuffer[1024] = {NULL};
    TCHAR   szTabs[] = TEXT("                                                                       ");
    static int s_iIndent = 0;

    va_list pArgs;

    if (iType & UNINDENT)
    {
        s_iIndent -= 4;
        if (s_iIndent < 0)
        {
            s_iIndent = 0;
        }
    }
    if (iType & RESETINDENT)
    {
        s_iIndent = 0;
    }

    if (szFormat)
    {
        if (s_iIndent < _tcslen(szTabs))
        {
            szTabs[s_iIndent] = 0;
        }
        else
        {
            szTabs[_tcslen(szTabs)] = 0;
        }
        
        va_start(pArgs, szFormat);
        if(FAILED(StringCbVPrintf(szBuffer, countof(szBuffer) - 1, szFormat, pArgs)))
           OutputDebugString(TEXT("StringCbVPrintf failed"));
        va_end(pArgs);

        // If we need to use a different logging system than Kato, denoted by
        // having this function pointer set, then use that function instead of
        // aLog
        if (g_pfnLogger)
        {
            g_pfnLogger(iType & ~(INDENT | UNINDENT | RESETINDENT), szBuffer);
        }
        else
        {
            // Clear out the flags
            switch (iType & ~(INDENT | UNINDENT | RESETINDENT))
            {
                case FAIL:
                    aLog->Log(MY_FAIL, TEXT("%sFAIL: %s"), szTabs, szBuffer);
                    break;
                case ECHO:
                    aLog->Log(MY_PASS, TEXT("%s%s"), szTabs, szBuffer);
                    break;
                case DETAIL:
                    aLog->Log(MY_PASS + 1, TEXT("%s    %s"), szTabs, szBuffer);
                    break;
                case ABORT:
                    aLog->Log(MY_ABORT, TEXT("%sAbort: %s"), szTabs, szBuffer);
                    break;
                case SKIP:
                    aLog->Log(MY_SKIP, TEXT("%sSkip: %s"), szTabs, szBuffer);
                    break;
            }
        }
    }
    if (iType & INDENT)
    {
        s_iIndent += 4;
    }
}

//******************************************************************************
TESTPROCAPI getCode(void)
{

    for (int i = 0; i < 15; i++)
        if (aLog->GetVerbosityCount((DWORD) i) > 0)
            switch (i)
            {
                case MY_EXCEPTION:
                    return TPR_HANDLED;
                case MY_FAIL:
                    return TPR_FAIL;
                case MY_ABORT:
                    return TPR_ABORT;
                case MY_SKIP:
                    return TPR_SKIP;
                case MY_NOT_IMPLEMENTED:
                    return TPR_HANDLED;
                case MY_PASS:
                    return TPR_PASS;
                default:
                    return TPR_NOT_HANDLED;
            }
    return TPR_PASS;
}

HRESULT InternalVerifyHRSuccess(const TCHAR * tszFilename, int iLine, const TCHAR * tszFunction, HRESULT hr)
{
    if (FAILED(hr))
    {
        info(FAIL, TEXT("VerifyHRSuccess: %s failed (0x%08x);  %s:%d"), tszFunction, hr, tszFilename, iLine);
    }
    return hr;
}

HRESULT InternalVerifyHRFailure(const TCHAR * tszFilename, int iLine, const TCHAR * tszFunction, HRESULT hr)
{
    if (!FAILED(hr))
    {
        info(FAIL, TEXT("VerifyHRFailure: %s succeeded (0x%08x);  %s:%d"), tszFunction, hr, tszFilename, iLine);
    }
    return hr;
}

BOOL InternalVerifyConditionTrue(const TCHAR * tszFilename, int iLine, const TCHAR * tszFunction, BOOL fBool)
{
    if (!fBool)
    {
        info(FAIL, TEXT("VerifyConditionTrue: %s is false, expected true;  %s:%d"), tszFunction, tszFilename, iLine);
    }
    return fBool;
}

BOOL InternalVerifyConditionFalse(const TCHAR * tszFilename, int iLine, const TCHAR * tszFunction, BOOL fBool)
{
    if (fBool)
    {
        info(FAIL, TEXT("VerifyConditionFalse: %s is true (%d), expected false;  %s:%d"), tszFunction, fBool, tszFilename, iLine);
    }
    return fBool;
}

// The following function was replaced by the FileStream implementation.
// Though this function creates a fully tested stream object using 
// CreateStreamOnHGlobal, it allocates a memory block to hold the entire file,
// which is, frankly, ridiculous. The CFileStream object handles this in a much
// more efficient manner by using the Win32 APIs CreateFile, ReadFile, etc., 
// which means the file doesn't need to be completely loaded into memory.

// This function is still useful for performance testing, where the whole image
// needs to be loaded into memory to get rid of slow relfs problems.

HRESULT CreateStreamOnFile(const TCHAR * tszFilename, IStream ** ppStream)
{
    HRESULT hrRet = S_OK;
    HGLOBAL hg = NULL;
    HANDLE hFile = NULL;
    DWORD dwSize, dwRead;
    BYTE* pbLocked = NULL;

    // Open the file
    hFile = CreateFile(tszFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (INVALID_HANDLE_VALUE == hFile)
    {
        info(ABORT, TEXT("CreateFile failed with GLE = %d (%s)"), GetLastError(), tszFilename);
        hrRet = 0x80000000 + GetLastError();
        goto error;
    }

    dwSize = GetFileSize(hFile, NULL);
    if (0xffffffff == dwSize)
    {
        info(ABORT, TEXT("GetFileSize failed with GLE = %d"), GetLastError());
        hrRet = 0x80000000 + GetLastError();
        goto error;
    }

    // Open a memory object
    hg = GlobalAlloc(GMEM_MOVEABLE, dwSize);
    if (NULL == hg)
    {
        info(ABORT, TEXT("GlobalAlloc failed with GLE = %d"), GetLastError());
        hrRet = 0x80000000 + GetLastError();
        goto error;
    }

    // Ge a pointer to the memory we just allocated
    pbLocked = (BYTE*) GlobalLock(hg);
    if (NULL == pbLocked)
    {
        info(ABORT, TEXT("GlobalLock failed with GLE = %d"), GetLastError());
        hrRet = 0x80000000 + GetLastError();
        goto error;
    }

    // copy the file
    if (!ReadFile(hFile, pbLocked, dwSize, &dwRead, NULL))
    {
        info(ABORT, TEXT("ReadFile failed with GLE = %d"), GetLastError());
        hrRet = 0x80000000 + GetLastError();
        goto error;
    }

    GlobalUnlock(hg);
    
    // Create the stream
    hrRet = CreateStreamOnHGlobal(hg, TRUE, ppStream);

    CloseHandle(hFile);
    return hrRet;
error:
    if (pbLocked)
        GlobalUnlock(hg);
    if (hg)
        GlobalFree(hg);
    if (hFile)
        CloseHandle(hFile);
    return hrRet;
}


