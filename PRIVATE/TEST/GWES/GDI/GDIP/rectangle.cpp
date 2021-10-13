//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "Rectangle.h"

BOOL
CRectangleTestSuite::Initialize(TestSuiteInfo * tsi)
{
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("In CRectangleTestSuite::Initialize"));
    BOOL bRval = TRUE;
    
#ifndef UNDER_CE
    m_dwOldBatchLimit = GdiSetBatchLimit(1);
    if(0 == m_dwOldBatchLimit)
        g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to disable GDI call batching."));
#endif

    tsi->tsTestName = TEXT("Rectangle");

    // initialize everything, if anything fails return failure.
    // cleanup is called whether or not there is a failure, which will deallocate 
    // anything that was allocated if there was a failure.
    if(bRval && (bRval = bRval && m_Dest.Initialize(tsi, TEXT("Dest"))))
        if(bRval = bRval && m_Coordinates.Initialize(tsi, TEXT("Coordinates")))
            if(bRval = bRval && m_Rop2.Initialize(tsi))
                if(bRval = bRval && m_Pen.Initialize(tsi))
                    if(bRval = bRval && m_Brush.Initialize(tsi))
                        if(bRval = bRval && m_Rgn.Initialize(tsi))
                            bRval = bRval && m_DispPerfData.Initialize(tsi);

    return bRval;
}

BOOL
CRectangleTestSuite::PreRun(TestInfo *tiRunInfo)
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CRectangleTestSuite::PreRun"));

    m_Dest.PreRun(tiRunInfo);
    m_hdcDest = m_Dest.GetSurface();

    m_Coordinates.PreRun(tiRunInfo);
    m_sCoordinateInUse = m_Coordinates.GetCoordinates();

    m_Rop2.PreRun(tiRunInfo, m_hdcDest);

    m_Pen.PreRun(tiRunInfo, m_hdcDest);

    m_Brush.PreRun(tiRunInfo, m_hdcDest);

    m_Rgn.PreRun(tiRunInfo, m_hdcDest);

    m_DispPerfData.PreRun(tiRunInfo);

    return TRUE;
}

BOOL
CRectangleTestSuite::Run()
{
    // logging here can cause timing issues.
    //g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CRectangleTestSuite::Run"));
    return(Rectangle(m_hdcDest,
                        m_sCoordinateInUse.left,
                        m_sCoordinateInUse.top,
                        m_sCoordinateInUse.right,
                        m_sCoordinateInUse.bottom));
}

BOOL
CRectangleTestSuite::AddPostRunData(TestInfo *tiRunInfo)
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CRectangleTestSuite::AddPostRunData"));

    return m_DispPerfData.AddPostRunData(tiRunInfo);
}

BOOL
CRectangleTestSuite::PostRun()
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CRectangleTestSuite::PostRun"));

    m_nIterationCount++;

    // iterate to the next options
    if(m_Dest.PostRun())
        if(m_Coordinates.PostRun())
            if(m_Rop2.PostRun(m_hdcDest))
                if(m_Pen.PostRun(m_hdcDest))
                    if(m_Brush.PostRun(m_hdcDest))
                        if(m_Rgn.PostRun())
                            return FALSE;

    return TRUE;
}

BOOL
CRectangleTestSuite::Cleanup()
{
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("In CRectangleTestSuite::Cleanup"));

    // clean up all of the test options
    m_Dest.Cleanup();
    m_Coordinates.Cleanup();
    m_Rop2.Cleanup();
    m_Pen.Cleanup();
    m_Brush.Cleanup();
    m_Rgn.Cleanup();
    m_DispPerfData.Cleanup();

#ifndef UNDER_CE
    if(m_dwOldBatchLimit > 0)
        GdiSetBatchLimit(m_dwOldBatchLimit);
#endif

    return TRUE;
}
