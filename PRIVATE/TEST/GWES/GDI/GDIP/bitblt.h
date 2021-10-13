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

#ifndef BITBLT_H
#define BITBLT_H

struct BitBltCoordinates
{
    int nDestTop;
    int nDestLeft;
    int nDestWidth;
    int nDestHeight;
    int nSrcTop;
    int nSrcLeft;
};
#define BITBLTCOORDINATEENTRYCOUNT 6

class CBitBltTestSuite : public CTestSuite
{
    public:
        CBitBltTestSuite(CSection * Section) : CTestSuite(Section), 
                                            m_Brush(Section), m_Dest(Section), m_Source(Section), m_Rops(Section), m_Rgn(Section), m_DispPerfData(),
                                            m_hdcDest(NULL), m_hdcSource(NULL), m_dwRop(0),
                                            m_sCoordinates(NULL), m_nCoordinatesIndex(0), m_nMaxCoordinatesIndex(0)
                                            { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CBitBltTestSuite overloaded constructor.")); }
         ~CBitBltTestSuite() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CBitBltTestSuite destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();

private:
        CBitBltTestSuite() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CBitBltTestSuite base constructor.")); }

        int m_nIterationCount;
        class CBrush m_Brush;
        class CSurface m_Dest;
        class CSurface m_Source;
        class CRop3 m_Rops;
        class CRgn m_Rgn;
        class CDispPerfData m_DispPerfData;

        HDC m_hdcDest;
        HDC m_hdcSource;
        DWORD m_dwRop;

        // rectangle sizes, current index into the rectangle size array,
        // and the current coordinates in use.
        BitBltCoordinates * m_sCoordinates;
        int m_nCoordinatesIndex, m_nMaxCoordinatesIndex;

#ifndef UNDER_CE
        DWORD m_dwOldBatchLimit;
#endif

        BOOL InitializeCoordinates(TestSuiteInfo * tsi);
};

#endif
