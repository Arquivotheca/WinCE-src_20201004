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
#pragma once

#include <windows.h>
#include <map>
#include <string>
#include <list>
#include <imaging.h>
#include <QATestUty\ConfigUty.h>
#include "main.h"

#define TESTTAG_SIMPLE_PARAMS       0x10000
#define TESTTAG_NEXT_FILE           (TESTTAG_SIMPLE_PARAMS + 1)
#define TESTTAG_THUMBNAIL_PRESENT   (TESTTAG_SIMPLE_PARAMS + 2)
#define TESTTAG_FRAMEDIM            (TESTTAG_SIMPLE_PARAMS + 3)
#define TESTTAG_FRAMECOUNT          (TESTTAG_SIMPLE_PARAMS + 4)
#define TESTTAG_PERFORMANCE_TEST    (TESTTAG_SIMPLE_PARAMS + 5)

extern ConfigUty::ConfigMap g_cmTags[];

class CImageDescriptor
{
public:
    HRESULT SetImage(
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
        bool fInvalid = false);

    HRESULT SetConfigTags(
        const ConfigUty::CConfig & cfgTags);
    
    // Returns an addref'ed stream. The callee needs to release.
    IStream * GetStream(
        const TCHAR * tszReplayFile = NULL, 
        int iSeed = 0) const;

    // Returns a stream to the file loaded into local memory.
    IStream * GetMemoryStream() const;

    // Returns an addref'ed IImage pointer randomly created from either
    // CreateImageFromStream, CreateImageFromFile, or CreateImageFromBuffer
    HRESULT GetImageInterface(
        IImagingFactory * pImagingFactory, 
        IImage ** ppImage,
        const TCHAR * tszReplayFile = NULL, 
        int iSeed = 0) const;

    HRESULT GetBitmapImageInterface(
        IImagingFactory * pImagingFactory,
        IBitmapImage ** ppBitmapImage) const;
    
    // Returns the file name
    const tstring & GetFilename() const;

    const tstring & GetReferenceBmp() const;

    // Returns the mime type, not currently used.
    const tstring & GetMimeType() const;
    const GUID * GetImageFormat() const;
    PixelFormat GetPixelFormat() const;
    UINT GetWidth() const {return m_uiWidth;}
    UINT GetHeight() const {return m_uiHeight;}
    double GetXdpi() const {return m_dfXdpi;}
    double GetYdpi() const {return m_dfYdpi;}
    UINT GetThreshold() const {return m_uiThreshold;}
    UINT GetAvgThreshold() const {return m_uiAvgThreshold;}
    bool GetInvalid() const {return m_fInvalid;}

    const ConfigUty::CConfig & GetConfigTags() const {return m_cfgTags;}

    CImageDescriptor();
    ~CImageDescriptor();

    CImageDescriptor(const CImageDescriptor & idOther);
    CImageDescriptor & operator=(const CImageDescriptor & idOther);
    HRESULT Clone(const CImageDescriptor & idOther);
private:
    tstring m_sFilename;
    tstring m_sMimeType;
    tstring m_sReferenceBmp;
    const GUID* m_pguidImageFormat;    
    // Need to add more info, such as width, heith, and pixel format.
    PixelFormat m_pfPixelFormat;
    UINT m_uiWidth;
    UINT m_uiHeight;
    double m_dfXdpi;
    double m_dfYdpi;
    UINT m_uiThreshold;
    UINT m_uiAvgThreshold;

    // If invalid is set, this image is known to be bad. This is used by the
    // EncoderFuncT with images that decode on the desktop but not on the device.
    bool m_fInvalid;
    ConfigUty::CConfig m_cfgTags;
};

class CImageProvider
{
public:
    CImageProvider() : m_pCurrImgFmt(NULL), m_fInitialized(false) {}
    ~CImageProvider();

    // Initialize can be called more than once. Each time it is called, the 
    // images in the config file will be added to the multimap, even duplicates.
    HRESULT Initialize(tstring sConfigFile, IImagingFactory *pImagingFactory = NULL);

    // InitializeForBitmaps will load ImageDescriptors that describe memory bitmaps.
    // Interfaces to the bitmaps can be obtained with CImageDescriptor->GetBitmapImageInterface.
    HRESULT InitializeForBitmaps();

    // InitializeComplex will load the image information from a ConfigUty file.
    // The extra information will be used by the Decoder tests, which will use
    // the information to determine which tags are contained in an image and
    // what value the tags should have.
    HRESULT InitializeComplex(tstring sConfigFile, IImagingFactory *pImagingFactory = NULL);

    HRESULT InitializeSimple(tstring sConfigFile, IImagingFactory *pImagingFactory = NULL);
    
    bool IsInitialized();

    // If pImgFmt is NULL, these functions will grab all images of any type.
    // If GetFirstImageDesc is called with an imgfmt, however, only images
    // of that format will be returned by the call and any subsequent calls to
    // GetNextImageDesc, until GetFirstImageDesc is called again.
    HRESULT GetFirstImageDesc(CImageDescriptor & id, const GUID * pImgFmt = NULL, unsigned int uiStart = 0);
    HRESULT GetNextImageDesc(CImageDescriptor & id);

    // Choose a random image from the image map. This is useful for testing large groups
    // of images, where only a small portion might be tested at a time.
    HRESULT GetRandomImageDesc(CImageDescriptor & id, const GUID * pImgFmt = NULL);

    // How many image descriptions are in the provider?
    HRESULT GetImageCount(unsigned int * puiCount, const GUID * pImgFmt = NULL);
private:
    // Helper function. This will parse a line from one of the simple config files
    // (such as ImagingDepth.cfg), filling in the appropriate parameters in the
    // CImageDescriptor object. This is used by Initialize and InitializeComplex
    HRESULT ParseSimpleParams(TCHAR * tszSimpleParams, CImageDescriptor & id, IImagingFactory * pImagingFactory);
    
    // A multimap stores values associated to keys. There can be more than one
    // value associated with a certain key (a plain map doesn't allow this).
    // Currently I use the imgfmt guid as the key. This allows me to grab
    // all images of a certain format, as well as all the images of any format.
    typedef std::multimap<const GUID *, CImageDescriptor *> ImageMap;
    ImageMap m_mmImages;
    ImageMap::iterator m_imIter;

    std::list<tstring> m_ltsLoadedFiles;
    const GUID * m_pCurrImgFmt;
    bool m_fInitialized;
};
