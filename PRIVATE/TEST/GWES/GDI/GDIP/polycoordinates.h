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

#ifndef POLYCOORDINATES_H
#define POLYCOORDINATES_H

#define POLYCOORDINATEENTRYCOUNT 2

class CPolyCoordinates
{
    public:
        CPolyCoordinates(CSection * Section) : m_SectionList(Section), m_sCoordinates(NULL), m_nCoordinatesCount(NULL),
                                                 m_nCoordinatesIndex(0), m_nMaxCoordinatesIndex(0)
                        { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CPolyCoordinates overloaded constructor.")); }
        ~CPolyCoordinates() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CPolyCoordinates Destructor.")); }
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
