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
#include "tuxmain.h"
#include "section.h"

#ifndef TESTSUITE_H
#define TESTSUITE_H

// data structures
struct TestInfo
{
    std::list<TSTRING> Descriptions;
};

enum DataSelectionType {
   COUNT,
   CONFIDENCE
};

struct TestSuiteInfo
{
    int nDataSelectionType;
    double dDataValue;
    double dMaxMinutesToRun;
    TSTRING tsTestName;
    TSTRING tsTestModifiers;
    DWORD dwCacheRangeFlush;
    int nPriority;
    std::list<TSTRING> tsFieldDescription;
};

// class declarations
class CTestSuite
{
    private:

    protected:
        // private pointer to the section data.
        class CSection *m_SectionList;

    public:
        CTestSuite();
        CTestSuite(CSection * Section);
        virtual ~CTestSuite();

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();
};
#endif
