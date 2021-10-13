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
#include "DDraw_Utils.h"

void GenRandRect(DWORD dwWidth, DWORD dwHeight, RECT *prc)
{
    // This is really random - tends toward smaller rects
    // Pick two random points [ 0, dwHeight ]
    dwWidth++;
    dwHeight++;
    POINT pt1 = { rand() % dwWidth, rand() % dwHeight },
          pt2 = { rand() % dwWidth, rand() % dwHeight };

    // Make sure the points are not horizontally or vertically colinear.
    if (pt1.x == pt2.x) pt2.x = (pt2.x+1) % dwWidth;
    if (pt1.y == pt2.y) pt2.y = (pt2.y+1) % dwHeight;

    prc->top    = min(pt1.y, pt2.y);
    prc->left   = min(pt1.x, pt2.x);
    prc->bottom = max(pt1.y, pt2.y);
    prc->right  = max(pt1.x, pt2.x);
}

////////////////////////////////////////////////////////////////////////////////
// GenRandRect
//  Fills in RECT pointed to by prcRand with a random rectangle contained
// within the RECT pointed to by prcBound
//
void GenRandRect(RECT *prcBound, RECT *prcRand)
{
    // Get random rectangles based at (0,0)
    GenRandRect(prcBound->right - prcBound->left, prcBound->bottom - prcBound->top, prcRand);
    // Shift to fit in bounding rect
    prcRand->top += prcBound->top;
    prcRand->bottom += prcBound->top;
    prcRand->left += prcBound->left;
    prcRand->right += prcBound->left;
}

BOOL FillSurfaceHorizontal(IDirectDrawSurface *piDDS, DWORD dwColor1, DWORD dwColor2, const RECT * prcWindow)
{
    BOOL fReturnValue = TRUE;
    DDSURFACEDESC ddsd;
    HRESULT hr;
    HRESULT hrFailed = S_OK;
    RECT rc;

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    const int nRectHeight = (rand() % 20)+1;
    
    if(FAILED(hr = piDDS->GetSurfaceDesc(&ddsd)))
    {
        LogFail(_T("Unable to get surfacedesc. hr:%s"), ErrorName[hr]);
        fReturnValue = FALSE;
    }
    
    rc.top = 0;
    rc.left= 0;
    rc.right=ddsd.dwWidth;
    
    if(ddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
    {
        rc = *prcWindow;

        ddsd.dwWidth = rc.right - rc.left;
        ddsd.dwHeight = rc.bottom - rc.top;
        
    }
    rc.bottom=rc.top+nRectHeight;
    
    // Fill the surface with horizontal rectangles of alternating colors

    bool fKey = true;
    LONG lOffset=rc.top;
    
    while (rc.bottom <= (LONG)(ddsd.dwHeight+lOffset))
    {
        DDBLTFX ddbltfx;
        memset(&ddbltfx, 0, sizeof(ddbltfx));
        ddbltfx.dwSize = sizeof(ddbltfx);
        
        ddbltfx.dwFillColor = fKey ? dwColor1 : dwColor2;

        for(int i = 0; i <= TRIES; i++)
        {   
            hr = piDDS->Blt(&rc, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, &ddbltfx);
            if(SUCCEEDED(hr))
                break;
            if(DDERR_SURFACELOST == hr)
            {
                piDDS->Restore();
                continue;
            }
            if(hr != DDERR_SURFACEBUSY || i == TRIES)
            {
                // BUGBUG: This message should be uncommented
                //LogFail(_T("FillSurface Horizontal unable to colorfill surface. hr:%s"), ErrorName[hr]);
                hrFailed = hr;
                fReturnValue = FALSE;
                break;
            }
        }
        
        // Shift the rect down, and toggle its color
        rc.top += nRectHeight;
        rc.bottom += nRectHeight;
        fKey = !fKey;
    }

    if (FAILED(hrFailed))
    {
        LogFail(_T("FillSurface Horizontal unable to colorfill surface. Last error returned was %s"), ErrorName[hrFailed]);
    }

    return fReturnValue;
}

BOOL FillSurfaceVertical(IDirectDrawSurface *piDDS, DWORD dwColor1, DWORD dwColor2, const RECT * prcWindow)
{
    BOOL fReturnValue = TRUE;
    DDSURFACEDESC ddsd;
    HRESULT hr;
    HRESULT hrFailed = S_OK;
    RECT rc;

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    const int nRectWidth = (rand() % 20)+1;
    
    if(FAILED(hr = piDDS->GetSurfaceDesc(&ddsd)))
    {
        LogFail(_T("Unable to get surfacedesc. hr:%s"), ErrorName[hr]);
        fReturnValue = FALSE;
    }
    
    rc.top = 0;
    rc.left= 0;
    rc.bottom=ddsd.dwHeight;
    
    if(ddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
    {
        rc = *prcWindow;
        ddsd.dwWidth = rc.right - rc.left;
        ddsd.dwHeight = rc.bottom - rc.top;
    }
    
    rc.right=rc.left+nRectWidth;
    // Fill the surface with horizontal rectangles of alternating colors

    
    bool fKey = true;
    long lOffset=rc.left;
    
    while (rc.right <= (LONG)(ddsd.dwWidth+lOffset))
    {
        DDBLTFX ddbltfx;
        memset(&ddbltfx, 0, sizeof(ddbltfx));
        ddbltfx.dwSize = sizeof(ddbltfx);
        
        ddbltfx.dwFillColor = fKey ? dwColor1 : dwColor2;

        for(int i = 0; i <= TRIES; i++)
        {   
            hr = piDDS->Blt(&rc, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, &ddbltfx);
            if(SUCCEEDED(hr))
                break;
            if(DDERR_SURFACELOST == hr)
            {
                piDDS->Restore();
                continue;
            }
            if(hr != DDERR_SURFACEBUSY || i == TRIES)
            {
                // BUGBUG: This message should be uncommented
//                LogFail(_T("FillSurface Vertical unable to colorfill surface. hr:%s"), ErrorName[hr]);
                hrFailed = hr;
                fReturnValue = FALSE;
                break;
            }
        }

        // Shift the rect down, and toggle its color
        rc.left += nRectWidth;
        rc.right += nRectWidth;
        fKey = !fKey;
    }
    if (FAILED(hrFailed))
    {
        LogFail(_T("FillSurface Vertical unable to colorfill surface. Last error returned was %s"), ErrorName[hrFailed]);
    }

    return fReturnValue;
}

TCHAR  *
cMapClass::operator[] (int index)
{
    // initialize it to entry 0's string, which is "skip"
    TCHAR  *
        tcTmp = m_EntryStruct[0].szName;

    for (int i = 0; NULL != m_EntryStruct[i].szName; i++)
        if (m_EntryStruct[i].nVal == index)
            tcTmp = m_EntryStruct[i].szName;

    return tcTmp;
}

#define  ENTRY(x)   { x, TEXT(#x) }

NameValPair ErrorEntryStruct[] =
{
    ENTRY(DDERR_CURRENTLYNOTAVAIL),
    ENTRY(DDERR_GENERIC),
    ENTRY(DDERR_HEIGHTALIGN),
    ENTRY(DDERR_INCOMPATIBLEPRIMARY),
    ENTRY(DDERR_INVALIDCAPS),
    ENTRY(DDERR_INVALIDCLIPLIST),
    ENTRY(DDERR_INVALIDMODE),
    ENTRY(DDERR_INVALIDOBJECT),
    ENTRY(DDERR_INVALIDPARAMS),
    ENTRY(DDERR_INVALIDPIXELFORMAT),
    ENTRY(DDERR_INVALIDRECT),
    ENTRY(DDERR_LOCKEDSURFACES),
    ENTRY(DDERR_NOCLIPLIST),
    ENTRY(DDERR_NOALPHAHW),
    ENTRY(DDERR_NOCOLORCONVHW),
    ENTRY(DDERR_NOCOOPERATIVELEVELSET),
    ENTRY(DDERR_NOCOLORKEYHW),
    ENTRY(DDERR_NOFLIPHW),
    ENTRY(DDERR_NOTFOUND),
    ENTRY(DDERR_NOOVERLAYHW),
    ENTRY(DDERR_OVERLAPPINGRECTS),
    ENTRY(DDERR_NORASTEROPHW),
    ENTRY(DDERR_NOSTRETCHHW),
    ENTRY(DDERR_NOVSYNCHW),
    ENTRY(DDERR_NOZOVERLAYHW),
    ENTRY(DDERR_OUTOFMEMORY),
    ENTRY(DDERR_OUTOFVIDEOMEMORY),
    ENTRY(DDERR_PALETTEBUSY),
    ENTRY(DDERR_COLORKEYNOTSET),
    ENTRY(DDERR_SURFACEBUSY),
    ENTRY(DDERR_CANTLOCKSURFACE),
    ENTRY(DDERR_SURFACELOST),
    ENTRY(DDERR_TOOBIGHEIGHT),
    ENTRY(DDERR_TOOBIGSIZE),
    ENTRY(DDERR_TOOBIGWIDTH),
    ENTRY(DDERR_UNSUPPORTED),
    ENTRY(DDERR_VERTICALBLANKINPROGRESS),
    ENTRY(DDERR_WASSTILLDRAWING),
    ENTRY(DDERR_DIRECTDRAWALREADYCREATED),
    ENTRY(DDERR_PRIMARYSURFACEALREADYEXISTS),
    ENTRY(DDERR_CLIPPERISUSINGHWND),
    ENTRY(DDERR_NOCLIPPERATTACHED),
    ENTRY(DDERR_NOPALETTEATTACHED),
    ENTRY(DDERR_NOPALETTEHW),
    ENTRY(DDERR_NOBLTHW),
    ENTRY(DDERR_OVERLAYNOTVISIBLE),
    ENTRY(DDERR_NOOVERLAYDEST),
    ENTRY(DDERR_INVALIDPOSITION),
    ENTRY(DDERR_NOTAOVERLAYSURFACE),
    ENTRY(DDERR_EXCLUSIVEMODEALREADYSET),
    ENTRY(DDERR_NOTFLIPPABLE),
    ENTRY(DDERR_NOTLOCKED),
    ENTRY(DDERR_CANTCREATEDC),
    ENTRY(DDERR_NODC),
    ENTRY(DDERR_WRONGMODE),
    ENTRY(DDERR_IMPLICITLYCREATED),
    ENTRY(DDERR_NOTPALETTIZED),
    ENTRY(DDERR_DCALREADYCREATED),
    ENTRY(DDERR_MOREDATA),
    ENTRY(DDERR_VIDEONOTACTIVE),
    ENTRY(DDERR_DEVICEDOESNTOWNSURFACE),
};

MapClass ErrorName(&ErrorEntryStruct[0]);
