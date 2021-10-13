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
                        { info(DETAIL, TEXT("In CLineCoordinates overloaded constructor.")); }
        ~CLineCoordinates() { info(DETAIL, TEXT("In CLineCoordinates Destructor.")); }
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
