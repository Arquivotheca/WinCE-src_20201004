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
#include "rop4.h"

BOOL
CRop4::Initialize(TestSuiteInfo *tsi)
{
    BOOL bRval = TRUE;

    tsi->tsFieldDescription.push_back(TEXT("ROP"));

    m_nMaxRopIndex = m_SectionList->GetDWArray(TEXT("ROP"), 16, NULL, 0);

    // retrieve list of rop's, if no list exists default to SRCCOPY, SRCCOPY, rop's are base 16
    // when entered into the list.  2 ROP's per entry for ROP4's.
    if(m_nMaxRopIndex > 0 && ((m_nMaxRopIndex % 2) == 0))
    {
        // allocate a temporary array to hold all of the ROP's, since the section parser
        // doesn't know how to make a ROP4.
        DWORD *dwRop;

        dwRop = new(DWORD[m_nMaxRopIndex]);

        // nMaxRopIndex%2 is 0, so it's evenly divisible by 2.
        m_nMaxRopIndex = m_nMaxRopIndex/2;

        // allocate the array for the ROP4's.
        m_dwRop = new(DWORD[m_nMaxRopIndex]);

        if(dwRop)
        {
            int nActualRopCountReturned;

            // retrieve the full dwRop array, which is 2x the number in m_dwRop
            nActualRopCountReturned = m_SectionList->GetDWArray(TEXT("ROP"), 16, dwRop, m_nMaxRopIndex * 2);

            if(nActualRopCountReturned == m_nMaxRopIndex*2)
            {
                if(m_dwRop)
                {
                    for(m_nRopIndex=0; m_nRopIndex < m_nMaxRopIndex; m_nRopIndex++)
                    {
                        // the largest index into the dwRop array must be less than the end.
                        m_dwRop[m_nRopIndex] = MAKEROP4(dwRop[m_nRopIndex*2], dwRop[m_nRopIndex*2 + 1]);
                    }
                }
            }
            else
            {
                info(FAIL, TEXT("Original DWORD count returned %d not equal to actual dword count %d.."), m_nMaxRopIndex*2, nActualRopCountReturned);
                bRval = FALSE;
            }
            
            delete[] dwRop;
        }
        else
        {
            info(FAIL, TEXT("Temporary ROP allocation failed."));
            bRval = FALSE;
        }
    }
    else if(0 == m_nMaxRopIndex)
    {
        m_nMaxRopIndex = 1;
        m_dwRop = new(DWORD[m_nMaxRopIndex]);
        if(m_dwRop)
            m_dwRop[0] = MAKEROP4(SRCCOPY, SRCCOPY);
    }
    m_nRopIndex = 0;

    if(NULL == m_dwRop)
    {
        info(FAIL, TEXT("ROP allocation failed."));
        bRval = FALSE;
    }
    else info(ECHO, TEXT("%d rop's in use"), m_nMaxRopIndex);

    return bRval;
}


BOOL
CRop4::PreRun(TestInfo *tiRunInfo)
{
    BOOL bRval = TRUE;

    if(m_dwRop)
        tiRunInfo->Descriptions.push_back(itohs(m_dwRop[m_nRopIndex]));
    else bRval = FALSE;

    return bRval;
}

BOOL
CRop4::PostRun()
{
    BOOL bRVal = FALSE;

    m_nRopIndex++;
    if(m_nRopIndex >= m_nMaxRopIndex)
    {
        m_nRopIndex = 0;
        bRVal = TRUE;
    }
    return bRVal;
}

BOOL
CRop4::Cleanup()
{
    // free the ROP's
    delete[] m_dwRop;

    return TRUE;
}

DWORD
CRop4::GetRop()
{
    if(m_dwRop)
        return m_dwRop[m_nRopIndex];
    else return -1;
}
