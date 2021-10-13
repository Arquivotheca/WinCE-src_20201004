//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include "tuxmain.h"
#include "bencheng.h"
#include "surface.h"
#include "rop2.h"
#include "pen.h"
#include "brush.h"
#include "polycoordinates.h"
#include "rgn.h"
#include "dispperfdata.h"

#ifndef POLYLINE_H
#define POLYLINE_H
class CPolylineTestSuite : public CTestSuite
{
    public:
        CPolylineTestSuite(CSection * Section) : CTestSuite(Section), 
                                            m_Dest(Section), m_hdcDest(NULL), m_Coordinates(Section), 
                                            m_Rop2(Section), m_Pen(Section), m_Rgn(Section), m_DispPerfData()
                                            { info(DETAIL, TEXT("In CPolylineTestSuite overloaded constructor.")); }
         ~CPolylineTestSuite() { info(DETAIL, TEXT("In CPolylineTestSuite destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();

private:
        CPolylineTestSuite() { info(DETAIL, TEXT("In CPolylineTestSuite base constructor.")); }

        int m_nIterationCount;
        class CSurface m_Dest;
        class CPolyCoordinates m_Coordinates;
        class CRop2 m_Rop2;
        class CPen m_Pen;
        class CRgn m_Rgn;
        class CDispPerfData m_DispPerfData;

        HDC m_hdcDest;

        POINT *m_sPointsInUse;
        int m_nCoordinateCountInUse;

#ifndef UNDER_CE
        DWORD m_dwOldBatchLimit;
#endif

};
#endif
