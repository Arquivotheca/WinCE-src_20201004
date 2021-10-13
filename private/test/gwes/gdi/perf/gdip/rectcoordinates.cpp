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
#include "rectcoordinates.h"

BOOL
CRectCoordinates::Initialize(TestSuiteInfo *tsi, TSTRING tsEntryName)
{
    BOOL bRval = TRUE;
    int nCount = 0;

    tsi->tsFieldDescription.push_back(TEXT("Width"));
    tsi->tsFieldDescription.push_back(TEXT("Height"));
    tsi->tsFieldDescription.push_back(TEXT("Area"));

    // if the coordinates entry is defined, then use it, if it isn't then try the next
    // coordinate entry method.
    if(nCount = m_SectionList->GetDWArray(tsEntryName, 10, NULL, 0))
    {
        // the number of entries MUST be divisible by 6, we're expecting there to be
        // sets of six entries for the coordinates top, left, width, height, source top, and source left.
        if(nCount % RECTCOORDINATEENTRYCOUNT == 0)
        {
            // allocate a buffer to use to retrieve the coordinates from the script file
            // since we can't put it directly in the StretchCoordinates structure.
            DWORD *dw;
            int nCountConverted;
            dw = new(DWORD[nCount]);
            if(dw)
            {
                // retrieve the actual list of coordinates from the script file.
                nCountConverted = m_SectionList->GetDWArray(tsEntryName, 10, dw, nCount);

                if(nCountConverted == nCount)
                {
                    m_nMaxCoordinatesIndex = (int)(nCount / RECTCOORDINATEENTRYCOUNT);
                    m_sCoordinates = new(RECT[m_nMaxCoordinatesIndex]);

                    for(m_nCoordinatesIndex = 0; m_nCoordinatesIndex < m_nMaxCoordinatesIndex; m_nCoordinatesIndex++)
                    {
                        // the pattern is (dest left, top, width, height, source left, top), just like the blitting prototypes.
                        m_sCoordinates[m_nCoordinatesIndex].left= dw[m_nCoordinatesIndex * RECTCOORDINATEENTRYCOUNT];
                        m_sCoordinates[m_nCoordinatesIndex].top= dw[m_nCoordinatesIndex * RECTCOORDINATEENTRYCOUNT + 1];
                        m_sCoordinates[m_nCoordinatesIndex].right= dw[m_nCoordinatesIndex * RECTCOORDINATEENTRYCOUNT + 2];
                        m_sCoordinates[m_nCoordinatesIndex].bottom = dw[m_nCoordinatesIndex * RECTCOORDINATEENTRYCOUNT + 3];
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
        DWORD dwTop, dwLeft;

        m_nMaxCoordinatesIndex = 0;

        int nWidthsRecieved, nHeightsRecieved;
        BOOL bWidthStepFound, bHeightStepFound;

        // to retrieve a top/left offset if the user wants one.
        if(!m_SectionList->GetDWord(TEXT("Top"), 10, &dwTop))
            dwTop = 0;
        
        if(!m_SectionList->GetDWord(TEXT("Left"), 10, &dwLeft))
            dwLeft = 0;

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
            dwWidth[1] = rcWorkArea.right - rcWorkArea.left - 20;
            dwHeight[0] = 0;
            dwHeight[1] = rcWorkArea.bottom - rcWorkArea.top - 20;
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
            m_sCoordinates = new(RECT[m_nMaxCoordinatesIndex]);
        }
        else m_sCoordinates = NULL;

        // if the buffer allocation failed, log an error
        if(NULL != m_sCoordinates)
        {
            // we're using the coordinate index to initialize, reset to the start when complete.
            m_nCoordinatesIndex = 0;

            // create the coordinates to use
            for(DWORD nWidthToUse = dwWidth[0]; nWidthToUse <= dwWidth[1]; nWidthToUse += dwWidthStep)
                for(DWORD nHeightToUse = dwHeight[0]; nHeightToUse <= dwHeight[1]; nHeightToUse += dwHeightStep)
                {
                    m_sCoordinates[m_nCoordinatesIndex].left= dwLeft;
                    m_sCoordinates[m_nCoordinatesIndex].top= dwTop;
                    m_sCoordinates[m_nCoordinatesIndex].right = nWidthToUse + dwLeft;
                    m_sCoordinates[m_nCoordinatesIndex].bottom = nHeightToUse + dwTop;

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

    // reset the current coordinate index to the start of the array.
    m_nCoordinatesIndex = 0;
    return bRval;
}

BOOL
CRectCoordinates::PreRun(TestInfo *tiRunInfo)
{
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].right - m_sCoordinates[m_nCoordinatesIndex].left));
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].bottom - m_sCoordinates[m_nCoordinatesIndex].top));
    tiRunInfo->Descriptions.push_back(itos((m_sCoordinates[m_nCoordinatesIndex].right - m_sCoordinates[m_nCoordinatesIndex].left) * (m_sCoordinates[m_nCoordinatesIndex].bottom - m_sCoordinates[m_nCoordinatesIndex].top)));

    return TRUE;
}

BOOL
CRectCoordinates::PostRun()
{
    BOOL bRVal = FALSE;

    m_nCoordinatesIndex++;

    if(m_nCoordinatesIndex >= m_nMaxCoordinatesIndex)
    {
        m_nCoordinatesIndex = 0;
        bRVal = TRUE;
    }

    return bRVal;
}

BOOL
CRectCoordinates::Cleanup()
{
    delete [] m_sCoordinates;
    return TRUE;
}

RECT
CRectCoordinates::GetCoordinates()
{
    return m_sCoordinates[m_nCoordinatesIndex];
}

