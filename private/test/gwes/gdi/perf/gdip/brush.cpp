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
#include "brush.h"

BOOL
CBrush::Initialize(TestSuiteInfo *tsi)
{
    BOOL bRval = TRUE;

    tsi->tsFieldDescription.push_back(TEXT("Brush"));

    // retrieve number of brushes given in the script
    if(m_nMaxBrushIndex = m_SectionList->GetStringArray(TEXT("Brush"), NULL, 0))
    {
        int nActualBrushIndexReturned;

        m_tsBrushNameArray = new(TSTRING[m_nMaxBrushIndex]);
        m_hBrush = new(HBRUSH[m_nMaxBrushIndex]);

        // fill it in with the actual strings
        if(m_tsBrushNameArray)
        {
            nActualBrushIndexReturned = m_SectionList->GetStringArray(TEXT("Brush"), m_tsBrushNameArray, m_nMaxBrushIndex);
            if(nActualBrushIndexReturned != m_nMaxBrushIndex)
            {
                info(FAIL, TEXT("Original string count returned %d doesn't match actual string count %d."), m_nMaxBrushIndex, nActualBrushIndexReturned);
                bRval = FALSE;
            }
        }
    }
    else // no brushes in the script, use the default
    {
        m_nMaxBrushIndex = 1;
        m_tsBrushNameArray = new(TSTRING[m_nMaxBrushIndex]);
        m_hBrush = new(HBRUSH[m_nMaxBrushIndex]);
        // set the default
        if(m_tsBrushNameArray)
            m_tsBrushNameArray[0] = TEXT("NULL");
    }

    // if the array's allocated, fill them in.
    if(m_tsBrushNameArray && m_hBrush)
    {
        // Initialize Brush Array
        for(m_nBrushIndex = 0; m_nBrushIndex < m_nMaxBrushIndex; m_nBrushIndex++)
        {
            if(NULL == (m_hBrush[m_nBrushIndex] = myCreateBrush(m_tsBrushNameArray[m_nBrushIndex])))
            {
                info(FAIL, TEXT("Failed to create selected brush %s"), m_tsBrushNameArray[m_nBrushIndex].c_str());
                bRval = FALSE;
            }
        }
    }
    else
    {
        info(FAIL, TEXT("Brush array/name array allocation failed."));
        bRval = FALSE;
    }

    m_nBrushIndex = 0;

    info(ECHO, TEXT("%d brushes in use."), m_nMaxBrushIndex);

    return bRval;
}

BOOL
CBrush::PreRun(TestInfo *tiRunInfo, HDC hdc)
{
    BOOL bRval = TRUE;

    // if the pointers are invalid, fail the call.
    if(m_hBrush && m_tsBrushNameArray)
    {
        tiRunInfo->Descriptions.push_back(m_tsBrushNameArray[m_nBrushIndex]);
        m_StockBrush = (HBRUSH) SelectObject(hdc, m_hBrush[m_nBrushIndex]);
    }
    else bRval = FALSE;

    return bRval;
}

// PostRun increments 
BOOL
CBrush::PostRun(HDC hdc)
{
    BOOL bRVal = FALSE;

    SelectObject(hdc, m_StockBrush);

    m_nBrushIndex++;

    if(m_nBrushIndex >= m_nMaxBrushIndex)
    {
        m_nBrushIndex = 0;
        bRVal = TRUE;
    }

    return bRVal;
}

BOOL
CBrush::Cleanup()
{

    if(m_hBrush)
        for(m_nBrushIndex = 0; m_nBrushIndex < m_nMaxBrushIndex; m_nBrushIndex++)
        {
            DeleteObject(m_hBrush[m_nBrushIndex]);
        }
    delete[] m_tsBrushNameArray;
    delete[] m_hBrush;

    return TRUE;
}

HBRUSH
CBrush::GetBrush()
{
    return m_hBrush[m_nBrushIndex];
}
