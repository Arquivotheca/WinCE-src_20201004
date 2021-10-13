//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include "main.h"
#include "ImgDecoders.h"
#include "UnknownTests.h"
#include "ImageDecoderTests.h"
#include "FileStream.h"
#include "ImageProvider.h"
#include "ImagingHelpers.h"

using namespace std;

namespace ImageDecoderTests
{
    CImageProvider g_ImgProv;

    TESTPROCAPI RunTestOnImageDecoders(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
    {
        NO_MESSAGES
        
        HRESULT hr;
        IStream * pStream = NULL;
        CImageDescriptor id;
        PFNIMAGEDECODERTEST pfnImageDecoderTest = (PFNIMAGEDECODERTEST) lpFTE->dwUserData;

        IImagingFactory* pImagingFactory = NULL;
        hr = CoCreateInstance(CLSID_ImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IImagingFactory, (void**) &pImagingFactory);
        if (FAILED(hr))
        {
            info(ABORT, TEXT("Could not create ImagingFactory object, hr=0x%08x"), hr);
            info(DETAIL, _LOCATIONT);
            return getCode();
        }

        if (!g_ImgProv.IsInitialized())
        {
            tstring tsConfigFile = g_tsImageSource + _T("ImagingDecoder.cfg");
            hr = VerifyHRSuccess(g_ImgProv.InitializeComplex(tsConfigFile.c_str(), pImagingFactory));
            if (FAILED(hr))
            {
                info(ABORT, TEXT("Configuration file \"%s\" not found. Cannot continue without images to decode"),
                    tsConfigFile.c_str());
                info(DETAIL, _T("Please verify that the configuration file is in the correct directory: %s"),
                    g_tsImageSource.c_str());
                goto LPExit;
            }
        }

        hr = g_ImgProv.GetFirstImageDesc(id);
        while (SUCCEEDED(hr))
        {
            IImageDecoder * pImageDecoder;

            info(ECHO | RESETINDENT | INDENT, TEXT("Testing with image: %s"), id.GetFilename().c_str());
            if (!ImagingHelpers::IsDesiredCodecType(pImagingFactory, id))
            {
                hr = g_ImgProv.GetNextImageDesc(id);
                continue;
            }

            pStream = id.GetStream();
            if (!pStream)
            {
                info(FAIL, TEXT("Could not get stream from descriptor of \"%s\""),
                    id.GetFilename().c_str());
                info(DETAIL, _LOCATIONT);
                hr = g_ImgProv.GetNextImageDesc(id);
                continue;
            }
            
            PresetPtr(pImageDecoder);
            VerifyHRSuccess(pImagingFactory->CreateImageDecoder(
                pStream, g_fBuiltinFirst ? DecoderInitFlagBuiltIn1st : DecoderInitFlagNone, 
                &pImageDecoder));
            if (!pImageDecoder || PTRPRESET == pImageDecoder)
            {
                SAFERELEASE(pStream);
                hr = g_ImgProv.GetNextImageDesc(id);
                continue;
            }
            
            try
            {
                (*pfnImageDecoderTest)(pImagingFactory, pImageDecoder, id);
            }
            catch (...)
            {
                info(FAIL, _T("Exception occured during test"));
                info(DETAIL, _LOCATIONT);
            }
            info(ECHO | UNINDENT, TEXT("Done testing with image: %s"), id.GetFilename().c_str());
            VerifyConditionFalse(pImageDecoder->Release());
            VerifyConditionFalse(pStream->Release());
            hr = g_ImgProv.GetNextImageDesc(id);
        }

    LPExit:
        VerifyConditionFalse(pImagingFactory->Release());
        return getCode();
    }

    TESTPROCAPI RunTestUnknown(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
    {
        NO_MESSAGES
        
        using namespace UnknownTests;
        IImagingFactory* pImagingFactory;
        HRESULT hr;
        IStream * pStream = NULL;
        CImageDescriptor id;
        PFNUNKNOWNTEST_AR pfnUnknownTest = (PFNUNKNOWNTEST_AR) lpFTE->dwUserData;

        hr = CoCreateInstance(CLSID_ImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IImagingFactory, (void**) &pImagingFactory);
        if (FAILED(hr))
        {
            info(ABORT, TEXT("Could not create ImagingFactory object, hr=0x%08x"), hr);
            info(DETAIL, _LOCATIONT);
            return getCode();
        }
        
        if (!g_ImgProv.IsInitialized())
        {
            tstring tsConfigFile = g_tsImageSource + _T("ImagingDecoder.cfg");
            hr = VerifyHRSuccess(g_ImgProv.InitializeComplex(tsConfigFile.c_str(), pImagingFactory));
            if (FAILED(hr))
            {
                info(ABORT, TEXT("Configuration file \"%s\" not found. Cannot continue without images to decode"),
                    tsConfigFile.c_str());
                info(DETAIL, _T("Please verify that the configuration file is in the correct directory: %s"),
                    g_tsImageSource.c_str());
                goto LPExit;
            }
        }

        hr = g_ImgProv.GetFirstImageDesc(id);
        while (SUCCEEDED(hr))
        {
            IImageDecoder * pImageDecoder;
            IUnknown * pUnknown;

            info(ECHO | RESETINDENT | INDENT, TEXT("Testing with image: %s"), id.GetFilename().c_str());
            if (!ImagingHelpers::IsDesiredCodecType(pImagingFactory, id))
            {
                hr = g_ImgProv.GetNextImageDesc(id);
                continue;
            }

            pStream = id.GetStream();
            if (!pStream)
            {
                info(FAIL, TEXT("Could not get stream from descriptor of \"%s\""),
                    id.GetFilename().c_str());
                info(DETAIL, _LOCATIONT);
                hr = g_ImgProv.GetNextImageDesc(id);
                continue;
            }
            
            PresetPtr(pImageDecoder);
            VerifyHRSuccess(pImagingFactory->CreateImageDecoder(
                pStream, 
                g_fBuiltinFirst ? DecoderInitFlagBuiltIn1st : DecoderInitFlagNone, 
                &pImageDecoder));
            if (!pImageDecoder || PTRPRESET == pImageDecoder)
            {
                SAFERELEASE(pStream);
                hr = g_ImgProv.GetNextImageDesc(id);
                continue;
            }

            PresetPtr(pUnknown);
            VerifyHRSuccess(pImageDecoder->QueryInterface(IID_IUnknown, (void**)&pUnknown));
            if (!pUnknown || PTRPRESET == pUnknown)
            {
                SAFERELEASE(pStream);
                SAFERELEASE(pImageDecoder);
                hr = g_ImgProv.GetNextImageDesc(id);
                continue;
            }
            
            (*pfnUnknownTest)(pUnknown);
            info(ECHO | UNINDENT, TEXT("Done testing with image: %s"), id.GetFilename().c_str());
            VerifyConditionFalse(pUnknown->Release() - 1);
            VerifyConditionFalse(pImageDecoder->Release());
            VerifyConditionFalse(pStream->Release());
            hr = g_ImgProv.GetNextImageDesc(id);
        }
    LPExit:
        VerifyConditionFalse(pImagingFactory->Release());
        return getCode();
    }

    TESTPROCAPI RunTestUnknownGuids(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
    {
        NO_MESSAGES
        
        using namespace UnknownTests;
        IImagingFactory* pImagingFactory;
        HRESULT hr;
        IStream * pStream = NULL;
        CImageDescriptor id;
        PFNUNKNOWNTEST_QI pfnUnknownTest = (PFNUNKNOWNTEST_QI) lpFTE->dwUserData;
        GuidPList gplValid;
        GuidPList gplInvalid;

        gplValid.push_back(&IID_IUnknown);
        gplValid.push_back(&IID_IImageDecoder);

        gplInvalid.push_back(&IID_IImageSink);
        gplInvalid.push_back(&IID_IBasicBitmapOps);
        gplInvalid.push_back(&IID_IBitmapImage);
        gplInvalid.push_back(&IID_IImage);
        gplInvalid.push_back(&IID_IClassFactory);
        gplInvalid.push_back(&IID_IImagingFactory);

        hr = CoCreateInstance(CLSID_ImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IImagingFactory, (void**) &pImagingFactory);
        if (FAILED(hr))
        {
            info(ABORT, TEXT("Could not create ImagingFactory object, hr=0x%08x"), hr);
            info(DETAIL, _LOCATIONT);
            return getCode();
        }
        
        
        if (!g_ImgProv.IsInitialized())
        {
            tstring tsConfigFile = g_tsImageSource + _T("ImagingDecoder.cfg");
            hr = VerifyHRSuccess(g_ImgProv.InitializeComplex(tsConfigFile.c_str(), pImagingFactory));
            if (FAILED(hr))
            {
                info(ABORT, TEXT("Configuration file \"%s\" not found. Cannot continue without images to decode"),
                    tsConfigFile.c_str());
                info(DETAIL, _T("Please verify that the configuration file is in the correct directory: %s"),
                    g_tsImageSource.c_str());
                goto LPExit;
            }
        }

        hr = g_ImgProv.GetFirstImageDesc(id);
        while (SUCCEEDED(hr))
        {
            IImageDecoder * pImageDecoder;
            IUnknown * pUnknown;

            info(ECHO | RESETINDENT | INDENT, TEXT("Testing with image: %s"), id.GetFilename().c_str());
            if (!ImagingHelpers::IsDesiredCodecType(pImagingFactory, id))
            {
                hr = g_ImgProv.GetNextImageDesc(id);
                continue;
            }

            pStream = id.GetStream();
            if (!pStream)
            {
                info(FAIL, TEXT("Could not get stream from descriptor of \"%s\""),
                    id.GetFilename().c_str());
                info(DETAIL, _LOCATIONT);
                hr = g_ImgProv.GetNextImageDesc(id);
                continue;
            }
            
            PresetPtr(pImageDecoder);
            VerifyHRSuccess(pImagingFactory->CreateImageDecoder(
                pStream, 
                g_fBuiltinFirst ? DecoderInitFlagBuiltIn1st : DecoderInitFlagNone, 
                &pImageDecoder));
            if (!pImageDecoder || PTRPRESET == pImageDecoder)
            {
                SAFERELEASE(pStream);
                hr = g_ImgProv.GetNextImageDesc(id);
                continue;
            }
            
            VerifyHRSuccess(pImageDecoder->QueryInterface(IID_IUnknown, (void**)&pUnknown));
            if (!pUnknown || PTRPRESET == pUnknown)
            {
                info(FAIL, TEXT("pUnknown pointer not set by QueryInterface for image %s"),
                    id.GetFilename().c_str());
                info(DETAIL, _LOCATIONT);
                VerifyConditionFalse(pImageDecoder->Release());
                hr = g_ImgProv.GetNextImageDesc(id);
                continue;
            }
            (*pfnUnknownTest)(pUnknown, gplValid, gplInvalid);
            info(ECHO | UNINDENT, TEXT("Done testing with image: %s"), id.GetFilename().c_str());
            VerifyConditionFalse(pUnknown->Release() - 1);
            VerifyConditionFalse(pImageDecoder->Release());
            VerifyConditionFalse(pStream->Release());
            hr = g_ImgProv.GetNextImageDesc(id);
        }
    LPExit:
        VerifyConditionFalse(pImagingFactory->Release());
        return getCode();
    }
}
