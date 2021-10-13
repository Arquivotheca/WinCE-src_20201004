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
#include "rop3.h"

DWORD dwAllPatternDestRops[] =
                                                {
                                                    0x00000000,
                                                    0x00050000,
                                                    0x000A0000,
                                                    0x000F0000,
                                                    0x00500000,
                                                    0x00550000,
                                                    0x005A0000,
                                                    0x005F0000,
                                                    0x00A00000,
                                                    0x00A50000,
                                                    0x00AA0000,
                                                    0x00AF0000,
                                                    0x00F00000,
                                                    0x00F50000,
                                                    0x00FA0000,
                                                    0x00FF0000
                                                };

BOOL
CRop3::Initialize(TestSuiteInfo *tsi, BOOL bPatternDestRopsOnly)
{
    BOOL bRval = TRUE;
    TSTRING string;

    tsi->tsFieldDescription.push_back(TEXT("ROP"));

    // retrieve list of rop's, if no list exists default to SRCCOPY, rop's are base 16
    // when entered into the list.
    if(m_SectionList->GetString(TEXT("ROP"), &string) && string == TEXT("All"))
    {
        // when testing with PatBlt, you only want to deal with a subset of the rop3's.
        if(bPatternDestRopsOnly)
        {
            m_nMaxRopIndex=16;
            m_dwRop = new(DWORD[m_nMaxRopIndex]);

            if(m_dwRop)
            {
                for(int i = 0; i < m_nMaxRopIndex; i++)
                    m_dwRop[i] = dwAllPatternDestRops[i];
            }

        }
        else
        {
            m_nMaxRopIndex=256;
            m_dwRop = new(DWORD[m_nMaxRopIndex]);

            if(m_dwRop)
            {
                for(int i = 0; i < m_nMaxRopIndex; i++)
                    m_dwRop[i] = i << 16;
            }
        }
    }
    else
        bRval &= AllocDWArray(TEXT("ROP"), SRCCOPY, &m_dwRop, &m_nMaxRopIndex, m_SectionList, 16);

    m_nRopIndex = 0;

    if(NULL == m_dwRop)
    {
        info(FAIL, TEXT("ROP allocation failed."));
        bRval = FALSE;
    }

    info(ECHO, TEXT("%d rop's in use"), m_nMaxRopIndex);

    return bRval;
}

BOOL
CRop3::PreRun(TestInfo *tiRunInfo)
{
    BOOL bRval = TRUE;

    if(m_dwRop)
        tiRunInfo->Descriptions.push_back(itohs(m_dwRop[m_nRopIndex]));
    else bRval = FALSE;

    return bRval;
}

BOOL
CRop3::PostRun()
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
CRop3::Cleanup()
{
    // free the ROP's
    delete[] m_dwRop;

    return TRUE;
}

DWORD
CRop3::GetRop()
{
    if(m_dwRop)
        return m_dwRop[m_nRopIndex];
    else return -1;
}
