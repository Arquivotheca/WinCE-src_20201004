//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "handles.h"

#ifndef BRUSH_H
#define BRUSH_H

class CBrush
{
    public:
        CBrush(CSection * Section) : m_SectionList(Section), m_hBrush(NULL),
                                                      m_StockBrush(NULL), m_tsBrushNameArray(NULL), 
                                                      m_nBrushIndex(0), m_nMaxBrushIndex(0)
                        { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CBrush overloaded constructor.")); }
        ~CBrush() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CBrush Destructor.")); }
        CBrush() {}

        BOOL Initialize(TestSuiteInfo *);
        BOOL PreRun(TestInfo *, HDC hdc);
        BOOL PostRun(HDC hdc);
        BOOL Cleanup();
        HBRUSH GetBrush();

    private:

        // attached brush list, with the name of the brush and the current index
        CSection *m_SectionList;
        HBRUSH *m_hBrush;
        HBRUSH m_StockBrush;
        TSTRING *m_tsBrushNameArray;
        int m_nBrushIndex, m_nMaxBrushIndex;
};

#endif
