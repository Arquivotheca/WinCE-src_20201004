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
#include "bencheng.h"
#include <windows.h>
#include <stdio.h>
#ifdef UNDER_CE
#include <winddi.h>
#include <gpe.h>
#include <types.h>

#define DO_DISPPERF 1
#undef DISPPERF_DECLARE
#include "dispperf.h"
#endif

#ifndef DISPPERFDATA_H
#define DISPPERFDATA_H

class CDispPerfData
{
    public:
        CDispPerfData():m_bDispPerfAvailable(FALSE)
            { info(DETAIL, TEXT("In CDispPerfData constructor.")); }
        virtual ~CDispPerfData() { info(DETAIL, TEXT("In CDispPerfData destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL Cleanup();

    private:
        BOOL m_bDispPerfAvailable;
        HDC m_hdc;
        DWORD m_dwQuery;
};
#endif
