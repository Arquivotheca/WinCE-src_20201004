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
#include "UnknownTests.h"
#include "ImageEncoderTests.h"
#include "ImageSinkTests.h"

using namespace std;

namespace ImageEncoderTests
{
    HRESULT ResetEncoderSink(IImageSink* pImageSink)
    {
        HRESULT hr = S_OK;
        IImageEncoder* pImageEncoder;
        IStream* pStream = NULL;

        PresetPtr(pImageEncoder);
        hr = pImageSink->QueryInterface(IID_IImageEncoder, (void**)&pImageEncoder);
        if (FAILED(hr) || !pImageEncoder || PTRPRESET == pImageEncoder)
        {
            info(DETAIL, _FT("Could not retrieve ImageEncoder interface, hr = 0x%08x"),
                hr);
            if (!FAILED(hr))
                hr = E_FAIL;
            goto LPExit;
        }

        hr = pImageEncoder->TerminateEncoder();
        hr = CreateStreamOnHGlobal(NULL, TRUE, &pStream);
        if (FAILED(hr) || !pStream)
        {
            info(DETAIL, _FT("Cannot create stream"));
            if (!FAILED(hr))
                hr = E_FAIL;
            goto LPExit;
        }
        hr = pImageEncoder->InitEncoder(pStream);

    LPExit:
        SAFERELEASE(pImageEncoder);
        SAFERELEASE(pStream);
        return hr;
    }

    TESTPROCAPI RunTestOnImageEncoders(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
    {
        NO_MESSAGES
        
        HRESULT hr;
        IStream * pStream;
        unsigned int uiEncoderCount;
        int iCodecsTested = 0;
        ImageCodecInfo * pici = NULL;
        PFNIMAGEENCODERTEST pfnImageEncoderTest = (PFNIMAGEENCODERTEST) lpFTE->dwUserData;

        IImagingFactory* pImagingFactory = NULL;
        hr = CoCreateInstance(CLSID_ImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IImagingFactory, (void**) &pImagingFactory);
        if (FAILED(hr))
        {
            info(ABORT, TEXT("Could not create ImagingFactory object, hr=0x%08x"), hr);
            info(DETAIL, _LOCATIONT);
            return getCode();
        }

        pImagingFactory->GetInstalledEncoders(&uiEncoderCount, &pici);
        if (!pici)
        {
            info(SKIP, TEXT("No encoders are installed"));
            goto LPExit;
        }

        for (int iCntr = 0; iCntr < uiEncoderCount; iCntr++)
        {
            IImageEncoder * pImageEncoder;
            info(ECHO | RESETINDENT | INDENT, TEXT("Testing encoder: %s"), pici[iCntr].CodecName);
            if (!g_tsCodecType.empty())
            {
                if (_tcscmp(g_tsCodecType.c_str(), pici[iCntr].MimeType))
                {
                    info(ECHO, _T("Skipping encoder of type \"%ls\": does not support specified type \"%s\""),
                        pici[iCntr].MimeType, g_tsCodecType.c_str());
                    continue;
                }
            }
            if (g_fNoBuiltin && (pici[iCntr].Flags & ImageCodecFlagsBuiltin))
            {
                info(ECHO, _T("Builtin encoder skipped because /NoBuiltin parameter was specified."));
                continue;
            }
            if (g_fNoUser && !(pici[iCntr].Flags & ImageCodecFlagsBuiltin))
            {
                info(ECHO, _T("User encoder skipped because /NoUser parameter was specified."));
                continue;
            }
            ++iCodecsTested;
            VerifyHRSuccess(CreateStreamOnHGlobal(NULL, TRUE, &pStream));
            if (!pStream)
            {
                info(ABORT, TEXT("Cannot create stream"));
                goto LPExit;
            }

            PresetPtr(pImageEncoder);
            VerifyHRSuccess(pImagingFactory->CreateImageEncoderToStream(&(pici[iCntr].Clsid), pStream, &pImageEncoder));
            VerifyConditionFalse(pStream->Release() - 1);
            if (!pImageEncoder || PTRPRESET == pImageEncoder)
            {
                info(FAIL, TEXT("ImageEncoder pointer not set by CreateImageEncoderToStream for encoder %s"),
                    pici[iCntr].CodecName);
                info(DETAIL, _LOCATIONT);
                continue;
            }

            try
            {
                (*pfnImageEncoderTest)(pImagingFactory, pImageEncoder, pici + iCntr);
            }
            catch (...)
            {
                info(FAIL, _T("Exception occurred during test."));
                info(DETAIL, _LOCATIONT);
            }
            info(ECHO | UNINDENT, TEXT("Done testing encoder: %s"), pici[iCntr].CodecName);
            VerifyConditionFalse(pImageEncoder->Release());
        }

        if (0 == iCodecsTested)
        {
            info(SKIP, _T("No encoders matched the specified parameters"));
        }

    LPExit:
        if (pici)
            CoTaskMemFree(pici);
        VerifyConditionFalse(pImagingFactory->Release());
        return getCode();
    }

    TESTPROCAPI RunTestOnImageSinks(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
    {
        NO_MESSAGES
        
        using namespace ImageSinkTests;
        HRESULT hr;
        IStream * pStream;
        unsigned int uiEncoderCount;
        int iCodecsTested = 0;
        ImageCodecInfo * pici = NULL;
        SinkParam sParam;
        PFNIMAGESINKTEST pfnImageSinkTest = (PFNIMAGESINKTEST) lpFTE->dwUserData;

        IImagingFactory* pImagingFactory = NULL;
        hr = CoCreateInstance(CLSID_ImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IImagingFactory, (void**) &pImagingFactory);
        if (FAILED(hr))
        {
            info(ABORT, TEXT("Could not create ImagingFactory object, hr=0x%08x"), hr);
            info(DETAIL, _LOCATIONT);
            return getCode();
        }

        pImagingFactory->GetInstalledEncoders(&uiEncoderCount, &pici);
        if (!pici)
        {
            info(SKIP, TEXT("No encoders are installed"));
            goto LPExit;
        }

        for (int iCntr = 0; iCntr < uiEncoderCount; iCntr++)
        {
            IImageEncoder * pImageEncoder;
            IImageSink * pImageSink;
            info(ECHO | RESETINDENT | INDENT, TEXT("Testing encoder: %s"), pici[iCntr].CodecName);
            if (!g_tsCodecType.empty())
            {
                if (_tcscmp(g_tsCodecType.c_str(), pici[iCntr].MimeType))
                {
                    info(ECHO, _T("Skipping encoder of type \"%ls\": does not support specified type \"%s\""),
                        pici[iCntr].MimeType, g_tsCodecType.c_str());
                    continue;
                }
            }
            if (g_fNoBuiltin && (pici[iCntr].Flags & ImageCodecFlagsBuiltin))
            {
                info(ECHO, _T("Builtin encoder skipped because /NoBuiltin parameter was specified."));
                continue;
            }
            if (g_fNoUser && !(pici[iCntr].Flags & ImageCodecFlagsBuiltin))
            {
                info(ECHO, _T("User encoder skipped because /NoUser parameter was specified."));
                continue;
            }
            ++iCodecsTested;
            VerifyHRSuccess(CreateStreamOnHGlobal(NULL, TRUE, &pStream));
            if (!pStream)
            {
                info(ABORT, TEXT("Cannot create stream"));
                goto LPExit;
            }

            PresetPtr(pImageEncoder);
            VerifyHRSuccess(pImagingFactory->CreateImageEncoderToStream(&(pici[iCntr].Clsid), pStream, &pImageEncoder));
            VerifyConditionFalse(pStream->Release() - 1);
            if (!pImageEncoder || PTRPRESET == pImageEncoder)
            {
                info(FAIL, TEXT("ImageEncoder pointer not set by CreateImageEncoderToStream for encoder %s"),
                    pici[iCntr].CodecName);
                info(DETAIL, _LOCATIONT);
                continue;
            }

            PresetPtr(pImageSink);
            VerifyHRSuccess(pImageEncoder->GetEncodeSink(&pImageSink));
            if (!pImageSink || PTRPRESET == pImageSink)
            {
                info(FAIL, TEXT("pImageSink pointer not set by GetSink for encoder %s"),
                    pici[iCntr].CodecName);
                info(DETAIL, _LOCATIONT);
                VerifyConditionFalse(pImageEncoder->Release());
                continue;
            }

            info(ECHO | RESETINDENT | INDENT, TEXT("Testing encoder: %s"), pici[iCntr].CodecName);

            sParam.uiFlags = spfImageCodecInfo;
            sParam.pici = pici + iCntr;
            try
            {
                (*pfnImageSinkTest)(pImageSink, &sParam, (PFNRESETIMAGESINK)ResetEncoderSink);
            }
            catch (...)
            {
                info(FAIL, _T("Exception occurred during test."));
                info(DETAIL, _LOCATIONT);
            }
            info(ECHO | UNINDENT, TEXT("Done testing encoder: %s"), pici[iCntr].CodecName);
            VerifyConditionFalse(pImageSink->Release() - 1);
            VerifyConditionFalse(pImageEncoder->Release());
        }

        if (0 == iCodecsTested)
        {
            info(SKIP, _T("No encoders matched the specified parameters"));
        }
    
    LPExit:
        if (pici)
            CoTaskMemFree(pici);
        VerifyConditionFalse(pImagingFactory->Release());
        return getCode();
    }

    TESTPROCAPI RunTestUnknown(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
    {
        NO_MESSAGES
        
        using namespace UnknownTests;
        IImagingFactory* pImagingFactory;
        HRESULT hr;
        IStream * pStream;
        ImageCodecInfo * pici = NULL;
        unsigned int uiEncoderCount;
        int iCodecsTested = 0;
        PFNUNKNOWNTEST_AR pfnUnknownTest = (PFNUNKNOWNTEST_AR) lpFTE->dwUserData;

        hr = CoCreateInstance(CLSID_ImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IImagingFactory, (void**) &pImagingFactory);
        if (FAILED(hr))
        {
            info(ABORT, TEXT("Could not create ImagingFactory object, hr=0x%08x"), hr);
            info(DETAIL, _LOCATIONT);
            return getCode();
        }
        
        pImagingFactory->GetInstalledEncoders(&uiEncoderCount, &pici);
        if (!pici)
        {
            info(SKIP, TEXT("No encoders are installed"));
            goto LPExit;
        }

        for (int iCntr = 0; iCntr < uiEncoderCount; iCntr++)
        {
            IImageEncoder * pImageEncoder;
            IUnknown * pUnknown;
            info(ECHO | RESETINDENT | INDENT, TEXT("Testing encoder: %s"), pici[iCntr].CodecName);
            if (!g_tsCodecType.empty())
            {
                if (_tcscmp(g_tsCodecType.c_str(), pici[iCntr].MimeType))
                {
                    info(ECHO, _T("Skipping encoder of type \"%ls\": does not support specified type \"%s\""),
                        pici[iCntr].MimeType, g_tsCodecType.c_str());
                    continue;
                }
            }
            if (g_fNoBuiltin && (pici[iCntr].Flags & ImageCodecFlagsBuiltin))
            {
                info(ECHO, _T("Builtin encoder skipped because /NoBuiltin parameter was specified."));
                continue;
            }
            if (g_fNoUser && !(pici[iCntr].Flags & ImageCodecFlagsBuiltin))
            {
                info(ECHO, _T("User encoder skipped because /NoUser parameter was specified."));
                continue;
            }
            ++iCodecsTested;
            VerifyHRSuccess(CreateStreamOnHGlobal(NULL, TRUE, &pStream));
            if (!pStream)
            {
                info(ABORT, TEXT("Cannot create stream"));
                break;
            }

            PresetPtr(pImageEncoder);
            VerifyHRSuccess(pImagingFactory->CreateImageEncoderToStream(&(pici[iCntr].Clsid), pStream, &pImageEncoder));
            VerifyConditionFalse(pStream->Release() - 1);
            if (!pImageEncoder || PTRPRESET == pImageEncoder)
            {
                info(FAIL, TEXT("ImageEncoder pointer not set by CreateImageEncoderToStream for encoder %s"),
                    pici[iCntr].CodecName);
                info(DETAIL, _LOCATIONT);
                continue;
            }

            PresetPtr(pUnknown);
            VerifyHRSuccess(pImageEncoder->QueryInterface(IID_IUnknown, (void**)&pUnknown));
            if (!pUnknown || PTRPRESET == pUnknown)
            {
                info(FAIL, TEXT("pUnknown pointer not set by QueryInterface for encoder %s"),
                    pici[iCntr].CodecName);
                info(DETAIL, _LOCATIONT);
                VerifyConditionFalse(pImageEncoder->Release());
                continue;
            }
            
            info(ECHO, TEXT("Testing encoder: %s"), pici[iCntr].CodecName);
            (*pfnUnknownTest)(pUnknown);
            VerifyConditionFalse(pImageEncoder->Release() - 1);
            VerifyConditionFalse(pUnknown->Release());
        }

        if (0 == iCodecsTested)
        {
            info(SKIP, _T("No encoders matched the specified parameters"));
        }
    
    LPExit:
        if (pici)
            CoTaskMemFree(pici);
        VerifyConditionFalse(pImagingFactory->Release());
        return getCode();
    }

    TESTPROCAPI RunTestUnknownGuids(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
    {
        NO_MESSAGES
        
        using namespace UnknownTests;
        IImagingFactory* pImagingFactory;
        HRESULT hr;
        IStream * pStream;
        ImageCodecInfo * pici = NULL;
        unsigned int uiEncoderCount;
        int iCodecsTested = 0;
        PFNUNKNOWNTEST_QI pfnUnknownTest = (PFNUNKNOWNTEST_QI) lpFTE->dwUserData;
        GuidPList gplValid;
        GuidPList gplInvalid;

        gplValid.push_back(&IID_IUnknown);
        gplValid.push_back(&IID_IImageEncoder);

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
        
        
        pImagingFactory->GetInstalledEncoders(&uiEncoderCount, &pici);
        if (!pici)
        {
            info(SKIP, TEXT("No encoders are installed"));
            goto LPExit;
        }

        for (int iCntr = 0; iCntr < uiEncoderCount; iCntr++)
        {
            IImageEncoder * pImageEncoder;
            IUnknown * pUnknown;
            info(ECHO | RESETINDENT | INDENT, TEXT("Testing encoder: %s"), pici[iCntr].CodecName);
            if (!g_tsCodecType.empty())
            {
                if (_tcscmp(g_tsCodecType.c_str(), pici[iCntr].MimeType))
                {
                    info(ECHO, _T("Skipping encoder of type \"%ls\": does not support specified type \"%s\""),
                        pici[iCntr].MimeType, g_tsCodecType.c_str());
                    continue;
                }
            }
            if (g_fNoBuiltin && (pici[iCntr].Flags & ImageCodecFlagsBuiltin))
            {
                info(ECHO, _T("Builtin encoder skipped because /NoBuiltin parameter was specified."));
                continue;
            }
            if (g_fNoUser && !(pici[iCntr].Flags & ImageCodecFlagsBuiltin))
            {
                info(ECHO, _T("User encoder skipped because /NoUser parameter was specified."));
                continue;
            }
            ++iCodecsTested;
            VerifyHRSuccess(CreateStreamOnHGlobal(NULL, TRUE, &pStream));
            if (!pStream)
            {
                info(ABORT, TEXT("Cannot create stream"));
                break;
            }

            PresetPtr(pImageEncoder);
            VerifyHRSuccess(pImagingFactory->CreateImageEncoderToStream(&(pici[iCntr].Clsid), pStream, &pImageEncoder));
            VerifyConditionFalse(pStream->Release() - 1);
            if (!pImageEncoder || PTRPRESET == pImageEncoder)
            {
                info(FAIL, TEXT("ImageEncoder pointer not set by CreateImageEncoderToStream for encoder %s"),
                    pici[iCntr].CodecName);
                info(DETAIL, _LOCATIONT);
                continue;
            }

            PresetPtr(pUnknown);
            VerifyHRSuccess(pImageEncoder->QueryInterface(IID_IUnknown, (void**)&pUnknown));
            if (!pUnknown || PTRPRESET == pUnknown)
            {
                info(FAIL, TEXT("pUnknown pointer not set by QueryInterface for encoder %s"),
                    pici[iCntr].CodecName);
                info(DETAIL, _LOCATIONT);
                VerifyConditionFalse(pImageEncoder->Release());
                continue;
            }

            info(ECHO, TEXT("Testing encoder: %s"), pici[iCntr].CodecName);
            (*pfnUnknownTest)(pUnknown, gplValid, gplInvalid);
            VerifyConditionFalse(pUnknown->Release() - 1);
            VerifyConditionFalse(pImageEncoder->Release());
        }

        if (0 == iCodecsTested)
        {
            info(SKIP, _T("No encoders matched the specified parameters"));
        }
    
    LPExit:
        if (pici)
            CoTaskMemFree(pici);
        VerifyConditionFalse(pImagingFactory->Release());
        return getCode();
    }
}
