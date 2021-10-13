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
#include "RoundRect.h"

BOOL
CRoundRectTestSuite::InitializeCoordinates(TestSuiteInfo * tsi)
{
    BOOL bRval = TRUE;
    int nCount = 0;

    // Fill in the test suite's fields that are varied with each run.
    tsi->tsFieldDescription.push_back(TEXT("RectWidth"));
    tsi->tsFieldDescription.push_back(TEXT("RectHeight"));
    tsi->tsFieldDescription.push_back(TEXT("RectArea"));
    tsi->tsFieldDescription.push_back(TEXT("EllipseWidth"));
    tsi->tsFieldDescription.push_back(TEXT("EllipseHeight"));
    tsi->tsFieldDescription.push_back(TEXT("EllipseArea"));

    // if the coordinates entry is defined, then use it, if it isn't then try the next
    // coordinate entry method.
    if(nCount = m_SectionList->GetDWArray(TEXT("Coordinates"), 10, NULL, 0))
    {
        // the number of entries MUST be divisible by 6, we're expecting there to be
        // sets of six entries for the coordinates top, left, width, height, source top, and source left.
        if(nCount % ROUNDRECTCOORDINATEENTRYCOUNT == 0)
        {
            // allocate a buffer to use to retrieve the coordinates from the script file
            // since we can't put it directly in the BitBltCoordinates structure.
            DWORD *dw;
            int nCountConverted;
            dw = new(DWORD[nCount]);
            if(dw)
            {
                // retrieve the actual list of coordinates from the script file.
                nCountConverted = m_SectionList->GetDWArray(TEXT("Coordinates"), 10, dw, nCount);

                if(nCountConverted == nCount)
                {
                    // allocate the BltCoordinates structure that BitBlt uses, and copy over
                    // the entries into the right locations.
                    m_nMaxCoordinatesIndex = (int)(nCount / ROUNDRECTCOORDINATEENTRYCOUNT);
                    m_sCoordinates = new(RoundRectCoordinates[m_nMaxCoordinatesIndex]);

                    for(m_nCoordinatesIndex = 0; m_nCoordinatesIndex < m_nMaxCoordinatesIndex; m_nCoordinatesIndex++)
                    {
                        // the pattern is (left, top, right, bottom, ellipse width, ellipse height), just like the RoundRect prototype.
                        m_sCoordinates[m_nCoordinatesIndex].nLeft= dw[m_nCoordinatesIndex * ROUNDRECTCOORDINATEENTRYCOUNT];
                        m_sCoordinates[m_nCoordinatesIndex].nTop= dw[m_nCoordinatesIndex * ROUNDRECTCOORDINATEENTRYCOUNT + 1];
                        m_sCoordinates[m_nCoordinatesIndex].nRight= dw[m_nCoordinatesIndex * ROUNDRECTCOORDINATEENTRYCOUNT + 2];
                        m_sCoordinates[m_nCoordinatesIndex].nBottom = dw[m_nCoordinatesIndex * ROUNDRECTCOORDINATEENTRYCOUNT + 3];
                        m_sCoordinates[m_nCoordinatesIndex].nWidth = dw[m_nCoordinatesIndex * ROUNDRECTCOORDINATEENTRYCOUNT + 4];
                        m_sCoordinates[m_nCoordinatesIndex].nHeight = dw[m_nCoordinatesIndex * ROUNDRECTCOORDINATEENTRYCOUNT + 5];
                    }
                    delete[] dw;
                }
                else
                {
                    info(FAIL, TEXT("%d coordinate entries in the file, however only %d are valid!"), nCount, nCountConverted);
                    bRval = FALSE;
                }
            }
            else
            {
                info(FAIL, TEXT("Temporary DWORD buffer allocation failed."));
                bRval = FALSE;
            }
        }
        else
        {
                info(FAIL, TEXT("Incorrect number of coordinates given."));
                bRval = FALSE;
        }

    }
    // if the user didn't give detailed coordinates, then use the default behavior.
    else
    {
        DWORD dwRectWidth[2], dwRectHeight[2];
        DWORD dwRectWidthStep, dwRectHeightStep;
        DWORD dwEllipseWidth[2], dwEllipseHeight[2];
        DWORD dwEllipseWidthStep, dwEllipseHeightStep;
        DWORD dwTop, dwLeft;

        m_nMaxCoordinatesIndex = 0;
        m_sCoordinates = NULL;

        int nRectWidthsRecieved, nRectHeightsRecieved;
        BOOL bRectWidthStepFound, bRectHeightStepFound;
        int nEllipseWidthsRecieved, nEllipseHeightsRecieved;
        BOOL bEllipseWidthStepFound, bEllipseHeightStepFound;

        // to retrieve a top/left offset if the user wants one.
        if(!m_SectionList->GetDWord(TEXT("Top"), 10, &dwTop))
            dwTop = 0;
        
        if(!m_SectionList->GetDWord(TEXT("Left"), 10, &dwLeft))
            dwLeft = 0;
        
        // try to retrieve the controls for rectangles created.
        nRectWidthsRecieved = m_SectionList->GetDWArray(TEXT("RectWidth"), 10, dwRectWidth, 2);
        nRectHeightsRecieved = m_SectionList->GetDWArray(TEXT("RectHeight"), 10, dwRectHeight, 2);
        bRectWidthStepFound = m_SectionList->GetDWord(TEXT("RectWidthStep"), 10, &dwRectWidthStep);
        bRectHeightStepFound = m_SectionList->GetDWord(TEXT("RectHeightStep"), 10, &dwRectHeightStep);

        nEllipseWidthsRecieved = m_SectionList->GetDWArray(TEXT("EllipseWidth"), 10, dwEllipseWidth, 2);
        nEllipseHeightsRecieved = m_SectionList->GetDWArray(TEXT("EllipseHeight"), 10, dwEllipseHeight, 2);
        bEllipseWidthStepFound = m_SectionList->GetDWord(TEXT("EllipseWidthStep"), 10, &dwEllipseWidthStep);
        bEllipseHeightStepFound = m_SectionList->GetDWord(TEXT("EllipseHeightStep"), 10, &dwEllipseHeightStep);

        // if any one of these are set, then they all must be set, if they aren't then it's an error.
        if(nRectWidthsRecieved > 0 || nRectHeightsRecieved > 0 || bRectWidthStepFound || bRectHeightStepFound ||
            nEllipseWidthsRecieved > 0 || nEllipseHeightsRecieved > 0 || bEllipseWidthStepFound || bEllipseHeightStepFound)
        {
            // there must be 2 width entries and no no steps for a single coordinate.
            if(nRectWidthsRecieved == 1 && nRectHeightsRecieved == 1 && !bRectWidthStepFound && !bRectHeightStepFound &&
                nEllipseWidthsRecieved == 1 && nEllipseHeightsRecieved == 1 && !bEllipseWidthStepFound && !bEllipseHeightStepFound)
            {
                // set the width step/height step to be the width/height so we only have the one coordinate
                dwRectWidth[1] = dwRectWidth[0];
                dwRectHeight[1] = dwRectHeight[0];
                dwRectWidthStep = dwRectWidth[0];
                dwRectHeightStep = dwRectHeight[0];

                dwEllipseWidth[1] = dwEllipseWidth[0];
                dwEllipseHeight[1] = dwEllipseHeight[0];
                dwEllipseWidthStep = dwEllipseWidth[0];
                dwEllipseHeightStep = dwEllipseHeight[0];
            }
            else if(!(2 == nRectWidthsRecieved && 2 == nRectHeightsRecieved && bRectWidthStepFound && bRectHeightStepFound &&
                         2 == nEllipseWidthsRecieved && 2 == nEllipseHeightsRecieved && bEllipseWidthStepFound && bEllipseHeightStepFound))
            {
                info(FAIL, TEXT("RectWidth, RectHeight, RectWidthStep, RectHeightStep, EllipseWidth, EllipseHeight, EllipseWidthStep, or EllipseHeightStep not set properly."));
                bRval = FALSE;
            }
        }
        else
        {
            // set up the default coordinates if the user didn't request a set of coordinates.
            RECT rcWorkArea;
            SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);

            dwRectWidth[0] = 0;
            dwRectWidth[1] = rcWorkArea.right - rcWorkArea.left;
            dwRectHeight[0] = 0;
            dwRectHeight[1] = rcWorkArea.bottom - rcWorkArea.top;
            dwRectWidthStep = (dwRectWidth[1]-dwRectWidth[0])/10;
            dwRectHeightStep = (dwRectHeight[1]-dwRectHeight[0])/10;
            dwEllipseWidth[0] = 0;
            dwEllipseWidth[1] = rcWorkArea.right - rcWorkArea.left;
            dwEllipseHeight[0] = 0;
            dwEllipseHeight[1] = rcWorkArea.bottom - rcWorkArea.top;
            dwEllipseWidthStep = (dwEllipseWidth[1]-dwEllipseWidth[0])/10;
            dwEllipseHeightStep = (dwEllipseHeight[1]-dwEllipseHeight[0])/10;
        }

        // if we aren't exiting due to errors above, allocate the buffer.
        if(bRval)
        {
            // from one of the two paths above, the coordinate information should be set now.
            // 10 to 300 is 30 entries, (10-300)/10 is room for 29 entries, compensate for using the first and last entry.
            m_nMaxCoordinatesIndex = (((dwRectWidth[1]-dwRectWidth[0])/dwRectWidthStep) + 1) * (((dwRectHeight[1]-dwRectHeight[0])/dwRectHeightStep) + 1) * 
                                                (((dwEllipseWidth[1]-dwEllipseWidth[0])/dwEllipseWidthStep) + 1) * (((dwEllipseHeight[1]-dwEllipseHeight[0])/dwEllipseHeightStep) + 1);

            // allocate the buffer for coordinates.
            m_sCoordinates = new(RoundRectCoordinates[m_nMaxCoordinatesIndex]);

            // if the buffer allocation failed, log an error
            // otherwise fill in the coordinates with the ranges specified above.
            if(m_sCoordinates)
            {
                // we're using the coordinate index to initialize, reset to the start when complete.
                m_nCoordinatesIndex = 0;

                // create the coordinates to use
                for(DWORD nEllipseWidthToUse = dwEllipseWidth[0]; nEllipseWidthToUse <= dwEllipseWidth[1]; nEllipseWidthToUse += dwEllipseWidthStep)
                    for(DWORD nEllipseHeightToUse = dwEllipseHeight[0]; nEllipseHeightToUse <= dwEllipseHeight[1]; nEllipseHeightToUse += dwEllipseHeightStep)
                        for(DWORD nRectWidthToUse = dwRectWidth[0]; nRectWidthToUse <= dwRectWidth[1]; nRectWidthToUse += dwRectWidthStep)
                            for(DWORD nRectHeightToUse = dwEllipseHeight[0]; nRectHeightToUse <= dwEllipseHeight[1]; nRectHeightToUse += dwEllipseHeightStep)
                            {
                                m_sCoordinates[m_nCoordinatesIndex].nLeft= dwLeft;
                                m_sCoordinates[m_nCoordinatesIndex].nTop= dwTop;
                                m_sCoordinates[m_nCoordinatesIndex].nRight = nRectWidthToUse + dwLeft;
                                m_sCoordinates[m_nCoordinatesIndex].nBottom = nRectHeightToUse + dwTop;
                                m_sCoordinates[m_nCoordinatesIndex].nWidth= nEllipseHeightToUse;
                                m_sCoordinates[m_nCoordinatesIndex].nHeight = nEllipseWidthToUse;

                                // step to the next coordinate.
                                m_nCoordinatesIndex++;
                            }
            }
            else
            {
                // if we aren't already exiting due to an error, log an error.
                info(FAIL, TEXT("Coordinate allocation failed."));
                bRval = FALSE;
            }
        }
        // else we failed for reasons above, so don't do anything.
    }

    // reset the current coordinate index to the start of the array.
    m_nCoordinatesIndex = 0;

    info(ECHO, TEXT("%d coordinates in use."), m_nMaxCoordinatesIndex);

    return bRval;
}

BOOL
CRoundRectTestSuite::Initialize(TestSuiteInfo * tsi)
{
    info(ECHO, TEXT("In CRoundRectTestSuite::Initialize"));
    BOOL bRval = TRUE;
    
#ifndef UNDER_CE
    m_dwOldBatchLimit = GdiSetBatchLimit(1);
    if(0 == m_dwOldBatchLimit)
        info(FAIL, TEXT("Failed to disable GDI call batching."));
#endif

    tsi->tsTestName = TEXT("Rectangle");

    // initialize everything, if anything fails return failure.
    // cleanup is called whether or not there is a failure, which will deallocate 
    // anything that was allocated if there was a failure.
    if(bRval && (bRval = bRval && m_Dest.Initialize(tsi, TEXT("Dest"))))
        if(bRval = bRval && InitializeCoordinates(tsi))
            if(bRval = bRval && m_Rop2.Initialize(tsi))
                if(bRval = bRval && m_Pen.Initialize(tsi))
                    if(bRval = bRval && m_Brush.Initialize(tsi))
                       if(bRval = bRval && m_Rgn.Initialize(tsi))
                        bRval = bRval && m_DispPerfData.Initialize(tsi);


    return bRval;
}

BOOL
CRoundRectTestSuite::PreRun(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CRoundRectTestSuite::PreRun"));

    m_Dest.PreRun(tiRunInfo);
    m_hdcDest = m_Dest.GetSurface();

    // Must match initialize order.  Coordinates, rop's, source, dest, brush.
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].nRight - m_sCoordinates[m_nCoordinatesIndex].nLeft));
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].nBottom - m_sCoordinates[m_nCoordinatesIndex].nTop));
    tiRunInfo->Descriptions.push_back(itos((m_sCoordinates[m_nCoordinatesIndex].nRight - m_sCoordinates[m_nCoordinatesIndex].nLeft) * (m_sCoordinates[m_nCoordinatesIndex].nBottom - m_sCoordinates[m_nCoordinatesIndex].nTop)));
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].nWidth));
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].nHeight));
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].nWidth * m_sCoordinates[m_nCoordinatesIndex].nHeight));

    m_Rop2.PreRun(tiRunInfo, m_hdcDest);

    m_Pen.PreRun(tiRunInfo, m_hdcDest);

    m_Brush.PreRun(tiRunInfo, m_hdcDest);

    m_Rgn.PreRun(tiRunInfo, m_hdcDest);

    m_DispPerfData.PreRun(tiRunInfo);

    return TRUE;
}

BOOL
CRoundRectTestSuite::Run()
{
    // logging here can cause timing issues.
    //info(DETAIL, TEXT("In CRoundRectTestSuite::Run"));
    return(RoundRect(m_hdcDest,
                        m_sCoordinates[m_nCoordinatesIndex].nLeft,
                        m_sCoordinates[m_nCoordinatesIndex].nTop,
                        m_sCoordinates[m_nCoordinatesIndex].nRight,
                        m_sCoordinates[m_nCoordinatesIndex].nBottom,
                        m_sCoordinates[m_nCoordinatesIndex].nWidth,
                        m_sCoordinates[m_nCoordinatesIndex].nHeight));
}

BOOL
CRoundRectTestSuite::AddPostRunData(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CRoundRectTestSuite::AddPostRunData"));

    return m_DispPerfData.AddPostRunData(tiRunInfo);
}

BOOL
CRoundRectTestSuite::PostRun()
{
    info(DETAIL, TEXT("In CRoundRectTestSuite::PostRun"));

    m_nIterationCount++;

    // iterate to the next options
    m_nCoordinatesIndex++;
    if(m_nCoordinatesIndex >= m_nMaxCoordinatesIndex)
    {
        m_nCoordinatesIndex=0;
            if(m_Dest.PostRun())
                if(m_Rop2.PostRun(m_hdcDest))
                    if(m_Pen.PostRun(m_hdcDest))
                        if(m_Brush.PostRun(m_hdcDest))
                            if(m_Rgn.PostRun())
                                return FALSE;
    }
    return TRUE;
}

BOOL
CRoundRectTestSuite::Cleanup()
{
    info(ECHO, TEXT("In CRoundRectTestSuite::Cleanup"));

    delete[] m_sCoordinates;

    // clean up all of the test options
    m_Dest.Cleanup();
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
