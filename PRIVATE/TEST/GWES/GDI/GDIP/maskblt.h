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
#include "rop4.h"
#include "surface.h"
#include "rgn.h"
#include "dispperfdata.h"

#ifndef MASKBLT_H
#define MASKBLT_H

struct MaskBltCoordinates
{
    int nDestTop;
    int nDestLeft;
    int nDestWidth;
    int nDestHeight;
    int nSrcTop;
    int nSrcLeft;
    int nMaskTop;
    int nMaskLeft;
};

#define MASKBLTCOORDINATECOUNT 8
class CMaskBltTestSuite : public CTestSuite
{
    public:
        CMaskBltTestSuite(CSection * Section) : CTestSuite(Section), m_Brush(Section), m_Dest(Section), m_Source(Section), m_Rops(Section),
                                            m_Rgn(Section), m_DispPerfData(), m_hdcDest(NULL), m_hdcSource(NULL), m_dwRop(0),
                                            m_sCoordinates(NULL), m_nCoordinatesIndex(0), m_nMaxCoordinatesIndex(0)
                                            { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CMaskBltTestSuite overloaded constructor.")); }
         ~CMaskBltTestSuite() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CMaskBltTestSuite destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();

private:
        CMaskBltTestSuite() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CMaskBltTestSuite base constructor.")); }

        int m_nIterationCount;
        class CBrush m_Brush;
        class CSurface m_Dest;
        class CSurface m_Source;
        class CRop4 m_Rops;
        class CRgn m_Rgn;
        class CDispPerfData m_DispPerfData;

        // The Mask Bitmap to use
        HBITMAP m_hbmpMask;
        HDC m_hdcDest;
        HDC m_hdcSource;
        DWORD m_dwRop;

        // rectangle sizes, current index into the rectangle size array,
        // and the current coordinates in use.
        MaskBltCoordinates * m_sCoordinates;
        int m_nCoordinatesIndex, m_nMaxCoordinatesIndex;

#ifndef UNDER_CE
        DWORD m_dwOldBatchLimit;
#endif

        BOOL InitializeCoordinates(TestSuiteInfo * tsi);
        BOOL InitializeMask();
};
#endif
