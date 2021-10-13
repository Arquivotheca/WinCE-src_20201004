//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "bencheng.h"
#include "otak.h"
#include "brush.h"
#include "surface.h"
#include "rgn.h"
#include "dispperfdata.h"

#ifndef FILLRGN_H
#define FILLRGN_H

class CFillRgnTestSuite : public CTestSuite
{
    public:
        CFillRgnTestSuite(CSection * Section) : CTestSuite(Section),
                                            m_Brush(Section), m_Dest(Section), m_Rgn(Section), m_DispPerfData(),
                                            m_hdcDest(NULL), m_hRgn(NULL), m_hBrush(NULL)
                                            { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CFillRgnTestSuite overloaded constructor.")); }
         ~CFillRgnTestSuite() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CFillRgnTestSuite destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();

private:
        CFillRgnTestSuite() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CFillRgnTestSuite base constructor.")); }

        int m_nIterationCount;
        class CBrush m_Brush;
        class CSurface m_Dest;
        class CRgn m_Rgn;
        class CDispPerfData m_DispPerfData;

        HDC m_hdcDest;
        HRGN m_hRgn;
        HBRUSH m_hBrush;

#ifndef UNDER_CE
        DWORD m_dwOldBatchLimit;
#endif
};
#endif
