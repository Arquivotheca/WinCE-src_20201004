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
#ifndef INCLUDED_TOKEN_H
#define INCLUDED_TOKEN_H

// warning about symbol names being 
// too long and getting truncated
#pragma warning(disable:4786)

#include <iostream>
#include "..\mystring\mystring.h"
#include <vector>
#include <algorithm>

using namespace std;

class Token{
private:
  Token();

  static wchar_t DoLongFileNameFudge(wchar_t ch);
  static wchar_t UnDoLongFileNameFudge(wchar_t ch);
  
public:
  static bool  get_bool_value(const string &token, const string &value = "on", bool ret = true);
  static unsigned long get_hex_value(const string &token);
  static unsigned long get_dec_value(const string &token);
  static int CheckQuotes(wstring line, bool check = false);
  static vector<wstring> tokenize(wstring line);

  static string get_token(string line, unsigned long begin, string delim);
};

#endif
