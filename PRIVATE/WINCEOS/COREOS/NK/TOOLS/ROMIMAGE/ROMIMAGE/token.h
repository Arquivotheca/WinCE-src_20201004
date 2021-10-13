//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef INCLUDED_TOKEN_H
#define INCLUDED_TOKEN_H

// dumb warning about symbol names being 
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

  static char DoLongFileNameFudge(char ch);
  static char UnDoLongFileNameFudge(char ch);
  
public:
  static bool  get_bool_value(const string &token, const string &value = "on", bool ret = true);
  static unsigned long get_hex_value(const string &token);
  static unsigned long get_dec_value(const string &token);

  static vector<string> tokenize(string line);

  static string get_token(string line, unsigned long begin, string delim);
};

#endif
