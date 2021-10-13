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
#include "testsuite.h"
#include "bencheng.h"

CTestSuite::CTestSuite() : m_SectionList(NULL)
{
    info(DETAIL, TEXT("In CTestSuite base constructor"));
}

CTestSuite::CTestSuite(CSection * Section) : m_SectionList(Section)
{
    info(DETAIL, TEXT("In CTestSuite overloaded constructor"));
}

CTestSuite::~CTestSuite()
{
    info(DETAIL, TEXT("In CTestSuite destructor"));
}

BOOL
CTestSuite::Initialize(TestSuiteInfo *tsi)
{ 
    double dTmp = 0;
    BOOL bRval = TRUE;

    info(DETAIL, TEXT("In CTestSuite Initialize."));

    if(tsi && m_SectionList)
    {
        // retrieve the test suite type from the user.  
        // if it's not there then it will use the default.
        if(m_SectionList->GetDouble(TEXT("Confidence"), &dTmp))
        {
            if(dTmp > 0 && dTmp < 100)
            {
                tsi->dDataValue = dTmp;
                tsi->nDataSelectionType = CONFIDENCE;
                tsi->tsTestModifiers += (TEXT(",") + dtos(tsi->dDataValue) + TEXT("-Confidence"));
            }
            else
            {
                info(FAIL, TEXT("The confidence interval %f is invalid. Should be between 0 and 100"), dTmp);
                bRval = FALSE;
            }

            // max run time only applies to confidence interval runs
            if(m_SectionList->GetDouble(TEXT("MaxRunTime"), &dTmp))
            {
                if(dTmp > 0)
                {
                    tsi->dMaxMinutesToRun = dTmp;
                    tsi->tsTestModifiers += (TEXT(",") + dtos(tsi->dMaxMinutesToRun) + TEXT("-MaxRunTime"));
                }
                else
                {
                    info(FAIL, TEXT("The maximum number of minutes to run should be greater than 0."), dTmp);
                    bRval = FALSE;
                }
            }
        }
        // since the data value is stored as a double, we'll just retrieve
        // the value as a double and truncate off any ending bits in case
        // the user said they want .x runs.
        else if(m_SectionList->GetDouble(TEXT("Iterations"), &dTmp))
        {
            if(dTmp > 0)
            {
                tsi->dDataValue = (double) ((DWORD) dTmp);
                tsi->nDataSelectionType = COUNT;
                tsi->tsTestModifiers += TEXT(",");
                tsi->tsTestModifiers += itos((int) tsi->dDataValue);
                tsi->tsTestModifiers += TEXT("-Iterations");
            }
            else
            {
                info(FAIL, TEXT("The count %d requested is invalid."), (int) dTmp);
                bRval = FALSE;
            }
        }

        if(m_SectionList->GetDouble(TEXT("Priority"), &dTmp))
        {
            if(dTmp >= 0 && dTmp <= 7)
            {
                tsi->nPriority = (int) dTmp;
                tsi->tsTestModifiers += TEXT(",");
                tsi->tsTestModifiers += TEXT("Priority-");
                tsi->tsTestModifiers += itos((int)tsi->nPriority);
            }
            else
            {
                info(FAIL, TEXT("Invalid thread priority - %d. Must be between 0 and 7."), (int) dTmp);
                bRval = FALSE;
            }
        }


        // else default to what the Benchmark Engine initialized the test to use.
// cache flushing only applies to CE.
#ifdef UNDER_CE
        if(m_SectionList->GetDouble(TEXT("FlushICache"), &dTmp))
        {
            if(dTmp == 1.0)
            {
                tsi->dwCacheRangeFlush |= CACHE_SYNC_INSTRUCTIONS;
                tsi->tsTestModifiers += TEXT(",");
                tsi->tsTestModifiers += TEXT("FlushICache");
            }
            else
            {
                info(FAIL, TEXT("Invalid FlushICache - %d."), (int) dTmp);
                bRval = FALSE;
            }
        }

        if(m_SectionList->GetDouble(TEXT("FlushDCache"), &dTmp))
        {
            if(dTmp == 1.0)
            {
                tsi->tsTestModifiers += TEXT(",");
                tsi->tsTestModifiers += TEXT("FlushDCache");
                tsi->dwCacheRangeFlush |= CACHE_SYNC_DISCARD;
            }
            else
            {
                info(FAIL, TEXT("Invalid FlushDCache - %d."), (int) dTmp);
                bRval = FALSE;
            }
        }
#endif
    }
    else
    {
        info(FAIL, TEXT("CTestSuite::Initialize() called with NULL test suite info or null section list."), (int) dTmp);
        bRval = FALSE;
    }

    return bRval; 
}

BOOL
CTestSuite::PreRun(TestInfo *)
{
    info(DETAIL, TEXT("In CTestSuite PreRun"));

    return TRUE;
}

BOOL
CTestSuite::Run()
{
    // logging anything in here causes problems with the calibration.
    return TRUE;
}

BOOL
CTestSuite::AddPostRunData(TestInfo *)
{
    info(DETAIL, TEXT("In CTestSuite AddPostRunData"));

    return TRUE;
}

BOOL
CTestSuite::PostRun()
{
    info(DETAIL, TEXT("In CTestSuite PostRun"));

    return TRUE;
}

BOOL
CTestSuite::Cleanup()
{
    info(DETAIL, TEXT("In CTestSuite Cleanup"));

    return TRUE;
}

