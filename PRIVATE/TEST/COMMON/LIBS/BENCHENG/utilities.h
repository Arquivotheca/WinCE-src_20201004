//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// external includes
#include <tchar.h>
#include <map>
#include <string>
#include <list>
#include <math.h>

#include "bencheng.h"
#include "Otak.h"
#include "section.h"

#ifndef GLOBAL_H
#define GLOBAL_H

// global values
extern COtak *g_pCOtakLog;
extern COtak *g_pCOtakResult;


// typedef's
typedef std::basic_string<TCHAR> TSTRING;
typedef std::map<TSTRING, TSTRING> tsMap;

// internal function declarations
BOOL BenchmarkEngine(std::list<CSection *> *List);

// Global utility functions
BOOL ListToCSVString(std::list<TSTRING> *tsList, TSTRING *ts);
double NormDistInv(double);

#define ONE_SECOND 1000

#define DEFAULTSCRIPTFILE TEXT("scriptfile.txt")
#define DEFAULTLOGFILE TEXT("logresult.csv")
#define DEFAULTDEBUGFILE TEXT("status.txt")

#define  PROBZENTRY(x, y)   { x, y } 

struct ProbZPair {
   double dProbability;
   double dZValue;
};

#endif

