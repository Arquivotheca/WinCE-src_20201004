//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "section.h"
#include "otak.h"

#ifndef TESTSUITE_H
#define TESTSUITE_H

// global values
extern COtak *g_pCOtakLog;

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
