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
#include "helpers.h"

#ifndef ROP2_H
#define ROP2_H

class CRop2
{
    public:
        CRop2(CSection * Section) : m_SectionList(Section), m_dwRop(NULL),
                                                      m_nRopIndex(0), m_nMaxRopIndex(0)
                        { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CRop2 overloaded constructor.")); }
        ~CRop2() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CRop2 Destructor.")); }
        CRop2() {}

        BOOL Initialize(TestSuiteInfo *);
        BOOL PreRun(TestInfo *, HDC);
        BOOL PostRun(HDC);
        BOOL Cleanup();
        DWORD GetRop();

    private:
        CSection *m_SectionList;

        // ROP2's and the current index
        DWORD *m_dwRop;
        DWORD m_dwOldRop2;
        int m_nRopIndex, m_nMaxRopIndex;
};

#endif
