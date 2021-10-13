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
        TCHAR *m_pcstrBuf;
};

#endif
