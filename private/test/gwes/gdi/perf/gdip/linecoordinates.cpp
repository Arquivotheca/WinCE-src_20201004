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
#include "linecoordinates.h"

BOOL
CLineCoordinates::Initialize(TestSuiteInfo *tsi)
{
    BOOL bRval = TRUE;
    int nCount = 0;

    tsi->tsFieldDescription.push_back(TEXT("Length"));
    tsi->tsFieldDescription.push_back(TEXT("Angle"));

    // if the coordinates entry is defined, then use it, if it isn't then try the next
    // coordinate entry method.
    if(nCount = m_SectionList->GetDWArray(TEXT("Coordinates"), 10, NULL, 0))
    {
        // the number of entries MUST be divisible by 6, we're expecting there to be
        // sets of six entries for the coordinates top, left, width, height, source top, and source left.
        if(nCount % LINECOORDINATEENTRYCOUNT == 0)
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
                    m_nMaxCoordinatesIndex = (int)(nCount / LINECOORDINATEENTRYCOUNT);
                    m_sCoordinates = new(LineCoordinates[m_nMaxCoordinatesIndex]);

                    for(m_nCoordinatesIndex = 0; m_nCoordinatesIndex < m_nMaxCoordinatesIndex; m_nCoordinatesIndex++)
                    {
                        // the pattern is (dest left, top, width, height, source left, top), just like the blitting prototypes.
                        m_sCoordinates[m_nCoordinatesIndex].nStart.x= dw[m_nCoordinatesIndex * LINECOORDINATEENTRYCOUNT];
                        m_sCoordinates[m_nCoordinatesIndex].nStart.y= dw[m_nCoordinatesIndex * LINECOORDINATEENTRYCOUNT + 1];
                        m_sCoordinates[m_nCoordinatesIndex].nEnd.x= dw[m_nCoordinatesIndex * LINECOORDINATEENTRYCOUNT + 2];
                        m_sCoordinates[m_nCoordinatesIndex].nEnd.y= dw[m_nCoordinatesIndex * LINECOORDINATEENTRYCOUNT + 3];
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
    else
    {
        m_nMaxCoordinatesIndex = 8;
        m_sCoordinates = new(LineCoordinates[8]);

        if(m_sCoordinates)
        {
            // default test, horizontal, vertical, and 45% diagonal lines around a square.
            // left center to right center
            m_sCoordinates[0].nStart.x= 20;
            m_sCoordinates[0].nStart.y= 70;
            m_sCoordinates[0].nEnd.x= 120;
            m_sCoordinates[0].nEnd.y= 70;

            // left bottom to right top
            m_sCoordinates[1].nStart.x= 20;
            m_sCoordinates[1].nStart.y= 120;
            m_sCoordinates[1].nEnd.x= 120;
            m_sCoordinates[1].nEnd.y= 20;

            // bottom center to top center
            m_sCoordinates[2].nStart.x= 70;
            m_sCoordinates[2].nStart.y= 120;
            m_sCoordinates[2].nEnd.x= 70;
            m_sCoordinates[2].nEnd.y= 20;

            // right bottom to left top
            m_sCoordinates[3].nStart.x= 120;
            m_sCoordinates[3].nStart.y= 120;
            m_sCoordinates[3].nEnd.x= 20;
            m_sCoordinates[3].nEnd.y= 20;
     
            // right center to left center
            m_sCoordinates[4].nStart.x= 120;
            m_sCoordinates[4].nStart.y= 70;
            m_sCoordinates[4].nEnd.x= 20;
            m_sCoordinates[4].nEnd.y= 70;

            // right top to bottom left
            m_sCoordinates[5].nStart.x= 120;
            m_sCoordinates[5].nStart.y= 20;
            m_sCoordinates[5].nEnd.x= 20;
            m_sCoordinates[5].nEnd.y= 120;

            // top center to bottom center
            m_sCoordinates[6].nStart.x= 70;
            m_sCoordinates[6].nStart.y= 20;
            m_sCoordinates[6].nEnd.x= 70;
            m_sCoordinates[6].nEnd.y= 120;

            // left top to bottom right
            m_sCoordinates[7].nStart.x= 20;
            m_sCoordinates[7].nStart.y= 20;
            m_sCoordinates[7].nEnd.x= 120;
            m_sCoordinates[7].nEnd.y= 120;
        }
        else
        {
            info(FAIL, TEXT("Default coordinate allocation failed."));
            bRval = FALSE;
        }
    }

    info(ECHO, TEXT("%d lines in use"), m_nMaxCoordinatesIndex);

    // reset the current coordinate index to the start of the array.
    m_nCoordinatesIndex = 0;
    return bRval;
}

BOOL
CLineCoordinates::PreRun(TestInfo *tiRunInfo)
{
    double dwLengthX, dwLengthY, dwLength, dwAngle;
    double pi = 3.1415927;

    // x direction is the same as the normal coordinate space.
    dwLengthX = m_sCoordinates[m_nCoordinatesIndex].nEnd.x - m_sCoordinates[m_nCoordinatesIndex].nStart.x;
    // y direction is opposite of normal coordinate space, smaller is higher.
    dwLengthY = m_sCoordinates[m_nCoordinatesIndex].nStart.y - m_sCoordinates[m_nCoordinatesIndex].nEnd.y;

    dwLength = sqrt(pow(dwLengthX, 2) + pow(dwLengthY, 2));

    if(dwLengthX != 0)
    {
        // radians * 180/pi = degrees
        dwAngle = atan(abs(dwLengthY)/abs(dwLengthX));

        if(dwLengthX < 0 && dwLengthY > 0)
            dwAngle += pi/2;
        else if(dwLengthY <= 0 && dwLengthX < 0)
            dwAngle += pi;
        else if(dwLengthY < 0 && dwLengthX >= 0)
            dwAngle += 3*pi/2;

        dwAngle *= (180/pi);
    }
    // result will divide by 0, and the angle will be 90 or 270 anyway.
    else
    {
        if(dwLengthY > 0)
            dwAngle = 90;
        else dwAngle = 270;
    }
    // length = sqrt((x1 - x2)^2 + (y1 - y2)^2)
    tiRunInfo->Descriptions.push_back(dtos(dwLength));

    // angle = tan(y/x)
    tiRunInfo->Descriptions.push_back(dtos(dwAngle));

    return TRUE;
}

BOOL
CLineCoordinates::PostRun()
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
CLineCoordinates::Cleanup()
{
    delete [] m_sCoordinates;
    return TRUE;
}

BOOL
CLineCoordinates::GetCoordinates(POINT *Start, POINT *End)
{
    *Start = m_sCoordinates[m_nCoordinatesIndex].nStart;
    *End = m_sCoordinates[m_nCoordinatesIndex].nEnd;
    return TRUE;
}

