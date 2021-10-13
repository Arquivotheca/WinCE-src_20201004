//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "bencheng.h"
#include "otak.h"
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
            { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CDispPerfData constructor.")); }
        virtual ~CDispPerfData() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CDispPerfData destructor.")); }

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
