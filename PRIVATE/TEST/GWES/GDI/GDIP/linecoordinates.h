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
#include <math.h>

#ifndef LINECOORDINATES_H
#define LINECOORDINATES_H

struct LineCoordinates
{
    POINT nStart;
    POINT nEnd;
};
// 2 points, with 2 data values each, thus 4 data values.
#define LINECOORDINATEENTRYCOUNT 4

class CLineCoordinates
{
    public:
        CLineCoordinates(CSection * Section) : m_SectionList(Section), m_sCoordinates(NULL),
                                                 m_nCoordinatesIndex(0), m_nMaxCoordinatesIndex(0)
                        { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CLineCoordinates overloaded constructor.")); }
        ~CLineCoordinates() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CLineCoordinates Destructor.")); }
        CLineCoordinates() {}

        BOOL Initialize(TestSuiteInfo *);
        BOOL PreRun(TestInfo *);
        BOOL PostRun();
        BOOL Cleanup();
        BOOL GetCoordinates(POINT *Start, POINT *End);

    private:

        // attached brush list, with the name of the brush and the current index
        CSection *m_SectionList;
        LineCoordinates * m_sCoordinates;
        int m_nCoordinatesIndex, m_nMaxCoordinatesIndex;
};

#endif
