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
#include "pen.h"

BOOL
CPen::Initialize(TestSuiteInfo *tsi)
{
    BOOL bRval = TRUE;

    tsi->tsFieldDescription.push_back(TEXT("Pen"));

    bRval &= AllocTSArray(TEXT("Pen"), TEXT("SOLID"), &m_tsPenNameArray, &m_nMaxPenIndex, m_SectionList);

    // allocate the array for pens.
    m_hPen = new(HPEN[m_nMaxPenIndex]);

    if(m_hPen && m_tsPenNameArray)
    {
        // Initialize Pen Array
        for(m_nPenIndex = 0; m_nPenIndex < m_nMaxPenIndex; m_nPenIndex++)
        {
            if(NULL == (m_hPen[m_nPenIndex] = myCreatePen(m_tsPenNameArray[m_nPenIndex])))
            {
                info(FAIL, TEXT("Failed to create selected Pen %s"), m_tsPenNameArray[m_nPenIndex].c_str());
                bRval = FALSE;
            }
        }
    }
    else
    {
        info(FAIL, TEXT("Pen array/name array allocation failed."));
        bRval = FALSE;
    }

    m_nPenIndex = 0;

    info(ECHO, TEXT("%d Pens in use."), m_nMaxPenIndex);

    bRval &= m_BkMode.Initialize(tsi);

    return bRval;
}

BOOL
CPen::PreRun(TestInfo *tiRunInfo, HDC hdc)
{
    tiRunInfo->Descriptions.push_back(m_tsPenNameArray[m_nPenIndex]);

    m_StockPen = (HPEN) SelectObject(hdc, m_hPen[m_nPenIndex]);

    m_BkMode.PreRun(tiRunInfo, hdc);

    return TRUE;
}

BOOL
CPen::PostRun(HDC hdc)
{
    BOOL bRVal = FALSE;

    SelectObject(hdc, m_StockPen);

    m_nPenIndex++;

    if(m_nPenIndex >= m_nMaxPenIndex)
    {
        m_nPenIndex = 0;
        if(m_BkMode.PostRun())
            bRVal = TRUE;
    }

    return bRVal;
}

BOOL
CPen::Cleanup()
{

    if(m_hPen)
        for(m_nPenIndex = 0; m_nPenIndex < m_nMaxPenIndex; m_nPenIndex++)
        {
            DeleteObject(m_hPen[m_nPenIndex]);
        }
    delete[] m_tsPenNameArray;
    delete[] m_hPen;

    m_BkMode.Cleanup();

    return TRUE;
}

