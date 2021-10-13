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

#ifndef STRING_H
#define STRING_H

class CString
{
    public:
        CString(CSection * Section) : m_SectionList(Section),
                                                           m_tsString(NULL), m_nStringIndex(0), m_nMaxStringIndex(0)
                        { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CString overloaded constructor.")); }
        ~CString() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CString Destructor.")); }
        CString() {}

        BOOL Initialize(TestSuiteInfo *);
        BOOL PreRun(TestInfo *);
        BOOL PostRun();
        BOOL Cleanup();
        TCHAR * GetString();

    private:

        // attached brush list, with the name of the brush and the current index
        CSection *m_SectionList;
        TSTRING *m_tsString;
        int m_nStringIndex, m_nMaxStringIndex;
};

#endif
