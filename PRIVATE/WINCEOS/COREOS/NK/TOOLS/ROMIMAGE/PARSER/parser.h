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
#ifndef INCLUDED_PARSER_H
#define INCLUDED_PARSER_H

#include "..\romimage\headers.h"
#include "token.h"

bool mygetline(FILE *file, string &line, char delim = '\n');
bool mygetline(FILE *file, wstring &line, wchar_t delim = '\n');

bool IsSection(const wstring &token);
bool GetNextLine(FILE *file, wstring &line);
wstring FudgeIfs(wstring line);
wstring ExpandEnvironmentVariables(wstring line);
bool SkipLine(const wStringList &token_list);

#define WFILES   L"files"
#define WMODULES L"modules"
#define WCONFIG  L"config"
#define WMEMORY  L"memory"

#endif
