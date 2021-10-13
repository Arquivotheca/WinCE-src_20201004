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

#include "mystring.h"
#include <iostream>

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

bool operator==(const wstring &o1, const wstring &o2){
  wstring s1 = o1;
  wstring s2 = o2;

  s1 = lowercase(s1);
  s2 = lowercase(s2);

  return s1.compare(s2) == 0;
}

bool operator==(const wstring &o1, const wchar_t *o2){
  wstring s1 = o1;
  wstring s2 = o2;
  
  s1 = lowercase(s1);
  s2 = lowercase(s2);

  return s1.compare(s2) == 0;
}

bool operator==(const wchar_t *o1, const wstring &o2){
  wstring s1 = o1;
  wstring s2 = o2;
  
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

bool operator!=(const wstring &o1, const wstring &o2){
  return !(o1 == o2);
}

bool operator!=(const wstring &o1, const wchar_t *o2){
  return !(o1 == o2);
}

bool operator!=(const wchar_t *o1, const wstring &o2){
  return !(o1 == o2);
}


string lowercase(string s){
  for(unsigned int i = 0; i < s.length(); i++)
    s[i] = tolower(s[i]);

  return s;
}

wstring lowercase(wstring s){
  for(unsigned int i = 0; i < s.length(); i++)
    s[i] = towlower(s[i]);

  return s;
}

#define w2a(w, a, c) WideCharToMultiByte(CP_ACP,NULL,w,-1,a,c,NULL,NULL);
#define a2w(a, w, c) MultiByteToWideChar(CP_ACP,NULL,a,-1,w,c);

string toansi(wstring ws, bool warn /* = true */){
  char s[1024] = {0};

  w2a(ws.c_str(), s, sizeof(s));

  if(warn){
    for(unsigned i = 0; i < ws.length(); i++){
      if(s[i] == '?'){
        cerr  <<  "Warning, possible unicode to ansi error in line.\n";
        wcerr << L"  Unicode: '" << ws << L"'\n";
        cerr  <<  "  Ansi   : '" << s << "'\n";
        break;
      }
    }    
  }
  
  return string(s);
}

wstring tounicode(string s){
  wchar_t ws[1024] = {0};

  a2w(s.c_str(), ws, sizeof(ws)/sizeof(wchar_t));

  return wstring(ws);
}

