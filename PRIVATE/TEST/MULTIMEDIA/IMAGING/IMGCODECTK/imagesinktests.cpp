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
#include "ImageSinkTests.h"
#include <windows.h>
#include <stdlib.h>
#include "main.h"
#include "ImagingHelpers.h"
#include <string>
#include <list>
#include <exception>

using namespace ImagingHelpers;

namespace ImageSinkTests
{
    TestBeginEndSink(IImageSink* pImageSink, SinkParam* psp, PFNRESETIMAGESINK pfnResetImageSink)
    {
        ImageInfo ii;
        RECT rc;
        BitmapData bmd;

        UINT uiWidth, uiHeight;
        if (psp->uiFlags & spfSize)
        {
            uiWidth = psp->pSize->cx;
            uiHeight = psp->pSize->cy;
        }
        else
        {
            uiWidth = 10;
            uiHeight = 10;
        }

        if (g_fBadParams)
        {
            info(ECHO, TEXT("Testing using invalid parameters"));
            RECT rcInvalid;
            
            ii.RawDataFormat = IMGFMT_MEMORYBMP;
            ii.PixelFormat = PixelFormat32bppARGB;
            ii.Width = uiWidth;
            ii.Height = uiHeight;
            ii.TileWidth = ii.Width;
            ii.TileHeight = ii.Height;
            ii.Xdpi = 96;
            ii.Ydpi = 96;
            ii.Flags = SinkFlagsFullWidth;

            __try {
                VerifyHRFailure(pImageSink->BeginSink(NULL, NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("BeginSink excepted with NULL, NULL"));
                info(DETAIL, _LOCATIONT);
            }
            __try {
                VerifyHRFailure(pImageSink->BeginSink(NULL, &rcInvalid));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("BeginSink excepted with NULL, &rcInvalid"));
                info(DETAIL, _LOCATIONT);
            }
             __try {
                VerifyHRFailure(pImageSink->BeginSink(&ii, (RECT*)0xBAD));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("BeginSink excepted with &ii, 0xBAD"));
                info(DETAIL, _LOCATIONT);
            }
        }

        info(ECHO | INDENT, TEXT("Testing using valid parameters"));

        // Try each of the image formats, and make sure BeginSink either returns
        // that same format or the MemoryBitmap format.
        info(ECHO | INDENT, _T("Testing BeginSink with all known RawDataFormats"));
        for (int iFormat = 0; iFormat < g_cImgFmt; iFormat++)
        {
            ii.RawDataFormat = *(g_rgImgFmt[iFormat]);
            ii.PixelFormat = PixelFormat32bppARGB;
            ii.Width = uiWidth;
            ii.Height = uiHeight;
            ii.TileWidth = ii.Width;
            ii.TileHeight = ii.Height;
            ii.Xdpi = 96;
            ii.Ydpi = 96;
            ii.Flags = SinkFlagsFullWidth;

            info(ECHO, TEXT("Calling BeginSink with %s"),
                g_mapIMGFMT[ii.RawDataFormat]);
            VerifyHRSuccess(pImageSink->BeginSink(&ii, NULL));

            if (memcmp(&(ii.RawDataFormat), g_rgImgFmt[iFormat], sizeof(GUID)) &&
                memcmp(&(ii.RawDataFormat), &IMGFMT_MEMORYBMP, sizeof(GUID)))
            {
                info(FAIL, TEXT("Codec \"%s\" returned raw image format %s when passed %s, should set to IMGFMT_MEMORYBMP"),
                    psp->uiFlags & spfImageCodecInfo ? psp->pici->CodecName : TEXT("memory bitmap"), 
                    g_mapIMGFMT[ii.RawDataFormat], 
                    g_mapIMGFMT[*g_rgImgFmt[iFormat]]);
                info(DETAIL, _LOCATIONT);
            }

            // Some encoders must have data pushed into them. Since we can't be
            // sure if we're going to have the right data (i.e. the encoder
            // could be expecting raw data), ignore the return code of EndSink.
            pImageSink->EndSink(S_OK);

            if (pfnResetImageSink)
            {
                VerifyHRSuccess((*pfnResetImageSink)(pImageSink));
            }
        }
        info(ECHO | UNINDENT, _T("Done testing with all known RawDataFormats."));
        // Should now be in a state where we can begin sink again.

        // All encoders must support IMGFMT_MEMORYBMP at 32bpp argb.
        ii.RawDataFormat = IMGFMT_MEMORYBMP;
        ii.PixelFormat = PixelFormat32bppARGB;
        ii.Width = uiWidth;
        ii.Height = uiHeight;
        ii.TileWidth = ii.Width;
        ii.TileHeight = ii.Height;
        ii.Xdpi = 96;
        ii.Ydpi = 96;
        ii.Flags = SinkFlagsFullWidth;
        // Make sure a second call to EndSink does not result in a crash.
        // Return value is unspecified.
        info(ECHO, _T("Calling EndSink before calling BeginSink (should not crash"));
        pImageSink->EndSink(S_OK);

        // Now make sure BeginSink succeeds
        info(ECHO, _T("Calling BeginSink with valid ImageInfo data (should succeed)"));
        VerifyHRSuccess(pImageSink->BeginSink(&ii, NULL));

        // Now make sure a second call to BeginSink doesn't crash. Some
        // encoders do not support this being called more than once.
        info(ECHO, _T("Calling BeginSink a second time (should not crash)"));
        pImageSink->BeginSink(&ii, NULL);

        // Some encoders need pixel data pushed into them.
        SetRect(&rc, 0, 0, ii.Width, ii.Height);
        info(ECHO, _T("Setting pixel data to encode"));
        VerifyHRSuccess(pImageSink->GetPixelDataBuffer(&rc, PixelFormat32bppARGB, TRUE, &bmd));
        VerifyHRSuccess(pImageSink->ReleasePixelDataBuffer(&bmd));

        // Now make sure we can end the sink with no problems
        info(ECHO, _T("Calling EndSink (should succeed)"));
        VerifyHRSuccess(pImageSink->EndSink(S_OK));

        // Make sure a second call to EndSink does not result in a crash.
        // Return value is unspecified.
        info(ECHO, _T("Calling EndSink again (should not crash)"));
        pImageSink->EndSink(S_OK);
        info(UNINDENT, NULL);
        return getCode();
    }

    INT TestSetPalette(IImageSink* pImageSink, SinkParam* psp, PFNRESETIMAGESINK pfnResetImageSink)
    {
        // Start the sink up.
        ImageInfo ii;
        BitmapData bmd;
        struct {
            UINT Flags;
            UINT Count;
            ARGB Entries[256];
        } cp256;
        
        UINT uiWidth, uiHeight;
        if (psp->uiFlags & spfSize)
        {
            uiWidth = psp->pSize->cx;
            uiHeight = psp->pSize->cy;
        }
        else
        {
            uiWidth = 10;
            uiHeight = 10;
        }
        RECT rc = {0, 0, uiWidth, uiHeight};
        
        cp256.Flags = PaletteFlagsHalftone;
        cp256.Count = 256;
        for (int iCntr = 0; iCntr < cp256.Count; iCntr++)
        {
            // make sure there is no alpha.
            cp256.Entries[iCntr] = 0xFF000000 | RandomInt(0, 0xFFFFFF);
        }

        ii.RawDataFormat = IMGFMT_MEMORYBMP;
        ii.PixelFormat = PixelFormat8bppIndexed;
        ii.Width = uiWidth;
        ii.Height = uiHeight;
        ii.TileWidth = ii.Width;
        ii.TileHeight = ii.Height;
        ii.Xdpi = 96;
        ii.Ydpi = 96;
        ii.Flags = SinkFlagsFullWidth;

        info(ECHO, _T("Calling BeginSink with 8bpp to verify palette availability"));
        VerifyHRSuccess(pImageSink->BeginSink(&ii, NULL));
        
        if (g_fBadParams)
        {
            info(ECHO | INDENT, TEXT("Testing with invalid parameters"));
            __try {
                VerifyHRFailure(pImageSink->SetPalette(NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("SetPalette excepted with NULL parameter"));
                info(DETAIL, _LOCATIONT);
            }
            info(UNINDENT, NULL);
        }

        pImageSink->EndSink(S_OK);
        
        if (ii.PixelFormat != PixelFormat8bppIndexed)
        {
            info(ECHO, TEXT("Codec \"%s\" doesn't support palettes"),
                psp->uiFlags & spfImageCodecInfo ? psp->pici->CodecName : TEXT("memory bitmap"));
            return getCode();
        }

        info(ECHO | INDENT, TEXT("Testing with valid parameters"));

        if (pfnResetImageSink)
        {
            VerifyHRSuccess(pfnResetImageSink(pImageSink));
        }

        info(ECHO, _T("Calling BeginSink with 8bpp (should succeed)"));
        VerifyHRSuccess(pImageSink->BeginSink(&ii, NULL));
        info(ECHO, _T("Calling SetPalette (should succeed)"));
        VerifyHRSuccess(pImageSink->SetPalette((ColorPalette*)&cp256));

        // now make sure SetPalette doesn't crash after setting image data
        info(ECHO, _T("Setting pixel data"));
        VerifyHRSuccess(pImageSink->GetPixelDataBuffer(&rc, ii.PixelFormat, TRUE, &bmd));
        VerifyHRSuccess(pImageSink->ReleasePixelDataBuffer(&bmd));

        // Some encoders allow this, some do not. Make sure it doesn't cause
        // an exception.
        info(ECHO, _T("Calling SetPalette after pixel data has been set (should not crash)"));
        pImageSink->SetPalette((ColorPalette*)&cp256);

        // All done
        info(ECHO, _T("Calling EndSink (should succeed)"));
        VerifyHRSuccess(pImageSink->EndSink(S_OK));
        info(UNINDENT, NULL);
        return getCode();
    }

    INT TestPushPixelData(IImageSink* pImageSink, SinkParam* psp, PFNRESETIMAGESINK pfnResetImageSink)
    {
        ARGB rgBitmap[10 * 10];
        ImageInfo ii;
        BitmapData bmd;
        UINT uiWidth, uiHeight;
        if (psp->uiFlags & spfSize)
        {
            uiWidth = psp->pSize->cx;
            uiHeight = psp->pSize->cy;
        }
        else
        {
            uiWidth = 10;
            uiHeight = 10;
        }
        RECT rc = {0, 0, uiWidth, uiHeight};

        bmd.Width = uiWidth;
        bmd.Height = uiHeight;
        bmd.Stride = 40;
        bmd.PixelFormat = PixelFormat32bppARGB;
        bmd.Scan0 = (void*)rgBitmap;
        if (g_fBadParams)
        {
            info(ECHO | INDENT, TEXT("Testing with invalid parameters"));
            VerifyHRSuccess(pImageSink->BeginSink(&ii, NULL));
            __try {
                VerifyHRFailure(pImageSink->PushPixelData(NULL, NULL, TRUE));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("PushPixelData excepted with NULL, NULL, TRUE"));
                info(DETAIL, _LOCATIONT);
            }
            __try {
                VerifyHRFailure(pImageSink->PushPixelData(NULL, &bmd, TRUE));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("PushPixelData excepted with NULL, &bmd, TRUE"));
                info(DETAIL, _LOCATIONT);
            }
            __try {
                VerifyHRFailure(pImageSink->PushPixelData(&rc, NULL, TRUE));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("PushPixelData excepted with &rc, NULL, TRUE"));
                info(DETAIL, _LOCATIONT);
            }
            VerifyHRSuccess(pImageSink->EndSink(S_OK));
            if (pfnResetImageSink)
            {
                VerifyHRSuccess(pfnResetImageSink(pImageSink));
            }
            info(UNINDENT, NULL);
        }
        
        info(ECHO | INDENT, TEXT("Testing with valid parameters"));

        // PushPixelData should fail if we haven't begun yet.
        info(ECHO, TEXT("Calling PushPixelData before BeginSink (should fail)"));
        VerifyHRFailure(pImageSink->PushPixelData(&rc, &bmd, TRUE));

        // Now begin sinking
        
        ii.RawDataFormat = IMGFMT_MEMORYBMP;
        ii.PixelFormat = PixelFormat32bppARGB;
        ii.Width = uiWidth;
        ii.Height = uiHeight;
        ii.TileWidth = ii.Width;
        ii.TileHeight = ii.Height;
        ii.Xdpi = 96;
        ii.Ydpi = 96;
        ii.Flags = SinkFlagsFullWidth;
        info(ECHO, _T("Calling BeginSink with valid data (should succeed)"));
        VerifyHRSuccess(pImageSink->BeginSink(&ii, NULL));

        // Now the push should work
        info(ECHO, TEXT("Calling PushPixelData while sinking (should succeed)"));
        VerifyHRSuccess(pImageSink->PushPixelData(&rc, &bmd, TRUE));

        // Make sure a second call to Push doesn't crash the encoder.
        // Some encoders can handle this, some cannot.
        info(ECHO, TEXT("Calling PushPixelData a second time (should not crash)"));
        pImageSink->PushPixelData(&rc, &bmd, TRUE);

        info(ECHO, _T("Calling EndSink (should succeed)"));
        VerifyHRSuccess(pImageSink->EndSink(S_OK));

        // The push shouldn't work now that we've ended. Some encoders,
        // however, allow this to happen. Verify that no exceptions occur.
        info(ECHO, TEXT("Calling PushPixelData after EndSink (should not crash)"));
        pImageSink->PushPixelData(&rc, &bmd, TRUE);

        if (pfnResetImageSink)
        {
            VerifyHRSuccess(pfnResetImageSink(pImageSink));
        }
        
        ii.Flags &= SinkFlagsMultipass;
        info(ECHO, _T("Calling BeginSink with SinkFlagsMultipass"));
        VerifyHRSuccess(pImageSink->BeginSink(&ii, NULL));
        if (ii.Flags & SinkFlagsMultipass)
        {
            info(ECHO, TEXT("BeginSink accepted MULTIPASS flag, calling PushPixelData with lastPass == FALSE (should succeed)"));
            VerifyHRSuccess(pImageSink->PushPixelData(&rc, &bmd, FALSE));
            info(ECHO, _T("Calling PushPixelData with lastPass == TRUE (should succeed)"));
            VerifyHRSuccess(pImageSink->PushPixelData(&rc, &bmd, TRUE));
        }
        else
        {
            info(ECHO, TEXT("BeginSink did not accept MULTIPASS flag, PushPixelData with lastPass == FALSE should fail"));
            VerifyHRFailure(pImageSink->PushPixelData(&rc, &bmd, FALSE));
            info(ECHO, _T("PushPixelData a second time (with lastPass == TRUE) should succeed"));
            VerifyHRSuccess(pImageSink->PushPixelData(&rc, &bmd, TRUE));
        }

        info(ECHO, _T("Calling EndSink (should succeed)"));
        VerifyHRSuccess(pImageSink->EndSink(S_OK));
        info(UNINDENT, NULL);
        return getCode();
    }
    
    INT TestGetReleasePixelDataBuffer(IImageSink* pImageSink, SinkParam* psp, PFNRESETIMAGESINK pfnResetImageSink)
    {
        BitmapData bmd;
        ImageInfo ii;
        struct {
            UINT Flags;
            UINT Count;
            ARGB Entries[256];
        } cp256;
        UINT uiWidth, uiHeight;
        if (psp->uiFlags & spfSize)
        {
            uiWidth = psp->pSize->cx;
            uiHeight = psp->pSize->cy;
        }
        else
        {
            uiWidth = 10;
            uiHeight = 10;
        }
        RECT rc = {0, 0, uiWidth, uiHeight};

        if (g_fBadParams)
        {
            info(ECHO | INDENT, TEXT("Testing with invalid parameters"));
            // get ready
            ii.RawDataFormat = IMGFMT_MEMORYBMP;
            ii.PixelFormat = PixelFormat32bppARGB;
            ii.Width = uiWidth;
            ii.Height = uiHeight;
            ii.TileWidth = ii.Width;
            ii.TileHeight = ii.Height;
            ii.Xdpi = 96;
            ii.Ydpi = 96;
            ii.Flags = SinkFlagsFullWidth;
            VerifyHRSuccess(pImageSink->BeginSink(&ii, NULL));
            __try {
                VerifyHRFailure(pImageSink->GetPixelDataBuffer(NULL, PixelFormat32bppARGB, TRUE, NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetPixelDataBuffer excepted with NULL pointers"));
                info(DETAIL, _LOCATIONT);
            }
            __try {
                VerifyHRFailure(pImageSink->GetPixelDataBuffer(&rc, PixelFormat32bppARGB, TRUE, NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetPixelDataBuffer excepted with NULL bitmap data pointer"));
                info(DETAIL, _LOCATIONT);
            }
            __try {
                VerifyHRFailure(pImageSink->GetPixelDataBuffer(NULL, PixelFormat32bppARGB, TRUE, &bmd));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetPixelDataBuffer excepted with NULL rect pointer"));
                info(DETAIL, _LOCATIONT);
            }

            // get prepared to test the Release method
            VerifyHRSuccess(pImageSink->GetPixelDataBuffer(&rc, PixelFormat32bppARGB, TRUE, &bmd));
            __try {
                VerifyHRFailure(pImageSink->ReleasePixelDataBuffer(NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("ReleasePixelDataBuffer excepted with NULL pointer"));
                info(DETAIL, _LOCATIONT);
            }

            BitmapData bmdZeroed;
            memset(&bmdZeroed, 0x00, sizeof(BitmapData));
            __try {
                VerifyHRFailure(pImageSink->ReleasePixelDataBuffer(&bmdZeroed));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("ReleasePixelDataBuffer excepted with empty bitmap data"));
                info(DETAIL, _LOCATIONT);
            }
            VerifyHRSuccess(pImageSink->ReleasePixelDataBuffer(&bmd));
            VerifyHRSuccess(pImageSink->EndSink(S_OK));
            if (pfnResetImageSink)
            {
                VerifyHRSuccess(pfnResetImageSink(pImageSink));
            }
            
            info(UNINDENT, NULL);
        }
        info(ECHO | INDENT, TEXT("Testing with valid parameters"));

        info(ECHO, TEXT("Calling GetPixelDataBuffer before BeginSink (should fail)"));
        VerifyHRFailure(pImageSink->GetPixelDataBuffer(&rc, PixelFormat32bppARGB, TRUE, &bmd));
        info(ECHO, _T("Calling ReleasePixelDataBuffer before BeginSink (should fail)"));
        VerifyHRFailure(pImageSink->ReleasePixelDataBuffer(&bmd));

        // get the image sink back to a known state.
        if (pfnResetImageSink)
        {
            VerifyHRSuccess(pfnResetImageSink(pImageSink));
        }

        info(ECHO | INDENT, _T("Testing With different pixel formats"));
        for (int iFormat = 0; iFormat < g_cPixelFormats; iFormat++)
        {
            HRESULT hr = E_FAIL;
            ii.RawDataFormat = IMGFMT_MEMORYBMP;
            ii.PixelFormat = g_rgPixelFormats[iFormat];
            ii.Width = uiWidth;
            ii.Height = uiHeight;
            ii.TileWidth = ii.Width;
            ii.TileHeight = ii.Height;
            ii.Xdpi = 96;
            ii.Ydpi = 96;
            ii.Flags = SinkFlagsFullWidth;

            info(ECHO, TEXT("Calling BeginSink with %s"),
                g_mapPIXFMT[ii.PixelFormat]);
            VerifyHRSuccess(pImageSink->BeginSink(&ii, NULL));

            info(ECHO, TEXT("BeginSink set pixel format %s"),
                g_mapPIXFMT[ii.PixelFormat]);

            if (IsIndexedPixelFormat(ii.PixelFormat))
            {
                // Should cause no exceptions without palette
                info(ECHO, _T("Calling GetPixelDataBuffer with no palette set (should not crash)"));
                hr = pImageSink->GetPixelDataBuffer(&rc, ii.PixelFormat, TRUE, &bmd);
                
                cp256.Flags = PaletteFlagsHalftone;
                cp256.Count = 1 << GetPixelFormatSize(ii.PixelFormat);
                for (int iCntr = 0; iCntr < cp256.Count; iCntr++)
                {
                    // make sure there is no alpha.
                    cp256.Entries[iCntr] = 0xFF000000 | RandomInt(0, 0xFFFFFF);
                }
                info(ECHO, _T("Calling SetPalette with valid palette"));
                VerifyHRSuccess(pImageSink->SetPalette((ColorPalette*)&cp256));
            }

            if (FAILED(hr))
            {
                info(ECHO, _T("Calling GetPixelDataBuffer with palette set (should succeed)"));
                VerifyHRSuccess(pImageSink->GetPixelDataBuffer(&rc, ii.PixelFormat, TRUE, &bmd));
            }
            info(ECHO, _T("Calling ReleasePixelDataBuffer (should succeed)"));
            VerifyHRSuccess(pImageSink->ReleasePixelDataBuffer(&bmd));

            info(ECHO, _T("Calling EndSink (should succeed)"));
            VerifyHRSuccess(pImageSink->EndSink(S_OK));

            if (pfnResetImageSink)
            {
                VerifyHRSuccess((*pfnResetImageSink)(pImageSink));
            }
        }
        info(UNINDENT, NULL);

        ii.RawDataFormat = IMGFMT_MEMORYBMP;
        ii.PixelFormat = PixelFormat32bppARGB;
        ii.Width = 10;
        ii.Height = 10;
        ii.TileWidth = ii.Width;
        ii.TileHeight = ii.Height;
        ii.Xdpi = 96;
        ii.Ydpi = 96;
        ii.Flags = SinkFlagsFullWidth | SinkFlagsMultipass;

        info(ECHO, TEXT("Using MULTIPASS in BeginSink"));
        VerifyHRSuccess(pImageSink->BeginSink(&ii, NULL));

        if (ii.Flags & SinkFlagsMultipass)
        {
            info(ECHO, TEXT("BeginSink accepted MULTIPASS flag, calling GetPixelDataBuffer with lastPass == FALSE (should succeed)"));
            VerifyHRSuccess(pImageSink->GetPixelDataBuffer(&rc, ii.PixelFormat, FALSE, &bmd));
            info(ECHO, _T("Releasing buffer (should succeed)"));
            VerifyHRSuccess(pImageSink->ReleasePixelDataBuffer(&bmd));
            info(ECHO, _T("GetPixelDataBuffer with lastPass = TRUE (should succeed)"));
            VerifyHRSuccess(pImageSink->GetPixelDataBuffer(&rc, ii.PixelFormat, TRUE, &bmd));
            info(ECHO, _T("Releasing buffer (should succeed)"));
            VerifyHRSuccess(pImageSink->ReleasePixelDataBuffer(&bmd));
        }
        else
        {
            info(ECHO, TEXT("BeginSink did not accept MULTIPASS flag, GetPixelDataBuffer with lastPass == FALSE should fail"));
            if (SUCCEEDED(VerifyHRFailure(pImageSink->GetPixelDataBuffer(&rc, ii.PixelFormat, FALSE, &bmd))))
            {
                // Should have failed, but release anyway.
                info(ECHO, _T("Releasing buffer"));
                VerifyHRSuccess(pImageSink->ReleasePixelDataBuffer(&bmd));
            }
            info(ECHO, _T("GetPixelDataBuffer with lastPass = TRUE (should succeed)"));
            VerifyHRSuccess(pImageSink->GetPixelDataBuffer(&rc, ii.PixelFormat, TRUE, &bmd));
            info(ECHO, _T("Releasing buffer (should succeed)"));
            VerifyHRSuccess(pImageSink->ReleasePixelDataBuffer(&bmd));
        }

        info(ECHO, _T("Calling EndSink (should succeed)"));
        VerifyHRSuccess(pImageSink->EndSink(S_OK));

        if (pfnResetImageSink)
        {
            VerifyHRSuccess(pfnResetImageSink(pImageSink));
        }
        
        ii.RawDataFormat = IMGFMT_MEMORYBMP;
        ii.PixelFormat = PixelFormat32bppARGB;
        ii.Width = uiWidth;
        ii.Height = uiHeight;
        ii.TileWidth = ii.Width;
        ii.TileHeight = ii.Height;
        ii.Xdpi = 96;
        ii.Ydpi = 96;
        ii.Flags = SinkFlagsFullWidth;

        // Now try doing things out of order.
        info(ECHO, _T("Calling BeginSink (should succeed)"));
        VerifyHRSuccess(pImageSink->BeginSink(&ii, NULL));
        // This call can succeed on some encoders. Verify that a crash doesn't
        // happen.
        info(ECHO, _T("Calling ReleasePixelDataBuffer without calling GetPixelDataBuffer (should not crash)"));
        pImageSink->ReleasePixelDataBuffer(&bmd);
        info(ECHO, _T("Calling EndSink (should not crash)"));
        pImageSink->EndSink(S_OK);

        if (pfnResetImageSink)
        {
            VerifyHRSuccess(pfnResetImageSink(pImageSink));
        }

        info(ECHO, _T("Calling BeginSink (should succeed)"));
        VerifyHRSuccess(pImageSink->BeginSink(&ii, NULL));

        info(ECHO, _T("Calling GetPixelDataBuffer (should succeed)"));
        VerifyHRSuccess(pImageSink->GetPixelDataBuffer(&rc, ii.PixelFormat, TRUE, &bmd));
        // Verify that a second call causes no exceptions.
        info(ECHO, _T("Calling GetPixelDataBuffer a second time without first releasing the buffer (should not crash)"));
        pImageSink->GetPixelDataBuffer(&rc, ii.PixelFormat, TRUE, &bmd);
        info(ECHO, _T("Calling ReleasePixelDataBuffer (should succeed)"));
        VerifyHRSuccess(pImageSink->ReleasePixelDataBuffer(&bmd));

        info(ECHO, _T("Calling EndSink (should succeed)"));
        VerifyHRSuccess(pImageSink->EndSink(S_OK));

        if (pfnResetImageSink)
        {
            VerifyHRSuccess(pfnResetImageSink(pImageSink));
        }

        info(ECHO, _T("Calling BeginSink (should succeed)"));
        VerifyHRSuccess(pImageSink->BeginSink(&ii, NULL));

        info(ECHO, _T("Calling GetPixelDataBuffer (should succeed)"));
        VerifyHRSuccess(pImageSink->GetPixelDataBuffer(&rc, ii.PixelFormat, TRUE, &bmd));
        info(ECHO, _T("Calling ReleasePixelDataBuffer (should succeed)"));
        VerifyHRSuccess(pImageSink->ReleasePixelDataBuffer(&bmd));

        info(ECHO, _T("Calling ReleasePixelDataBuffer a second time (should not crash)"));
        // Some encoders succeed with a second call, but verify that no exceptions occur.
        pImageSink->ReleasePixelDataBuffer(&bmd);
        
        info(ECHO, _T("Calling EndSink (should succeed)"));
        VerifyHRSuccess(pImageSink->EndSink(S_OK));

        info(UNINDENT, NULL);
        return getCode();
    }
    INT TestGetPushProperty(IImageSink* pImageSink, SinkParam* psp, PFNRESETIMAGESINK pfnResetImageSink)
    {
        ImageInfo ii;
        PropertyItem * ppi;
        int iBufferSize;
        CHAR * szTitle = "Title";
        HRESULT hr;
        UINT uiWidth, uiHeight;
        if (psp->uiFlags & spfSize)
        {
            uiWidth = psp->pSize->cx;
            uiHeight = psp->pSize->cy;
        }
        else
        {
            uiWidth = 10;
            uiHeight = 10;
        }

        if (g_fBadParams)
        {
            info(ECHO | INDENT, TEXT("Testing with invalid parameters"));
            ii.RawDataFormat = IMGFMT_MEMORYBMP;
            ii.PixelFormat = PixelFormat32bppARGB;
            ii.Width = uiWidth;
            ii.Height = uiHeight;
            ii.TileWidth = ii.Width;
            ii.TileHeight = ii.Height;
            ii.Xdpi = 96;
            ii.Ydpi = 96;
            ii.Flags = SinkFlagsFullWidth;
            PropertyItem rgPI[2];
            
            VerifyHRSuccess(pImageSink->BeginSink(&ii, NULL));

            PresetPtr(ppi);
            __try {
                VerifyHRFailure(pImageSink->GetPropertyBuffer(0, &ppi));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetPropBuf excepted with 0 size"));
                info(DETAIL, _LOCATIONT);
            }
            if (PTRPRESET != ppi && ppi)
            {
                info(FAIL, TEXT("Filled out pointer with 0 size"));
                VerifyHRSuccess(pImageSink->PushPropertyItems(0, 0, ppi));
            }

            __try {
                VerifyHRFailure(pImageSink->GetPropertyBuffer(2 * sizeof(PropertyItem), NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("GetPropBuf excepted with NULL pointer"));
                info(DETAIL, _LOCATIONT);
            }

            __try {
                VerifyHRFailure(pImageSink->PushPropertyItems(2, 2 * sizeof(PropertyItem), NULL));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("PushPropItem excepted with NULL pointer"));
                info(DETAIL, _LOCATIONT);
            }

            __try {
                VerifyHRFailure(pImageSink->PushPropertyItems(2, sizeof(PropertyItem), rgPI));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("PushPropItem excepted with invalid buffer size"));
                info(DETAIL, _LOCATIONT);
            }
            VerifyHRSuccess(pImageSink->EndSink(S_OK));
            if (pfnResetImageSink)
            {
                VerifyHRSuccess(pfnResetImageSink(pImageSink));
            }
            info(UNINDENT, NULL);
        }

        info(ECHO | INDENT, TEXT("Testing with valid parameters"));
        
        ii.RawDataFormat = IMGFMT_MEMORYBMP;
        ii.PixelFormat = PixelFormat32bppARGB;
        ii.Width = uiWidth;
        ii.Height = uiHeight;
        ii.TileWidth = ii.Width;
        ii.TileHeight = ii.Height;
        ii.Xdpi = 96;
        ii.Ydpi = 96;
        ii.Flags = SinkFlagsFullWidth;
        info(ECHO, _T("Calling BeginSink with valid parameters (should succeed)"));
        VerifyHRSuccess(pImageSink->BeginSink(&ii, NULL));
        PresetPtr(ppi);
        iBufferSize = 2 * sizeof(PropertyItem) + 2 * sizeof(LONG) + strlen(szTitle) + 1;
        info(ECHO, _T("Calling GetPropertyBuffer"));
        hr = pImageSink->GetPropertyBuffer(iBufferSize, &ppi);
        if (E_NOTIMPL == hr)
        {
            info(ECHO, TEXT("Unsupported!"));
            info(UNINDENT, NULL);
            return getCode();
        }
        else if (FAILED(hr))
        {
            info(FAIL, TEXT("GetPropertyBuffer failed with hr = %08x"), hr);
            info(DETAIL, _LOCATIONT);
        }
        else if (!ppi || PTRPRESET == ppi)
        {
            info(FAIL, TEXT("GetPropertyBuffer did not set the pointer"));
            info(DETAIL, _LOCATIONT);
        }
        else
        {
            // Set up the property items
            info(ECHO, _T("Calling PushPropertyItems (GAMMA and IMAGE_TITLE) (should succeed)"));
            ppi[0].id = PropertyTagGamma;
            ppi[0].length = 2 * sizeof(LONG);
            ppi[0].type = PropertyTagTypeRational;
            ppi[0].value = (void*)(ppi + 2);
            ((DWORD*)(ppi[0].value))[0] = 1000;
            ((DWORD*)(ppi[0].value))[1] = 1000;
            ppi[1].id = PropertyTagImageTitle;
            ppi[1].length = strlen(szTitle) + 1;
            ppi[1].type = PropertyTagTypeASCII;
            ppi[1].value = (void*)((BYTE*)(ppi + 2) + 2 * sizeof(LONG));
            strcpy((CHAR*)(ppi[1].value), szTitle);

            VerifyHRSuccess(pImageSink->PushPropertyItems(2, iBufferSize, ppi));
        }

        // Some encoders cannot EndSink successfully if no data was set.
        info(ECHO, _T("Calling EndSink (should not crash)"));
        pImageSink->EndSink(S_OK);
        
        return getCode();
    }
}
