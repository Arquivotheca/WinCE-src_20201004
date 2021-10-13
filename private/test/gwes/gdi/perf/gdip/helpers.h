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
#include "tuxmain.h"
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
