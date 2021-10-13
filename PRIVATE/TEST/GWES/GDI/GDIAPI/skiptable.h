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
#include <windows.h>
#include <tchar.h>

#ifndef SKIPTABLE_H
#define SKIPTABLE_H

#define EMPTYSKIPTABLE TEXT("empty")
#define MAX_FONTLINKSTRINGLENGTH 256

typedef class cSkipTable {
    public:
        cSkipTable(): m_pnSkipTable(NULL), m_nSkipTableEntryCount(0), m_tszSkipString(EMPTYSKIPTABLE) {}
        ~cSkipTable();

        bool InitializeSkipTable(const TCHAR *tszSkipString);
        bool IsInSkipTable(int nSkipEntry);
        bool OutputSkipTable();
        TCHAR* SkipString();

        private:
            int *m_pnSkipTable;
            int m_nSkipTableEntryCount;
            TCHAR *m_tszSkipString;
} SKIPTABLE;
#endif

