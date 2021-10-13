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

//free was declared deprecated ...
//BUGBUG is a leak here
//should free each string
#pragma warning (disable:4996)
    ~cCompareResults() 
    {
        if (m_tszLogInfo)
        {
            free(m_tszLogInfo);
            m_tszLogInfo = NULL;
        }
    }
#pragma warning (default:4996)


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
    virtual void SetMaxResults(int nMaxResults);
    virtual int GetMaxResults() { return m_nMaxResults;    }

    virtual TCHAR* GetLogLine(int iIndex);
    virtual BOOL LogIsFull();
    virtual void AppendLogInfo(TCHAR *tszInfo);
    virtual void DumpLog();
};


#endif //_IMGCOMPARE_H_
