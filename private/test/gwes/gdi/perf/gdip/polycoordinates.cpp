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
#include "polycoordinates.h"

BOOL
CPolyCoordinates::Initialize(TestSuiteInfo *tsi)
{
    BOOL bRval = TRUE;
    int nCount = 0;

    tsi->tsFieldDescription.push_back(TEXT("Coordinate Count"));

    // find out how many sets of coordinate arrays there are.
    for(m_nMaxCoordinatesIndex=0; ;m_nMaxCoordinatesIndex++)
        if(0 == m_SectionList->GetDWArray(TEXT("Coordinates") + itos(m_nMaxCoordinatesIndex), 10, NULL, 0))
            break;

    if(m_nMaxCoordinatesIndex > 0)
    {
        m_sCoordinates = new(POINT *[m_nMaxCoordinatesIndex]);
        m_nCoordinatesCount = new(int[m_nMaxCoordinatesIndex]);

        // if the coordinates entry is defined, then use it, if it isn't then try the next
        // coordinate entry method.
        for(m_nCoordinatesIndex = 0; m_nCoordinatesIndex < m_nMaxCoordinatesIndex; m_nCoordinatesIndex++)
        {
            nCount = m_SectionList->GetDWArray(TEXT("Coordinates") + itos(m_nCoordinatesIndex), 10, NULL, 0);

            // the number of entries MUST be divisible by 6, we're expecting there to be
            // sets of six entries for the coordinates top, left, width, height, source top, and source left.
            if(nCount % POLYCOORDINATEENTRYCOUNT == 0)
            {
                // allocate a buffer to use to retrieve the coordinates from the script file
                // since we can't put it directly in the structure.
                DWORD *dw;
                int nSubCoordinatesIndex;
                int nCountConverted;
                dw = new(DWORD[nCount]);
                if(dw)
                {
                    // retrieve the actual list of coordinates from the script file.
                    nCountConverted = m_SectionList->GetDWArray(TEXT("Coordinates") + itos(m_nCoordinatesIndex), 10, dw, nCount);

                    if(nCountConverted == nCount)
                    {
                        // get the number of points in this set.
                        m_nCoordinatesCount[m_nCoordinatesIndex] = (int)(nCount / POLYCOORDINATEENTRYCOUNT);
                        // allocate the array of points
                        m_sCoordinates[m_nCoordinatesIndex] = new(POINT[m_nCoordinatesCount[m_nCoordinatesIndex]]);

                        for(nSubCoordinatesIndex = 0; nSubCoordinatesIndex < m_nCoordinatesCount[m_nCoordinatesIndex]; nSubCoordinatesIndex++)
                        {
                            // the pattern is (dest left, top, width, height, source left, top), just like the blitting prototypes.
                            m_sCoordinates[m_nCoordinatesIndex][nSubCoordinatesIndex].x= dw[nSubCoordinatesIndex * POLYCOORDINATEENTRYCOUNT];
                            m_sCoordinates[m_nCoordinatesIndex][nSubCoordinatesIndex].y= dw[nSubCoordinatesIndex * POLYCOORDINATEENTRYCOUNT + 1];
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
    }
    else
    {
        m_nMaxCoordinatesIndex = 1;

        // the base array of pointers to arrays, and the array of entry counts
        m_sCoordinates = new(POINT *[m_nMaxCoordinatesIndex]);
        m_nCoordinatesCount = new(int[m_nMaxCoordinatesIndex]);

        if(m_sCoordinates && m_nCoordinatesCount)
        {
            m_sCoordinates[0] = new(POINT[8]);
            if(m_sCoordinates[0])
            {
                m_nCoordinatesCount[0] = 5;

                // default to drawing a rectangle.

                // draw the top
                m_sCoordinates[0][0].x = 20;
                m_sCoordinates[0][0].y = 20;
                m_sCoordinates[0][1].x = 120;
                m_sCoordinates[0][1].y = 20;

                // draw the right
                m_sCoordinates[0][2].x = 120;
                m_sCoordinates[0][2].y = 120;

                // draw the bottom
                m_sCoordinates[0][3].x = 20;
                m_sCoordinates[0][3].y = 120;

                // finish off with the left.
                m_sCoordinates[0][4].x = 20;
                m_sCoordinates[0][4].y = 20;
            }
            else
            {
                info(FAIL, TEXT("Default point allocation failed."));
                bRval = FALSE;
            }

        }
        else
        {
            info(FAIL, TEXT("Default coordinate allocation failed."));
            bRval = FALSE;
        }

    }

    info(ECHO, TEXT("%d coordinate sets in use"), m_nMaxCoordinatesIndex);

    if(m_nCoordinatesCount)
    {
        for(m_nCoordinatesIndex = 0; m_nCoordinatesIndex < m_nMaxCoordinatesIndex; m_nCoordinatesIndex++)
              info(ECHO, TEXT("Coordinate set %d has %d entries."), m_nCoordinatesIndex, m_nCoordinatesCount[m_nCoordinatesIndex]);
    }
    // reset the current coordinate index to the start of the array.
    m_nCoordinatesIndex = 0;
    return bRval;
}

BOOL
CPolyCoordinates::PreRun(TestInfo *tiRunInfo)
{
    tiRunInfo->Descriptions.push_back(itos(m_nCoordinatesCount[m_nCoordinatesIndex]));

    return TRUE;
}

BOOL
CPolyCoordinates::PostRun()
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
CPolyCoordinates::Cleanup()
{
    for(m_nCoordinatesIndex = 0; m_nCoordinatesIndex < m_nMaxCoordinatesIndex; m_nCoordinatesIndex++)
        delete[] m_sCoordinates[m_nCoordinatesIndex];

    if(m_sCoordinates)
        delete [] m_sCoordinates;

    if(m_nCoordinatesCount)
        delete [] m_nCoordinatesCount;

    return TRUE;
}

POINT *
CPolyCoordinates::GetCoordinates()
{
    return m_sCoordinates[m_nCoordinatesIndex];
}

int
CPolyCoordinates::GetCoordinatesCount()
{
    return m_nCoordinatesCount[m_nCoordinatesIndex];
}
