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
#include "helpers.h"

#ifndef STRING_H
#define STRING_H

class CString
{
    public:
        CString(CSection * Section) : m_SectionList(Section),
                                                           m_tsString(NULL), m_nStringIndex(0), m_nMaxStringIndex(0)
                        { info(DETAIL, TEXT("In CString overloaded constructor.")); }
        ~CString() { info(DETAIL, TEXT("In CString Destructor.")); }
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
