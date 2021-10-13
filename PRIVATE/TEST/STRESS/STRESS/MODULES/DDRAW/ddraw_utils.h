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
#pragma once
#if !defined(__DDRAW_UTILS_H__)
#define __DDRAW_UTILS_H__

#include "ddraw.h"
#include <stressutils.h>

//******************************************************************************
// Function Prototypes

void GenRandRect(DWORD dwWidth, DWORD dwHeight, RECT *prc);
void GenRandRect(RECT *prcBound, RECT *prcRand);

BOOL FillSurfaceHorizontal(IDirectDrawSurface *piDDS, DWORD dwColor1, DWORD dwColor2, const RECT * prcWindow);
BOOL FillSurfaceVertical(IDirectDrawSurface *piDDS, DWORD dwColor1, DWORD dwColor2, const RECT * prcWindow);

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
#define TRIES 1000

extern const TCHAR                     g_szClassName[];

extern MapClass ErrorName;

#endif // header protection
