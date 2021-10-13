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
#include "stretchcoordinates.h"
#include "rgn.h"
#include "dispperfdata.h"

#ifndef STRETCHBLT_H
#define STRETCHBLT_H
class CStretchBltTestSuite : public CTestSuite
{
    public:
        CStretchBltTestSuite(CSection * Section) : CTestSuite(Section), 
                                            m_Brush(Section), m_Dest(Section), m_Source(Section), m_Rops(Section), 
                                            m_StretchCoordinates(Section), m_Rgn(Section), m_DispPerfData(),
                                            m_hdcDest(NULL), m_hdcSource(NULL), m_dwRop(0)
                                            { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CStretchBltTestSuite overloaded constructor.")); }
         ~CStretchBltTestSuite() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CStretchBltTestSuite destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();

private:
        CStretchBltTestSuite() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CStretchBltTestSuite base constructor.")); }

        int m_nIterationCount;
        class CBrush m_Brush;
        class CSurface m_Dest;
        class CSurface m_Source;
        class CRop3 m_Rops;
        class CStretchCoordinates m_StretchCoordinates;
        class CRgn m_Rgn;
        class CDispPerfData m_DispPerfData;

        HDC m_hdcDest;
        HDC m_hdcSource;
        DWORD m_dwRop;

        // rectangle sizes, current index into the rectangle size array,
        // and the current coordinates in use.
        StretchCoordinates m_sCoordinateInUse;

        int * m_nStretchBltModes;
        TSTRING *m_tsStretchBltModeNameArray;
        int m_nStretchBltModeIndex, m_nMaxStretchBltModeIndex;
        int m_nOldStretchBltMode;

#ifndef UNDER_CE
        DWORD m_dwOldBatchLimit;
#endif

        BOOL InitializeStretchBltModeIndex(TestSuiteInfo *);
};
#endif
