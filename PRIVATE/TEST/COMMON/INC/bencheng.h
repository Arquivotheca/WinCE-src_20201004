//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef BENCHENG_H
#define BENCHENG_H
#include "section.h"
#include "testsuite.h"

#define  countof(x)        (sizeof(x) / sizeof(x[0]))

// defined values for usage.
#define CALIBRATION_CYCLES 5000
#define MIN_SAMPLE_SIZE 10
#define WIDTH_MULTIPLIER 2
// default to 10 minutes run time as the max.
#define DEFAULTMAXRUNTIME 10
// function declarations for bencheng
CTestSuite *CreateTestSuite(TSTRING tsName, CSection *SectionData);
int RunBenchmark(LPCTSTR CommandLine);
TSTRING dtos(double);
TSTRING itos(int);
TSTRING itohs(int);
#endif
