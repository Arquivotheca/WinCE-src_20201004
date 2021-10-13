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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
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
        DWORD PackYUV(DWORD dwFourCC, DWORD Y1, DWORD Y2, DWORD U, DWORD V)
        {
            switch(dwFourCC)
            {
            case MAKEFOURCC('Y','U','Y','V'):
            case MAKEFOURCC('Y','U','Y','2'):
                return (((V<< 24)&0xFF000000)|((Y1<< 16)&0x00FF0000)|((U<< 8)&0x0000FF00)|(Y2&0x000000FF));
            case MAKEFOURCC('V','Y','U','Y'):
                return (((Y1<< 24)&0xFF000000)|((U<< 16)&0x00FF0000)|((Y2<< 8)&0x0000FF00)|(V&0x000000FF));
            case MAKEFOURCC('U','Y','V','Y'):
                return(((Y1<< 24)&0xFF000000)|((V<< 16)&0x00FF0000)|((Y2<< 8)&0x0000FF00)|(U&0x000000FF));
            default:
                return(((Y1<< 24)&0xFF000000)|((Y2<< 16)&0x00FF0000)|((U<< 8)&0x0000FF00)|(V&0x000000FF));
            }
        }

        void UnpackYUV(DWORD dwFourCC, DWORD dwColor, DWORD * pY1, DWORD * pY2, DWORD * pU, DWORD * pV)
        {
            switch(dwFourCC)
            {
            case MAKEFOURCC('Y','U','Y','V'):
            case MAKEFOURCC('Y','U','Y','2'):
                *pV = (dwColor&0xFF000000)>> 24;
                *pY1 = (dwColor&0x00FF0000)>> 16;
                *pU = (dwColor&0x0000FF00)>> 8;
                *pY2 = (dwColor&0x000000FF);
                break;

            case MAKEFOURCC('V','Y','U','Y'):
                *pY1 = (dwColor&0xFF000000)>> 24;
                *pU = (dwColor&0x00FF0000)>> 16;
                *pY2 = (dwColor&0x0000FF00)>> 8;
                *pV = (dwColor&0x000000FF);
                break;

            case MAKEFOURCC('U','Y','V','Y'):
                *pY1 = (dwColor&0xFF000000)>> 24;
                *pV = (dwColor&0x00FF0000)>> 16;
                *pY2 = (dwColor&0x0000FF00)>> 8;
                *pU = (dwColor&0x000000FF);
                break;

            default:
                *pY1 = (dwColor&0xFF000000)>> 24;
                *pY2 = (dwColor&0x00FF0000)>> 16;
                *pU = (dwColor&0x0000FF00)>> 8;
                *pV = (dwColor&0x000000FF);
                break;        
            }
            return;
        }
        
        BOOL IsPlanarFourCC(DWORD dwFourCC)
        {
            switch(dwFourCC)
            {
            case MAKEFOURCC('Y','V','1','2'):
            case MAKEFOURCC('N','V','1','2'):
            case MAKEFOURCC('I','4','2','0'):
                return TRUE;
            default:
                return FALSE;
            }
        }

        HRESULT SetPlanarPixel(const CDDSurfaceDesc * pcddsd, int iX, int iY, DWORD dwColor)
        {
            DWORD dwY1, dwY2, dwU, dwV;

            if (!(pcddsd->ddpfPixelFormat.dwFlags & DDPF_FOURCC))
            {
                return E_INVALIDARG;
            }

            UnpackYUV(pcddsd->ddpfPixelFormat.dwFourCC, dwColor, &dwY1, &dwY2, &dwU, &dwV);

            // These will determine where the specific pixel values go.
            BYTE * pYOffset = NULL;
            BYTE * pUOffset = NULL;
            BYTE * pVOffset = NULL;

            // These will be used to determine what the offsets should be.
            BYTE * pYPlane = NULL;
            BYTE * pUPlane = NULL;
            BYTE * pVPlane = NULL;

            DWORD dwWidth = pcddsd->dwWidth;
            DWORD dwHeight = pcddsd->dwHeight;
            DWORD dwYPlaneSize;
            DWORD dwUPlaneSize;
            DWORD dwVPlaneSize;

            switch(pcddsd->ddpfPixelFormat.dwFourCC)
            {
            case MAKEFOURCC('Y','V','1','2'):
                // 8bpp NxM Y plane
                // 8bpSample (N/2)x(M/2) V plane
                // 8bpSample (N/2)x(M/2) U plane
                dwYPlaneSize = dwWidth * dwHeight;
                dwUPlaneSize = dwYPlaneSize / 4;
                dwVPlaneSize = dwUPlaneSize;
                pYPlane = (BYTE*)pcddsd->lpSurface;
                pVPlane = pYPlane + dwYPlaneSize;
                pUPlane = pYPlane + dwYPlaneSize + dwVPlaneSize;

                pYOffset = pYPlane + iY * dwWidth + iX;
                if ((iX & 1) || (iY & 1))
                {
                    // Don't set U or V here
                    pVOffset = 0;
                    pUOffset = 0;
                }
                else
                {
                    pVOffset = pVPlane + (iY/2) * (dwWidth/2) + iX/2;
                    pUOffset = pUPlane + (iY/2) * (dwWidth/2) + iX/2;
                }
                break;
            case MAKEFOURCC('N','V','1','2'):
                // 8bpp NxM Y plane
                // 8bpsample Nx(M/2) U.V plane (U and V interleaved)
                dwYPlaneSize = dwWidth * dwHeight;
                pYPlane = (BYTE*)pcddsd->lpSurface;
                pUPlane = pYPlane + dwYPlaneSize;
                pYOffset = pYPlane + iY * dwWidth + iX;
                if (iY & 1)
                {
                    // Don't set U or V here
                    pVOffset = 0;
                    pUOffset = 0;
                }
                else
                {
                    pUOffset = pVOffset = pUPlane + (iY/2) * dwWidth + iX/2;
                    if (iX & 1)
                    {
                        pUOffset = 0;
                    }
                    else
                    {
                        pVOffset = 0;
                    }
                }
                break;
            case MAKEFOURCC('I','4','2','0'):
                // 8bpp NxM Y plane
                // 8bpSample (N/2)x(M/2) U plane
                // 8bpSample (N/2)x(M/2) V plane
                // Same as YV12 with U and V swapped
                dwYPlaneSize = dwWidth * dwHeight;
                dwUPlaneSize = dwYPlaneSize / 4;
                dwVPlaneSize = dwUPlaneSize;
                pYPlane = (BYTE*)pcddsd->lpSurface;
                pUPlane = pYPlane + dwYPlaneSize;
                pVPlane = pYPlane + dwYPlaneSize + dwUPlaneSize;

                pYOffset = pYPlane + iY * dwWidth + iX;
                if (iX & 1 || iY & 1)
                {
                    // Don't set U or V here
                    pVOffset = 0;
                    pUOffset = 0;
                }
                else
                {
                    pUOffset = pUPlane + (iY/2) * (dwWidth/2) + iX/2;
                    pVOffset = pVPlane + (iY/2) * (dwWidth/2) + iX/2;
                }
                break;
            default:
                dbgout<<"SetPlanarPixel called with unknown FOURCC"<<endl;
                return E_NOTIMPL;
            }

            // Will we use Y1 or Y2?
            *pYOffset = (BYTE)(!(iX & 1) ? dwY1 : dwY2);
            if(pVOffset)
            {
                *pVOffset = (BYTE)dwV;
            }
            if(pUOffset)
            {
                *pUOffset = (BYTE)dwU;
            }
            return S_OK;
        }
        
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
                
                return PackYUV(cddsd.ddpfPixelFormat.dwFourCC, Y, Y, U, V);
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
            HRESULT hr=S_OK;

            hr = piDDS->GetSurfaceDesc(&cddsd);
            CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
            
            if ((cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
                (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
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
                    hr = piDDS->Blt(&rc, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, &cddbltfx);
                    CheckHRESULT(hr, "ColorFill Blt", trAbort);

                    // Shift the rect down, and toggle its color
                    rc.top += nRectHeight;
                    rc.bottom += nRectHeight;
                    fKey = !fKey;
                }
            }
            else
            {
                RECT rc = { 0, 0, cddsd.dwWidth, nRectHeight };
                bool fKey = true;
                DWORD dwFillColor;
                DWORD *dwSrc;

                if(FAILED(piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL)))
                {
                        dbgout << "failure locking Destination surface for manual fill" << endl;
                        return trFail;
                }
                dwSrc=(DWORD*)cddsd.lpSurface;
                while (rc.bottom <= (LONG)cddsd.dwHeight)
                {
                    dwFillColor = fKey ? dwColor1 : dwColor2;
                    for(int y=rc.top;y<(int)rc.bottom;y++)
                    {
                        for(int x=rc.left;x<(int)rc.right;x += 2)
                        {
                            if (!IsPlanarFourCC(cddsd.ddpfPixelFormat.dwFourCC))
                            {
                                ((WORD*)((BYTE*)dwSrc + cddsd.lPitch*y + cddsd.lXPitch * x))[0]=((WORD*)&dwFillColor)[0];
                                ((WORD*)((BYTE*)dwSrc + cddsd.lPitch*y + cddsd.lXPitch * (x + 1)))[0]=((WORD*)&dwFillColor)[1];
                            }
                            else
                            {
                                SetPlanarPixel(&cddsd, x*2, y, dwFillColor);
                                SetPlanarPixel(&cddsd, x*2 + 1, y, dwFillColor);
                            }
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
            HRESULT hr=S_OK;

            hr = piDDS->GetSurfaceDesc(&cddsd);
            CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
            
            if ((cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
                (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
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
                    hr = piDDS->Blt(&rc, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, &cddbltfx);
                    CheckHRESULT(hr, "ColorFill Blt", trAbort);

                    // Shift the rect down, and toggle its color
                    rc.left += nRectWidth;
                    rc.right += nRectWidth;
                    fKey = !fKey;
                }
            }
            else
            {
                RECT rc = { 0, 0, nRectWidth, cddsd.dwHeight };
                bool fKey = true;
                DWORD dwFillColor;
                DWORD *dwSrc;

                if(FAILED(piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL)))
                {
                        dbgout << "failure locking Destination surface for manual fill" << endl;
                        return trFail;
                }
                dwSrc=(DWORD*)cddsd.lpSurface;
                while (rc.right <= (LONG)cddsd.dwWidth)
                {
                    dwFillColor = fKey ? dwColor1 : dwColor2;
                    for(int y=rc.top;y<(int)rc.bottom;y++)
                    {
                        for(int x=rc.left;x<(int)rc.right;x+=2)
                        {
                            if (!IsPlanarFourCC(cddsd.ddpfPixelFormat.dwFourCC))
                            {
                                ((WORD*)((BYTE*)dwSrc + cddsd.lPitch*y + cddsd.lXPitch * x))[0]=((WORD*)&dwFillColor)[0];
                                ((WORD*)((BYTE*)dwSrc + cddsd.lPitch*y + cddsd.lXPitch * (x + 1)))[0]=((WORD*)&dwFillColor)[1];
                            }
                            else
                            {
                                SetPlanarPixel(&cddsd, x*2, y, dwFillColor);
                                SetPlanarPixel(&cddsd, x*2 + 1, y, dwFillColor);
                            }
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
            HRESULT hr=S_OK;
            CDDBltFX cddbltfx;
            bool fBlt = true;

            hr = piDDS->GetSurfaceDesc(&cddsd);
            CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
            
            if ((cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
                (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
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
            if(DDERR_UNSUPPORTED == piDDS->Blt(&rcTest, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, &cddbltfx)
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
                        hr = piDDS->Blt(&rc2, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, &cddbltfx);
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
                bool fFourCC = false;
                DWORD dwFillColor;
                BYTE *rbSrc;
                const WORD *rwSrc;
                const DWORD *rdwSrc;
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
                    fFourCC = true;
                }

                dbgout << "Bytes per pixel: " << dwBpp << endl;
                dbgout << "Fill colors: " << HEX(dwColor1) 
                       << " and " << HEX(dwColor2) << endl;

                if(FAILED(piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL)))
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
                                if (!IsPlanarFourCC(cddsd.ddpfPixelFormat.dwFourCC))
                                {
                                    switch(dwBpp)
                                    {
                                    case 1:
                                        ((BYTE*)rbSrc + cddsd.lPitch * y + cddsd.lXPitch * x)[0] = (BYTE)dwFillColor;
                                        break;
                                    case 2:
                                        if (!fFourCC)
                                        {
                                            ((WORD*)((BYTE*)rbSrc + cddsd.lPitch * y + cddsd.lXPitch * x))[0] = (WORD)dwFillColor;
                                        }
                                        else
                                        {
                                            // If we're doing FourCC, properly handle this.
                                            ((WORD*)((BYTE*)rbSrc + cddsd.lPitch * y + cddsd.lXPitch * x))[0] = ((WORD*)&dwFillColor)[x & 1];
                                        }
                                        break;
                                    case 4:
                                        ((DWORD*)((BYTE*)rbSrc + cddsd.lPitch * y + cddsd.lXPitch * x))[0] = dwFillColor;
                                        break;
                                    default:
                                        dbgout << "Unable to fill surface: " 
                                               << dwBpp 
                                               << " Bytes/pixel is not supported by this tool" 
                                               << endl;
                                        return trAbort;
                                    }
                                }
                                else
                                {
                                    SetPlanarPixel(&cddsd, x*2, y, dwFillColor);
                                    SetPlanarPixel(&cddsd, x*2 + 1, y, dwFillColor);
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
        HRESULT hr=S_OK;
        
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
        HRESULT hr=S_OK;
        CDDSurfaceDesc cddsd; 
        BOOL fSrcOverlay;
        
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
            fSrcOverlay = g_DDConfig.HALCaps().dwOverlayCaps & DDOVERLAYCAPS_CKEYSRCCLRSPACEYUV;
        }
        else
        {
            ddck.dwColorSpaceLowValue = ddck.dwColorSpaceHighValue = dwColor1;
            fSrcOverlay = g_DDConfig.HALCaps().dwOverlayCaps & DDOVERLAYCAPS_CKEYSRC;
        }
        ddckDest.dwColorSpaceLowValue = ddckDest.dwColorSpaceHighValue = dwColorDest1;

        tr |= FillSurfaceCombo(m_piDDSDst.AsInParam(), dwColor1, dwColor2);

        if (fSrcOverlay)
        {
            dbgout  << "Setting Source Color Key to " << ddck << endl;
            hr = m_piDDSDst->SetColorKey(DDCKEY_SRCOVERLAY, &ddck);
            CheckHRESULT(hr, "SetColorKey", trAbort);
        }

        // Only use the DESTOVERLAY colorkey if we are in fullscreen. Otherwise, the overlay would not be
        // visible if it were not on top of the primary window.
        if (g_DDConfig.CooperativeLevel() & DDSCL_FULLSCREEN && g_DDConfig.HALCaps().dwOverlayCaps & DDOVERLAYCAPS_CKEYDEST)
        {
            dbgout  << "Setting Dest Color Key to " << ddckDest << endl;
            hr = m_piDDS->SetColorKey(DDCKEY_DESTOVERLAY, &ddckDest);
            CheckHRESULT(hr, "SetColorKey", trAbort);
        }
        
        return tr;
    }
};



