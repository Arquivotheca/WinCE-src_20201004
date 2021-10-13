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

#ifndef ROP4_H
#define ROP4_H

class CRop4
{
    public:
        CRop4(CSection * Section) : m_SectionList(Section), m_dwRop(NULL),
                                                      m_nRopIndex(0), m_nMaxRopIndex(0)
                        { info(DETAIL, TEXT("In CRop4 overloaded constructor.")); }
        ~CRop4() { info(DETAIL, TEXT("In CRop4 Destructor.")); }
        CRop4() {}

        BOOL Initialize(TestSuiteInfo *);
        BOOL PreRun(TestInfo *);
        BOOL PostRun();
        BOOL Cleanup();
        DWORD GetRop();

    private:
        CSection *m_SectionList;

        // ROP4's and the current index
        DWORD *m_dwRop;
        int m_nRopIndex, m_nMaxRopIndex;
};

#endif
