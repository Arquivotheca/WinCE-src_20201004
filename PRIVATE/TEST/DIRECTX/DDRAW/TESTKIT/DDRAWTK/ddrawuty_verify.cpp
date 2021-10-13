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
#include "DDrawUty_Verify.h"

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
    eTestResult CDDrawSurfaceVerify::Verify(WORD *dwsrc, 
                                            WORD *dwdest,
                                            CDDSurfaceDesc *ddsdSrc, 
                                            CDDSurfaceDesc *ddsdDst)
    {
        DDCOLORKEY ddckSrc, 
                                ddckDst;
        bool bSrcKey=false,
               bDstKey=false;
        eTestResult tr=trPass;
        DWORD lSrcPitch = ddsdSrc->lPitch/2;
        DWORD lDstPitch = ddsdDst->lPitch/2;
        if (SUCCEEDED(m_piDDSSrc->GetColorKey(DDCKEY_SRCBLT, &ddckSrc)))
            bSrcKey=true;
        if (SUCCEEDED(m_piDDSDst->GetColorKey(DDCKEY_DESTBLT, &ddckDst)))
            bDstKey=true;

        int y1=m_rcSrc.top;
        int y2=m_rcDst.top;
        int x1;
        int x2;
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
                if(dwdest[(lDstPitch*y2)+x2]!=dwsrc[(lSrcPitch*y1)+x1] && !IsClipped(x2,y2))
                {
                    wSRed=dwsrc[(lSrcPitch*y1)+x1] & ddsdSrc->ddpfPixelFormat.dwRBitMask;
                    wSGreen=dwsrc[(lSrcPitch*y1)+x1] & ddsdSrc->ddpfPixelFormat.dwGBitMask;
                    wSBlue=dwsrc[(lSrcPitch*y1)+x1] & ddsdSrc->ddpfPixelFormat.dwBBitMask;
                    wDRed=dwdest[(lDstPitch*y2)+x2] & ddsdSrc->ddpfPixelFormat.dwRBitMask;
                    wDGreen=dwdest[(lDstPitch*y2)+x2] & ddsdSrc->ddpfPixelFormat.dwGBitMask;
                    wDBlue=dwdest[(lDstPitch*y2)+x2] & ddsdSrc->ddpfPixelFormat.dwBBitMask;
                    
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
            // compensate for increment at end of loop.
            x1--;
            x2--;
            y1--;
            y2--;
            dbgout << "source contains data " << HEX(dwsrc[(lSrcPitch*y1)+x1]) << " dest contains data " << HEX(dwdest[(lDstPitch*y2)+x2]) << endl;
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
    eTestResult CDDrawSurfaceVerify::Verify(DWORD *dwsrc, 
                                            DWORD *dwdest,
                                            CDDSurfaceDesc *ddsdSrc, 
                                            CDDSurfaceDesc *ddsdDst)
    {
        DDCOLORKEY ddckSrc, 
                                ddckDst;
        bool bSrcKey=false, bDstKey=false;
        eTestResult tr=trPass;
        DWORD lSrcPitch = ddsdSrc->lPitch/4;
        DWORD lDstPitch = ddsdDst->lPitch/4;
        if (SUCCEEDED(m_piDDSSrc->GetColorKey(DDCKEY_SRCBLT, &ddckSrc)))
            bSrcKey=true;
        if (SUCCEEDED(m_piDDSDst->GetColorKey(DDCKEY_DESTBLT, &ddckDst)))
            bDstKey=true;

        int y1=m_rcSrc.top;
        int y2=m_rcDst.top;
        int x1;
        int x2;
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
                if(dwdest[(lDstPitch*y2)+x2]!=dwsrc[(lSrcPitch*y1)+x1] && !IsClipped(x2,y2))
                {
                    wSRed=dwsrc[(lSrcPitch*y1)+x1] & ddsdSrc->ddpfPixelFormat.dwRBitMask;
                    wSGreen=dwsrc[(lSrcPitch*y1)+x1] & ddsdSrc->ddpfPixelFormat.dwGBitMask;
                    wSBlue=dwsrc[(lSrcPitch*y1)+x1] & ddsdSrc->ddpfPixelFormat.dwBBitMask;
                    wDRed=dwdest[(lDstPitch*y2)+x2] & ddsdSrc->ddpfPixelFormat.dwRBitMask;
                    wDGreen=dwdest[(lDstPitch*y2)+x2] & ddsdSrc->ddpfPixelFormat.dwGBitMask;
                    wDBlue=dwdest[(lDstPitch*y2)+x2] & ddsdSrc->ddpfPixelFormat.dwBBitMask;
                    
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
            dbgout << "source contains data " << HEX(dwsrc[(lSrcPitch*y1)+x1]) << " dest contains data " << HEX(dwdest[(lDstPitch*y2)+x2]) << endl;
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
        CDDSurfaceDesc ddsdSrc, 
                       ddsdDst;
        BYTE *dwsrc=NULL;
        DWORD dwSize=0;
        LPDIRECTDRAWCLIPPER FAR lpDDClipper;

        m_rcSrc=rcSrc;
        m_rcDst=rcDst;
        m_piDDSDst=piDDSDst;
        m_piDDSSrc=piDDSSrc;

        dbgout << "Test will copy from (top,left,bottom, right) (" << rcSrc.top << "," << rcSrc.left <<","<<rcSrc.bottom <<","<< rcSrc.right <<")" << endl;
        dbgout << "To (top,left,bottom, right) (" << rcDst.top << "," << rcDst.left <<","<<rcDst.bottom <<","<< rcDst.right <<")" << endl;

        if(m_bClipDataValid)
        {    
            delete[] m_rgndClipData;
            m_bClipDataValid=false;
            m_rgndClipData=NULL;
        }

        if(piDDSDst->GetClipper(&lpDDClipper)!=DDERR_NOCLIPPERATTACHED)
        {
            lpDDClipper->GetClipList(&rcDst, m_rgndClipData,&dwSize);
            m_rgndClipData=(RGNDATA *) new BYTE[dwSize];
            if(FAILED(lpDDClipper->GetClipList(&rcDst, m_rgndClipData,&dwSize)))
            {
                dbgout << "Clipper attached, however unable to retrieve clip list" << endl;
                return trFail;
            }
            m_bClipDataValid=true;
        }
        
        // check if the two are the same surface
        if(piDDSSrc==piDDSDst)
        {
            // if they're overlapping, then make a backup.
            if(IsOverlapping(rcSrc, rcDst))
            {
                // relock the entire source so that we copy the right part.
                if(FAILED(piDDSSrc->Lock(NULL, &ddsdSrc, DDLOCK_WAIT, NULL)))
                {
                    dbgout << "failure locking source surface for source copy" << endl;
                    return trFail;
                }

                // if we have an old backup, deallocate
                if(m_bBackupIsValid)
                {
                    delete[] m_dwSurfaceBackup;
                    m_bBackupIsValid=false;
                }

                // the array needs to be the number of lines * the number of dword's per line. (lpitch is bytes)
                m_dwSize=ddsdSrc.dwHeight*(ddsdSrc.lPitch);
                // allocate enough for the rectangle needed.
                m_dwSurfaceBackup=new BYTE[m_dwSize];

                if(m_dwSurfaceBackup==NULL)
                {
                    dbgout << "Unable to allocate space for surface backup." << endl;
                    return trFail;
                }

                // we have a valid backup, set the source and dest for copy
                m_bBackupIsValid=true;

                dwsrc = (BYTE *)ddsdSrc.lpSurface;

                for(int y=rcSrc.top;y<rcSrc.bottom ;y++)
                {
                    for(int x=(rcSrc.left *(ddsdSrc.ddpfPixelFormat.dwRGBBitCount/8));x<(int) (rcSrc.right *(ddsdSrc.ddpfPixelFormat.dwRGBBitCount/8));x++)
                    {
                    m_dwSurfaceBackup[(ddsdSrc.lPitch*y)+x]=dwsrc[(ddsdSrc.lPitch*y)+x];
                    }
                }
                // done copying, unlock the surface.
                piDDSSrc->Unlock(NULL);
                                
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
        CDDSurfaceDesc ddsdSrc, 
                       ddsdDst;
        BYTE *dwdest=NULL, 
             *dwsrc=NULL;
        eTestResult tr=trPass;
        
        if(((m_rcSrc.right-m_rcSrc.left) != (m_rcDst.right-m_rcDst.left)) || ((m_rcSrc.bottom-m_rcSrc.top) != (m_rcDst.bottom-m_rcDst.top)))
        {
        dbgout << "rectangles to copy are not the same size, verification test cannot scale." << endl;
        dbgout << "Source rectangle is " << m_rcSrc.right-m_rcSrc.left << "x" << m_rcSrc.bottom-m_rcSrc.top << endl;
        dbgout << "Dest rectangle is " << m_rcDst.right-m_rcDst.left << "x" << m_rcDst.bottom-m_rcDst.top << endl;
        return trFail;
        }

        // lock the destination surface for use in verify
        if(FAILED(m_piDDSDst->Lock(NULL, &ddsdDst, DDLOCK_WAIT, NULL)))
        {
            dbgout << "failure locking dest surface for source copy" << endl;
            return trFail;
        }
        dwdest = (BYTE*)ddsdDst.lpSurface;
        
        if(m_bBackupIsValid)
            dwsrc = (BYTE*)m_dwSurfaceBackup;
        // if they're the same surface, but not overlapping, then use the same surface pointer
        else if(m_piDDSSrc==m_piDDSDst)
        {
            dwsrc= (BYTE*)ddsdDst.lpSurface;
        }
        // they're completely different surfaces, get a pointer to the source.
        else 
        {
            if(FAILED(m_piDDSSrc->Lock(NULL, &ddsdSrc, DDLOCK_WAIT, NULL)))
            {
                dbgout << "failure locking dest surface for source copy" << endl;
                m_piDDSDst->Unlock(NULL);
                return trFail;
             }
            dwsrc = (BYTE*)ddsdSrc.lpSurface;
            // the two surfaces are completely different, so verify that the pixel
            // formats match.
             if(ddsdSrc.ddpfPixelFormat.dwRGBBitCount != ddsdDst.ddpfPixelFormat.dwRGBBitCount)
             {
                 dbgout << "Nonmatching pixel formats" << endl;
                 m_piDDSSrc->Unlock(NULL);
                 m_piDDSDst->Unlock(NULL);
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
            m_piDDSSrc->Unlock(NULL);
        m_piDDSDst->Unlock(NULL);
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

        if(piDDS->GetClipper(&lpDDClipper)!=DDERR_NOCLIPPERATTACHED)
        {
            lpDDClipper->GetClipList(NULL, m_rgndClipData,&dwSize);
            m_rgndClipData=(RGNDATA *) new BYTE[dwSize];
            if(FAILED(lpDDClipper->GetClipList(NULL, m_rgndClipData,&dwSize)))
            {
                dbgout << "Clipper attached, however unable to retrieve clip list" << endl;
                return trFail;
            }
            m_bClipDataValid=true;
        }
    return trPass;
    }
        
    ////////////////////////////////////////////////////////////////////////////////
    // VerifyColorFill
    // steps through the surface to verifiy that the whole surface is filled with the
    // color specified.
    eTestResult CDDrawSurfaceVerify::VerifyColorFill(DWORD dwfillcolor)
    {
        CDDSurfaceDesc ddsd;
        DWORD *dwSrc=NULL;
        WORD *wSrc=NULL;
        dbgout << "Verifying color fill" << endl;
        
        if(FAILED(m_piDDSSrc->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL)))
        {
            dbgout << "failure locking Destination surface for verification" << endl;
            return trFail;
        }
        

        if(ddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC)
        {
            dwSrc=(DWORD*)ddsd.lpSurface;
            for(int y=0;y<(int)ddsd.dwHeight;y++)
            {
                for(int x=0;x<(int)ddsd.dwWidth/2;x++)
                {
                    if(dwSrc[x]!=dwfillcolor && !IsClipped(x,y))
                        {
                        m_piDDSSrc->Unlock(NULL);
                        dbgout << "Color fill FAILED, color at dest " << HEX(dwSrc[x]) << " expected color " << HEX(dwfillcolor) << endl;
                        dbgout << "location (x,y) " << x << " " << y << endl;
                        return trFail;
                        }
                }
                dwSrc+=ddsd.lPitch/4;
            }
        }
        else if(ddsd.ddpfPixelFormat.dwRGBBitCount ==16)
        {
            wSrc=(WORD*)ddsd.lpSurface;
            for(int y=0;y<(int)ddsd.dwHeight;y++)
            {
                for(int x=0;x<(int)ddsd.dwWidth;x++)
                {
                    if(wSrc[x]!=(WORD)dwfillcolor && !IsClipped(x,y))
                        {
                        m_piDDSSrc->Unlock(NULL);
                        dbgout << "Color fill FAILED, color at dest " << HEX(wSrc[x]) << " expected color " << HEX((WORD)dwfillcolor) << endl;
                        dbgout << "location (x,y) " << x << " " << y << endl;
                        return trFail;
                        }
                }
                wSrc+=ddsd.lPitch/2;
            }
        }
        else if(ddsd.ddpfPixelFormat.dwRGBBitCount ==32)
        {
            dwSrc=(DWORD*)ddsd.lpSurface;
            for(int y=0;y<(int)ddsd.dwHeight;y++)
            {
                for(int x=0;x<(int)ddsd.dwWidth;x++)
                {
                    if(dwSrc[x]!=dwfillcolor && !IsClipped(x,y))
                        {
                        m_piDDSSrc->Unlock(NULL);
                        dbgout << "Color fill FAILED, color at dest " << HEX(dwSrc[x]) << " expected color " << HEX(dwfillcolor) << endl;
                        dbgout << "location (x,y) " << x << " " << y << endl;
                        return trFail;
                        }
                }
                dwSrc+=ddsd.lPitch/4;
            }
        }
        dbgout << "Color fill successful." << endl;
        m_piDDSSrc->Unlock(NULL);
        return trPass;
    }
    
}
