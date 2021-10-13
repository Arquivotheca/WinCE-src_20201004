//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "fillrgn.h"

BOOL
CFillRgnTestSuite::Initialize(TestSuiteInfo * tsi)
{
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("In CFillRgnTestSuite::Initialize"));
    BOOL bRval = TRUE;

#ifndef UNDER_CE
    m_dwOldBatchLimit = GdiSetBatchLimit(1);
    if(0 == m_dwOldBatchLimit)
        g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to disable GDI call batching."));
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
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CFillRgnTestSuite::PreRun"));

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
    //g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CFillRgnTestSuite::Run"));
    return(FillRgn(m_hdcDest, m_hRgn, m_hBrush));
}

BOOL
CFillRgnTestSuite::AddPostRunData(TestInfo *tiRunInfo)
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CFillRgnTestSuite::AddPostRunData"));

    return m_DispPerfData.AddPostRunData(tiRunInfo);
}

BOOL
CFillRgnTestSuite::PostRun()
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CFillRgnTestSuite::PostRun"));

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
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("In CFillRgnTestSuite::Cleanup"));

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
