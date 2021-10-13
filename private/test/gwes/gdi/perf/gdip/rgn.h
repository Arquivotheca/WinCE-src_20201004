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
#include "bencheng.h"

#ifndef RGN_H
#define RGN_H

struct MYRGNDATA {
    RGNDATAHEADER rdh;
    RECT Buffer[1];
};

#define RGNRECTCOUNT 4

class CRgn
{
    public:
        CRgn(CSection * Section) : m_SectionList(Section), m_sRgnRects(NULL), m_hRgn(NULL),
                                                 m_nMaxRgnRects(0), m_dwMinRgnCount(0), m_dwMaxRgnCount(0),
                                                 m_dwRgnStep(0), m_nCurrentRgnRectCount(0), m_nRgnCombineMode(0),
                                                 m_bCombineRgn(FALSE)
                        { info(DETAIL, TEXT("In CRgn overloaded constructor.")); }
        ~CRgn() { info(DETAIL, TEXT("In CRgn Destructor.")); }
        CRgn() {}

        BOOL Initialize(TestSuiteInfo *);
        BOOL PreRun(TestInfo *, HDC hdc);
        BOOL PostRun();
        BOOL Cleanup();
        HRGN GetRgn();

    private:

        // attached brush list, with the name of the brush and the current index
        CSection *m_SectionList;

        MYRGNDATA * m_sRgnRects;
        int m_nRgnCombineMode;
        BOOL m_bCombineRgn;
        int m_nMaxRgnRects;
        DWORD m_dwMinRgnCount, m_dwMaxRgnCount, m_dwRgnStep;
        int m_nCurrentRgnRectCount;
        HRGN m_hRgn;
};

#endif
