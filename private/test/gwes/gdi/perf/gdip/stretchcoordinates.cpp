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
#include "stretchcoordinates.h"

BOOL
CStretchCoordinates::Initialize(TestSuiteInfo *tsi)
{
    BOOL bRval = TRUE;
    int nCount = 0;

    tsi->tsFieldDescription.push_back(TEXT("Dest Width"));
    tsi->tsFieldDescription.push_back(TEXT("Dest Height"));
    tsi->tsFieldDescription.push_back(TEXT("Dest Area"));
    tsi->tsFieldDescription.push_back(TEXT("Source Width"));
    tsi->tsFieldDescription.push_back(TEXT("Source Height"));
    tsi->tsFieldDescription.push_back(TEXT("Source Area"));

    // if the coordinates entry is defined, then use it, if it isn't then try the next
    // coordinate entry method.
    if(nCount = m_SectionList->GetDWArray(TEXT("Coordinates"), 10, NULL, 0))
    {
        // the number of entries MUST be divisible by 6, we're expecting there to be
        // sets of six entries for the coordinates top, left, width, height, source top, and source left.
        if(nCount % STRETCHCOORDINATEENTRYCOUNT == 0)
        {
            // allocate a buffer to use to retrieve the coordinates from the script file
            // since we can't put it directly in the StretchCoordinates structure.
            DWORD *dw;
            int nCountConverted;
            dw = new(DWORD[nCount]);
            if(dw)
            {
                // retrieve the actual list of coordinates from the script file.
                nCountConverted = m_SectionList->GetDWArray(TEXT("Coordinates"), 10, dw, nCount);

                if(nCountConverted == nCount)
                {
                    m_nMaxCoordinatesIndex = (int)(nCount / STRETCHCOORDINATEENTRYCOUNT);
                    m_sCoordinates = new(StretchCoordinates[m_nMaxCoordinatesIndex]);

                    for(m_nCoordinatesIndex = 0; m_nCoordinatesIndex < m_nMaxCoordinatesIndex; m_nCoordinatesIndex++)
                    {
                        // the pattern is (dest left, top, width, height, source left, top), just like the blitting prototypes.
                        m_sCoordinates[m_nCoordinatesIndex].nDestLeft = dw[m_nCoordinatesIndex * STRETCHCOORDINATEENTRYCOUNT];
                        m_sCoordinates[m_nCoordinatesIndex].nDestTop = dw[m_nCoordinatesIndex * STRETCHCOORDINATEENTRYCOUNT + 1];
                        m_sCoordinates[m_nCoordinatesIndex].nDestWidth = dw[m_nCoordinatesIndex * STRETCHCOORDINATEENTRYCOUNT + 2];
                        m_sCoordinates[m_nCoordinatesIndex].nDestHeight = dw[m_nCoordinatesIndex * STRETCHCOORDINATEENTRYCOUNT + 3];
                        m_sCoordinates[m_nCoordinatesIndex].nSrcLeft = dw[m_nCoordinatesIndex * STRETCHCOORDINATEENTRYCOUNT + 4];
                        m_sCoordinates[m_nCoordinatesIndex].nSrcTop = dw[m_nCoordinatesIndex * STRETCHCOORDINATEENTRYCOUNT + 5];
                        m_sCoordinates[m_nCoordinatesIndex].nSrcWidth = dw[m_nCoordinatesIndex * STRETCHCOORDINATEENTRYCOUNT + 6];
                        m_sCoordinates[m_nCoordinatesIndex].nSrcHeight = dw[m_nCoordinatesIndex * STRETCHCOORDINATEENTRYCOUNT + 7];

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
            m_sCoordinates = new(StretchCoordinates[m_nMaxCoordinatesIndex]);
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
                    m_sCoordinates[m_nCoordinatesIndex].nDestLeft= 0;
                    m_sCoordinates[m_nCoordinatesIndex].nDestTop= 0;
                    m_sCoordinates[m_nCoordinatesIndex].nSrcLeft = 0;
                    m_sCoordinates[m_nCoordinatesIndex].nSrcTop = 0;
                    m_sCoordinates[m_nCoordinatesIndex].nDestHeight= nHeightToUse;
                    m_sCoordinates[m_nCoordinatesIndex].nDestWidth = nWidthToUse;
                    m_sCoordinates[m_nCoordinatesIndex].nSrcHeight= nHeightToUse;
                    m_sCoordinates[m_nCoordinatesIndex].nSrcWidth = nWidthToUse;

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
CStretchCoordinates::PreRun(TestInfo *tiRunInfo)
{
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].nDestWidth));
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].nDestHeight));
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].nDestWidth * m_sCoordinates[m_nCoordinatesIndex].nDestHeight));
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].nSrcWidth));
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].nSrcHeight));
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].nSrcWidth * m_sCoordinates[m_nCoordinatesIndex].nSrcHeight));

    return TRUE;
}

BOOL
CStretchCoordinates::PostRun()
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
CStretchCoordinates::Cleanup()
{
    delete [] m_sCoordinates;
    return TRUE;
}

struct StretchCoordinates
CStretchCoordinates::GetCoordinates()
{
    return m_sCoordinates[m_nCoordinatesIndex];
}

