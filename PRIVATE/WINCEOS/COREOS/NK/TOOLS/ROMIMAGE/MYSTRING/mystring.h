//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <string>

using namespace std;

bool operator==(const string &s1, const string &s2);
bool operator==(const string &o1, const char *o2);
bool operator==(const char *o1, const string &o2);

bool operator!=(const string &s1, const string &s2);
bool operator!=(const string &o1, const char *o2);
bool operator!=(const char *o1, const string &o2);

string lowercase(string s);

