//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
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
                        { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CRgn overloaded constructor.")); }
        ~CRgn() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CRgn Destructor.")); }
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
