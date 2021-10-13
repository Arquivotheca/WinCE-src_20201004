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
#include "skiptable.h"
#include "global.h"

cSkipTable::~cSkipTable()
{
    if(m_pnSkipTable)
        delete[] m_pnSkipTable;
    m_nSkipTableEntryCount = 0;

    if(m_tszSkipString != EMPTYSKIPTABLE)
        delete[] m_tszSkipString;
}

bool
cSkipTable::IsInSkipTable(int nSkipEntry)
{
    for(int i =0; i < m_nSkipTableEntryCount; i++)
        if(nSkipEntry == m_pnSkipTable[i])
            return true;

    return false;
}

int
FillSkipTable(TCHAR * tszSkipString, int *nEntries, int nCount)
{
    TCHAR seps[] = TEXT(", ");
    TCHAR tszFormatString2[] =  TEXT("%x-%x");
    TCHAR tszFormatString1[] =  TEXT("%x");
    TCHAR *token;
    TCHAR *next_token;
    int nTempNum1, nTempNum2;
    int nEntryCount = 0;
    int nEntriesAssigned;

    token = _tcstok_s(tszSkipString, seps, &next_token);

    while(token)
    {
        nTempNum1 = nTempNum2 = 0;

        if(EOF != (nEntriesAssigned =  _stscanf_s(token, tszFormatString2, &nTempNum1, &nTempNum2)))
        {
            if(nEntriesAssigned > 0)
            {
                // if only 1 entry was set, then we only have one entry.
                if(nEntriesAssigned == 1)
                    nTempNum2 = nTempNum1;

                for(int i = nTempNum1; i <= nTempNum2; i++)
                {
                    if(nEntries && nEntryCount < nCount)
                    {
                        nEntries[nEntryCount] = i;
                    }
                    nEntryCount++;
                }
            }
        }

        // go to the next token.
        token = _tcstok_s(NULL, seps, &next_token);
    }

    if(nEntries && nEntryCount > nCount)
        info(ABORT, TEXT("SkipTableEntry counted %d is greater than array passed in %d, data entry truncated."), nEntryCount, nCount);

    return nEntryCount;
}

bool
cSkipTable::InitializeSkipTable(const TCHAR * tszSkipString)
{
    bool bRval = true;
    TCHAR *tszSkipStringBackup = NULL;
    SIZE_T StringLength;

    // if this was previously called and the skip string was changed,
    // then free the string so we don't leak memory.
    if(m_tszSkipString != EMPTYSKIPTABLE)
    {
        delete[] m_tszSkipString;
        m_tszSkipString = EMPTYSKIPTABLE;
    }

    if(tszSkipString)
    {
        StringLength = _tcslen(tszSkipString);

        // if the length is 0, then there's nothing to do.
        if(StringLength > 0)
        {
            // cleanup the previous skip table.
            if(m_pnSkipTable)
                delete[] m_pnSkipTable;
            m_nSkipTableEntryCount = 0;

            // backup the skip string, since strtok changes it.
            tszSkipStringBackup = new TCHAR[StringLength + 1];

            if(tszSkipStringBackup)
            {
                memset(tszSkipStringBackup, 0, StringLength * sizeof(TCHAR));
                // count the number of entries so we know how big to allocate the skip table
                // array to be.
                _tcsncpy_s(tszSkipStringBackup, StringLength + 1, tszSkipString, _TRUNCATE);

                m_nSkipTableEntryCount = FillSkipTable(tszSkipStringBackup, NULL, 0);
                // restore the tszSkipStringBackup because FillSkipTable modified it.
                _tcsncpy_s(tszSkipStringBackup, StringLength + 1, tszSkipString, _TRUNCATE);

                // if we have a number of entries, allocate the array and fill it.
                if(m_nSkipTableEntryCount > 0)
                {
                    m_pnSkipTable = new int[m_nSkipTableEntryCount];
                    if(m_pnSkipTable)
                    {
                        if(m_nSkipTableEntryCount != FillSkipTable(tszSkipStringBackup, m_pnSkipTable, m_nSkipTableEntryCount))
                        {
                            info(ABORT, TEXT("Error filling the skip table array"));
                            bRval = false;
                        }
                    }
                    else
                    {
                        info(ABORT, TEXT("Failed to allocate skip table array"));
                        bRval = false;
                    }
                }
                // an empty skip table just means that there's nothing in it, which is acceptable.

                // restore the tszSkipStringBackup because FillSkipTable modified it.
                _tcsncpy_s(tszSkipStringBackup, StringLength + 1, tszSkipString, _TRUNCATE);

                // grab the pointer to the string allocated
                m_tszSkipString = tszSkipStringBackup;
            }
            else
            {
                info(ABORT, TEXT("Failed to allocate string backup array to initialize the skip table with."));
                bRval = false;
            }
        }
    }
    else
    {
        info(ABORT, TEXT("No skiptable string given."));
        bRval = false;
    }

    return bRval;
}

bool
cSkipTable::OutputSkipTable()
{
    info(DETAIL, TEXT("Skip table returned"));
    if(m_pnSkipTable)
    {
        for(int i =0; i < m_nSkipTableEntryCount; i++)
            info(DETAIL, TEXT("0x%x"), m_pnSkipTable[i]);
    }
    else return false;

    return true;
}

TCHAR *
cSkipTable::SkipString()
{
    return m_tszSkipString;
}

