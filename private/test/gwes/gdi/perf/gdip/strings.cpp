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
#include "strings.h"

BOOL
CString::Initialize(TestSuiteInfo *tsi)
{
    BOOL bRval = TRUE;
    int nCount;
    int nMaxIndex;
    TCHAR *tcPointer = NULL;

    tsi->tsFieldDescription.push_back(TEXT("StringIndex"));
    tsi->tsFieldDescription.push_back(TEXT("StringLength"));

    m_nMaxStringIndex = 0;
    m_nStringIndex = 0;
    // count the number of strings in the script
    while(m_SectionList->GetString(TEXT("String") + itos(m_nStringIndex), NULL))
    {
        m_nStringIndex++;
        m_nMaxStringIndex++;
    }

    // count the number of unicode strings in the script
    m_nStringIndex = 0;
    while(m_SectionList->GetDWArray(TEXT("Unicode") + itos(m_nStringIndex), 10, NULL, 0))
    {
        m_nStringIndex++;
        m_nMaxStringIndex++;
    }

    if(m_nMaxStringIndex != 0)
    {
        // allocate an array of tstrings
        m_tsString = new(TSTRING[m_nMaxStringIndex]);

        if(m_tsString)
        {
            // retrive the string's
            m_nStringIndex = 0;
            while(m_nStringIndex < m_nMaxStringIndex)
            {
                if(!m_SectionList->GetString(TEXT("String") + itos(m_nStringIndex), &m_tsString[m_nStringIndex]))
                    break;

                m_nStringIndex++;
            }

            nCount = 0;
            // retrieve the rest of the strings and copy them to the string array
            while(m_nStringIndex < m_nMaxStringIndex)
            {
                // -1 isn't a valid character, add the string to the list if the first character isn't the default
                // character.
                if(!AllocTCArrayFromDWArray(TEXT("Unicode") + itos(nCount), &tcPointer, &nMaxIndex, m_SectionList, 16))
                    break;

				if(tcPointer)
					m_tsString[m_nStringIndex] = tcPointer;
                m_nStringIndex++;
            }

            if(m_nStringIndex != m_nMaxStringIndex)
            {
                info(FAIL, TEXT("Incorrect number of strings retrieved, expected %d, retrieved %d."), m_nMaxStringIndex, m_nStringIndex);
                bRval = FALSE;
            }
        }
        else
        {
            info(FAIL, TEXT("String allocation failed. %d"), GetLastError());
            bRval = FALSE;
        }
    }
    else
    {
        m_nMaxStringIndex = 1;
        m_tsString = new(TSTRING[m_nMaxStringIndex]);
        // BUGBUG: DO SOMETHING SMARTER HERE.
        m_tsString[0] = TEXT("TempTestString");
    }

    info(ECHO, TEXT("%d strings in use."), m_nMaxStringIndex);

    m_nStringIndex=0;

    return bRval;
}

BOOL
CString::PreRun(TestInfo *tiRunInfo)
{
    tiRunInfo->Descriptions.push_back(itos(m_nStringIndex));
    tiRunInfo->Descriptions.push_back(itos(_tcslen(m_tsString[m_nStringIndex].c_str())));

    return TRUE;
}

BOOL
CString::PostRun()
{
    BOOL bRVal = FALSE;

    m_nStringIndex++;

    if(m_nStringIndex >= m_nMaxStringIndex)
    {
        m_nStringIndex = 0;
        bRVal = TRUE;
    }

    return bRVal;
}

BOOL
CString::Cleanup()
{

    delete[] m_tsString;

    return TRUE;
}

TCHAR *
CString::GetString()
{
    return (TCHAR *) m_tsString[m_nStringIndex].c_str();
}
