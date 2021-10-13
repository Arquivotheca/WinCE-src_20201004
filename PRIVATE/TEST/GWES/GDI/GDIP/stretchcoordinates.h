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

#ifndef STRETCHCOORDINATES_H
#define STRETCHCOORDINATES_H

struct StretchCoordinates
{
    int nDestTop;
    int nDestLeft;
    int nDestWidth;
    int nDestHeight;
    int nSrcTop;
    int nSrcLeft;
    int nSrcWidth;
    int nSrcHeight;
};
#define STRETCHCOORDINATEENTRYCOUNT 8


class CStretchCoordinates
{
    public:
        CStretchCoordinates(CSection * Section) : m_SectionList(Section), m_sCoordinates(NULL),
                                                 m_nCoordinatesIndex(0), m_nMaxCoordinatesIndex(0)
                        { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CStretchCoordinates overloaded constructor.")); }
        ~CStretchCoordinates() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CStretchCoordinates Destructor.")); }
        CStretchCoordinates() {}

        BOOL Initialize(TestSuiteInfo *);
        BOOL PreRun(TestInfo *);
        BOOL PostRun();
        BOOL Cleanup();
        struct StretchCoordinates GetCoordinates();

    private:

        // attached brush list, with the name of the brush and the current index
        CSection *m_SectionList;
        struct StretchCoordinates * m_sCoordinates;
        int m_nCoordinatesIndex, m_nMaxCoordinatesIndex;
};

#endif
