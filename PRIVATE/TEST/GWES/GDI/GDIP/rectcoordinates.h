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

#ifndef RECTCOORDINATES_H
#define RECTCOORDINATES_H

#define RECTCOORDINATEENTRYCOUNT 4

class CRectCoordinates
{
    public:
        CRectCoordinates(CSection * Section) : m_SectionList(Section), m_sCoordinates(NULL),
                                                 m_nCoordinatesIndex(0), m_nMaxCoordinatesIndex(0)
                        { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CRectCoordinates overloaded constructor.")); }
        ~CRectCoordinates() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CRectCoordinates Destructor.")); }
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
