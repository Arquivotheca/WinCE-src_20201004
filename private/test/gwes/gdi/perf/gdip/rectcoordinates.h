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

#ifndef RECTCOORDINATES_H
#define RECTCOORDINATES_H

#define RECTCOORDINATEENTRYCOUNT 4

class CRectCoordinates
{
    public:
        CRectCoordinates(CSection * Section) : m_SectionList(Section), m_sCoordinates(NULL),
                                                 m_nCoordinatesIndex(0), m_nMaxCoordinatesIndex(0)
                        { info(DETAIL, TEXT("In CRectCoordinates overloaded constructor.")); }
        ~CRectCoordinates() { info(DETAIL, TEXT("In CRectCoordinates Destructor.")); }
        CRectCoordinates() {}

        BOOL Initialize(TestSuiteInfo *, TSTRING ts);
        BOOL PreRun(TestInfo *);
        BOOL PostRun();
        BOOL Cleanup();
        RECT GetCoordinates();

    private:

        // attached brush list, with the name of the brush and the current index
        CSection *m_SectionList;
        RECT * m_sCoordinates;
        int m_nCoordinatesIndex, m_nMaxCoordinatesIndex;
};

#endif
