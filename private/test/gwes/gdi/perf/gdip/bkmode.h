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

#ifndef BKMODE_H
#define BKMODE_H

class CBkMode
{
    public:
        CBkMode(CSection * Section) : m_SectionList(Section),
                                                           m_dwBkModes(NULL), m_nBkModeIndex(0), m_nMaxBkModeIndex(0),
                                                           m_tsBkModeName(NULL)
                        { info(DETAIL, TEXT("In CBkMode overloaded constructor.")); }
        ~CBkMode() { info(DETAIL, TEXT("In CBkMode Destructor.")); }
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
