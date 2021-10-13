//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "mystring.h"
#include <iostream>
#include <algorithm>

using namespace std;

bool operator==(const string &o1, const string &o2){
  string s1 = o1;
  string s2 = o2;

  s1 = lowercase(s1);
  s2 = lowercase(s2);

  return s1.compare(s2) == 0;
}

bool operator==(const string &o1, const char *o2){
  string s1 = o1;
  string s2 = o2;
  
  s1 = lowercase(s1);
  s2 = lowercase(s2);

  return s1.compare(s2) == 0;
}

bool operator==(const char *o1, const string &o2){
  string s1 = o1;
  string s2 = o2;
  
  s1 = lowercase(s1);
  s2 = lowercase(s2);

  return s1.compare(s2) == 0;
}

bool operator!=(const string &o1, const string &o2){
  return !(o1 == o2);
}

bool operator!=(const string &o1, const char *o2){
  return !(o1 == o2);
}

bool operator!=(const char *o1, const string &o2){
  return !(o1 == o2);
}

string lowercase(string s){
  for(int i = 0; i < s.length(); i++)
    s[i] = tolower(s[i]);

  return s;
}
