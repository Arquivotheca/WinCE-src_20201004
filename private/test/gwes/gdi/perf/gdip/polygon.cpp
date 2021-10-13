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
#include "Polygon.h"

BOOL
CPolygonTestSuite::Initialize(TestSuiteInfo * tsi)
{
    info(ECHO, TEXT("In CPolygonTestSuite::Initialize"));
    BOOL bRval = TRUE;
    
#ifndef UNDER_CE
    m_dwOldBatchLimit = GdiSetBatchLimit(1);
    if(0 == m_dwOldBatchLimit)
        info(FAIL, TEXT("Failed to disable GDI call batching."));
#endif

    tsi->tsTestName = TEXT("Polygon");

    // initialize everything, if anything fails return failure.
    // cleanup is called whether or not there is a failure, which will deallocate 
    // anything that was allocated if there was a failure.
    if(bRval && (bRval = bRval && m_Dest.Initialize(tsi, TEXT("Dest"))))
        if(bRval = bRval && m_Coordinates.Initialize(tsi))
            if(bRval = bRval && m_Rop2.Initialize(tsi))
                if(bRval = bRval && m_Brush.Initialize(tsi))
                    if(bRval = bRval && m_Pen.Initialize(tsi))
                        if(bRval = bRval && m_Rgn.Initialize(tsi))
                            bRval = bRval && m_DispPerfData.Initialize(tsi);

    return bRval;
}

BOOL
CPolygonTestSuite::PreRun(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CPolygonTestSuite::PreRun"));

    m_Dest.PreRun(tiRunInfo);
    m_hdcDest = m_Dest.GetSurface();

    m_Coordinates.PreRun(tiRunInfo);
    m_sPointsInUse = m_Coordinates.GetCoordinates();
    m_nCoordinateCountInUse = m_Coordinates.GetCoordinatesCount();

    m_Rop2.PreRun(tiRunInfo, m_hdcDest);

    m_Brush.PreRun(tiRunInfo, m_hdcDest);

    m_Pen.PreRun(tiRunInfo, m_hdcDest);

    m_Rgn.PreRun(tiRunInfo, m_hdcDest);

    m_DispPerfData.PreRun(tiRunInfo);

    return TRUE;
}

BOOL
CPolygonTestSuite::Run()
{
    BOOL bRval = TRUE;
    // logging here can cause timing issues.
    //info(DETAIL, TEXT("In CPolygonTestSuite::Run"));
    return Polygon(m_hdcDest, m_sPointsInUse, m_nCoordinateCountInUse);
}

BOOL
CPolygonTestSuite::AddPostRunData(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CPolygonTestSuite::AddPostRunData"));

    return m_DispPerfData.AddPostRunData(tiRunInfo);
}

BOOL
CPolygonTestSuite::PostRun()
{
    info(DETAIL, TEXT("In CPolygonTestSuite::PostRun"));

    m_nIterationCount++;

    // iterate to the next options
    if(m_Dest.PostRun())
        if(m_Coordinates.PostRun())
            if(m_Rop2.PostRun(m_hdcDest))
                if(m_Brush.PostRun(m_hdcDest))
                    if(m_Pen.PostRun(m_hdcDest))
                        if(m_Rgn.PostRun())
                            return FALSE;

    return TRUE;
}

BOOL
CPolygonTestSuite::Cleanup()
{
    info(ECHO, TEXT("In CPolygonTestSuite::Cleanup"));

    // clean up all of the test options
    m_Dest.Cleanup();
    m_Coordinates.Cleanup();
    m_Rop2.Cleanup();
    m_Brush.Cleanup();
    m_Pen.Cleanup();
    m_Rgn.Cleanup();
    m_DispPerfData.Cleanup();

#ifndef UNDER_CE
    if(m_dwOldBatchLimit > 0)
        GdiSetBatchLimit(m_dwOldBatchLimit);
#endif

    return TRUE;
}
