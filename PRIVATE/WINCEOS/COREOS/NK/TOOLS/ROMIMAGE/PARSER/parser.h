//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef INCLUDED_PARSER_H
#define INCLUDED_PARSER_H

#include "..\romimage\headers.h"
#include "token.h"

bool IsSection(const string &token);
bool GetNextLine(istream &file, string &line);
string FudgeIfs(string line);
string ExpandEnvironmentVariables(string line);
bool SkipLine(const StringList &token_list);

#define FILES   "files"
#define MODULES "modules"
#define CONFIG  "config"
#define MEMORY  "memory"

#endif
