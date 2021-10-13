//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "section.h"

BOOL
InitializeSectionList(TSTRING tszFileName, std::list<CSection *> *List)
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In InitializeSectionList"));

    FILE *pfFile = NULL;
    TSTRING tsPreviousEntry;
    TCHAR tszKeySeps[] = TEXT("=\n\\");
    TCHAR tszSectionSeps[] = TEXT("[]");
    TCHAR tszBuffer[MAXLINE];
    TCHAR * tszKey1, * tszKey2;
    BOOL bRVal = TRUE;
    BOOL bCurrentContinuation = FALSE, bNextContinuation = FALSE;

    CSection * tmp = NULL;

    // Try to open the config file for serial reading.
    if ((pfFile = _tfopen(tszFileName.c_str(), TEXT("r"))) == NULL)
    {
        g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to open the file %s"), tszFileName.c_str());
        // If we can't open the file, there's no use in continuing.
        bRVal = FALSE;
        goto Cleanup;
    }

    // now that we have the file open, read from it line by line,
    // parsing the values.
    while (_fgetts(tszBuffer, MAXLINE, pfFile))
    {
        int nIndexOfFirstChar;
        int ntszBufferLength;

        ntszBufferLength = _tcslen(tszBuffer);

        // get the continuation from the previous loop.
        // we can't wait to get this at the end becuase strtok changes the string we're testing.
        bCurrentContinuation = bNextContinuation;

        // should we treat this as a continuation on the next loop?
        if(tszBuffer[ntszBufferLength - 2] == '\\')
            bNextContinuation = TRUE;
        else
            bNextContinuation = FALSE;

        // clear off any leading white space
        for(nIndexOfFirstChar=0; tszBuffer[nIndexOfFirstChar] == ' ' || tszBuffer[nIndexOfFirstChar] == '\t'; nIndexOfFirstChar++);

        // Grab the first key.
        tszKey1 = _tcstok(&(tszBuffer[nIndexOfFirstChar]), tszKeySeps);
        
        // If the line is empty, or a comment continue to the next line.
        if (!tszKey1 || tszKey1[0] == '#')
            continue;

        // If the first character is a '[', this is a section header.
        if ('[' == tszKey1[0])
        {
            // We really don't care if the section header ends in ']', and this
            // bit of code completely does not worry about that.
            tszKey1 = _tcstok(tszKey1, tszSectionSeps);

            // save off the previous section.
            if(tmp)
                List->push_back(tmp);

            // create a section class for this header
            tmp = new(CSection);

            // if allocation failed, exit.
            if(!tmp)
            {
                g_pCOtakLog->Log(OTAK_ERROR, TEXT("Allocation for a CSection failed."));
                bRVal = FALSE;
                goto Cleanup;
            }

            // set the name.
            if(!tmp->SetName(tszKey1))
            {
                g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to set the name on the section allocated"));
                // failure to set the section name, so cleanup the allocation and exit.
                delete tmp;
                tmp = NULL;
                bRVal = FALSE;
                goto Cleanup;
            }
        }
        // only add the entries once we have an initial section header, ignore
        // anything before the section header.
        else if(tmp)
        {
            // put anything after the = into the map
            tszKey2 = _tcstok(NULL, tszKeySeps);
            if(tszKey2)
            {
                // store off the name of this entry.
                tsPreviousEntry = tszKey1;
                tmp->AddEntry(tszKey1, tszKey2);
            }
            // if the previous loop indicated a continuation, then add it.
            else if(bCurrentContinuation)
                tmp->CombineEntry(tsPreviousEntry.c_str(), tszKey1);
            else    
            {
                g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to find data after the = in the script file.  Line \"%s\""), tszBuffer);
                bRVal = FALSE;
                delete tmp;
                tmp = NULL;
                goto Cleanup;
            }
        }
        // put the class into the list
    }

    // we've completed adding everything in the list, so add the last one in.
    if(tmp)
        List->push_back(tmp);

Cleanup:

    if(pfFile)
        fclose(pfFile);

    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("%d sections found"), List->size());

    return bRVal;
}

BOOL
FreeSectionList(std::list<CSection *> *List)
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In FreeSectionList"));

    while(!List->empty())
    {
        delete List->front();
        List->pop_front();
    }

    // Currently no failure conditions.
    return TRUE;
}

// This function takes a string and searches the structure
// for the string, returning the corresponding DWORD
BOOL CSection::GetDWord(TSTRING tsString, int dwBase, DWORD *dwOutNum)
{
    BOOL bRval = FALSE;
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In GetDWord(), %s"), tsString.c_str());
    // find the string in the map class
    tsMap::iterator itr = m_tcmEntriesList.find(tsString);
    TSTRING tsFormatString =  TEXT("%ld \t");
    DWORD dwTmpNum;

    if(16 == dwBase)
        tsFormatString = TEXT("0x%08x ");

    // if it was found, try to convert it to a number
    if(itr != m_tcmEntriesList.end())
    {
        if(EOF != _stscanf(((*itr).second).c_str(), tsFormatString.c_str(), &dwTmpNum))
        {
            if(NULL != dwOutNum)
                *dwOutNum = dwTmpNum;
            g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("Found DWORD, %d"), dwTmpNum);

            // if it exists and is valid but the user didn't pass in a pointer to hold it, just validate it's existance.
            bRval = TRUE;
        }
    }

    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("Leaving GetDWord()"));
    return bRval;
}

// This function takes a string and searches the structure
// for the string, returning the corresponding double
BOOL CSection::GetDouble(TSTRING tsString, double *dOutNum)
{
    BOOL bRval = FALSE;
    double dTmpNum;

    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In GetDouble(), %s"), tsString.c_str());
    // find the string in the map class
    tsMap::iterator itr = m_tcmEntriesList.find(tsString);

    // if it was found, try to convert it to a number
    if(itr != m_tcmEntriesList.end())
    {
        if(EOF != _stscanf(((*itr).second).c_str(), TEXT("%lf "), &dTmpNum))
        {
            if(NULL != dOutNum)
                *dOutNum = dTmpNum;

            g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("Found double, %f"), *dOutNum);
            // if a pointer wasn't passed to receive the value, validate it's existance.
            bRval = TRUE;
        }
    }

    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("Leaving GetDWord()"));
    return bRval;
}

// This functiontakes a string and searches the structure
// for the string, returning the corresponding TSTRING
BOOL CSection::GetString(TSTRING tsString, TSTRING * tcOutString)
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In GetString(), %s"), tsString.c_str());
    BOOL bRval = FALSE;

    // find the string in the map class
    tsMap::iterator itr = m_tcmEntriesList.find(tsString);

    // if it was found, save it.
    if(itr != m_tcmEntriesList.end())
    {
        if(tcOutString)
            *tcOutString = (*itr).second;

        g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("Found String, %s"), (*itr).second.c_str());
        // if no pointer was passed to recieve the string, just validate it's existance.
        bRval = TRUE;
    }

    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("Leaving GetString()"));
    return bRval;
}

// This function takes a string and searches the structure
// for the string, returning the corresponding DWORD array.
// it returns the number of valid entries on success, 0 on failure,
// and stops filling at nMaxEntries.
int CSection::GetDWArray(TSTRING tsString, int dwBase, DWORD dwArray[], int nMaxEntries)
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In GetDWArray(), %s"), tsString.c_str());
    int nFilledEntries = 0;
    TCHAR *string;
    TCHAR seps[] = TEXT(" ,()-\t");
    TCHAR *token;
    TSTRING tsFormatString =  TEXT("%ld ");
    DWORD dwTmpNum;

    if(16 == dwBase)
        tsFormatString = TEXT("0x%08x ");

    // find the string in the map class
    tsMap::iterator itr = m_tcmEntriesList.find(tsString);

    // if it was found, try to convert it to an array
    if(itr != m_tcmEntriesList.end())
    {
        // copy the string to a local buffer for parsing.
        string = new(TCHAR[_tcslen((*itr).second.c_str()) + 1]);
        if(string)
        {
            _tcscpy(string, (*itr).second.c_str());
            token = _tcstok(string, seps);

            // go until it's filled, or we run out of entries.
            // if dwArray is NULL, then we want to go until we run out of tokens.
            while(token && (!dwArray || nFilledEntries < nMaxEntries))
            {
                if(EOF != _stscanf(token, tsFormatString.c_str(), &dwTmpNum))
                {
                    if(NULL != dwArray)
                        dwArray[nFilledEntries] = dwTmpNum;

                    // if no pointer was passed, then just validate it's existance.
                    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("Entry %d found"), dwTmpNum);
                    nFilledEntries++;
                }

                // go to the next token.
                token = _tcstok(NULL, seps);
            }
            delete[] string;
        }
        else
            g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to allocate tempoary string buffer."));
    }
    else goto cleanup;

cleanup:
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("leaving GetDWArray() %d entries found."), nFilledEntries);
    return nFilledEntries;
}

// This function takes a string and searches the structure
// for the string, returning the corresponding TSTRING array.
// it returns the number of valid entries on success, 0 on failure,
// and stops filling at nMaxEntries.
int CSection::GetStringArray(TSTRING tsString, TSTRING tsArray[], int nMaxEntries)
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In GetStringArray(), %s"), tsString.c_str());
    int nFilledEntries = 0;
    TCHAR *string;
    TCHAR seps[] = TEXT(",()-");
    TCHAR *token;

    // find the string in the map class
    tsMap::iterator itr = m_tcmEntriesList.find(tsString);

    // if it was found, try to convert it to an array
    if(itr != m_tcmEntriesList.end())
    {
        string = new(TCHAR[_tcslen((*itr).second.c_str()) + 1]);
        if(string)
        {
            // copy the string to a local buffer for parsing.
            _tcscpy(string, (*itr).second.c_str());

            token = _tcstok(string, seps);

            // chop off leading white space.
            while(token[0] == ' ' || token[0] == '\t')
                token++;

            // go until we hit the end, or run out of entries
            // if tsArray is NULL, then we want to go until we're out of tokens.
            while(token && (!tsArray || nFilledEntries < nMaxEntries))
            {
                // put the string in the array if the array is valid.
                // if the array isn't valid, then we just want a count.
                if(tsArray)
                {
                    // chop off leading white space.
                    while(token[0] == ' ' || token[0] == '\t')
                        token++;
                    tsArray[nFilledEntries]=token;
                }

                // if the array isn't valid, then just validate it's existance.
                g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("Entry %s found"), token);
                nFilledEntries++;

                // go to the next token.
                token = _tcstok(NULL, seps);
            }
            delete[] string;
        }
        else
            g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to allocate tempoary string buffer."));
    }

    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("leaving GetStringArray() %d entries found."), nFilledEntries);
    return nFilledEntries;
}

BOOL CSection::AddEntry(TSTRING tsKey, TSTRING tsEntry)
{
    m_tcmEntriesList[tsKey] = tsEntry;
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("   Adding key %s with entry %s"), tsKey.c_str(), tsEntry.c_str());
    return TRUE;
}

BOOL CSection::CombineEntry(TSTRING tsKey, TSTRING tsEntry)
{
    m_tcmEntriesList[tsKey] += tsEntry;
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("   Combining key %s with entry %s"), tsKey.c_str(), tsEntry.c_str());
    return TRUE;
}

BOOL CSection::SetName(TSTRING tsName)
{
    m_tsName = tsName;
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("Setting the section name to %s"), tsName.c_str());
    return TRUE;
}

TSTRING CSection::GetName()
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("Returning section name %s"), m_tsName.c_str());
    return m_tsName;
}
