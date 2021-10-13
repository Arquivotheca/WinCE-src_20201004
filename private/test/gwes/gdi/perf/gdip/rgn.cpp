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
#include "rgn.h"

BOOL
CRgn::Initialize(TestSuiteInfo *tsi)
{
    BOOL bRval = TRUE;
    int nCount = 0;
    TSTRING tsTmp;

    tsi->tsFieldDescription.push_back(TEXT("Rgn Count"));

    // if the coordinates entry is defined, then use it, if it isn't then try the next
    // coordinate entry method.
    if(nCount = m_SectionList->GetDWArray(TEXT("RgnRects"), 10, NULL, 0))
    {
        if(nCount % RGNRECTCOUNT == 0)
        {
            // allocate a buffer to use to retrieve the coordinates from the script file
            // since we can't put it directly in the StretchCoordinates structure.
            DWORD *dw;
            int nCountConverted;
            dw = new(DWORD[nCount]);
            if(dw)
            {
                // retrieve the actual list of coordinates from the script file.
                nCountConverted = m_SectionList->GetDWArray(TEXT("RgnRects"), 10, dw, nCount);

                if(nCountConverted == nCount)
                {
                    m_nMaxRgnRects = (int)(nCount / RGNRECTCOUNT);
                    m_sRgnRects = (MYRGNDATA *) new(byte[m_nMaxRgnRects * sizeof(RECT) + sizeof(RGNDATA)]);

                    for(m_nCurrentRgnRectCount = 0; m_nCurrentRgnRectCount < m_nMaxRgnRects; m_nCurrentRgnRectCount++)
                    {
                        // the pattern is (dest left, top, width, height, source left, top), just like the blitting prototypes.
                        m_sRgnRects->Buffer[m_nCurrentRgnRectCount].left= dw[m_nCurrentRgnRectCount * RGNRECTCOUNT];
                        m_sRgnRects->Buffer[m_nCurrentRgnRectCount].top= dw[m_nCurrentRgnRectCount * RGNRECTCOUNT + 1];
                        m_sRgnRects->Buffer[m_nCurrentRgnRectCount].right= dw[m_nCurrentRgnRectCount * RGNRECTCOUNT + 2];
                        m_sRgnRects->Buffer[m_nCurrentRgnRectCount].bottom = dw[m_nCurrentRgnRectCount * RGNRECTCOUNT + 3];
                    }
                    delete[] dw;
                }
                else
                {
                    info(FAIL, TEXT("%d rectangle entries in the file, however only %d are valid!"), nCount, nCountConverted);
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
            info(FAIL, TEXT("Incorrect number of coordinates given for rectangles, %d."), nCount);
            bRval = FALSE;
        }

        // use these values to step through the numbers of regions.
        if(!m_SectionList->GetDWord(TEXT("MinNumberOfRgns"), 10, &m_dwMinRgnCount))
            m_dwMinRgnCount = m_nMaxRgnRects;

        if(!m_SectionList->GetDWord(TEXT("MaxNumberOfRgns"), 10, &m_dwMaxRgnCount))
            m_dwMaxRgnCount = m_nMaxRgnRects;

        if(!m_SectionList->GetDWord(TEXT("RgnStep"), 10, &m_dwRgnStep))
            m_dwRgnStep = m_nMaxRgnRects;

        if(!m_SectionList->GetString(TEXT("CombineMode"), &tsTmp))
        {
            m_bCombineRgn = FALSE;
            m_nRgnCombineMode = 0;
        }
        else
        {
            m_bCombineRgn = TRUE;
            if(tsTmp == TEXT("RGN_AND"))
                m_nRgnCombineMode = RGN_AND;
            else if(tsTmp == TEXT("RGN_COPY"))
                m_nRgnCombineMode = RGN_COPY;
            else if(tsTmp == TEXT("RGN_DIFF"))
                m_nRgnCombineMode = RGN_DIFF;
            else if(tsTmp == TEXT("RGN_OR"))
                m_nRgnCombineMode = RGN_OR;
            else if(tsTmp == TEXT("RGN_XOR"))
                m_nRgnCombineMode = RGN_XOR;
            else
            {
                info(FAIL, TEXT("Invalid combine mode %s."), tsTmp.c_str());
                bRval = FALSE;
            }
        }
    }
    else
    {
        m_dwMinRgnCount = 0;
        m_dwMaxRgnCount = 0;
        m_nMaxRgnRects = 0;
        m_dwRgnStep = 0;
        m_sRgnRects = NULL;
    }

    info(ECHO, TEXT("%d rgn rects given, %d max rect to use, %d rects to start, %d rects for each step."), 
                                                            m_nMaxRgnRects, m_dwMaxRgnCount, m_dwMinRgnCount, m_dwRgnStep);

    // start at the minimum.
    m_nCurrentRgnRectCount = m_dwMinRgnCount;

    return bRval;
}

BOOL
CRgn::PreRun(TestInfo *tiRunInfo, HDC hdc)
{
    RECT rc;
    BOOL bRVal = TRUE;
    HRGN hrgnWhole = 0;
    DWORD nCount;

    if(ERROR == SelectClipRgn(hdc, NULL))
    {
        info(FAIL, TEXT("SelectClipRgn failed, %d."), GetLastError());
        bRVal = FALSE;
    }

    m_hRgn = NULL;

    // if no region coordinates are given, do nothing.
    // if the user doesn't want any regions in the group, do nothing.
    if(m_sRgnRects && m_nCurrentRgnRectCount > 0)
    {
        if(ERROR != GetClipBox(hdc, &rc))
        {
            if(NULL != (hrgnWhole = CreateRectRgnIndirect(&rc)))
            {
                tiRunInfo->Descriptions.push_back(itos(m_nCurrentRgnRectCount));

                if(m_nCurrentRgnRectCount > m_nMaxRgnRects)
                {
                    info(FAIL, TEXT("Current index is greater than the max index."));
                    return FALSE;
                }

                m_sRgnRects->rdh.dwSize = sizeof(RGNDATAHEADER);
                m_sRgnRects->rdh.iType = RDH_RECTANGLES;
                m_sRgnRects->rdh.nCount = m_nCurrentRgnRectCount;
                m_sRgnRects->rdh.nRgnSize = sizeof(RECT) * m_nCurrentRgnRectCount;
                m_sRgnRects->rdh.rcBound = rc;
                nCount = m_sRgnRects->rdh.dwSize + m_sRgnRects->rdh.nRgnSize;
                if(NULL == (m_hRgn = ExtCreateRegion(NULL, nCount, (RGNDATA *) m_sRgnRects)))
                {
                    info(FAIL, TEXT("ExtCreateRegion failed, %d."), GetLastError());
                    bRVal = FALSE;
                }

                if(m_bCombineRgn)
                {
                    if(ERROR == CombineRgn(m_hRgn, m_hRgn, hrgnWhole, m_nRgnCombineMode))
                    {
                        info(FAIL, TEXT("CombineRgn failed, %d."), GetLastError());
                        bRVal = FALSE;
                    }
                }

                if(ERROR == SelectClipRgn(hdc, m_hRgn))
                {
                    info(FAIL, TEXT("SelectClipRgn failed for the new region, %d."), GetLastError());
                    bRVal = FALSE;
                }

                bRVal &= DeleteObject(hrgnWhole);
            }
            else
            {
                info(FAIL, TEXT("CreateRectRgnIndirect failed, %d."), GetLastError());
                bRVal = FALSE;
            }
        }
        else
        {
            info(FAIL, TEXT("GetClipBox failed, %d."), GetLastError());
            bRVal = FALSE;
        }

    }
    else
        tiRunInfo->Descriptions.push_back(itos(0));

    return bRVal;
}

BOOL
CRgn::PostRun()
{
    BOOL bRVal = FALSE;

    // Delete the region.  this may fail if no region was created.
    if(m_hRgn)
        DeleteObject(m_hRgn);

    // if no region is given, do nothing.
    if(m_sRgnRects)
    {
        m_nCurrentRgnRectCount+=m_dwRgnStep;

        if(m_nCurrentRgnRectCount > (int) m_dwMaxRgnCount || m_nCurrentRgnRectCount > m_nMaxRgnRects)
        {
            m_nCurrentRgnRectCount = m_dwMinRgnCount;
            bRVal = TRUE;
        }
    }
    else bRVal = TRUE;

    return bRVal;
}

BOOL
CRgn::Cleanup()
{
    if(m_sRgnRects)
        delete [] m_sRgnRects;
    return TRUE;
}

HRGN
CRgn::GetRgn()
{
    return m_hRgn;
}

