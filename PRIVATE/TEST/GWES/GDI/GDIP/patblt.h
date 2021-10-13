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
#include "rop3.h"
#include "surface.h"
#include "rgn.h"
#include "dispperfdata.h"

#ifndef PATBLT_H
#define PATBLT_H

struct PatBltCoordinates
{
    int nDestTop;
    int nDestLeft;
    int nDestWidth;
    int nDestHeight;
};
#define PATBLTCOORDINATEENTRYCOUNT 4

class CPatBltTestSuite : public CTestSuite
{
    public:
        CPatBltTestSuite(CSection * Section) : CTestSuite(Section),
                                            m_Brush(Section), m_Dest(Section), m_Rops(Section), m_Rgn(Section),
                                            m_DispPerfData(), m_hdcDest(NULL), m_dwRop(0),
                                            m_sCoordinates(NULL), m_nCoordinatesIndex(0), m_nMaxCoordinatesIndex(0)
                                            { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CPatBltTestSuite overloaded constructor.")); }
         ~CPatBltTestSuite() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CPatBltTestSuite destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();

private:
        CPatBltTestSuite() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CPatBltTestSuite base constructor.")); }

        int m_nIterationCount;
        class CBrush m_Brush;
        class CSurface m_Dest;
        class CRop3 m_Rops;
        class CRgn m_Rgn;
        class CDispPerfData m_DispPerfData;

        HDC m_hdcDest;
        DWORD m_dwRop;

        // rectangle sizes, current index into the rectangle size array,
        // and the current coordinates in use.
        PatBltCoordinates * m_sCoordinates;
        int m_nCoordinatesIndex, m_nMaxCoordinatesIndex;

#ifndef UNDER_CE
        DWORD m_dwOldBatchLimit;
#endif

        BOOL InitializeCoordinates(TestSuiteInfo *);
};
#endif
