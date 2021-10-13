//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>

#ifndef _IMGCOMPARE_H_
#define _IMGCOMPARE_H_

#define MAX_LOGLINES 512

class cCompareResults
{

private:
    BOOL m_bPass;
    int  m_nMaxResults;
    TCHAR **m_tszLogInfo;

public:

    cCompareResults() :
        m_nMaxResults(MAX_LOGLINES),
        m_bPass(FALSE),
        m_nLogLinesCount(0),
        m_tszLogInfo(NULL)
    { 
    }

    ~cCompareResults() 
    {
        if (m_tszLogInfo)
        {
            free(m_tszLogInfo);
            m_tszLogInfo = NULL;
        }
    }

    // an  array of strings which the app can output to give the user
    // whatever useful debugging data the comparitor had.
    int m_nLogLinesCount;

    // simple pass/fail results
    void SetPassResult(BOOL bPass) { m_bPass = bPass; }
    BOOL GetPassResult() {    return m_bPass; }

    // resets the pass flag, and also clears the existing log
    // this can called before comparisons in the test loop.
    void Reset();
    
    // the maximum number of result information lines stored in the log.  
    // -1 means all results as it the default    
    void SetMaxResults(int nMaxResults);
    int GetMaxResults() { return m_nMaxResults;    }

    TCHAR* GetLogLine(int iIndex);
    BOOL LogIsFull();
    void AppendLogInfo(TCHAR *tszInfo);
    void DumpLog();
};


#endif //_IMGCOMPARE_H_
