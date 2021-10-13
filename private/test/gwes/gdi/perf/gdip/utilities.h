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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
// external includes required for NT
#include <tchar.h>
#pragma warning (push)
#pragma warning (disable: 4995)  //turn off deprecated str functions in stl
#include <map>
#include <string>
#include <list>
#pragma warning (pop)
#include <math.h>

#include "bencheng.h"
#include "Otak.h"
#include "section.h"

#ifndef GLOBAL_H
#define GLOBAL_H

enum KATO_VERBOSITY
{
    VERBOSITY_DEFAULT = 0,
    VERBOSITY_RESULTS = 0,
    VERBOSITY_ECHO,
    VERBOSITY_ALL,
};

// typedef's
typedef std::basic_string<TCHAR> TSTRING;
typedef std::map<TSTRING, TSTRING> tsMap;

// internal function declarations
BOOL BenchmarkEngine(std::list<CSection *> *List);

// Global utility functions
BOOL ParseCommandLine(LPCTSTR CommandLine, TSTRING * tsFileName);
BOOL ListToCSVString(std::list<TSTRING> *tsList, TSTRING *ts);
double NormDistInv(double);

#define DEFAULTSCRIPTFILE TEXT("scriptfile.txt")
#define DEFAULTLOGFILE TEXT("logresult.csv")
#define DEFAULTXMLFILE TEXT("logresult.xml")

#define  PROBZENTRY(x, y)   { x, y } 

struct ProbZPair {
   double dProbability;
   double dZValue;
};

#endif
