//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "handles.h"
#include "helpers.h"

#ifndef BKMODE_H
#define BKMODE_H

class CBkMode
{
    public:
        CBkMode(CSection * Section) : m_SectionList(Section),
                                                           m_dwBkModes(NULL), m_nBkModeIndex(0), m_nMaxBkModeIndex(0),
                                                           m_tsBkModeName(NULL)
                        { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CBkMode overloaded constructor.")); }
        ~CBkMode() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CBkMode Destructor.")); }
        CBkMode() {}

        BOOL Initialize(TestSuiteInfo *);
        BOOL PreRun(TestInfo *, HDC hdc);
        BOOL PostRun();
        BOOL Cleanup();

    private:

        // attached brush list, with the name of the brush and the current index
        CSection *m_SectionList;
        TSTRING *m_tsBkModeName;
        DWORD *m_dwBkModes;
        int m_nBkModeIndex, m_nMaxBkModeIndex;

};

#endif
