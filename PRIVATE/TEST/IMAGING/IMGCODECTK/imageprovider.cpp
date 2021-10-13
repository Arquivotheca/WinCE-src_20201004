//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "ImageProvider.h"
#include "main.h"
#include "FileStream.h"
#include "ImagingHelpers.h"
#include <algorithm>

// This is a list of all the possible property ID tags in the imaging library.
// See imaging.h for comments.
BEGIN_CONFIG_TABLE(g_cmTags)
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

// Some parameters to keep track of file, etc.
    CONFIG_ENTRY(TESTTAG_SIMPLE_PARAMS)
    CONFIG_ENTRY(TESTTAG_NEXT_FILE)
    CONFIG_ENTRY(TESTTAG_THUMBNAIL_PRESENT)
    CONFIG_ENTRY(TESTTAG_FRAMEDIM)
    CONFIG_ENTRY(TESTTAG_FRAMECOUNT)
    CONFIG_ENTRY(TESTTAG_PERFORMANCE_TEST)
END_CONFIG_TABLE

//
// CImageDescriptor Methods
//
using namespace ImagingHelpers;
HRESULT CImageDescriptor::SetImage(
    const tstring & sFilename,
    const tstring & sReferenceBmp,
    const GUID * pguidImageFormat, 
    PixelFormat pfPixelFormat,
    UINT uiWidth,
    UINT uiHeight,
    double dfXdpi,
    double dfYdpi,
    UINT uiThreshold,
    UINT uiAvgThreshold,
    bool fInvalid)
{
    m_sFilename = sFilename;
    m_sReferenceBmp = sReferenceBmp;
    m_pguidImageFormat = pguidImageFormat;
    m_pfPixelFormat = pfPixelFormat;
    m_uiWidth = uiWidth;
    m_uiHeight = uiHeight;
    m_dfXdpi = dfXdpi;
    m_dfYdpi = dfYdpi;
    m_uiThreshold = uiThreshold;
    m_uiAvgThreshold = uiAvgThreshold;
    m_fInvalid = fInvalid;
    return S_OK;
}

HRESULT CImageDescriptor::SetConfigTags(const ConfigUty::CConfig & cfgTags)
{
    if (m_cfgTags.Copy(cfgTags))
        return S_OK;
    else
        return E_FAIL;
}

// Returns an addref'ed stream. The callee needs to release.
IStream* CImageDescriptor::GetStream(const TCHAR * tszReplayFile, int iSeed) const
{
    CFileStream* pfs = NULL;
    IStream* pStream = NULL;
    HRESULT hr;
    pfs = new CFileStream(NULL != tszReplayFile, iSeed);
    if (!pfs)
    {
        info(DETAIL, _FT("Out of memory"));
        return NULL;
    }
    hr = pfs->InitFile(m_sFilename.c_str(), tszReplayFile);
    if (FAILED(hr))
    {
        info(DETAIL, _FT("pfs->InitFile failed, hr = 0x%08x"), hr);
        delete pfs;
        return NULL;
    }
    
    pStream = static_cast<IStream*>(pfs);
    pStream->AddRef();
    return pStream;
}

IStream* CImageDescriptor::GetMemoryStream() const
{
    IStream* pStream = NULL;

    CreateStreamOnFile(m_sFilename.c_str(), &pStream);
    return pStream;
}

HRESULT CImageDescriptor::GetImageInterface(IImagingFactory * pImagingFactory, IImage ** ppImage, const TCHAR * tszReplayFile, int iSeed) const
{
    HRESULT hr;
    if (m_sFilename.empty())
    {
        info(DETAIL, _FT("Cannot create image without filename"));
        return E_FAIL;
    }
    if (NULL != tszReplayFile)
    {
        IStream* pStream = NULL;
        pStream = GetStream(tszReplayFile, iSeed);
        if (!pStream)
        {
            return E_FAIL;
        }
        
        info(DETAIL, _FT("Using CreateImageFromStream with randomized file (%s)"), m_sFilename.c_str());
        try
        {
            hr = pImagingFactory->CreateImageFromStream(pStream, ppImage);
        }
        catch (...)
        {
            info(FAIL, _FT("Exception occurred!"));
            info(DETAIL, _LOCATIONT);
            hr = HRESULT_FROM_WIN32(ERROR_EXCEPTION_IN_SERVICE);
        }
        
        // CreateImageFromStream AddRef's the stream pointer. We can now
        // release it ourselves.
        SAFERELEASE(pStream);
        return hr;
    }
    switch (RandomInt(0,2))
    {
        // Use CreateImageFromStream
        case 0:
        {
            IStream* pStream = NULL;
            pStream = GetStream();
            if (!pStream)
            {
                return E_FAIL;
            }

            info(DETAIL, _FT("Using CreateImageFromStream with file (%s)"), m_sFilename.c_str());
            try
            {
                hr = pImagingFactory->CreateImageFromStream(pStream, ppImage);
            }
            catch (...)
            {
                info(FAIL, _FT("Exception occurred!"));
                info(DETAIL, _LOCATIONT);
                hr = HRESULT_FROM_WIN32(ERROR_EXCEPTION_IN_SERVICE);
            }

            // CreateImageFromStream AddRef's the stream pointer. We can now
            // release it ourselves.
            SAFERELEASE(pStream);
            return hr;
        }
        case 1:
        {
            info(DETAIL, _FT("Using CreateImageFromFile with file (%s)"), m_sFilename.c_str());
            try
            {
                return pImagingFactory->CreateImageFromFile(m_sFilename.c_str(), ppImage);
            }
            catch (...)
            {
                info(FAIL, _FT("Exception occurred!"));
                info(DETAIL, _LOCATIONT);
                return HRESULT_FROM_WIN32(ERROR_EXCEPTION_IN_SERVICE);
            }
        }
        case 2:
        {
            AllocType rgatSupported[] = {
                //atGLOBAL,
                atCTM,
                atFM
            };
            
            BufferDisposalFlag rgbdfmap[] = {
                BufferDisposalFlagGlobalFree,    // atGLOBAL = 0
                BufferDisposalFlagCoTaskMemFree, // atCTM = 1
                BufferDisposalFlagUnmapView      // atFM = 2
            };

            AllocType atAlloc = rgatSupported[rand() % countof(rgatSupported)];
            BufferDisposalFlag bdf = rgbdfmap[atAlloc];

            IStream* pStream = GetStream();
            if (!pStream)
            {
                return E_FAIL;
            }

            DWORD cbBuffer = 0;
            void * pv = GetBufferFromStream(pStream, atAlloc, &cbBuffer);

            // We don't need the stream any more.
            SAFERELEASE(pStream);

            info(DETAIL, _FT("Using CreateImageFromBuffer with file (%s), AllocType (%s)"), 
                m_sFilename.c_str(), g_mapAllocType[atAlloc]);

            try
            {
                hr = pImagingFactory->CreateImageFromBuffer(pv, cbBuffer, bdf, ppImage);
            }
            catch (...)
            {
                info(FAIL, _FT("Exception occurred!"));
                info(DETAIL, _LOCATIONT);
                hr = HRESULT_FROM_WIN32(ERROR_EXCEPTION_IN_SERVICE);
            }
            if (FAILED(hr))
            {
                // If CreateImageFromBuffer fails, the caller is responsible for
                // disposing the buffer.
                switch(bdf)
                {
                case BufferDisposalFlagGlobalFree:
                    GlobalFree((HGLOBAL) pv);
                    break;
                case BufferDisposalFlagCoTaskMemFree:
                    CoTaskMemFree(pv);
                    break;
                case BufferDisposalFlagUnmapView:
                    UnmapViewOfFile(pv);
                    break;
                }
            }
            return hr;
        }
    }
    return E_NOTIMPL;
}

HRESULT CImageDescriptor::GetBitmapImageInterface(
    IImagingFactory * pImagingFactory,
    IBitmapImage ** ppBitmapImage) const
{
    if (!m_sFilename.empty())
    {
        info(DETAIL, _FT("Warning: creating bitmap image when filename is present"));
    }

    if (!pImagingFactory || IsBadWritePtr(ppBitmapImage, sizeof(IBitmapImage*)))
    {
        info(DETAIL, _FT("Bad pointers"));
        return E_POINTER;
    }

    return pImagingFactory->CreateNewBitmap(m_uiWidth, m_uiHeight, m_pfPixelFormat, ppBitmapImage);
}

// Returns the file name
const tstring & CImageDescriptor::GetFilename() const
{
    return m_sFilename;
}
// Returns the file name or the reference bmp
const tstring & CImageDescriptor::GetReferenceBmp() const
{
    return m_sReferenceBmp;
}

const tstring & CImageDescriptor::GetMimeType() const
{
    return m_sMimeType;
}

const GUID * CImageDescriptor::GetImageFormat() const
{
    return m_pguidImageFormat;
}

PixelFormat CImageDescriptor::GetPixelFormat() const
{
    return m_pfPixelFormat;
}

CImageDescriptor::CImageDescriptor()
  : m_sFilename(TEXT("")),
    m_sReferenceBmp(TEXT("")),
    m_sMimeType(TEXT("")),
    m_pguidImageFormat(NULL),
    m_pfPixelFormat(PixelFormatUndefined),
    m_uiWidth(0),
    m_uiHeight(0),
    m_dfXdpi(0.0),
    m_dfYdpi(0.0),
    m_uiThreshold(0),
    m_uiAvgThreshold(0),
    m_fInvalid(false)
{
}

CImageDescriptor::CImageDescriptor(const CImageDescriptor & idOther)
  : m_sFilename(TEXT("")),
    m_sReferenceBmp(TEXT("")),
    m_sMimeType(TEXT("")),
    m_pguidImageFormat(NULL),
    m_pfPixelFormat(PixelFormatUndefined),
    m_uiWidth(0),
    m_uiHeight(0),
    m_dfXdpi(0.0),
    m_dfYdpi(0.0),
    m_uiThreshold(0),
    m_uiAvgThreshold(0),
    m_fInvalid(false)
{
    Clone(idOther);
}

CImageDescriptor::~CImageDescriptor()
{
}

CImageDescriptor & CImageDescriptor::operator=(const CImageDescriptor & idOther)
{
    Clone(idOther);
    return *this;
}

HRESULT CImageDescriptor::Clone(const CImageDescriptor & idOther)
{
    m_sFilename = idOther.m_sFilename;
    m_sReferenceBmp = idOther.m_sReferenceBmp;
    m_sMimeType = idOther.m_sMimeType;
    m_pguidImageFormat = idOther.m_pguidImageFormat;
    m_pfPixelFormat = idOther.m_pfPixelFormat;
    m_uiWidth = idOther.m_uiWidth;
    m_uiHeight = idOther.m_uiHeight;
    m_dfXdpi = idOther.m_dfXdpi;
    m_dfYdpi = idOther.m_dfYdpi;
    m_uiThreshold = idOther.m_uiThreshold;
    m_uiAvgThreshold = idOther.m_uiAvgThreshold;
    m_fInvalid = idOther.m_fInvalid;

    m_cfgTags.Copy(idOther.m_cfgTags);
    return S_OK;
}


//
// CImageProvider Implementation
//

using namespace std;
CImageProvider::~CImageProvider()
{
    for (m_imIter = m_mmImages.begin(); m_imIter != m_mmImages.end(); m_imIter++)
    {
        delete (*m_imIter).second;
    }
}

HRESULT CImageProvider::Initialize(tstring sConfigFile, IImagingFactory* pImagingFactory)
{
    FILE * pConfigFile;
    TCHAR tszLine[1024];

    if (find(m_ltsLoadedFiles.begin(), m_ltsLoadedFiles.end(), sConfigFile) != m_ltsLoadedFiles.end())
    {
        return S_FALSE;
    }
    
    pConfigFile = _tfopen(sConfigFile.c_str(), TEXT("r"));
    if (!pConfigFile)
    {
        return E_FAIL;
    }

    while (_fgetts(tszLine, 1024, pConfigFile))
    {
        CImageDescriptor * pid = new CImageDescriptor;
        if (!pid)
        {
            // we can't continue.
            break;
        }
        if (S_OK == ParseSimpleParams(tszLine, *pid, pImagingFactory))
        {
            m_mmImages.insert(pair<const GUID *, CImageDescriptor*>(pid->GetImageFormat(), pid));
        }
        else
        {
            delete pid;
        }
    }

    fclose(pConfigFile);

    m_ltsLoadedFiles.push_back(sConfigFile);
    m_fInitialized = true;
    
    return S_OK;
}

HRESULT CImageProvider::InitializeForBitmaps()
{
    int iFormat;
    UINT uiWidth;
    UINT uiHeight;
    int iSizes;
    CImageDescriptor * pid;
    tstring tsEmpty(TEXT(""));
    UINT rgrguiSizes[][2] = {
        {1, 100},
        {100, 1},
        {100, 100},
        {101, 101},
        {1, 1}
    };
    
    for (iFormat = 0; iFormat < g_cPixelFormats; iFormat++)
    {
        // Save the most recent stride for OOM message (on OOM, cannot
        // determine what the stride is).
        int iLastStride = 0;
        info(DETAIL, TEXT("Valid size, pixel format (%s)"), g_mapPIXFMT[g_rgPixelFormats[iFormat]]);
        for (iSizes = 0; iSizes < countof(rgrguiSizes); iSizes++)
        {
            uiWidth = rgrguiSizes[iSizes][0];
            uiHeight = rgrguiSizes[iSizes][1];

            if (!uiWidth)
            {
                uiWidth = RandomInt(1, 10000);
            }

            if (!uiHeight)
            {
                uiHeight = GetValidHeightFromWidth(uiWidth, g_rgPixelFormats[iFormat]);
            }
            pid = new CImageDescriptor;
            if (!pid)
                break;

            pid->SetImage(
                tsEmpty, 
                tsEmpty, 
                &IMGFMT_MEMORYBMP, 
                g_rgPixelFormats[iFormat],
                uiWidth,
                uiHeight,
                96, 96, 0, 0);
            m_mmImages.insert(pair<const GUID *, CImageDescriptor*>(&IMGFMT_MEMORYBMP, pid));
        }
    }
    m_fInitialized = true;
    return S_OK;
}

HRESULT CImageProvider::InitializeComplex(tstring sConfigFile, IImagingFactory* pImagingFactory)
{
    using namespace ConfigUty;
    CConfig cfgTags;
    TCHAR tszNextFile[MAX_PATH] = {0};
    TCHAR tszSimpleParams[1024] = {0};
    CImageDescriptor * pid;
    HRESULT hr;
    list<tstring> lstFilesHit;
    tstring sNextFile;
    info(DETAIL, _T("Initializing ImageProvider with configuration file \"%s\""), sConfigFile.c_str());
    cfgTags.Initialize(sConfigFile.c_str(), g_cmTags, TEXT("header"));
    if (-1 == cfgTags.QueryString(TESTTAG_NEXT_FILE, tszNextFile, countof(tszNextFile)))
    {
        info(DETAIL, _T("Invalid configuration file specified."));
        return E_FAIL;
    }

    while(1)
    {
        cfgTags.Initialize(sConfigFile.c_str(), g_cmTags, tszNextFile);
        if (-1 == cfgTags.QueryString(TESTTAG_NEXT_FILE, tszNextFile, countof(tszNextFile)))
        {
            break;
        }

        sNextFile = tszNextFile;
        if (find(lstFilesHit.begin(), lstFilesHit.end(), sNextFile) != lstFilesHit.end())
        {
            info(DETAIL, TEXT("Recursion found in config file \"%s\", file name \"%s\" repeated"), sConfigFile.c_str(), tszNextFile);
            return E_FAIL;
        }
        lstFilesHit.push_back(sNextFile);

        if (-1 == cfgTags.QueryString(TESTTAG_SIMPLE_PARAMS, tszSimpleParams, countof(tszSimpleParams)))
        {
            continue;
        }
        
        pid = new CImageDescriptor;
        if (!pid)
        {
            return E_OUTOFMEMORY;
        }

        hr = ParseSimpleParams(tszSimpleParams, *pid, pImagingFactory);
        if (S_OK != hr)
        {
            delete pid;
            continue;
        }
        pid->SetConfigTags(cfgTags);
        m_mmImages.insert(pair<const GUID *, CImageDescriptor*>(pid->GetImageFormat(), pid));
    }
    
    m_fInitialized = true;
    return S_OK;
}

HRESULT CImageProvider::InitializeSimple(tstring sConfigFile, IImagingFactory * pImagingFactory)
{
    TCHAR * tszFilename;
    TCHAR * tszImgFmt;
    TCHAR * tszInvalid;
    FILE * pConfigFile;
    TCHAR tszLine[1024];
    TCHAR tszFilenameDelim[] = TEXT("?");
    TCHAR tszImgFmtDelim[] = TEXT("\n \t");
    GUID * pguidImgFmt = NULL;
    tstring sFilename;
    tstring sReference = TEXT("");
    bool fInvalid = false;
    
    list<const GUID *> lstSupportedDecoders;
    list<const GUID *> lstSupportedEncoders;

    if (pImagingFactory)
    {
        GetSupportedCodecs(pImagingFactory, lstSupportedDecoders, lstSupportedEncoders);
    }

    if (find(m_ltsLoadedFiles.begin(), m_ltsLoadedFiles.end(), sConfigFile) != m_ltsLoadedFiles.end())
    {
        return S_FALSE;
    }
    
    pConfigFile = _tfopen(sConfigFile.c_str(), TEXT("r"));
    if (!pConfigFile)
    {
        return E_FAIL;
    }

    while (_fgetts(tszLine, 1024, pConfigFile))
    {
        if (TEXT('#') == tszLine[0])
        {
            continue;
        }
        tszFilename = _tcstok(tszLine, tszFilenameDelim);
        if (!tszFilename)
        {
            continue;
        }
        tszImgFmt = _tcstok(NULL, tszImgFmtDelim);
        if (!tszImgFmt)
        {
            continue;
        }
        tszInvalid = _tcstok(NULL, tszImgFmtDelim);
        if (tszInvalid)
            fInvalid = true;

        sFilename = tszFilename;
        pguidImgFmt = (GUID*) g_mapIMGFMT[tszImgFmt];
        
        // Make sure the image format is supported by the current configuration.
        if (pImagingFactory)
        {
            if (find(lstSupportedDecoders.begin(), lstSupportedDecoders.end(), pguidImgFmt) 
                == lstSupportedDecoders.end())
            {
                continue;
            }
        }
        
        CImageDescriptor * pid = new CImageDescriptor;
        if (!pid)
        {
            // we can't continue.
            break;
        }
        pid->SetImage(g_tsImageSource + sFilename, sReference, pguidImgFmt, PixelFormatDontCare, 0, 0, 0.0, 0.0, 0, 0, fInvalid);
        m_mmImages.insert(pair<const GUID *, CImageDescriptor*>(pid->GetImageFormat(), pid));
    }

    m_ltsLoadedFiles.push_back(sConfigFile);
    m_fInitialized = true;

    fclose(pConfigFile);
    
    return S_OK;
    
}

bool CImageProvider::IsInitialized()
{
    return m_fInitialized;
}

HRESULT CImageProvider::GetFirstImageDesc(CImageDescriptor & id, const GUID * pImgFmt, unsigned int uiStartPoint)
{
    if (!m_fInitialized)
        return E_UNEXPECTED;
   
    if (pImgFmt)
    {
        // lower_bound(pImgFmt) returns an iterator to the first pair in the multimap with
        // a key that matches pImgFmt.
        m_imIter = m_mmImages.lower_bound(pImgFmt);
        if (m_imIter == m_mmImages.upper_bound(pImgFmt))
        {
            info(DETAIL, _FT("Could not find image of type: %s"), g_mapIMGFMT[(LONG)pImgFmt]);
            return E_FAIL;
        }

        for (int i = 0; i < uiStartPoint; i++)
        {
            ++m_imIter;
            if (m_imIter == m_mmImages.upper_bound(pImgFmt))
            {
                info(DETAIL, _FT("Start point was beyond range of images: start pt (%d), total (%d)"), 
                    uiStartPoint, i);
                return E_FAIL;
            }
        }
    }
    else
    {
        m_imIter = m_mmImages.begin();
        if (m_imIter == m_mmImages.end())
        {
            info(DETAIL, _FT("Could not find image of unspecified type"));
            return E_FAIL;
        }
        
        for (int i = 0; i < uiStartPoint; i++)
        {
            ++m_imIter;
            if (m_imIter == m_mmImages.end())
            {
                info(DETAIL, _FT("Start point was beyond range of images: start pt (%d), total (%d)"), 
                    uiStartPoint, i);
                return E_FAIL;
            }
        }
    }

    // *m_imIter is a pair<const GUID *, CImageDescriptor*>.
    // (*m_imIter).first is the const GUID *, and (*m_imIter).second is the
    // CImageDescriptor*.
    id.Clone(*((*m_imIter).second));
    ++m_imIter;
    m_pCurrImgFmt = pImgFmt;
    return S_OK;
}

HRESULT CImageProvider::GetNextImageDesc(CImageDescriptor & id)
{
    if (m_pCurrImgFmt && m_imIter == m_mmImages.upper_bound(m_pCurrImgFmt))
    {
        return E_FAIL;
    }
    else if (!m_pCurrImgFmt && m_imIter == m_mmImages.end())
    {
        return E_FAIL;
    }
    id.Clone(*((*m_imIter).second));
    ++m_imIter;
    return S_OK;
}

HRESULT CImageProvider::GetRandomImageDesc(CImageDescriptor & id, const GUID * pImgFmt)
{
    int iImageCount;
    int iRandomIndex;
    ImageMap::iterator imIter;
    if (pImgFmt)
    {
        iImageCount = m_mmImages.count(pImgFmt);
    }
    else
    {
        iImageCount = m_mmImages.size();
    }
    if (0 == iImageCount)
    {
        return E_FAIL;
    }
    iRandomIndex = RandomInt(0, iImageCount-1);
    
    if (pImgFmt)
    {
        // lower_bound(pImgFmt) returns an iterator to the first pair in the multimap with
        // a key that matches pImgFmt.
        imIter = m_mmImages.lower_bound(pImgFmt);
        while(iRandomIndex--)
            ++imIter;
        id.Clone(*((*imIter).second));
    }
    else
    {
        imIter = m_mmImages.begin();
        while(iRandomIndex--)
            ++imIter;
        id.Clone(*((*imIter).second));
    }
    return S_OK;
}

HRESULT CImageProvider::GetImageCount(unsigned int * puiCount, const GUID * pImgFmt)
{
    if (!IsInitialized())
    {
        return E_UNEXPECTED;
    }

    if (IsBadWritePtr(puiCount, sizeof(unsigned int)))
    {
        return E_POINTER;
    }

    if (pImgFmt)
    {
        *puiCount = m_mmImages.count(pImgFmt);
    }
    else
    {
        *puiCount = m_mmImages.size();
    }
    
    return S_OK;
}

HRESULT CImageProvider::ParseSimpleParams(
    TCHAR * tszSimpleParams, 
    CImageDescriptor & id, 
    IImagingFactory * pImagingFactory)
{
    TCHAR tszDelims[] = TEXT(": \n\r\t");
    TCHAR * tszFile;
    TCHAR * tszImgFmt;
    TCHAR * tszPixFmt;
    TCHAR * tszWidth;
    TCHAR * tszHeight;
    TCHAR * tszXdpi;
    TCHAR * tszYdpi;
    TCHAR * tszReferenceBmp;
    TCHAR * tszThreshold;
    TCHAR * tszAvgThreshold;
    GUID * pguidImgFmt;
    PixelFormat pfPixFmt;
    UINT uiWidth;
    UINT uiHeight;
    double dfXdpi;
    double dfYdpi;
    UINT uiThreshold;
    UINT uiAvgThreshold;
    list<const GUID *> lstSupportedDecoders;
    list<const GUID *> lstSupportedEncoders;

    if (pImagingFactory)
    {
        GetSupportedCodecs(pImagingFactory, lstSupportedDecoders, lstSupportedEncoders);
    }

    tszFile = _tcstok(tszSimpleParams, tszDelims);
    
    // Lines starting with # are comments.
    if (tszFile && '#' == tszFile[0])
        return S_FALSE;
        
    tszImgFmt = _tcstok(NULL, tszDelims);
    tszPixFmt = _tcstok(NULL, tszDelims);
    tszWidth = _tcstok(NULL, tszDelims);
    tszHeight = _tcstok(NULL, tszDelims);
    tszXdpi = _tcstok(NULL, tszDelims);
    tszYdpi = _tcstok(NULL, tszDelims);
    tszReferenceBmp = _tcstok(NULL, tszDelims);
    tszThreshold = _tcstok(NULL, tszDelims);
    tszAvgThreshold = _tcstok(NULL, tszDelims);
    
    // Reference Bitmap is not required.
    if (!tszFile || !tszImgFmt || !tszPixFmt || !tszWidth || !tszHeight || !tszXdpi || !tszYdpi)
        return E_INVALIDARG;
    
    info(DETAIL, TEXT("file: %s, imgfmt: %s, pxlfmt: %s, width: %s, height: %s, xdpi: %s, ydpi: %s"), 
        tszFile, tszImgFmt, tszPixFmt, tszWidth, tszHeight, tszXdpi, tszYdpi);
    
    tstring sFilename (tszFile);
    sFilename = g_tsImageSource + sFilename;
    tstring sReferenceBmp (tszReferenceBmp ? tszReferenceBmp : TEXT(""));
    pguidImgFmt = (GUID*) g_mapIMGFMT[tszImgFmt];
    
    // Make sure the image format is supported by the current configuration.
    if (pImagingFactory)
    {
        if (find(lstSupportedDecoders.begin(), lstSupportedDecoders.end(), pguidImgFmt) 
            == lstSupportedDecoders.end())
        {
            info(DETAIL, TEXT("Not using %s, unsupported format %s"), sFilename.c_str(), tszImgFmt);
            return S_FALSE;
        }
    }

    if (0xffffffff == GetFileAttributes(sFilename.c_str()))
    {
        info(ABORT, _T("Could not access test image %s, error %d"), sFilename.c_str(), GetLastError());
        info(DETAIL, _T("Either the file is in the wrong place or it should be removed from the configuration file"));
        info(DETAIL, _LOCATIONT);
        return S_FALSE;
    }
    
    pfPixFmt = (PixelFormat) g_mapPIXFMT[tszPixFmt];
    uiWidth = _ttoi(tszWidth);
    uiHeight = _ttoi(tszHeight);
    if (!_stscanf(tszXdpi, TEXT("%lf"), &dfXdpi))
    {
        dfXdpi = 0.0;
    }
    if (!_stscanf(tszYdpi, TEXT("%lf"), &dfYdpi))
    {
        dfYdpi = 0.0;
    }
    if (tszThreshold)
        uiThreshold = _ttoi(tszThreshold);
    else
        uiThreshold = 0;
    if (tszAvgThreshold)
        uiAvgThreshold = _ttoi(tszAvgThreshold);
    else
        uiAvgThreshold = 0;
    
    id.SetImage(sFilename, sReferenceBmp, pguidImgFmt, pfPixFmt, uiWidth, uiHeight, dfXdpi, dfYdpi, uiThreshold, uiAvgThreshold);
    return S_OK;
}

