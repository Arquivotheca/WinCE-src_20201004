//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once
#if !defined(__DDRAW_UTILS_H__)
#define __DDRAW_UTILS_H__

#include "ddraw.h"
#include <stressutils.h>

//******************************************************************************
// Function Prototypes

void GenRandRect(DWORD dwWidth, DWORD dwHeight, RECT *prc);
void GenRandRect(RECT *prcBound, RECT *prcRand);

BOOL FillSurfaceHorizontal(IDirectDrawSurface *piDDS, DWORD dwColor1, DWORD dwColor2);
BOOL FillSurfaceVertical(IDirectDrawSurface *piDDS, DWORD dwColor1, DWORD dwColor2);

//******************************************************************************
// Classes and structures

struct NameValPair {
   int nVal;
   TCHAR *szName;
};

class cMapClass
{
public:
    cMapClass() {}
    cMapClass(struct NameValPair *nvp):m_EntryStruct(nvp) {}
    ~cMapClass() {}
    
    TCHAR * operator [](int index);

private:
    struct NameValPair *m_EntryStruct;
};

//******************************************************************************
// typedef's

typedef cMapClass MapClass;

//******************************************************************************
// External globals

// number of times to try to access the surface before giving up.
#define TRIES 100000

extern const TCHAR                     g_szClassName[];
extern DWORD                            g_dwWidth,
                                                 g_dwHeight;
extern HINSTANCE                      g_hInst;
extern HWND                              g_hwndMain;
extern HINSTANCE                      g_hInstDDraw;
extern LPDIRECTDRAW                g_pDD;
extern LPDIRECTDRAWSURFACE   g_pDDSPrimary,
                                                 g_pDDSOffScreen;
extern MapClass ErrorName;

#endif // header protection
