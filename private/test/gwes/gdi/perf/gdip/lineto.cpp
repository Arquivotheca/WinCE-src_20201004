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
#include "LineTo.h"

BOOL
CLineToTestSuite::Initialize(TestSuiteInfo * tsi)
{
    info(ECHO, TEXT("In CLineToTestSuite::Initialize"));
    BOOL bRval = TRUE;
    
#ifndef UNDER_CE
    m_dwOldBatchLimit = GdiSetBatchLimit(1);
    if(0 == m_dwOldBatchLimit)
        info(FAIL, TEXT("Failed to disable GDI call batching."));
#endif

    tsi->tsTestName = TEXT("LineTo");

    // determine whether we're using poly coordinates or standard.
    if(m_SectionList->GetDWArray(TEXT("Coordinates0"), 10, NULL, 0))
    {
        if(!m_SectionList->GetDWArray(TEXT("Coordinates"), 10, NULL, 0))
                m_bUsePolyCoordinates = TRUE;
        else
        {
            info(FAIL, TEXT("Both line coordinates and polycoordintes exist, exiting the test."));
            bRval = FALSE;
        }
    }
    else m_bUsePolyCoordinates = FALSE;

    // initialize everything, if anything fails return failure.
    // cleanup is called whether or not there is a failure, which will deallocate 
    // anything that was allocated if there was a failure.
    if(bRval && (bRval = bRval && m_Dest.Initialize(tsi, TEXT("Dest"))))
        if(m_bUsePolyCoordinates?bRval = bRval && m_PolyCoordinates.Initialize(tsi):bRval = bRval && m_Coordinates.Initialize(tsi))
            if(bRval = bRval && m_Rop2.Initialize(tsi))
                if(bRval = bRval && m_Pen.Initialize(tsi))
                    if(bRval = bRval && m_Rgn.Initialize(tsi))
                        bRval = bRval && m_DispPerfData.Initialize(tsi);

    return bRval;
}

BOOL
CLineToTestSuite::PreRun(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CLineToTestSuite::PreRun"));

    m_Dest.PreRun(tiRunInfo);
    m_hdcDest = m_Dest.GetSurface();

    if(!m_bUsePolyCoordinates)
    {
        m_Coordinates.PreRun(tiRunInfo);
        m_Coordinates.GetCoordinates(&m_ptStartInUse, &m_ptEndInUse);
    }
    else
    {
        m_PolyCoordinates.PreRun(tiRunInfo);
        m_sPointsInUse = m_PolyCoordinates.GetCoordinates();
        m_nCoordinateCountInUse = m_PolyCoordinates.GetCoordinatesCount();
    }

    m_Rop2.PreRun(tiRunInfo, m_hdcDest);

    m_Pen.PreRun(tiRunInfo, m_hdcDest);

    m_Rgn.PreRun(tiRunInfo, m_hdcDest);

    m_DispPerfData.PreRun(tiRunInfo);

    return TRUE;
}

BOOL
CLineToTestSuite::Run()
{
    BOOL bRval = TRUE;
    // logging here can cause timing issues.
    //info(DETAIL, TEXT("In CLineToTestSuite::Run"));
    // in order to be consistent with other drawing api's, we have to move the start and end points for each run,
    // thus this requres two individual API calls.

    if(!m_bUsePolyCoordinates)
    {
        bRval &= MoveToEx(m_hdcDest, m_ptStartInUse.x, m_ptStartInUse.y, NULL);
        bRval &= LineTo(m_hdcDest, m_ptEndInUse.x, m_ptEndInUse.y);
    }
    else
    {
        bRval &= MoveToEx(m_hdcDest, m_sPointsInUse[0].x, m_sPointsInUse[0].y, NULL);
        for(int i = 1; i < m_nCoordinateCountInUse; i++)
            bRval &= LineTo(m_hdcDest, m_sPointsInUse[i].x, m_sPointsInUse[i].y);
    }

    return bRval;
}

BOOL
CLineToTestSuite::AddPostRunData(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CLineToTestSuite::AddPostRunData"));

    return m_DispPerfData.AddPostRunData(tiRunInfo);
}

BOOL
CLineToTestSuite::PostRun()
{
    info(DETAIL, TEXT("In CLineToTestSuite::PostRun"));

    m_nIterationCount++;

    // iterate to the next options
    if(m_Dest.PostRun())
        if(m_bUsePolyCoordinates?m_PolyCoordinates.PostRun():m_Coordinates.PostRun())
            if(m_Rop2.PostRun(m_hdcDest))
                if(m_Pen.PostRun(m_hdcDest))
                    if(m_Rgn.PostRun())
                        return FALSE;

    return TRUE;
}

BOOL
CLineToTestSuite::Cleanup()
{
    info(ECHO, TEXT("In CLineToTestSuite::Cleanup"));

    // clean up all of the test options
    m_Dest.Cleanup();

    if(m_bUsePolyCoordinates)
        m_PolyCoordinates.Cleanup();
    else
        m_Coordinates.Cleanup();

    m_Rop2.Cleanup();
    m_Pen.Cleanup();
    m_Rgn.Cleanup();
    m_DispPerfData.Cleanup();
#ifndef UNDER_CE
    if(m_dwOldBatchLimit > 0)
        GdiSetBatchLimit(m_dwOldBatchLimit);
#endif

    return TRUE;
}
