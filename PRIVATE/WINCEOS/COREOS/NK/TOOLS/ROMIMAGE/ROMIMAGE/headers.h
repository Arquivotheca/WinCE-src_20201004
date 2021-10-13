//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef INCLUDED_HEADERS_H
#define INCLUDED_HEADERS_H

// dumb warning about symbol names being 
// too long and getting truncated
#pragma warning(disable:4786)
#pragma warning(disable:4800)

#define _WIN32_WINNT 0x0400

#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <fstream>

#include <..\mystring\mystring.h>
#include <vector>
#include <list>
#include <map>
#include <iterator>
#include <algorithm>
#include <functional>

#include <windows.h>
#include <assert.h>

using namespace std;

extern bool last_pass;
#define LAST_PASS_PRINT if(last_pass)

typedef vector<string> StringList;

#endif
