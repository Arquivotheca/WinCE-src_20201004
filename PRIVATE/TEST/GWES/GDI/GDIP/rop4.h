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

#ifndef ROP4_H
#define ROP4_H

class CRop4
{
    public:
        CRop4(CSection * Section) : m_SectionList(Section), m_dwRop(NULL),
                                                      m_nRopIndex(0), m_nMaxRopIndex(0)
                        { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CRop4 overloaded constructor.")); }
        ~CRop4() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CRop4 Destructor.")); }
        CRop4() {}

        BOOL Initialize(TestSuiteInfo *);
        BOOL PreRun(TestInfo *);
        BOOL PostRun();
        BOOL Cleanup();
        DWORD GetRop();

    private:
        CSection *m_SectionList;

        // ROP4's and the current index
        DWORD *m_dwRop;
        int m_nRopIndex, m_nMaxRopIndex;
};

#endif
