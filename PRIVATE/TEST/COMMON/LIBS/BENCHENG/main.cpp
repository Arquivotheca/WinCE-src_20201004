//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "clparse.h"
#include "utilities.h"

COtak *g_pCOtakLog=NULL;
COtak *g_pCOtakResult=NULL;

BOOL
ParseCommandLine(LPCTSTR CommandLine, TSTRING * tsFileName)
{
    CClParse cCLPparser(CommandLine);

    TCHAR tcFileName[MAX_PATH];
    TCHAR tcModuleName[MAX_PATH];
    BOOL bRval = TRUE;
    DWORD dwVerbosity;
    OtakDestinationStruct ODS;

    if(GetModuleFileName(NULL, tcModuleName, countof(tcModuleName)) < MAX_PATH)
    {

        // start at the first non null character, an emtpy string results in index being less than 0
        // which skips the loop
        int index = _tcslen(tcModuleName) - 1;

        for(; tcModuleName[index] != '\\' && index > 0; index--);

        // if the index is less than 0, then it's an "empty" path.
        // if the index is 0, then it's a single character
        if(index >= 0 && tcModuleName[index] == '\\')
            tcModuleName[index + 1] = NULL;

        // Set up user command line options for the source script file.
        if(cCLPparser.GetOptString(TEXT("S"), tcFileName, MAX_PATH))
            *tsFileName = tcFileName;
        else
        {
            *tsFileName = tcModuleName;
            *tsFileName += DEFAULTSCRIPTFILE;
        }

        // the Result output logger always has a verbosity of 0, since
        // the only thing it's used for is results.
        g_pCOtakResult->SetVerbosity(0);

        if(cCLPparser.GetOptVal(TEXT("v"), &dwVerbosity))
            g_pCOtakLog->SetVerbosity(dwVerbosity);
        else g_pCOtakLog->SetVerbosity(OTAK_DETAIL);

        ODS.dwSize = sizeof(OtakDestinationStruct);

        // Set up the logger for errors/etc
        if(cCLPparser.GetOpt(TEXT("o")))
            ODS.dwDestination = OTAKDEBUGDESTINATION;
        else if(cCLPparser.GetOptString(TEXT("l"), tcFileName, MAX_PATH))
        {
            ODS.dwDestination = OTAKFILEDESTINATION;
            ODS.tsFileName = tcFileName;
        }
        else
        {
            ODS.dwDestination = OTAKFILEDESTINATION;
            ODS.tsFileName = tcModuleName;
            ODS.tsFileName += DEFAULTDEBUGFILE;
        }

        g_pCOtakLog->SetDestination(ODS);

        // Set up the logger for the results.
        if(cCLPparser.GetOptString(TEXT("f"), tcFileName, MAX_PATH))
        {
            ODS.dwDestination = OTAKFILEDESTINATION;
            ODS.tsFileName = tcFileName;
        }
        else if(cCLPparser.GetOpt(TEXT("d")))
            ODS.dwDestination = OTAKDEBUGDESTINATION;
        else
        {
            ODS.dwDestination = OTAKFILEDESTINATION;
            ODS.tsFileName = tcModuleName;
            ODS.tsFileName += DEFAULTLOGFILE;
        }

        g_pCOtakResult->SetDestination(ODS);
    }
    else
    {
        OutputDebugString(TEXT("Failed to retrieve module file name."));
        bRval = FALSE;
    }

    return bRval;
}

int
RunBenchmark(LPCTSTR CommandLine)
{
    TSTRING tsFileName;

    // create a linked list
    std::list<CSection *> SectionList;

    if(CommandLine)
    {
        g_pCOtakLog = new(COtak);
        g_pCOtakResult = new(COtak);

        if(g_pCOtakLog && g_pCOtakResult)
        {
            if(ParseCommandLine(CommandLine, &tsFileName))
            {
                // Otak logging is now available, since ParseCommandLine configured it.
                g_pCOtakLog->Log(OTAK_REQUIRED, TEXT("Test started, file name %s"), tsFileName.c_str());

                if(InitializeSectionList(tsFileName, &SectionList))
                {
                    if(!BenchmarkEngine(&SectionList))
                        g_pCOtakLog->Log(OTAK_ERROR, TEXT("Benchmark run failed."));
                }
                else g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to initialize the section list.  Exiting."));

                // whether or not Section List initialization succeeded, we need to clean up anything it may have allocated.
                FreeSectionList(&SectionList);

                g_pCOtakLog->Log(OTAK_REQUIRED, TEXT("Test suit complete"));
            }
            else OutputDebugString(TEXT("Failed to parse command line."));
        }
        else OutputDebugString(TEXT("Failed to create the Otak logging engine."));

        if(g_pCOtakLog)
            delete g_pCOtakLog;
        if(g_pCOtakResult)
            delete g_pCOtakResult;
    }
    else OutputDebugString(TEXT("RunBenchmark given an invalid command line."));

    return 0;
}
