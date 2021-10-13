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


// ***************************************************************************************************************
//
// This is a shim that interfaces TuxMain to the main lower-level MemoryPerf fTests 
// Main issue this shim solves is to allocate and deallocate buffers for tux's individual test runs 
// in contrast to the simple exe which allocates and deallocates a single time.
//
// ***************************************************************************************************************


// ********************************************************************************************************
// Instructions:
// 1.  install the makefile, sources and these program source files into a directory
// 2.  open a build (retail or ship) build window
// 3.  build -c
//     That will create a signed copy of memoryperf.dll in the flat release directory
// 4.  Boot the device with KITL attached
// 5.  in target command window, type:
//     s tux -o -d MemoryPerf.dll -c"-c -o" -f\release\memoryperf.log
//     tux option -x1 => run test 1 (cache), or -x2 (uncached) or -x3 (ddraw surface) or -x4 (scoring).
// The output log is MemoryPerf.log and contains key results and warnings.
// PerfScenario output is in MemoryPerf_test.xml
// Details will be sent to an Excel file with the -c or -o option shown above which will be in \release\MemoryPerf.csv
//
// To debug this code in a retail or ship window, set ENABLE_OPTIMIZER=0 & set COMPILE_DEBUG=1
// The key tests loops will continue to be optimized - see the #pragma at the beginning of the *Loop?D.cpp files.
//
// You will notice results have less variance when the device has been freshly booted - this is due 
// to the tendency to malloc sequential physical memory when the device is freshly booted and malloc a complex 
// mapping of physical pages after lots of stuff has been running.
// The use of the Median rather than the Mean for the primary results allows us to report stable results in spite
// of this high variance.  However, some result will be cleaner and more reliable right after a boot.
// ********************************************************************************************************

#include "MemoryPerf.h"
#include "globals.h"


// ********************************************************************************************************
// The following functions wrap the raw MemoryPerf functions from MemoryPerf.cpp so Tux can execute them.
// ********************************************************************************************************


// ********************************************************************************************************
// CachedMemoryPerfTest() wraps CachedMemoryPerf into a tux callable function
// ********************************************************************************************************

TESTPROCAPI CachedMemoryPerfTest(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /* lpFTE */)
{
    // The shell doesn't necessarily want us to execute the test. Make sure first
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
          
    DWORD dwTickStart = GetTickCount();

    if (!MemPerfBlockAlloc( true, false ))
    {
         g_pKato->Log(LOG_FAIL, _T("Error allocating cached buffers."));
         MemPerfBlockFree();
         return TPR_FAIL;
    }
    
    g_pKato->Log(LOG_DETAIL,("starting CachedMemoryPerfTest"));

    bool fRet = CachedMemoryPerf();

    // ************************************************************************
    MemPerfBlockFree();
    DWORD dwTickStop = GetTickCount();
    LogPrintf( "Cached Memory Test took %d ms of which %d ms was in test loops.\r\n", 
                dwTickStop-dwTickStart,
                (DWORD)((gtGlobal.gdTotalTestLoopUS+500)/1000)
            );

    if ( ! fRet )
    {
        g_pKato->Log(LOG_FAIL,_T("Error measuring Cached Memory Performance."));
    }
    return fRet ? TPR_PASS : TPR_FAIL;
}

// ********************************************************************************************************
// UnCachedMemoryPerfTest() wraps UnCachedMemoryPerf into a tux callable function
// ********************************************************************************************************

TESTPROCAPI UnCachedMemoryPerfTest(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /* lpFTE */)
{
    // The shell doesn't necessarily want us to execute the test. Make sure first
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwTickStart = GetTickCount();

    if (!MemPerfBlockAlloc( false, true))
    {
         g_pKato->Log(LOG_FAIL,_T("Error allocating uncached buffers."));
         MemPerfBlockFree();
         return TPR_FAIL;
    }
    
    g_pKato->Log(LOG_DETAIL,("starting UnCachedMemoryPerfTest"));

    bool fRet = UnCachedMemoryPerf();

    MemPerfBlockFree();
    DWORD dwTickStop = GetTickCount();
    LogPrintf( "UnCached Memory Test took %d ms of which %d ms was in test loops.\r\n", 
                dwTickStop-dwTickStart,
                (DWORD)((gtGlobal.gdTotalTestLoopUS+500)/1000)
            );

    if ( ! fRet )
    {
        g_pKato->Log(LOG_FAIL,_T("Error measuring Uncached Memory Performance."));
    }
    return fRet ? TPR_PASS : TPR_FAIL;
}


// ********************************************************************************************************
// DDrawMemoryPerfTest() wraps DDrawMemoryPerf into a tux callable function
// ********************************************************************************************************

TESTPROCAPI DDrawMemoryPerfTest(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /* lpFTE */)
{
    // The shell doesn't necessarily want us to execute the test. Make sure first
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwTickStart = GetTickCount();

    if ( !MemPerfBlockAlloc( false, false ) )
    {
         g_pKato->Log(LOG_FAIL,_T("Error allocating DDraw buffers."));
         MemPerfBlockFree();
         return TPR_FAIL;
    }

    BOOL fRet = DDrawMemoryPerf();

    MemPerfBlockFree();
    DWORD dwTickStop = GetTickCount();
    LogPrintf( "DDraw Surface Memory Test took %d ms of which %d ms was in test loops.\r\n", 
                dwTickStop-dwTickStart,
                (DWORD)((gtGlobal.gdTotalTestLoopUS+500)/1000)
            );

    if ( ! fRet )
    {
        g_pKato->Log(LOG_FAIL,_T("Error measuring DDraw Surfaces."));
    }
    return fRet ? TPR_PASS : TPR_FAIL;
}

// ********************************************************************************************************
// OverallMemoryPerfTest() wraps ScoreMemoryPerf into a tux callable function 
// whose purpose is to calculate the overall score using the measurements collected in the three tests
// CachedMemoryPerf, UnCachedMemoryPerf, and DDrawMemoryPerf
// of course, if those tests do not previously run properly, we'll retry running them here.
// ********************************************************************************************************

TESTPROCAPI    OverallMemoryPerfTest(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /* lpFTE */)
{
    // The shell doesn't necessarily want us to execute the test. Make sure first
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwTickStart = GetTickCount();

    if ( !MemPerfBlockAlloc( true, true ) )
    {
         g_pKato->Log(LOG_FAIL,_T("Error allocating buffers."));
         MemPerfBlockFree();
         return TPR_FAIL;
    }
    
    if ( !gtGlobal.bCachedMemoryTested )
    {
        // main cached memory test
        CachedMemoryPerf();
    }

    if ( gtGlobal.bUncachedTest && !gtGlobal.bUncachedMemoryTested )
    {
        // main uncached memory test
        UnCachedMemoryPerf();
    }

    if ( gtGlobal.bVideoTest && !gtGlobal.bDDrawMemoryTested )
    {
        // DDraw surface test
        DDrawMemoryPerf();
    }

    // ********************************************************************************************************
    // calculate an overall performance score
    // ********************************************************************************************************

    bool fRet = ScoreMemoryPerf();

    MemPerfBlockFree();
    DWORD dwTickStop = GetTickCount();
    LogPrintf( "Scoring Memory Test took %d ms of which %d ms was in test loops.\r\n", 
                dwTickStop-dwTickStart,
                (DWORD)((gtGlobal.gdTotalTestLoopUS+500)/1000)
            );

    if ( ! fRet )
    {
        g_pKato->Log(LOG_FAIL,_T("Error measuring Scoring MemoryPerf."));
    }
    return fRet ? TPR_PASS : TPR_FAIL;
}
