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
#ifndef INCLUDED_HEADERS_H
#define INCLUDED_HEADERS_H

// warning about symbol names being 
// too long and getting truncated
#pragma warning(disable:4786)
#pragma warning(disable:4800)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif

#include <windows.h>
#include <io.h>
#include <FCNTL.H>

#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <fstream>

#include "..\mystring\mystring.h"
#include <vector>
#include <list>
#include <map>
#include <iterator>
#include <algorithm>
#include <functional>

#include <assert.h>

using namespace std;

extern bool last_pass;
#define LAST_PASS_PRINT if(last_pass)

typedef vector<wstring> wStringList;
typedef vector<string> StringList;

#define SIGNATURE_SIZE 2

#endif
