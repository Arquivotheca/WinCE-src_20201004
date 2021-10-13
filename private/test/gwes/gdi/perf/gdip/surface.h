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
#include "tuxmain.h"
#include "handles.h"
#include "testsuite.h"
#include "bencheng.h"

#ifndef SURFACE_H
#define SURFACE_H

class CSurface
{
    public:
        CSurface(CSection * Section) : m_SectionList(Section), m_DC(NULL), m_tsDCNameArray(NULL),
                          m_nBitDepth(NULL), m_nIndex(0), m_nMaxIndex(0), m_hWnd(0)
                        { info(DETAIL, TEXT("In CSurface overloaded constructor.")); }
        CSurface() {}
        ~CSurface() { info(DETAIL, TEXT("In CSurface Destructor.")); }

        BOOL Initialize(TestSuiteInfo *tsi, TSTRING ts, TSTRING tsDefault = TEXT("Primary"));
        BOOL PreRun(TestInfo *);
        BOOL PostRun();
        BOOL Cleanup();
        HDC GetSurface();

    private:
        // attached brush list, with the name of the brush and the current index
        // dest dc list with the name of the dc and the current index
        CSection *m_SectionList;
        HDC *m_DC;
        HWND *m_hWnd;
        TSTRING *m_tsDCNameArray;
        TSTRING m_tsDeviceName;
        UINT *m_nBitDepth;
        int m_nIndex, m_nMaxIndex;
};

#endif

