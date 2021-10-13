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

#include <DDrawUty.h>

#include "IDDSurfaceTests.h"
#include "DDrawUty_Config.h"
#include "DDrawUty_Verify.h"
#include "DDTestKit_Modifiers.h"

#include <vector>

// Disable  'DWORD' : forcing value to bool 'true' or 'false' (performance warning)
#pragma warning ( disable : 4800 )

#define KEY_DOWN_BIT 0x80   //the key is down if the most significant bit is set 

enum TOUCH_RESPONSE
{
    NoTap  = 0, // No input from touch screen 
    UpperTap,   // Upper half of screen is tapped
    LowerTap    // Lower half of screen is tapped
    
};
UINT uNumber = 0;

using namespace com_utilities;
using namespace DebugStreamUty;
using namespace DDrawUty;
using namespace DDrawVerifyUty;
using namespace std;

namespace priv_Overlay
{
    void RealignRects(RECT * prcSrc, RECT * prcDest, const DDCAPS * pCaps)
    {
        DWORD dwAlignSizeSrc;
        DWORD dwAlignBoundarySrc;
        DWORD dwAlignSizeDest;
        DWORD dwAlignBoundaryDest;

        DWORD dwAlignSrc;
        DWORD dwAlignDest;

        dwAlignSizeSrc = pCaps->dwAlignSizeSrc;
        dwAlignBoundarySrc = pCaps->dwAlignBoundarySrc;
        dwAlignSizeDest = pCaps->dwAlignSizeDest;
        dwAlignBoundaryDest = pCaps->dwAlignBoundaryDest;

        if (!dwAlignSizeSrc)
        {
            dwAlignSizeSrc = 1;
        }

        if (!dwAlignBoundarySrc)
        {
            dwAlignBoundarySrc = 1;
        }

        if (!dwAlignSizeDest)
        {
            dwAlignSizeDest = 1;
        }

        if (!dwAlignBoundaryDest)
        {
            dwAlignBoundaryDest = 1;
        }

        if (!(dwAlignSizeSrc % dwAlignBoundarySrc))
        {
            dwAlignSrc = dwAlignSizeSrc;
        }
        else if (!(dwAlignBoundarySrc % dwAlignSizeSrc))
        {
            dwAlignSrc = dwAlignBoundarySrc;
        }
        else
        {
            dwAlignSrc = dwAlignBoundarySrc * dwAlignSizeSrc;
        }

        if (!(dwAlignSizeDest % dwAlignBoundaryDest))
        {
            dwAlignDest = dwAlignSizeDest;
        }
        else if (!(dwAlignBoundaryDest % dwAlignSizeDest))
        {
            dwAlignDest = dwAlignBoundaryDest;
        }
        else
        {
            dwAlignDest = dwAlignBoundaryDest * dwAlignSizeDest;
        }

        // Check for a size restriction on overlays
        if (prcSrc)
        {
            // If AlignBoundarySrc is non-zero, the source rectangle boundaries
            // need to be a multilple.
            if (dwAlignBoundarySrc > 1)
            {
                // Make sure the overlay is properly aligned at the four corners
                prcSrc->left -= (prcSrc->left % dwAlignBoundarySrc);
                prcSrc->right -= (prcSrc->right % dwAlignBoundarySrc);
                prcSrc->top -= (prcSrc->top % dwAlignBoundarySrc);
                prcSrc->bottom -= (prcSrc->bottom % dwAlignBoundarySrc);
            }

            // If AlignSizeSrc is non-zero, then the source rectange
            // height and width must be a multiple of it
            if (dwAlignSizeSrc > 1)
            {
                // Make sure the overlay width and height are properly aligned
                // (and make sure this doesn't throw off the AlignBoundary requirement)
                prcSrc->right -= ((prcSrc->right - prcSrc->left) % (dwAlignSrc));
                prcSrc->bottom -= ((prcSrc->bottom - prcSrc->top) % (dwAlignSrc));
            }

            // Make sure the rect is non-zero width and height.
            if (prcSrc->right - prcSrc->left <= 0)
            {
                prcSrc->right += dwAlignSrc;
            }

            if (prcSrc->bottom - prcSrc->top <= 0)
            {
                prcSrc->bottom += dwAlignSrc;
            }
        }

        // Check for position restrictions on overlays
        if (prcDest)
        {
            // If AlignBoundaryDest is non-zero then the destination
            // rectange top-left postion must be a multiple of it

            if (dwAlignBoundaryDest > 1)
            {
                // Make sure the overlay is properly aligned at the four corners
                prcDest->left -= (prcDest->left % dwAlignBoundaryDest);
                prcDest->right -= (prcDest->right % dwAlignBoundaryDest);
                prcDest->top -= (prcDest->top % dwAlignBoundaryDest);
                prcDest->bottom -= (prcDest->bottom % dwAlignBoundaryDest);
            }

            // If AlignSizeDest is non-zero, then the destination rectange
            // height and width must be a multiple of it
            if (dwAlignSizeDest > 1)
            {
                // Make sure the overlay width and height are properly aligned
                prcDest->right -= ((prcDest->right - prcDest->left) % (dwAlignDest));
                prcDest->bottom -= ((prcDest->bottom - prcDest->top) % (dwAlignDest));
            }

            // Make sure the rect is non-zero width and height.
            if (prcDest->right - prcDest->left <= 0)
            {
                prcDest->right += dwAlignDest;
            }

            if (prcDest->bottom - prcDest->top <= 0)
            {
                prcDest->bottom += dwAlignDest;
            }
        }
    }
}

namespace
{
    HFONT GetSuitableFont(HDC hdc)
    {
        static HFONT hfontChosen = NULL;

        // Choosing 8 as the font size as it is readable on all resolutions.
        static const DWORD POINT_SIZE = 8;

        // If we have not already found the desired font, search for one.
        if(NULL == hfontChosen)
        {
            LOGFONT lfDesiredFont;
            memset(&lfDesiredFont, 0, sizeof(LOGFONT));
            lfDesiredFont.lfCharSet = ANSI_CHARSET;
            lfDesiredFont.lfHeight = -MulDiv(POINT_SIZE, GetDeviceCaps(hdc, LOGPIXELSY), 72);

            hfontChosen = CreateFontIndirect(&lfDesiredFont);
        }

        return hfontChosen;
    }

    eTestResult DisplayVerifyString(IDirectDrawSurface * piDDS, TCHAR * szInstructions, bool bUpperLeftText=true)
    {
        eTestResult tr = trPass;
        static RECT rc = {0, 0, 0, 0};
        int iWidth = 190;
        HRESULT hr;
        HDC hdc;
        DDSURFACEDESC ddsd;
        memset(&ddsd, 0x00, sizeof(DDSURFACEDESC));
        ddsd.dwSize = sizeof(DDSURFACEDESC);
        ddsd.dwFlags = DDSD_VALID;

        hr = piDDS->GetSurfaceDesc(&ddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trFail);

        if (ddsd.dwWidth < (DWORD) iWidth + 10)
        {
            iWidth = ddsd.dwWidth - 10;
            dbgout << "Cutting down on width available for string: " << iWidth << endl;
        }

        // print out what's going on to the user.

        // Get the DC of the Window in case it is a Windowed Primary
        if((ddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
        {
            hdc = GetDC(g_DirectDraw.m_hWnd);
            if(NULL == hdc)
            {
                dbgout(LOG_FAIL) << "Unable to retrieve a DC to the Window." << endl;
                return trFail;
            }
        }
        else
        {
            hr = piDDS->GetDC(&hdc);
            CheckHRESULT(hr, "GetDC", trFail);
        }

        HFONT hfont = GetSuitableFont(hdc);
        if(hfont != NULL) // Select the font into the DC if available, else use whatever is available by default.
        {
            SelectObject(hdc, hfont);
        }

        // determine the size of the rectangle to clear for the current message.
        if (bUpperLeftText)
        {
            SetRect(&rc, 5, 0, iWidth + 5, 1);
        }
        else
        {
            SetRect(&rc, ddsd.dwWidth-iWidth, ddsd.dwHeight/2, ddsd.dwWidth, ddsd.dwHeight/2+1); 
        }

       // make sure any previous messages are cleared.
        DrawText(hdc, szInstructions, -1, &rc, DT_WORDBREAK | DT_CALCRECT);
        BitBlt(hdc, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, NULL, 0, 0, WHITENESS);

        // draw the current message.
        DrawText(hdc, szInstructions, -1, &rc, DT_WORDBREAK);

        if((ddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
        {
            ReleaseDC(g_DirectDraw.m_hWnd, hdc);
        }
        else
        {
            hr = piDDS->ReleaseDC(hdc);
            CheckHRESULT(hr, "ReleaseDC", trFail);
        }

        // spit the instructions out to the debug output in case they are obscured in the window.
        dbgout << szInstructions << endl;
        return tr;
    }

    int CheckForKeyboardInput(int cChars, int rgLookFor[])
    {
        for (int i = 0; i < cChars; ++i)
        {
            if (KEY_DOWN_BIT & GetAsyncKeyState(rgLookFor[i]))
            {
                return rgLookFor[i];
            }
        }
        return 0;
    }

    int CheckForTouchInput()
    {
        if (KEY_DOWN_BIT & GetAsyncKeyState(VK_LBUTTON))    // if the screen is touched (same as left mouse botton is pressed down)
        {
            POINT cursorLocation;
            if (GetCursorPos(&cursorLocation))
            {
                if (cursorLocation.y<(GetSystemMetrics(SM_CYSCREEN)/2)) // upper half of the screen
                {
                    return UpperTap;  // Return UpperTap
                }
                else
                {
                    return LowerTap;  // Return LowerTap when lower half of the screen is tapped
                }
            }
        }
        return NoTap;  //if not tapped at all
    }
    ////////////////////////////////////////////////////////////////////////////////
    // VerifyOutput
    // prompts the user to verify the output matches the expected output.
    eTestResult VerifyOutput(BOOL bInteractive,
                                                bool bDelayed,
                                                IDirectDrawSurface *piDDS,
                                                TCHAR *Display,
                                                RECT *prcDest = NULL,
                                                bool bUpperLeftText = true
                                                )
    {
        eTestResult tr = trPass;
        const int iInstrWidth = 1024;
        TCHAR szInstructions[iInstrWidth]={NULL};
        int iWidth = 190;
        bool bTouchOnly;       // when the GetKeyboardStatus() bug is fixed, the boolean will be updated depending on the return value of calling GetKeyboardStatus() 
        errno_t  Error;

        if (GetKeyboardStatus())    //when GetKeyboardStatus return non-zero, we have hardware keyboard support
        {
            bTouchOnly=false;
        }
        else
        {
            bTouchOnly=true;
        }
        if(bInteractive || bDelayed)
        {
            // construct our instruction string
            Error = _tcsncpy_s (szInstructions,countof(szInstructions), Display, _TRUNCATE);
            szInstructions[iInstrWidth-1] = 0;
            if(Error != 0)
            {
                dbgout(LOG_FAIL) << "FAIL :VerifyOutput - Error in _tcsncpy_s" << endl;
            }
            if (bInteractive)
            {
                if (bTouchOnly)
                {
                    Error = _tcsncat_s (szInstructions,countof(szInstructions), TEXT("\nTap upper half of screen for Pass or lower half for Fail"),_TRUNCATE);
                }
                else
                {
                    Error = _tcsncat_s (szInstructions,countof(szInstructions), TEXT("\nPress '1' for Pass or '0' for Fail"), _TRUNCATE);
                }
            }
            else if(bDelayed)
            {
                Error = _tcsncat_s (szInstructions,countof(szInstructions), TEXT("\nSleeping for verification."), _TRUNCATE);
            }
            if(Error != 0)
            {
                dbgout(LOG_FAIL) << "FAIL :VerifyOutput - Error in _tcsncat_s" << endl;
    }
            tr = DisplayVerifyString(piDDS, szInstructions, bUpperLeftText);

            if (bInteractive)
            {
                BOOL bDone = FALSE;

                int rgLookFor[] = {
                    '0',
                    '1'
                };

                while (!bDone)
                {
                    int touchResult, keyResult;

                    touchResult = CheckForTouchInput();
                    keyResult = CheckForKeyboardInput(countof(rgLookFor), rgLookFor);

                    if (('1' == keyResult) || (UpperTap == touchResult))
                    {
                        dbgout(LOG_PASS) << "Pattern successfully displayed to screen" << endl;
                        bDone = TRUE;
                    }
                    else if (('0' == keyResult) ||(LowerTap == touchResult))
                    {
                        dbgout(LOG_FAIL) << "Pattern NOT displayed to screen" << endl;
                        tr |= trFail;
                        bDone = TRUE;
                    }
                    Sleep(0);
                }
                // sleep long enough so we don't double register this response from the user
                // Wait until the keys have been raised
                bDone = FALSE;
                int iWaitCount = 50;
                while (!bDone && iWaitCount)
                {
                    bDone = TRUE;
                    if (
                         (CheckForKeyboardInput(countof(rgLookFor), rgLookFor)) ||
                         (CheckForTouchInput())
                         )
                    {
                        bDone = FALSE;
                        Sleep(10);
                        --iWaitCount;
                    }
                }
                //Refill the pattern to remove previous text instruction and white background
                

                DDCOLORKEY ddcheck;  

                DWORD dwColor1;
                DWORD dwColor2;
                if (SUCCEEDED(piDDS->GetColorKey(DDCKEY_DESTOVERLAY, &ddcheck)))
                {
                    dwColor1= TestKit_Surface_Modifiers::Surface_Fill_Helpers::SetColorVal(0.5, 0,1, piDDS);    //purple for colorkey enabled case
                }
                else
                {
                    dwColor1= TestKit_Surface_Modifiers::Surface_Fill_Helpers::SetColorVal(1, 1,1, piDDS);
                }
                dwColor2= TestKit_Surface_Modifiers::Surface_Fill_Helpers::SetColorVal(0, 0,0, piDDS);


                tr |= TestKit_Surface_Modifiers::Surface_Fill_Helpers::FillSurfaceVertical(piDDS, dwColor1, dwColor2);
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

    eTestResult SetColorVal(double dwRed, double dwGreen,double dwBlue, IDirectDrawSurface *piDDS, DWORD *dwFillColor, RECT* prcFillRect = NULL)
    {
        using namespace TestKit_Surface_Modifiers::Surface_Fill_Helpers;
        using namespace DDrawUty::Surface_Helpers;

        CDDSurfaceDesc cddsd;
        HRESULT hr;
        DWORD *dwSrc=NULL;
        DWORD Y, U, V;
        CDDBltFX cddbltfx;

        LPRECT prcBoundRect = NULL;
        RECT rcBoundRect = {0, 0, 0, 0};

        hr = piDDS->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        // For a Primary in Windowed mode, use the Window's bounding rectangle in Lock, instead of NULL.
        if((cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
        {
            eTestResult tr = GetClippingRectangle(piDDS, rcBoundRect);
            if(trPass != tr)
            {
                dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
                return tr;
            }
            else
            {
                prcBoundRect = &rcBoundRect;
            }

            // Now, retrieve the surface description of the Window surface using a call to Lock, as GetSurfaceDesc
            // returns the surface description of the true primary.
            hr = piDDS->Lock(prcBoundRect, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
            if(FAILED(hr))
            {
                dbgout << "Failure locking primary surface. hr = " 
                       << g_ErrorMap[hr] << " (" << HEX((DWORD)hr) << "). "
                       << "GetLastError() = " << HEX((DWORD)GetLastError())
                       << endl;
                return trFail;
            }

            hr = piDDS->Unlock(prcBoundRect);
            if(FAILED(hr))
            {
                dbgout << "Failure unlocking primary surface. hr = " 
                       << g_ErrorMap[hr] << " (" << HEX((DWORD)hr) << "). "
                       << "GetLastError() = " << HEX((DWORD)GetLastError())
                       << endl;
                return trFail;
            }
        }

        if(cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC)
        {
            dwRed=(dwRed*0xFF);
            dwGreen=(dwGreen*0xFF);
            dwBlue=(dwBlue*0xFF);

            Y= (DWORD)(0.29*dwRed + 0.57*dwGreen + 0.14*dwBlue);
            U= (DWORD)(128.0 - 0.14*dwRed - 0.29*dwGreen+ 0.43*dwBlue);
            V= (DWORD)(128.0 + 0.36*dwRed - 0.29*dwGreen - 0.07*dwBlue);

            *dwFillColor = PackYUV(cddsd.ddpfPixelFormat.dwFourCC, Y, Y, U, V);
            dbgout << "Filling with " << HEX(*dwFillColor) << endl;

            if(FAILED(piDDS->Lock(prcBoundRect, &cddsd, DDLOCK_WAITNOTBUSY, NULL)))
            {
                dbgout << "failure locking Destination surface for manual fill" << endl;
                return trFail;
            }
            dwSrc=(DWORD*)cddsd.lpSurface;
            for(int y=0;y< static_cast<int> (cddsd.dwHeight);y++)
            {
                for(int x=0;x< static_cast<int>(cddsd.dwWidth) ;x++)
                {
                    if(IsWithinRect(x, y, prcFillRect))
                    {
                        WORD wCurrentFill = (*dwFillColor >> (16 * (x%2))) & 0xffff;
                        ((WORD*)((BYTE*)dwSrc + cddsd.lPitch*y + cddsd.lXPitch * x))[0]=wCurrentFill;
                    }
                }
            }
            piDDS->Unlock(prcBoundRect);
        }
        else if(cddsd.ddpfPixelFormat.dwFlags & DDPF_RGB)
        {
            *dwFillColor=cddbltfx.dwFillColor=((DWORD)(cddsd.ddpfPixelFormat.dwRBitMask * dwRed) & cddsd.ddpfPixelFormat.dwRBitMask)+
                                            ((DWORD)(cddsd.ddpfPixelFormat.dwGBitMask * dwGreen) & cddsd.ddpfPixelFormat.dwGBitMask)+
                                            ((DWORD)(cddsd.ddpfPixelFormat.dwBBitMask * dwBlue) & cddsd.ddpfPixelFormat.dwBBitMask);
            dbgout << "Filling with " << HEX(cddbltfx.dwFillColor) << endl;

            hr = piDDS->Blt(prcFillRect, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, &cddbltfx);
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

    // Fills the dwFillColor with the surface specific color value.
    eTestResult GetColorVal(double dRed, double dGreen, double dBlue, IDirectDrawSurface *piDDS, DWORD *dwFillColor)
    {
        using namespace TestKit_Surface_Modifiers::Surface_Fill_Helpers;

        CDDSurfaceDesc cddsd;
        HRESULT hr;
        DWORD *dwSrc=NULL;
        DWORD Y, U, V;
        CDDBltFX cddbltfx;

        hr = piDDS->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        if(cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC)
        {
            dRed=(dRed*0xFF);
            dGreen=(dGreen*0xFF);
            dBlue=(dBlue*0xFF);

            Y= (DWORD)(0.29*dRed + 0.57*dGreen + 0.14*dBlue);
            U= (DWORD)(128.0 - 0.14*dRed - 0.29*dGreen+ 0.43*dBlue);
            V= (DWORD)(128.0 + 0.36*dRed - 0.29*dGreen - 0.07*dBlue);

            *dwFillColor = PackYUV(cddsd.ddpfPixelFormat.dwFourCC, Y, Y, U, V);
        }
        else if(cddsd.ddpfPixelFormat.dwFlags & DDPF_RGB)
        {
            *dwFillColor=cddbltfx.dwFillColor = ((DWORD)(cddsd.ddpfPixelFormat.dwRBitMask * dRed) & cddsd.ddpfPixelFormat.dwRBitMask)+
                                                ((DWORD)(cddsd.ddpfPixelFormat.dwGBitMask * dGreen) & cddsd.ddpfPixelFormat.dwGBitMask)+
                                                ((DWORD)(cddsd.ddpfPixelFormat.dwBBitMask * dBlue) & cddsd.ddpfPixelFormat.dwBBitMask);
        }
        else
        {
            dbgout << "Unknown pixel format, ddpf.dwflags is " << cddsd.ddpfPixelFormat.dwFlags << endl;
            return trFail;
        }

        return trPass;
    }

    HRESULT PositionAndDisplayOverlay(IDirectDrawSurface * piDDSOverlay, IDirectDrawSurface * piDDSPrimary, const DDCAPS & ddcaps, const CDDSurfaceDesc & cddsd)
    {
        DWORD  dwAlignSizeSrc = 1;
        DWORD  dwAlignBoundaryDest = 1;

        RECT rcOverlayPosition;
        RECT rcOverlayCropping;

        // set the overlay width
        int nOverlayHeight = cddsd.dwHeight / 2,
             nOverlayWidth  = cddsd.dwWidth / 2;
        // set the overlay x and y top left positions
        int nDestX = cddsd.dwWidth / 4,
            nDestY = cddsd.dwHeight / 4;

        // set the overlay position/size
        int x = GetSystemMetrics(SM_CXSCREEN) - nOverlayWidth;
        int y = GetSystemMetrics(SM_CYSCREEN) - nOverlayHeight;
        SetRect(&rcOverlayPosition, nDestX, nDestY, nDestX + nOverlayWidth, nDestY + nOverlayHeight);
        SetRect(&rcOverlayCropping, 0, 0, nOverlayWidth, nOverlayHeight);
        priv_Overlay::RealignRects(&rcOverlayCropping, &rcOverlayPosition, &ddcaps);

        // If this is an overlay, make the overlay visible.
        return piDDSOverlay->UpdateOverlay(&rcOverlayCropping, piDDSPrimary, &rcOverlayPosition, DDOVER_SHOW, NULL);
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

        // Only test if this is a flipping surface
        if (m_cddsd.ddsCaps.dwCaps & DDSCAPS_FLIP)
        {
            // If this is a primary is system memory, then we can't lock
            BOOL fCanLock = !((m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
                              (m_cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY));
            const WORD wPrimary    = 0x1234,
                       wBackbuffer = 0x9ABC;
            CDDSurfaceDesc cddsd;
            HRESULT hr;

            if (fCanLock)
            {
                // Lock and write known data to the initial primary
                dbgout << "Writing to primary" << endl;
                hr = m_piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
                CheckHRESULT(hr, "Lock", trAbort);
                *(WORD*)cddsd.lpSurface = wPrimary;
                hr = m_piDDS->Unlock(NULL);
                CheckHRESULT(hr, "Unlock", trAbort);
            }

            // Flip to the first backbuffer
            dbgout << "Flipping to first backbuffer" << endl;
            hr = m_piDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
            CheckHRESULT(hr, "Flip", trFail);

            if (fCanLock)
            {
                // Lock and write known data to the first backbuffer
                dbgout << "Writing to first backbuffer" << endl;
                hr = m_piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
                CheckHRESULT(hr, "Lock", trAbort);
                *(WORD*)cddsd.lpSurface = wBackbuffer;
                hr = m_piDDS->Unlock(NULL);
                CheckHRESULT(hr, "Unlock", trAbort);
            }

            // Flip back to the original frontbuffer
            dbgout << "Flipping to primary" << endl;
            for (int i = 0; i < static_cast<int> (m_cddsd.dwBackBufferCount); i++)
            {
                dbgout << "Flip #" << i << endl;
                hr = m_piDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
                CheckHRESULT(hr, "Flip", trFail);
            }

            if (fCanLock)
            {
                // Verify the original data
                dbgout << "Verifying original primary data" << endl;
                hr = m_piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
                CheckHRESULT(hr, "Lock", trAbort);
                QueryCondition(wPrimary != *(WORD*)cddsd.lpSurface, "Expected Primary Data not found", tr |= trFail);
                hr = m_piDDS->Unlock(NULL);
                CheckHRESULT(hr, "Unlock", trAbort);
            }

            // Flip back to the original backbuffer without waiting
            dbgout << "Flipping to first backbuffer without DDFLIP_WAITNOTBUSY" << endl;
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
                hr = m_piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
                CheckHRESULT(hr, "Lock", trAbort);
                QueryCondition(wBackbuffer != *(WORD*)cddsd.lpSurface, "Expected Backbuffer Data not found", tr |= trFail);
                hr = m_piDDS->Unlock(NULL);
                CheckHRESULT(hr, "Unlock", trAbort);
            }

            // Do a few more flips for good measure...
            for (i = 0; i < 30; i++)
            {
                hr = m_piDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
                CheckHRESULT(hr, "Flip", trFail);
            }
        }
        else
            dbgout << "Skipping Surface" << endl;

        return tr;
    }
    namespace priv_FlipInterlacedHelpers
    {
        const DWORD FAE_RESET = (DWORD)(~(DDFLIP_ODD|DDFLIP_EVEN));
        bool FlipAsExpected(IDirectDrawSurface* pDDS, DWORD Flags, HRESULT hrFlipResult)
        {
            // If dwFlipNeededForSync is DDFLIP_EVEN, this means that DDFLIP_ODD will have no affect,
            // and DDFLIP_EVEN|DDFLIP_ODD and Flip with no flags will fail;
//            static DWORD dwFlipNeededForSync = 0;

            // Which buffer is the surface pointer going to Lock to?
            static int iExpectedFrontBuffer = 0;

            // How many buffers are in this chain
            int iBufferCount = 0;

            // Support for up to 4 backbuffers.
            const WORD wBufferKeys[] = {
                0xAABB,
                0xDDCC,
                0x5566,
                0x8118,
                0x9933
            };

            bool fIsInterlaced = false;
            bool fIsOddEvenSupported = (g_DDConfig.HALCaps().dwMiscCaps & DDMISCCAPS_FLIPODDEVEN);

            bool fIsCorrect = true;
            bool fFlipped = false;
            HRESULT hr;

            CDDSurfaceDesc cddsd;
            CDDSurfaceDesc cddsdLocked;

            // Get the surface description.
            hr = pDDS->GetSurfaceDesc(&cddsd);
            CheckHRESULT(hr, "GetSurfaceDesc", false);
            bool fIsOverlay = cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY;

            iBufferCount = cddsd.dwBackBufferCount + 1;

            DWORD dwTmp = 0;
            DDrawUty::g_DDConfig.GetSymbol(TEXT("l"), &dwTmp);
            fIsInterlaced = (dwTmp != 0) && fIsOddEvenSupported;

            // Check for reset
            if (FAE_RESET == Flags)
            {
//                // Make sure the surface is in sync
//                if (dwFlipNeededForSync && S_OK == hrFlipResult)
//                {
//                    hr = pDDS->Flip(NULL, DDFLIP_WAITNOTBUSY | dwFlipNeededForSync);
//                    CheckHRESULT(hr, "Flip to reset sync", false);
//                }

                // Clear out statics
//                dwFlipNeededForSync = 0;
                iExpectedFrontBuffer = 0;

                // For each buffer in flipping chain, lock and put an indicator on the surface
                for (int i = 0; i < iBufferCount; ++i)
                {
                    hr = pDDS->Lock(NULL, &cddsdLocked, DDLOCK_WAITNOTBUSY, NULL);
                    CheckHRESULT(hr, "Lock", false);
                    WORD * pw = (WORD*)cddsdLocked.lpSurface;
                    *pw = wBufferKeys[i];

                    pDDS->Unlock(NULL);

                    hr = pDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
                    CheckHRESULT(hr, "Flip", false);
                }
                // return
                return true;
            }


            if ((!fIsOverlay && (DDFLIP_ODD | DDFLIP_EVEN) & Flags))
            {
                fFlipped = false;
                QueryCondition(
                    DDERR_INVALIDPARAMS != hrFlipResult,
                    "Flip should have returned DDERR_INVALIDPARAMS with flags " << HEX(Flags) << ";",
                    fIsCorrect = false; fFlipped = true)
            }
            else
            {
                fFlipped = true;
                QueryHRESULT(
                    hrFlipResult,
                    "Flip failed when it should have succeeded: DDFLIP_ODD or EVEN should be ignored when flipping non-interlaced surfaces;",
                    fIsCorrect = false; fFlipped = false);
            }

            if (fFlipped)
            {
                ++iExpectedFrontBuffer;
                if (iExpectedFrontBuffer == iBufferCount)
                {
                    iExpectedFrontBuffer = 0;
                }
            }
//            // Check if a certain flip is needed for sync, and if the flip we're checking was the right one
//            if (!fIsInterlaced)
//            {
//            }
//            else if (dwFlipNeededForSync && (dwFlipNeededForSync != Flags))
//            {
//                // if not, verify that the hr is failed
//
//                // if both odd and even are successfully flipped and we're out of sync, fail
//                QueryCondition(
//                    SUCCEEDED(hrFlipResult) && (0 == Flags || (DDFLIP_EVEN | DDFLIP_ODD) == Flags),
//                    "Flip succeeded when it should have failed: odd and even are out of sync, cannot flip both in this case" << endl,
//                    fIsCorrect = false;++iExpectedFrontBuffer)
//                // if we are out of sync with one field ahead, and we flip that field again, the flip should succeed but not do anything.
//                else QueryCondition(
//                    FAILED(hrFlipResult) && (0 != Flags && (DDFLIP_EVEN | DDFLIP_ODD) != Flags),
//                    "Flip failed when it should have succeeded: odd and even are out of sync, flipping with " << Flags << " should be ignored" << endl,
//                    fIsCorrect = false);
//
//                // do not update the expected front buffer
//            }
//            else
//            {
//                // if so, verify that the hr is successful
//                QueryHRESULT(
//                    hrFlipResult,
//                    "Flip failed when it should have succeeded: odd and even were in sync, any flip should succeed" << endl,
//                    fIsCorrect = false);
//
//                // update expected front buffer
//                // (if the flip failed, then don't actually update these, so that the next time we are properly in sync)
//                if (fIsCorrect)
//                {
//                    if ((0 == Flags) || ((DDFLIP_ODD|DDFLIP_EVEN) == Flags) || (0 != dwFlipNeededForSync))
//                    {
//                        // front buffer changes with the ODD field, not the even.
//                        ++iExpectedFrontBuffer;
//                        if (iExpectedFrontBuffer == iBufferCount)
//                        {
//                            iExpectedFrontBuffer = 0;
//                        }
//                    }
//
//                    // update FlipNeededForSync
//                    if (dwFlipNeededForSync)
//                    {
//                        dwFlipNeededForSync = 0;
//                    }
//                    else
//                    {
//                        if (DDFLIP_ODD == Flags)
//                        {
//                            dwFlipNeededForSync = DDFLIP_EVEN;
//                        }
//                        else if (DDFLIP_EVEN == Flags)
//                        {
//                            dwFlipNeededForSync = DDFLIP_ODD;
//                        }
//                        // if Flags is neither or both, then we are in sync still
//                    }
//                }
//            }

            // Lock the surface and verify that the expected front buffer is the one we have.
            hr = pDDS->Lock(NULL, &cddsdLocked, DDLOCK_WAITNOTBUSY, NULL);
            QueryHRESULT(
                hr,
                "Lock failed when trying to verify correct front buffer" << endl,
                fIsCorrect = false)
            else
            {
                WORD * pwSurface = (WORD*)cddsdLocked.lpSurface;
                QueryCondition(
                    (iExpectedFrontBuffer < countof(wBufferKeys) && *pwSurface != wBufferKeys[iExpectedFrontBuffer]),
                    "Unexpected surface value; found " << HEX(*pwSurface) << ", expected " << HEX(wBufferKeys[iExpectedFrontBuffer]) << endl,
                    fIsCorrect = false);
            }
            pDDS->Unlock(NULL);
            return fIsCorrect;
        }

        eTestResult PerformInterlacedFlipVerify(IDirectDrawSurface* pDDS, DWORD InterlaceFlags)
        {
            const int MAX_RETRIES=100000;

            eTestResult tr = trPass;
            HRESULT hr;
            DWORD Flags = 0;
            dbgout << indent;           
            // determine which flags to use for flipping
            rand_s(&uNumber);
            if (uNumber & 1)
            {
                Flags |= DDFLIP_WAITNOTBUSY;
            }
            rand_s(&uNumber);
            if (uNumber & 1)
            {
                Flags |= DDFLIP_WAITVSYNC;
            }

            int iTries = MAX_RETRIES;
            do
            {
                hr = pDDS->Flip(NULL, Flags | InterlaceFlags);
                Sleep(1);
            } while(!(Flags & DDFLIP_WAITNOTBUSY) && --iTries && (DDERR_WASSTILLDRAWING == hr));

            QueryCondition(
                (!FlipAsExpected(pDDS, InterlaceFlags, hr)),
                "Flip didn't do the right thing, flags: " << HEX(Flags | InterlaceFlags),
                tr |= trFail)

            dbgout << unindent;

            return tr;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_FlipInterlaced::TestIDirectDrawSurface
    //  tests flipping for interlaced surfaces.
    eTestResult CTest_FlipInterlaced::TestIDirectDrawSurface()
    {
        using namespace priv_FlipInterlacedHelpers;
        eTestResult tr = trPass;
        eTestResult trTemp = trPass;
        HRESULT hr;
        CDDCaps ddcaps;
        CDirectDrawSurfacePrimary PrimarySingleton;
        

        m_piDD->GetCaps(&ddcaps, NULL);
        bool fIsOddEvenSupported = (ddcaps.dwMiscCaps & DDMISCCAPS_FLIPODDEVEN);

        bool fResetResult;

        // i will be used in any iterations
        int i;

        // Determine the visible extents of the surface (if the surface is an overlay,
        // use these same extents for displaying the surface later on)
        bool fIsOverlay = m_cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY;

        if (fIsOverlay)
        {
            // If this is an overlay, make the overlay visible.
            IDirectDrawSurface * piDDSPrimary = PrimarySingleton.GetObject().AsInParam();
            hr = PositionAndDisplayOverlay(m_piDDS.AsInParam(), piDDSPrimary, ddcaps, m_cddsd);
            CheckHRESULT(hr, "UpdateOverlay to the primary surface", trAbort);
        }

        if (!(m_cddsd.ddsCaps.dwCaps & DDSCAPS_FLIP) || !fIsOddEvenSupported)
        {
            DWORD dwFlags = 0;
            rand_s(&uNumber);
            if (uNumber & 1)
            {
                dwFlags |= DDFLIP_WAITNOTBUSY;
            }
            rand_s(&uNumber);
            if (uNumber & 1)
            {
                dwFlags |= DDFLIP_WAITVSYNC;
            }
            hr = m_piDDS->Flip(NULL, dwFlags | DDFLIP_EVEN);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL) << "Flip even succeeded on nonflipping surface: " << hr << endl;
                tr |= trFail;
            }

            hr = m_piDDS->Flip(NULL, dwFlags | DDFLIP_ODD);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL) << "Flip odd succeeded on nonflipping surface: " << hr << endl;
                tr |= trFail;
            }

            hr = m_piDDS->Flip(NULL, dwFlags | DDFLIP_ODD | DDFLIP_EVEN);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL) << "Flip odd|even succeeded on nonflipping surface: " << hr << endl;
                tr |= trFail;
            }
        }
        else if (fIsOddEvenSupported)
        {
            const int FLIP_COUNT = 10;
            const int FLIP_32PULLDOWNCOUNT = 100;
            const int FLIP_RANDOMCOUNT = 300;
            DWORD Flags = 0;
            fResetResult = FlipAsExpected(m_piDDS.AsInParam(), FAE_RESET, S_FALSE);
            QueryCondition(!fResetResult, "FlipAsExpected", tr |= trAbort);
            dbgout << "Testing Interlaced Flipping surface" << endl;

            dbgout << "Testing flipping with DDFLIP_ODD and DDFLIP_EVEN specified at the same time." << endl;
            // Test even and odd flipping
            // Flip both even and odd at same time
            for (i = 0; i < FLIP_COUNT; ++i)
            {
                trTemp = PerformInterlacedFlipVerify(m_piDDS.AsInParam(), 0);
                QueryCondition(
                    trPass != trTemp,
                    "Flip odd|even failed when it should have succeeded.",
                    tr |= trFail);
            }

            dbgout << "Testing with DDFLIP_EVEN multiple times" << endl;
            // Flip Even multiple times, checking after each flip
            for (i = 0; i < FLIP_COUNT; ++i)
            {
                trTemp = PerformInterlacedFlipVerify(m_piDDS.AsInParam(), DDFLIP_EVEN);
                QueryCondition(
                    trPass != trTemp,
                    "Flip even failed on flip " << i,
                    tr |= trFail);

                // Flip both even and odd should not work
                trTemp = PerformInterlacedFlipVerify(m_piDDS.AsInParam(), 0);
                QueryCondition(
                    trPass != trTemp,
                    "Flip odd|even failed on surface that was already flipped even",
                    tr |= trFail);

                // Flip neither even nor odd should not work
                trTemp = PerformInterlacedFlipVerify(m_piDDS.AsInParam(), 0);
                QueryCondition(
                    trPass != trTemp,
                    "Flip failed on surface that was already flipped even",
                    tr |= trFail);

            }

            // After flipping Even a number of times, flip odd.
            trTemp = PerformInterlacedFlipVerify(m_piDDS.AsInParam(), DDFLIP_ODD);
            QueryCondition(
                trPass != trTemp,
                "Flip odd failed after flipping even",
                tr |= trFail);

                // Flip both even and odd should work
            trTemp = PerformInterlacedFlipVerify(m_piDDS.AsInParam(), 0);
            QueryCondition(
                trPass != trTemp,
                "Flip odd|even failed when it should have succeeded",
                tr |= trFail);

                // Flip neither even nor odd should work
            trTemp = PerformInterlacedFlipVerify(m_piDDS.AsInParam(), 0);
            QueryCondition(
                trPass != trTemp,
                "Flip failed when it should have succeeded",
                tr |= trFail);

            fResetResult = FlipAsExpected(m_piDDS.AsInParam(), FAE_RESET, S_OK);
            QueryCondition(!fResetResult, "FlipAsExpected", tr |= trAbort);

            dbgout << "Testing with DDFLIP_ODD multiple times" << endl;
            // Flip Odd multiple times same as even
            for (i = 0; i < FLIP_COUNT; ++i)
            {
                trTemp = PerformInterlacedFlipVerify(m_piDDS.AsInParam(), DDFLIP_ODD);
                QueryCondition(
                    trPass != trTemp,
                    "Flip odd failed on flip " << i,
                    tr |= trFail);

                // Flip both even and odd should not work
                trTemp = PerformInterlacedFlipVerify(m_piDDS.AsInParam(), 0);
                QueryCondition(
                    trPass != trTemp,
                    "Flip odd|even failed on surface that was already flipped odd",
                    tr |= trFail);

                // Flip neither even nor odd should not work
                trTemp = PerformInterlacedFlipVerify(m_piDDS.AsInParam(), 0);
                QueryCondition(
                    trPass != trTemp,
                    "Flip failed on surface that was already flipped odd",
                    tr |= trFail);
            }

            // After flipping Odd a number of times, flip Even.
            trTemp = PerformInterlacedFlipVerify(m_piDDS.AsInParam(), DDFLIP_EVEN);
            QueryCondition(
                trPass != trTemp,
                "Flip even failed after flipping odd",
                tr |= trFail);

//                // Flip both even and odd should work
//            trTemp = PerformInterlacedFlipVerify(m_piDDS.AsInParam(), 0);
//            QueryCondition(
//                trPass != trTemp,
//                "Flip odd|even failed when it should have succeeded",
//                tr |= trFail);

                // Flip neither even nor odd should work
            trTemp = PerformInterlacedFlipVerify(m_piDDS.AsInParam(), 0);
            QueryCondition(
                trPass != trTemp,
                "Flip failed when it should have succeeded",
                tr |= trFail);

            fResetResult = FlipAsExpected(m_piDDS.AsInParam(), FAE_RESET, S_OK);
            QueryCondition(!fResetResult, "FlipAsExpected", tr |= trAbort);

            dbgout << "Testing 3:2 pulldown (odd, even, even, odd, even, odd, odd, even, odd, even, even, etc)" << endl;
            // Simulate 3:2 pulldown: Flip odd, even, even, odd even odd odd even odd even even etc. for lots of flips
            DWORD rg32FlipFlags[] = {
                DDFLIP_ODD,
                DDFLIP_EVEN,
                DDFLIP_EVEN,
                DDFLIP_ODD,
            };
            for (i = 0; i < countof(rg32FlipFlags) * FLIP_32PULLDOWNCOUNT; ++i)
            {
                trTemp = PerformInterlacedFlipVerify(m_piDDS.AsInParam(), rg32FlipFlags[i % countof(rg32FlipFlags)]);
                QueryCondition(
                    trPass != trTemp,
                    "Flip failed with 3:2 pulldown simulation on iteration " << i,
                    tr |= trFail);
            }

            fResetResult = FlipAsExpected(m_piDDS.AsInParam(), FAE_RESET, S_OK);
            QueryCondition(!fResetResult, "FlipAsExpected", tr |= trAbort);

            dbgout << "Testing with random Interlace flags" << endl;
            // Perform large number of random flips, checking after each that values are as expected.
            DWORD rgRandomFlipFlags[] = {
                0,
                DDFLIP_ODD,
                DDFLIP_EVEN,
            };
            for (i = 0; i < FLIP_RANDOMCOUNT; ++i)
            {
                rand_s(&uNumber);
                int iRandIndex = uNumber%countof(rgRandomFlipFlags);
                trTemp = PerformInterlacedFlipVerify(m_piDDS.AsInParam(), rgRandomFlipFlags[iRandIndex]);
                QueryCondition(
                    trPass != trTemp,
                    "Flip did not perform as expected with random flags: " << HEX(rgRandomFlipFlags[iRandIndex]),
                    tr |= trFail);
            }
        }

        if (fIsOverlay)
        {
            IDirectDrawSurface *piDDSPrimary = PrimarySingleton.GetObject().AsInParam();
            m_piDDS->UpdateOverlay(NULL, piDDSPrimary, NULL, DDOVER_HIDE, NULL);
            PrimarySingleton.ReleaseObject();
        }
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
        DWORD  dwHALCKeyCaps;

        hr = m_piDDS->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        GetBltCaps(cddsd, cddsd, GBC_HAL, &dwHALBltCaps, &dwHALCKeyCaps);

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

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_SetGetPixel::TestIDirectDrawSurface
    //  tests colorfilling on standard surfaces.
    //
    eTestResult CTest_SetGetPixel::TestIDirectDrawSurface()
    {
        using namespace DDrawUty::Surface_Helpers;
        eTestResult tr = trPass;
        HRESULT hr;

        CDDSurfaceDesc cddsd;
        HDC hdcSurface;
        COLORREF crPixelBlack, crPixelWhite, crPixelReturned;

        LPRECT prcBoundRect = NULL;
        RECT rcBoundRect = {0, 0, m_cddsd.dwWidth, m_cddsd.dwHeight};
        DWORD dwFirstPixelX = 0, dwFirstPixelY = 0;

        // For a Primary in Windowed mode, use the Window's bounding rectangle in Lock, instead of NULL.
        if((m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
        {
            tr = GetClippingRectangle(m_piDDS.AsInParam(), rcBoundRect);
            if(trPass != tr)
            {
                dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
                return tr;
            }
            else
            {
                prcBoundRect = &rcBoundRect;
                dwFirstPixelY = rcBoundRect.top;
                dwFirstPixelX = rcBoundRect.left;
            }
        }
        else
        {
            // Remove the clipper from the non-primary surface (if there is one attached).
            m_piDDS->SetClipper(NULL);
        }

        hr = m_piDDS->GetDC(&hdcSurface);
        if (NULL == hdcSurface)
        {
            // If we can't get the surface's DC, skip.
            dbgout << "Could not obtain DC for surface" << endl;
            return trPass;
        }

        // First, set the first pixel to black.
        dbgout << "Setting first pixel to black" << endl;
        crPixelBlack = SetPixel(hdcSurface, dwFirstPixelX, dwFirstPixelY, RGB(0,0,0));
        if (-1 == static_cast<int> (crPixelBlack))
        {
            tr|=trFail;
            dbgout(LOG_FAIL) << "SetPixel failed, GetLastError returns " <<
                GetLastError() << endl;
        }

        // Get the pixel, verify that the color returned is black.
        dbgout << "Checking pixel with GetPixel" << endl;
        if ((crPixelReturned = GetPixel(hdcSurface, dwFirstPixelX, dwFirstPixelY)) != crPixelBlack)
        {
            tr|=trFail;
            dbgout(LOG_FAIL) << "GetPixel did not return the correct pixel color (got " <<
                HEX(crPixelReturned) << ", expected " << HEX(crPixelBlack) << endl;
        }

        // Release the DC
        hr = m_piDDS->ReleaseDC(hdcSurface);

        // Lock the surface, verify that the first pixel is black.
        dbgout << "Verifying that the locked surface has black as first pixel" << endl;
        hr = m_piDDS->Lock(prcBoundRect, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
        CheckHRESULT(hr, "Lock", trAbort);


        for (int iByte = 0; iByte <static_cast<int>  (cddsd.ddpfPixelFormat.dwRGBBitCount / 8); iByte++)
        {
            if (0x00 != ((BYTE*)cddsd.lpSurface)[iByte])
            {
                tr|=trFail;
                dbgout(LOG_FAIL) << "After SetPixel(black), byte " << iByte
                    << " of first pixel was " << ((BYTE*)cddsd.lpSurface)[iByte]
                    << endl;
            }
        }

        m_piDDS->Unlock(prcBoundRect);

        hr = m_piDDS->GetDC(&hdcSurface);
        if((S_OK != hr) || (NULL == hdcSurface))
        {
            // If we can't get the surface's DC, skip.
            dbgout << "Could not obtain DC for surface" << endl;
            return trPass;
        }

        // First, set the first pixel to white.
        dbgout << "Setting first pixel to white" << endl;
        crPixelWhite = SetPixel(hdcSurface, dwFirstPixelX, dwFirstPixelY, RGB(255,255,255));

        // Get the pixel, verify that the color returned is what we set.
        dbgout << "Checking pixel with GetPixel" << endl;
        if ((crPixelReturned = GetPixel(hdcSurface, dwFirstPixelX, dwFirstPixelY)) != crPixelWhite)
        {
            tr|=trFail;
            dbgout(LOG_FAIL) << "GetPixel did not return the correct pixel color (got " <<
                HEX(crPixelReturned) << ", expected " << HEX(crPixelWhite) << endl;
        }

        if (crPixelWhite == crPixelBlack)
        {
            tr|=trFail;
            dbgout(LOG_FAIL) << "SetPixel(black) and SetPixel(white) set the pixel to the same color: "
                << HEX(crPixelWhite) << endl;
        }

        // Release the DC
        hr = m_piDDS->ReleaseDC(hdcSurface);
        // Lock the surface, verify that the first pixel is black.

        dbgout << "Verifying that the locked surface does not have black as first pixel" << endl;
        hr = m_piDDS->Lock(prcBoundRect, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
        CheckHRESULT(hr, "Lock", trAbort);

        // We cannot check that all bytes are non-zero, because if there is an alpha byte
        // or an unused byte, it could be validly zero.
        int iZeroCount = 0;
        for (int iByte = 0; iByte < static_cast<int> (cddsd.ddpfPixelFormat.dwRGBBitCount / 8); iByte++)
        {
            dbgout << "After SetPixel(white), byte " << iByte + 1
                << " of first pixel was " << ((BYTE*)cddsd.lpSurface)[iByte]
                << endl;
            if (0x00 == ((BYTE*)cddsd.lpSurface)[iByte])
            {
                ++iZeroCount;
            }
        }
        if (iZeroCount == iByte)
        {
            tr|=trFail;
            dbgout(LOG_FAIL) << "All bytes were zero, implying pixel is still black"
                << endl;
        }

        m_piDDS->Unlock(prcBoundRect);
        return tr;
    }
};

namespace Test_IDirectDrawSurface_TWO
{
    // Generates dwNumRandRects Src & Destination rectangle pairs.
    // It returns the following
    // - Full Surface Source rectangle, Full Surface Destination rectangle
    // - Random rects within the Source and Destination bounds
    void GenerateTestRects(vector<RECT>* rgrcSrcRects, vector<RECT>* rgrcDstRects, const RECT& rcSrcBound, const RECT& rcDstBound, DWORD dwNumRandRects)
    {
        using namespace DDrawUty::Surface_Helpers;

        if(!rgrcSrcRects || !rgrcDstRects)
        {
            return;
        }

        RECT rcSrcRect = rcSrcBound;
        RECT rcDstRect = rcDstBound;

        RECT rcSrcBoundRect = rcSrcBound;
        RECT rcDstBoundRect = rcDstBound;

        // Full surface
        rgrcSrcRects->push_back(rcSrcRect);
        rgrcDstRects->push_back(rcDstRect);

        // Rects within the source / destination.
        for(int i=0; i<static_cast<int> (dwNumRandRects-1); ++i)
        {
            GenRandRect(&rcSrcBoundRect, &rcSrcRect);
            GenRandRect(&rcDstBoundRect, &rcDstRect);

            rgrcSrcRects->push_back(rcSrcRect);
            rgrcDstRects->push_back(rcDstRect);
        }
    }

    // Returns the Surface Desc and Bounding rectangle for surface
    eTestResult GetSurfaceDescAndBound(IDirectDrawSurface* pidds, LPDDSURFACEDESC pddsd, LPRECT prcBoundRect)
    {
        using namespace DDrawUty::Surface_Helpers;

        if(!pidds || !pddsd || !prcBoundRect)
        {
            return trAbort;
        }

        memset(pddsd, 0, sizeof(DDSURFACEDESC));
        pddsd->dwSize = sizeof(DDSURFACEDESC);


        HRESULT hr = pidds->GetSurfaceDesc(pddsd);
        if(S_OK != hr)
        {
            dbgout(LOG_ABORT) << "GetSurfaceDesc Failed" << endl;
            return trAbort;
        }

        prcBoundRect->top = 0;
        prcBoundRect->left = 0;
        prcBoundRect->bottom = pddsd->dwHeight;
        prcBoundRect->right = pddsd->dwWidth;

        // if a clipping rectangle has been set on the surface, retrieve it. Ignore the return value.
        GetClippingRectangle(pidds, *prcBoundRect);

        if((pddsd->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
           (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL)
          ) // For a Windowed Primary's, get the actual surface desc using Lock.
        {
            hr = pidds->Lock(prcBoundRect, pddsd, DDLOCK_WAITNOTBUSY, NULL);
            if(DDERR_SURFACELOST == hr) // try restoring the surface, if lost
            {
                dbgout << "Surface has been lost, try restoring it and Locking it." << endl;

                hr = pidds->Restore();
                CheckHRESULT(hr, "Restore", trAbort);

                hr = pidds->Lock(prcBoundRect, pddsd, DDLOCK_WAITNOTBUSY, NULL);
            }
            CheckHRESULT(hr, "Lock", trAbort);

            hr = pidds->Unlock(prcBoundRect);
            CheckHRESULT(hr, "Unlock", trAbort);
        }

        return trPass;
    }

    // Performs tests on the Blt API with rotations.
    // - Generates random rectangle pairs for the source and destination surfaces. (By default, 10 rectangle pairs will be used)
    // - Paints both the Src and Dest surfaces Black.
    // - For each source/destination rectangle pair
    //   - Paint the top left quarter of the Src Rect Blue
    //   - Paint the bottom left quarter of the Src Rect Yellow
    //   - Paint the top right quarter of the Src Rect Red
    //   - Paint the bottom right quarter of the Src Rect White
    //   - Do the following for 90, 180 & 270 degree Rotated Blts
    //     - Rotate Blt from Src Rect on the Src Surface to the Dest Rect on the Dest Surface
    //     - Calculate the Destination rectangles where Yellow, Blue, Red & White should be found.
    //     - Verify that the Destination rectangles contain the colors in the correct portions of the Dst rectangle.
    eTestResult RotatedBltTests(bool bTestAlphaBlt, IDirectDrawSurface* piddsSrc, IDirectDrawSurface* piddsDst, DWORD dwNumRandRects = 10)
    {
        using namespace DDrawUty::Surface_Helpers;

        if(!piddsSrc || !piddsDst)
        {
            dbgout(LOG_ABORT) << "Invalid surface pointers" << endl;
            return trAbort;
        }

        eTestResult tr = trPass;
        HRESULT hr = S_OK;
        DDSURFACEDESC ddsdSrc;
        DDSURFACEDESC ddsdDst;
        RECT rcSrcBoundRect;
        RECT rcDstBoundRect;
        DDALPHABLTFX ddabltfx;
        memset(&ddabltfx, 0, sizeof(ddabltfx));
        ddabltfx.dwSize = sizeof(ddabltfx);
        memset(&(ddabltfx.ddargbScaleFactors), 0xFF, sizeof(DDARGB));
        DWORD dwAlphaBltFlags = DDABLT_DDFX | DDBLT_WAITNOTBUSY;
        DWORD dwAlpha = 0xff;
        UINT uNumber1 = 0;
        rand_s(&uNumber1);
        // Get the surface descriptions for the surfaces.
        tr = GetSurfaceDescAndBound(piddsSrc, &ddsdSrc, &rcSrcBoundRect);
        if(trPass != tr)
        {
            dbgout(LOG_ABORT) << "Unable to retrieve Surface Desc and Bound for Source Surface" << endl;
            return trAbort;
        }

        tr = GetSurfaceDescAndBound(piddsDst, &ddsdDst, &rcDstBoundRect);
        if(trPass != tr)
        {
            dbgout(LOG_ABORT) << "Unable to retrieve Surface Desc and Bound for Destination Surface" << endl;
            return trAbort;
        }

        // Get a list of random rectangles for the source and destination surfaces.
        vector<RECT> rgrcSrcRects;
        vector<RECT> rgrcDstRects;
        GenerateTestRects(&rgrcSrcRects, &rgrcDstRects, rcSrcBoundRect, rcDstBoundRect, dwNumRandRects);

        DWORD dwSrcBlue = 0;
        DWORD dwSrcYellow = 0;
        DWORD dwSrcWhite = 0;
        DWORD dwSrcRed = 0;
        RECT rcSrcRect = {0, 0, 0, 0};
        RECT rcDstRect = {0, 0, 0, 0};
        DDBLTFX ddbltfx;
        DWORD dwSrcBlack = 0;
        DWORD dwDstBlack = 0;
        for(int i=0; (i< static_cast<int>(rgrcSrcRects.size())) && (trPass == tr); ++i) // For each random rectangle
        {
            // Reset the ddbltfx structure
            memset(&ddbltfx, 0, sizeof(ddbltfx));
            ddbltfx.dwSize = sizeof(ddbltfx);
            ddbltfx.dwFillColor = 0;

            // Paint the surfaces Black.
            tr = SetColorVal(0.0, 0.0, 0.0, piddsSrc, &dwSrcBlack);
            if(trPass != tr)
            {
                dbgout(LOG_ABORT) << "Unable to fill Source Surface with Black" << endl;
                return trAbort;
            }

            tr = SetColorVal(0.0, 0.0, 0.0, piddsDst, &dwDstBlack);
            if(trPass != tr)
            {
                dbgout(LOG_ABORT) << "Unable to fill Destination Surface with Black" << endl;
                return trAbort;
            }

            // Get the next pair of src & dst rects
            rcSrcRect = rgrcSrcRects[i];
            rcDstRect = rgrcDstRects[i];

            // Only verify rectangles with height and width of atleast 4 pixels.
            if(((rcSrcRect.bottom - rcSrcRect.top) < 4) || ((rcSrcRect.right - rcSrcRect.left) < 4)
                ||
               ((rcDstRect.bottom - rcDstRect.top) < 4) || ((rcDstRect.right - rcDstRect.left) < 4)
              )
            {
                // Exercise Blt with small rectangles but do not verify exact pixel values..
                hr = piddsDst->Blt(&rcDstRect, piddsSrc, &rcSrcRect, DDBLT_WAITNOTBUSY, &ddbltfx);
                CheckHRESULT(hr, "Rotated Blt From Src to Destination", trFail);

                memset(&(ddabltfx.ddargbScaleFactors), 0xFF, sizeof(DDARGB));
                hr = piddsDst->AlphaBlt(&rcDstRect, piddsSrc, &rcSrcRect, DDABLT_NOBLEND | DDBLT_WAITNOTBUSY, &ddabltfx);
                CheckHRESULT(hr, "Rotated AlphaBlt From Src to Destination", trFail);

                continue;
            }

            dbgout << "Source Rect : " << rcSrcRect << endl;
            dbgout << "Destination Rect : " << rcDstRect << endl;

            RECT rcTopLeftRect = {0,0,0,0};
            RECT rcBottomLeftRect = {0,0,0,0};
            RECT rcTopRightRect = {0,0,0,0};
            RECT rcBottomRightRect = {0,0,0,0};

            // Test the rotated blt Blt API with 90, 180 & 270 rotations.
            memset(&ddbltfx, 0, sizeof(ddbltfx));
            ddbltfx.dwSize = sizeof(ddbltfx);
            DWORD rgdwRotationFlags[] = {DDBLTFX_ROTATE90, DDBLTFX_ROTATE180, DDBLTFX_ROTATE270};
            for(int nextAngle=0; (nextAngle<sizeof(rgdwRotationFlags)/sizeof(DWORD)) && (trPass == tr); ++nextAngle)
            {
                // Repaint the Src rect on each iteration if the Src and Dst surfaces are the same.
                // Else do it only on the first iteration.
                if((piddsDst == piddsSrc) || (0 == nextAngle))
                {
                    // Paint the top left quarter of the Src rect Blue
                    rcTopLeftRect = rcSrcRect;
                    rcTopLeftRect.bottom = rcSrcRect.top + (rcSrcRect.bottom - rcSrcRect.top)/2;
                    rcTopLeftRect.right = rcSrcRect.left + (rcSrcRect.right - rcSrcRect.left)/2;
                    tr = SetColorVal(0.0, 0.0, 1.0, piddsSrc, &dwSrcBlue, &rcTopLeftRect);
                    if(trPass != tr)
                    {
                        dbgout(LOG_ABORT) << "Failed filling the top left quarter of the Src rectangle : " << rcTopLeftRect << endl;
                        return trAbort;
                    }

                    // Paint the bottom left quarter of the Src rect Yellow
                    rcBottomLeftRect = rcSrcRect;
                    rcBottomLeftRect.top = rcTopLeftRect.bottom;
                    rcBottomLeftRect.right = rcTopLeftRect.right;
                    tr = SetColorVal(1.0, 1.0, 0.0, piddsSrc, &dwSrcYellow, &rcBottomLeftRect);
                    if(trPass != tr)
                    {
                        dbgout(LOG_ABORT) << "Failed filling the bottom left quarter of the Src rectangle : " << rcBottomLeftRect << endl;
                        return trAbort;
                    }

                    // Paint the top right quarter of the Src rect Red
                    rcTopRightRect = rcSrcRect;
                    rcTopRightRect.bottom = rcTopLeftRect.bottom;
                    rcTopRightRect.left = rcTopLeftRect.right;
                    tr = SetColorVal(1.0, 0.0, 0.0, piddsSrc, &dwSrcRed, &rcTopRightRect);
                    if(trPass != tr)
                    {
                        dbgout(LOG_ABORT) << "Failed filling the top right quarter of the Src rectangle : " << rcTopRightRect << endl;
                        return trAbort;
                    }

                    // Paint the bottom right quarter of the Src rect White
                    rcBottomRightRect = rcSrcRect;
                    rcBottomRightRect.top = rcTopLeftRect.bottom;
                    rcBottomRightRect.left = rcTopLeftRect.right;
                    tr = SetColorVal(1.0, 1.0, 1.0, piddsSrc, &dwSrcWhite, &rcBottomRightRect);
                    if(trPass != tr)
                    {
                        dbgout(LOG_ABORT) << "Failed filling the bottom right quarter of the Src rectangle : " << rcBottomRightRect << endl;
                        return trAbort;
                    }
                }

                // By now we have a Source surface with the four quarters of rcSrcRect
                // filled with blue,yellow,red and white.

                rcSrcRect = rgrcSrcRects[i];
                rcDstRect = rgrcDstRects[i];

                // If source and destination are not the same, clear the destination rectangle.
                if(piddsDst != piddsSrc)
                {
                    tr = SetColorVal(0.0, 0.0, 0.0, piddsDst, &dwDstBlack, &rcDstRect);
                    if(trPass != tr)
                    {
                        dbgout(LOG_ABORT) << "Failed clearing the Dst rectangle : " << rcDstRect << endl;
                        return trAbort;
                    }
                }

                dbgout << "Rotation : " << (nextAngle*90) + 90 << endl;

                dwAlpha = 0xFF; // Treat a Blt as an AlphaBlt with an opaque alpha.

                // APIs Under Test
                if(!bTestAlphaBlt)
                {
                    ddbltfx.dwDDFX = rgdwRotationFlags[nextAngle];
                    hr = piddsDst->Blt(&rcDstRect, piddsSrc, &rcSrcRect, DDBLT_DDFX | DDBLT_WAITNOTBUSY, &ddbltfx);
                    CheckHRESULT(hr, "Rotated Blt From Src to Destination", trFail);
                }
                else
                {
                    // Initialize the scale factors.
                    memset(&(ddabltfx.ddargbScaleFactors), 0xFF, sizeof(DDARGB));

                    // Switch off the DDABLT_NOBLEND flag randomly.
                    rand_s(&uNumber);
                    if(uNumber%2)
                    {
                        dwAlphaBltFlags &= ~DDABLT_NOBLEND;

                        // Choose a random alpha
                        rand_s(&uNumber);
                        ddabltfx.ddargbScaleFactors.alpha = static_cast<BYTE> (dwAlpha = uNumber%256);
                        rand_s(&uNumber);
                        if(uNumber%3)
                        {
                            ddabltfx.ddargbScaleFactors.alpha = static_cast<BYTE>(dwAlpha = 0);
                        }                        

                        else if(uNumber1%5)
                        {
                            ddabltfx.ddargbScaleFactors.alpha = static_cast<BYTE>(dwAlpha = 0xff);
                        }

                        dbgout << "AlphaBlt Flag used : ~DDABLT_NOBLEND with Alpha = " << dwAlpha << endl;
                    }
                    else
                    {
                        dwAlphaBltFlags |= DDABLT_NOBLEND;
                        dbgout << "AlphaBlt Flag used : DDABLT_NOBLEND" << endl;
                    }

                    ddabltfx.dwDDFX = rgdwRotationFlags[nextAngle];
                    hr = piddsDst->AlphaBlt(&rcDstRect, piddsSrc, &rcSrcRect, dwAlphaBltFlags, &ddabltfx);
                    CheckHRESULT(hr, "Rotated AlphaBlt From Src to Destination", trFail);
                }

                // Wait till GetBltStatus(DDGBS_ISBLITDONE) returns DD_OK.
                while(1)
                {
                    hr = piddsDst->GetBltStatus(DDGBS_ISBLTDONE);
                    if(DDERR_WASSTILLDRAWING == hr)
                    {
                        Sleep(0);
                    }
                    else if(DD_OK == hr)
                    {
                        break;
                    }
                    else
                    {
                        CheckHRESULT(hr, "GetBltStatus", trFail);
                    }
                }

                // Verify that the colors on the Dst are correct.

                // Expected results (for the non blended case):
                //      90          180           270
                // -----------  -----------   -----------
                // |    |    |  |    |    |   |    |    |
                // |  R | W  |  |  W |  Y |   |  Y | B  |
                // -----------  -----------   -----------
                // |    |    |  |    |    |   |    |    |
                // |  B | Y  |  |  R |  B |   |  W | R  |
                // -----------  -----------   -----------
                // Calculate the quarters of the rectangle based on the rotation angle.
                // Leave out the one pixel border for each half, as a stretched Blt could Blt either color there.
                RECT rcRectWithBlue = rcDstRect;
                RECT rcRectWithYellow = rcDstRect;
                RECT rcRectWithRed = rcDstRect;
                RECT rcRectWithWhite = rcDstRect;

                DWORD dwDstRectWidth = (rcDstRect.right - rcDstRect.left);
                DWORD dwDstRectHeight = (rcDstRect.bottom - rcDstRect.top);

                // Fraction of the top half
                double dTopFraction = (double)(rcTopLeftRect.bottom - rcTopLeftRect.top)/(double)(rcSrcRect.bottom - rcSrcRect.top);
                // Fraction of the left half
                double dLeftFraction = (double)(rcTopLeftRect.right - rcTopLeftRect.left)/(double)(rcSrcRect.right - rcSrcRect.left);

                switch(rgdwRotationFlags[nextAngle])
                {
                case(DDBLTFX_ROTATE90):
                    {
                        // Red to the top left quarter
                        rcRectWithRed.right = rcDstRect.left + dwDstRectWidth*dTopFraction;
                        rcRectWithRed.bottom = rcDstRect.top + dwDstRectHeight*(1.0 - dLeftFraction);

                        // Blue to the bottom left quarter
                        rcRectWithBlue.top = rcRectWithRed.bottom;
                        rcRectWithBlue.right = rcRectWithRed.right;

                        // White to the top right quarter
                        rcRectWithWhite.left = rcRectWithRed.right;
                        rcRectWithWhite.bottom = rcRectWithRed.bottom;

                        // Yellow to the bottom right quarter
                        rcRectWithYellow.left = rcRectWithRed.right;
                        rcRectWithYellow.top = rcRectWithRed.bottom;

                        // Leave the one pixel border in the middle out of the verification.
                        --rcRectWithRed.right;
                        --rcRectWithRed.bottom;

                        ++rcRectWithWhite.left;
                        --rcRectWithWhite.bottom;

                        --rcRectWithBlue.right;
                        ++rcRectWithBlue.top;

                        ++rcRectWithYellow.left;
                        ++rcRectWithYellow.top;

                        break;
                    }
                case(DDBLTFX_ROTATE180):
                    {
                        // White to the top left quarter
                        rcRectWithWhite.right = rcDstRect.left + dwDstRectWidth*(1.0 - dLeftFraction);
                        rcRectWithWhite.bottom = rcDstRect.top + dwDstRectHeight*(1.0 - dTopFraction);

                        // Red to the bottom left quarter
                        rcRectWithRed.top = rcRectWithWhite.bottom;
                        rcRectWithRed.right = rcRectWithWhite.right;

                        // Yellow to the top right quarter
                        rcRectWithYellow.left = rcRectWithWhite.right;
                        rcRectWithYellow.bottom = rcRectWithWhite.bottom;

                        // Blue to the bottom right quarter
                        rcRectWithBlue.left = rcRectWithWhite.right;
                        rcRectWithBlue.top = rcRectWithWhite.bottom;

                        // Leave the one pixel border in the middle out of the verification.
                        --rcRectWithWhite.right;
                        --rcRectWithWhite.bottom;

                        ++rcRectWithBlue.left;
                        ++rcRectWithBlue.top;

                        --rcRectWithRed.right;
                        ++rcRectWithRed.top;

                        ++rcRectWithYellow.left;
                        --rcRectWithYellow.bottom;

                        break;
                    }
                case(DDBLTFX_ROTATE270):
                    {
                        // Yellow to the top left quarter
                        rcRectWithYellow.right = rcDstRect.left + dwDstRectWidth*(1.0 - dTopFraction);
                        rcRectWithYellow.bottom = rcDstRect.top + dwDstRectHeight*dLeftFraction;

                        // White to the bottom left quarter
                        rcRectWithWhite.top = rcRectWithYellow.bottom;
                        rcRectWithWhite.right = rcRectWithYellow.right;

                        // Blue to the top right quarter
                        rcRectWithBlue.left = rcRectWithYellow.right;
                        rcRectWithBlue.bottom = rcRectWithYellow.bottom;

                        // Red to the bottom right quarter
                        rcRectWithRed.left = rcRectWithYellow.right;
                        rcRectWithRed.top = rcRectWithYellow.bottom;

                        // Leave the one pixel border in the middle out of the verification.
                        --rcRectWithYellow.right;
                        --rcRectWithYellow.bottom;

                        ++rcRectWithRed.left;
                        ++rcRectWithRed.top;

                        --rcRectWithWhite.right;
                        ++rcRectWithWhite.top;

                        ++rcRectWithBlue.left;
                        --rcRectWithBlue.bottom;

                        break;
                    }
                default:
                    {
                        break;
                    }
                }

                dbgout << "Yellow is expected in : " << rcRectWithYellow << endl;
                dbgout << "Blue is expected in : " << rcRectWithBlue << endl;
                dbgout << "Red is expected in : " << rcRectWithRed << endl;
                dbgout << "White is expected in : " << rcRectWithWhite << endl;

                // Verify that the rects are filled with the correct colors.
                CDDrawSurfaceVerify cddsvFillVerifier;

                // Get the color values for the source rect
                DWORD dwDstBlue = 0;
                DWORD dwDstYellow = 0;
                DWORD dwDstWhite = 0;
                DWORD dwDstRed = 0;

                // Get the color values for the destination surface.
                // dwAlpha would be 0xFF while testing Blt & may have a random value while testing AlphaBlt(with blending)
                // Also, the destination rectangle will be cleared with black on each iteration.
                tr = GetColorVal(0.0, 0.0, (double)dwAlpha/(double)0xff, piddsDst, &dwDstBlue);
                if(trPass != tr)
                {
                    dbgout(LOG_ABORT) << "Unable to retrieve the destination surface's blue pixel value" << endl;
                    return trAbort;
                }

                tr = GetColorVal((double)dwAlpha/(double)0xff, (double)dwAlpha/(double)0xff, 0.0, piddsDst, &dwDstYellow);
                if(trPass != tr)
                {
                    dbgout(LOG_ABORT) << "Unable to retrieve the destination surface's yellow pixel value" << endl;
                    return trAbort;
                }

                tr = GetColorVal((double)dwAlpha/(double)0xff, 0.0, 0.0, piddsDst, &dwDstRed);
                if(trPass != tr)
                {
                    dbgout(LOG_ABORT) << "Unable to retrieve the destination surface's red pixel value" << endl;
                    return trAbort;
                }

                tr = GetColorVal((double)dwAlpha/(double)0xff, (double)dwAlpha/(double)0xff, (double)dwAlpha/(double)0xff, piddsDst, &dwDstWhite);
                if(trPass != tr)
                {
                    dbgout(LOG_ABORT) << "Unable to retrieve the destination surface's white pixel value" << endl;
                    return trAbort;
                }

                dbgout << "Blue on Destination = " << HEX((DWORD)dwDstBlue) << endl;
                dbgout << "Yellow on Destination = " << HEX((DWORD)dwDstYellow) << endl;
                dbgout << "Red on Destination = " << HEX((DWORD)dwDstRed) << endl;
                dbgout << "White on Destination = " << HEX((DWORD)dwDstWhite) << endl;

                // Check if all the pixels in the rcRectWithBlue rect are blue
                dbgout << "Verifying the Blue Rectangle" << endl;
                tr |= cddsvFillVerifier.PreVerifyColorFill(piddsDst);
                tr |= cddsvFillVerifier.VerifyColorFill(dwDstBlue, &rcRectWithBlue);

                // Check if all the pixels in the rcRectWithYellow rect are yellow
                dbgout << "Verifying the Yellow Rectangle" << endl;
                tr |= cddsvFillVerifier.PreVerifyColorFill(piddsDst);
                tr |= cddsvFillVerifier.VerifyColorFill(dwDstYellow, &rcRectWithYellow);

                // Check if all the pixels in the rcRectWithWhite rect are white
                dbgout << "Verifying the Blue Rectangle" << endl;
                tr |= cddsvFillVerifier.PreVerifyColorFill(piddsDst);
                tr |= cddsvFillVerifier.VerifyColorFill(dwDstWhite, &rcRectWithWhite);

                // Check if all the pixels in the rcRectWithRed rect are red
                dbgout << "Verifying the Yellow Rectangle" << endl;
                tr |= cddsvFillVerifier.PreVerifyColorFill(piddsDst);
                tr |= cddsvFillVerifier.VerifyColorFill(dwDstRed, &rcRectWithRed);

                // If we are testing AlphaBlt blending, allow pixel comparison failures.
                if((tr != trPass) && bTestAlphaBlt && !(dwAlphaBltFlags & DDABLT_NOBLEND))
                {
                    dbgout << "WARNING: Failed comparing the results of an AlphaBlt." <<endl
                           << "Passing the test as alpha calculations may vary slightly." << endl;
                    tr = trPass;
                }

                if(trPass != tr)
                {
                    dbgout(LOG_FAIL) << "Color Verification failed after a Rotated " << (bTestAlphaBlt?"AlphaBlt":"Blt") << endl;
                    dbgout(LOG_FAIL) << "Source Surface Description: " << endl << ddsdSrc << endl;
                    dbgout(LOG_FAIL) << "Destination Surface Description: " << endl << ddsdDst << endl;

                    // Save the Destination surface to a bmp for manual verification
                    static const int iBUFSIZE = 128;
                    TCHAR szFileName[iBUFSIZE];
                    SYSTEMTIME stCurrentTime;
                    GetSystemTime(&stCurrentTime);

                    int nChars = swprintf_s(szFileName,countof(szFileName), _T("RotatedBlt%d_Failure_%d_%d_%d_%d.bmp"), (nextAngle*90) + 90,
                             stCurrentTime.wHour, stCurrentTime.wMinute, stCurrentTime.wSecond, stCurrentTime.wMilliseconds );
                    if(nChars < 0)
                    {
                        dbgout(LOG_FAIL) << "FAIL :RotatedBltTests - Error in _tcsncat_s" << endl;
                    }

                    if(S_OK != BitMap_Helpers::SaveSurfaceToBMP(piddsDst, szFileName))
                    {
                        dbgout << "!!!!NOTE: UNABLE TO SAVE DESTINATION SURFACE TO " << szFileName << "!!!!" << endl;
                    }
                    else
                    {
                        dbgout << "!!!!NOTE: DESTINATION SURFACE HAS BEEN STORED IN " << szFileName << "!!!!" << endl;
                        dbgout << "Check the source and destination surface descriptions above." << endl;
                    }
                }
                // The loop will terminate if tr != trPass, so don't do anything on failure here.
            }
       }

       return tr;
    }

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
        const DWORD cdwInstructionSize = 256;
        TCHAR szInstructions[cdwInstructionSize];
        HRESULT hrExpected = S_OK;
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

        if (cddsdDst.ddsCaps.dwCaps & DDSCAPS_FLIP)
        {
            dbgout << "Test with overlay flip chain is redundant" << endl;
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
        if(cddsdDst.ddpfPixelFormat.dwFlags & DDPF_FOURCC &&
            TestKit_Surface_Modifiers::Surface_Fill_Helpers::IsPlanarFourCC(cddsdDst.ddpfPixelFormat.dwFourCC))
        {
            dbgout << "Test does not support planar YUV formats" << endl;
            return trPass;
        }

        // set the overlay width (1/2 the width of the primary
        int nOverlayHeight = cddsdDst.dwHeight / 2,
                nOverlayWidth  = cddsdDst.dwWidth / 2;

        // Check for a size restriction on overlays
        if (g_DDConfig.HALCaps().dwAlignSizeSrc != 0)
        {
            // If AlignSizeSrc is non-zero, then the source rectange
            // height and width must be a multiple of it

            // We don't determine if we're using HAL or HEL before we make the call to UpdateOverlay.
            // Therefore, we go for the lowest common denominator for determining the alignment.
            DWORD dwAlignSizeSrc = g_DDConfig.HALCaps().dwAlignSizeSrc;
            nOverlayHeight -= (nOverlayHeight % dwAlignSizeSrc);
            nOverlayWidth  -= (nOverlayWidth  % dwAlignSizeSrc);
        }
        // set the overlay x and y top left positions
        int nDestX = cddsdDst.dwWidth / 4,
            nDestY = cddsdDst.dwHeight / 4;
        // Check for position restrictions on overlays
        if (g_DDConfig.HALCaps().dwAlignBoundaryDest != 0)
        {
            // If AlignBoundaryDes is non-zero then the destination
            // rectange top-left postion must be a multiple of it

            // We don't determine if we're using HAL or HEL before we make the call to UpdateOverlay.
            // Therefore, we go for the lowest common denominator for determining the alignment.
            DWORD dwAlignBoundaryDest = g_DDConfig.HALCaps().dwAlignBoundaryDest;
            nDestX -= (nDestX % dwAlignBoundaryDest);
            nDestY -= (nDestY % dwAlignBoundaryDest);
        }
        // set the overlay
        RECT rcRect = { nDestY, nDestX, nDestY + nOverlayWidth, nDestX + nOverlayHeight};
        RECT rcDest;
        CopyRect(&rcDest, &rcRect);
        priv_Overlay::RealignRects(&rcRect, &rcDest, &(g_DDConfig.HALCaps()));

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
        hr = m_piDDSDst->UpdateOverlay(&rcRect, m_piDDS.AsInParam(), &rcDest, DDOVER_SHOW, NULL);
        QueryForHRESULT(hr, hrExpected, "Failure code returned from UpdateOverlay with wait and NULL BltFx", tr|=trFail);

        int nChars = swprintf_s(szInstructions,countof(szInstructions), TEXT("The primary is blue and the overlay is red."));
        if(nChars < 0)
        {
            dbgout(LOG_FAIL) << "FAIL :CTest_InteractiveColorFillOverlayBlts - Error in swprintf_s" << endl;
        }
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
        hr = m_piDDSDst->UpdateOverlay(&rcRect, m_piDDS.AsInParam(), &rcDest, DDOVER_SHOW, NULL);
        QueryForHRESULT(hr, hrExpected, "Failure code returned from UpdateOverlay with wait and NULL BltFx", tr|=trFail);
        nChars = swprintf_s(szInstructions,countof(szInstructions), TEXT("The primary is red and the overlay is green."));
        if(nChars < 0)
        {
            dbgout(LOG_FAIL) << "FAIL :CTest_InteractiveColorFillOverlayBlts - Error in swprintf_s" << endl;
        }
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

        hr = m_piDDSDst->UpdateOverlay(&rcRect, m_piDDS.AsInParam(), &rcDest, DDOVER_SHOW, NULL);
        QueryForHRESULT(hr, hrExpected, "Failure code returned from UpdateOverlay with wait and NULL BltFx", tr|=trFail);
        nChars = swprintf_s(szInstructions,countof(szInstructions), TEXT("The primary is green and the overlay is blue."));
        if(nChars < 0)
        {
            dbgout(LOG_FAIL) << "FAIL :CTest_InteractiveColorFillOverlayBlts - Error in swprintf_s" << endl;
        }
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

        // Need to check if pixel format is matching before doing BLT and corresponding tests.
        // If there is no windowed primary surface invovled, GetSurfaceDesc() will give us correct pixel format.
        // When there is windowed primary surface invovled. We need use lock to get the correct pixel format

        // get the dimensions of the rectangle we have to work with (if in normal mode)
        LPRECT prc = NULL;
        RECT rc;
        rc.left=0;
        rc.right=cddsdDst.dwWidth;
        rc.top=0;
        rc.bottom=cddsdDst.dwHeight;

        // If a destination/src surface is a Windowed Primary, constrain all rect calculations to it's clipping rectangle.
        if(g_DDConfig.CooperativeLevel() == DDSCL_NORMAL)
        {
            if(cddsdSrc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
            {
                tr = GetClippingRectangle(m_piDDS.AsInParam(), rc);
                if(trPass != tr)
                {
                    dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
                    return tr;
                }
                prc = &rc;
                // lock the source surface getting  getting accurate descriptor used for checking format
                hr = m_piDDS->Lock(prc, &cddsdSrc, DDLOCK_WAITNOTBUSY, NULL);
                if(FAILED(hr))
                {
                    dbgout (LOG_FAIL)<< "Failure locking src surface. hr = " 
                                     << g_ErrorMap[hr] << " (" << HEX((DWORD)hr) << "). "
                                     << "GetLastError() = " << HEX((DWORD)GetLastError())
                                     << endl;
                    m_piDDS->Unlock(prc);
                    return trFail;
                }
                m_piDDS->Unlock(prc);    //after successfully locking, we only need the descriptor, and we don't want to keep locking the surface
            }
            else if(cddsdDst.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
            {
                tr = GetClippingRectangle(m_piDDSDst.AsInParam(), rc);
                if(trPass != tr)
                {
                    dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
                    return tr;
                }
                prc = &rc;
                // lock the destination surface getting accurate descriptor used for checking format
                hr = m_piDDSDst->Lock(prc, &cddsdDst, DDLOCK_WAITNOTBUSY, NULL);
                if(FAILED(hr))
                {
                    dbgout (LOG_FAIL)<< "Failure locking dest surface. hr = "     
                                     << g_ErrorMap[hr] << " (" << HEX((DWORD)hr) << "). "
                                     << "GetLastError() = " << HEX((DWORD)GetLastError())
                                     << endl;
                    m_piDDSDst->Unlock(prc);
                    return trFail;
                }
                m_piDDSDst->Unlock(prc);     //after successfully locking, we only need the descriptor, and we don't want to keep locking the surface
            }
        }

        // we only do the following blt test when the pixel format is matching each other
        if(     (cddsdSrc.ddpfPixelFormat.dwRGBBitCount == cddsdDst.ddpfPixelFormat.dwRGBBitCount)
            &&  (cddsdSrc.ddpfPixelFormat.dwRBitMask == cddsdDst.ddpfPixelFormat.dwRBitMask)
            &&  (cddsdSrc.ddpfPixelFormat.dwGBitMask == cddsdDst.ddpfPixelFormat.dwGBitMask)
            &&  (cddsdSrc.ddpfPixelFormat.dwBBitMask == cddsdDst.ddpfPixelFormat.dwBBitMask)
           )

        {
            // In case the descriptors are updated using the version obtainted by lock()
            // we will restore it to avoid conflicts of the following tests when format, height,width are involved
            hr = m_piDDS->GetSurfaceDesc(&cddsdSrc);
            hr = m_piDDSDst->GetSurfaceDesc(&cddsdDst);

            HRESULT hrExpected = S_OK;

            // Get the HAL blt caps for the correct surface locations
            DWORD  dwHALBltCaps;
            DWORD  dwHALCKeyCaps;

            GetBltCaps(cddsdSrc, cddsdDst, GBC_HAL, &dwHALBltCaps, &dwHALCKeyCaps);

            // if we can't source colorkey, and this is the colorkey test, do nothing.
            if((dwBltFlags & DDBLT_KEYSRC) && !(dwHALCKeyCaps & DDCKEYCAPS_SRCBLT))
            {
                dbgout << "source colorkeying not supported -- Skipping Test" << endl;
                return trPass;
            }

            // if we can't dest colorkey, and this is the dest colorkey test, do nothing.
            if((dwBltFlags & DDCKEY_DESTBLT) && !(dwHALCKeyCaps & DDCKEYCAPS_DESTBLT))
            {
                dbgout << "destination colorkeying not supported -- Skipping Test" << endl;
                return trPass;
            }

            // offset the dst from the source so if the source/dest are the same, the dest still changes
            int dwWidth=static_cast<int> ((rc.right-rc.left)/2);
            int dwHeight=static_cast<int>((rc.bottom-rc.top)/2);

            rcSrc.left=rc.left;
            rcSrc.right=rcSrc.left+dwWidth;
            rcSrc.top=rc.top;
            rcSrc.bottom=rcSrc.top+dwHeight;

            rcDst.left=rc.left;
            rcDst.right=rcDst.left+dwWidth;
            rcDst.top=rc.top;
            rcDst.bottom=rcDst.top+dwHeight;

            tr|=cddsvSurfaceVerify.PreSurfaceVerify(m_piDDS.AsInParam(), rcSrc, m_piDDSDst.AsInParam(), rcDst);
            hr = m_piDDSDst->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, DDBLT_WAITNOTBUSY | dwBltFlags, NULL);
            QueryForHRESULT(hr, hrExpected, "Failure code returned from BLT with wait and NULL BltFx", tr|=trFail);
            tr|=cddsvSurfaceVerify.SurfaceVerify();

            rcDst.left=rc.left+dwWidth;
            rcDst.right=rcDst.left+dwWidth;
            rcDst.top=rc.top;
            rcDst.bottom=rcDst.top+dwHeight;

            tr|=cddsvSurfaceVerify.PreSurfaceVerify(m_piDDS.AsInParam(), rcSrc, m_piDDSDst.AsInParam(), rcDst);
            hr = m_piDDSDst->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, DDBLT_WAITNOTBUSY | dwBltFlags, NULL);
            QueryForHRESULT(hr, hrExpected, "Failure code returned from BLT with wait and NULL BltFx", tr|=trFail);
            tr|=cddsvSurfaceVerify.SurfaceVerify();

            rcDst.left=rc.left;
            rcDst.right=rcDst.left+dwWidth;
            rcDst.top=rc.top+dwHeight;
            rcDst.bottom=rcDst.top+dwHeight;

            tr|=cddsvSurfaceVerify.PreSurfaceVerify(m_piDDS.AsInParam(), rcSrc, m_piDDSDst.AsInParam(), rcDst);
            hr = m_piDDSDst->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, DDBLT_WAITNOTBUSY | dwBltFlags, NULL);
            QueryForHRESULT(hr, hrExpected, "Failure code returned from BLT with wait and NULL BltFx", tr|=trFail);
            tr|=cddsvSurfaceVerify.SurfaceVerify();

            rcDst.left=rc.left+dwWidth;
            rcDst.right=rcDst.left+dwWidth;
            rcDst.top=rc.top+dwHeight;
            rcDst.bottom=rcDst.top+dwHeight;

            tr|=cddsvSurfaceVerify.PreSurfaceVerify(m_piDDS.AsInParam(), rcSrc, m_piDDSDst.AsInParam(), rcDst);
            hr = m_piDDSDst->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, DDBLT_WAITNOTBUSY | dwBltFlags, NULL);
            QueryForHRESULT(hr, hrExpected, "Failure code returned from BLT with wait and NULL BltFx", tr|=trFail);
            tr|=cddsvSurfaceVerify.SurfaceVerify();


            rcSrc.left=(rc.left+(rc.right-rc.left)/4);
            rcSrc.right=(rc.left+((rc.right-rc.left)-(rc.right-rc.left)/4));
            rcSrc.top=(rc.top+(rc.bottom-rc.top)/4);
            rcSrc.bottom=(rc.top+((rc.bottom-rc.top)-(rc.bottom-rc.top)/4));

            rcDst.left=rcSrc.left;
            rcDst.right=rcSrc.right;
            rcDst.top=rcSrc.top;
            rcDst.bottom=rcSrc.bottom;

            tr|=cddsvSurfaceVerify.PreSurfaceVerify(m_piDDS.AsInParam(), rcSrc, m_piDDSDst.AsInParam(), rcDst);
            hr = m_piDDSDst->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, DDBLT_WAITNOTBUSY | dwBltFlags, NULL);
            QueryForHRESULT(hr, hrExpected, "Failure code returned from BLT with wait and NULL BltFx", tr|=trFail);
            tr|=cddsvSurfaceVerify.SurfaceVerify();

            // do a full surface copy, just for fun.
            rcSrc.left=rc.left;
            rcSrc.right=rc.right;
            rcSrc.top=rc.top;
            rcSrc.bottom=rc.bottom;

            tr|=cddsvSurfaceVerify.PreSurfaceVerify(m_piDDS.AsInParam(), rcSrc, m_piDDSDst.AsInParam(), rcSrc);
            hr = m_piDDSDst->Blt(&rcSrc, m_piDDS.AsInParam(), &rcSrc, DDBLT_WAITNOTBUSY | dwBltFlags, NULL);
            QueryForHRESULT(hr, hrExpected, "Failure code returned from BLT with wait and NULL BltFx", tr|=trFail);
            tr|=cddsvSurfaceVerify.SurfaceVerify();

            // Perform a full surface Blt with NULL rects, but verify the output only in case of non-windowed primaries.
            hr = m_piDDSDst->Blt(NULL, m_piDDS.AsInParam(), NULL, DDBLT_WAITNOTBUSY | dwBltFlags, NULL);
            QueryForHRESULT(hr, hrExpected, "Failure code returned from BLT with wait and NULL BltFx", tr|=trFail);
            if(!((g_DDConfig.CooperativeLevel() == DDSCL_NORMAL) && ((cddsdSrc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
                || (cddsdDst.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE))
                )
              )
            {
                tr|=cddsvSurfaceVerify.SurfaceVerify();
            }

            if(trPass == tr)
            {
                if((dwHALBltCaps & DDBLTCAPS_ROTATION90)) // if the platform supports Rotated Blts in multiples of 90 degrees.
                {
                    dbgout << "Rotated Blt is supported on this platform. Performing Rotated Blt tests." << endl;
                    tr = RotatedBltTests(false, m_piDDS.AsInParam(), m_piDDSDst.AsInParam()); // Blt Tests
                    tr = RotatedBltTests(true, m_piDDS.AsInParam(), m_piDDSDst.AsInParam());  // AlphaBlt Tests
                }
                else
                {
                    dbgout << "Rotated Blt is not supported on this platform. Skipping the Rotated Blt tests." << endl;
                }
            }
        }
        else // nonmatching pixel format
        {
            // when windowed primary is invovled in either src or dst. It is possible to have nonmatching pixel format
            // skip BLT testing and output nonmatching information
            if ( (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL) 
                 && ( (cddsdSrc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)||(cddsdDst.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) )
               )
            {
                dbgout << "Nonmatching src and dst pixel formats and Primary Windowed Surface is invovled" << endl;
                dbgout << "Skipping BLT testing for this pair." << endl;
            }
            else    // when no windowed primary is involved, nonmatching pixel format should signal a failure
            {
                dbgout (LOG_FAIL)<< "FAILED: Nonmatching src and dst pixel formats when Primary Windowed Surface is not invovled" << endl;
                tr = trFail;
            }
        }
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


        // resize the window it is a Windowed Primary, to make sure we can see the instruction text
        if((cddsdSrc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
        {
            dbgout << "cddsdSrc.dwWidth: "<< cddsdSrc.dwWidth << "cddsdSrc.dwHeight: " << cddsdSrc.dwHeight << endl;
            if(!MoveWindow(g_DirectDraw.m_hWnd, 0, 0, cddsdSrc.dwWidth, cddsdSrc.dwHeight, FALSE))
            {
                dbgout << "Unable to move the window (Error : " << HEX((DWORD)GetLastError()) << "). Continuing anyway.." << endl;
            }
            //Refill the pattern after resizing the window, otherwise we will not see the pattern and will be difficult to check the transparency of overlay
            DDCOLORKEY ddcheck;  

            DWORD dwColor1;
            DWORD dwColor2;
            if (SUCCEEDED(m_piDDS.AsInParam()->GetColorKey(DDCKEY_DESTOVERLAY, &ddcheck)))
            {
                dwColor1= TestKit_Surface_Modifiers::Surface_Fill_Helpers::SetColorVal(0.5, 0,1, m_piDDS.AsInParam());    //purple for dest overlay colorkey enabled case
            }
            else
            {
                dwColor1= TestKit_Surface_Modifiers::Surface_Fill_Helpers::SetColorVal(1, 1,1, m_piDDS.AsInParam());
            }
            dwColor2= TestKit_Surface_Modifiers::Surface_Fill_Helpers::SetColorVal(0, 0,0, m_piDDS.AsInParam());

            tr |= TestKit_Surface_Modifiers::Surface_Fill_Helpers::FillSurfaceVertical(m_piDDS.AsInParam(), dwColor1, dwColor2);
        }

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

        HRESULT hrExpected = S_OK;

        // Get the HAL blt caps for the correct surface locations
        DWORD  dwHALBltCaps;
        DWORD  dwHALCKeyCaps;
        DWORD  dwAlignSizeSrc = 1;
        DWORD  dwAlignBoundaryDest = 1;
        DWORD  dwHALCaps;

        GetBltCaps(cddsdSrc, cddsdDst, GBC_HAL, &dwHALBltCaps, &dwHALCKeyCaps);

        dwHALCaps = g_DDConfig.HALCaps().dwOverlayCaps;

        if (cddsdDst.ddsCaps.dwCaps & DDSCAPS_FLIP)
        {
            dbgout << "Test with overlay flip chain is redundant" << endl;
            return trPass;
        }

        // if we can't source colorkey, and this is the colorkey test, do nothing.
        if((dwAvailBltFlags & DDOVER_KEYSRC) && !(dwHALCaps & DDOVERLAYCAPS_CKEYSRCCLRSPACEYUV))
        {
            dbgout << "source colorkeying not supported on overlays-- Skipping Test" << endl;
            return trPass;
        }
        if((dwAvailBltFlags & DDOVER_KEYSRC) && !(dwHALCaps & DDOVERLAYCAPS_CKEYSRC))
        {
            dbgout << "source colorkeying not supported on overlays-- Skipping Test" << endl;
            return trPass;
        }
         if((dwAvailBltFlags & DDOVER_KEYDEST) && !(dwHALCaps & DDOVERLAYCAPS_CKEYDEST))
        {
            dbgout << "dest colorkeying not supported on overlays-- Skipping Test" << endl;
            return trPass;
        }

        // set the overlay width
        int nOverlayHeight = cddsdDst.dwHeight / 2,
             nOverlayWidth  = cddsdDst.dwWidth / 2;

        // Check for a size restriction on overlays
        if (g_DDConfig.HALCaps().dwAlignSizeSrc != 0)
        {
            // If AlignSizeSrc is non-zero, then the source rectange
            // height and width must be a multiple of it

            // We don't determine if we're using HAL or HEL before we make the call to UpdateOverlay.
            // Therefore, we go for the lowest common denominator for determining the alignment.
            dwAlignSizeSrc = g_DDConfig.HALCaps().dwAlignSizeSrc;
            nOverlayHeight -= (nOverlayHeight % dwAlignSizeSrc);
            nOverlayWidth  -= (nOverlayWidth  % dwAlignSizeSrc);
        }
        // set the overlay x and y top left positions
        int nDestX = cddsdDst.dwWidth / 4,
            nDestY = cddsdDst.dwHeight / 4;
        // Check for position restrictions on overlays
        if (g_DDConfig.HALCaps().dwAlignBoundaryDest != 0)
        {
            // If AlignBoundaryDes is non-zero then the destination
            // rectange top-left postion must be a multiple of it

            // We don't determine if we're using HAL or HEL before we make the call to UpdateOverlay.
            // Therefore, we go for the lowest common denominator for determining the alignment.
            dwAlignBoundaryDest = g_DDConfig.HALCaps().dwAlignBoundaryDest;
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


        bool bUpperLeftText = true;
        rcSrc.left=0;
        rcSrc.right=nOverlayWidth;
        rcSrc.top=0;
        rcSrc.bottom=nOverlayHeight;
        int nChars = 0 ;
        for (int iPos = 0; iPos < sizeof(rcRect)/sizeof(*rcRect); iPos++)
        {
            rand_s(&uNumber);
            dwBltFlags = ((DDOVER_KEYDEST & dwAvailBltFlags) && uNumber%2) ? DDOVER_KEYDEST : dwAvailBltFlags & ~DDOVER_KEYDEST;
            priv_Overlay::RealignRects(&rcSrc, &rcRect[iPos], &(g_DDConfig.HALCaps()));
            dbgout << "Testing overlay at position (top, left, bottom, right) (" << rcSrc.top << "," << rcSrc.left << "," << rcSrc.bottom << "," << rcSrc.right << ")" << endl;
            hr=m_piDDSDst->UpdateOverlay(&rcSrc, m_piDDS.AsInParam(), &rcRect[iPos], DDOVER_SHOW | dwBltFlags, NULL);
            QueryForHRESULT(hr, hrExpected, "Failure code returned from UpdateOverlay with wait and NULL BltFx", tr|=trFail);
            if(dwBltFlags & DDOVER_KEYSRC)
                nChars = _snwprintf_s(szInstructions, countof(szInstructions), TEXT("Overlay should appear as a checkerboard, with every other square transparent."));
            else if(dwBltFlags & DDOVER_KEYDEST)
                nChars = _snwprintf_s (szInstructions, countof(szInstructions), TEXT("Overlay should appear where the primary is purple, but not where it is black."));
            else
                nChars =_snwprintf_s (szInstructions, countof(szInstructions), TEXT("Overlay should appear as a checkerboard, with no transparency."));
            if(nChars < 0)
            {
                dbgout << "FAIL :CTest_InteractiveOverlayBlt::TestIDirectDrawSurface_TWO -Error in _snwprintf_s in function  ";
                
            }
            bUpperLeftText = (iPos != 4);  // when the overlay is at upper left (iPos==4), set flag to false, it will move text to lower right
            tr|=VerifyOutput(bInteractive, bDelayed, m_piDDS.AsInParam(), szInstructions, &rcRect[iPos],bUpperLeftText);
            if (tr & trAbort)
                return tr;
        }

        for (DWORD j = 0; j < 16 && j * dwAlignSizeSrc < (DWORD) nOverlayWidth && rcSrc.right - rcSrc.left > 0; j++)
        {
            dbgout << "Src width: " << rcSrc.right - rcSrc.left << endl;
            rand_s(&uNumber);
            dwBltFlags = ((DDOVER_KEYDEST & dwAvailBltFlags) && uNumber%2) ? DDOVER_KEYDEST : dwAvailBltFlags & ~DDOVER_KEYDEST;
            // Calling RealignRects with the rect for both params will ensure that the rect will
            // meet the requirements for the most stringent alignment values (Src or Dest).
            priv_Overlay::RealignRects(&rcSrc, &rcSrc, &(g_DDConfig.HALCaps()));
            hr=m_piDDSDst->UpdateOverlay(&rcSrc, m_piDDS.AsInParam(), &rcSrc, DDOVER_SHOW | dwBltFlags, NULL);
            QueryForHRESULT(hr, hrExpected, "Failure code returned from UpdateOverlay with wait and NULL BltFx", tr|=trFail);
            if(dwBltFlags & DDOVER_KEYSRC)
                nChars =_snwprintf_s (szInstructions, countof(szInstructions), TEXT("Overlay should appear as a checkerboard, with every other square transparent."));
            else if(dwBltFlags & DDOVER_KEYDEST)
                nChars =_snwprintf_s (szInstructions, countof(szInstructions), TEXT("Overlay should appear where the primary is purple, but not where it is black."));
            else
                nChars =_snwprintf_s (szInstructions, countof(szInstructions), TEXT("Overlay should appear as a checkerboard, with no transparency."));
            if(nChars < 0)
            {
                dbgout << "FAIL :CTest_InteractiveOverlayBlt::TestIDirectDrawSurface_TWO -Error in _snwprintf_s in function  ";
                
            }
            // overlay verification in this series will be always on upper left, set 6th parameter as false to make it always display the text in lower-right
            tr|=VerifyOutput(bInteractive, bDelayed, m_piDDS.AsInParam(), szInstructions, &rcRect[0],false);  
            if (tr & trAbort)
                return tr;
            rcSrc.right -= dwAlignSizeSrc;
        }
        rcSrc.right = nOverlayWidth;

        if (g_DDConfig.HALCaps().dwMinOverlayStretch != 1000 || g_DDConfig.HALCaps().dwMaxOverlayStretch != 1000)
        {
            dbgout << "DDCAPS_OVERLAYSTRETCH is enabled, testing stretching" << endl;
            DWORD dwStretchFactor;
            DWORD dwMinOverlayStretch;
            DWORD dwMaxOverlayStretch;
            DWORD dwMinSrcWidth;
            DWORD dwMaxSrcWidth;
            DWORD dwStep;
            int   iSrcRight;
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
            if (dwMaxSrcWidth > cddsdDst.dwWidth)
                dwMaxSrcWidth = cddsdDst.dwWidth;

            // determine an appropriate step size
            dwStep = (dwMaxSrcWidth - dwMinSrcWidth) / 10;
            dbgout << "Using step size " << dwStep << endl;

            for (iSrcRight = dwMinSrcWidth; iSrcRight <= static_cast<int> (dwMaxSrcWidth); iSrcRight += dwStep)
            {
                rcSrc.right = iSrcRight;
                rcSrc.bottom = rcSrc.right*nOverlayHeight/nOverlayWidth;
                dwStretchFactor = nOverlayWidth * 1000 / rcSrc.right;
                int nChars =0;
                priv_Overlay::RealignRects(&rcSrc, &rcDest, &(g_DDConfig.HALCaps()));

                dbgout << "Stretch factor: " << dwStretchFactor << endl;
                dbgout << "Source rectangle: " << rcSrc << endl;
                dbgout << "Destination rectangle: " << rcDest << endl;
                rand_s(&uNumber);
                dwBltFlags = ((DDOVER_KEYDEST & dwAvailBltFlags) && uNumber%2) ? DDOVER_KEYDEST : dwAvailBltFlags & ~DDOVER_KEYDEST;
                hr = m_piDDSDst->UpdateOverlay(&rcSrc, m_piDDS.AsInParam(), &rcDest, DDOVER_SHOW | dwBltFlags, NULL);
                
                if(hr == DDERR_NOSTRETCHHW)
                { 
                    dbgout << "Hardware doesn't support stretching. Skipping the test" << endl;
                    return trSkip;
                } 

                QueryForHRESULT(hr, hrExpected, "Failure code returned from UpdateOverlay with wait, NULL BltFx, and stretch", tr|=trFail);
                if(dwBltFlags & DDOVER_KEYSRC)
                    nChars =_snwprintf_s (szInstructions, countof(szInstructions), TEXT("Overlay should appear as a stretched checkerboard, with every other square transparent."));
                else if(dwBltFlags & DDOVER_KEYDEST)
                    nChars = _snwprintf_s (szInstructions, countof(szInstructions), TEXT("Overlay should appear (stretched) where the primary is purple, but not where it is black."));
                else
                    nChars = _snwprintf_s (szInstructions, countof(szInstructions), TEXT("Overlay should appear as a stretched checkerboard, with no transparency."));
                if(nChars < 0)
                {
                    dbgout << "FAIL :CTest_InteractiveOverlayBlt::TestIDirectDrawSurface_TWO -Error in _snwprintf_s  ";
                }
                // overlay verification in this series will be always in the center, use default as 6th parameter (true) to make it always display the text in upper left
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
        RECT rcSrcRect,
             rcDstRect;
        RECT rc;
        CDDrawSurfaceVerify cddsvSurfaceVerify;
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
        if (!(cddsdDst.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (bDelayed || bInteractive))
        {
            dbgout << "Colorfilling cannot be verified on an offscreen surface -- Skipping Test" << endl;
            return trPass;
        }

        DWORD  dwHALBltCaps;
        DWORD  dwHALCKeyCaps;
        const DWORD cdwInstructionSize = 256;
        TCHAR szInstructions[cdwInstructionSize];
        int nChars = 0;
        GetBltCaps(cddsdSrc, cddsdDst, GBC_HAL, &dwHALBltCaps, &dwHALCKeyCaps);

        if ((cddsdDst.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
            (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
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

        hr = m_piDDSDst->Blt(&rcDstRect, m_piDDS.AsInParam(), &rcSrcRect, DDBLT_WAITNOTBUSY, NULL);
        nChars = swprintf_s(szInstructions,countof(szInstructions), TEXT("There are smaller vertical lines over larger horizontal lines?"));
        if(nChars < 0)
        {
            dbgout << "FAIL :CTest_InteractiveBlt::TestIDirectDrawSurface_TWO() -Error in _snwprintf_s  ";
        }
        tr|=VerifyOutput(bInteractive, bDelayed, m_piDDSDst.AsInParam(), szInstructions);
        if (tr & trAbort)
            return tr;

        // prepare for color fill test verification

        tr|=cddsvSurfaceVerify.PreVerifyColorFill(m_piDDSDst.AsInParam());

        tr |= SetColorVal(1,0,0,m_piDDSDst.AsInParam(), &dwTmp);
        tr |=cddsvSurfaceVerify.VerifyColorFill(dwTmp);
        nChars = swprintf_s(szInstructions,countof(szInstructions), TEXT("The primary is red."));
        if(nChars < 0)
        {
            dbgout << "FAIL :CTest_InteractiveBlt::TestIDirectDrawSurface_TWO() -Error in _snwprintf_s  ";
        }
        tr|=VerifyOutput(bInteractive, bDelayed, m_piDDSDst.AsInParam(), szInstructions);
        if (tr & trAbort)
            return tr;

        tr |= SetColorVal(0,1,0,m_piDDSDst.AsInParam(), &dwTmp);
        tr |=cddsvSurfaceVerify.VerifyColorFill(dwTmp);
        nChars = swprintf_s(szInstructions,countof(szInstructions), TEXT("The primary is green."));
        if(nChars < 0)
        {
            dbgout << "FAIL :CTest_InteractiveBlt::TestIDirectDrawSurface_TWO() -Error in _snwprintf_s  ";
        }
        tr|=VerifyOutput(bInteractive, bDelayed, m_piDDSDst.AsInParam(), szInstructions);
        if (tr & trAbort)
            return tr;

        tr |= SetColorVal(0,0,1,m_piDDSDst.AsInParam(), &dwTmp);
        tr |=cddsvSurfaceVerify.VerifyColorFill(dwTmp);
        nChars = swprintf_s(szInstructions,countof(szInstructions), TEXT("The primary is blue."));
        if(nChars < 0)
        {
            dbgout << "FAIL :CTest_InteractiveBlt::TestIDirectDrawSurface_TWO() -Error in swprintf_s  ";
        }
        tr|=VerifyOutput(bInteractive, bDelayed, m_piDDSDst.AsInParam(), szInstructions);
        if (tr & trAbort)
            return tr;

        return tr;

    }
    namespace priv_InterlacedOverlayFlipHelpers
    {
        class InterlaceBoxHelper {
        public:
            void Initialize(int iMaxX, int iMaxY);
            void Reset();
            void SetDirection(bool fGoingDown);
            HRESULT PrepareSurface(IDirectDrawSurface * pSurface);
            HRESULT DrawBoxes(IDirectDrawSurface * pSurface, bool fTopOnLeft);
        private:
            void DrawBox(CDDSurfaceDesc * pcddsd, int iX, int iY, int iWidth, int iHeight, DWORD dwFillColor);

            int m_iCurrentY;
            int m_iYVel;

            int m_iBoxHeight;
            int m_iBoxWidth;
            int m_iMaxY;
            int m_iMaxX;
        };
        void InterlaceBoxHelper::Initialize(int iMaxX, int iMaxY)
        {
            Reset();
            m_iBoxHeight = 2;
            m_iBoxWidth = 10;
            m_iMaxY = iMaxY;
            m_iMaxX = iMaxX;
            return;
        }
        void InterlaceBoxHelper::Reset()
        {
            m_iCurrentY = 0;
            m_iYVel = 2;
        }
        void InterlaceBoxHelper::SetDirection(bool fGoingDown)
        {
            if (m_iYVel < 0 && fGoingDown)
                m_iYVel = -m_iYVel;
            else if (m_iYVel > 0 && !fGoingDown)
                m_iYVel = -m_iYVel;
        }
        HRESULT InterlaceBoxHelper::PrepareSurface(IDirectDrawSurface * pSurface)
        {
            HRESULT hr;
            CDDSurfaceDesc cddsd;
            // Lock the backbuffer
            hr = pSurface->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
            if (FAILED(hr))
            {
                dbgout << "Unable to lock surface to validation boxes " << hr << endl;
                return hr;
            }

            // Clear the backbuffer
            DrawBox(&cddsd, 0, 0, m_iMaxX, m_iMaxY, 0x00000000);

            // Unlock the backbuffer
            return pSurface->Unlock(NULL);
        }
        HRESULT InterlaceBoxHelper::DrawBoxes(IDirectDrawSurface * pSurface, bool fTopOnLeft)
        {
            int iXTop = 20;
            int iXBottom = 40;
            CDDSurfaceDesc cddsd;
            HRESULT hr;

            if (iXBottom + m_iBoxWidth > m_iMaxX)
            {
                int iDif = iXBottom + m_iBoxWidth - m_iMaxX;
                iXBottom -= iDif;
                iXTop -= iDif/2;
            }

            if (!fTopOnLeft)
            {
                // Swap the X location of the two boxes
                iXTop ^= iXBottom;
                iXBottom ^= iXTop;
                iXTop ^= iXBottom;
            }
            m_iCurrentY += m_iYVel;

            if (m_iYVel > 0 && m_iCurrentY + m_iBoxHeight >= m_iMaxY)
            {
                m_iCurrentY = 0;
            }
            else if (m_iYVel < 0 && m_iCurrentY <= 0)
            {
                m_iCurrentY = m_iMaxY - m_iBoxHeight;
            }

            // The Y must always be consistently odd or even. We choose even, so if it is odd,
            // adjust as necessary. We do this adjustment here because the maxY might be either
            // odd or even, and instead of going through the process of creating and proving a
            // solution that will always keep m_iCurrentY even, we just adjust it if it isn't.
            if (m_iCurrentY % 2)
            {
                m_iCurrentY -= 1;
            }

            // Lock the backbuffer
            hr = pSurface->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
            if (FAILED(hr))
            {
                dbgout << "Unable to lock surface to validation boxes " << hr << endl;
                return hr;
            }

            // Clear the backbuffer
            DrawBox(&cddsd, iXTop, 0, m_iBoxWidth, m_iMaxY, 0x00000000);
            DrawBox(&cddsd, iXBottom, 0, m_iBoxWidth, m_iMaxY, 0x00000000);

            // Draw the first box
            DrawBox(&cddsd, iXTop, m_iCurrentY, m_iBoxWidth, m_iBoxHeight, 0xFFFFFFFF);
            // Draw the second box
            DrawBox(&cddsd, iXBottom, m_iCurrentY + 1, m_iBoxWidth, m_iBoxHeight, 0xFFFFFFFF);

            // Unlock the backbuffer
            return pSurface->Unlock(NULL);
        }

        void InterlaceBoxHelper::DrawBox(CDDSurfaceDesc * pcddsd, int iX, int iY, int iWidth, int iHeight, DWORD dwFillColor)
        {
            using namespace TestKit_Surface_Modifiers::Surface_Fill_Helpers;

            // determine if this is RGB or YUV
            bool fIsFourCC = (pcddsd->ddpfPixelFormat.dwFlags & DDPF_FOURCC)?true:false;
            DWORD dwFourCC = pcddsd->ddpfPixelFormat.dwFourCC;
            // if YUV, clear out the U and V to prevent subsampling from disrupting the results.
            if (fIsFourCC)
            {
                DWORD Y1, Y2, U, V;
                UnpackYUV(dwFourCC, dwFillColor, &Y1, &Y2, &U, &V);
                dwFillColor = PackYUV(dwFourCC, Y1, Y2, 0, 0);
            }

            // Loop through Y
            DWORD * pdwSrc=(DWORD*)pcddsd->lpSurface;
            DWORD dwLSize=pcddsd->lPitch/4;
            bool fNonPlanar = false;
            if (!fIsFourCC || !IsPlanarFourCC(dwFourCC))
            {
                fNonPlanar = true;
                iWidth = iWidth * pcddsd->ddpfPixelFormat.dwRGBBitCount / 32;
                iX = iX * pcddsd->ddpfPixelFormat.dwRGBBitCount / 32;
            }
            for(int y=iY;y<iY+iHeight;y++)
            {
                DWORD dwYOffset = dwLSize * y;
                for(int x=iX;x<iX+iWidth;x++)
                {
                    if (fNonPlanar)
                    {
                        pdwSrc[dwYOffset+x]=dwFillColor;
                    }
                    else
                    {
                        SetPlanarPixel(pcddsd, x, y, dwFillColor);
                    }
                }
            }
        }

        int g_iDesiredBackBuffer = 0;
        HRESULT WINAPI FindBackBuffer(IDirectDrawSurface * pDDS, DDSURFACEDESC * pddsd, void * lpContext)
        {
            IDirectDrawSurface ** ppSurface = (IDirectDrawSurface**)lpContext;
            --g_iDesiredBackBuffer;
            if(!g_iDesiredBackBuffer)
            {
                *ppSurface = pDDS;
                return DDENUMRET_CANCEL;
            }
            if (!(pddsd->ddsCaps.dwCaps & DDSCAPS_FRONTBUFFER))
            {
                pDDS->EnumAttachedSurfaces(lpContext, (LPDDENUMSURFACESCALLBACK)FindBackBuffer);
            }
            pDDS->Release();
            return DDENUMRET_CANCEL;
        }

        eTestResult CheckTestResult(int iResult, bool fTopIsLeft, bool fIsTopBoxGood, bool fIsInterlacedSurface)
        {
            bool fUserChoseLeft = false;
            // If user chooses the correct box, pass, otherwise fail
            bool fLeftIsCorrect = (fTopIsLeft && fIsTopBoxGood) || (!fTopIsLeft && !fIsTopBoxGood);
            if ('S' == iResult)
            {
                dbgout(LOG_FAIL) << "Tester is skipping this test case." << endl;
                return trPass;
            }

            if (fIsInterlacedSurface)
            {
                dbgout
                    << "Command line parameters indicate surface is interlaced. (parameter 'l' is used to indicate surfaces are interlaced)" << endl
                    << "If not testing on an interlaced device, remove 'l' from the command line parameters." << endl;
            }
            else
            {
                dbgout
                    << "Command line parameters indicate surface is not interlaced. (parameter 'l' is used to indicate surfaces are interlaced)" << endl
                    << "If testing on an interlaced device, add 'l' to the command line parameters (tux -o -d ddrawtk -c il)." << endl;
            }

            if (VK_UP == iResult || !fIsInterlacedSurface)
            {
                dbgout << "Tester discerned no difference between boxes" << endl;
            }
            else if (VK_LEFT == iResult)
            {
                dbgout << "Tester chose Left box" << endl;
                fUserChoseLeft = true;
            }
            else if (VK_RIGHT == iResult)
            {
                dbgout << "Tester chose right box" << endl;
                fUserChoseLeft = false;
            }
            else
            {
                dbgout(LOG_ABORT) << "Tester aborted the test case" << endl;
                return trAbort;
            }

            if (VK_UP == iResult || !fIsInterlacedSurface)
            {
                if (!fIsInterlacedSurface && VK_UP == iResult)
                {
                    dbgout << "Flipping was performed correctly (surface not marked interlaced behaved as it should)" << endl;
                    return trPass;
                }
                else
                {
                    dbgout(LOG_FAIL)
                        << "Surface was " << (fIsInterlacedSurface ? "" : "not ")
                        << "marked interlaced, but " << (!fIsInterlacedSurface ? "behaves " : "does not behave ")
                        << "as an interlaced surface." << endl;
                    if (fIsInterlacedSurface)
                    {
                        dbgout << "Box that should have looked better was the " << (fLeftIsCorrect ? "Left" : "Right") << "box" << endl;
                    }
                    else
                    {
                        dbgout << "Neither box should have looked better" << endl;
                    }
                    return trFail;
                }
            }

            if ((fUserChoseLeft && fLeftIsCorrect) || (!fUserChoseLeft && !fLeftIsCorrect))
            {
                dbgout << "Flipping was performed correctly" << endl;
                return trPass;
            }
            else
            {
                dbgout(LOG_FAIL)
                    << "Incorrect box was determined to look better." << endl
                    << "Box that should have looked better was " << (fLeftIsCorrect ? "<-" : "->") << "box" << endl;
                return trFail;
            }
        }

        // Returns 0 if flip is finished with no user input.
        int CheckInputOrFlipDone(IDirectDrawSurface * piDDS, int cInputs, int rgExpectedInput[], BOOL bInteractive)
        {
            int iResult;
            BOOL bFlipDone = FALSE;
            BOOL bKeyPressed = FALSE;
            // check for user input while waiting for flip to finish
            // while no keyboard input and we are still flipping

            do
            {
                Sleep(0);
                bFlipDone = DD_OK == piDDS->GetFlipStatus(DDGFS_ISFLIPDONE);
                iResult = CheckForKeyboardInput(cInputs, rgExpectedInput);
                bKeyPressed = iResult;
            } while ((bInteractive && !bKeyPressed && !bFlipDone) || (!bInteractive && !bFlipDone));
            if (iResult)
            {
                int iWaitCount = 50;
                while(CheckForKeyboardInput(cInputs, rgExpectedInput) && iWaitCount)
                {
                    --iWaitCount;
                    Sleep(10);
                }
            }
            return iResult;
        }
    }

    eTestResult CTest_InteractiveInterlacedOverlayFlip::TestIDirectDrawSurface_TWO()
    {
        using namespace priv_InterlacedOverlayFlipHelpers;
        eTestResult tr = trPass;
        CDDSurfaceDesc cddsdBackBuffer;
        CComPointer<IDirectDrawSurface> piDDSBackBuffer = NULL;
        bool fIsOverlay = false;
        CDDCaps ddcaps;
        InterlaceBoxHelper Boxer;
        HRESULT hr;
        BOOL bInteractive = FALSE;
        const int NONINTERACTIVE_FLIPCOUNT = 120;
        const int INTERACTIVE_FLIPCOUNT = 120000;
        int FlipCount = 0;

        // If this isn't an interlaced surface, there shouldn't be any difference
        // (since every time the screen is redrawn, both fields are rasterized)
        bool fIsInterlacedSurface;

        int iResult;
        int rgExpectedInput[] = {
            VK_LEFT,
            VK_RIGHT,
            VK_UP,
            'A',
            'S'
        };

        TCHAR szInstructions[] = _T("Which box looks more solid? (Left or right arrow, or up arrow for no difference, or 'a' to abort)");

        struct InterlaceParams{
            // Do we start flipping with the ODD field first?
            bool fIsOddFirst;
            // Do we want the boxes to move up or down?
            bool fMovingDown;
            // Is the top box the one that should look good?
            bool fIsTopBoxGood;
        };


        // The both fields test cases involve starting off with flipping the given
        // field, then the opposite field, then flipping both fields (with either
        // no field flags or both flags at once). Since the driver should always
        // flip the odd field first when both or neither flag is specified, the
        // good box should always be the same as if the odd field were the first
        // flipped.
        InterlaceParams rgBothFields[] = {
            {true, true, true},
            {true, false, false},
            {false, true, false},
            {false, false, true}
        };

        DWORD dwTmp = 0;
        DDrawUty::g_DDConfig.GetSymbol(TEXT("i"), &dwTmp);
        if(dwTmp != 0)
            bInteractive=TRUE;

        DDrawUty::g_DDConfig.GetSymbol(TEXT("l"), &dwTmp);
        m_piDD->GetCaps(&ddcaps, NULL);
        fIsInterlacedSurface = (dwTmp != 0) && (ddcaps.dwMiscCaps & DDMISCCAPS_FLIPODDEVEN);

        // Main test idea: animate two boxes on the screen. Each box is 2 pixels tall.
        // One box will start on an odd line, the other box will start on an even line.
        // The boxes will be right next to each other (to be easily distinguished, one
        // from the other. In half the cases, the boxes will move up by two pixels every
        // frame, the other half of the cases the boxes will move down by two pixels.
        // The tester will need to determine which of the two boxes will look "better"
        // (based on which field of the frame is being draw first, the two boxes will
        // appear slightly different: one will appear to be a solid box, while the other
        // will flicker and appear to be two smaller boxes that "dance").

        // Determine if the current surface is applicable for testing
        // The surface must be flippable and interlaced. (the test will work for all
        // color formats: YUV, where U and V might be 2x2 subsampled, will only use
        // the Y channel).
        if (!(m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_FLIP) || !(fIsInterlacedSurface))
        {
            dbgout << "Test only applies to interlaced flipping surfaces" << endl;
            return trPass;
        }

        // Get the last backbuffer in the flipping chain.
        g_iDesiredBackBuffer = m_cddsdDst.dwBackBufferCount;
        m_piDDSDst->EnumAttachedSurfaces(piDDSBackBuffer.AsOutParam(), (LPDDENUMSURFACESCALLBACK)FindBackBuffer);
        if (NULL == piDDSBackBuffer.AsInParam())
        {
            dbgout(LOG_ABORT)<<"Unable to find desired backbuffer of surface" << endl;
            return trAbort;
        }


        // Determine the visible extents of the surface (if the surface is an overlay,
        // use these same extents for displaying the surface later on)
        fIsOverlay = m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_OVERLAY;

        if (fIsOverlay)
        {
            Boxer.Initialize(m_cddsdDst.dwWidth/2, m_cddsdDst.dwHeight/2);

            // If this is an overlay, make the overlay visible.
            hr = PositionAndDisplayOverlay(m_piDDSDst.AsInParam(), m_piDDS.AsInParam(), ddcaps, m_cddsdDst);
            CheckHRESULT(hr, "UpdateOverlay to the primary surface", trAbort);
        }
        else
        {
            Boxer.Initialize(m_cddsdDst.dwWidth, m_cddsdDst.dwHeight);
        }

        // Clear out all the surfaces in the flipping chain.
        for (int i = 0; i <= static_cast<int> (m_cddsdDst.dwBackBufferCount); ++i)
        {
            Boxer.PrepareSurface(m_piDDSDst.AsInParam());
            hr = m_piDDSDst->Flip(NULL, DDFLIP_WAITNOTBUSY);
            QueryHRESULT(hr, "Flip surface for preparation", tr|=trFail);
        }

        // Enter into test case loop for flipping odd and even together. This test case verifies
        // that drivers follow the rule that when flipping both odd and even, the odd must be done first
        for (i = 0; i < countof(rgBothFields); ++i)
        {
            rand_s(&uNumber);
            bool fTopIsLeft = (uNumber & 1);
            bool fUserChoseLeft = false;

            Boxer.Reset();
            Boxer.SetDirection(rgBothFields[i].fMovingDown);
            // Start by flipping twice: Flip odd then flip even, or flip even then flip odd
            // Determine from parameters which to do first
            dbgout << "Double field test starting with "
                << (rgBothFields[i].fIsOddFirst?"Odd":"Even")
                << " flip first and boxes moving "
                << (rgBothFields[i].fMovingDown?"Down":"Up") << endl;

            if (bInteractive)
            {
                DisplayVerifyString(m_piDDS.AsInParam(), szInstructions);
            }
            DWORD dwFirstFlip = rgBothFields[i].fIsOddFirst ? DDFLIP_ODD : DDFLIP_EVEN;

            if (bInteractive)
                FlipCount = INTERACTIVE_FLIPCOUNT;
            else
                FlipCount = NONINTERACTIVE_FLIPCOUNT;
            do
            {
                //DWORD dwFlipFlag = rgBothFields[i].fFlipBoth ? (DDFLIP_ODD|DDFLIP_EVEN) : 0;
                // If odd and even are on the same surface, draw the boxes to the backbuffer
                Boxer.DrawBoxes(piDDSBackBuffer.AsInParam(), fTopIsLeft);

                hr = m_piDDSDst->Flip(NULL, DDFLIP_WAITNOTBUSY | dwFirstFlip);
                if (FAILED(hr))
                {
                    dbgout(LOG_FAIL)<<"Flip failed, FlipFlag was " << HEX(dwFirstFlip)
                        << "; " << hr << endl;
                    tr |= trFail;
                }

                // check for user input while waiting for flip to finish
                iResult = CheckInputOrFlipDone(m_piDDSDst.AsInParam(), countof(rgExpectedInput), rgExpectedInput, bInteractive);
            } while (0 == iResult && --FlipCount);

            // If user chooses the correct box, pass, otherwise fail
            if (bInteractive)
            {
                tr |= CheckTestResult(iResult, fTopIsLeft, rgBothFields[i].fIsTopBoxGood, fIsInterlacedSurface);
                if (tr & trAbort)
                {
                    return tr;
                }
            }
            else
            {
                tr |= trPass;
            }
        }

        if (fIsOverlay)
        {
            m_piDDSDst->UpdateOverlay(NULL, m_piDDS.AsInParam(), NULL, DDOVER_HIDE, NULL);
        }

        return tr;
    }
};

namespace Test_DeviceModeChange
{
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetSurfaceDesc::TestDevModeChange
    //
    // Scope: This test checks if the "correct" surface description is returned by
    //        the driver after the display settings are changed.
    //
    ///////////////////////////////////////////////////////////////////////////////
    eTestResult CTest_GetSurfaceDesc::TestDevModeChange(
                                                        DEVMODE& dmOldDevMode,
                                                        DEVMODE& dmCurrentDevMode
                                                        )
    {
        using namespace DDrawUty::Surface_Helpers;

        eTestResult tr = trPass;
        HRESULT result = DD_OK;

        // Log the old and new devmodes
        dbgout << "Old DevMode:" << dmOldDevMode << endl;
        dbgout << "Current DevMode:" << dmCurrentDevMode << endl;

        // Get the un-rotated widths & heights.
        DWORD dwOldWidth = dmOldDevMode.dmPelsWidth;
        DWORD dwOldHeight = dmOldDevMode.dmPelsHeight;
        if(dmOldDevMode.dmDisplayOrientation & DMDO_90 || dmOldDevMode.dmDisplayOrientation & DMDO_270)
        {
            dwOldWidth = dmOldDevMode.dmPelsHeight;
            dwOldHeight = dmOldDevMode.dmPelsWidth;
        }

        DWORD dwCurrentWidth = dmCurrentDevMode.dmPelsWidth;
        DWORD dwCurrentHeight = dmCurrentDevMode.dmPelsHeight;
        if(dmCurrentDevMode.dmDisplayOrientation & DMDO_90 || dmCurrentDevMode.dmDisplayOrientation & DMDO_270)
        {
            dwCurrentWidth = dmCurrentDevMode.dmPelsHeight;
            dwCurrentHeight = dmCurrentDevMode.dmPelsWidth;
        }

        // Retrieve the surface description after the dev mode change.
        CDDSurfaceDesc cddsdAfterDevModeChange;
        cddsdAfterDevModeChange.dwSize = sizeof(DDSURFACEDESC);

        // Restore the surface if it has been lost.
        if(DDERR_SURFACELOST == m_piDDS->IsLost())
        {
            // A surface that owns a DC should never be lost.
            if(m_cddsd.ddsCaps.dwCaps & DDSCAPS_OWNDC)
            {
                dbgout(LOG_FAIL) << "A surface with DDSCAPS_OWNDC was lost." << endl;

                return trFail;
            }

            result = m_piDDS->Restore();
            if(DD_OK != result)
            {
                // If the surface couldn't be restored due to lack of memory
                // it's valid, pass the test.
                if(DDERR_OUTOFMEMORY == result || DDERR_OUTOFVIDEOMEMORY == result)
                {
                    dbgout << "Surface Restore Failed with an out of memory." << endl;
                    dbgout << "Passing the test..." << endl;

                    return trPass;
                }
                else
                {
                    dbgout << "Surface Restore failed with error code : "
                           << g_ErrorMap[result] << "  (" << HEX((DWORD)result)
                           << ")" << endl;
                    dbgout << "Aborting test .." << endl;
                    return trAbort;
                }
            }
        }
        else
        {
            // Ensure that Primary and overlay surfaces are lost on a dev mode change
            // or on a rotation.
            if((m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE ||
                m_cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY) &&
               (dwOldWidth != dwCurrentWidth ||
                dwOldHeight != dwCurrentHeight ||
                dmOldDevMode.dmBitsPerPel != dmCurrentDevMode.dmBitsPerPel ||
                dmOldDevMode.dmDisplayOrientation != dmCurrentDevMode.dmDisplayOrientation
               )
              )
            {
                dbgout(LOG_FAIL)
                    << "The "
                    << ((m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)?"Primary ":"Overlay ")
                    << "Surface was not lost after a dev mode change."
                    << endl;
                return trFail;
            }
        }

        LPRECT prcBoundRect = NULL;
        RECT rcBoundRect = {0, 0, m_cddsd.dwWidth, m_cddsd.dwHeight};

        // For a Primary in Windowed mode, use the Window's bounding rectangle in Lock, instead of NULL.
        if((m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
        {
            tr = GetClippingRectangle(m_piDDS.AsInParam(), rcBoundRect);
            if(trPass != tr)
            {
                dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
                return tr;
            }
            else
            {
                prcBoundRect = &rcBoundRect;
            }
        }

        // Lock the surface and get the surface description.
        //set main test window to front to avoid DDERR_SURFACENOTATTCHED error if other windows popped up
        if (!SetWindowPos(CWindowsModule::GetWinModule().m_hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE))
        {
            dbgout << "Setting main test window as top window failed" << endl;

            dbgout << "Aborting test..." << endl;

            return trAbort;
        }
        result = m_piDDS->Lock(prcBoundRect, &cddsdAfterDevModeChange, DDLOCK_WAITNOTBUSY, NULL);
        if(FAILED(result))
        {
            dbgout << "Locking the surface failed with error code : "
                   << g_ErrorMap[result] << "  (" << HEX((DWORD)result)
                   << ")" << endl;

            dbgout << "Aborting test..." << endl;

            return trAbort;
        }

        // Log the surface descriptions.
        dbgout << "Surface Description (Before DevMode change):"
               << m_cddsd
               << endl;

        dbgout << "Surface Description (After DevMode change):"
               << cddsdAfterDevModeChange
               << endl;

        // Check that the Pixelformat, Caps, Height and width flags are set.
        CheckCondition(!(cddsdAfterDevModeChange.dwFlags & DDSD_CAPS), "Caps not filled", trAbort);
        CheckCondition(!(cddsdAfterDevModeChange.dwFlags & DDSD_HEIGHT), "Height not filled", trAbort);
        CheckCondition(!(cddsdAfterDevModeChange.dwFlags & DDSD_WIDTH), "Width not filled", trAbort);
        CheckCondition(!(cddsdAfterDevModeChange.dwFlags & DDSD_PIXELFORMAT), "PixelFormat not filled", trAbort);

        // Get the Bytes Per Pixel from the PixelFormat
        DWORD dwBytesPerPixel = 0;
        if(cddsdAfterDevModeChange.ddpfPixelFormat.dwFlags & DDPF_RGB)
        {
            dwBytesPerPixel = cddsdAfterDevModeChange.ddpfPixelFormat.dwRGBBitCount/8;
        }
        else if(cddsdAfterDevModeChange.ddpfPixelFormat.dwFlags & DDPF_FOURCC)
        {
            dwBytesPerPixel = cddsdAfterDevModeChange.ddpfPixelFormat.dwYUVBitCount/8;
        }
        else if(cddsdAfterDevModeChange.ddpfPixelFormat.dwFlags & DDPF_ALPHA)
        {
            dwBytesPerPixel = cddsdAfterDevModeChange.ddpfPixelFormat.dwAlphaBitDepth/8;
        }
        else
        {
            dbgout(LOG_FAIL) << "Unable to retrieve the Bytes Per Pixel for this surface"
                             << endl
                             << "None of these flags were set in the pixel format structure"
                             << endl
                             << "DDPF_RGB, DDPF_YUV, DDPF_ALPHA"
                             <<endl;
            return trFail;
        }

        // if the srcblt color key had been set successfully in
        // CTest_DevModeChange::PreDevModeChangeTest() verify that
        // the values are correct.
        if(m_cddsd.dwFlags & DDSD_CKSRCBLT)
        {
            if(!(cddsdAfterDevModeChange.dwFlags & DDSD_CKSRCBLT))
            {
                dbgout(LOG_FAIL)
                    << "DDSD_CKSRCBLT is not set after Dev Mode Change"
                    << endl;

                tr |= trFail;
            }
            else
            {
                if(m_cddsd.ddckCKSrcBlt.dwColorSpaceLowValue
                   != cddsdAfterDevModeChange.ddckCKSrcBlt.dwColorSpaceLowValue
                  )
                {
                    dbgout(LOG_FAIL)
                        << "SrcBlt.dwColorSpaceLowValue ("
                        << HEX((unsigned int)cddsdAfterDevModeChange.ddckCKSrcBlt.dwColorSpaceLowValue)
                        <<") != "
                        << HEX((unsigned int)m_cddsd.ddckCKSrcBlt.dwColorSpaceLowValue)
                        <<endl;

                    tr |= trFail;
                }
                if(m_cddsd.ddckCKSrcBlt.dwColorSpaceHighValue
                  != cddsdAfterDevModeChange.ddckCKSrcBlt.dwColorSpaceHighValue)
                {
                    dbgout(LOG_FAIL)
                         << "SrcBlt.dwColorSpaceHighValue ("
                         << HEX((unsigned int)cddsdAfterDevModeChange.ddckCKSrcBlt.dwColorSpaceHighValue)
                         <<") != "
                         << HEX((unsigned int)m_cddsd.ddckCKSrcBlt.dwColorSpaceHighValue)
                         <<endl;

                    tr |= trFail;
                }
            }
        }

        // Perform some sanity tests on the Surface Description.
        if(!(cddsdAfterDevModeChange.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY ||
             cddsdAfterDevModeChange.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY
            )
          )
        {
            dbgout(LOG_FAIL)
                << "DDSCAPS_SYSTEMMEMORY or DDSCAPS_VIDEOMEMORY flags have not been set in dwCaps"
                << " after the device mode change"
                << endl;

            tr |= trFail;
        }

        if(((m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && !(g_DDConfig.CooperativeLevel() == DDSCL_NORMAL)) || // For Primary in Exclusive mode
           (m_cddsd.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER) ||
           (m_cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY)
          )
        {
            // Minimum of the lPitch and lXPitch should be the bytes per pixel
            DWORD dwMin = min(abs(cddsdAfterDevModeChange.lXPitch), abs(cddsdAfterDevModeChange.lPitch));
            if(dwMin != dwBytesPerPixel)
            {
                dbgout(LOG_FAIL)
                    << "Min(abs(lXpitch), abs(lPitch)) != Bytes per pixel"
                    << endl
                    << dwMin << " != " << dwBytesPerPixel
                    << endl;

                tr |= trFail;
            }

            // Max of the lPitch and lXPitch should not be less than the scanline width.
            DWORD dwMax = max(abs(cddsdAfterDevModeChange.lXPitch), abs(cddsdAfterDevModeChange.lPitch));
            DWORD dwMinVal = dwBytesPerPixel*cddsdAfterDevModeChange.dwWidth; //0 & 180
            if(DMDO_90 == dmCurrentDevMode.dmDisplayOrientation || DMDO_270 == dmCurrentDevMode.dmDisplayOrientation)
            {
                dwMinVal = dwBytesPerPixel*cddsdAfterDevModeChange.dwHeight;
            }

            if(dwMax < dwMinVal)
            {
                dbgout
                    << "WARNING: Max(abs(lXpitch), abs(lPitch)) < Bytes Per Pixel * Surface Width"
                    << endl
                    << dwMax << " < " << dwMinVal
                    << endl
                    << "The driver might be returning incorrect lPitch. Please verify."
                    << endl;
            }

            // Calculate the expected surface size
            DWORD dwExpectedSize = dwMax*cddsdAfterDevModeChange.dwHeight; //0 & 180
            if(DMDO_90 == dmCurrentDevMode.dmDisplayOrientation || DMDO_270 == dmCurrentDevMode.dmDisplayOrientation)
            {
                dwExpectedSize = dwMax*cddsdAfterDevModeChange.dwWidth;
            }

            // Some drivers may allocate more memory for the surface, so issue a warning instead of failing the test.
            if(cddsdAfterDevModeChange.dwSurfaceSize > dwExpectedSize)
            {
                dbgout
                    << "WARNING: Surface Size is greater than the Expected Surface Size"
                    << endl
                    << cddsdAfterDevModeChange.dwSurfaceSize  << " > "
                    << dwExpectedSize
                    << endl
                    << "The driver might be allocating more memory than required. Please verify."
                    << endl;
            }
            else if(cddsdAfterDevModeChange.dwSurfaceSize < dwExpectedSize)
            {
                // If a driver allocates lesser memory than the expected size,
                // signal a warning.

                dbgout
                    << "WARNING: Surface Size is less than the Expected Surface Size"
                    << endl
                    << cddsdAfterDevModeChange.dwSurfaceSize  << " < "
                    << dwExpectedSize
                    << endl
                    << "The expected surface size used in this test is caculated based on assumption of unchanged size during rotation."
                    << endl
                    << "Please verify that the tested driver should be modifying the surface size. If it should be, this warning can be ignored."
                    << endl;
            }

            // Check signs for pitches. Some hardwares won't have negative pitches for any rotation angle,
            // therefore if there is mismatch, instead of failing the test case, now we will output a 
            // warning and let user decide if it is expected.

            // Check if the sign of lPitch is proper.
            // Expected signs for                0  90   180 270
            static const int rgnPitchSigns[] =  {1,  1,  -1, -1};
            static const int rgnXPitchSigns[] = {1, -1,  -1,  1};
            int nIndex = DevMode_Helpers::GetAngle(dmCurrentDevMode.dmDisplayOrientation)/90;
            if((rgnPitchSigns[nIndex]*cddsdAfterDevModeChange.lPitch) <= 0)
            {
                dbgout
                    << "WARNING: Lpitch Sign Mismatch"
                    << endl
                    << "Expected a "  << ((rgnPitchSigns[nIndex]>0)?"positive ":"negative ")
                    << "value, but got a " << ((cddsdAfterDevModeChange.lPitch>0)?"positive ":"negative ") << "value "
                    << cddsdAfterDevModeChange.lPitch << " instead."
                    << endl;
            }

            // Check if the sign of lXPitch is proper.
            if((rgnXPitchSigns[nIndex]*cddsdAfterDevModeChange.lXPitch) <= 0)
            {
                dbgout
                    << "WARNING: Xpitch Sign Mismatch"
                    << endl
                    << "Expected a "  << ((rgnXPitchSigns[nIndex]>0)?"positive ":"negative ")
                    << "value, but got a " << ((cddsdAfterDevModeChange.lXPitch>0)?"positive ":"negative ") << "value "
                    << cddsdAfterDevModeChange.lXPitch << " instead."
                    << endl;
            }
        }

        // Check if the BB or Primary Surface dimensions match the current Dev Mode.
        if(((m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && !(g_DDConfig.CooperativeLevel() == DDSCL_NORMAL)) || // For Primary in the exclusive mode
           (m_cddsd.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER)
          )
        {
            DWORD dwExpectedWidth = dmCurrentDevMode.dmPelsWidth;
            DWORD dwExpectedHeight = dmCurrentDevMode.dmPelsHeight;
 
            if(dwExpectedWidth != cddsdAfterDevModeChange.dwWidth)
            {
                dbgout(LOG_FAIL)
                    << "Surface Width does not match current Dev Mode"
                    << endl
                    << "Expected " << dwExpectedWidth << " but got "
                    << cddsdAfterDevModeChange.dwWidth << " instead."
                    << endl;

                tr |= trFail;
            }

            if(dwExpectedHeight!= cddsdAfterDevModeChange.dwHeight)
            {
                dbgout(LOG_FAIL)
                    << "Surface Width does not match current Dev Mode"
                    << endl
                    << "Expected " << dwExpectedHeight << " but got "
                    << cddsdAfterDevModeChange.dwHeight << " instead."
                    << endl;

                tr |= trFail;
            }
        }

        // If there has been no resolution change, only a rotation has been performed.
        if((trPass == tr) &&
           (dwOldWidth == dwCurrentWidth) &&
           (dwOldHeight == dwCurrentHeight) &&
           (dmOldDevMode.dmBitsPerPel == dmCurrentDevMode.dmBitsPerPel)
          )
        {
            // The expected values of the fields will be filled here.
            CDDSurfaceDesc cddsdExpected;
            DevMode_Helpers::GetExpectedSurfDesc(cddsdExpected, m_cddsd,
                                                 dmCurrentDevMode.dmDisplayOrientation,
                                                 dmOldDevMode.dmDisplayOrientation
                                                );

            dbgout << "Expected Surface Description:" << cddsdExpected << endl;

            // Check if the value of the filled fields are the same as expected.
            tr |= Surface_Helpers::CompareSurfaceDescs( cddsdExpected,
                                                        cddsdAfterDevModeChange
                                                      );


            if(trPass != tr)
            {
                // GDI (on Smartfon builds) tries to resize a window if it exceeds the current display resolution.
                // So do not fail the test if the surface involved is a Windowed Primary & the expected width/height 
                // exceeds the current unrotated resolution.
                if((m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && 
                   (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL) &&
                   ((cddsdExpected.dwWidth > dwCurrentWidth) ||
                   (cddsdExpected.dwHeight > dwCurrentHeight))
                  )
                {                    
                    dbgout
                        << "Passing the test case even though Surface Descriptions do not match." << endl
                        << "Primary surface was resized as it exceeded the display resolution."<< endl;
                    tr = trPass;
                }
                // When the windowe size before devmode change is exactly the current resolution, the window will be treated as a full screen case
                // So do not fail the test if the surface involved is a Windowed Primary & the width/height is the resolution before devmode change
                else if (   (m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && 
                            (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL) &&      //if it is windowed primary
                            (   
                                (dwOldWidth == m_cddsd.dwWidth) &&
                                (dwOldHeight == m_cddsd.dwHeight)                   //if the before devMode change, the window size is the same as screen resolution
                            ) &&
                            (                                                       //if the failing reason is the height and width are switched
                                (cddsdExpected.dwWidth == cddsdAfterDevModeChange.dwHeight)&&
                                (cddsdExpected.dwHeight == cddsdAfterDevModeChange.dwWidth)
                            )

                        )
                {
                    dbgout
                        << "Passing the test case even if the the width and height is switched." << endl
                        << "Primary surface was rotated as the window size is the same as display resolution."<< endl
                        << "And it is treated as a Full screen case by shell team." << endl;
                    tr = trPass;
                }
                else
                {
                    dbgout(LOG_FAIL)
                        << "Surface Descriptions do not match."
                        << endl;
                }
            }
        }

        DWORD dwMemHeight = (prcBoundRect != NULL)?(prcBoundRect->bottom - prcBoundRect->top):cddsdAfterDevModeChange.dwHeight;
        DWORD dwMemWidth = (prcBoundRect != NULL)?(prcBoundRect->right - prcBoundRect->left):cddsdAfterDevModeChange.dwWidth;

        // Try reading and writing to each byte on the surface.
        // We dont care about the pixel format or the actual color.
        BYTE* next = NULL;
        __try
        {
            for(int y=0;(y < static_cast<int> (dwMemHeight)) && (trPass == tr);++y)
            {
                for(int x=0;(x < static_cast<int> (dwMemWidth)) && (trPass == tr);++x)
                {
                    next = ((BYTE*)cddsdAfterDevModeChange.lpSurface
                             +(y*cddsdAfterDevModeChange.lPitch)
                             +(x*cddsdAfterDevModeChange.lXPitch)
                           );

                   for(int z=0;(z < static_cast<int> (dwBytesPerPixel)) && (trPass == tr);++z)
                   {
                       *next = 0xFF;

                       if(*next != 0xFF)
                       {
                           dbgout(LOG_FAIL)
                               << "Memory at " << HEX((unsigned int)next)
                               << " was not set correctly."
                               << endl;

                           tr |= trFail;
                       }

                       ++next;
                   }
                }
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            dbgout(LOG_FAIL)
                << "Exception while trying to access address : "
                << HEX((unsigned int)next)
                << endl;

            tr |= trFail;
        }

        m_piDDS->Unlock(prcBoundRect);

        // Use GDI APIs to write to the DDSCAPS_OWNDC surface.
        // m_hdc is set in CIterateDevModes::TestIDirectDrawSurface
        // before the dev mode changes are made, for DDSCAPS_OWNDC surfaces.
        if(m_cddsd.ddsCaps.dwCaps & DDSCAPS_OWNDC)
        {
            dbgout << "Setting first pixel to Red" << endl;

            COLORREF crPixelRed, crPixelReturned;
            crPixelRed = SetPixel(m_hdc, 0, 0, RGB(255,0,0));
            if(-1 == static_cast<int> (crPixelRed))
            {
                tr |= trFail;
                dbgout(LOG_FAIL) << "SetPixel failed, GetLastError returns "
                                 << GetLastError()
                                 << endl;
            }

            // Get the pixel, verify that the color returned is black.
            dbgout << "Checking pixel with GetPixel" << endl;
            if((crPixelReturned = GetPixel(m_hdc, 0, 0)) != crPixelRed)
            {
                tr|=trFail;
                dbgout(LOG_FAIL)
                    << "GetPixel did not return the correct pixel color (got "
                    << HEX(crPixelReturned) << ", expected "
                    << HEX(crPixelRed)
                    << ")"
                    << endl;
            }

            // Check that IDirectDraw::GetSurfaceFromDC() succeeds if m_hdc is passed.
            dbgout << "Verifying IDirectDraw::GetSurfaceFromDC()" << endl;

            IDirectDrawSurface* ddsTemp = NULL;
            result = m_piDD->GetSurfaceFromDC(m_hdc, &ddsTemp);
            if(DD_OK != result)
            {
                dbgout(LOG_FAIL)
                       << "GetSurfaceFromDC failed with error code : "
                       << g_ErrorMap[result] << "  (" << HEX((DWORD)result)
                       << ")" << endl;
                tr |= trFail;
            }
            else if(NULL == ddsTemp)
            {
                dbgout(LOG_FAIL) << "GetSurfaceFromDC return a NULL DirectDraw Surface" << endl;
                tr |= trFail;
            }
            else
            {
                ddsTemp->Release();
            }
        }

        // Verify that GetDisplayMode swaps the width and height when orientation is 90/270.
        dbgout << "Verifying GetDisplayMode" << endl;

        CDDSurfaceDesc cddsdReturned;
        cddsdReturned.dwSize = sizeof(DDSURFACEDESC);

        result = m_piDD->GetDisplayMode(&cddsdReturned);
        CheckHRESULT(result, "GetDisplayMode", trAbort);

        CDDSurfaceDesc cddsdCorrect;
        cddsdCorrect.dwSize = sizeof(DDSURFACEDESC);
        cddsdCorrect.dwFlags = (DDSD_HEIGHT | DDSD_WIDTH);
        cddsdCorrect.dwWidth = dmCurrentDevMode.dmPelsWidth;
        cddsdCorrect.dwHeight = dmCurrentDevMode.dmPelsHeight;

        // Check if the value of the filled fields are the same as expected.
        if(trPass != Surface_Helpers::CompareSurfaceDescs( cddsdCorrect,
                                                           cddsdReturned
                                                          )
          )
        {
            dbgout << "Display Mode returned by GetDisplayMode:" << cddsdReturned << endl;
            dbgout << "Expected Display Mode:" << cddsdCorrect << endl;
            dbgout(LOG_FAIL) << "Expected Display Mode does not match returned Display Mode."
                             << endl;
            tr |= trFail;
        }

        if(trPass == tr)
        {
            if((g_DDConfig.HALCaps().dwBltCaps & DDBLTCAPS_ROTATION90)) // if the platform supports Rotated Blts in multiples of 90 degrees.
            {
                IDirectDrawSurface* piddsPrimary = g_DDSPrimary.GetObject().AsInParam();
                if(DDERR_SURFACELOST == piddsPrimary->IsLost())
                {
                    HRESULT result = piddsPrimary->Restore();
                    if(DD_OK != result)
                    {
                        // If the surface couldn't be restored due to lack of memory
                        // it's valid, pass the test.
                        if(DDERR_OUTOFMEMORY == result || DDERR_OUTOFVIDEOMEMORY == result)
                        {
                            dbgout << "Surface Restore Failed with an out of memory." << endl;
                            dbgout << "Passing the test..." << endl;
                            return trPass;
                        }
                        else
                        {
                            dbgout << "Surface Restore failed with error code : "
                                   << g_ErrorMap[result] << "  (" << HEX((DWORD)result)
                                   << ")" << endl;
                            dbgout << "Aborting test .." << endl;
                            return trAbort;
                        }
                    }
                }

                if(piddsPrimary)
                {
                    dbgout << "Rotated Blt is supported on this platform. Performing Rotated Blt tests from Surface to Primary." << endl;
                    tr = Test_IDirectDrawSurface_TWO::RotatedBltTests(false, m_piDDS.AsInParam(), piddsPrimary, 1); // Test Blt with only one rect


                    dbgout << "Rotated Blt is supported on this platform. Performing Rotated Blt tests from Primary to Surface." << endl;
                    tr |= Test_IDirectDrawSurface_TWO::RotatedBltTests(false, piddsPrimary, m_piDDS.AsInParam(), 1); // Test Blt with only one rect

                    dbgout << "Rotated Blt is supported on this platform. Performing Rotated AlphaBlt tests from Surface to Primary." << endl;
                    tr = Test_IDirectDrawSurface_TWO::RotatedBltTests(true, m_piDDS.AsInParam(), piddsPrimary, 1); // Test AlphaBlt with only one rect

                    dbgout << "Rotated Blt is supported on this platform. Performing Rotated AlphaBlt tests from Primary to Surface." << endl;
                    tr |= Test_IDirectDrawSurface_TWO::RotatedBltTests(true, piddsPrimary, m_piDDS.AsInParam(), 1); // Test AlphaBlt with only one rect
                }
            }
            else
            {
                dbgout << "Rotated Blt is not supported on this platform. Skipping the Rotated Blt tests." << endl;
            }
        }

        return tr;
    }
};
