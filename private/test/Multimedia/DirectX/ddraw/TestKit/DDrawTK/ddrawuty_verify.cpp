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
#include "DDrawUty_Verify.h"

extern "C" void CompactAllHeaps();

using namespace DebugStreamUty;
using namespace DDrawUty;

namespace
{
     ////////////////////////////////////////////////////////////////////////////////
     // IsOverlapping
     // This function returns true or false depending on wether or not the source and dest rectangles overlap.
     //

    bool IsOverlapping(RECT src, RECT dst)
    {
        // src/dst overlap, src within dest
        if((src.top >= dst.top && src.top <= dst.bottom) || (src.bottom >= dst.top && src.bottom <=dst.bottom))
        {
            if(src.left >= dst.left && src.left <=dst.right)
                return true;
            if(src.right >=dst.left && src.right <=dst.right)
                return true;
        }

        // dst within source
        if(src.left < dst.left && src.top < dst.top && src.right > dst.right && src.bottom > dst.bottom)
            return true;

        return false;
    }
}

namespace DDrawVerifyUty
{
    ////////////////////////////////////////////////////////////////////////////////
     // CDDrawSurfaceVerify::IsClipped
     // This function tests the coordinates compared to the clip list.
     //
    bool CDDrawSurfaceVerify::IsClipped(DWORD x, DWORD y)
    {
        RECT rcRect;
        if(!m_bClipDataValid)
            return false;
        else
        {
            // check the bounding rectangle, if outside then we're clipped.
            if(x<=(DWORD)m_rgndClipData->rdh.rcBound.left || x>=(DWORD)m_rgndClipData->rdh.rcBound.right || y<=(DWORD)m_rgndClipData->rdh.rcBound.top || y>=(DWORD)m_rgndClipData->rdh.rcBound.bottom)
                    return true;
            //step through the clipping list (if any)
            for(int i=0; i<(int)m_rgndClipData->rdh.nCount; i++)
            {
                rcRect= *((RECT*)m_rgndClipData->Buffer);
                if(x<=(DWORD)rcRect.left || x>=(DWORD)rcRect.right || y<=(DWORD)rcRect.top || y>=(DWORD)rcRect.bottom)
                    return true;
            }

        }
        return false;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CDDrawSurfaceVerify::Verify (16 bit version)
    // steps through every pixel in the surface and verifies that it was copied correctly.
    // Handles source colorkeying, not dest color keying, and no scaling.
    eTestResult CDDrawSurfaceVerify::Verify(const WORD *dwsrc,
                                            const WORD *dwdest,
                                            CDDSurfaceDesc *ddsdSrc,
                                            CDDSurfaceDesc *ddsdDst)
    {
        DDCOLORKEY ddckSrc,
                                ddckDst;
        bool bSrcKey=false,
               bDstKey=false;
        eTestResult tr=trPass;
        DWORD lSrcPitch = ddsdSrc->lPitch/2;
        DWORD lSrcXPitch = ddsdSrc->lXPitch/2;
        DWORD lDstPitch = ddsdDst->lPitch/2;
        DWORD lDstXPitch = ddsdDst->lXPitch/2;
        if (SUCCEEDED(m_piDDSSrc->GetColorKey(DDCKEY_SRCBLT, &ddckSrc)))
            bSrcKey=true;
        if (SUCCEEDED(m_piDDSDst->GetColorKey(DDCKEY_DESTBLT, &ddckDst)))
            bDstKey=true;

        int y1=m_rcSrc.top;
        int y2=m_rcDst.top;
        int x1 = 0;
        int x2 = 0;
        DWORD wSRed,
                    wSGreen,
                    wSBlue;
        DWORD wDRed,
                    wDGreen,
                    wDBlue;

        for(;y1<m_rcSrc.bottom && y2 < m_rcDst.bottom && tr==trPass;y1++, y2++)
        {

            x1=m_rcSrc.left;
            x2=m_rcDst.left;
            for(;x1<(int)(m_rcSrc.right) && x2<(int)(m_rcDst.right) && tr==trPass;x1++, x2++)
            {
                // if the pixels are different and it's not clipped, then we have a problem. otherwise, we're ok.
                if(dwdest[(lDstPitch*y2)+(lDstXPitch*x2)]!=dwsrc[(lSrcPitch*y1)+(lSrcXPitch*x1)] && !IsClipped(x2,y2))
                {
                    wSRed=dwsrc[(lSrcPitch * y1) + (lSrcXPitch * x1)] & ddsdSrc->ddpfPixelFormat.dwRBitMask;
                    wSGreen=dwsrc[(lSrcPitch * y1) + (lSrcXPitch * x1)] & ddsdSrc->ddpfPixelFormat.dwGBitMask;
                    wSBlue=dwsrc[(lSrcPitch * y1) + (lSrcXPitch * x1)] & ddsdSrc->ddpfPixelFormat.dwBBitMask;
                    wDRed=dwdest[(lDstPitch * y2) + (lDstXPitch * x2)] & ddsdSrc->ddpfPixelFormat.dwRBitMask;
                    wDGreen=dwdest[(lDstPitch * y2) + (lDstXPitch * x2)] & ddsdSrc->ddpfPixelFormat.dwGBitMask;
                    wDBlue=dwdest[(lDstPitch * y2) + (lDstXPitch * x2)] & ddsdSrc->ddpfPixelFormat.dwBBitMask;

                    if(bSrcKey==true)
                    {
                        // if we're source color keyed, verify the surface
                        DWORD wSCKLRed=ddckSrc.dwColorSpaceLowValue & ddsdSrc->ddpfPixelFormat.dwRBitMask;
                        DWORD wSCKLGreen=ddckSrc.dwColorSpaceLowValue & ddsdSrc->ddpfPixelFormat.dwGBitMask;
                        DWORD wSCKLBlue=ddckSrc.dwColorSpaceLowValue & ddsdSrc->ddpfPixelFormat.dwBBitMask;
                        DWORD wSCKHRed=ddckSrc.dwColorSpaceHighValue & ddsdSrc->ddpfPixelFormat.dwRBitMask;
                        DWORD wSCKHGreen=ddckSrc.dwColorSpaceHighValue & ddsdSrc->ddpfPixelFormat.dwGBitMask;
                        DWORD wSCKHBlue=ddckSrc.dwColorSpaceHighValue & ddsdSrc->ddpfPixelFormat.dwBBitMask;
                        // if the source color is in the color key, then we're ok, if it's not in the color key, then it was supposed to be copied, so there's an error.
                        if(wSRed < wSCKLRed || wSRed > wSCKHRed ||
                            wSGreen < wSCKLGreen || wSGreen > wSCKHGreen ||
                            wSBlue < wSCKLBlue || wSBlue > wSCKHBlue)
                                tr|=trFail;
                    }
                    // it's not possible to test dest colorkeying unless we also have a backup of the destination prior to the blit!
                    else if(!(abs(wSRed-wDRed) <2 && abs(wSGreen-wDGreen) < 2 && abs(wSBlue-wDBlue) < 2) && bDstKey!=true)
                        tr|=trFail;

                }
            }
        }

        if(tr!=trPass)
        {
            // compensate for increment at end of loop.
            x1--;
            x2--;
            y1--;
            y2--;
            dbgout << "source contains data " << HEX(dwsrc[(lSrcPitch * y1) + (lSrcXPitch * x1)]) << " dest contains data " << HEX(dwdest[(lDstPitch * y2) + (lDstXPitch * x2)]) << endl;
            dbgout << "nonmatching values in the surface comparison copying from (x,y) " << x1 << "," << y1 << " to " << x2 << ","<<y2 << endl;
            dbgout << "copy from (top,left,bottom, right) (" << m_rcSrc.top << "," << m_rcSrc.left <<","<< m_rcSrc.bottom <<","<< m_rcSrc.right <<")" << endl;
            dbgout << "To (top,left,bottom, right) (" << m_rcDst.top << "," << m_rcDst.left <<","<<m_rcDst.bottom <<","<< m_rcDst.right <<")" << endl;
            dbgout << "FAILED" << endl;
            return tr;
        }
        else
        {
            dbgout << "copy from (top,left,bottom, right) (" << m_rcSrc.top << "," << m_rcSrc.left <<","<< m_rcSrc.bottom <<","<< m_rcSrc.right <<")" << endl;
            dbgout << "To (top,left,bottom, right) (" << m_rcDst.top << "," << m_rcDst.left <<","<< m_rcDst.bottom <<","<< m_rcDst.right <<")" << endl;
            dbgout << "PASSED" << endl;

            return tr;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CDDrawSurfaceVerify::Verify (32 bit version)
    // steps through every pixel in the surface and verifies that it was copied correctly.
    // Handles source colorkeying, not dest color keying, and no scaling.
    eTestResult CDDrawSurfaceVerify::Verify(const DWORD *dwsrc,
                                            const DWORD *dwdest,
                                            CDDSurfaceDesc *ddsdSrc,
                                            CDDSurfaceDesc *ddsdDst)
    {
        DDCOLORKEY ddckSrc,
                                ddckDst;
        bool bSrcKey=false, bDstKey=false;
        eTestResult tr=trPass;
        DWORD lSrcPitch = ddsdSrc->lPitch/4;
        DWORD lSrcXPitch = ddsdSrc->lXPitch/4;
        DWORD lDstPitch = ddsdDst->lPitch/4;
        DWORD lDstXPitch = ddsdDst->lXPitch/4;
        if (SUCCEEDED(m_piDDSSrc->GetColorKey(DDCKEY_SRCBLT, &ddckSrc)))
            bSrcKey=true;
        if (SUCCEEDED(m_piDDSDst->GetColorKey(DDCKEY_DESTBLT, &ddckDst)))
            bDstKey=true;

        int y1=m_rcSrc.top;
        int y2=m_rcDst.top;
        int x1 = 0;
        int x2 = 0;
        DWORD wSRed,
                     wSGreen,
                     wSBlue;
        DWORD wDRed,
                     wDGreen,
                     wDBlue;

        for(;y1<m_rcSrc.bottom && y2 < m_rcDst.bottom && tr==trPass;y1++, y2++)
        {

            x1=m_rcSrc.left;
            x2=m_rcDst.left;
            for(;x1<(int)(m_rcSrc.right) && x2<(int)(m_rcDst.right) && tr==trPass;x1++, x2++)
            {
                // if the pixels are different and it's not clipped, then we have a problem. otherwise, we're ok.
                if(dwdest[(lDstPitch*y2)+(lDstXPitch*x2)]!=dwsrc[(lSrcPitch*y1)+(lDstXPitch*x1)] && !IsClipped(x2,y2))
                {
                    wSRed=dwsrc[(lSrcPitch * y1) + (lSrcXPitch * x1)] & ddsdSrc->ddpfPixelFormat.dwRBitMask;
                    wSGreen=dwsrc[(lSrcPitch * y1) + (lSrcXPitch * x1)] & ddsdSrc->ddpfPixelFormat.dwGBitMask;
                    wSBlue=dwsrc[(lSrcPitch * y1) + (lSrcXPitch * x1)] & ddsdSrc->ddpfPixelFormat.dwBBitMask;
                    wDRed=dwdest[(lDstPitch * y2) + (lDstXPitch * x2)] & ddsdSrc->ddpfPixelFormat.dwRBitMask;
                    wDGreen=dwdest[(lDstPitch * y2) + (lDstXPitch * x2)] & ddsdSrc->ddpfPixelFormat.dwGBitMask;
                    wDBlue=dwdest[(lDstPitch * y2) + (lDstXPitch * x2)] & ddsdSrc->ddpfPixelFormat.dwBBitMask;

                    if(bSrcKey==true)
                    {
                        // if we're source color keyed, verify the surface
                        DWORD wSCKLRed=ddckSrc.dwColorSpaceLowValue&ddsdSrc->ddpfPixelFormat.dwRBitMask;
                        DWORD wSCKLGreen=ddckSrc.dwColorSpaceLowValue&ddsdSrc->ddpfPixelFormat.dwGBitMask;
                        DWORD wSCKLBlue=ddckSrc.dwColorSpaceLowValue&ddsdSrc->ddpfPixelFormat.dwBBitMask;
                        DWORD wSCKHRed=ddckSrc.dwColorSpaceHighValue&ddsdSrc->ddpfPixelFormat.dwRBitMask;
                        DWORD wSCKHGreen=ddckSrc.dwColorSpaceHighValue&ddsdSrc->ddpfPixelFormat.dwGBitMask;
                        DWORD wSCKHBlue=ddckSrc.dwColorSpaceHighValue&ddsdSrc->ddpfPixelFormat.dwBBitMask;
                        // if the source color is in the color key, then we're ok, if it's not in the color key, then it was supposed to be copied, so there's an error.
                        if(wSRed < wSCKLRed || wSRed > wSCKHRed ||
                            wSGreen < wSCKLGreen || wSGreen > wSCKHGreen ||
                            wSBlue < wSCKLBlue || wSBlue > wSCKHBlue)
                                tr|=trFail;
                    }
                    // it's not possible to test dest colorkeying unless we also have a backup of the destination prior to the blit!
                    else if(!(abs(wSRed-wDRed) <2 && abs(wSGreen-wDGreen) < 2 && abs(wSBlue-wDBlue) < 2) && bDstKey!=true)
                        tr|=trFail;

                }
            }
        }

        if(tr!=trPass)
        {
            dbgout << "source contains data " << HEX(dwsrc[(lSrcPitch * y1) + (lSrcXPitch * x1)]) << " dest contains data " << HEX(dwdest[(lDstPitch * y2) + (lDstXPitch * x2)]) << endl;
            dbgout << "nonmatching values in the surface comparison copying from (x,y) " << x1 << "," << y1 << " to " << x2 << ","<<y2 << endl;
            dbgout << "copy from (top,left,bottom, right) (" << m_rcSrc.top << "," << m_rcSrc.left <<","<< m_rcSrc.bottom <<","<< m_rcSrc.right <<")" << endl;
            dbgout << "To (top,left,bottom, right) (" << m_rcDst.top << "," << m_rcDst.left <<","<<m_rcDst.bottom <<","<< m_rcDst.right <<")" << endl;
            dbgout << "FAILED" << endl;
            return tr;
        }
        else
        {
            dbgout << "copy from (top,left,bottom, right) (" << m_rcSrc.top << "," << m_rcSrc.left <<","<< m_rcSrc.bottom <<","<< m_rcSrc.right <<")" << endl;
            dbgout << "To (top,left,bottom, right) (" << m_rcDst.top << "," << m_rcDst.left <<","<< m_rcDst.bottom <<","<< m_rcDst.right <<")" << endl;
            dbgout << "PASSED" << endl;

            return tr;
        }
    }


    ////////////////////////////////////////////////////////////////////////////////
    // CDDrawSurfaceVerify::PreSurfaceVerify
    // This function is called prior to the test, it makes backups of the source if needed and
    // sets up the clipping lists and other accounting for a surface verification.
    eTestResult CDDrawSurfaceVerify::PreSurfaceVerify(IDirectDrawSurface *piDDSSrc,
                                                      RECT rcSrc,
                                                      IDirectDrawSurface *piDDSDst,
                                                      RECT rcDst)
    {
        using namespace DDrawUty::Surface_Helpers;

        CDDSurfaceDesc ddsdSrc,
                       ddsdDst;
        const BYTE *dwsrc=NULL;
        DWORD dwSize=0;
        LPDIRECTDRAWCLIPPER FAR lpDDClipper;
        HRESULT hr = S_OK;
        LPRECT prcSrcBoundRect = NULL;
        RECT rcSrcBoundRect = {0, 0, 0, 0};
        RECT rcDstBoundRect = {0, 0, 0, 0};        
        RECT rcWndRect = {0, 0, 0, 0};
        eTestResult tr = trPass;

        m_rcSrc=rcSrc;
        m_rcDst=rcDst;
        m_piDDSDst=piDDSDst;
        m_piDDSSrc=piDDSSrc;

        hr = m_piDDSSrc->GetSurfaceDesc(&ddsdSrc);
        if(S_OK != hr)
        {
            dbgout (LOG_ABORT) << "Failure while trying to retrieve the Src surface desc" << endl;
            return trAbort;
        }

        hr = m_piDDSDst->GetSurfaceDesc(&ddsdDst);
        if(S_OK != hr)
        {
            dbgout (LOG_ABORT) << "Failure while trying to retrieve the Dst surface desc" << endl;
            return trAbort;
        }

        if((ddsdSrc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
        {
            tr = GetClippingRectangle(m_piDDSSrc, rcSrcBoundRect);
            if(trPass != tr)
            {
                dbgout (LOG_ABORT) << "Failure while trying to Src retrieve the clipping rectangle" << endl;
                return trAbort;
            }
            else
            {
                prcSrcBoundRect = &rcSrcBoundRect;

                // Shift m_rcSrc based on the bound rect
                m_rcSrc.top = m_rcSrc.top - rcSrcBoundRect.top;
                m_rcSrc.bottom = m_rcSrc.bottom - rcSrcBoundRect.top;
                m_rcSrc.left = m_rcSrc.left - rcSrcBoundRect.left;
                m_rcSrc.right = m_rcSrc.right - rcSrcBoundRect.left;
            }
        }

        if((ddsdDst.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
        {
            tr = GetClippingRectangle(m_piDDSDst, rcDstBoundRect);
            if(trPass != tr)
            {
                dbgout (LOG_ABORT) << "Failure while trying to retrieve the Dst clipping rectangle" << endl;
                return trAbort;
            }
            else
            {
                // Shift m_rcDst based on the bound rect
                m_rcDst.top = m_rcDst.top - rcDstBoundRect.top;
                m_rcDst.bottom = m_rcDst.bottom - rcDstBoundRect.top;
                m_rcDst.left = m_rcDst.left - rcDstBoundRect.left;
                m_rcDst.right = m_rcDst.right - rcDstBoundRect.left;
            }
        }

        dbgout << "Test will copy from (top,left,bottom, right) (" << m_rcSrc.top << "," << m_rcSrc.left <<","<<m_rcSrc.bottom <<","<< m_rcSrc.right <<")" << endl;
        dbgout << "To (top,left,bottom, right) (" << m_rcDst.top << "," << m_rcDst.left <<","<<m_rcDst.bottom <<","<< m_rcDst.right <<")" << endl;

        if(m_bClipDataValid)
        {
            delete[] m_rgndClipData;
            m_bClipDataValid=false;
            m_rgndClipData=NULL;
        }

        // We dont need the clipper data, as the locked region will be exactly the same as the clipper.
        if(!((ddsdSrc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL)))
        {
            if(SUCCEEDED(piDDSDst->GetClipper(&lpDDClipper)))
            {
                lpDDClipper->GetClipList(&m_rcDst, m_rgndClipData,&dwSize);
                m_rgndClipData=(RGNDATA *) new BYTE[dwSize];
                if(FAILED(lpDDClipper->GetClipList(&m_rcDst, m_rgndClipData,&dwSize)))
                {
                    dbgout << "Clipper attached, however unable to retrieve clip list" << endl;
                    dbgout << "FAILED" << endl;

                    lpDDClipper->Release();

                    return trFail;
                }
                lpDDClipper->Release();
                m_bClipDataValid=true;
            }
        }

        // check if the two are the same surface
        if(piDDSSrc==piDDSDst)
        {
            // if they're overlapping, then make a backup.
            if(IsOverlapping(m_rcSrc, m_rcDst))
            {
                // relock the entire source so that we copy the right part.
                if(FAILED(piDDSSrc->Lock(prcSrcBoundRect, &ddsdSrc, DDLOCK_WAITNOTBUSY, NULL)))
                {
                    dbgout << "Failure locking source surface for source copy" << endl;
                    dbgout << "FAILED" << endl;
                    return trFail;
                }

                // if we have an old backup, deallocate
                if(m_bBackupIsValid)
                {
                    delete[] m_dwSurfaceBackup;
                    m_bBackupIsValid=false;
                }

                // the array needs to be the number of lines * the number of dword's per line. (lpitch is bytes)
                DWORD dwHeightSize = abs(ddsdSrc.dwHeight*ddsdSrc.lPitch);
                DWORD dwWidthSize = abs(ddsdSrc.dwWidth*ddsdSrc.lXPitch);

                m_dwSize=max(dwHeightSize, dwWidthSize);

                if((ddsdSrc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
                {
                    // The pitches are for the entire window surface, so allocate memory for the Window Rect surface
                    // and not just the clipped region.
                    GetWindowRect(g_DirectDraw.m_hWnd, &rcWndRect);

                    dwHeightSize = abs((rcWndRect.bottom - rcWndRect.top)*ddsdSrc.lPitch);
                    dwWidthSize = abs((rcWndRect.right - rcWndRect.left)*ddsdSrc.lXPitch);
                }

                // Make sure the heap isn't fragmented
                CompactAllHeaps();
                // allocate enough for the rectangle needed.
                m_dwSurfaceBackup=new BYTE[m_dwSize];

                if(m_dwSurfaceBackup==NULL)
                {
                    dbgout << "Unable to allocate space for surface backup" << endl;
                    dbgout << "FAILED" << endl;
                    piDDSSrc->Unlock(NULL);
                    return trFail;
                }

                if (ddsdSrc.lPitch < 0 && ddsdSrc.lXPitch < 0)
                {
                    // 180
                    UINT uiOffset = -ddsdSrc.lPitch * (ddsdSrc.dwHeight - 1) - ddsdSrc.lXPitch * (ddsdSrc.dwWidth - 1);
                    m_pBackupSurfaceStart = m_dwSurfaceBackup + uiOffset;
                }
                else if (ddsdSrc.lPitch < 0)
                {
                    // 270
                    UINT uiOffset = -ddsdSrc.lPitch * (ddsdSrc.dwHeight - 1);
                    m_pBackupSurfaceStart = m_dwSurfaceBackup + uiOffset;
                }
                else if (ddsdSrc.lXPitch < 0)
                {
                    // 90
                    UINT uiOffset = -ddsdSrc.lXPitch * (ddsdSrc.dwWidth - 1);
                    m_pBackupSurfaceStart = m_dwSurfaceBackup + uiOffset;
                }
                else
                {
                    m_pBackupSurfaceStart = m_dwSurfaceBackup;
                }
                
                if((ddsdSrc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
                {
                    // The origin of the copy should be the origin of the clipped region.
                    DWORD dwOriginOfClipperX = rcSrcBoundRect.left - rcWndRect.left;
                    DWORD dwOriginOfClipperY = rcSrcBoundRect.top - rcWndRect.top;
                    m_pBackupSurfaceStart += (ddsdSrc.lPitch*dwOriginOfClipperY)+(ddsdSrc.lXPitch*dwOriginOfClipperX);
                }
                
                // we have a valid backup, set the source and dest for copy
                m_bBackupIsValid=true;

                dwsrc = (BYTE *)ddsdSrc.lpSurface;

                for(int y=m_rcSrc.top;y<m_rcSrc.bottom ;y++)
                {
                    for(int x=(m_rcSrc.left);x< static_cast<int> (m_rcSrc.right);x++)
                    {
                        for (int b = 0; b <ddsdSrc.ddpfPixelFormat.dwRGBBitCount/8; b++)
                        {
                            m_pBackupSurfaceStart[(ddsdSrc.lPitch*y)+(ddsdSrc.lXPitch*x) + b]=dwsrc[(ddsdSrc.lPitch*y)+(ddsdSrc.lXPitch*x) + b];
                        }
                    }
                }
                // done copying, unlock the surface.
                piDDSSrc->Unlock(prcSrcBoundRect);

            }
        }

    return trPass;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CDDrawSurfaceVerify::SurfaceVerify
    // This function performs the surface verification and returns pass or fail depending on wether
    // the destination matches the source properly.

    eTestResult CDDrawSurfaceVerify::SurfaceVerify()
    {
        using namespace DDrawUty::Surface_Helpers;

        HRESULT hr = S_OK;
        CDDSurfaceDesc ddsdSrc,
                       ddsdDst;        
        BYTE *dwdest=NULL,
             *dwsrc=NULL;
        eTestResult tr=trPass;
        LPRECT prcDstBoundRect = NULL;
        RECT rcDstBoundRect = {0, 0, 0, 0};
        LPRECT prcSrcBoundRect = NULL;
        RECT rcSrcBoundRect = {0, 0, 0, 0};

        memset(&ddsdSrc, 0, sizeof(ddsdSrc));
        memset(&ddsdDst, 0, sizeof(ddsdDst));
        ddsdSrc.dwSize = sizeof(ddsdSrc);
        ddsdDst.dwSize = sizeof(ddsdDst);

        // For a Primary in Windowed mode, use the Window's bounding rectangle in Lock, instead of NULL.
        hr = m_piDDSDst->GetSurfaceDesc(&ddsdDst);
        if(S_OK != hr)
        {
            dbgout (LOG_ABORT) << "Failure while trying to retrieve the surface desc" << endl;
            return trAbort;
        }

        if((ddsdDst.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
        {
            tr = GetClippingRectangle(m_piDDSDst, rcDstBoundRect);
            if(trPass != tr)
            {
                dbgout (LOG_ABORT) << "Failure while trying to retrieve the clipping rectangle" << endl;
                return trAbort;
            }
            else
            {
                prcDstBoundRect = &rcDstBoundRect;
            }
        }

        hr = m_piDDSSrc->GetSurfaceDesc(&ddsdSrc);
        if(S_OK != hr)
        {
            dbgout (LOG_ABORT) << "Failure while trying to retrieve the surface desc" << endl;
            return trAbort;
        }

        if((ddsdSrc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
        {
            tr = GetClippingRectangle(m_piDDSSrc, rcSrcBoundRect);
            if(trPass != tr)
            {
                dbgout (LOG_ABORT) << "Failure while trying to retrieve the clipping rectangle" << endl;
                return trAbort;
            }
            else
            {
                prcSrcBoundRect = &rcSrcBoundRect;
            }
        }

        if(((m_rcSrc.right-m_rcSrc.left) != (m_rcDst.right-m_rcDst.left)) || ((m_rcSrc.bottom-m_rcSrc.top) != (m_rcDst.bottom-m_rcDst.top)))
        {
        dbgout << "rectangles to copy are not the same size, verification test cannot scale." << endl;
        dbgout << "Source rectangle is " << m_rcSrc.right-m_rcSrc.left << "x" << m_rcSrc.bottom-m_rcSrc.top << endl;
        dbgout << "Dest rectangle is " << m_rcDst.right-m_rcDst.left << "x" << m_rcDst.bottom-m_rcDst.top << endl;
        dbgout << "FAILED" << endl;
        return trFail;
        }

        // lock the destination surface for use in verify
        if(FAILED(m_piDDSDst->Lock(prcDstBoundRect, &ddsdDst, DDLOCK_WAITNOTBUSY, NULL)))
        {
            dbgout << "Failure locking dest surface for source copy" << endl;
            dbgout << "FAILED" << endl;
            return trFail;
        }
        dwdest = (BYTE*)ddsdDst.lpSurface;

        if(m_bBackupIsValid)
        {
            dwsrc = (BYTE*)m_pBackupSurfaceStart;
            ddsdSrc = ddsdDst;
        }
        // if they're the same surface, but not overlapping, then use the same surface pointer
        else if(m_piDDSSrc==m_piDDSDst)
        {
            dwsrc= (BYTE*)ddsdDst.lpSurface;
        }
        // they're completely different surfaces, get a pointer to the source.
        else
        {
            if(FAILED(m_piDDSSrc->Lock(prcSrcBoundRect, &ddsdSrc, DDLOCK_WAITNOTBUSY, NULL)))
            {
                dbgout << "Failure locking dest surface for source copy" << endl;
                dbgout << "FAILED" << endl;
                m_piDDSDst->Unlock(prcDstBoundRect);
                return trFail;
            }
            dwsrc = (BYTE*)ddsdSrc.lpSurface;
            // the two surfaces are completely different, so verify that the pixel
            // formats match.
             if(ddsdSrc.ddpfPixelFormat.dwRGBBitCount != ddsdDst.ddpfPixelFormat.dwRGBBitCount)
             {
                 dbgout << "Nonmatching pixel formats" << endl;
                 dbgout << "FAILED" << endl;
                 m_piDDSSrc->Unlock(prcSrcBoundRect);
                 m_piDDSDst->Unlock(prcDstBoundRect);
                 return trFail;
             }
        }



        if(ddsdDst.ddpfPixelFormat.dwRGBBitCount ==16 && !(ddsdDst.ddpfPixelFormat.dwFlags & DDPF_FOURCC))
        tr|=Verify((WORD*)dwsrc,
                         (WORD*)dwdest,
                         &ddsdSrc,
                         &ddsdDst);
        else if(ddsdDst.ddpfPixelFormat.dwRGBBitCount ==32 || (ddsdDst.ddpfPixelFormat.dwFlags & DDPF_FOURCC))
            tr|=Verify((DWORD*)dwsrc,
                         (DWORD*)dwdest,
                         &ddsdSrc,
                         &ddsdDst);
        else dbgout << "Unknown surface format/paletted format, unable to verify" << endl;

        // if the source and dest aren't the same surface (so we had to call lock twice earlier), unlock it.
        if(m_piDDSSrc!=m_piDDSDst)
            m_piDDSSrc->Unlock(prcSrcBoundRect);
        m_piDDSDst->Unlock(prcDstBoundRect);
        return tr;
    }

        ////////////////////////////////////////////////////////////////////////////////
    // CDDrawSurfaceVerify::PreSurfaceVerify
    // This function is called prior to the test, it makes backups of the source if needed and
    // sets up the clipping lists and other accounting for a surface verification.
    eTestResult CDDrawSurfaceVerify::PreVerifyColorFill(IDirectDrawSurface *piDDS)
    {
        DWORD dwSize=0;
        LPDIRECTDRAWCLIPPER FAR lpDDClipper;
        m_piDDSSrc=piDDS;
        if(m_bClipDataValid)
        {
            delete[] m_rgndClipData;
            m_bClipDataValid=false;
            m_rgndClipData=NULL;
        }

        if(SUCCEEDED(piDDS->GetClipper(&lpDDClipper)))
        {
            lpDDClipper->GetClipList(NULL, m_rgndClipData,&dwSize);
            m_rgndClipData=(RGNDATA *) new BYTE[dwSize];
            if(FAILED(lpDDClipper->GetClipList(NULL, m_rgndClipData,&dwSize)))
            {
                dbgout << "Clipper attached, however unable to retrieve clip list" << endl;
                dbgout << "FAILED" << endl;

                lpDDClipper->Release();

                return trFail;
            }
            lpDDClipper->Release();
            m_bClipDataValid=true;
        }
    return trPass;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // VerifyColorFill
    // steps through the surface to verify that the specified rectangle on the
    // surface is filled with the color specified.
    eTestResult CDDrawSurfaceVerify::VerifyColorFill(DWORD dwfillcolor, const RECT* prcVerifyRect)
    {       
        using namespace DDrawUty::Surface_Helpers;

        CDDSurfaceDesc ddsd;
        BYTE *dwSrc=NULL;
        BYTE *wSrc=NULL;
        HRESULT hr = S_OK;
        eTestResult tr = trPass;
        dbgout << "Verifying color fill" << endl;

        LPRECT prcBoundRect = NULL;
        RECT rcBoundRect = {0, 0, 0, 0};

        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.dwSize = sizeof(ddsd);

        // For a Primary in Windowed mode, use the Window's bounding rectangle in Lock, instead of NULL.
        hr = m_piDDSSrc->GetSurfaceDesc(&ddsd);
        if(S_OK != hr)
        {
            dbgout (LOG_ABORT) << "Failure while trying to retrieve the surface desc" << endl;
            return trAbort;
        }

        DWORD dwHeight = ddsd.dwHeight;
        DWORD dwWidth = ddsd.dwWidth;
        DWORD dwOffsetX = 0;
        DWORD dwOffsetY = 0;

        if((ddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
        {
            tr = GetClippingRectangle(m_piDDSSrc, rcBoundRect);
            if(trPass != tr)
            {
                dbgout (LOG_ABORT) << "Failure while trying to retrieve the clipping rectangle" << endl;
                return trAbort;
            }
            else
            {
                prcBoundRect = &rcBoundRect;
            }

            // For a windowed primary, lpSurface points to the memory area of the clipping rectangle,
            // but the ddsd.dwWidth/dwHeight are for the entire surface & not the locked region.
            // So use the clipping rectangle's width and height here.
            dwHeight = rcBoundRect.bottom - rcBoundRect.top;
            dwWidth = rcBoundRect.right - rcBoundRect.left;

            // Also, the clipping rectangle & the verifying rectangle are in screen coordinates while
            // the x,y in the loops below are w.r.t. the clipping rectangle's top left coordinate.
            // Update the offsets to map the x,y to screen coordinates
            dwOffsetX = rcBoundRect.left;
            dwOffsetY = rcBoundRect.top;
        }

        if(FAILED(m_piDDSSrc->Lock(prcBoundRect, &ddsd, DDLOCK_WAITNOTBUSY, NULL)))
        {
            dbgout << "failure locking Destination surface for verification" << endl;
            return trFail;
        }


        if(ddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC)
        {
            dwSrc=(BYTE*)ddsd.lpSurface;
            for(int y=0;y<(int)dwHeight;y++)
            {
                for(int x=0;x<(int)dwWidth;x++)
                {
                    WORD wCurrentFill = (dwfillcolor >> (16 * (x%2))) & 0xffff;
                    // If the FourCC format is smaller than 16 BPP, accessing a word at a time could lead to data misalignment errors
                    if(memcmp((dwSrc + y * ddsd.lPitch + x * ddsd.lXPitch), &wCurrentFill, sizeof(wCurrentFill)) 
                       && !IsClipped(x+dwOffsetX, y+dwOffsetY) 
                       && IsWithinRect(x+dwOffsetX, y+dwOffsetY, prcVerifyRect))
                        {
                        m_piDDSSrc->Unlock(prcBoundRect);
                        dbgout << "Color fill FAILED, color at dest " << HEX(((WORD*)(dwSrc + y * ddsd.lPitch + x * ddsd.lXPitch))[0]) << " expected color " << HEX(wCurrentFill) << endl;
                        dbgout << "location (x,y) " << x << " " << y << endl;
                        dbgout << "FAILED" << endl;
                        return trFail;
                        }
                }
            }
        }
        else if(ddsd.ddpfPixelFormat.dwRGBBitCount ==16)
        {
            wSrc=(BYTE*)ddsd.lpSurface;
            for(int y=0;y<(int)dwHeight;y++)
            {
                for(int x=0;x<(int)dwWidth;x++)
                {
                    if(((WORD*)(wSrc + y * ddsd.lPitch + x * ddsd.lXPitch))[0] != (WORD)dwfillcolor 
                       && !IsClipped(x+dwOffsetX, y+dwOffsetY) 
                       && IsWithinRect(x+dwOffsetX, y+dwOffsetY, prcVerifyRect))
                        {
                        m_piDDSSrc->Unlock(prcBoundRect);
                        dbgout << "Color fill FAILED, color at dest " << HEX(((WORD*)(wSrc + y * ddsd.lPitch + x * ddsd.lXPitch))[0]) << " expected color " << HEX((WORD)dwfillcolor) << endl;
                        dbgout << "location (x,y) " << x << " " << y << endl;
                        dbgout << "FAILED" << endl;
                        return trFail;
                        }
                }
            }
        }
        else if(ddsd.ddpfPixelFormat.dwRGBBitCount ==32)
        {
            dwSrc=(BYTE*)ddsd.lpSurface;
            for(int y=0;y<(int)dwHeight;y++)
            {
                for(int x=0;x<(int)dwWidth;x++)
                {
                    if(((DWORD*)(dwSrc + y * ddsd.lPitch + x * ddsd.lXPitch))[0] != dwfillcolor 
                       && !IsClipped(x+dwOffsetX, y+dwOffsetY) 
                       && IsWithinRect(x+dwOffsetX, y+dwOffsetY, prcVerifyRect))
                        {
                        m_piDDSSrc->Unlock(prcBoundRect);
                        dbgout << "Color fill FAILED, color at dest " << HEX(((DWORD*)(dwSrc + y * ddsd.lPitch + x * ddsd.lXPitch))[0]) << " expected color " << HEX(dwfillcolor) << endl;
                        dbgout << "location (x,y) " << x << " " << y << endl;
                        dbgout << "FAILED" << endl;
                        return trFail;
                        }
                }
            }
        }
        dbgout << "Color fill successful." << endl;
        m_piDDSSrc->Unlock(prcBoundRect);
        return trPass;
    }

}
