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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "results.h"

TCHAR* g_tszLogSizeError = TEXT("error lines exceed the log size limit\n");

//
// AppendLogInfo
//
//   Appends a string to the log info.
//
//   The log is an array of strings, so the array is resized before adding the new string
//   to the array.
//
//
// Arguments:
//
//   TCHAR *tszInfo: the string to be added to the log
//
// Return Value:
//
//   none
//
void cCompareResults::AppendLogInfo(TCHAR *tszInfo)
{

    TCHAR **tszTemp;

    //checks for null pointer
    if (NULL == tszInfo) return;

    //exit if the user set a max number of lines for the log results
    if (m_nLogLinesCount >= m_nMaxResults )
        return;

    //if this is the last line we are going to
    //use this line to say that we reached the log limit
    if (m_nLogLinesCount == m_nMaxResults - 1)
        tszInfo = (TCHAR*) g_tszLogSizeError;

    int nSize = wcslen(tszInfo);

    //check if the line is empty
    if (!nSize) return;

    //resizes the array of strings
    tszTemp = (TCHAR **) realloc( m_tszLogInfo, ( m_nLogLinesCount + 1) * sizeof(TCHAR *));
    if (tszTemp)
    {
        // prevents m_tszLogInfo to be clobbered by realloc's return value of NULL
        m_tszLogInfo = tszTemp;
    }
    else
    {
        OutputDebugString(TEXT("AppendLogInfo returned OUT OF MEMORY\n"));
        return;
    }

    //allocates the memory needed to store the string
    m_tszLogInfo[m_nLogLinesCount] = (TCHAR *) malloc(sizeof(TCHAR) * (nSize + 1));
    if (!m_tszLogInfo[m_nLogLinesCount])
    {
        OutputDebugString(TEXT("AppendLogInfo returned OUT OF MEMORY\n"));
        return;
    }

    //stores it in the array
    wcsncpy( m_tszLogInfo[m_nLogLinesCount], tszInfo, nSize + 1);


    m_nLogLinesCount++;

}

//
// GetLogLine
//
//   Gets a pointer to the log line at a specific index.
//
//
// Arguments:
//
//   int iIndex: index of the line being requested (zero based)
//
// Return Value:
//
//   TCHAR *: pointer to the string containing the info
//
TCHAR* cCompareResults::GetLogLine(int iIndex)
{

    if (iIndex < 0 || iIndex >= m_nLogLinesCount)
        return NULL;

    return m_tszLogInfo[iIndex];

}

//
// LogIsFull
//
//   Tells the caller whether or not the log is at or past it's maximum.
//
//
// Arguments:
//
//   none
//
// Return Value:
//
//   BOOL: True if the log is full, false otherwise
//
BOOL cCompareResults::LogIsFull()
{
    return(m_nLogLinesCount >= m_nMaxResults);
}

//
// SetMaxResults
//
//   Sets the maximum size for the log.
//
//   -1 means the max size which cannot be more than MAX_LOGLINES
//
//
// Arguments:
//
//   int nMaxResults: max size fot th log
//
// Return Value:
//
//   none
//
void cCompareResults::SetMaxResults(int nMaxResults)
{
    if (nMaxResults > MAX_LOGLINES || nMaxResults < 0)
        m_nMaxResults = MAX_LOGLINES;
    else
        m_nMaxResults = nMaxResults;
}

//
// DumpLog
//
//   Dumps the log lines to the debugger
//
//
// Arguments:
//
//   none
//
// Return Value:
//
//   none
//
void cCompareResults::DumpLog()
{
    for (int i=0; i<m_nLogLinesCount ; i++)
        if (GetLogLine(i))
            OutputDebugString(GetLogLine(i));

}

//
// Reset
//
//   Resets the log info by freeing the strings and resetting variables
//
//
// Arguments:
//
//   none
//
// Return Value:
//
//   none
//
void cCompareResults::Reset()
{
    m_bPass = FALSE;
    m_nLogLinesCount = 0;

    if (m_tszLogInfo)
    {
        free(m_tszLogInfo);
        m_tszLogInfo = NULL;
    }

}
