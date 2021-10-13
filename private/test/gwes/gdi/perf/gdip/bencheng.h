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

void CalibrateBenchmarkEngine();
BOOL RunTestSuite(std::list<CTestSuite *> *TestList);
BOOL CleanupTestSuite(std::list<CTestSuite *> *TestList);

#endif
