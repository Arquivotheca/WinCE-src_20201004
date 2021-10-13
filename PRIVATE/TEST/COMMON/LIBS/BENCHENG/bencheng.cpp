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
#include "bencheng.h"
#include "utilities.h"

static double g_CalibrationDeviation;
static LARGE_INTEGER g_CalibrationWidth;

// make sure the compiler doesn't optimize this call out, because we're timing an empty function call
// for calibration.
#pragma optimize("", off)
void
CalibrateBenchmarkEngine()
{
    CTestSuite cTest;
    LARGE_INTEGER liStart, liEnd, liDiff;
    LARGE_INTEGER liMax, liMin;
    int CollectionLoopCount = 0;
    double dSumOfSquares = 0;
    double dSumToSquare = 0;

    liMax.QuadPart = 0;
    liMin.QuadPart = LONG_MAX;

    for(CollectionLoopCount = 0; CollectionLoopCount < CALIBRATION_CYCLES; CollectionLoopCount++)
    {
        Sleep(0);
        QueryPerformanceCounter(&liStart);
        if(!cTest.Run())
        {
            // do nothing because this won't ever happen, the base class always returns true.
            // this wont be optimized out because optimizations are turned off for this function.
        }

        QueryPerformanceCounter(&liEnd);

        // track the sum of squares and sum to calculate the standard deviation later.
        liDiff.QuadPart = liEnd.QuadPart - liStart.QuadPart;
        dSumOfSquares += (liDiff.QuadPart * liDiff.QuadPart);
        dSumToSquare += liDiff.QuadPart;

        // update the max and min.
        if(liDiff.QuadPart > liMax.QuadPart)
            liMax.QuadPart = liDiff.QuadPart;
        if(liDiff.QuadPart < liMin.QuadPart)
            liMin.QuadPart = liDiff.QuadPart;

    }

    // the sample variance is  -  s^2 = (sum(xi - mean)^2) / n - 1
    // The numerator of the variance can be expanded to sum(x^2) - (sum(x)^2)/n
    // thus, variance = (sum(x^2) - (sum(x)^2)/n) / (n - 1)
    // stddevation = sqrrt(variance)
    double dSumOfSquaredDeviations = (dSumOfSquares - ((dSumToSquare * dSumToSquare)/CollectionLoopCount));
    double dVariance =  dSumOfSquaredDeviations / (CollectionLoopCount - 1);
    g_CalibrationDeviation = sqrt(dVariance);
    g_CalibrationWidth.QuadPart = WIDTH_MULTIPLIER * (liMax.QuadPart - liMin.QuadPart);

    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("Calibration complete, Deviation %f Width %d"), g_CalibrationDeviation, g_CalibrationWidth.QuadPart);
}

#pragma optimize("", on)

BOOL
InitTestSuite(std::list<CSection *> *SectionList, std::list<CTestSuite *> *TestList)
{
    BOOL bRVal = TRUE;

    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In InitTestSuite"));

    std::list<CSection *>::iterator itr;

    for(itr = SectionList->begin(); itr != SectionList->end(); itr++)
    {
        TSTRING tsName = (*itr)->GetName();
        CTestSuite * TestHolder = NULL;

        g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("Found a section named %s, looking for entry."), tsName.c_str());

        // call the tests initialization function
        TestHolder = CreateTestSuite(tsName, *itr);

        // add it to the list of tests to run.
        if(NULL != TestHolder )
            TestList->push_back(TestHolder);
        else
        {
            g_pCOtakLog->Log(OTAK_ERROR, TEXT("No section named %s found.."), tsName.c_str());
            bRVal = FALSE;
        }
    }

    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("%d test cases found in the %d sections"), TestList->size(), SectionList->size());
    return bRVal;
}

BOOL
RunTestSuite(std::list<CTestSuite *> *TestList)
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In RunTestSuite"));
    BOOL bRVal = TRUE;
    std::list<CTestSuite *>::iterator itr;
    LARGE_INTEGER liStart, liEnd, liDiff, liFrequency, liTicksPerMinute;
    LARGE_INTEGER liMax, liMin;
    TestInfo tiTestEntryInfo;
    TestSuiteInfo tsiSuiteInfo;
    LARGE_INTEGER liSumOfSquares, liSumToSquare;
    TSTRING tsTmp;
    double dFrequencyMultiplier;
    HANDLE hThread;
    int nBasePriority;

    hThread = GetCurrentThread();
    nBasePriority = GetThreadPriority(hThread);

    // get the performance counter frequency in ticks per second.
    // if this fails, then it defaults to a GetTickCount implementation which has 1000 ticks per second.
    if(!QueryPerformanceFrequency(&liFrequency))
        liFrequency.QuadPart = 1000;

    // all calculations are kept in the QPC's resolution until they're outputted to the user.
    // the multiplier converts the result to Microseconds for output to the user.
    dFrequencyMultiplier = 1000000. / liFrequency.QuadPart;
    // the frequency is the # of ticks per second, multiply by 60 to get the ticks in a second for dividing later.
    liTicksPerMinute.QuadPart = liFrequency.QuadPart * 60;

    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("PerformanceCounter frequency is %d"), liFrequency.QuadPart);
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("Multiplier is %f"), dFrequencyMultiplier);
    g_pCOtakResult->Log(OTAK_RESULT, TEXT("Timing data is in Microseconds."));

    // Loop until complete
    for(itr = TestList->begin(); itr != TestList->end(); itr++)
    {
        // default to an 80% confidence interval test.
        tsiSuiteInfo.dDataValue = 75;
        tsiSuiteInfo.nDataSelectionType = CONFIDENCE;
        tsiSuiteInfo.dMaxMinutesToRun = DEFAULTMAXRUNTIME;
        tsiSuiteInfo.dwCacheRangeFlush = 0;
        tsiSuiteInfo.nPriority = THREAD_PRIORITY_ABOVE_NORMAL;
        tsiSuiteInfo.tsTestName = TEXT("");
        tsiSuiteInfo.tsTestModifiers= TEXT("");

        // Clear out the field descriptions for the new test
        while(!tsiSuiteInfo.tsFieldDescription.empty())
            tsiSuiteInfo.tsFieldDescription.pop_front();    

        // initialize the base class to configure the global script options
        if(!(*itr)->CTestSuite::Initialize(&tsiSuiteInfo))
        {
            g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to initialize test"));
            bRVal = FALSE;
        }
        // initilize the actual test case
        else if(!(*itr)->Initialize(&tsiSuiteInfo))
        {
            g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to initialize test"));
            bRVal = FALSE;
        }
        // only run the test if initialize succeeded.
        else
        {
            if(!SetThreadPriority(hThread, tsiSuiteInfo.nPriority))
            {
                g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to set thread priority."));
                bRVal = FALSE;
            }

            // the order here must match the order the data is outputted below.
            tsiSuiteInfo.tsFieldDescription.push_back(TEXT("SampleCount"));
            tsiSuiteInfo.tsFieldDescription.push_back(TEXT("Min"));
            tsiSuiteInfo.tsFieldDescription.push_back(TEXT("Max"));
            tsiSuiteInfo.tsFieldDescription.push_back(TEXT("Mean"));
            tsiSuiteInfo.tsFieldDescription.push_back(TEXT("Std Deviation"));
            tsiSuiteInfo.tsFieldDescription.push_back(TEXT("CV%"));

            g_pCOtakResult->Log(OTAK_RESULT, TEXT("Test,%s,%s"), tsiSuiteInfo.tsTestName.c_str(), tsiSuiteInfo.tsTestModifiers.c_str());

            // clear out the combination information for the new test case.
            while(!tiTestEntryInfo.Descriptions.empty())
                tiTestEntryInfo.Descriptions.pop_front();    

            if(!ListToCSVString(&(tsiSuiteInfo.tsFieldDescription), &tsTmp))
            {
                g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to parse Field description list into the CSV format."));
                bRVal = FALSE;
            }

            g_pCOtakResult->Log(OTAK_RESULT, TEXT("%s"), tsTmp.c_str());

            do
            {

                if(!(*itr)->PreRun(&tiTestEntryInfo) && bRVal)
                {
                    g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to set up the next test case."));
                    bRVal = FALSE;
                }

                // this is used to count the number of collections made
                int CollectionLoopCount = 0;
                BOOL bComplete = FALSE;
                liSumOfSquares.QuadPart = 0;
                liSumToSquare.QuadPart = 0;
                liMax.QuadPart = 0;
                liMin.QuadPart = LONG_MAX;
                double dSumOfSquaredDeviations = 0;
                double dVariance = 0;
                double dStdDeviation = 0;
                double dMean = 0;
                int nConfidenceLoopCount = 0;
                BOOL bConfidenceSet = FALSE;

                do
                {
                    // if the script requested a cache flush, do it before each test call.
                    // cache flushing only applies to CE.
#ifdef UNDER_CE
                    if(tsiSuiteInfo.dwCacheRangeFlush)
                    {
                        CacheSync(tsiSuiteInfo.dwCacheRangeFlush);
                    }
#endif
                    // allow any other processes to run, so they don't preempt the test.
                    Sleep(0);

                    QueryPerformanceCounter(&liStart);
                    if(!(*itr)->Run())
                    {
                        g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to run test"));
                        bRVal = FALSE;
                        break;
                    }
                    QueryPerformanceCounter(&liEnd);

                    liDiff.QuadPart = liEnd.QuadPart - liStart.QuadPart;
                    liSumOfSquares.QuadPart += (liDiff.QuadPart * liDiff.QuadPart);

                    // if the double goes below 0, then we have an overflow
                    if(liSumOfSquares.QuadPart < 0)
                    {
                        g_pCOtakLog->Log(OTAK_ERROR, TEXT("Overflow of running sum of squares, terminating test."));
                        bRVal = FALSE;
                        break;
                    }
                    liSumToSquare.QuadPart += liDiff.QuadPart;

                    if(liDiff.QuadPart > liMax.QuadPart)
                        liMax.QuadPart = liDiff.QuadPart;
                    if(liDiff.QuadPart < liMin.QuadPart)
                        liMin.QuadPart = liDiff.QuadPart;

                    // store the result of that test run

                    // We've successfully made a collection
                    CollectionLoopCount++;

                    if(tsiSuiteInfo.nDataSelectionType == COUNT)
                    {
                        // if we're on the first run, tell the user how many cycles we're going to do.
                        if(CollectionLoopCount == 1)
                            g_pCOtakLog->Log(OTAK_DETAIL, TEXT("Sampling %d cycles"), (DWORD) tsiSuiteInfo.dDataValue);

                        if(CollectionLoopCount >= tsiSuiteInfo.dDataValue)
                            bComplete = TRUE;
                    }
                    else if(CONFIDENCE == tsiSuiteInfo.nDataSelectionType)
                    {
                        if(!bConfidenceSet && CollectionLoopCount >= MIN_SAMPLE_SIZE)
                        {
                            // since this is a difference calculation, a LONG LONG isn't needed.
                            dSumOfSquaredDeviations = (double) (liSumOfSquares.QuadPart - ((liSumToSquare.QuadPart * liSumToSquare.QuadPart)/CollectionLoopCount));
                            dVariance =  dSumOfSquaredDeviations / (CollectionLoopCount - 1);
                            dStdDeviation = sqrt(dVariance);

                            // NOTENOTE: Get the real zeta based on the distribution.
                            // 100-requested value to get the area outside of the requested curve area
                            // divide by 100 to get percentage
                            // divide by 2 to split interval on both sides of standard curve
                            // ex. 95% ci would be a area of .025.
                            double ZetaSubAlphaOverTwo = NormDistInv((100-tsiSuiteInfo.dDataValue)/200);
                            double count = pow(2*ZetaSubAlphaOverTwo * (dStdDeviation/g_CalibrationWidth.QuadPart), 2);
                            double EstimatedRunTime;

                            // round up
                            if((count - (int) count) > 0)
                                count += 1;

                            // Estimated run time in minutes is the sample number * sample mean / performance counter ticks per minute
                            EstimatedRunTime = (count * (liSumToSquare.QuadPart/CollectionLoopCount)) / liTicksPerMinute.QuadPart;

                            if(EstimatedRunTime > tsiSuiteInfo.dMaxMinutesToRun)
                            {
                                int nOldCount = (int) count;
                                // the new count is the max # of performance counter ticks we are willing to run divided by the mean run time per cycle.
                                count = (liTicksPerMinute.QuadPart * tsiSuiteInfo.dMaxMinutesToRun) / (liSumToSquare.QuadPart/CollectionLoopCount);
                                if((count - (int) count) > 0)
                                    count += 1;
                                EstimatedRunTime = (count * (liSumToSquare.QuadPart/CollectionLoopCount)) / (liTicksPerMinute.QuadPart * 60);
                                g_pCOtakLog->Log(OTAK_DETAIL, TEXT("Max Run time exceded, sample count adjusted from %d to %d."), nOldCount, (int) max(count, MIN_SAMPLE_SIZE));
                            }

                            nConfidenceLoopCount = (int) count;
                            bConfidenceSet = TRUE;

                            g_pCOtakLog->Log(OTAK_DETAIL, TEXT("Sampling %d cycles, sample deviation was %f, estimated run time %d'%d\""), 
                                                                max(nConfidenceLoopCount, MIN_SAMPLE_SIZE), dStdDeviation, (int) EstimatedRunTime, ((int) (EstimatedRunTime * 60)) % 60);

                        }
                        if(bConfidenceSet && CollectionLoopCount > nConfidenceLoopCount)
                            bComplete = TRUE;
                    }
                }while(!bComplete);

                // only add post run data if the test run succeeded.
                // otherwise there could be issues of uninitialized variables from breaking out
                // of the sampling above.
                if(bRVal)
                {

                    // the sample variance is  -  s^2 = (sum(xi - mean)^2) / n - 1
                    // The numerator of the variance can be expanded to sum(x^2) - (sum(x)^2)/n
                    // thus, variance = (sum(x^2) - (sum(x)^2)/n) / (n - 1)
                    // stddevation = sqrrt(variance)
                    // since this is a difference calculation, a LONG LONG isn't needed.
                    dSumOfSquaredDeviations = (double) (liSumOfSquares.QuadPart - ((liSumToSquare.QuadPart * liSumToSquare.QuadPart)/CollectionLoopCount));
                    if(CollectionLoopCount > 1)
                    {
                        dVariance =  dSumOfSquaredDeviations / (CollectionLoopCount - 1);
                        dStdDeviation = sqrt(dVariance);
                    }

                    dMean = (double) liSumToSquare.QuadPart / CollectionLoopCount;
                    // store the final results with the test description for output.

                    if(!(*itr)->AddPostRunData(&tiTestEntryInfo))
                    {
                        g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to add post test data"));
                        bRVal = FALSE;
                    }

                    // The order below must match the order above that the descriptions were
                    // put in.
                    tiTestEntryInfo.Descriptions.push_back(itos(CollectionLoopCount));
                    // compensate for the performance counters resolution, our base is microseconds.
                    tiTestEntryInfo.Descriptions.push_back(dtos(liMin.QuadPart * dFrequencyMultiplier));
                    tiTestEntryInfo.Descriptions.push_back(dtos(liMax.QuadPart * dFrequencyMultiplier));
                    tiTestEntryInfo.Descriptions.push_back(dtos(dMean * dFrequencyMultiplier));
                    tiTestEntryInfo.Descriptions.push_back(dtos(dStdDeviation * dFrequencyMultiplier));
                    // coefficient of Variation (cv%) is the ((std deviation)/(mean))*100
                    tiTestEntryInfo.Descriptions.push_back(dtos(((dStdDeviation * dFrequencyMultiplier)/(dMean * dFrequencyMultiplier))*100));

                    // if the sizes are wrong, then output an error.
                    if(tiTestEntryInfo.Descriptions.size() != tsiSuiteInfo.tsFieldDescription.size())
                    {
                        g_pCOtakLog->Log(OTAK_ERROR, TEXT("The number of data entries doesn't match the number of column descriptions."));
                        bRVal = FALSE;
                    }
                    // if the sizes were right, but we failed to parse it, then output an error.
                    else if(!ListToCSVString(&(tiTestEntryInfo.Descriptions), &tsTmp))
                    {
                        g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to parse Test result list into the CSV format."));
                        bRVal = FALSE;
                    }
                    // the size was right, the string was parsed, so we can output our results.
                    else g_pCOtakResult->Log(OTAK_RESULT, TEXT("%s"), tsTmp.c_str());

                    // clear out the combination information for the next test case.
                    while(!tiTestEntryInfo.Descriptions.empty())
                        tiTestEntryInfo.Descriptions.pop_front();
                }
            // loop until the test suite completes.
            }while((*itr)->PostRun() && bRVal);

            // restore thread priority.
            if(!SetThreadPriority(hThread, nBasePriority))
            {
                g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to restore thread priority to original priority."));
                bRVal = FALSE;
            }
        }
        g_pCOtakResult->Log(OTAK_RESULT, TEXT(" "));

        // cleanup for both the bast test suite and the test suite we're running.
        if(!(*itr)->CTestSuite::Cleanup() || !(*itr)->Cleanup())
        {
            g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to cleanup"));
            bRVal = FALSE;
        }
        // output a carriage return between tests so they aren't so cluttered.
        g_pCOtakLog->Log(OTAK_DETAIL, TEXT(" "));
    }
    return bRVal;
}

BOOL
CleanupTestSuite(std::list<CTestSuite *> *TestList)
{
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CleanupTestSuite"));

    while(!TestList->empty())
    {
        delete TestList->front();
        TestList->pop_front();
    }
    // no failure conditions.
    return TRUE;
}

BOOL
BenchmarkEngine(std::list<CSection *> *List)
{
    BOOL bRVal = TRUE;
    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In BenchmarkEngine"));

    std::list<CTestSuite *> TestList;

    // allow the system to stabilize after loading everything.
    Sleep(ONE_SECOND);

    CalibrateBenchmarkEngine();

    // initialize the testlist for use when running the test suite.
    if(!InitTestSuite(List, &TestList))
    {
        // initialization failed, so exit with failure.
        g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to initialize test suite."));
        bRVal = FALSE;
    }
    else        // run the test suites.
    {
        if(!RunTestSuite(&TestList))
        {
            bRVal = FALSE;
            g_pCOtakLog->Log(OTAK_ERROR, TEXT("Test suite execution failed."));
        }
    }

    // cleanup any partially allocated test's.
    CleanupTestSuite(&TestList);

    g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("BenchmarkEngine complete"));

    return bRVal;
}
