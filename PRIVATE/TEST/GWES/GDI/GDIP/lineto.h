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
#include "surface.h"
#include "rop2.h"
#include "pen.h"
#include "brush.h"
#include "linecoordinates.h"
#include "polycoordinates.h"
#include "rgn.h"
#include "dispperfdata.h"

#ifndef LINETO_H
#define LINETO_H
class CLineToTestSuite : public CTestSuite
{
    public:
        CLineToTestSuite(CSection * Section) : CTestSuite(Section), 
                                            m_Dest(Section), m_hdcDest(NULL), m_Coordinates(Section), m_PolyCoordinates(Section),
                                            m_Rop2(Section), m_Pen(Section), m_Rgn(Section), m_DispPerfData(),
                                            m_bUsePolyCoordinates(FALSE), m_sPointsInUse(NULL), m_nCoordinateCountInUse(0)
                                            { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CLineToTestSuite overloaded constructor.")); }
         ~CLineToTestSuite() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CLineToTestSuite destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();

private:
        CLineToTestSuite() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CLineToTestSuite base constructor.")); }

        int m_nIterationCount;
        class CSurface m_Dest;
        class CLineCoordinates m_Coordinates;
        class CPolyCoordinates m_PolyCoordinates;
        class CRop2 m_Rop2;
        class CPen m_Pen;
        class CRgn m_Rgn;
        class CDispPerfData m_DispPerfData;

        HDC m_hdcDest;

        POINT m_ptStartInUse;
        POINT m_ptEndInUse;

        BOOL m_bUsePolyCoordinates;
        POINT *m_sPointsInUse;
        int m_nCoordinateCountInUse;

#ifndef UNDER_CE
        DWORD m_dwOldBatchLimit;
#endif

};
#endif
