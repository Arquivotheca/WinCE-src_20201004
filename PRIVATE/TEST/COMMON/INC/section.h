//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
