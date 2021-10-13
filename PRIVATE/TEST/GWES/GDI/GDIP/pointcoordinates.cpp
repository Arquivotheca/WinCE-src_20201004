//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "pointcoordinates.h"

BOOL
CPointCoordinates::Initialize(TestSuiteInfo *tsi, TSTRING tsEntryName)
{
    BOOL bRval = TRUE;
    int nCount = 0;

    tsi->tsFieldDescription.push_back(TEXT("x"));
    tsi->tsFieldDescription.push_back(TEXT("y"));

    // if the coordinates entry is defined, then use it, if it isn't then try the next
    // coordinate entry method.
    if(nCount = m_SectionList->GetDWArray(tsEntryName, 10, NULL, 0))
    {
        // the number of entries MUST be divisible by 6, we're expecting there to be
        // sets of six entries for the coordinates top, left, width, height, source top, and source left.
        if(nCount % POINTCOORDINATEENTRYCOUNT == 0)
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
                    m_nMaxCoordinatesIndex = (int)(nCount / POINTCOORDINATEENTRYCOUNT);
                    m_sCoordinates = new(POINT[m_nMaxCoordinatesIndex]);

                    for(m_nCoordinatesIndex = 0; m_nCoordinatesIndex < m_nMaxCoordinatesIndex; m_nCoordinatesIndex++)
                    {
                        // the pattern is (dest left, top, width, height, source left, top), just like the blitting prototypes.
                        m_sCoordinates[m_nCoordinatesIndex].x= dw[m_nCoordinatesIndex * POINTCOORDINATEENTRYCOUNT];
                        m_sCoordinates[m_nCoordinatesIndex].y= dw[m_nCoordinatesIndex * POINTCOORDINATEENTRYCOUNT + 1];
                    }
                    delete[] dw;
                }
                else
                {
                    g_pCOtakLog->Log(OTAK_ERROR, TEXT("%d coordinate entries in the file, however only %d are valid!"), nCount, nCountConverted);
                    bRval = FALSE;
                }
            }
            else
            {
                g_pCOtakLog->Log(OTAK_ERROR, TEXT("Temporary DWORD buffer allocation failed."));
                bRval = FALSE;
            }
        }
        else
        {
            g_pCOtakLog->Log(OTAK_ERROR, TEXT("Incorrect number of coordinates given."));
            bRval = FALSE;
        }
    }
    else
    {
        m_sCoordinates = new(POINT[1]);

        if(m_sCoordinates)
        {
            m_nMaxCoordinatesIndex = 1;
            m_sCoordinates[m_nCoordinatesIndex].x = 1;
            m_sCoordinates[m_nCoordinatesIndex].y = 1;
        }
        else
        {
            g_pCOtakLog->Log(OTAK_ERROR, TEXT("Default coordinate allocation failed."));
            bRval = FALSE;
        }
    }

    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d points in use"), m_nMaxCoordinatesIndex);

    // reset the current coordinate index to the start of the array.
    m_nCoordinatesIndex = 0;
    return bRval;
}

BOOL
CPointCoordinates::PreRun(TestInfo *tiRunInfo)
{
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].x));
    tiRunInfo->Descriptions.push_back(itos(m_sCoordinates[m_nCoordinatesIndex].y));

    return TRUE;
}

BOOL
CPointCoordinates::PostRun()
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
CPointCoordinates::Cleanup()
{
    delete [] m_sCoordinates;
    return TRUE;
}

POINT
CPointCoordinates::GetCoordinate()
{
    return m_sCoordinates[m_nCoordinatesIndex];
}

