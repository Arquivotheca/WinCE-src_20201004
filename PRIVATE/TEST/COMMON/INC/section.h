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

#ifndef SECTION
#define SECTION

#include <tchar.h>
#include <string>
#include <map>
#include <list>
#include "otak.h"

// typedef's
typedef std::basic_string<TCHAR> TSTRING;
typedef std::map<TSTRING, TSTRING> tsMap;

// defined values
#define MAXLINE 256

// global values
extern COtak *g_pCOtakLog;

// class definitions
class CSection
{
    public:
        CSection() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CSection constructor")); }
        ~CSection() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CSectionDestructor")); }

        BOOL GetDWord(TSTRING, int, DWORD *);
        BOOL GetDouble(TSTRING, double *);
        BOOL GetString(TSTRING, TSTRING *);
        int GetDWArray(TSTRING, int, DWORD [], int);
        int GetStringArray(TSTRING, TSTRING [], int);
        TSTRING GetName();

        BOOL AddEntry(TSTRING, TSTRING);
        BOOL CombineEntry(TSTRING, TSTRING);
        BOOL SetName(TSTRING);

    private:
        tsMap m_tcmEntriesList;
        TSTRING m_tsName;
};

// functions exported
BOOL InitializeSectionList(TSTRING tszFileName, std::list<CSection *> *List);
BOOL FreeSectionList(std::list<CSection *> *List);

#endif
