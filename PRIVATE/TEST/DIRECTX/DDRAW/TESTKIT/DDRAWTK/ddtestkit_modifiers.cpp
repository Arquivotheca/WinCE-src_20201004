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

#include "DDrawUty.h"

#include "DDTestKit_Modifiers.h"

using namespace com_utilities;
using namespace DebugStreamUty;
using namespace DDrawUty;

namespace TestKit_Surface_Modifiers
{
    namespace Surface_Fill_Helpers
    {
        DWORD SetColorVal(double dwRed, double dwGreen,double dwBlue, IDirectDrawSurface *piDDS)
        {
            CDDSurfaceDesc cddsd;
            HRESULT hr;
            DWORD Y, U, V;
            
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
                    return (((V<< 24)&0xFF000000)|((Y<< 16)&0x00FF0000)|((U<< 8)&0x0000FF00)|(Y&0x000000FF));
                else if(cddsd.ddpfPixelFormat.dwFourCC==MAKEFOURCC('V', 'Y', 'U', 'Y'))
                    return(((Y<< 24)&0xFF000000)|((U<< 16)&0x00FF0000)|((Y<< 8)&0x0000FF00)|(V&0x000000FF));
                else if(cddsd.ddpfPixelFormat.dwFourCC==MAKEFOURCC('U', 'Y', 'V', 'Y'))
                    return(((Y<< 24)&0xFF000000)|((V<< 16)&0x00FF0000)|((Y<< 8)&0x0000FF00)|(U&0x000000FF));
                else
                    dbgout << "Unknown FourCC format " << cddsd.ddpfPixelFormat.dwFourCC << endl;
            }
            else if(cddsd.ddpfPixelFormat.dwFlags & DDPF_RGB)
            {
                return(((DWORD)(cddsd.ddpfPixelFormat.dwRBitMask * dwRed) & cddsd.ddpfPixelFormat.dwRBitMask)+
                                                ((DWORD)(cddsd.ddpfPixelFormat.dwGBitMask * dwGreen) & cddsd.ddpfPixelFormat.dwGBitMask)+
                                                ((DWORD)(cddsd.ddpfPixelFormat.dwBBitMask * dwBlue) & cddsd.ddpfPixelFormat.dwBBitMask));
            }
            else dbgout << "Unknown pixel format, ddpf.dwflags is " << cddsd.ddpfPixelFormat.dwFlags << endl;

            return 0x0;
        }

        eTestResult FillSurfaceHorizontal(IDirectDrawSurface *piDDS, DWORD dwColor1, DWORD dwColor2)
        {
            const int nRectHeight = 10;
            
            eTestResult tr = trPass;
            CDDSurfaceDesc cddsd;
            HRESULT hr=DD_OK;

            hr = piDDS->GetSurfaceDesc(&cddsd);
            CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
            
            if ((cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
                (g_DDConfig.CooperativeLevel() & DDSCL_NORMAL))
            {
                // Special case the primary in windowed mode, to perform
                // our own clipping
                RECT rc;
                GetWindowRect(g_DirectDraw.m_hWnd, &rc);
                //tr |= SimpleFillSurface(m_piDDS.AsInParam(), rc.right-rc.left, rc.bottom-rc.top);
                cddsd.dwWidth = rc.right - rc.left;
                cddsd.dwHeight = rc.bottom - rc.top;
            }

            // Fill the surface with horizontal rectangles of alternating colors
            if(!(cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC))
            {
                RECT rc = { 0, 0, cddsd.dwWidth, nRectHeight };
                bool fKey = true;
                while (rc.bottom <= (LONG)cddsd.dwHeight)
                {
                    CDDBltFX cddbltfx;
                    
                    cddbltfx.dwFillColor = fKey ? dwColor1 : dwColor2;
                    hr = piDDS->Blt(&rc, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &cddbltfx);
                    CheckHRESULT(hr, "ColorFill Blt", trAbort);

                    // Shift the rect down, and toggle its color
                    rc.top += nRectHeight;
                    rc.bottom += nRectHeight;
                    fKey = !fKey;
                }
            }
            else
            {
                RECT rc = { 0, 0, cddsd.dwWidth/2, nRectHeight };
                bool fKey = true;
                DWORD dwFillColor;
                DWORD *dwSrc;

                if(FAILED(piDDS->Lock(NULL, &cddsd, DDLOCK_WAIT, NULL)))
                {
                        dbgout << "failure locking Destination surface for manual fill" << endl;
                        return trFail;
                }
                dwSrc=(DWORD*)cddsd.lpSurface;
                DWORD dwLSize=cddsd.lPitch/4;
                while (rc.bottom <= (LONG)cddsd.dwHeight)
                {
                    dwFillColor = fKey ? dwColor1 : dwColor2;
                    for(int y=rc.top;y<(int)rc.bottom;y++)
                    {
                        for(int x=rc.left;x<(int)rc.right;x++)
                        {
                            dwSrc[dwLSize*y+x]=dwFillColor;
                        }
                    }
                    // Shift the rect down, and toggle its color
                    rc.top += nRectHeight;
                    rc.bottom += nRectHeight;
                    fKey = !fKey;
                }
                piDDS->Unlock(NULL);
            }
            return tr;
        }
        
        eTestResult FillSurfaceVertical(IDirectDrawSurface *piDDS, DWORD dwColor1, DWORD dwColor2)
        {
            const int nRectWidth = 10;
            
            eTestResult tr = trPass;
            CDDSurfaceDesc cddsd;
            HRESULT hr=DD_OK;

            hr = piDDS->GetSurfaceDesc(&cddsd);
            CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
            
            if ((cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
                (g_DDConfig.CooperativeLevel() & DDSCL_NORMAL))
            {
                // Special case the primary in windowed mode, to perform
                // our own clipping
                RECT rc;
                GetWindowRect(g_DirectDraw.m_hWnd, &rc);
                //tr |= SimpleFillSurface(m_piDDS.AsInParam(), rc.right-rc.left, rc.bottom-rc.top);
                cddsd.dwWidth = rc.right - rc.left;
                cddsd.dwHeight = rc.bottom - rc.top;
            }

            
            if(!(cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC))
            {
                // Fill the surface with horizontal rectangles of alternating colors
                RECT rc = { 0, 0, nRectWidth, cddsd.dwHeight };
                bool fKey = true;
                while (rc.right <= (LONG)cddsd.dwWidth)
                {
                    CDDBltFX cddbltfx;
                    
                    cddbltfx.dwFillColor = fKey ? dwColor1 : dwColor2;
                    hr = piDDS->Blt(&rc, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &cddbltfx);
                    CheckHRESULT(hr, "ColorFill Blt", trAbort);

                    // Shift the rect down, and toggle its color
                    rc.left += nRectWidth;
                    rc.right += nRectWidth;
                    fKey = !fKey;
                }
            }
            else
            {
                RECT rc = { 0, 0, nRectWidth/2, cddsd.dwHeight };
                bool fKey = true;
                DWORD dwFillColor;
                DWORD *dwSrc;

                if(FAILED(piDDS->Lock(NULL, &cddsd, DDLOCK_WAIT, NULL)))
                {
                        dbgout << "failure locking Destination surface for manual fill" << endl;
                        return trFail;
                }
                dwSrc=(DWORD*)cddsd.lpSurface;
                DWORD dwLSize=cddsd.lPitch/4;
                while (rc.right <= (LONG)cddsd.dwWidth)
                {
                    dwFillColor = fKey ? dwColor1 : dwColor2;
                    for(int y=rc.top;y<(int)rc.bottom;y++)
                    {
                        for(int x=rc.left;x<(int)rc.right;x++)
                        {
                            dwSrc[dwLSize*y+x]=dwFillColor;
                        }
                    }
                    // Shift the rect down, and toggle its color
                    rc.left += nRectWidth;
                    rc.right += nRectWidth;
                    fKey = !fKey;
                }
                piDDS->Unlock(NULL);
            }
            return tr;
        }

        eTestResult FillSurfaceCombo(IDirectDrawSurface *piDDS, DWORD dwColor1, DWORD dwColor2)
        {
            const int nRectSize = 10;
            
            eTestResult tr = trPass;
            CDDSurfaceDesc cddsd;
            HRESULT hr=DD_OK;
            CDDBltFX cddbltfx;
            bool fBlt = true;

            hr = piDDS->GetSurfaceDesc(&cddsd);
            CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
            
            if ((cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
                (g_DDConfig.CooperativeLevel() & DDSCL_NORMAL))
            {
                // Special case the primary in windowed mode, to perform
                // our own clipping
                RECT rc;
                GetWindowRect(g_DirectDraw.m_hWnd, &rc);
                //tr |= SimpleFillSurface(m_piDDS.AsInParam(), rc.right-rc.left, rc.bottom-rc.top);
                cddsd.dwWidth = rc.right - rc.left;
                cddsd.dwHeight = rc.bottom - rc.top;
            }

            // Make sure this surface can be blted to. With some drivers, overlays
            // are in a different format than the primary, and the blting
            // hardware does not work properly.
            cddbltfx.dwFillColor = dwColor1;
            RECT rcTest = {0, 0, 5, 5};
            if(DDERR_UNSUPPORTED == piDDS->Blt(&rcTest, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &cddbltfx)
               ||
               cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC)
            {
                dbgout << "Blting not supported with this surface, using Lock"
                       << endl;
                fBlt = false;
            }

            // Fill the surface with horizontal rectangles of alternating colors
            if(fBlt)
            {
                RECT rc = { 0, 0, cddsd.dwWidth / 2, nRectSize };
                bool fKey = true;
                while (rc.bottom <= (LONG)cddsd.dwHeight)
                {
                    RECT rc2 = {0, rc.top, nRectSize, rc.bottom};
                    while (rc2.right <= (LONG)cddsd.dwWidth)
                    {
                        cddbltfx.dwFillColor = fKey ? dwColor1 : dwColor2;
                        hr = piDDS->Blt(&rc2, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &cddbltfx);
                        CheckHRESULT(hr, "ColorFill Blt", trAbort);

                        // Shift the rect right and toggle its color
                        rc2.left += nRectSize;
                        rc2.right += nRectSize;
                        fKey = !fKey;
                    }

                    // Shift the rect down and toggle its color
                    rc.top += nRectSize;
                    rc.bottom += nRectSize;
                    fKey = !fKey;
                }
            }
            else
            {
                RECT rc;
                bool fKey = true;
                DWORD dwFillColor;
                BYTE *rbSrc;
                WORD *rwSrc;
                DWORD *rdwSrc;
                DWORD dwScale = 1;
                // Since dwRGBBitCount and dwYUVBitCount are in the same union,
                // they are interchangeable.
                DWORD dwBpp = cddsd.ddpfPixelFormat.dwRGBBitCount / 8;
                if(!dwBpp)
                    dwBpp = 1;
                if(3 == dwBpp)
                    dwBpp = 4;

                if(cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC)
                {
                    // For FOURCC pixel formats (especially YUV formats), 
                    // it generally takes two pixels to define the actual color.
                    dwBpp *= 2;
                    dwScale = 2;
                }

                dbgout << "Bytes per pixel: " << dwBpp << endl;
                dbgout << "Fill colors: " << HEX(dwColor1) 
                       << " and " << HEX(dwColor2) << endl;

                if(FAILED(piDDS->Lock(NULL, &cddsd, DDLOCK_WAIT, NULL)))
                {
                        dbgout << "failure locking Destination surface for manual fill" << endl;
                        return trFail;
                }
                SetRect(&rc, 0, 0, cddsd.dwWidth, nRectSize);

                // Get BYTE, WORD, and DWORD pointers to the surface, for
                // the different pixel sizes.
                rbSrc=(BYTE*)cddsd.lpSurface;
                rwSrc=(WORD*)cddsd.lpSurface;
                rdwSrc=(DWORD*)cddsd.lpSurface;

                // Determine the number of pixels in the stride of the surface.
                DWORD dwLSize=cddsd.lPitch/dwBpp;
                while (rc.bottom <= (LONG)cddsd.dwHeight)
                {
                    RECT rc2 = {0, rc.top, nRectSize/dwScale, rc.bottom};
                    while (rc2.right <= (LONG)cddsd.dwWidth)
                    {
                        dwFillColor = fKey ? dwColor1 : dwColor2;
                        for(int y=rc2.top;y<(int)rc2.bottom;y++)
                        {
                            for(int x=rc2.left;x<(int)rc2.right;x++)
                            {
                                switch(dwBpp)
                                {
                                case 1:
                                    rbSrc[dwLSize*y+x] = (BYTE)dwFillColor;
                                    break;
                                case 2:
                                    rwSrc[dwLSize*y+x] = (WORD)dwFillColor;
                                    break;
                                case 4:
                                    rdwSrc[dwLSize*y+x] = dwFillColor;
                                    break;
                                default:
                                    dbgout << "Unable to fill surface: " 
                                           << dwBpp 
                                           << " Bytes/pixel is not supported by this tool" 
                                           << endl;
                                    return trAbort;
                                }
                            }
                        }
                        // Shift the rect right and toggle its color
                        rc2.left += nRectSize/dwScale;
                        rc2.right += nRectSize/dwScale;
                        fKey = !fKey;
                    }
                    
                    // Shift the rect down and toggle its color
                    rc.top += nRectSize;
                    rc.bottom += nRectSize;
                    fKey = !fKey;
                }

                piDDS->Unlock(NULL);

            }
            return tr;
        }

    };
   
    eTestResult CTestKit_Fill::ConfigDirectDrawSurface()
    {
        using namespace Surface_Fill_Helpers;

        const DWORD dwColor1= SetColorVal(1, 1,0, m_piDDS.AsInParam()),
                                dwColor2 = SetColorVal(0, 0,.5, m_piDDS.AsInParam());
        
        eTestResult tr = trPass;

        tr |= FillSurfaceVertical(m_piDDS.AsInParam(), dwColor1, dwColor2);

        return tr;
    }
    
    eTestResult CTestKit_Fill::ConfigDirectDrawSurface_TWO()
    {
        using namespace Surface_Fill_Helpers;
        using namespace Surface_Helpers;
        
        const DWORD dwColor1= SetColorVal(0, 1,0, m_piDDSDst.AsInParam()),
                                dwColor2 = SetColorVal(0, 1,1, m_piDDSDst.AsInParam());
        
        eTestResult tr = trPass;

        tr |= FillSurfaceHorizontal(m_piDDSDst.AsInParam(), dwColor1, dwColor2);

        return tr;
    }
    
    eTestResult CTestKit_ColorKey::ConfigDirectDrawSurface()
    {
 
        using namespace Surface_Fill_Helpers;
        using namespace Surface_Helpers;
        HRESULT hr=DD_OK;
        
        const DWORD dwColor1 = SetColorVal(0, 0,1, m_piDDS.AsInParam()),
                   dwColor2 = SetColorVal(0, 1,0, m_piDDS.AsInParam());
        std::vector<DWORD> vectdwSrcFlags;
        eTestResult tr = trPass;
        DDCOLORKEY ddck;
         tr |= FillSurfaceVertical(m_piDDS.AsInParam(), dwColor1, dwColor2);
        ddck.dwColorSpaceLowValue = ddck.dwColorSpaceHighValue = dwColor1;
        dbgout  << "Setting Source Color Key to " << ddck << endl;
        hr = m_piDDS->SetColorKey(DDCKEY_SRCBLT, &ddck);
        CheckHRESULT(hr, "SetColorKey", trAbort);

        return tr;
    }
    
    eTestResult CTestKit_ColorKey::ConfigDirectDrawSurface_TWO()
    {
         using namespace Surface_Fill_Helpers;
         using namespace Surface_Helpers;

        const DWORD dwColor1 = SetColorVal(1, 0,0, m_piDDSDst.AsInParam()),
                                dwColor2 = SetColorVal(1, 1,0, m_piDDSDst.AsInParam());

         std::vector<DWORD> vectdwDstFlags;
         eTestResult tr = trPass;

         tr |= FillSurfaceHorizontal(m_piDDSDst.AsInParam(), dwColor1, dwColor2);
         return tr;
    }

    eTestResult CTestKit_Overlay::ConfigDirectDrawSurface()
    {
       using namespace Surface_Fill_Helpers;

        const DWORD dwColor1 = SetColorVal(1, 1,1, m_piDDS.AsInParam()),
                                dwColor2 = SetColorVal(0, 0,0, m_piDDS.AsInParam());
        eTestResult tr = trPass;

        tr |= FillSurfaceVertical(m_piDDS.AsInParam(), dwColor1, dwColor2);

        return tr;
    }
    eTestResult CTestKit_Overlay::ConfigDirectDrawSurface_TWO()
    {
         using namespace Surface_Fill_Helpers;
        using namespace Surface_Helpers;
        
         const DWORD dwColor1 = SetColorVal(1, 1,0, m_piDDSDst.AsInParam()),
                                dwColor2 = SetColorVal(0, 0,1, m_piDDSDst.AsInParam());
        
        std::vector<DWORD> vectdwDstFlags;
        eTestResult tr = trPass;
        
        tr |= FillSurfaceCombo(m_piDDSDst.AsInParam(), dwColor1, dwColor2);
        
        return tr;

    }
    
    eTestResult CTestKit_ColorKeyOverlay::ConfigDirectDrawSurface()
    {
        using namespace Surface_Fill_Helpers;
        using namespace Surface_Helpers;

        const DWORD dwColor1 = SetColorVal(0.5, 0,1, m_piDDS.AsInParam()),
                   dwColor2 = SetColorVal(0, 0,0, m_piDDS.AsInParam());
        eTestResult tr = trPass;

         tr |= FillSurfaceVertical(m_piDDS.AsInParam(), dwColor1, dwColor2);

        return tr;

    }

    eTestResult CTestKit_ColorKeyOverlay::ConfigDirectDrawSurface_TWO()
    {
        using namespace Surface_Fill_Helpers;
        using namespace Surface_Helpers;
        
        DDCOLORKEY ddck, ddckDest;
        HRESULT hr=DD_OK;
        CDDSurfaceDesc cddsd;   
        
        DWORD dwColor1 = SetColorVal(0, 0.5, 1, m_piDDSDst.AsInParam()),
              dwColor2 = SetColorVal(0, 1,1, m_piDDSDst.AsInParam());

        const DWORD dwColorDest1 = SetColorVal(0.5, 0,1, m_piDDS.AsInParam());

        std::vector<DWORD> vectdwDstFlags;
        eTestResult tr = trPass;
        
        hr = m_piDDSDst->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
        
        if (!(cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY))
        {
            dbgout << "Destination is not an overlay surface-- Skipping Test" << endl;
            return trPass;
        }
        
        if((cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC))
        {
            // It is very difficult to get the correct RGB value
            // for a specific YUV value, except for black.
            ddck.dwColorSpaceLowValue = ddck.dwColorSpaceHighValue = SetColorVal(0, 0, 0, m_piDDS.AsInParam());
            dwColor1 = SetColorVal(0, 0, 0, m_piDDSDst.AsInParam());
        }
        else
        {
            ddck.dwColorSpaceLowValue = ddck.dwColorSpaceHighValue = dwColor1;
        }
        ddckDest.dwColorSpaceLowValue = ddckDest.dwColorSpaceHighValue = dwColorDest1;

        tr |= FillSurfaceCombo(m_piDDSDst.AsInParam(), dwColor1, dwColor2);

        dbgout  << "Setting Source Color Key to " << ddck << endl;
        hr = m_piDDSDst->SetColorKey(DDCKEY_SRCOVERLAY, &ddck);
        CheckHRESULT(hr, "SetColorKey", trAbort);

        // Only use the DESTOVERLAY colorkey if we are in fullscreen. Otherwise, the overlay would not be
        // visible if it were not on top of the primary window.
        if (g_DDConfig.CooperativeLevel() & DDSCL_FULLSCREEN && g_DDConfig.HALCaps().dwCKeyCaps & DDCKEYCAPS_DESTOVERLAY)
        {
            dbgout  << "Setting Dest Color Key to " << ddckDest << endl;
            hr = m_piDDS->SetColorKey(DDCKEY_DESTOVERLAY, &ddckDest);
            CheckHRESULT(hr, "SetColorKey", trAbort);
        }
        
        return tr;
    }
};



