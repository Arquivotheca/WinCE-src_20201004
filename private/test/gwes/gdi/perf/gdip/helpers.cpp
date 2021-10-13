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
#include "helpers.h"

BOOL
AllocDWArray(TSTRING tsEntryName, DWORD dwDefaultEntry, DWORD **dwPointer, int * MaxIndex, CSection * SectionList, int base)
{
    int nCountRecieved;
    BOOL bRval = TRUE;

    if(nCountRecieved = SectionList->GetDWArray(tsEntryName, base, NULL, 0))
    {
        if(*dwPointer = new(DWORD[nCountRecieved]))
        {
            *MaxIndex = SectionList->GetDWArray(tsEntryName, base, *dwPointer, nCountRecieved);

            if(nCountRecieved != *MaxIndex)
            {
                info(FAIL, TEXT("%s - Original dword count returned %d doesn't match actual dword count %d."),
                    tsEntryName, nCountRecieved, *MaxIndex);
                bRval = FALSE;
            }
        }
        else
        {
            info(FAIL, TEXT("%s - Array Allocation failed."), tsEntryName);
            bRval = FALSE;
        }
    }
    else
    {
        if(*dwPointer = new(DWORD[1]))
        {
            *dwPointer[0] = dwDefaultEntry;
            *MaxIndex = 1;
        }
        else
        {
            info(FAIL, TEXT("%s - Default Array Allocation failed."), tsEntryName);
            bRval = FALSE;
        }
    }

    return bRval;
}


BOOL
AllocTCArrayFromDWArray(TSTRING tsEntryName, TCHAR **tcPointer, int * MaxIndex, CSection * SectionList, int base)
{
    int nCountRecieved;
    BOOL bRval = TRUE;
    // temporary pointer to the DWORD array until we copy it into the tchar array.
    DWORD * dwPointer;

    // find out how many entries there are in the dword array
	// use base 10 so we count both base 10 and base 16 lists.
    if(nCountRecieved = SectionList->GetDWArray(tsEntryName, 10, NULL, 0))
    {
        // allocate that many entries int the dword array, max+1 in the tchar array for NULL termination.
        dwPointer = new(DWORD[nCountRecieved]);
        *tcPointer = new(TCHAR[nCountRecieved + 1]);

        if(NULL != dwPointer && NULL != *tcPointer)
        {
            // retrieve the dword array.
            *MaxIndex = SectionList->GetDWArray(tsEntryName, base, dwPointer, nCountRecieved);

            // if we recieved an incorrect number of entries, then we have an error.
            if(nCountRecieved == *MaxIndex)
            {
                // copy the dword array into the character array
                for(int i = 0; i < nCountRecieved; i++)
                    (*tcPointer)[i] = (TCHAR) dwPointer[i];

                // NULL terminate the character array, and delete the temporary buffer.
                (*tcPointer)[*MaxIndex] = '\0';
                delete[] dwPointer;
            }
            else
            {
                delete[] dwPointer;
                delete[] *tcPointer;
                (*tcPointer) = NULL;
                info(FAIL, TEXT("%s - Original dword count returned %d doesn't match actual dword count %d."),
                    tsEntryName, nCountRecieved, *MaxIndex);
                bRval = FALSE;
            }
        }
        else
        {
            delete[] dwPointer;
            delete[] *tcPointer;
            *tcPointer = NULL;
            info(FAIL, TEXT("%s - Array Allocation failed."), tsEntryName);
            bRval = FALSE;
        }
    }
    else
        bRval = FALSE;

    return bRval;
}


BOOL
AllocTSArray(TSTRING tsEntryName, TSTRING tsDefaultEntry, TSTRING **tsPointer, int * MaxIndex, CSection * SectionList)
{
    int nCountRecieved;
    BOOL bRval = TRUE;

    if(nCountRecieved = SectionList->GetStringArray(tsEntryName, NULL, 0))
    {
        if(*tsPointer = new(TSTRING[nCountRecieved]))
        {
            *MaxIndex = SectionList->GetStringArray(tsEntryName, *tsPointer, nCountRecieved);

            if(nCountRecieved != *MaxIndex)
            {
                info(FAIL, TEXT("%s - Original string count returned %d doesn't match actual string count %d."),
                    tsEntryName, nCountRecieved, *MaxIndex);
                bRval = FALSE;
            }
        }
        else
        {
            info(FAIL, TEXT("%s - Array Allocation failed."), tsEntryName);
            bRval = FALSE;
        }
    }
    else
    {
        if(*tsPointer = new(TSTRING[1]))
        {
            *tsPointer[0] = tsDefaultEntry;
            *MaxIndex = 1;
        }
        else
        {
            info(FAIL, TEXT("%s - Default Array Allocation failed."), tsEntryName);
            bRval = FALSE;
        }
    }

    return bRval;
}

BOOL nvSearch(struct NameValPair *nvList, TSTRING tsKey, DWORD * dwReturnValue)
{
    BOOL bRval = FALSE;

    if(nvList && dwReturnValue)
    {
        for(int i = 0; ; i++)
        {
            if(nvList[i].szName == NULL)
                break;
            else if(tsKey == nvList[i].szName)
            {
                *dwReturnValue = nvList[i].dwVal;
                bRval = TRUE;
                break;
            }
        }
    }

    return bRval;
}

