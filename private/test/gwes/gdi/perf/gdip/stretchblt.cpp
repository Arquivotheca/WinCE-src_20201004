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
#include "StretchBlt.h"

BOOL
CStretchBltTestSuite::InitializeStretchBltModeIndex(TestSuiteInfo * tsi)
{
    BOOL bRval = TRUE;

    tsi->tsFieldDescription.push_back(TEXT("StretchBltMode"));

    // retrieve list of modes
    if(m_nMaxStretchBltModeIndex = m_SectionList->GetStringArray(TEXT("StretchBltMode"), NULL, 0))
    {
        m_tsStretchBltModeNameArray = new(TSTRING[m_nMaxStretchBltModeIndex]);
        m_nStretchBltModes = new(int[m_nMaxStretchBltModeIndex]);

        if(m_tsStretchBltModeNameArray)
            m_SectionList->GetStringArray(TEXT("StretchBltMode"), m_tsStretchBltModeNameArray, m_nMaxStretchBltModeIndex);
    }
    // if there's no destinations, use the default.
    else
    {
        m_nMaxStretchBltModeIndex = 1;
        m_tsStretchBltModeNameArray = new(TSTRING[m_nMaxStretchBltModeIndex]);
        m_nStretchBltModes = new(int[m_nMaxStretchBltModeIndex]);

        if(m_tsStretchBltModeNameArray)
            m_tsStretchBltModeNameArray[0] = TEXT("BLACKONWHITE");
    }

    // if the destinations are available, use them
    if(m_tsStretchBltModeNameArray && m_nStretchBltModes)
    {
        for(m_nStretchBltModeIndex = 0; m_nStretchBltModeIndex < m_nMaxStretchBltModeIndex && bRval; m_nStretchBltModeIndex++)
        {
            if(TEXT("BLACKONWHITE") == m_tsStretchBltModeNameArray[m_nStretchBltModeIndex])
                m_nStretchBltModes[m_nStretchBltModeIndex] = BLACKONWHITE;
        #ifdef UNDER_CE
            else if (TEXT("BILINEAR") == m_tsStretchBltModeNameArray[m_nStretchBltModeIndex])
                m_nStretchBltModes[m_nStretchBltModeIndex] = BILINEAR;
        #endif
            else
            {
                info(FAIL, TEXT("Unknown StretchBltMode %s."), m_tsStretchBltModeNameArray[m_nStretchBltModeIndex]);
                bRval = FALSE;
            }
        }
    }
    else
    {
        info(FAIL, TEXT("Failed to allocate StretchBltMode name/value array."));
        bRval = FALSE;
    }
    
    m_nStretchBltModeIndex = 0;

    return bRval;
}

BOOL
CStretchBltTestSuite::Initialize(TestSuiteInfo * tsi)
{
    info(ECHO, TEXT("In CStretchBltTestSuite::Initialize"));
    BOOL bRval = TRUE;
    
#ifndef UNDER_CE
    m_dwOldBatchLimit = GdiSetBatchLimit(1);
    if(0 == m_dwOldBatchLimit)
        info(FAIL, TEXT("Failed to disable GDI call batching."));
#endif

    tsi->tsTestName = TEXT("StretchBlt");

    // initialize everything, if anything fails return failure.
    // cleanup is called whether or not there is a failure, which will deallocate 
    // anything that was allocated if there was a failure.
    if(bRval && (bRval = bRval && m_StretchCoordinates.Initialize(tsi)))
        if(bRval = bRval && InitializeStretchBltModeIndex(tsi))
            if(bRval = bRval && m_Rops.Initialize(tsi))
                if(bRval = bRval &&m_Source.Initialize(tsi, TEXT("Source")))
                    if(bRval = bRval && m_Dest.Initialize(tsi, TEXT("Dest")))
                        if(bRval = bRval && m_Brush.Initialize(tsi))
                            if(bRval = bRval && m_Rgn.Initialize(tsi))
                                bRval = bRval && m_DispPerfData.Initialize(tsi);

    return bRval;
}

BOOL
CStretchBltTestSuite::PreRun(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CStretchBltTestSuite::PreRun"));

    m_StretchCoordinates.PreRun(tiRunInfo);
    m_sCoordinateInUse = m_StretchCoordinates.GetCoordinates();

    tiRunInfo->Descriptions.push_back(m_tsStretchBltModeNameArray[m_nStretchBltModeIndex]);

    m_Rops.PreRun(tiRunInfo);
    m_dwRop = m_Rops.GetRop();

    m_Source.PreRun(tiRunInfo);
    m_hdcSource = m_Source.GetSurface();

    m_Dest.PreRun(tiRunInfo);
    m_hdcDest = m_Dest.GetSurface();

    m_Brush.PreRun(tiRunInfo, m_hdcDest);

    m_Rgn.PreRun(tiRunInfo, m_hdcDest);

    m_nOldStretchBltMode = SetStretchBltMode(m_hdcDest, m_nStretchBltModes[m_nStretchBltModeIndex]);

    m_DispPerfData.PreRun(tiRunInfo);

    return TRUE;
}

BOOL
CStretchBltTestSuite::Run()
{
    // logging here can cause timing issues.
    //info(DETAIL, TEXT("In CStretchBltTestSuite::Run"));
    return(StretchBlt(m_hdcDest,
                        m_sCoordinateInUse.nDestLeft,
                        m_sCoordinateInUse.nDestTop,
                        m_sCoordinateInUse.nDestWidth,
                        m_sCoordinateInUse.nDestHeight,
                        m_hdcSource,
                        m_sCoordinateInUse.nSrcLeft,
                        m_sCoordinateInUse.nSrcTop,
                        m_sCoordinateInUse.nSrcWidth,
                        m_sCoordinateInUse.nSrcHeight,
                        m_dwRop));
}

BOOL
CStretchBltTestSuite::AddPostRunData(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CStretchBltTestSuite::AddPostRunData"));

    return m_DispPerfData.AddPostRunData(tiRunInfo);
}

BOOL
CStretchBltTestSuite::PostRun()
{
    info(DETAIL, TEXT("In CStretchBltTestSuite::PostRun"));

    SetStretchBltMode(m_hdcDest, m_nOldStretchBltMode);

    m_nIterationCount++;

    // iterate to the next options
    if(m_StretchCoordinates.PostRun())
    {
        m_nStretchBltModeIndex++;
        if(m_nStretchBltModeIndex >= m_nMaxStretchBltModeIndex)
        {
            m_nStretchBltModeIndex=0;
            if(m_Rops.PostRun())
                if(m_Source.PostRun())
                    if(m_Dest.PostRun())
                        if(m_Brush.PostRun(m_hdcDest))
                            if(m_Rgn.PostRun())
                                return FALSE;
        }
    }
    return TRUE;
}

BOOL
CStretchBltTestSuite::Cleanup()
{
    info(ECHO, TEXT("In CStretchBltTestSuite::Cleanup"));

    delete[] m_tsStretchBltModeNameArray;
    delete[] m_nStretchBltModes;

    // clean up all of the test options
    m_StretchCoordinates.Cleanup();
    m_Rops.Cleanup();
    m_Source.Cleanup();
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
