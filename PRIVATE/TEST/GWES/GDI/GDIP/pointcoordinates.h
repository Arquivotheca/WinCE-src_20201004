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

#ifndef POINTCOORDINATES_H
#define POINTCOORDINATES_H

#define POINTCOORDINATEENTRYCOUNT 2

class CPointCoordinates
{
    public:
        CPointCoordinates(CSection * Section) : m_SectionList(Section), m_sCoordinates(NULL),
                                                 m_nCoordinatesIndex(0), m_nMaxCoordinatesIndex(0)
                        { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CPointCoordinates overloaded constructor.")); }
        ~CPointCoordinates() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CPointCoordinates Destructor.")); }
        CPointCoordinates() {}

        BOOL Initialize(TestSuiteInfo *, TSTRING ts);
        BOOL PreRun(TestInfo *);
        BOOL PostRun();
        BOOL Cleanup();
        POINT GetCoordinate();

    private:
        // attached brush list, with the name of the brush and the current index
        CSection *m_SectionList;
        POINT * m_sCoordinates;
        int m_nCoordinatesIndex, m_nMaxCoordinatesIndex;
};

#endif
