//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "bencheng.h"

#ifndef HELPERS_H
#define HELPERS_H

struct NameValPair {
   DWORD dwVal;
   TCHAR *szName;
};

BOOL nvSearch(struct NameValPair *nvList, TSTRING tsKey, DWORD * dwReturnValue);
BOOL AllocDWArray(TSTRING tsEntryName, DWORD dwDefaultEntry, DWORD **dwPointer, int * MaxIndex, CSection * SectionList, int base);
BOOL AllocTSArray(TSTRING tsEntryName, TSTRING tsDefaultEntry, TSTRING **tsPointer, int * MaxIndex, CSection * SectionList);
BOOL AllocTCArrayFromDWArray(TSTRING tsEntryName, TCHAR **tcPointer, int * MaxIndex, CSection * SectionList, int base);

#endif
