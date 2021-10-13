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
#include "Otak.h"

COtak::COtak()
{
    m_hFile = NULL ;
    m_nDestination = NULL;
    m_nVerbosity = NULL;
    m_ptcBuf = new(TCHAR[OTAK_MAX_STRING]);
    m_pcstrBuf = new(CHAR[OTAK_MAX_STRING]);
}

COtak::~COtak()
{
    if(m_hFile)
        CloseHandle(m_hFile);
    delete[] m_ptcBuf;
    delete[] m_pcstrBuf;
}

void
COtak::Log(UINT nVerbosity, LPTSTR szFmt, ...)
{
    va_list vlArg;
    DWORD dwBytesWritten;

    if(nVerbosity <= m_nVerbosity)
    {
        va_start(vlArg, szFmt);
        _vsntprintf(m_ptcBuf, OTAK_MAX_STRING, szFmt, vlArg);
        va_end(vlArg);
        m_ptcBuf[OTAK_MAX_STRING-1] = '\0';

        switch(m_nDestination)
        {
            case OTAKDEBUGDESTINATION:
                OutputDebugString(m_ptcBuf);

                // NT OutputDebugString doesn't put line feeds after the strings.
                #ifndef UNDER_CE
                OutputDebugString(TEXT("\n"));
                #endif
                break;
            case OTAKFILEDESTINATION:
                // write to the file.
#ifdef UNICODE
                wcstombs(m_pcstrBuf, m_ptcBuf, OTAK_MAX_STRING - 3);
#else
                _tcsncpy(m_pcstrBuf, m_ptcBuf, OTAK_MAX_STRING -3);
#endif
                strncat(m_pcstrBuf, "\r\n", OTAK_MAX_STRING-strlen(m_pcstrBuf));
                if(!WriteFile(m_hFile, m_pcstrBuf, strlen(m_pcstrBuf), &dwBytesWritten, NULL))
                    OutputDebugString(TEXT("Error writing to the file."));
                FlushFileBuffers(m_hFile);
                break;
        }
    }
}

BOOL
COtak::SetDestination(OtakDestinationStruct ODS)
{
    BOOL bRval = FALSE;

    if(ODS.dwSize == sizeof(OtakDestinationStruct))
        if(ODS.dwDestination < OTAKINVALIDDESTINATION)
        {
            m_nDestination = ODS.dwDestination;

            if(m_nDestination == OTAKFILEDESTINATION)
            {
                m_hFile= CreateFile(ODS.tsFileName.c_str(),
                              GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL,
                              CREATE_ALWAYS, 
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);
                if(!m_hFile)
                {
                    OutputDebugString(TEXT("Failed to open file for log output."));
                    // in case anyone tries to use the logger after this fails, output to debug.
                    m_nDestination = OTAKDEBUGDESTINATION;
                    bRval = FALSE;
                }
            }
        }

    return bRval;
}

int
COtak::SetVerbosity(int nVerbosity)
{
    int nOldVerbosity = -1;

    if(m_nVerbosity < OTAK_MAX_VERBOSITY)
    {
        nOldVerbosity = m_nVerbosity;
        m_nVerbosity = nVerbosity;
    }

    return nOldVerbosity;
}
