//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "handles.h"
#include "helpers.h"
#include "bkmode.h"

#ifndef PEN_H
#define PEN_H

class CPen
{
    public:
        CPen(CSection * Section) : m_SectionList(Section), m_BkMode(Section), m_hPen(NULL),
                                                      m_StockPen(NULL), m_tsPenNameArray(NULL), 
                                                      m_nPenIndex(0), m_nMaxPenIndex(0)
                        { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CPen overloaded constructor.")); }
        ~CPen() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CPen Destructor.")); }
        CPen() {}

        BOOL Initialize(TestSuiteInfo *);
        BOOL PreRun(TestInfo *, HDC hdc);
        BOOL PostRun(HDC hdc);
        BOOL Cleanup();

    private:
        // pens are affected by the BKMode
        class CBkMode m_BkMode;

        // attached brush list, with the name of the brush and the current index
        CSection *m_SectionList;
        HPEN *m_hPen;
        HPEN m_StockPen;
        TSTRING *m_tsPenNameArray;
        int m_nPenIndex, m_nMaxPenIndex;
};

#endif
