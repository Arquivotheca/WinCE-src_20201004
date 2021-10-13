//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include <tchar.h>
#include <string>

#ifndef OTAK_H
#define OTAK_H

#define OTAK_MAX_VERBOSITY 256
#define OTAK_MAX_STRING 2048
#define OTAK_MAX_FILENAME MAX_PATH

struct OtakDestinationStruct
{
    UINT dwSize;
    UINT dwDestination;
    std::basic_string<TCHAR> tsFileName;
};

enum OTAKLOGTYPE
{
    OTAKFILEDESTINATION = 1,
    OTAKDEBUGDESTINATION,
    OTAKINVALIDDESTINATION
};

enum OTAKBASEVERBOSITIES
{
    OTAK_REQUIRED=0,
    OTAK_RESULT = 0,
    OTAK_ERROR = 0,
    OTAK_DETAIL,
    OTAK_VERBOSE
};

class COtak
{
    public:
        COtak();
        ~COtak();

        void Log(UINT nVerbosity, LPTSTR szFmt, ...);
        BOOL SetDestination(OtakDestinationStruct ODS);
        int SetVerbosity(int nVerbosity);

    private:
        HANDLE m_hFile;
        UINT m_nDestination;
        UINT m_nVerbosity;
        TCHAR *m_ptcBuf;
        CHAR *m_pcstrBuf;
};

#endif
