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
////////////////////////////////////////////////////////////////////////////////
//
//  DDBVT TUX DLL
//  Copyright (c) 1996-1998, Microsoft Corporation
//
//  Module: blt_x.cpp
//          BVT for advanced Blt functions.
//
//  Revision History:
//      09/29/97    lblanco     Created.
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#pragma warning(disable : 4995)
#if TEST_BLT

////////////////////////////////////////////////////////////////////////////////
// Local types

#define NUM_ASYNC_BLTS      20
#define SLICE_STACK_SIZE    20
#define dwrand()            ((DWORD)rand())
#define MAX_CFFAIL_PER_LINE 10
#define MAX_CFFAIL_LINES    10

struct BLTINFO
{
    IDDS    m_idds;
    DDCAPS  m_ddcHAL;
};

////////////////////////////////////////////////////////////////////////////////
// Local prototypes

bool    InitBltTest(BLTINFO &, FNFINISH pfnFinish = NULL);
void    FinishBltTest(void);
bool    Help_CheckBlt(
            IDirectDrawSurface *,
            RECT *,
            IDirectDrawSurface *,
            RECT *);
bool    Help_PrepareSurfacesForBlt(IDirectDrawSurface *, IDirectDrawSurface *);
UINT    Help_GetRandomRectangles(RECT *, DWORD, RECT *, UINT);
ITPR    Help_VerifyFilledSurface(IDirectDrawSurface *, RECT &, DWORD, bool);

////////////////////////////////////////////////////////////////////////////////
// Globals for this module

static CFinishManager   *g_pfmBlt = NULL;
static BLTINFO          g_bltInfo = { 0 };
static bool             g_bInitialized = false;

////////////////////////////////////////////////////////////////////////////////
// InitBltTest
//  Creates surfaces and queries for the Blt capabilities.
//
// Parameters:
//  bi              Reference to a structure that will receive the information
//                  needed by the tests.
//  pfnFinish       Pointer to a FinishXXX function. If this parameter is not
//                  NULL, the system understands that whenever this object is
//                  destroyed, the FinishXXX function must be called as well.
//
// Return value:
//  true if successful, false otherwise.

bool InitBltTest(BLTINFO &bi, FNFINISH pfnFinish)
{
    HRESULT hr;
    IDDS    *pIDDS;

    if(!g_bInitialized)
    {
        // Get the surfaces
        if(!InitDirectDrawSurface(pIDDS, FinishBltTest))
        {
            memset(&bi, 0, sizeof(bi));
            return false;
        }

        Debug(LOG_COMMENT, TEXT("Running InitBltTest..."));

        // Get the device and emulation caps
        memset(&g_bltInfo.m_ddcHAL, 0, sizeof(DDCAPS));
        g_bltInfo.m_ddcHAL.dwSize = sizeof(DDCAPS);
        hr = pIDDS->m_pDD->GetCaps(&g_bltInfo.m_ddcHAL, NULL);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDraw,
                c_szGetCaps,
                hr);
            memset(&bi, 0, sizeof(bi));
            return false;
        }

        // Copy the surface pointers
        memcpy(&g_bltInfo.m_idds, pIDDS, sizeof(IDDS));

        g_bInitialized = true;
        Debug(LOG_COMMENT, TEXT("Done with InitBltTest"));
    }

    // Do we have a CFinishManager object?
    if(!g_pfmBlt)
    {
        g_pfmBlt = new CFinishManager;
        if(!g_pfmBlt)
        {
            FinishBltTest();
            memset(&bi, 0, sizeof(bi));
            return false;
        }
    }

    // Add the FinishXXX function to the chain.
    g_pfmBlt->AddFunction(pfnFinish);

    // Return the data
    memcpy(&bi, &g_bltInfo, sizeof(g_bltInfo));

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// FinishBltTest
//  Releases all resources created by InitBltTest.
//
// Parameters:
//  None.
//
// Return value:
//  None.

void FinishBltTest(void)
{
    // Terminate any dependent objects
    if(g_pfmBlt)
    {
        g_pfmBlt->Finish();
        delete g_pfmBlt;
        g_pfmBlt = NULL;
    }

    // Uninitialize
    memset(&g_bltInfo, 0, sizeof(g_bltInfo));
    g_bInitialized = false;
}

////////////////////////////////////////////////////////////////////////////////
// Help_CompareSurfaces
//  Compares the contents of two areas of two surfaces. The surfaces must have
//  the same pixel format and the rectangular areas must have the same
//  dimensions. The function assumes that all pointers are valid.
//
// Parameters:
//  pDDS1           First surface.
//  pRect1          Region to compare in the first surface.
//  pDDS2           Second surface.
//  pRect2          Region to compare in the second surface.
//
// Return value:
//  true if the surfaces contain the same data, false otherwise.

bool Help_CompareSurfaces(
    IDirectDrawSurface  *pDDS1,
    RECT                *pRect1,
    IDirectDrawSurface  *pDDS2,
    RECT                *pRect2)
{
    HRESULT         hr;
    DDSURFACEDESC   ddsd1,
                    ddsd2;
    bool            bSuccess = false;
    BYTE            *pLine1,
                    *pLine2;
    DWORD           dwLineSize;
    int             nLine,
                    nNumLines;

    if ((pRect1->right - pRect1->left <= pRect2->right)||
        (pRect1->bottom - pRect1->top <= pRect1->bottom))
    {
        Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("Help_CompareSurfaces was passed rectangles of")
                TEXT(" different dimensions"));
            return false;
    }
    else
    {
        if(pRect1->right - pRect1->left != pRect2->right - pRect2->left ||
        pRect1->bottom - pRect1->top != pRect2->bottom - pRect2->top)
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("Help_CompareSurfaces was passed rectangles of")
                TEXT(" different dimensions"));
            return false;
        }
    }
    memset(&ddsd1, 0, sizeof(ddsd1));
    memset(&ddsd2, 0, sizeof(ddsd2));
    ddsd1.dwSize = sizeof(ddsd1);
    ddsd2.dwSize = sizeof(ddsd2);
    __try
    {
        // Lock the first surface
        ddsd1.lpSurface = (void *)PRESET;
        hr = pDDS1->Lock(
            pRect1,
            &ddsd1,
            DDLOCK_WAITNOTBUSY,
            NULL);
        if(!CheckReturnedPointer(
            CRP_DONTRELEASE | CRP_ALLOWNONULLOUT,
            ddsd1.lpSurface,
            c_szIDirectDrawSurface,
            c_szLock,
            hr))
        {
            ddsd1.lpSurface = NULL;
            __leave;
        }

        // Lock the second surface
        ddsd2.lpSurface = (void *)PRESET;
        hr = pDDS2->Lock(
            pRect2,
            &ddsd2,
            DDLOCK_WAITNOTBUSY,
            NULL);
        if(!CheckReturnedPointer(
            CRP_NOTINTERFACE | CRP_ALLOWNONULLOUT,
            ddsd2.lpSurface,
            c_szIDirectDrawSurface,
            c_szLock,
            hr))
        {
            ddsd2.lpSurface = NULL;
            __leave;
        }

        // Determine how long a line is, for the given rectangle width, in
        // bytes
        dwLineSize = (pRect1->right - pRect1->left)*
                     ddsd1.ddpfPixelFormat.dwRGBBitCount/8;
        nNumLines = pRect1->bottom - pRect1->top;

        // Iterate over both surfaces and compare
        pLine1 = (BYTE *)ddsd1.lpSurface;
        pLine2 = (BYTE *)ddsd2.lpSurface;
        for(nLine = 0; nLine < nNumLines; nLine++)
        {
            if(memcmp(pLine1, pLine2, dwLineSize))
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("Surfaces contain different data on")
                    TEXT(" line %d"),
                    nLine);
                __leave;
            }

            pLine1 += ddsd1.lPitch;
            pLine2 += ddsd2.lPitch;
        }

        Debug(LOG_PASS, TEXT("Surfaces compare OK"));
        bSuccess = true;
    }
    __finally
    {
        if(ddsd1.lpSurface)
        {
            pDDS1->Unlock(NULL);
        }

        if(ddsd2.lpSurface)
        {
            pDDS2->Unlock(NULL);
        }
    }

    return bSuccess;
}

////////////////////////////////////////////////////////////////////////////////
// Help_PrepareSurfacesForBlt
//  Fills two surfaces with different data so that it is later obvious whether
//  a correct Blt between the two surfaces happened or not.
//
// Parameters:
//  pDDS1           The first surface.
//  pDDS2           The second surface.
//
// Return value:
//  true if successful, false otherwise.

bool Help_PrepareSurfacesForBlt(
    IDirectDrawSurface  *pDDS1,
    IDirectDrawSurface  *pDDS2)
{
    HRESULT hr;
    DDBLTFX fx;
    DWORD   dwColor1,
            dwColor2;

    // Pick two colors at random
    dwColor1 = dwrand();
    do
    {
        dwColor2 = dwrand();
    } while(dwColor2 == dwColor1);

    memset(&fx, 0, sizeof(fx));
    fx.dwSize = sizeof(fx);

    // Fill the first surface with color #1
    fx.dwFillColor = dwColor1;
    hr = pDDS1->Blt(
        NULL,
        NULL,
        NULL,
        DDBLT_COLORFILL | DDBLT_WAITNOTBUSY,
        &fx);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szBlt,
            hr,
            TEXT("DDBLT_COLORFILL"),
            TEXT("for first surface"));
        return false;
    }

    // Fill the second surface with color #2
    fx.dwFillColor = dwColor2;
    hr = pDDS2->Blt(
        NULL,
        NULL,
        NULL,
        DDBLT_COLORFILL | DDBLT_WAITNOTBUSY,
        &fx);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szBlt,
            hr,
            TEXT("DDBLT_COLORFILL"),
            TEXT("for second surface"));
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Help_GetRandomRectangles
//  Creates random-sized non-overlapping non-empty rectangles. No parameter
//  validation is performed for speed. This version is recursive; watch out for
//  large numbers of rectangles. Also, when specifying too many rectangles it is
//  possible that they don't fit (because their "allocated" sub-regions shrink
//  below one pixel of width and/or height).
//
// Parameters:
//  pRect           The enclosing rectangle (typically, the total surface area).
//  dwFlags         Reserved at this point; must be zero.
//  pRectOut        Pointer to an array of rectangles to be returned.
//  nRects          Number of rectangles to generate. The array in rgRectOut
//                  must be large enough to contain this many rectangles.
//
// Return value:
//  The number of rectangles generated.

UINT Help_GetRandomRectangles(
    RECT    *pRect,
    DWORD   dwFlags,
    RECT    *pRectOut,
    UINT    nRects)
{
    UINT    nGenerated = 0;
    LONG    lWidth,
            lHeight;
    UINT    nCurSlice = 0,
            nDepth = 1;

    struct
    {
        RECT    m_rect;
        UINT    m_nRects;
    } slice[SLICE_STACK_SIZE];

    DebugBeginLevel(
        0,
        TEXT("Help_GetRandomRectangles(0x%08x, 0x%08x, 0x%08x, %d) called"),
        pRect,
        dwFlags,
        pRectOut,
        nRects);

    // Push the given parameters into our stack
    slice[0].m_rect = *pRect;
    slice[0].m_nRects = nRects;
    nCurSlice++;

    // Process until we are done
    while(nCurSlice--)
    {
        lWidth = slice[nCurSlice].m_rect.right - slice[nCurSlice].m_rect.left;
        lHeight = slice[nCurSlice].m_rect.bottom - slice[nCurSlice].m_rect.top;

        if(lWidth == 0 || lHeight == 0)
        {
            // This comprises a problem, because we can't generate any
            // rectangles in a region of area zero
            continue;
        }

        nRects = slice[nCurSlice].m_nRects;

        // If many rectangles are to be generated, slice the original rectangle
        // into two regions.
        if(nRects > 1)
        {
            UINT    nRects1,
                    nRects2,
                    nMinRects;
            RECT    rcTemp,
                    *prcNewEnclosing2;

            // We can slice vertically or horizontally. We'll first determine
            // how many rectangles will go in each sub-area, then we'll divide
            // the area accordingly, to within 10%. To slice the rectangle,
            // we'll replace the current rectangle with one slice, and push the
            // other slice on top of it. If we don't have enough room, we'll
            // have to use recursion to allocate a new stack
            nRects1 = dwrand()%(nRects - 1) + 1;
            nRects2 = nRects - nRects1;
            if(nRects1 < nRects2)
            {
                nMinRects = nRects1;
            }
            else
            {
                nMinRects = nRects2;
            }

            RECT &rcNewEnclosing1 = slice[nCurSlice].m_rect;
            if(nCurSlice == SLICE_STACK_SIZE - 1)
            {
                prcNewEnclosing2 = &rcTemp;
            }
            else
            {
                prcNewEnclosing2 = &slice[nCurSlice + 1].m_rect;
                slice[nCurSlice + 1].m_nRects = nRects2;
            }
            memcpy(prcNewEnclosing2, &slice[nCurSlice].m_rect, sizeof(RECT));
            slice[nCurSlice].m_nRects = nRects1;

            if(dwrand()%2)
            {
                // Slice vertically
                rcNewEnclosing1.right =
                    slice[nCurSlice].m_rect.left +
                    (10*lWidth*nRects1 - lWidth*nMinRects +
                     dwrand()%(2*lWidth*nMinRects + 1))/
                    (10*nRects);
                prcNewEnclosing2->left = rcNewEnclosing1.right;
            }
            else
            {
                // Slice horizontally
                rcNewEnclosing1.bottom =
                    slice[nCurSlice].m_rect.top +
                    (10*lHeight*nRects1 - lHeight*nMinRects +
                     dwrand()%(2*lHeight*nMinRects + 1))/
                    (10*nRects);
                prcNewEnclosing2->top = rcNewEnclosing1.bottom;
            }

            // We've pushed a new element into the stack
            nCurSlice++;

            // If we've gone over, we need to perform a recursive step here
            if(nCurSlice == SLICE_STACK_SIZE)
            {
                nGenerated += Help_GetRandomRectangles(
                    prcNewEnclosing2,
                    dwFlags,
                    pRectOut,
                    nRects2);
                pRectOut += nRects2;
            }
            else
            {
                // No need for recursion; just push this rectangle back into
                // the stack
                nCurSlice++;
            }
        }
        else
        {
            LONG    lSize;

            // Just pick a random rectangle inside the enclosing area. First,
            // figure out random horizontal coordinates.
            lSize = dwrand()%lWidth + 1;
            pRectOut->left = slice[nCurSlice].m_rect.left +
                             dwrand()%(lWidth - lSize + 1);
            pRectOut->right = pRectOut->left + lSize;

            // Then, figure out random vertical coordinates
            lSize = dwrand()%lHeight + 1;
            pRectOut->top = slice[nCurSlice].m_rect.top +
                            dwrand()%(lHeight - lSize + 1);
            pRectOut->bottom = pRectOut->top + lSize;

            // One more rectangle generated; move to generate the next one
            nGenerated++;
            pRectOut++;
        }

        if(nCurSlice > nDepth)
            nDepth = nCurSlice;
    }

    DebugEndLevel(TEXT("Maximum depth reached was %d"), nDepth);

    return nGenerated;
}

////////////////////////////////////////////////////////////////////////////////
// Test_Blt_Async
//  Tests asynchronous Blt's.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_Blt_Async(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    return TPRFromITPR(ITPR_SKIP);
/*    
    HRESULT hr;
    BLTINFO bi;
    RECT    rect[NUM_ASYNC_BLTS],
            rectAll;
    int     nIndex;
    ITPR    nITPR = ITPR_PASS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitBltTest(bi))
        return TPRFromITPR(g_tprReturnVal);
        
    // We can't run if we don't have a system memory surface
    if(!bi.m_idds.m_pDDSSysMem)
    {
        Debug(
            LOG_ABORT,
            TEXT("This test requires a system memory surface"));
        return TPR_ABORT;
    }

    // Make sure the two surfaces contain different data (or we won't know if
    // a Blt ever occurred)
    if(!Help_PrepareSurfacesForBlt(
        bi.m_idds.m_pDDSPrimary,
        bi.m_idds.m_pDDSSysMem))
    {
        return TPR_ABORT;
    }

    rectAll.left = 0;
    rectAll.right = bi.m_idds.m_ddsdPrimary.dwWidth;
    rectAll.top = 0;
    rectAll.bottom = bi.m_idds.m_ddsdPrimary.dwHeight;

    // Generate random areas over which we'll do the Blts
    Help_GetRandomRectangles(
        &rectAll,
        0,
        rect,
        NUM_ASYNC_BLTS);

    // Do some async Blt's
    for(nIndex = 0; nIndex < NUM_ASYNC_BLTS; nIndex++)
    {
        hr = bi.m_idds.m_pDDSPrimary->Blt(
            &rect[nIndex],
            bi.m_idds.m_pDDSSysMem,
            &rect[nIndex],
            DDBLT_ASYNC,
            NULL);
        if(FAILED(hr))
        {
            TCHAR szMessage[64];

            wsprintf(szMessage, TEXT("on attempt #%d"), nIndex + 1);
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szBlt,
                hr,
                TEXT("DDBLT_ASYNC"),
                szMessage);
            return TPR_FAIL;
        }
    }

    // Compare the data for the Blts
    for(nIndex = 0; nIndex < NUM_ASYNC_BLTS; nIndex++)
    {
        if(!Help_CompareSurfaces(
            bi.m_idds.m_pDDSPrimary,
            &rect[0],
            bi.m_idds.m_pDDSSysMem,
            &rect[0]))
        {
            nITPR |= ITPR_FAIL;
        }
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, TEXT("Asynchronous Blts"));

    return TPRFromITPR(nITPR);
*/
}

////////////////////////////////////////////////////////////////////////////////
// Test_Blt_ColorFill
//  Tests color fills.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_Blt_ColorFill(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    BLTINFO bi;
    RECT    rect;
    ITPR    nITPR = ITPR_PASS;
    HRESULT hr;
    DDBLTFX fx;
    DWORD   dwFillColor;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitBltTest(bi))
        return TPRFromITPR(g_tprReturnVal);

    // Select a random region of the primary to do a color fill
    rect.left   = 0;
    rect.top    = 0;
    rect.right  = bi.m_idds.m_ddsdPrimary.dwWidth;
    rect.bottom = bi.m_idds.m_ddsdPrimary.dwHeight;
    Help_GetRandomRectangles(&rect, 0, &rect, 1);

    // Select a random fill color
    dwFillColor = (DWORD)rand()<<16 | (DWORD)rand();

    // Log some information
    Debug(
        LOG_DETAIL,
        TEXT("Filling region { %d, %d, %d, %d } with 0x%08X"),
        rect.left,
        rect.top,
        rect.right,
        rect.bottom,
        dwFillColor);

    // Set up the parameters and call the method
    memset(&fx, 0, sizeof(fx));
    fx.dwSize       = sizeof(fx);
    fx.dwFillColor  = dwFillColor;
    hr = bi.m_idds.m_pDDSPrimary->Blt(
        &rect,
        NULL,
        NULL,
        DDBLT_COLORFILL | DDBLT_WAITNOTBUSY,
        &fx);
    if(FAILED(hr))
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDrawSurface,
            c_szBlt,
            hr,
            TEXT("DDBLT_COLORFILL"),
            TEXT("for primary"));
        nITPR |= ITPR_FAIL;
    }
    else
    {
        // Verify that the surface has been filled with this value
        nITPR |= Help_VerifyFilledSurface(
            bi.m_idds.m_pDDSPrimary,
            rect,
            dwFillColor,
            false);
    }

    if(nITPR == ITPR_PASS)
    {
        Report(
            RESULT_SUCCESS,
            c_szIDirectDrawSurface,
            c_szBlt,
            0,
            TEXT("DDBLT_COLORFILL"));
    }

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Help_VerifyFilledSurface
//  Verifies that the given region of the given surface is filled with the
//  given fill color.
//
// Parameters:
//  pDDS            The IDirectDrawSurface pointer.
//  rect            The rectangle to verify.
//  dwFillColor     The expected fill color.
//  bCheckOutside   If true, the function verifies that the rest of the surface
//                  is not filled with the given fill color. Otherwise, it
//                  doesn't perform that check.
//
// Return value:
//  ITPR_PASS if everything checks, ITPR_FAIL if it doesn't, or ITPR_ABORT if
//  the verification process is blocked by a different failure.

ITPR Help_VerifyFilledSurface(
    IDirectDrawSurface  *pDDS,
    RECT                &rect,
    DWORD               dwFillColor,
    bool                bCheckOutside)
{
    HRESULT hr;
    ITPR    nITPR = ITPR_PASS;
    void    *pLine,
            *pPixel;
    int     x,
            y,
            nBPP;
    DWORD   dwPixel = 0;
    TCHAR   szErrorFormat[64];
    int     nFailed,
            nFailedLines;

    DDSURFACEDESC   ddsd;

    // Lock the entire surface
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    hr = pDDS->Lock(NULL, &ddsd, DDLOCK_WAITNOTBUSY, NULL);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szLock,
            hr);
        return ITPR_ABORT;
    }
    nBPP = ddsd.ddpfPixelFormat.dwRGBBitCount;

    // Based on the surface's pixel format, determine the correct fill
    // value (some of the DWORD bits will have been truncated)
    dwFillColor &= (
        ddsd.ddpfPixelFormat.dwRBitMask |
        ddsd.ddpfPixelFormat.dwGBitMask |
        ddsd.ddpfPixelFormat.dwBBitMask);

    // Prepare a formatting string, in case of error
    _stprintf_s(
        szErrorFormat,
        TEXT("Pixel at (%%3d, %%3d) was expected to be 0x%%0%dX")
        TEXT(" (was 0x%%0%dX)"),
        nBPP/4,
        nBPP/4);

    // Verify the region that should be filled.
    nFailedLines = 0;
    pLine = (void *)((BYTE *)ddsd.lpSurface + ddsd.lPitch*rect.top);
    for(y = rect.top; y < rect.bottom; y++)
    {
        nFailed = 0;
        pPixel = (void *)((BYTE *)pLine + rect.left*nBPP/8);
        for(x = rect.left; x < rect.right; x++)
        {
            switch(nBPP)
            {
            case 8:
                dwPixel = DWORDREADBYTE(pPixel);
                break;

            case 16:
                dwPixel = DWORDREADWORD(pPixel);
                break;

            case 32:
                dwPixel = (DWORD)(*(DWORD *)pPixel);
                break;
            }

            if(dwPixel != dwFillColor)
            {
                if(nFailed++ == MAX_CFFAIL_PER_LINE)
                {
                    Debug(
                        LOG_FAIL,
                        TEXT("(there are additional mismatches in line %d)"),
                        y);
                    break;
                }
                else if(nFailedLines == MAX_CFFAIL_LINES)
                {
                    break;
                }
                else
                {
                    Debug(
                        LOG_FAIL,
                        szErrorFormat,
                        x,
                        y,
                        dwFillColor,
                        dwPixel);
                }
                nITPR |= ITPR_FAIL;
            }

            pPixel = (void *)((BYTE *)pPixel + nBPP/8);
        }

        pLine = (void *)((BYTE *)pLine + ddsd.lPitch);
        if(nFailed)
        {
            if(nFailedLines++ == MAX_CFFAIL_LINES)
            {
                Debug(
                    LOG_FAIL,
                    TEXT("(there are additional mismatches in other lines"));
                break;
            }
        }
    }

    // Unlock the surface
    hr = pDDS->Unlock(NULL);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szUnlock,
            hr);
        nITPR |= ITPR_ABORT;
    }

    return nITPR;
}
#endif // TEST_BLT

////////////////////////////////////////////////////////////////////////////////
