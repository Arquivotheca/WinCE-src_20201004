//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>

#ifndef __CSVFILE_H__
#define __CSVFILE_H__

class CCSVFile
{
public:
    CCSVFile(LPWSTR pszFileName) :
        m_pFile(NULL)
    { 
        if(pszFileName && pszFileName[0]) {
            m_pFile = ::_wfopen(pszFileName, L"w");
        }
        ::InitializeCriticalSection(&m_cs);
    }
        
    ~CCSVFile()
    {
        if(m_pFile) {
            ::fclose(m_pFile);
            m_pFile = NULL;
        }
        ::DeleteCriticalSection(&m_cs);
    }    

    void NextColumn()
    {
        WriteValue(L",");
    }

    void NextRow()
    {
        WriteValue(L"\n");
        ::fflush(m_pFile);
    }

    void WriteValue(LPWSTR pszFormat, ...)
    {
        va_list va;

        if(m_pFile) {
            va_start(va, pszFormat);
            ::EnterCriticalSection(&m_cs);
            ::vfwprintf(m_pFile, pszFormat, va);
            ::LeaveCriticalSection(&m_cs);
            va_end(va);
        }
    }

    void WriteValueV(LPWSTR pszFormat, va_list va)
    {
        if(m_pFile) {
            ::EnterCriticalSection(&m_cs);
            ::vfwprintf(m_pFile, pszFormat, va);
            ::LeaveCriticalSection(&m_cs);
        }
    }

private:
    FILE *m_pFile;
    CRITICAL_SECTION m_cs;
};

class CPerfCSV : protected CCSVFile
{
public:
    CPerfCSV(LPWSTR pszFileName) :
        CCSVFile(pszFileName),
        m_fEmptyFile(TRUE)
    {

    }

    void InitTest(LPWSTR pszFormat, ...)
    {
        va_list va;

        if(m_fEmptyFile) {
           m_fEmptyFile = FALSE; 
        } else {
            NextRow();
        }

        va_start(va, pszFormat);
        WriteValueV(pszFormat, va);
        va_end(va);
    }

    void StartTick() 
    {
        m_tLastTick = ::GetTickCount();
    }

    void EndTick()
    {
        NextColumn();
        WriteValue(L"%u", ::GetTickCount() - m_tLastTick);
    }
    
private:
    BOOL m_fEmptyFile;
    DWORD m_tLastTick;
};

#endif // __CSVFILE_H__
