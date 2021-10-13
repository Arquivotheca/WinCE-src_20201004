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
#include "MaskBlt.h"

BOOL
CMaskBltTestSuite::InitializeCoordinates(TestSuiteInfo * tsi)
{
    BOOL bRval = TRUE;
    int nCount = 0;

    tsi->tsFieldDescription.push_back(TEXT("Width"));
    tsi->tsFieldDescription.push_back(TEXT("Height"));
    tsi->tsFieldDescription.push_back(TEXT("Area"));

    // if the coordinates entry is defined, then use it, if it isn't then try the next
    // coordinate entry method.
    if(nCount = m_SectionList->GetDWArray(TEXT("Coordinates"), 10, NULL, 0))
    {
        // the number of entries MUST be divisible by 6, we're expecting there to be
        // sets of six entries for the coordinates top, left, width, height, source top, and source left.
        if(nCount % MASKBLTCOORDINATECOUNT == 0)
        {
            // allocate a buffer to use to retrieve the coordinates from the script file
            // since we can't put it directly in the MaskBltCoordinates structure.
            DWORD *dw;
            int nCountConverted;
            dw = new(DWORD[nCount]);
            if(dw)
            {
                // retrieve the actual list of coordinates from the script file.
                nCountConverted = m_SectionList->GetDWArray(TEXT("Coordinates"), 10, dw, nCount);

                if(nCountConverted == nCount)
                {
                    // allocate the BltCoordinates structure that MaskBlt uses, and copy over
                    // the entries into the right locations.
                    m_nMaxCoordinatesIndex = (int)(nCount / MASKBLTCOORDINATECOUNT);
                    m_sCoordinates = new(MaskBltCoordinates[m_nMaxCoordinatesIndex]);

                    if(m_sCoordinates)
                    {
                        for(m_nCoordinatesIndex = 0; m_nCoordinatesIndex < m_nMaxCoordinatesIndex; m_nCoordinatesIndex++)
                        {
                            // the pattern is (dest left, top, width, height, source left, top), just like the MaskBlt prototype.
                            m_sCoordinates[m_nCoordinatesIndex].nDestLeft= dw[m_nCoordinatesIndex * MASKBLTCOORDINATECOUNT];
                            m_sCoordinates[m_nCoordinatesIndex].nDestTop= dw[m_nCoordinatesIndex * MASKBLTCOORDINATECOUNT + 1];
                            m_sCoordinates[m_nCoordinatesIndex].nDestWidth= dw[m_nCoordinatesIndex * MASKBLTCOORDINATECOUNT + 2];
                            m_sCoordinates[m_nCoordinatesIndex].nDestHeight = dw[m_nCoordinatesIndex * MASKBLTCOORDINATECOUNT + 3];
                            m_sCoordinates[m_nCoordinatesIndex].nSrcLeft = dw[m_nCoordinatesIndex * MASKBLTCOORDINATECOUNT + 4];
                            m_sCoordinates[m_nCoordinatesIndex].nSrcTop = dw[m_nCoordinatesIndex * MASKBLTCOORDINATECOUNT + 5];
                            m_sCoordinates[m_nCoordinatesIndex].nMaskLeft = dw[m_nCoordinatesIndex * MASKBLTCOORDINATECOUNT + 6];
                            m_sCoordinates[m_nCoordinatesIndex].nMaskTop = dw[m_nCoordinatesIndex * MASKBLTCOORDINATECOUNT + 7];

                        }
                    }
                    else
                    {
                        info(FAIL, TEXT("Coordinate allocation failed!"));
                        bRval = FALSE;
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
        DWORD dwWidth[2], dwHeight[2];
        DWORD dwWidthStep, dwHeightStep;

        m_sCoordinates = NULL;
        m_nMaxCoordinatesIndex = 0;

        int nWidthsRecieved, nHeightsRecieved;
        BOOL bWidthStepFound, bHeightStepFound;

        // try to retrieve the controls for rectangles created.
        nWidthsRecieved = m_SectionList->GetDWArray(TEXT("Width"), 10, dwWidth, 2);
        nHeightsRecieved = m_SectionList->GetDWArray(TEXT("Height"), 10, dwHeight, 2);
        bWidthStepFound = m_SectionList->GetDWord(TEXT("WidthStep"), 10, &dwWidthStep);
        bHeightStepFound = m_SectionList->GetDWord(TEXT("HeightStep"), 10, &dwHeightStep);

        // if any one of these are set, then they all must be set, if they aren't then it's an error.
        if(nWidthsRecieved > 0 || nHeightsRecieved > 0 || bWidthStepFound || bHeightStepFound)
        {
            // there must be 2 width entries and no no steps for a single coordinate.
            if(nWidthsRecieved == 1 && nHeightsRecieved == 1 && !bWidthStepFound && !bHeightStepFound)
            {
                // set the width step/height step to be the width/height so we only have the one coordinate
                dwWidth[1] = dwWidth[0];
                dwHeight[1] = dwHeight[0];
                dwWidthStep = dwWidth[0];
                dwHeightStep = dwHeight[0];
            }
            else if(!(2 == nWidthsRecieved && 2 == nHeightsRecieved && bWidthStepFound && bHeightStepFound))
            {
                info(FAIL, TEXT("Width, Height, WidthStep or HeightStep not set properly."));
                bRval = FALSE;
            }
        }
        else
        {
            // set up the default coordinates if the user didn't request a set of coordinates.
            RECT rcWorkArea;
            SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);

            dwWidth[0] = 0;
            dwWidth[1] = rcWorkArea.right - rcWorkArea.left;
            dwHeight[0] = 0;
            dwHeight[1] = rcWorkArea.bottom - rcWorkArea.top;
            dwWidthStep = (dwWidth[1]-dwWidth[0])/10;
            dwHeightStep = (dwHeight[1]-dwHeight[0])/10;
        }

        // if we aren't exiting due to errors above, allocate the buffer.
        if(bRval)
        {
            // from one of the two paths above, the coordinate information should be set now.
            // 10 to 300 is 30 entries, (10-300)/10 is room for 29 entries, compensate for using the first and last entry.
            m_nMaxCoordinatesIndex = (((dwWidth[1]-dwWidth[0])/dwWidthStep) + 1) * (((dwHeight[1]-dwHeight[0])/dwHeightStep) + 1);

            // allocate the buffer for coordinates.
            m_sCoordinates = new(MaskBltCoordinates[m_nMaxCoordinatesIndex]);

            // if the buffer allocation failed, log an error
            // otherwise fill it
            if(NULL != m_sCoordinates)
            {
                // we're using the coordinate index to initialize, reset to the start when complete.
                m_nCoordinatesIndex = 0;

                // create the coordinates to use
                for(DWORD nWidthToUse = dwWidth[0]; nWidthToUse <= dwWidth[1]; nWidthToUse += dwWidthStep)
                    for(DWORD nHeightToUse = dwHeight[0]; nHeightToUse <= dwHeight[1]; nHeightToUse += dwHeightStep)
                    {
                        m_sCoordinates[m_nCoordinatesIndex].nDestLeft= 0;
                        m_sCoordinates[m_nCoordinatesIndex].nDestTop= 0;
                        m_sCoordinates[m_nCoordinatesIndex].nSrcLeft = 0;
                        m_sCoordinates[m_nCoordinatesIndex].nSrcTop = 0;
                        m_sCoordinates[m_nCoordinatesIndex].nDestHeight= nHeightToUse;
                        m_sCoordinates[m_nCoordinatesIndex].nDestWidth = nWidthToUse;
                        m_sCoordinates[m_nCoordinatesIndex].nMaskTop= 0;
                        m_sCoordinates[m_nCoordinatesIndex].nMaskLeft = 0;

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
        // else we failed earlier, so the error was already sent to the user.
    }

    // reset the current coordinate index to the start of the array.
    m_nCoordinatesIndex = 0;
    return bRval;
}

BOOL
CMaskBltTestSuite::InitializeMask()
{
    RECT rcWorkArea;
    BOOL bRval = FALSE;

    if(SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0))
    {
        m_hbmpMask = CreateBitmap(rcWorkArea.right - rcWorkArea.left, 
                                  rcWorkArea.bottom - rcWorkArea.top, 1, 1, NULL);
        if(m_hbmpMask)
            bRval = TRUE;
    }

    return bRval;
}

BOOL
CMaskBltTestSuite::Initialize(TestSuiteInfo * tsi)
{
    info(ECHO, TEXT("In CMaskBltTestSuite::Initialize"));
    BOOL bRval = TRUE;

#ifndef UNDER_CE
    m_dwOldBatchLimit = GdiSetBatchLimit(1);
    if(0 == m_dwOldBatchLimit)
        info(FAIL, TEXT("Failed to disable GDI call batching."));
#endif

    tsi->tsTestName = TEXT("MaskBlt");

    // initialize everything, if anything fails return failure.
    // cleanup is called whether or not there is a failure, which will deallocate 
    // anything that was allocated if there was a failure.
    if(bRval && (bRval = bRval && InitializeCoordinates(tsi)))
        if(bRval = bRval && m_Rops.Initialize(tsi))
            if(bRval = bRval && m_Source.Initialize(tsi, TEXT("Source")))
                if(bRval = bRval && m_Dest.Initialize(tsi, TEXT("Dest")))
                    if(bRval = bRval && m_Brush.Initialize(tsi))
                        if(bRval = bRval && m_Rgn.Initialize(tsi))
                            if(bRval = bRval && InitializeMask())
                                bRval = bRval && m_DispPerfData.Initialize(tsi);

    return bRval;
}

BOOL
CMaskBltTestSuite::PreRun(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CMaskBltTestSuite::PreRun"));

    // Must match initialize order.  Coordinates, rop's, source, dest, brush.
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].nDestWidth));
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].nDestHeight));
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].nDestWidth * m_sCoordinates[m_nCoordinatesIndex].nDestHeight));

    m_Rops.PreRun(tiRunInfo);
    m_dwRop = m_Rops.GetRop();

    m_Source.PreRun(tiRunInfo);
    m_hdcSource = m_Source.GetSurface();

    m_Dest.PreRun(tiRunInfo);
    m_hdcDest = m_Dest.GetSurface();

    m_Brush.PreRun(tiRunInfo, m_hdcDest);

    m_Rgn.PreRun(tiRunInfo, m_hdcDest);

    m_DispPerfData.PreRun(tiRunInfo);

    return TRUE;
}

BOOL
CMaskBltTestSuite::Run()
{
    // logging here can cause timing issues.
    //info(DETAIL, TEXT("In CMaskBltTestSuite::Run"));
    return(MaskBlt(m_hdcDest,
                        m_sCoordinates[m_nCoordinatesIndex].nDestLeft,
                        m_sCoordinates[m_nCoordinatesIndex].nDestTop,
                        m_sCoordinates[m_nCoordinatesIndex].nDestWidth,
                        m_sCoordinates[m_nCoordinatesIndex].nDestHeight,
                        m_hdcSource,
                        m_sCoordinates[m_nCoordinatesIndex].nSrcLeft,
                        m_sCoordinates[m_nCoordinatesIndex].nSrcTop,
                        m_hbmpMask,
                        m_sCoordinates[m_nCoordinatesIndex].nMaskTop,
                        m_sCoordinates[m_nCoordinatesIndex].nMaskLeft,
                        m_dwRop));
}

BOOL
CMaskBltTestSuite::AddPostRunData(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CMaskBltTestSuite::AddPostRunData"));

    return m_DispPerfData.AddPostRunData(tiRunInfo);
}

BOOL
CMaskBltTestSuite::PostRun()
{
    info(DETAIL, TEXT("In CMaskBltTestSuite::PostRun"));

    m_nIterationCount++;

    m_nCoordinatesIndex++;
    if(m_nCoordinatesIndex >= m_nMaxCoordinatesIndex)
    {
        m_nCoordinatesIndex=0;
        if(m_Rops.PostRun())
            if(m_Source.PostRun())
                if(m_Dest.PostRun())
                    if(m_Brush.PostRun(m_hdcDest))
                        if(m_Rgn.PostRun())
                            return FALSE;
    }

    return TRUE;
}

BOOL
CMaskBltTestSuite::Cleanup()
{
    info(ECHO, TEXT("In CMaskBltTestSuite::Cleanup"));

    // free coordinates.
    delete[] m_sCoordinates;

    // free the ROP's
    // clean up all of the test options
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
