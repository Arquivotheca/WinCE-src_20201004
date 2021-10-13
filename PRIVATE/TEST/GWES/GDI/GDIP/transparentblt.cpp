//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "transparentblt.h"

BOOL
CTransparentBltTestSuite::Initialize(TestSuiteInfo * tsi)
{
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("In CTransparentBltTestSuite::Initialize"));
    BOOL bRval = TRUE;
    
#ifndef UNDER_CE
    m_dwOldBatchLimit = GdiSetBatchLimit(1);
    if(0 == m_dwOldBatchLimit)
        g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to disable GDI call batching."));
#endif

    tsi->tsTestName = TEXT("TransparentBlt");

    // the transparent color used should be irrelevent to timing.
    m_crTransparentColor = RGB(0x00, 0x00, 0x00);

    // initialize everything, if anything fails return failure.
    // cleanup is called whether or not there is a failure, which will deallocate 
    // anything that was allocated if there was a failure.
    if(bRval && (bRval = bRval && m_StretchCoordinates.Initialize(tsi)))
        if(bRval = bRval &&m_Source.Initialize(tsi, TEXT("Source"), TEXT("DIB32_RGB8888")))
            if(bRval = bRval && m_Dest.Initialize(tsi, TEXT("Dest")))
                if(bRval = bRval && m_Rgn.Initialize(tsi))
                    bRval = bRval && m_DispPerfData.Initialize(tsi);

    return bRval;
}

BOOL
CTransparentBltTestSuite::PreRun(TestInfo *tiRunInfo)
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CTransparentBltTestSuite::PreRun"));

    m_StretchCoordinates.PreRun(tiRunInfo);
    m_sCoordinateInUse = m_StretchCoordinates.GetCoordinates();

    m_Source.PreRun(tiRunInfo);
    m_hdcSource = m_Source.GetSurface();

    m_Dest.PreRun(tiRunInfo);
    m_hdcDest = m_Dest.GetSurface();

    m_Rgn.PreRun(tiRunInfo, m_hdcDest);

    m_DispPerfData.PreRun(tiRunInfo);

    return TRUE;
}

BOOL
CTransparentBltTestSuite::Run()
{
    // logging here can cause timing issues.
    //g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CTransparentBltTestSuite::Run"));
    return(TransparentBlt(m_hdcDest,
                        m_sCoordinateInUse.nDestLeft,
                        m_sCoordinateInUse.nDestTop,
                        m_sCoordinateInUse.nDestWidth,
                        m_sCoordinateInUse.nDestHeight,
                        m_hdcSource,
                        m_sCoordinateInUse.nSrcLeft,
                        m_sCoordinateInUse.nSrcTop,
                        m_sCoordinateInUse.nSrcWidth,
                        m_sCoordinateInUse.nSrcHeight,
                        m_crTransparentColor));
}

BOOL
CTransparentBltTestSuite::AddPostRunData(TestInfo *tiRunInfo)
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CTransparentBltTestSuite::AddPostRunData"));

    return m_DispPerfData.AddPostRunData(tiRunInfo);
}

BOOL
CTransparentBltTestSuite::PostRun()
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CTransparentBltTestSuite::PostRun"));

    m_nIterationCount++;

    // iterate to the next options
    if(m_StretchCoordinates.PostRun())
        if(m_Source.PostRun())
            if(m_Dest.PostRun())
                if(m_Rgn.PostRun())
                    return FALSE;

    return TRUE;
}

BOOL
CTransparentBltTestSuite::Cleanup()
{
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("In CTransparentBltTestSuite::Cleanup"));

    // clean up all of the test options
    m_StretchCoordinates.Cleanup();
    m_Source.Cleanup();
    m_Dest.Cleanup();
    m_Rgn.Cleanup();
    m_DispPerfData.Cleanup();

#ifndef UNDER_CE
    if(m_dwOldBatchLimit > 0)
        GdiSetBatchLimit(m_dwOldBatchLimit);
#endif

    return TRUE;
}
