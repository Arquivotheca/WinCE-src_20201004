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

#ifndef POLYCOORDINATES_H
#define POLYCOORDINATES_H

#define POLYCOORDINATEENTRYCOUNT 2

class CPolyCoordinates
{
    public:
        CPolyCoordinates(CSection * Section) : m_SectionList(Section), m_sCoordinates(NULL), m_nCoordinatesCount(NULL),
                                                 m_nCoordinatesIndex(0), m_nMaxCoordinatesIndex(0)
                        { info(DETAIL, TEXT("In CPolyCoordinates overloaded constructor.")); }
        ~CPolyCoordinates() { info(DETAIL, TEXT("In CPolyCoordinates Destructor.")); }
        CPolyCoordinates() {}

        BOOL Initialize(TestSuiteInfo *);
        BOOL PreRun(TestInfo *);
        BOOL PostRun();
        BOOL Cleanup();
        POINT * GetCoordinates();
        int GetCoordinatesCount();

    private:
        // attached brush list, with the name of the brush and the current index
        CSection *m_SectionList;
        POINT ** m_sCoordinates;
        int * m_nCoordinatesCount;
        int m_nCoordinatesIndex, m_nMaxCoordinatesIndex;
};

#endif
