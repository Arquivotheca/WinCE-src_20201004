//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "ExtTextOut.h"

BOOL
CExtTextOutTestSuite::InitializeOptions(TestSuiteInfo * tsi)
{
    BOOL bRval = TRUE;

    // Fill in the test suite's fields that are varied with each run.
    tsi->tsFieldDescription.push_back(TEXT("Clipped"));
    tsi->tsFieldDescription.push_back(TEXT("Opaque"));

    bRval &= AllocDWArray(TEXT("Clipped"), 0, &m_dwClipped, &m_nMaxClippedIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("Opaque"), 0, &m_dwOpaque, &m_nMaxOpaqueIndex, m_SectionList, 10);

    m_nClippedIndex = 0;
    m_nOpaqueIndex = 0;

    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d opaques in use."), m_nMaxOpaqueIndex);
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d clips in use."), m_nMaxClippedIndex);

    return bRval;
}

BOOL
CExtTextOutTestSuite::Initialize(TestSuiteInfo * tsi)
{
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("In CExtTextOutTestSuite::Initialize"));
    BOOL bRval = TRUE;
    
#ifndef UNDER_CE
    m_dwOldBatchLimit = GdiSetBatchLimit(1);
    if(0 == m_dwOldBatchLimit)
        g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to disable GDI call batching."));
#endif

    tsi->tsTestName = TEXT("ExtTextOut");

    // initialize everything, if anything fails return failure.
    // cleanup is called whether or not there is a failure, which will deallocate 
    // anything that was allocated if there was a failure.

    // the order is string, clipped, opaque, dest, clip rect, position, region, font.
    if(bRval && (bRval = bRval && InitializeOptions(tsi)))
        if(bRval = bRval && m_String.Initialize(tsi))
            if(bRval = bRval && m_Dest.Initialize(tsi, TEXT("Dest")))
                if(bRval = bRval && m_Coordinates.Initialize(tsi, TEXT("ClipCoordinates")))
                    if(bRval = bRval && m_ptCoordinates.Initialize(tsi, TEXT("PositionCoordinates")))
                        if(bRval = bRval && m_Rgn.Initialize(tsi))
                            if(bRval = bRval && m_Font.Initialize(tsi))
                                bRval = bRval && m_DispPerfData.Initialize(tsi);

    return bRval;
}

BOOL
CExtTextOutTestSuite::PreRun(TestInfo *tiRunInfo)
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CExtTextOutTestSuite::PreRun"));

    tiRunInfo->Descriptions.push_back(itos(m_dwClipped[m_nClippedIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwOpaque[m_nOpaqueIndex]));

    // add clipped info
    m_fuOptionsInUse = 0;
    if(m_dwClipped[m_nClippedIndex])
        m_fuOptionsInUse |= ETO_CLIPPED;

    // add opaque info
    if(m_dwOpaque[m_nOpaqueIndex])
        m_fuOptionsInUse |= ETO_OPAQUE;

    m_String.PreRun(tiRunInfo);
    m_tcStringInUse = m_String.GetString();

    m_Dest.PreRun(tiRunInfo);
    m_hdcDest = m_Dest.GetSurface();

    m_Coordinates.PreRun(tiRunInfo);
    m_rcRectInUse = m_Coordinates.GetCoordinates();

    m_ptCoordinates.PreRun(tiRunInfo);
    m_ptPosInUse = m_ptCoordinates.GetCoordinate();

    m_Rgn.PreRun(tiRunInfo, m_hdcDest);

    m_Font.PreRun(tiRunInfo, m_hdcDest);

    m_nStringLengthInUse = _tcslen(m_tcStringInUse);

    m_DispPerfData.PreRun(tiRunInfo);

    return TRUE;
}

BOOL
CExtTextOutTestSuite::Run()
{
    // logging here can cause timing issues.
    //g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CExtTextOutTestSuite::Run"));
    return ExtTextOut(m_hdcDest, m_ptPosInUse.x, m_ptPosInUse.y, m_fuOptionsInUse, &m_rcRectInUse, m_tcStringInUse, m_nStringLengthInUse, NULL);
}

BOOL
CExtTextOutTestSuite::AddPostRunData(TestInfo *tiRunInfo)
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CExtTextOutTestSuite::AddPostRunData"));

    return m_DispPerfData.AddPostRunData(tiRunInfo);
}

BOOL
CExtTextOutTestSuite::PostRun()
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CExtTextOutTestSuite::PostRun"));

    m_nIterationCount++;

    // iterate to the next options
    m_nClippedIndex++;
    if(m_nClippedIndex >= m_nMaxClippedIndex)
    {
        m_nClippedIndex=0;
        m_nOpaqueIndex++;
        if(m_nOpaqueIndex >= m_nMaxOpaqueIndex)
        {
            m_nOpaqueIndex=0;
            // iterate to the next options
            if(m_String.PostRun())
                if(m_Dest.PostRun())
                    if(m_Coordinates.PostRun())
                        if(m_Rgn.PostRun())
                            if(m_Font.PostRun(m_hdcDest))
                                return FALSE;
        }
    }
    return TRUE;
}

BOOL
CExtTextOutTestSuite::Cleanup()
{
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("In CExtTextOutTestSuite::Cleanup"));

    delete[] m_dwClipped;
    delete[] m_dwOpaque;

    // clean up all of the test options
    m_String.Cleanup();
    m_Dest.Cleanup();
    m_Coordinates.Cleanup();
    m_Rgn.Cleanup();
    m_Font.Cleanup();
    m_DispPerfData.Cleanup();

#ifndef UNDER_CE
    if(m_dwOldBatchLimit > 0)
        GdiSetBatchLimit(m_dwOldBatchLimit);
#endif

    return TRUE;
}
