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
#include "SetPixel.h"

BOOL
CSetPixelTestSuite::Initialize(TestSuiteInfo * tsi)
{
    info(ECHO, TEXT("In CSetPixelTestSuite::Initialize"));
    BOOL bRval = TRUE;
    
#ifndef UNDER_CE
    m_dwOldBatchLimit = GdiSetBatchLimit(1);
    if(0 == m_dwOldBatchLimit)
        info(FAIL, TEXT("Failed to disable GDI call batching."));
#endif

    tsi->tsTestName = TEXT("SetPixel");

    // initialize everything, if anything fails return failure.
    // cleanup is called whether or not there is a failure, which will deallocate 
    // anything that was allocated if there was a failure.
    if(bRval && (bRval = bRval && m_Dest.Initialize(tsi, TEXT("Dest"))))
        if(bRval = bRval && m_Coordinates.Initialize(tsi, TEXT("Coordinates")))
            if(bRval = bRval && m_Rgn.Initialize(tsi))
                bRval = bRval && m_DispPerfData.Initialize(tsi);


    return bRval;
}

BOOL
CSetPixelTestSuite::PreRun(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CSetPixelTestSuite::PreRun"));

    m_Dest.PreRun(tiRunInfo);
    m_hdcDest = m_Dest.GetSurface();

    m_Coordinates.PreRun(tiRunInfo);
    m_ptPointInUse = m_Coordinates.GetCoordinate();

    m_Rgn.PreRun(tiRunInfo, m_hdcDest);

    m_DispPerfData.PreRun(tiRunInfo);

    m_crInUse = DEFAULT_COLOR_VALUE;

    return TRUE;
}

BOOL
CSetPixelTestSuite::Run()
{
    BOOL bRval = TRUE;
    // logging here can cause timing issues.
    //info(DETAIL, TEXT("In CSetPixelTestSuite::Run"));
    return SetPixel(m_hdcDest, m_ptPointInUse.x, m_ptPointInUse.y, m_crInUse);
}

BOOL
CSetPixelTestSuite::AddPostRunData(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CSetPixelTestSuite::AddPostRunData"));

    return m_DispPerfData.AddPostRunData(tiRunInfo);
}

BOOL
CSetPixelTestSuite::PostRun()
{
    info(DETAIL, TEXT("In CSetPixelTestSuite::PostRun"));

    m_nIterationCount++;

    // iterate to the next options
    if(m_Dest.PostRun())
        if(m_Coordinates.PostRun())
            if(m_Rgn.PostRun())
                return FALSE;

    return TRUE;
}

BOOL
CSetPixelTestSuite::Cleanup()
{
    info(ECHO, TEXT("In CSetPixelTestSuite::Cleanup"));

    // clean up all of the test options
    m_Dest.Cleanup();
    m_Coordinates.Cleanup();
    m_Rgn.Cleanup();
    m_DispPerfData.Cleanup();

#ifndef UNDER_CE
    if(m_dwOldBatchLimit > 0)
        GdiSetBatchLimit(m_dwOldBatchLimit);
#endif

    return TRUE;
}
