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
#ifndef INCLUDED_MYSTRING_H
#define INCLUDED_MYSTRING_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <FCNTL.H>

#include <string>

using namespace std;

bool operator==(const string &s1, const string &s2);
bool operator==(const string &o1, const char *o2);
bool operator==(const char *o1, const string &o2);

bool operator==(const wstring &s1, const wstring &s2);
bool operator==(const wstring &o1, const wchar_t *o2);
bool operator==(const wchar_t *o1, const wstring &o2);

bool operator!=(const string &s1, const string &s2);
bool operator!=(const string &o1, const char *o2);
bool operator!=(const char *o1, const string &o2);

bool operator!=(const wstring &s1, const wstring &s2);
bool operator!=(const wstring &o1, const wchar_t *o2);
bool operator!=(const wchar_t *o1, const wstring &o2);

string lowercase(string s);
wstring lowercase(wstring s);

string toansi(wstring s, bool warn = true);
wstring tounicode(string s);

#endif

