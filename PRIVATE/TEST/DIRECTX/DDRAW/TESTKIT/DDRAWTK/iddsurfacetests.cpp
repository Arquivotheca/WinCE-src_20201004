//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//

#include "main.h"

#include <DDrawUty.h>

#include "IDDSurfaceTests.h"
#include "DDrawUty_Config.h"
#include "DDrawUty_Verify.h"
#include "DDTestKit_Modifiers.h"

using namespace com_utilities;
using namespace DebugStreamUty;
using namespace DDrawUty;
using namespace DDrawVerifyUty;

namespace
{
    ////////////////////////////////////////////////////////////////////////////////
    // VerifyOutput
    // prompts the user to verify the output matches the expected output.
    eTestResult VerifyOutput(BOOL bInteractive, 
                                                bool bDelayed, 
                                                IDirectDrawSurface *piDDS, 
                                                TCHAR *Display,
                                                RECT *prcDest = NULL)
    {
        eTestResult tr = trPass;
        HDC hdc;
        HRESULT hr;
        const int iInstrWidth = 1024;
        TCHAR szInstructions[iInstrWidth]={NULL};
        int iWidth = 190;
        static RECT rc = {0, 0, 0, 0};
        DDSURFACEDESC ddsd;
        memset(&ddsd, 0x00, sizeof(DDSURFACEDESC));
        ddsd.dwSize = sizeof(DDSURFACEDESC);
        ddsd.dwFlags = DDSD_ALL;
        hr = piDDS->GetSurfaceDesc(&ddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trFail);

        if (ddsd.dwWidth < (DWORD) iWidth + 10)
        {
            iWidth = ddsd.dwWidth - 10;
            dbgout << "Cutting down on width available for string: " << iWidth << endl;
        }
            
        if(bInteractive || bDelayed)
        {
            // construct our instruction string
            _tcsncpy(szInstructions, Display, iInstrWidth-1);
            if (bInteractive)
                _tcsncat(szInstructions, TEXT("\nPress 'p' for pass, 'f' for fail, or 'a' for abort."), iInstrWidth - _tcslen(szInstructions) - 1);
            else if(bDelayed)
                _tcsncat(szInstructions, TEXT("\nSleeping for verification."), iInstrWidth - _tcslen(szInstructions) - 1);
        
            // print out what's going on to the user.
            hr = piDDS->GetDC(&hdc);
            CheckHRESULT(hr, "GetDC", trFail);

            // make sure any previous messages are cleared.
            BitBlt(hdc, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, NULL, 0, 0, WHITENESS);

            // determine the size of the rectangle to clear for the current message.
            SetRect(&rc, 5, 30, iWidth + 5, 31);

            DrawText(hdc, szInstructions, -1, &rc, DT_WORDBREAK | DT_CALCRECT);

            // draw the current message.
            BitBlt(hdc, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, NULL, 0, 0, WHITENESS);
            DrawText(hdc, szInstructions, -1, &rc, DT_WORDBREAK);
            
            hr = piDDS->ReleaseDC(hdc);
            CheckHRESULT(hr, "ReleaseDC", trFail);

            // spit the instructions out to the debug output in case they are obscured in the window.
            dbgout << szInstructions << endl;

            if (bInteractive)
            {
                BOOL bDone = FALSE;

                while (!bDone)
                {
                    if (0x80 & GetAsyncKeyState('P'))
                    {
                        dbgout(LOG_PASS) << "Pattern successfully displayed to screen" << endl;
                        bDone = TRUE;
                    }
                    else if (0x80 & GetAsyncKeyState('F'))
                    {
                        dbgout(LOG_FAIL) << "Pattern NOT displayed to screen" << endl;
                        tr |= trFail;
                        bDone = TRUE;
                    }
                    else if (0x80 & GetAsyncKeyState('A'))
                    {
                        dbgout(LOG_ABORT) << "User requested abort" << endl;
                        tr |= trAbort;
                        bDone = TRUE;
                    }
                    Sleep(0);
                }
                // sleep long enough so we don't double register this response from the user
                Sleep(500);
            }
            else if(bDelayed)
            {
                // Pause to allow any viewer to verify the message
                dbgout << "In Delayed mode, assuming user verification without interaction" << endl;
                Sleep(5000);
            }
        }
        return tr;
    }

    eTestResult SetColorVal(double dwRed, double dwGreen,double dwBlue, IDirectDrawSurface *piDDS, DWORD *dwFillColor)
    {
        CDDSurfaceDesc cddsd;
        HRESULT hr;
        DWORD *dwSrc=NULL;
        DWORD Y, U, V;
        CDDBltFX cddbltfx;
        
        hr = piDDS->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        if(cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC)
        {
            dwRed=(dwRed*0xFF);
            dwGreen=(dwGreen*0xFF);
            dwBlue=(dwBlue*0xFF);
            
            Y= (DWORD)(0.29*dwRed + 0.57*dwGreen + 0.14*dwBlue);
            U= (DWORD)(128.0 - 0.14*dwRed - 0.29*dwGreen+ 0.43*dwBlue);
            V= (DWORD)(128.0 + 0.36*dwRed - 0.29*dwGreen - 0.07*dwBlue);
            
            if(cddsd.ddpfPixelFormat.dwFourCC==MAKEFOURCC('Y', 'U', 'Y', 'V'))
            {
                *dwFillColor=((V<< 24)&0xFF000000)|((Y<< 16)&0x00FF0000)|((U<< 8)&0x0000FF00)|(Y&0x000000FF);
            }
            else if(cddsd.ddpfPixelFormat.dwFourCC==MAKEFOURCC('V', 'Y', 'U', 'Y'))
            {
                *dwFillColor=((Y<< 24)&0xFF000000)|((U<< 16)&0x00FF0000)|((Y<< 8)&0x0000FF00)|(V&0x000000FF);
            }
            else if(cddsd.ddpfPixelFormat.dwFourCC==MAKEFOURCC('U', 'Y', 'V', 'Y'))
            {
                *dwFillColor=((Y<< 24)&0xFF000000)|((V<< 16)&0x00FF0000)|((Y<< 8)&0x0000FF00)|(U&0x000000FF);
            }
            else
            {
                return trFail;
                dbgout << "Unknown FourCC format " << cddsd.ddpfPixelFormat.dwFourCC << endl;
            }
            dbgout << "Filling with " << HEX(*dwFillColor) << endl;
            if(FAILED(piDDS->Lock(NULL, &cddsd, DDLOCK_WAIT, NULL)))
            {
                dbgout << "failure locking Destination surface for manual fill" << endl;
                return trFail;
            }
            dwSrc=(DWORD*)cddsd.lpSurface;
            for(int y=0;y<(int)cddsd.dwHeight;y++)
            {
                for(int x=0;x<(int)cddsd.dwWidth/2;x++)
                {
                    dwSrc[x]=*dwFillColor;
                }
                dwSrc+=cddsd.lPitch/4;
            }
            piDDS->Unlock(NULL);
        }
        else if(cddsd.ddpfPixelFormat.dwFlags & DDPF_RGB)
        {
            *dwFillColor=cddbltfx.dwFillColor=((DWORD)(cddsd.ddpfPixelFormat.dwRBitMask * dwRed) & cddsd.ddpfPixelFormat.dwRBitMask)+
                                            ((DWORD)(cddsd.ddpfPixelFormat.dwGBitMask * dwGreen) & cddsd.ddpfPixelFormat.dwGBitMask)+
                                            ((DWORD)(cddsd.ddpfPixelFormat.dwBBitMask * dwBlue) & cddsd.ddpfPixelFormat.dwBBitMask);
            dbgout << "Filling with " << HEX(cddbltfx.dwFillColor) << endl;
            hr = piDDS->Blt(NULL, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &cddbltfx);
            if (DDERR_UNSUPPORTED == hr)
            {
                dbgout << "Skipping: Blting is unsupported with this surface type." << endl;
                return trPass;
            }
            else CheckHRESULT(hr, "Blt with colorfill and wait", trFail);
        }
        else
        {
            dbgout << "Unknown pixel format, ddpf.dwflags is " << cddsd.ddpfPixelFormat.dwFlags << endl;
            return trFail;
        }

        return trPass;
    }
}


namespace Test_IDirectDrawSurface
{
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_Flip::TestIDirectDrawSurface
    //  Tests the flip Method of IDirectDrawSurface.
    //
    eTestResult CTest_Flip::TestIDirectDrawSurface()
    {
        eTestResult tr = trPass;

        // Only test if this is a flipping primary surface
        if ((m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
            (m_cddsd.ddsCaps.dwCaps & DDSCAPS_FLIP))
        {
            // If this is a primary is system memory, then we can't lock
            BOOL fCanLock = !((m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
                              (m_cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY));
            const DWORD dwPrimary    = 0x12345678,
                        dwBackbuffer = 0x9ABCDEF0;
            CDDSurfaceDesc cddsd;
            HRESULT hr;

            if (fCanLock)
            {
                // Lock and write known data to the initial primary
                dbgout << "Writing to primary" << endl;
                hr = m_piDDS->Lock(NULL, &cddsd, DDLOCK_WAIT, NULL);
                CheckHRESULT(hr, "Lock", trAbort);
                *(DWORD*)cddsd.lpSurface = dwPrimary;
                hr = m_piDDS->Unlock(cddsd.lpSurface);
                CheckHRESULT(hr, "Unlock", trAbort);
            }

            // Flip to the first backbuffer
            dbgout << "Flipping to first backbuffer" << endl;
            hr = m_piDDS->Flip(NULL, DDFLIP_WAIT);
            CheckHRESULT(hr, "Flip", trFail);
            
            if (fCanLock)
            {
                // Lock and write known data to the first backbuffer
                dbgout << "Writing to first backbuffer" << endl;
                hr = m_piDDS->Lock(NULL, &cddsd, DDLOCK_WAIT, NULL);
                CheckHRESULT(hr, "Lock", trAbort);
                *(DWORD*)cddsd.lpSurface = dwBackbuffer;
                hr = m_piDDS->Unlock(cddsd.lpSurface);
                CheckHRESULT(hr, "Unlock", trAbort);
            }

            // Flip back to the original frontbuffer
            dbgout << "Flipping to primary" << endl;
            for (int i = 0; i < (int)m_cddsd.dwBackBufferCount; i++)
            {
                dbgout << "Flip #" << i << endl;
                hr = m_piDDS->Flip(NULL, DDFLIP_WAIT);
                CheckHRESULT(hr, "Flip", trFail);
            }

            if (fCanLock)
            {
                // Verify the original data
                dbgout << "Verifying original primary data" << endl;
                hr = m_piDDS->Lock(NULL, &cddsd, DDLOCK_WAIT, NULL);
                CheckHRESULT(hr, "Lock", trAbort);
                QueryCondition(dwPrimary != *(DWORD*)cddsd.lpSurface, "Expected Primary Data not found", tr |= trFail);
                hr = m_piDDS->Unlock(cddsd.lpSurface);
                CheckHRESULT(hr, "Unlock", trAbort);
            }

            // Flip back to the original backbuffer without waiting
            dbgout << "Flipping to first backbuffer without DDFLIP_WAIT" << endl;
            while (1)
            {
                hr = m_piDDS->Flip(NULL, 0);
                if (hr != DDERR_WASSTILLDRAWING)
                    break;
            }
            CheckHRESULT(hr, "Flip", trFail);

            if (fCanLock)
            {
                // Verify the original data
                dbgout << "Verifying original backbuffer data" << endl;
                hr = m_piDDS->Lock(NULL, &cddsd, DDLOCK_WAIT, NULL);
                CheckHRESULT(hr, "Lock", trAbort);
                QueryCondition(dwBackbuffer != *(DWORD*)cddsd.lpSurface, "Expected Backbuffer Data not found", tr |= trFail);
                hr = m_piDDS->Unlock(cddsd.lpSurface);
                CheckHRESULT(hr, "Unlock", trAbort);
            }

            // Do a few more flips for good measure...
            for (i = 0; i < 30; i++)
            {
                hr = m_piDDS->Flip(NULL, DDFLIP_WAIT);
                CheckHRESULT(hr, "Flip", trFail);
            }
        }
        else
            dbgout << "Skipping Surface" << endl;

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_ColorFillBlts::TestIDirectDrawSurface
    //  tests colorfilling on standard surfaces.
    //
    eTestResult CTest_ColorFillBlts::TestIDirectDrawSurface()
    {
        using namespace DDrawUty::Surface_Helpers;
        eTestResult tr = trPass;
        HRESULT hr;
        
        CDDSurfaceDesc cddsd;
        CDDrawSurfaceVerify cddsvSurfaceVerify;
        DWORD dwFillColor;
        DWORD  dwHALBltCaps;
        DWORD  dwHALFXCaps;
        DWORD  dwHALCKeyCaps;
        DWORD  dwHELBltCaps;
        DWORD  dwHELFXCaps;
        DWORD  dwHELCKeyCaps;
 
        hr = m_piDDS->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
        
        GetBltCaps(cddsd, cddsd, GBC_HAL, &dwHALBltCaps, &dwHALFXCaps, &dwHALCKeyCaps);
        GetBltCaps(cddsd, cddsd, GBC_HEL, &dwHELBltCaps, &dwHELFXCaps, &dwHELCKeyCaps);

        // if we can't blit, do nothing
        if(!(dwHALBltCaps & DDCAPS_BLTCOLORFILL) && !(dwHELBltCaps & DDCAPS_BLTCOLORFILL))
        {
            dbgout << "Color filling not supported on this surface -- Skipping Test" << endl;
            return trPass;
        }
        
        CheckCondition(!(cddsd.dwFlags & DDSD_CAPS), "CAPS not filled", trAbort);
        CheckCondition(!(cddsd.dwFlags & DDSD_PIXELFORMAT), "Pixel Format not filled", trAbort);
        // verify the surface if rgb, if it is, then color fill blt it.
        if(cddsd.ddpfPixelFormat.dwFlags & DDPF_RGB)
        {
            tr |=cddsvSurfaceVerify.PreVerifyColorFill(m_piDDS.AsInParam());
            // set the color to be red
            tr |= SetColorVal(1,0,0,m_piDDS.AsInParam(), &dwFillColor);
            tr |=cddsvSurfaceVerify.VerifyColorFill(dwFillColor);
            // set the color to be green
            tr |= SetColorVal(0,1,0,m_piDDS.AsInParam(), &dwFillColor);
            tr |= cddsvSurfaceVerify.VerifyColorFill(dwFillColor);
            //set the color to be blue
            tr |= SetColorVal(0,0,1,m_piDDS.AsInParam(), &dwFillColor);
            tr|=cddsvSurfaceVerify.VerifyColorFill(dwFillColor);
        }
        // This test isn't designed for anything but RGB
        else 
        {
            tr|=trSkip;
            dbgout << "Test not capable of testing non-RGB surfaces" << endl;
        }
        return tr;
    }

};

namespace Test_IDirectDrawSurface_TWO
{
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_InteractiveColorFillBlts::TestIDirectDrawSurface
    //  an optionally interactive color fill test
    //
    eTestResult CTest_InteractiveColorFillOverlayBlts::TestIDirectDrawSurface_TWO()
    {
        eTestResult tr = trPass;
        HRESULT hr;
        bool bInteractive=FALSE, 
                bDelayed=FALSE;
        DWORD dwTmp;
        DWORD dwFillColor;
        TCHAR szInstructions[256];
        HRESULT hrExpected = DD_OK;
        CDDSurfaceDesc cddsdSrc, cddsdDst;
        CDDrawSurfaceVerify cddsvSurfaceVerify1, cddsvSurfaceVerify2;
        
        hr = m_piDDS->GetSurfaceDesc(&cddsdSrc);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
        hr = m_piDDSDst->GetSurfaceDesc(&cddsdDst);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
            
        // verify the structures we need are filled
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_CAPS), "CAPS not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_PIXELFORMAT), "Pixel Format not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_HEIGHT), "Height not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_WIDTH), "Width not filled", trAbort);

        if (!(cddsdDst.ddsCaps.dwCaps & DDSCAPS_OVERLAY))
        {
            dbgout << "Destination is not an overlay surface-- Skipping Test" << endl;
            return trPass;
        }

        // verifiy that the pixel formats we need are set
        if(!(cddsdSrc.ddpfPixelFormat.dwFlags & DDPF_RGB))
        {
            dbgout << "test not capable of non-rgb pixel format tests" << endl;
            return trSkip;
        }
        if(!(cddsdDst.ddpfPixelFormat.dwFlags & DDPF_RGB) && !(cddsdDst.ddpfPixelFormat.dwFlags & DDPF_FOURCC))
        {
            dbgout << "test only supports rgb and some fourcc formats on overlays" << endl;
            return trSkip;
        }

        // set the overlay width (1/2 the width of the primary
        int nOverlayHeight = cddsdDst.dwHeight / 2,
                nOverlayWidth  = cddsdDst.dwWidth / 2;

        // Check for a size restriction on overlays
        if (g_DDConfig.HALCaps().dwAlignSizeSrc != 0 || g_DDConfig.HELCaps().dwAlignSizeSrc != 0)
        {
            // If AlignSizeSrc is non-zero, then the source rectange
            // height and width must be a multiple of it

            // We don't determine if we're using HAL or HEL before we make the call to UpdateOverlay.
            // Therefore, we go for the lowest common denominator for determining the alignment.
            DWORD dwAlignSizeSrc = g_DDConfig.HALCaps().dwAlignSizeSrc;
            if (dwAlignSizeSrc < g_DDConfig.HELCaps().dwAlignSizeSrc)
            {
                // Assumption: the smaller (less restrictive) alignment will divide evenly
                // into the larger alignment (e.g. HEL align == 2 and HAL align == 8).
                dwAlignSizeSrc = g_DDConfig.HELCaps().dwAlignSizeSrc;
            }
            nOverlayHeight -= (nOverlayHeight % dwAlignSizeSrc);
            nOverlayWidth  -= (nOverlayWidth  % dwAlignSizeSrc);
        }
        // set the overlay x and y top left positions
        int nDestX = cddsdDst.dwWidth / 4,
            nDestY = cddsdDst.dwHeight / 4;
        // Check for position restrictions on overlays
        if (g_DDConfig.HALCaps().dwAlignBoundaryDest != 0 || g_DDConfig.HELCaps().dwAlignBoundaryDest != 0)
        {
            // If AlignBoundaryDes is non-zero then the destination
            // rectange top-left postion must be a multiple of it

            // We don't determine if we're using HAL or HEL before we make the call to UpdateOverlay.
            // Therefore, we go for the lowest common denominator for determining the alignment.
            DWORD dwAlignBoundaryDest = g_DDConfig.HALCaps().dwAlignBoundaryDest;
            if (dwAlignBoundaryDest < g_DDConfig.HELCaps().dwAlignBoundaryDest)
            {
                // Assumption: the smaller (less restrictive) alignment will divide evenly
                // into the larger alignment (e.g. HEL align == 2 and HAL align == 8).
                dwAlignBoundaryDest = g_DDConfig.HELCaps().dwAlignBoundaryDest;
            }
            nDestX -= (nDestX % dwAlignBoundaryDest);
            nDestY -= (nDestY % dwAlignBoundaryDest);
        }
        // set the overlay
        RECT rcRect = { nDestY, nDestX, nDestY + nOverlayWidth, nDestX + nOverlayHeight};

        // determine if we're interactive, delayed, or normal
        DDrawUty::g_DDConfig.GetSymbol(TEXT("i"), &dwTmp);
        if(dwTmp != 0)
            bInteractive=TRUE;
        DDrawUty::g_DDConfig.GetSymbol(TEXT("d"), &dwTmp);
        if(dwTmp != 0)
            bDelayed=TRUE;

        // prepare for color fill test verification
        tr |= cddsvSurfaceVerify1.PreVerifyColorFill(m_piDDS.AsInParam());
        tr |= cddsvSurfaceVerify2.PreVerifyColorFill(m_piDDSDst.AsInParam());
        
        ///////////////////////////////////////////////////////////////////////
        tr |= SetColorVal(0,0,1,m_piDDS.AsInParam(), &dwFillColor);
        tr |= cddsvSurfaceVerify1.VerifyColorFill(dwFillColor);
        // set the overlay's surfaces color fill and fill it
        tr |= SetColorVal(1,0,0,m_piDDSDst.AsInParam(), &dwFillColor);
        if (tr & trSkip)
            return tr;
        tr |= cddsvSurfaceVerify2.VerifyColorFill(dwFillColor);
        hr = m_piDDSDst->UpdateOverlay(&rcRect, m_piDDS.AsInParam(), &rcRect, DDOVER_SHOW, NULL);
        QueryForHRESULT(hr, hrExpected, "Failure code returned from UpdateOverlay with wait and NULL BltFx", tr|=trFail);
        
        wsprintf(szInstructions, TEXT("The primary is blue and the overlay is red."));
        tr|=VerifyOutput(bInteractive, bDelayed, m_piDDS.AsInParam(), szInstructions);
        if (tr & trAbort)
            return tr;
        ///////////////////////////////////////////////////////////////////////
        
        ///////////////////////////////////////////////////////////////////////
        tr |= SetColorVal(1,0,0,m_piDDS.AsInParam(), &dwFillColor);
        tr |= cddsvSurfaceVerify1.VerifyColorFill(dwFillColor);
        tr |= SetColorVal(0,1,0,m_piDDSDst.AsInParam(), &dwFillColor);
        if (tr & trSkip)
            return tr;
        tr |= cddsvSurfaceVerify2.VerifyColorFill(dwFillColor);
        hr = m_piDDSDst->UpdateOverlay(&rcRect, m_piDDS.AsInParam(), &rcRect, DDOVER_SHOW, NULL);
        QueryForHRESULT(hr, hrExpected, "Failure code returned from UpdateOverlay with wait and NULL BltFx", tr|=trFail);
        wsprintf(szInstructions, TEXT("The primary is red and the overlay is green."));
        tr |= VerifyOutput(bInteractive, bDelayed, m_piDDS.AsInParam(), szInstructions);
        if (tr & trAbort)
            return tr;
        ///////////////////////////////////////////////////////////////////////
        
        ///////////////////////////////////////////////////////////////////////
        tr |= SetColorVal(0,1,0,m_piDDS.AsInParam(), &dwFillColor);
        tr |= cddsvSurfaceVerify1.VerifyColorFill(dwFillColor);
        tr |= SetColorVal(0,0,1,m_piDDSDst.AsInParam(), &dwFillColor);
        if (tr & trSkip)
            return tr;
        tr |= cddsvSurfaceVerify2.VerifyColorFill(dwFillColor);
        
        hr = m_piDDSDst->UpdateOverlay(&rcRect, m_piDDS.AsInParam(), &rcRect, DDOVER_SHOW, NULL);
        QueryForHRESULT(hr, hrExpected, "Failure code returned from UpdateOverlay with wait and NULL BltFx", tr|=trFail);
        wsprintf(szInstructions, TEXT("The primary is green and the overlay is blue."));
        tr |= VerifyOutput(bInteractive, bDelayed, m_piDDS.AsInParam(), szInstructions);
        if (tr & trAbort)
            return tr;
        ///////////////////////////////////////////////////////////////////////
        
        return tr;

    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_Blt::TestIDirectDrawSurface_TWO
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_Blt::TestIDirectDrawSurface_TWO()
    {
        using namespace DDrawUty::Surface_Helpers;
        
        eTestResult tr = trPass;
        CDDSurfaceDesc cddsdSrc,
                                  cddsdDst;
        CDDrawSurfaceVerify cddsvSurfaceVerify;
        DDCOLORKEY ddck;
        DWORD dwBltFlags = 0;
        HRESULT hr;
        RECT rcSrc, rcDst;
        
        hr = m_piDDS->GetSurfaceDesc(&cddsdSrc);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
        hr = m_piDDSDst->GetSurfaceDesc(&cddsdDst);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        // There are some things that must be available in the descripton
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_CAPS), "CAPS not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_PIXELFORMAT), "Pixel Format not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_HEIGHT), "Height not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_WIDTH), "Width not filled", trAbort);

        // See if we should pass any color key flags
        if (SUCCEEDED(m_piDDS->GetColorKey(DDCKEY_SRCBLT, &ddck)))
            dwBltFlags |= DDBLT_KEYSRC;
        // currently the test does not do any dest color keying, but may in the future.
        if (SUCCEEDED(m_piDDSDst->GetColorKey(DDCKEY_DESTBLT, &ddck)))
            dwBltFlags |= DDBLT_KEYDEST;


        // get the dimensions of the rectangle we have to work with (if in normal mode)
        RECT rc;

        if ((cddsdDst.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
            (g_DDConfig.CooperativeLevel() & DDSCL_NORMAL))
            GetWindowRect(g_DirectDraw.m_hWnd, &rc);
        else{
            rc.left=0;
            rc.right=cddsdDst.dwWidth;
            rc.top=0;
            rc.bottom=cddsdDst.dwHeight;
        }

        HRESULT hrExpected = DD_OK;
        
        // Get the HAL blt caps for the correct surface locations
        DWORD  dwHALBltCaps;
        DWORD  dwHALFXCaps;
        DWORD  dwHALCKeyCaps;
        DWORD  dwHELBltCaps;
        DWORD  dwHELFXCaps;
        DWORD  dwHELCKeyCaps;

        GetBltCaps(cddsdSrc, cddsdDst, GBC_HAL, &dwHALBltCaps, &dwHALFXCaps, &dwHALCKeyCaps);
        GetBltCaps(cddsdSrc, cddsdDst, GBC_HEL, &dwHELBltCaps, &dwHELFXCaps, &dwHELCKeyCaps);

        // if we can't blit, do nothing
        if(!(dwHALBltCaps & DDCAPS_BLT) && !(dwHELBltCaps & DDCAPS_BLT))
        {
            dbgout << "blitting not supported between these surfaces -- Skipping Test" << endl;
            return trPass;
        }

        // if we can't source colorkey, and this is the colorkey test, do nothing.
        if((dwBltFlags & DDBLT_KEYSRC) && !(dwHALCKeyCaps & DDCKEYCAPS_SRCBLT) && !(dwHELCKeyCaps & DDCKEYCAPS_SRCBLT))
        {
            dbgout << "source colorkeying not supported -- Skipping Test" << endl;
            return trPass;
        }

        // if we can't dest colorkey, and this is the dest colorkey test, do nothing.
        if((dwBltFlags & DDCKEY_DESTBLT) && !(dwHALCKeyCaps & DDCKEYCAPS_DESTBLT) && !(dwHELCKeyCaps & DDCKEYCAPS_DESTBLT))
        {
            dbgout << "destination colorkeying not supported -- Skipping Test" << endl;
            return trPass;
        }
        
        // offset the dst from the source so if the source/dest are the same, the dest still changes
        int dwWidth=(int)(rc.right-rc.left)/2;
        int dwHeight=(int)(rc.bottom-rc.top)/2;
        
        rcSrc.left=rc.left+7;
        rcSrc.right=rcSrc.left+dwWidth;
        rcSrc.top=rc.top+7;
        rcSrc.bottom=rcSrc.top+dwHeight;
        
        rcDst.left=rc.left;
        rcDst.right=rcDst.left+dwWidth;
        rcDst.top=rc.top;
        rcDst.bottom=rcDst.top+dwHeight;

        tr|=cddsvSurfaceVerify.PreSurfaceVerify(m_piDDS.AsInParam(), rcSrc, m_piDDSDst.AsInParam(), rcDst);
        hr = m_piDDSDst->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, DDBLT_WAIT | dwBltFlags, NULL);
        QueryForHRESULT(hr, hrExpected, "Failure code returned from BLT with wait and NULL BltFx", tr|=trFail);
        tr|=cddsvSurfaceVerify.SurfaceVerify();

        rcDst.left=(int)((rc.right-rc.left)/2);
        rcDst.right=rcDst.left+dwWidth;
        rcDst.top=rc.top;
        rcDst.bottom=rcDst.top+dwHeight;

        tr|=cddsvSurfaceVerify.PreSurfaceVerify(m_piDDS.AsInParam(), rcSrc, m_piDDSDst.AsInParam(), rcDst);
        hr = m_piDDSDst->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, DDBLT_WAIT | dwBltFlags, NULL);
        QueryForHRESULT(hr, hrExpected, "Failure code returned from BLT with wait and NULL BltFx", tr|=trFail);
        tr|=cddsvSurfaceVerify.SurfaceVerify();

        rcDst.left=rc.left;
        rcDst.right=rcDst.left+dwWidth;
        rcDst.top=(int)((rc.bottom-rc.top)/2);
        rcDst.bottom=rcDst.top+dwHeight;

        tr|=cddsvSurfaceVerify.PreSurfaceVerify(m_piDDS.AsInParam(), rcSrc, m_piDDSDst.AsInParam(), rcDst);
        hr = m_piDDSDst->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, DDBLT_WAIT | dwBltFlags, NULL);
        QueryForHRESULT(hr, hrExpected, "Failure code returned from BLT with wait and NULL BltFx", tr|=trFail);
        tr|=cddsvSurfaceVerify.SurfaceVerify();

        rcDst.left=(int)((rc.right-rc.left)/2);
        rcDst.right=rcDst.left+dwWidth;
        rcDst.top=(int)((rc.bottom-rc.top)/2);
        rcDst.bottom=rcDst.top+dwHeight;

        tr|=cddsvSurfaceVerify.PreSurfaceVerify(m_piDDS.AsInParam(), rcSrc, m_piDDSDst.AsInParam(), rcDst);
        hr = m_piDDSDst->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, DDBLT_WAIT | dwBltFlags, NULL);
        QueryForHRESULT(hr, hrExpected, "Failure code returned from BLT with wait and NULL BltFx", tr|=trFail);
        tr|=cddsvSurfaceVerify.SurfaceVerify();


        rcSrc.left=(int)((rc.right-rc.left)/4);
        rcSrc.right=(int)((rc.right-rc.left)-(rc.right-rc.left)/4);
        rcSrc.top=(int)((rc.bottom-rc.top)/4);
        rcSrc.bottom=(int)((rc.bottom-rc.top)-(rc.bottom-rc.top)/4);
        
        rcDst.left=(int)((rc.right-rc.left)/4)+5;
        rcDst.right=(int)((rc.right-rc.left)-(rc.right-rc.left)/4)+5;
        rcDst.top=(int)((rc.bottom-rc.top)/4)+5;
        rcDst.bottom=(int)((rc.bottom-rc.top)-(rc.bottom-rc.top)/4)+5;

        tr|=cddsvSurfaceVerify.PreSurfaceVerify(m_piDDS.AsInParam(), rcSrc, m_piDDSDst.AsInParam(), rcDst);
        hr = m_piDDSDst->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, DDBLT_WAIT | dwBltFlags, NULL);
        QueryForHRESULT(hr, hrExpected, "Failure code returned from BLT with wait and NULL BltFx", tr|=trFail);
        tr|=cddsvSurfaceVerify.SurfaceVerify();

        tr|=cddsvSurfaceVerify.PreSurfaceVerify(m_piDDS.AsInParam(), rcDst, m_piDDSDst.AsInParam(), rcSrc);
        hr = m_piDDSDst->Blt(&rcSrc, m_piDDS.AsInParam(), &rcDst, DDBLT_WAIT | dwBltFlags, NULL);
        QueryForHRESULT(hr, hrExpected, "Failure code returned from BLT with wait and NULL BltFx", tr|=trFail);
        tr|=cddsvSurfaceVerify.SurfaceVerify();

        // do a full surface copy, just for fun.
        rcSrc.left=rc.left;
        rcSrc.right=rc.right;
        rcSrc.top=rc.top;
        rcSrc.bottom=rc.bottom;

        tr|=cddsvSurfaceVerify.PreSurfaceVerify(m_piDDS.AsInParam(), rcSrc, m_piDDSDst.AsInParam(), rcSrc);
        hr = m_piDDSDst->Blt(NULL, m_piDDS.AsInParam(), NULL, DDBLT_WAIT | dwBltFlags, NULL);
        QueryForHRESULT(hr, hrExpected, "Failure code returned from BLT with wait and NULL BltFx", tr|=trFail);
        tr|=cddsvSurfaceVerify.SurfaceVerify();
        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_Blt::TestIDirectDrawSurface_TWO
    //  Tests overlay blitting, optionally interactive.
    //
    eTestResult CTest_InteractiveOverlayBlt::TestIDirectDrawSurface_TWO()
    {
        using namespace DDrawUty::Surface_Helpers;

        eTestResult tr = trPass;
        CDDSurfaceDesc cddsdSrc,
                                  cddsdDst;

        DWORD dwBltFlags = 0;
        DWORD dwAvailBltFlags = 0;
        HRESULT hr;
        RECT rcSrc;
        const DWORD cdwInstructionSize = 256;
        TCHAR szInstructions[cdwInstructionSize];
        DDCOLORKEY ddck;
        DWORD dwTmp;
        bool bInteractive=FALSE, bDelayed=FALSE;

        // determine the test interactivity.
        DDrawUty::g_DDConfig.GetSymbol(TEXT("i"), &dwTmp);
        if(dwTmp != 0)
            bInteractive=TRUE;
        DDrawUty::g_DDConfig.GetSymbol(TEXT("d"), &dwTmp);
        if(dwTmp != 0)
            bDelayed=TRUE;

        hr = m_piDDS->GetSurfaceDesc(&cddsdSrc);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
        hr = m_piDDSDst->GetSurfaceDesc(&cddsdDst);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        // There are some things that must be available in the descripton
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_CAPS), "CAPS not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_PIXELFORMAT), "Pixel Format not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_HEIGHT), "Height not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_WIDTH), "Width not filled", trAbort);

        if (!(cddsdDst.ddsCaps.dwCaps & DDSCAPS_OVERLAY))
        {
            dbgout << "Destination is not an overlay surface-- Skipping Test" << endl;
            return trPass;
        }

        // See if we should pass any color key flags
        if (SUCCEEDED(m_piDDSDst->GetColorKey(DDCKEY_SRCOVERLAY, &ddck)))
            dwAvailBltFlags |= DDOVER_KEYSRC;
        if (SUCCEEDED(m_piDDS->GetColorKey(DDCKEY_DESTOVERLAY, &ddck)))
            dwAvailBltFlags |= DDOVER_KEYDEST;

        HRESULT hrExpected = DD_OK;
        
        // Get the HAL blt caps for the correct surface locations
        DWORD  dwHALBltCaps;
        DWORD  dwHALFXCaps;
        DWORD  dwHALCKeyCaps;
        DWORD  dwHELBltCaps;
        DWORD  dwHELFXCaps;
        DWORD  dwHELCKeyCaps;
        DWORD  dwAlignSizeSrc = 1;
        DWORD  dwAlignBoundaryDest = 1;
        DWORD  dwHALCaps;
        DWORD  dwHELCaps;

        GetBltCaps(cddsdSrc, cddsdDst, GBC_HAL, &dwHALBltCaps, &dwHALFXCaps, &dwHALCKeyCaps);
        GetBltCaps(cddsdSrc, cddsdDst, GBC_HEL, &dwHELBltCaps, &dwHELFXCaps, &dwHELCKeyCaps);

        dwHALCaps = g_DDConfig.HALCaps().dwCaps;
        dwHELCaps = g_DDConfig.HELCaps().dwCaps;

        // if we can't source colorkey, and this is the colorkey test, do nothing.
        if((dwAvailBltFlags & DDOVER_KEYSRC) && !(dwHALCKeyCaps & DDCKEYCAPS_SRCOVERLAYYUV) && !(dwHELCKeyCaps & DDCKEYCAPS_SRCOVERLAYYUV))
        {
            dbgout << "source colorkeying not supported on overlays-- Skipping Test" << endl;
            return trPass;
        }
        if((dwAvailBltFlags & DDOVER_KEYSRC) && !(dwHALCKeyCaps & DDCKEYCAPS_SRCOVERLAY) && !(dwHELCKeyCaps & DDCKEYCAPS_SRCOVERLAY))
        {
            dbgout << "source colorkeying not supported on overlays-- Skipping Test" << endl;
            return trPass;
        }
         if((dwAvailBltFlags & DDOVER_KEYDEST) && !(dwHALCKeyCaps & DDCKEYCAPS_DESTOVERLAY) && !(dwHELCKeyCaps & DDCKEYCAPS_DESTOVERLAY))
        {
            dbgout << "dest colorkeying not supported on overlays-- Skipping Test" << endl;
            return trPass;
        }

        // set the overlay width
        int nOverlayHeight = cddsdDst.dwHeight / 2,
             nOverlayWidth  = cddsdDst.dwWidth / 2;

        // Check for a size restriction on overlays
        if (g_DDConfig.HALCaps().dwAlignSizeSrc != 0 || g_DDConfig.HELCaps().dwAlignSizeSrc != 0)
        {
            // If AlignSizeSrc is non-zero, then the source rectange
            // height and width must be a multiple of it

            // We don't determine if we're using HAL or HEL before we make the call to UpdateOverlay.
            // Therefore, we go for the lowest common denominator for determining the alignment.
            dwAlignSizeSrc = g_DDConfig.HALCaps().dwAlignSizeSrc;
            if (dwAlignSizeSrc < g_DDConfig.HELCaps().dwAlignSizeSrc)
            {
                // Assumption: the smaller (less restrictive) alignment will divide evenly
                // into the larger alignment (e.g. HEL align == 2 and HAL align == 8).
                dwAlignSizeSrc = g_DDConfig.HELCaps().dwAlignSizeSrc;
            }
            nOverlayHeight -= (nOverlayHeight % dwAlignSizeSrc);
            nOverlayWidth  -= (nOverlayWidth  % dwAlignSizeSrc);
        }
        // set the overlay x and y top left positions
        int nDestX = cddsdDst.dwWidth / 4,
            nDestY = cddsdDst.dwHeight / 4;
        // Check for position restrictions on overlays
        if (g_DDConfig.HALCaps().dwAlignBoundaryDest != 0 || g_DDConfig.HELCaps().dwAlignBoundaryDest != 0)
        {
            // If AlignBoundaryDes is non-zero then the destination
            // rectange top-left postion must be a multiple of it

            // We don't determine if we're using HAL or HEL before we make the call to UpdateOverlay.
            // Therefore, we go for the lowest common denominator for determining the alignment.
            dwAlignBoundaryDest = g_DDConfig.HALCaps().dwAlignBoundaryDest;
            if (dwAlignBoundaryDest < g_DDConfig.HELCaps().dwAlignBoundaryDest)
            {
                // Assumption: the smaller (less restrictive) alignment will divide evenly
                // into the larger alignment (e.g. HEL align == 2 and HAL align == 8).
                dwAlignBoundaryDest = g_DDConfig.HELCaps().dwAlignBoundaryDest;
            }
            nDestX -= (nDestX % dwAlignBoundaryDest);
            nDestY -= (nDestY % dwAlignBoundaryDest);
        }
        // set the overlay position/size
        int x = GetSystemMetrics(SM_CXSCREEN) - nOverlayWidth;
        int y = GetSystemMetrics(SM_CYSCREEN) - nOverlayHeight;
        RECT rcRect[] = {
            // Center
            { nDestX, nDestY, nDestX + nOverlayWidth, nDestY + nOverlayHeight},

            // Lower Right
            { x - dwAlignBoundaryDest, 
              y - 1, 
              x + nOverlayWidth - dwAlignBoundaryDest, 
              y + nOverlayHeight - 1 },

            // Upper Right
            { x - dwAlignBoundaryDest, 
              1,
              x + nOverlayWidth - dwAlignBoundaryDest, 
              1 + nOverlayHeight },

            // Lower Left
            { dwAlignBoundaryDest, 
              y - 1, 
              dwAlignBoundaryDest + nOverlayWidth, 
              y + nOverlayHeight - 1 },

            // Upper Left
            { dwAlignBoundaryDest, 
              1, 
              dwAlignBoundaryDest + nOverlayWidth, 
              1 + nOverlayHeight }
        };

        rcSrc.left=0;
        rcSrc.right=nOverlayWidth;
        rcSrc.top=0;
        rcSrc.bottom=nOverlayHeight;
        for (int iPos = 0; iPos < sizeof(rcRect)/sizeof(*rcRect); iPos++)
        {
            dwBltFlags = ((DDOVER_KEYDEST & dwAvailBltFlags) && rand()%2) ? DDOVER_KEYDEST : dwAvailBltFlags & ~DDOVER_KEYDEST;
            dbgout << "Testing overlay at position (top, left, bottom, right) (" << rcSrc.top << "," << rcSrc.left << "," << rcSrc.bottom << "," << rcSrc.right << ")" << endl;
            hr=m_piDDSDst->UpdateOverlay(&rcSrc, m_piDDS.AsInParam(), &rcRect[iPos], DDOVER_SHOW | dwBltFlags, NULL);
            QueryForHRESULT(hr, hrExpected, "Failure code returned from UpdateOverlay with wait and NULL BltFx", tr|=trFail);
            if(dwBltFlags & DDOVER_KEYSRC)
                _sntprintf(szInstructions, cdwInstructionSize, TEXT("Overlay should appear as a checkerboard, with every other square transparent."));
            else if(dwBltFlags & DDOVER_KEYDEST)
                _sntprintf(szInstructions, cdwInstructionSize, TEXT("Overlay should appear where the primary is purple, but not where it is black."));
            else
                _sntprintf(szInstructions, cdwInstructionSize, TEXT("Overlay should appear as a checkerboard, with no transparency."));
            tr|=VerifyOutput(bInteractive, bDelayed, m_piDDS.AsInParam(), szInstructions, &rcRect[iPos]);
            if (tr & trAbort)
                return tr;
        }

        for (DWORD j = 0; j < 16 && j * dwAlignSizeSrc < (DWORD) nOverlayWidth; j++)
        {
            dbgout << "Src width: " << rcSrc.right - rcSrc.left << endl;
            dwBltFlags = ((DDOVER_KEYDEST & dwAvailBltFlags) && rand()%2) ? DDOVER_KEYDEST : dwAvailBltFlags & ~DDOVER_KEYDEST;
            SetRect (&rcRect[0], 
                nDestX + j * dwAlignSizeSrc, 
                nDestY,
                nDestX + nOverlayWidth, 
                nDestY + nOverlayHeight);
            hr=m_piDDSDst->UpdateOverlay(&rcSrc, m_piDDS.AsInParam(), &rcRect[0], DDOVER_SHOW | dwBltFlags, NULL);
            QueryForHRESULT(hr, hrExpected, "Failure code returned from UpdateOverlay with wait and NULL BltFx", tr|=trFail);
            if(dwBltFlags & DDOVER_KEYSRC)
                _sntprintf(szInstructions, cdwInstructionSize, TEXT("Overlay should appear as a checkerboard, with every other square transparent."));
            else if(dwBltFlags & DDOVER_KEYDEST)
                _sntprintf(szInstructions, cdwInstructionSize, TEXT("Overlay should appear where the primary is purple, but not where it is black."));
            else
                _sntprintf(szInstructions, cdwInstructionSize, TEXT("Overlay should appear as a checkerboard, with no transparency."));
            tr|=VerifyOutput(bInteractive, bDelayed, m_piDDS.AsInParam(), szInstructions, &rcRect[0]);
            if (tr & trAbort)
                return tr;
            rcSrc.right -= dwAlignSizeSrc;
        }
        rcSrc.right = nOverlayWidth;

        if (dwHALCaps & DDCAPS_OVERLAYSTRETCH)
        {
            dbgout << "DDCAPS_OVERLAYSTRETCH is enabled, testing stretching" << endl;
            DWORD dwStretchFactor;
            DWORD dwMinOverlayStretch;
            DWORD dwMaxOverlayStretch;
            DWORD dwMinSrcWidth;
            DWORD dwMaxSrcWidth;
            DWORD dwStep;
            RECT  rcDest = { nDestX, nDestY, nDestX + nOverlayWidth, nDestY + nOverlayHeight};

            dwMinOverlayStretch = g_DDConfig.HALCaps().dwMinOverlayStretch;
            dwMaxOverlayStretch = g_DDConfig.HALCaps().dwMaxOverlayStretch;

            if (dwMaxOverlayStretch < dwMinOverlayStretch)
            {
                tr |= trFail;
                dbgout << "Inconsistent OverlayStretch capabilities: dwMinOverlayStretch = " << dwMinOverlayStretch
                       << "; dwMaxOverlayStretch = " << dwMaxOverlayStretch << endl;
                dbgout << "Capablities must fulfill the following requirement: dwMinOverlayStretch <= dwMaxOverlayStretch"
                       << endl;                
            }

            if (dwMaxOverlayStretch < 1000 || dwMinOverlayStretch > 1000)
            {
                dbgout << "WARNING: The valid range for overlay stretching does not include 1000 (no stretching)" << endl;
                dbgout << "       : While this is valid, it might be an indication of a mistake in the settings" << endl;
                dbgout << "       : Please check the settings for dwMinOverlayStretch and dwMaxOverlayStretch" << endl;
            }

            // determine the min and max width of our src rect within the limits of the stretching variables
            dwMinSrcWidth = nOverlayWidth*1000 / dwMaxOverlayStretch;
            dwMinSrcWidth += dwAlignSizeSrc - dwMinSrcWidth % dwAlignSizeSrc;
            while ((nOverlayWidth * 1000 / dwMinSrcWidth) > dwMaxOverlayStretch)
                dwMinSrcWidth += dwAlignSizeSrc;

            dwMaxSrcWidth = nOverlayWidth*1000 / dwMinOverlayStretch;
            dwMaxSrcWidth -= dwMaxSrcWidth % dwAlignSizeSrc;
            while ((nOverlayWidth * 1000 / dwMaxSrcWidth) < dwMinOverlayStretch)
                dwMaxSrcWidth -= dwAlignSizeSrc;

            // make sure we still have a valid src rect range (can't be larger than the actual overlay size
            if (dwMaxSrcWidth > cddsdSrc.dwWidth)
                dwMaxSrcWidth = cddsdSrc.dwWidth;
                
            // determine an appropriate step size
            dwStep = (dwMaxSrcWidth - dwMinSrcWidth) / 10;
            dbgout << "Using step size " << dwStep << endl;

            for (rcSrc.right = dwMinSrcWidth; rcSrc.right <= (int) dwMaxSrcWidth; rcSrc.right += dwStep)
            {
                rcSrc.bottom = rcSrc.right*nOverlayHeight/nOverlayWidth;
                dwStretchFactor = nOverlayWidth * 1000 / rcSrc.right;
                
                dbgout << "Stretch factor: " << dwStretchFactor << endl;
                dbgout << "Source rectangle: " << rcSrc << endl;
                dbgout << "Destination rectangle: " << rcDest;
                dwBltFlags = ((DDOVER_KEYDEST & dwAvailBltFlags) && rand()%2) ? DDOVER_KEYDEST : dwAvailBltFlags & ~DDOVER_KEYDEST;
                hr = m_piDDSDst->UpdateOverlay(&rcSrc, m_piDDS.AsInParam(), &rcDest, DDOVER_SHOW | dwBltFlags, NULL);
                QueryForHRESULT(hr, hrExpected, "Failure code returned from UpdateOverlay with wait, NULL BltFx, and stretch", tr|=trFail);
                if(dwBltFlags & DDOVER_KEYSRC)
                    _sntprintf(szInstructions, cdwInstructionSize, TEXT("Overlay should appear as a stretched checkerboard, with every other square transparent."));
                else if(dwBltFlags & DDOVER_KEYDEST)
                    _sntprintf(szInstructions, cdwInstructionSize, TEXT("Overlay should appear (stretched) where the primary is purple, but not where it is black."));
                else
                    _sntprintf(szInstructions, cdwInstructionSize, TEXT("Overlay should appear as a stretched checkerboard, with no transparency."));
                tr|=VerifyOutput(bInteractive, bDelayed, m_piDDS.AsInParam(), szInstructions, &rcDest);
                if (tr & trAbort)
                    return tr;
            } 
        }
        
        if(tr==trPass)
            dbgout << "Test PASSED" << endl;
        
        return tr;
    }

    eTestResult CTest_InteractiveBlt::TestIDirectDrawSurface_TWO()
    {
        using namespace DDrawUty::Surface_Helpers;
        
        eTestResult tr = trPass;
        HRESULT hr;
        DWORD dwTmp;
        CDDSurfaceDesc cddsdSrc, cddsdDst;
        hr = m_piDDS->GetSurfaceDesc(&cddsdSrc);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
        hr = m_piDDSDst->GetSurfaceDesc(&cddsdDst);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
            
        // verify the structures we need are filled
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_CAPS), "CAPS not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_PIXELFORMAT), "Pixel Format not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_HEIGHT), "Height not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_WIDTH), "Width not filled", trAbort);

        // verifiy that the pixel formats we need are set
        if(!(cddsdSrc.ddpfPixelFormat.dwFlags & DDPF_RGB))
        {
            dbgout << "test not capable of non-rgb pixel format tests" << endl;
            return trSkip;
        }
        if(!(cddsdDst.ddpfPixelFormat.dwFlags & DDPF_RGB) && !(cddsdDst.ddpfPixelFormat.dwFlags & DDPF_FOURCC))
        {
            dbgout << "test only supports rgb and some fourcc formats on overlays" << endl;
            return trSkip;
        }

        bool bInteractive=FALSE, 
             bDelayed=FALSE;
        // determine if we're interactive, delayed, or normal
        DDrawUty::g_DDConfig.GetSymbol(TEXT("i"), &dwTmp);
        if(dwTmp != 0)
            bInteractive=TRUE;
        DDrawUty::g_DDConfig.GetSymbol(TEXT("d"), &dwTmp);
        if(dwTmp != 0)
            bDelayed=TRUE;

        // if this is an offscreen surface, do nothing
        if (cddsdDst.ddsCaps.dwCaps & DDSCAPS_OFFSCREENPLAIN && (bDelayed || bInteractive))
        {
            dbgout << "Colorfilling cannot be verified on an offscreen surface -- Skipping Test" << endl;
            return trPass;
        }

        DWORD  dwHALBltCaps;
        DWORD  dwHALFXCaps;
        DWORD  dwHALCKeyCaps;
        DWORD  dwHELBltCaps;
        DWORD  dwHELFXCaps;
        DWORD  dwHELCKeyCaps;
        TCHAR szInstructions[256];
        
        GetBltCaps(cddsdSrc, cddsdDst, GBC_HAL, &dwHALBltCaps, &dwHALFXCaps, &dwHALCKeyCaps);
        GetBltCaps(cddsdSrc, cddsdDst, GBC_HEL, &dwHELBltCaps, &dwHELFXCaps, &dwHELCKeyCaps);

        if(dwHALBltCaps & DDCAPS_BLTSTRETCH || dwHELBltCaps & DDCAPS_BLTSTRETCH)
        {
            RECT rcSrcRect,
                 rcDstRect;
            RECT rc;

            if ((cddsdDst.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
                (g_DDConfig.CooperativeLevel() & DDSCL_NORMAL))
                GetWindowRect(g_DirectDraw.m_hWnd, &rc);
            else{
            rc.left=0;
            rc.right=cddsdDst.dwWidth;
            rc.top=0;
            rc.bottom=cddsdDst.dwHeight;
            }
            
            rcSrcRect.top=rc.top;
            rcSrcRect.left=rc.left;
            rcSrcRect.bottom=rc.bottom;
            rcSrcRect.right=rc.right;

            rcDstRect.top=rc.top/2;
            rcDstRect.left=rc.left/2;
            rcDstRect.bottom=rc.bottom/2;
            rcDstRect.right=rc.right/2;
            
            hr = m_piDDSDst->Blt(&rcDstRect, m_piDDS.AsInParam(), &rcSrcRect, DDBLT_WAIT, NULL);
            wsprintf(szInstructions, TEXT("There are smaller vertical lines over larger horizontal lines?"));
            tr|=VerifyOutput(bInteractive, bDelayed, m_piDDSDst.AsInParam(), szInstructions);
            if (tr & trAbort)
                return tr;
        }
        else dbgout << "Stretch blt's not supported, test portion skipped" << endl;

 
        if(dwHALBltCaps & DDCAPS_BLTCOLORFILL || dwHELBltCaps & DDCAPS_BLTCOLORFILL)
        {
            // prepare for color fill test verification
            CDDrawSurfaceVerify cddsvSurfaceVerify;

            tr|=cddsvSurfaceVerify.PreVerifyColorFill(m_piDDSDst.AsInParam());

            tr |= SetColorVal(1,0,0,m_piDDSDst.AsInParam(), &dwTmp);
            tr |=cddsvSurfaceVerify.VerifyColorFill(dwTmp);
            wsprintf(szInstructions, TEXT("The primary is red."));
            tr|=VerifyOutput(bInteractive, bDelayed, m_piDDSDst.AsInParam(), szInstructions);
            if (tr & trAbort)
                return tr;

            tr |= SetColorVal(0,1,0,m_piDDSDst.AsInParam(), &dwTmp);
            tr |=cddsvSurfaceVerify.VerifyColorFill(dwTmp);
            wsprintf(szInstructions, TEXT("The primary is green."));
            tr|=VerifyOutput(bInteractive, bDelayed, m_piDDSDst.AsInParam(), szInstructions);
            if (tr & trAbort)
                return tr;

            tr |= SetColorVal(0,0,1,m_piDDSDst.AsInParam(), &dwTmp);
            tr |=cddsvSurfaceVerify.VerifyColorFill(dwTmp);
            wsprintf(szInstructions, TEXT("The primary is blue."));
            tr|=VerifyOutput(bInteractive, bDelayed, m_piDDSDst.AsInParam(), szInstructions);
            if (tr & trAbort)
                return tr;
        }
        else dbgout << "Hardware color filling not supported, test portion skipped" << endl;
        
        return tr;
        
    }
};
