//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

BOOL FillSurfaceHorizontal(IDirectDrawSurface *piDDS, DWORD dwColor1, DWORD dwColor2)
{
    BOOL fReturnValue = TRUE;
    DDSURFACEDESC ddsd;
    HRESULT hr;
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
        if(!GetWindowRect(g_hwndMain, &rc))
        {
            LogFail(_T("Unable to get main window rect. GetLastError: %d"), GetLastError());
            fReturnValue = FALSE;
        }

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
            hr = piDDS->Blt(&rc, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &ddbltfx);
            if(SUCCEEDED(hr))
                break;
            if(hr != DDERR_SURFACEBUSY || i == TRIES)
            {
                LogFail(_T("FillSurface Horizontal unable to colorfill surface. hr:%s"), ErrorName[hr]);
                fReturnValue = FALSE;
            }
        }
        
        // Shift the rect down, and toggle its color
        rc.top += nRectHeight;
        rc.bottom += nRectHeight;
        fKey = !fKey;
    }

    return fReturnValue;
}

BOOL FillSurfaceVertical(IDirectDrawSurface *piDDS, DWORD dwColor1, DWORD dwColor2)
{
    BOOL fReturnValue = TRUE;
    DDSURFACEDESC ddsd;
    HRESULT hr;
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
        if(!GetWindowRect(g_hwndMain, &rc))
        {
            LogFail(_T("Unable to get main window rect. hr:%s"), ErrorName[hr]);
            fReturnValue = FALSE;
        }

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
            hr = piDDS->Blt(&rc, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &ddbltfx);
            if(SUCCEEDED(hr))
                break;
            if(hr != DDERR_SURFACEBUSY || i == TRIES)
            {
                LogFail(_T("FillSurface Vertical unable to colorfill surface. hr:%s"), ErrorName[hr]);
                fReturnValue = FALSE;
            }
        }

        // Shift the rect down, and toggle its color
        rc.left += nRectWidth;
        rc.right += nRectWidth;
        fKey = !fKey;
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
        ENTRY(DD_OK),
        ENTRY(DD_FALSE),
        ENTRY(DDERR_ALREADYINITIALIZED),
        ENTRY(DDERR_BLTFASTCANTCLIP),
        ENTRY(DDERR_CANNOTATTACHSURFACE),
        ENTRY(DDERR_CANNOTDETACHSURFACE),
        ENTRY(DDERR_CANTCREATEDC),
        ENTRY(DDERR_CANTDUPLICATE),
        ENTRY(DDERR_CANTLOCKSURFACE),
        ENTRY(DDERR_CANTPAGELOCK),
        ENTRY(DDERR_CANTPAGEUNLOCK),
        ENTRY(DDERR_CLIPPERISUSINGHWND),
        ENTRY(DDERR_COLORKEYNOTSET),
        ENTRY(DDERR_CURRENTLYNOTAVAIL),
        ENTRY(DDERR_DCALREADYCREATED),
        ENTRY(DDERR_DIRECTDRAWALREADYCREATED),
        ENTRY(DDERR_EXCEPTION),
        ENTRY(DDERR_EXCLUSIVEMODEALREADYSET),
        ENTRY(DDERR_GENERIC),
        ENTRY(DDERR_HEIGHTALIGN),
        ENTRY(DDERR_HWNDALREADYSET),
        ENTRY(DDERR_HWNDSUBCLASSED),
        ENTRY(DDERR_IMPLICITLYCREATED),
        ENTRY(DDERR_INCOMPATIBLEPRIMARY),
        ENTRY(DDERR_INVALIDCAPS),
        ENTRY(DDERR_INVALIDCLIPLIST),
        ENTRY(DDERR_INVALIDDIRECTDRAWGUID),
        ENTRY(DDERR_INVALIDMODE),
        ENTRY(DDERR_INVALIDOBJECT),
        ENTRY(DDERR_INVALIDPARAMS),
        ENTRY(DDERR_INVALIDPIXELFORMAT),
        ENTRY(DDERR_INVALIDPOSITION),
        ENTRY(DDERR_INVALIDRECT),
        ENTRY(DDERR_INVALIDSURFACETYPE),
        ENTRY(DDERR_LOCKEDSURFACES),
        ENTRY(DDERR_NO3D),
        ENTRY(DDERR_NOALPHAHW),
        ENTRY(DDERR_NOBLTHW),
        ENTRY(DDERR_NOCLIPLIST),
        ENTRY(DDERR_NOCLIPPERATTACHED),
        ENTRY(DDERR_NOCOLORCONVHW),
        ENTRY(DDERR_NOCOLORKEY),
        ENTRY(DDERR_NOCOLORKEYHW),
        ENTRY(DDERR_NOCOOPERATIVELEVELSET),
        ENTRY(DDERR_NODC),
        ENTRY(DDERR_NODDROPSHW),
        ENTRY(DDERR_NODIRECTDRAWHW),
        ENTRY(DDERR_NODIRECTDRAWSUPPORT),
        ENTRY(DDERR_NOEMULATION),
        ENTRY(DDERR_NOEXCLUSIVEMODE),
        ENTRY(DDERR_NOFLIPHW),
        ENTRY(DDERR_NOGDI),
        ENTRY(DDERR_NOHWND),
        ENTRY(DDERR_NOMIPMAPHW),
        ENTRY(DDERR_NOMIRRORHW),
        ENTRY(DDERR_NOOVERLAYDEST),
        ENTRY(DDERR_NOOVERLAYHW),
        ENTRY(DDERR_NOPALETTEATTACHED),
        ENTRY(DDERR_NOPALETTEHW),
        ENTRY(DDERR_NORASTEROPHW),
        ENTRY(DDERR_NOROTATIONHW),
        ENTRY(DDERR_NOSTRETCHHW),
        ENTRY(DDERR_NOT4BITCOLOR),
        ENTRY(DDERR_NOT4BITCOLORINDEX),
        ENTRY(DDERR_NOT8BITCOLOR),
        ENTRY(DDERR_NOTAOVERLAYSURFACE),
        ENTRY(DDERR_NOTEXTUREHW),
        ENTRY(DDERR_NOTFLIPPABLE),
        ENTRY(DDERR_NOTFOUND),
        ENTRY(DDERR_NOTINITIALIZED),
        ENTRY(DDERR_NOTLOCKED),
        ENTRY(DDERR_NOTPAGELOCKED),
        ENTRY(DDERR_NOTPALETTIZED),
        ENTRY(DDERR_NOVSYNCHW),
        ENTRY(DDERR_NOZBUFFERHW),
        ENTRY(DDERR_NOZOVERLAYHW),
        ENTRY(DDERR_OUTOFCAPS),
        ENTRY(DDERR_OUTOFMEMORY),
        ENTRY(DDERR_OUTOFVIDEOMEMORY),
        ENTRY(DDERR_OVERLAYCANTCLIP),
        ENTRY(DDERR_OVERLAYCOLORKEYONLYONEACTIVE),
        ENTRY(DDERR_OVERLAYNOTVISIBLE),
        ENTRY(DDERR_OVERLAPPINGRECTS),
        ENTRY(DDERR_PALETTEBUSY),
        ENTRY(DDERR_PRIMARYSURFACEALREADYEXISTS),
        ENTRY(DDERR_REGIONTOOSMALL),
        ENTRY(DDERR_SURFACEALREADYATTACHED),
        ENTRY(DDERR_SURFACEALREADYDEPENDENT),
        ENTRY(DDERR_SURFACEBUSY),
        ENTRY(DDERR_SURFACEISOBSCURED),
        ENTRY(DDERR_SURFACELOST),
        ENTRY(DDERR_SURFACENOTATTACHED),
        ENTRY(DDERR_TOOBIGHEIGHT),
        ENTRY(DDERR_TOOBIGSIZE),
        ENTRY(DDERR_TOOBIGWIDTH),
        ENTRY(DDERR_UNSUPPORTED),
        ENTRY(DDERR_UNSUPPORTEDFORMAT),
        ENTRY(DDERR_UNSUPPORTEDMASK),
        ENTRY(DDERR_UNSUPPORTEDMODE),
        ENTRY(DDERR_VERTICALBLANKINPROGRESS),
        ENTRY(DDERR_WASSTILLDRAWING),
        ENTRY(DDERR_WRONGMODE),
        ENTRY(DDERR_XALIGN),
        ENTRY(DDERR_MOREDATA),
        ENTRY(DDERR_EXPIRED),
        ENTRY(DDERR_VIDEONOTACTIVE),
        ENTRY(DDERR_DEVICEDOESNTOWNSURFACE),
        ENTRY(E_NOINTERFACE),
};

MapClass ErrorName(&ErrorEntryStruct[0]);
