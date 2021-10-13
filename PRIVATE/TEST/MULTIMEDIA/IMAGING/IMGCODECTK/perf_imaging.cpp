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
#include "main.h"
#include "ImageProvider.h"
#include "PerfTests.h"
#include "ImagingHelpers.h"

namespace PerfTests
{

    CImageProvider g_ImgProv;

    TESTPROCAPI RunPerfTestWithImage(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
    {
        NO_MESSAGES;

        PFNPERFTEST pfnPerfTest = (PFNPERFTEST) lpFTE->dwUserData;
        HRESULT hr;
        CImageDescriptor id;

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
        
        hr = g_ImgProv.GetFirstImageDesc(id, NULL);
        
        do 
        {
            if (!id.GetConfigTags().QueryInt(TESTTAG_PERFORMANCE_TEST))
            {
                info(DETAIL, _T("Skipping image %s, not marked with TESTTAG_PERFORMANCE_TEST=1 in configuration file"),
                    id.GetFilename().c_str());
                hr = g_ImgProv.GetNextImageDesc(id);
                continue;
            }
            info(ECHO | RESETINDENT, TEXT("Testing with image %s"), id.GetFilename().c_str());
            if (!ImagingHelpers::IsDesiredCodecType(pImagingFactory, id))
            {
                hr = g_ImgProv.GetNextImageDesc(id);
                continue;
            }

            (*pfnPerfTest)(pImagingFactory, id);
            info(ECHO | UNINDENT, TEXT("Done testing with image %s"), id.GetFilename().c_str());
        } while (!FAILED(g_ImgProv.GetNextImageDesc(id)));

    LPExit:
        VerifyConditionFalse(pImagingFactory->Release());
        return getCode();
        
    }
    TESTPROCAPI RunPerfTestWithEncoder(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
    {
        NO_MESSAGES
        
        HRESULT hr;
        unsigned int uiEncoderCount;
        int iCodecsTested = 0;
        ImageCodecInfo * pici = NULL;
        PFNPERFTESTENCODER pfnPerfTestEncoder = (PFNPERFTESTENCODER) lpFTE->dwUserData;
        UINT BitmapSizes [][2] = {
    //        {500, 500},
    //        {1600, 1200},
            {2048, 1536},
        };

        PixelFormat rgPixelFormats[] = {
            PixelFormat8bppIndexed,
            PixelFormat16bppRGB565,
            PixelFormat24bppRGB,
            PixelFormat32bppARGB
        };

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
            for (int j = 0; j < countof(rgPixelFormats); j++)
            {
                for (int i = 0; i < countof(BitmapSizes); i++)
                {
                    info(ECHO | INDENT, TEXT("Testing with image %dx%d  %s"), 
                        BitmapSizes[i][0], 
                        BitmapSizes[i][1], 
                        g_mapPIXFMT[rgPixelFormats[j]]);
                    CImageDescriptor id;
                    id.SetImage(
                        _T(""), 
                        _T(""), 
                        &IMGFMT_MEMORYBMP, 
                        rgPixelFormats[j], 
                        BitmapSizes[i][0],
                        BitmapSizes[i][1],
                        0.0,
                        0.0,
                        0,
                        0);
                    (*pfnPerfTestEncoder)(pImagingFactory, id, pici + iCntr);
                    info(ECHO | UNINDENT, TEXT("Done testing with image %s"), id.GetFilename().c_str());
                }
            }        
            info(ECHO | UNINDENT, TEXT("Done testing encoder: %s"), pici[iCntr].CodecName);
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
