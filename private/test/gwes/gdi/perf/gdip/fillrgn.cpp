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
#include "fillrgn.h"

BOOL
CFillRgnTestSuite::Initialize(TestSuiteInfo * tsi)
{
    info(ECHO, TEXT("In CFillRgnTestSuite::Initialize"));
    BOOL bRval = TRUE;

#ifndef UNDER_CE
    m_dwOldBatchLimit = GdiSetBatchLimit(1);
    if(0 == m_dwOldBatchLimit)
        info(FAIL, TEXT("Failed to disable GDI call batching."));
#endif

    tsi->tsTestName = TEXT("FillRgn");

    if(bRval && (bRval = bRval && m_Dest.Initialize(tsi, TEXT("Dest"))))
        if(bRval = bRval && m_Brush.Initialize(tsi))
            if(bRval = bRval && m_Rgn.Initialize(tsi))
                bRval = bRval && m_DispPerfData.Initialize(tsi);

    return bRval;
}

BOOL
CFillRgnTestSuite::PreRun(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CFillRgnTestSuite::PreRun"));

    m_Dest.PreRun(tiRunInfo);
    m_hdcDest = m_Dest.GetSurface();

    m_Brush.PreRun(tiRunInfo, m_hdcDest);
    m_hBrush = m_Brush.GetBrush();

    m_Rgn.PreRun(tiRunInfo, m_hdcDest);
    m_hRgn = m_Rgn.GetRgn();

    m_DispPerfData.PreRun(tiRunInfo);

    return TRUE;
}

BOOL
CFillRgnTestSuite::Run()
{
    // logging here can cause timing issues.
    //info(DETAIL, TEXT("In CFillRgnTestSuite::Run"));
    return(FillRgn(m_hdcDest, m_hRgn, m_hBrush));
}

BOOL
CFillRgnTestSuite::AddPostRunData(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CFillRgnTestSuite::AddPostRunData"));

    return m_DispPerfData.AddPostRunData(tiRunInfo);
}

BOOL
CFillRgnTestSuite::PostRun()
{
    info(DETAIL, TEXT("In CFillRgnTestSuite::PostRun"));

    m_nIterationCount++;

    if(m_Dest.PostRun())
        if(m_Brush.PostRun(m_hdcDest))
            if(m_Rgn.PostRun())
                return FALSE;

    return TRUE;
}

BOOL
CFillRgnTestSuite::Cleanup()
{
    info(ECHO, TEXT("In CFillRgnTestSuite::Cleanup"));

    m_Dest.Cleanup();
    m_Brush.Cleanup();
    m_Rgn.Cleanup();
    m_DispPerfData.Cleanup();

#ifndef UNDER_CE
    if(m_dwOldBatchLimit > 0)
        GdiSetBatchLimit(m_dwOldBatchLimit);
#endif

    return TRUE;
}
