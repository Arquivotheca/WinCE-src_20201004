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
// These functions organize the top level testing of memory performance
// as such, they are composed of collections of sergeant patterns which test individual parts
// and gradually collect the information on the whole system
//
// ***************************************************************************************************************


// ***************************************************************************************************************
//
// Instructions:
// 1.  install the makefile, sources and these program source files into a directory
// 2.  open a build (retail or ship) build window
// 3.  build -c
//     That will create a signed copy of memoryperf.dll in the flat release directory
// 4.  Boot the device with KITL attached
// 5.  For tux version, in target command window, type:
//     s tux -o -d MemoryPerf.dll -c"-c -o -p" -f\release\memoryperf.log
//     tux option -x1 => run test 1 (cache), or -x2 (uncached) or -x3 (ddraw surface).
//     The output log is MemoryPerf.log and contains key results and warnings.
//     Details will be sent to an Excel file with the -c or -o option shown above which will be in \release\MemoryPerf.csv
//     PerfScenario output is in MemoryPerf_test.xml
// 6.  For exe verson, in target command window, type:
//     s MemoryPerfExe.exe -c -o -d
//     The output log is \release\MemoryPerfExe.log and there is no PerfScenario output
//     Details will be sent to an Excel file with the -c or -o option shown above which will be in \release\MemoryPerf.csv
//
// To debug this code in a retail or ship window, set ENABLE_OPTIMIZER=0 & set COMPILE_DEBUG=1
// The key test-loops will continue to be optimized - see the #pragma at the beginning of the *Loop*D.cpp files.
//
// You will notice results have less variance when the device has been freshly booted - this is due 
// to the tendency to malloc sequential physical memory when the device is freshly booted and malloc a complex 
// mapping of physical pages after lots of stuff has been running.
// The use of the Median rather than the Mean for the primary results allows us to report stable results in spite
// of this high variance.
// 
// See MemoryPerf.docx for more documentation
//
// ***************************************************************************************************************

#include "MemoryPerf.h"
#include "globals.h"
#include "Latency.h"
#include "EffectiveSize.h"
#include "DDrawSurface.h"
#include "cache.h"

#pragma warning( disable : 6262 )   // allow large stack due to these test functions wanting to hold a large number of objects on the stack.

bool bApproximatelyEqual( const DWORD cdw1, const DWORD cdw2, const float cfTol );
bool bMuchLessThan( const DWORD cdw1, const DWORD cdw2, const float cfHowMuch );
bool bApproximatelyLessThan( const DWORD cdw1, const DWORD cdw2, const float cfTol );
void WarmUpCPU( const DWORD *pdwBase, const int ciMilliSeconds );
void TestCPULatency( int * piGrandTotal );

const int ciAssumedLineSize = 64;
const tStridePattern ctspCPULatency = { eCPULoop, 1, 1 };
const float ciTolerance = 0.05f;            // 5% Tolerance used for bApproximatelyEqual() and bApproximatelyLessThan()

//-----------------------------------------------------------------------------

#ifdef SIMULATE
    tStridePattern gtspThreshold = {eTopTest, -1, -1, -1, -1 };
    #pragma message( __FILE__ "(33) : warning C4000: SIMULATE defined : use only for Dev testing on DE." )
#endif


// ***************************************************************************************************************
// 
// Main Cached Memory Performance Tests - CachedMemoryPerf
//
// ***************************************************************************************************************

extern bool CachedMemoryPerf(void)
{
    bool fRet = true;

    int iGrandTotal = 0;    // Keep very clever optimizing compilers at bay from trivializing the tests.

#if defined(VERIFY_OPTIMIZED)
    {
        // ****************************************************************************************
        // Sequential L1 Latency (used to evaluate the instruction execution quality of the low-level 1D loops)
        // The 1D pattern executes at 255 loop16 + 2 loop4 + 2 Loop1
        // ****************************************************************************************
        const tStridePattern ctspL1SeqLatency = { eReadLoop1D, (16*ci1K-24)/sizeof(DWORD), 1 };
        tLatency tL1SeqLatency = tLatency( gtGlobal.iL1LineSize, ctspL1SeqLatency, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
        tL1SeqLatency.SetExpected(1);
        tL1SeqLatency.Measure( gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
        tL1SeqLatency.LogReport( "Sequential L1" );
        PerfReport( _T("Seq L1 Read ps"), 10*tL1SeqLatency.dwLatency100() );
        if ( 0 == tL1SeqLatency.dwLatency100() ) fRet = false;

        // ****************************************************************************************
        // Sequential L1 2D Latency (used to evaluate the instruction execution quality of the low-level 2D loop)
        // first of the 2D patterns uses the inner loop wisely and the outer loop only a little
        // ****************************************************************************************
        const int ciXBytes = 488;       // 122 DWORDS will get all the inner loops executed 7,2,2 times.
        const tStridePattern ctspL1TDSeqLatency = { eReadLoop2D, 16*ci1K/ciXBytes, ciXBytes/sizeof(DWORD), ciXBytes/sizeof(DWORD), 1 };
        tLatency tL1TDSeqLatency = tLatency( gtGlobal.iL1LineSize, ctspL1TDSeqLatency, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
        tL1TDSeqLatency.SetExpected(1);
        tL1TDSeqLatency.Measure( gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
        tL1TDSeqLatency.LogReport( "Seq L1 2D X" );
        PerfReport( _T("Seq L1 2D X Read ps"), 10*tL1TDSeqLatency.dwLatency100() );
        if ( 0 == tL1TDSeqLatency.dwLatency100() ) fRet = false;

        // ****************************************************************************************
        // Sequential L1 2D Latency (used to evaluate the instruction execution quality of the low-level 2D loop)
        // Second of the 2D patterns uses the inner loop only briefly so maximizes the outer loop overhead
        // ****************************************************************************************
        const int ciYBytes = 112;       // 122 DWORDS will get all the inner loops executed 1,2,2 times.
        const tStridePattern ctspL1UDSeqLatency = { eReadLoop2D, 16*ci1K/ciYBytes, ciYBytes/sizeof(DWORD), ciYBytes/sizeof(DWORD), 1 };
        tLatency tL1UDSeqLatency = tLatency( gtGlobal.iL1LineSize, ctspL1UDSeqLatency, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
        tL1UDSeqLatency.SetExpected(1);
        tL1UDSeqLatency.Measure( gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
        tL1UDSeqLatency.LogReport( "Seq L1 2D Y" );
        PerfReport( _T("Seq L1 2D Y Read ps"), 10*tL1UDSeqLatency.dwLatency100() );
        if ( 0 == tL1UDSeqLatency.dwLatency100() ) fRet = false;
    }
#endif

    // ****************************************************************************************
    // order of tests assumes 1MB section mapped memory is not available.
    // result of the tests which do not need section mapped memory will be used to 
    // correct the results of the tests which need section mapped memory but will be operating on page mapped memory.
    // ****************************************************************************************

    // ****************************************************************************************
    // L1 Latency
    // ****************************************************************************************
    const int ciL1AssumedLineSize = max( ciAssumedLineSize, gtGlobal.iL1LineSize );
    const tStridePattern ctspL1Latency = { eReadLoop1D, 16*ci1K/ciL1AssumedLineSize, ciL1AssumedLineSize/sizeof(DWORD) };
    tLatency tL1Latency = tLatency( ciL1AssumedLineSize, ctspL1Latency );
    tL1Latency.Measure( gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
    tL1Latency.LogReport( "L1" );
    PerfReport( _T("L1 Read Latency ps"),  10*tL1Latency.dwLatency100() );
    gtGlobal.dwL1Latency = tL1Latency.dwLatency100();
    if ( 0 == tL1Latency.dwLatency100() ) fRet = false;
    

    // ****************************************************************************************
    // Page Walk Latency
    // iStrides must be larger than largest set of TLB registers (72)
    // Stride distance is a cache line size larger than a page to avoid aliasing issues
    // ****************************************************************************************
    const int ciL2AssumedLineSize = max( ciL1AssumedLineSize, gtGlobal.iL2LineSize );
    const tStridePattern ctspPWLatency = { eReadLoop1D, 125, (ciPageSize + ciL2AssumedLineSize)/sizeof(DWORD) };
    tLatency tPWLatency = tLatency( ciL2AssumedLineSize, ctspPWLatency );
    tPWLatency.Measure( gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
    tPWLatency.LogReport( "Page Walk" );
    PerfReport( _T("Page Walk Latency ps"),  10*tPWLatency.dwLatency100() );
    gtGlobal.dwPWLatency = tPWLatency.dwLatency100();
    if ( 0 == tPWLatency.dwLatency100() ) fRet = false;

    // ****************************************************************************************
    // TLB - Translation Look-aside Buffer
    // Current implementations seem to have 32-72 TLB registers but we expect them to grow to quite large - perhaps 512*4 in a few years
    // First scope range then zoom in
    // ****************************************************************************************
    const int ciTLBExploreStride  = ciPageSize +  ciL2AssumedLineSize;
    const int ciTLBExploreStrides = 4096;
    tStridePattern tspTLBExplore = { eReadLoop1D, ciTLBExploreStrides, ciTLBExploreStride/sizeof(DWORD) };
    tPartition tTLBExploreSize = tPartition( ciL2AssumedLineSize, 16, ciTLBExploreStrides, 1,
                                               ePower2, eStepYStrides, tspTLBExplore, 1 );
    tTLBExploreSize.SetLatency( gtGlobal.dwL1Latency,  gtGlobal.dwPWLatency );
    #ifdef SIMULATE
        gtspThreshold.m_iYStrides = 32;
    #endif
    tTLBExploreSize.EffectiveSize( gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
    CSVMessage( "Ignore cache percentages shown for Wide-Range Exploratory TLB since we don't know enough about the cache yet to model it properly.\r\n" );
    tTLBExploreSize.LogReport( "Wide-Range Exploratory TLB", 1 );
    #ifdef SIMULATE
        gtspThreshold.m_iYStrides = -1;
    #endif

    // ****************************************************************************************
    // To find the effective size of the TLB, we need to stride a page size plus a line size.  
    // the page size forces each access into a new page register.  The line size scatters the accesses 
    // through the L1 cache which typically has aliasing factors similar to the TLB.
    // ****************************************************************************************
    const int ciTLBSizeStride  = ciPageSize + ciL2AssumedLineSize;
    int iTLBMaximumStrides = 128;
    if ( tTLBExploreSize.fHighConfidence()>=0.9f && tTLBExploreSize.iHighSize() > iTLBMaximumStrides ) 
    {   // be cautious of using a very large size as it is a lot of measurements and can be quite slow.
        if ( tTLBExploreSize.fLowConfidence()>=0.9f && tTLBExploreSize.iLowSize() < tTLBExploreSize.iHighSize()/8 )
            iTLBMaximumStrides = max(iTLBMaximumStrides,2*tTLBExploreSize.iLowSize());
        else
            iTLBMaximumStrides = max(iTLBMaximumStrides,tTLBExploreSize.iHighSize());
    }
    tStridePattern tspTLB = { eReadLoop1D, iTLBMaximumStrides, ciTLBSizeStride/sizeof(DWORD) };
    tPartition tTLBEffectiveSize = tPartition( ciL2AssumedLineSize, 4, iTLBMaximumStrides, 4,
                                               eSequential, eStepYStrides, tspTLB, 1 );
    tTLBEffectiveSize.SetLatency( gtGlobal.dwL1Latency,  gtGlobal.dwPWLatency );
    #ifdef SIMULATE
        gtspThreshold.m_iYStrides = 16;
    #endif
    tTLBEffectiveSize.EffectiveSize( gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
    CSVMessage( "Ignore cache percentages shown for TLB since we don't know enough about the cache yet to model it properly.\r\n" );
    tTLBEffectiveSize.LogReport( "TLB", 1 );
    gtGlobal.iTLBSize = tTLBEffectiveSize.iMeasuredSize();
    PerfReport( _T("Effective TLB Size"),  gtGlobal.iTLBSize ); 
    if ( 0 == tTLBEffectiveSize.iMeasuredSize() ) fRet = false;
    #ifdef SIMULATE
        gtspThreshold.m_iYStrides = -1;
    #endif

    if ( gtGlobal.iTLBSize > 125 )
    {
        // We measured the page walk latency with too small of a pattern, so repeat the measurement now
        const tStridePattern ctspPWLatency2 = { eReadLoop1D, gtGlobal.iTLBSize+40, (ciPageSize + ciL2AssumedLineSize)/sizeof(DWORD) };
        tLatency tPWLatency2 = tLatency( ciL2AssumedLineSize, ctspPWLatency2 );
        tPWLatency2.Measure( gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
        tPWLatency2.LogReport( "Page Walk Repeat" );
        PerfReport( _T("Page Walk Latency ps"),  10*tPWLatency2.dwLatency100() );
        gtGlobal.dwPWLatency = tPWLatency2.dwLatency100();
        if ( 0 == tL1Latency.dwLatency100() ) fRet = false;
    }

    // repeat RAW CPU speed test looking for CPU frequency changes.
    TestCPULatency( &iGrandTotal );

    // ****************************************************************************************
    // the following tests ideally should use section mapped memory (page size == 1MB)
    // however, since such memory is highly restricted in user mode by the O/S, 
    // use correction factors from the tests above to make "corrections"
    // 
    // ****************************************************************************************

    // ****************************************************************************************
    // L1 Cache Size and L1 Line Size
    // first find the L1 cache size using this assumed cache line size
    // ****************************************************************************************
    int iL1CacheSize   = 32*ci1K;
    tStridePattern tspL1Size = { eReadLoop1D, 128*ci1K/ciL1AssumedLineSize, ciL1AssumedLineSize/sizeof(DWORD) };
    tPartition tL1Size = tPartition( ciL1AssumedLineSize, 
                                     8*ci1K/ciL1AssumedLineSize,
                                     128*ci1K/ciL1AssumedLineSize,
                                     1,
                                     ePower2, eStepYStrides, tspL1Size, 0 );
    tL1Size.SetPageWalkLatency( gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
    #ifdef SIMULATE
        gtspThreshold.m_iYStrides = 32*ci1K/ciL1AssumedLineSize;
    #endif
    tL1Size.EffectiveSize( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
    CSVMessage( "Ignore cache percentages shown for Preliminary L1 since we don't know enough about the cache yet to model it properly.\r\n" );
    tL1Size.LogReport( "Preliminary L1", ciL1AssumedLineSize );
    if ( 0 == tL1Size.iMeasuredSize() ) fRet = false;
    #ifdef SIMULATE
        gtspThreshold.m_iYStrides = -1;
    #endif
    if ( tL1Size.fLowConfidence() > cfConfidenceThreshold )
    {
        iL1CacheSize = tL1Size.iLowSize()*ciL1AssumedLineSize;
        if ( gtGlobal.bThisCacheInfo && (int)gtGlobal.tThisCacheInfo.dwL1DCacheSize != iL1CacheSize && gtGlobal.bWarnings )
        {
            LogPrintf( "Warning: System reports L1 size as %d but measured %d.\r\n", gtGlobal.tThisCacheInfo.dwL1DCacheSize, iL1CacheSize );
        }
    }
    else if (gtGlobal.bWarnings)
    {
        LogPrintf( "Warning: Preliminary L1 Size results ignored due to low confidence %.3f\r\n", tL1Size.fLowConfidence() );
    }

    // ****************************************************************************************
    // determining Line Size turns out to be a little tricky
    // 2D variation ( alternating small-stride followed by big stride ) did not show any clarity in results.
    // Using 1D, we stride from 12 bytes (smaller than the smallest line size) to 72 bytes (larger than the largest line size)
    // We will span somewhat larger than L1 but less than L2 and hopefully less than the any page table limitations.
    // When the step size is below the line size, we get faster average access times (close to L1) 
    // When the step size matches or exceeds the line size, we get slower average access time (close to L2)
    // ****************************************************************************************
    int iL1LineSize = (gtGlobal.bThisCacheInfo && gtGlobal.tThisCacheInfo.dwL1ICacheLineSize>0) ? gtGlobal.tThisCacheInfo.dwL1ICacheLineSize  : 32;
    const int ciLineSizeSpan = (6*iL1CacheSize)/2; // typically 96k to protect from large L1 way sizes.
    const int ciL1LineSizeStep = 2;
    tStridePattern tspLineSize = { eReadLoop1D, ciLineSizeSpan/(12/sizeof(DWORD)), 12/sizeof(DWORD) };
    tPartition tL1LineSize = tPartition( ciL1AssumedLineSize, 
                                       12/sizeof(DWORD), 80/sizeof(DWORD), ciL1LineSizeStep,
                                       eSequential, eStepYStride, tspLineSize, ciL1LineSizeStep*sizeof(DWORD) );
    #ifdef SIMULATE
        gtspThreshold.m_iYdwStride = (32/sizeof(DWORD))-1;
    #endif
    tL1LineSize.EffectiveSize( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
    CSVMessage( "Ignore cache percentages shown for L1 Line Size since we don't know enough about the cache yet to model it properly.\r\n" );
    tL1LineSize.LogReport( "L1 Line", 0 );
    if ( 0 == tL1LineSize.iMeasuredSize() ) fRet = false;
    #ifdef SIMULATE
        gtspThreshold.m_iYdwStride = -1;
    #endif
    if ( tL1LineSize.fLowConfidence() > cfConfidenceThreshold )
    {
        iL1LineSize = tL1LineSize.iLowSize()*sizeof(DWORD) + ciL1LineSizeStep*sizeof(DWORD);
        // round up to nearest power of 2.
        if ( iL1LineSize & (iL1LineSize-1) )
        {
            iL1LineSize = 1<<(iLog2(iL1LineSize)+1);
        }
        if ( iL1LineSize < 16 )
        {
            assert( iL1LineSize >= 16 );
            iL1LineSize = gtGlobal.iL1LineSize;
        }
        if ( iL1LineSize <= 8 ) iL1LineSize = 8;    // protect from unrealable values, particularly 0.
        if ( gtGlobal.bThisCacheInfo && (int)gtGlobal.tThisCacheInfo.dwL1DCacheLineSize != iL1LineSize && gtGlobal.bWarnings )
        {
            LogPrintf( "Warning: System reports L1 Line Size as %d but measured %d.\r\n", gtGlobal.tThisCacheInfo.dwL1DCacheLineSize, iL1LineSize );
        }

        if ( iL1LineSize != ciL1AssumedLineSize )
        {
            // if we found a new size, repeat L1Size Measurements
            // find the L1 cache size using this new cache line size
            tStridePattern tspL1Resize = { eReadLoop1D, 128*ci1K/iL1LineSize, iL1LineSize/sizeof(DWORD) };
            tPartition tL1Resize = tPartition( iL1LineSize, 
                                             8*ci1K/iL1LineSize,
                                             128*ci1K/iL1LineSize,
                                             1,
                                             ePower2, eStepYStrides, tspL1Resize, 0 );
            tL1Resize.SetPageWalkLatency( gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
            #ifdef SIMULATE
                gtspThreshold.m_iYStrides = 32*ci1K/iL1LineSize;
            #endif
            tL1Resize.EffectiveSize( gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
            CSVMessage( "Ignore cache percentages shown for L1 Revised Size since we don't know enough about the cache yet to model it properly.\r\n" );
            tL1Resize.LogReport( "L1 Revised", iL1LineSize );
            if ( 0 ==  tL1Resize.iMeasuredSize() ) fRet = false;
            #ifdef SIMULATE
                gtspThreshold.m_iYStrides = -1;
            #endif
            if ( tL1Resize.fLowConfidence() > cfConfidenceThreshold )
            {
                iL1CacheSize = tL1Resize.iLowSize()*iL1LineSize;
                if ( gtGlobal.bThisCacheInfo && (int)gtGlobal.tThisCacheInfo.dwL1DCacheSize != iL1CacheSize && gtGlobal.bWarnings )
                {
                    LogPrintf( "Warning: System reports L1 size as %d but measured %d.\r\n", gtGlobal.tThisCacheInfo.dwL1DCacheSize, iL1CacheSize );
                }
            }
            else if (gtGlobal.bWarnings)
            {
                LogPrintf( "Warning: L1 Size results ignored due to low confidence %.3f\r\n", tL1Resize.fLowConfidence() );
            }
        }
    }
    else if (gtGlobal.bWarnings)
    {
        LogPrintf( "Warning: Line Size results ignored due to low confidence %.3f\r\n", tL1LineSize.fLowConfidence() );
    }

    gtGlobal.iL1LineSize  = max(iL1LineSize,8);     // protect from unreasonably small values, particularly 0
    gtGlobal.iL1SizeBytes = iL1CacheSize;


    // ****************************************************************************************
    // Find the L1 Replacement Policy
    // ****************************************************************************************
    const int ciL1MinStrides = (   gtGlobal.iL1SizeBytes/8 )/gtGlobal.iL1LineSize;
    const int ciL1MaxStrides = ( 2*gtGlobal.iL1SizeBytes   )/gtGlobal.iL1LineSize;
    tStridePattern tspL1CReplace = { eReadLoop1D, ciL1MinStrides, gtGlobal.iL1LineSize/sizeof(DWORD) };
    tPartition tL1CReplace = tPartition( gtGlobal.iL1LineSize, 
                                         ciL1MinStrides, ciL1MaxStrides, ciL1MinStrides,
                                         eSequential, eStepYStrides, tspL1CReplace, 0 );
    float fL1RPConfidence = -1;
    tL1CReplace.SetPageWalkLatency( gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
    tL1CReplace.EffectiveSize( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
    gtGlobal.iL1ReplacementPolicy = tL1CReplace.iCacheReplacementPolicy( 1, fL1RPConfidence );
    LogPrintf( "L1 Cache Replacement Policy %s with confidence %9.6f\r\n",
                strCacheReplacementPolicy( gtGlobal.iL1ReplacementPolicy ), 
                fL1RPConfidence 
                );
    CSVMessage( "Ignore cache percentages shown for L1 Replacement Policy since we don't know enough about the L2 cache yet to model it properly.\r\n" );
    tL1CReplace.LogReport( "L1 Replacement Policy",  ciCSVReportOnly );



    // ****************************************************************************************
    // from here on, use measured L1 values as constants
    // ****************************************************************************************
   
    LogPrintf( "\r\nFinal L1 Cache Line Size is %d and L1 Size is %d\r\n\r\n", gtGlobal.iL1LineSize, gtGlobal.iL1SizeBytes );
    PerfReport( _T("Final L1 Cache Line Size"),  gtGlobal.iL1LineSize );
    PerfReport( _T("Final L1 Size"),  gtGlobal.iL1SizeBytes );

    if ( gtGlobal.bThisCacheInfo && (int)gtGlobal.tThisCacheInfo.dwL1DCacheLineSize != gtGlobal.iL1LineSize )
    {
        LogPrintf( "Error:  Reported L1 Line Size = %d != Measured L1 Line size = %d\r\n",
                    gtGlobal.tThisCacheInfo.dwL1DCacheLineSize,
                    gtGlobal.iL1LineSize
                   );
    }
    if ( gtGlobal.bThisCacheInfo && (int)gtGlobal.tThisCacheInfo.dwL1DCacheSize != gtGlobal.iL1SizeBytes )
    {
        LogPrintf( "Error:  Reported L1 Cache Size = %d != Measured L1 Cache size = %d\r\n",
                    gtGlobal.tThisCacheInfo.dwL1DCacheSize,
                    gtGlobal.iL1SizeBytes
                   );
    }

    // and set these temporary variables to bad values to catch misuse
    iL1LineSize = iL1CacheSize = -1;

    // repeat RAW CPU speed test looking for CPU frequency changes.
    TestCPULatency( &iGrandTotal );

    // ****************************************************************************************
    // L2 cache size and line size
    // first find the L2 cache size
    // important concept - we will use half the lowest point in the high range 
    // the reason we don't use the top of the low range is the high variance in latencies as one tries to fill a cache to capacity 
    // The virtual-to-physical mapping inherent in physically addressed set-associative caches causes this variance.
    // ****************************************************************************************
    int iL2SizeBytes = 128*ci1K;    // assume a smaller size than expected
    int iL2LineSize = gtGlobal.iL1LineSize;     // assume it is the same.
    if ( gtGlobal.bNoL2 && 0==gtGlobal.tThisCacheInfo.dwL2DCacheSize )
    {
        // don't check for an L2 if command line says we don't have one and BSP says we don't have one
        // note that if getting the cache info failed, gtGlobal.tThisCacheInfo.dwL2DCacheSize will be 0.
        // essentially, it is too error prone to distinguish noisy measurements from the lack of an L2.
        iL2SizeBytes = 0;
    }
    else
    {

        const int ciYStrides = max( 64*ci1K/gtGlobal.iL1LineSize, (2*iL1CacheSize/gtGlobal.iL1LineSize));
        tStridePattern tspL2Size = { eReadLoop1D, 128*ci1K/gtGlobal.iL1LineSize, gtGlobal.iL1LineSize/sizeof(DWORD) };
        tPartition tL2Size = tPartition( gtGlobal.iL1LineSize, 
                                         ciYStrides, 512*ci1K/gtGlobal.iL1LineSize, 1,
                                         ePower2, eStepYStrides, tspL2Size, 0 );
        tL2Size.SetPageWalkLatency( gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
        if ( 256 > gtGlobal.iTLBSize*4 && gtGlobal.iTLBSize*4 > 128 )
        {   // we want to avoid doing a lot of page walks during this test
            tL2Size.SetReduce256ths( 256 - 4*gtGlobal.iTLBSize );
        }
        else
        {   // reduce a little 32/256 = 1/8th reduction
            tL2Size.SetReduce256ths( 32 );
        }
        #ifdef SIMULATE
            gtspThreshold.m_iYStrides = (256+1)*ci1K/gtGlobal.iL1LineSize;
        #endif
        tL2Size.EffectiveSize( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
        CSVMessage( "Ignore cache percentages shown for L2 Size since we don't know enough about the L2 cache yet to model it properly.\r\n" );
        tL2Size.LogReport( "L2", -gtGlobal.iL1LineSize );
        if ( 0 ==  tL2Size.iMeasuredSize() ) fRet = false;
        #ifdef SIMULATE
            gtspThreshold.m_iYStrides = -1;
        #endif
        if ( tL2Size.fHighConfidence() > cfConfidenceThreshold )
        {
            iL2SizeBytes = tL2Size.iHighSize()*gtGlobal.iL1LineSize/2;
            if ( gtGlobal.bThisCacheInfo && (int)gtGlobal.tThisCacheInfo.dwL2DCacheSize != iL2SizeBytes && gtGlobal.bWarnings )
            {
                LogPrintf( "Warning: System reports L2 size as %d but measured %d.\r\n", gtGlobal.tThisCacheInfo.dwL2DCacheSize, iL2SizeBytes );
            }
        }
        else if (tL2Size.fLowConfidence() > cfConfidenceThreshold )
        {
            iL2SizeBytes = tL2Size.iLowSize()*gtGlobal.iL1LineSize;
            if ( gtGlobal.bThisCacheInfo && (int)gtGlobal.tThisCacheInfo.dwL2DCacheSize != iL2SizeBytes && gtGlobal.bWarnings )
            {
                LogPrintf( "Warning: System reports L2 size as %d but measured %d.\r\n", gtGlobal.tThisCacheInfo.dwL2DCacheSize, iL2SizeBytes );
            }
        }
        else if (gtGlobal.bWarnings)
        {
            LogPrintf( "Warning: L2 Size results ignored due to low confidence %.3f.  Assuming L2 size = %d.\r\n", 
                        tL2Size.fHighConfidence(), iL2SizeBytes );
        }

        // ****************************************************************************************
        // L2 line size might be a multiple of L1 Line Size
        // ****************************************************************************************
        const int ciL2LineSizeSpan = (3*iL2SizeBytes)/2; // typically 384K
        tStridePattern tspL2LineSize = { eReadLoop1D, ciL2LineSizeSpan/(gtGlobal.iL1LineSize/sizeof(DWORD)), gtGlobal.iL1LineSize/sizeof(DWORD) };
        tPartition tL2LineSize = tPartition( gtGlobal.iL1LineSize, 
                                           gtGlobal.iL1LineSize/sizeof(DWORD), 8*gtGlobal.iL1LineSize/sizeof(DWORD), 1,
                                           ePower2, eStepYStride, tspL2LineSize, 4 );
        tL2LineSize.SetPageWalkLatency( gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
        #ifdef SIMULATE
            gtspThreshold.m_iYdwStride = (128/sizeof(DWORD))-1;
        #endif
        tL2LineSize.EffectiveSize( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
        CSVMessage( "Ignore cache percentages shown for L2 Line Size since we don't know enough about the L2 cache yet to model it properly.\r\n" );
        tL2LineSize.LogReport( "L2 Line", 0 );
        if ( 0 ==  tL2LineSize.iMeasuredSize() ) fRet = false;
        #ifdef SIMULATE
            gtspThreshold.m_iYdwStride = -1;
        #endif
        if ( tL2LineSize.fLowConfidence() > cfConfidenceThreshold || tL2LineSize.fHighConfidence() > cfConfidenceThreshold )
        {
            if ( tL2LineSize.fLowConfidence() > cfConfidenceThreshold && 
                 tL2LineSize.fHighConfidence() > cfConfidenceThreshold &&
                 2*tL1LineSize.iLowSize() == tL1LineSize.iHighSize()
                 )
            {
                iL2LineSize = tL2LineSize.iHighSize()*sizeof(DWORD);
            }
            else if ( tL2LineSize.fLowConfidence() > cfConfidenceThreshold )
            {
                iL2LineSize = tL2LineSize.iLowSize()*sizeof(DWORD) + 4*sizeof(DWORD);
                // round up to nearest power of 2.
                if ( iL2LineSize & (iL2LineSize-1) )
                {
                    iL2LineSize = 1<<(iLog2(iL2LineSize)+1);
                }
                if ( iL2LineSize < 16 )
                {
                    assert( iL2LineSize >= 16 );
                    iL2LineSize = gtGlobal.iL2LineSize>0 ? gtGlobal.iL2LineSize : max(16,gtGlobal.iL1LineSize);
                }
            }
            else
            {
                iL2LineSize = tL2LineSize.iHighSize()*sizeof(DWORD);
            }
            if ( iL2LineSize < iL1LineSize ) iL2LineSize = iL1LineSize;     // protect from unreasonable values.
            if ( gtGlobal.bThisCacheInfo && (int)gtGlobal.tThisCacheInfo.dwL2DCacheLineSize != iL2LineSize && gtGlobal.bWarnings )
            {
                LogPrintf( "Warning: System reports L2 Line Size as %d but measured %d.\r\n", gtGlobal.tThisCacheInfo.dwL2DCacheLineSize, iL2LineSize );
            }
            if ( iL2LineSize != gtGlobal.iL1LineSize )
            {
                // if we found a new L2 Line Size, repeat L2Size Measurements
                const int ciYReStrides = max( 64*ci1K/iL2LineSize, (2*iL1CacheSize/iL2LineSize));
                tStridePattern tspL2Resize = { eReadLoop1D, 128*ci1K/iL2LineSize, iL2LineSize/sizeof(DWORD) };
                tPartition tL2Resize = tPartition( iL2LineSize, 
                                                 ciYReStrides, 1024*ci1K/iL2LineSize, 1,
                                                 ePower2, eStepYStrides, tspL2Resize, 0 );
                tL2Resize.SetPageWalkLatency( gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
                if ( 256 > gtGlobal.iTLBSize*4 && gtGlobal.iTLBSize*4 > 128 )
                {   // we want to avoid doing a lot of page walks during this test
                    tL2Resize.SetReduce256ths( 256 - 4*gtGlobal.iTLBSize );
                }
                else
                {   // reduce a little 32/256 = 1/8th reduction
                    tL2Resize.SetReduce256ths( 32 );
                }
                #ifdef SIMULATE
                    gtspThreshold.m_iYStrides = (256+1)*ci1K/iL2LineSize;
                #endif
                tL2Resize.EffectiveSize( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
                CSVMessage( "Ignore cache percentages shown for L2 Revised Size since we don't know enough about the L2 cache yet to model it properly.\r\n" );
                tL2Resize.LogReport( "L2 Revised", -iL2LineSize );
                if ( 0 ==  tL2Resize.iMeasuredSize() ) fRet = false;
                #ifdef SIMULATE
                    gtspThreshold.m_iYStrides = -1;
                #endif
                if ( tL2Resize.fHighConfidence() > cfConfidenceThreshold )
                {
                    iL2SizeBytes = tL2Resize.iHighSize()*iL2LineSize/2;
                    if ( gtGlobal.bThisCacheInfo && (int)gtGlobal.tThisCacheInfo.dwL2DCacheSize != iL2SizeBytes && gtGlobal.bWarnings )
                    {
                        LogPrintf( "Warning: System reports L2 size as %d but measured %d.\r\n", gtGlobal.tThisCacheInfo.dwL2DCacheSize, iL2SizeBytes );
                    }
                }
                else if ( tL2Resize.fLowConfidence() > cfConfidenceThreshold )
                {
                    iL2SizeBytes = tL2Resize.iLowSize()*iL2LineSize;
                    if ( gtGlobal.bThisCacheInfo && (int)gtGlobal.tThisCacheInfo.dwL2DCacheSize != iL2SizeBytes && gtGlobal.bWarnings )
                    {
                        LogPrintf( "Warning: System reports L2 size as %d but measured %d.\r\n", gtGlobal.tThisCacheInfo.dwL2DCacheSize, iL2SizeBytes );
                    }
                }
                else if (gtGlobal.bWarnings)
                {
                    LogPrintf( "Warning: L2 Revised Size results ignored due to low confidence %.3f\r\n", tL2Resize.fLowConfidence() );
                }
            }
        }
        else if (gtGlobal.bWarnings)
        {
            LogPrintf( "Warning: L2 Line Size results ignored due to low confidence %.3f\r\n", tL2LineSize.fLowConfidence() );
        }
    }

    gtGlobal.iL2LineSize  = iL2LineSize>=gtGlobal.iL1LineSize ? iL2LineSize :max(16,gtGlobal.iL1LineSize);
    gtGlobal.iL2SizeBytes = iL2SizeBytes;

    // test note - one way to test the program for robustness is to set the L2 cache size to 0 here to simulate a device without an L2.

    LogPrintf( "\r\nFinal L2 Cache Line Size is %d and L2 Size is %d\r\n\r\n", gtGlobal.iL2LineSize, gtGlobal.iL2SizeBytes );

    PerfReport( _T("Final L2 Cache Line Size"),  gtGlobal.iL2LineSize );    
    PerfReport( _T("Final L2 Size"),  gtGlobal.iL2SizeBytes );

    // and set these temporary variables to bad values to catch misuse below
    iL2LineSize = iL2SizeBytes = -1;
    if ( gtGlobal.bThisCacheInfo && (int)gtGlobal.tThisCacheInfo.dwL2DCacheLineSize != gtGlobal.iL2LineSize )
    {
        LogPrintf( "Error:  Reported L2 Line Size = %d != Measured L2 Line size = %d\r\n",
                    gtGlobal.tThisCacheInfo.dwL2DCacheLineSize,
                    gtGlobal.iL2LineSize
                   );
    }
    if ( gtGlobal.bThisCacheInfo && (int)gtGlobal.tThisCacheInfo.dwL2DCacheSize != gtGlobal.iL2SizeBytes )
    {
        LogPrintf( "Error:  Reported L2 Cache Size = %d != Measured L2 Cache size = %d\r\n",
                    gtGlobal.tThisCacheInfo.dwL2DCacheSize,
                    gtGlobal.iL2SizeBytes
                   );
    }

    float fL2RPConfidence = -1;
    if ( gtGlobal.iL2SizeBytes >= gtGlobal.iL1SizeBytes )
    {
        // ****************************************************************************************
        // Find the L2 Replacement Policy
        // ****************************************************************************************
        const int ciL2MinStrides = ( 2*gtGlobal.iL1SizeBytes   )/gtGlobal.iL2LineSize;
        const int ciL2MaxStrides = ( 2*gtGlobal.iL2SizeBytes   )/gtGlobal.iL2LineSize;
        const int ciL2StepSize   = (   gtGlobal.iL1SizeBytes/2 )/gtGlobal.iL2LineSize;
        tStridePattern tspL2CReplace = { eReadLoop1D, ciL2MinStrides, gtGlobal.iL2LineSize/sizeof(DWORD) };
        tPartition tL2CReplace = tPartition( gtGlobal.iL2LineSize, 
                                             ciL2MinStrides, ciL2MaxStrides, ciL2StepSize,
                                             eSequential, eStepYStrides, tspL2CReplace, 0 );
        tL2CReplace.SetPageWalkLatency( gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
        tL2CReplace.EffectiveSize( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
        gtGlobal.iL2ReplacementPolicy = tL2CReplace.iCacheReplacementPolicy( 2, fL2RPConfidence );
        LogPrintf( "L2 Cache Replacement Policy %s with confidence %9.6f\r\n",
                    strCacheReplacementPolicy( gtGlobal.iL2ReplacementPolicy ), 
                    fL2RPConfidence 
                    );
        tL2CReplace.LogReport( "L2 Replacement Policy",  ciCSVReportOnly );
    }

    // ****************************************************************************************
    // L2 Latency
    // ****************************************************************************************
    gtGlobal.dwL2Latency = 0;
    const int ciL2Strides = max( 2*gtGlobal.iL1SizeBytes/gtGlobal.iL2LineSize, (gtGlobal.iL2SizeBytes/2)/gtGlobal.iL2LineSize );
    const tStridePattern ctspL2Latency = { eReadLoop1D, ciL2Strides, gtGlobal.iL2LineSize/sizeof(DWORD) };
    tLatency tL2Latency = tLatency( gtGlobal.iL2LineSize, ctspL2Latency, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
    tL2Latency.Measure( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
    tL2Latency.LogReport( "L2", 2*gtGlobal.dwL1Latency, INT_MAX );
    PerfReport( _T("L2 Read Latency ps"),  10*tL2Latency.dwLatency100() );
    gtGlobal.dwL2Latency = tL2Latency.dwLatency100();
    if ( 0 == tL2Latency.dwLatency100() ) fRet = false;


    // repeat RAW CPU speed test looking for CPU frequency changes.
    TestCPULatency( &iGrandTotal );

    // ****************************************************************************************
    // Memory Bus Latency
    // ****************************************************************************************
    const int ciMBStrides = max( 1024*ci1K/gtGlobal.iL2LineSize, 2*gtGlobal.iL2SizeBytes/gtGlobal.iL2LineSize);         // span more than L2
    const tStridePattern ctspMBLatency = { eReadLoop1D, ciMBStrides, gtGlobal.iL2LineSize/sizeof(DWORD) };
    tLatency tMBLatency = tLatency( gtGlobal.iL2LineSize, ctspMBLatency, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
    tMBLatency.SetExpected(ciFastestMemory);
    tMBLatency.Measure( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
    tMBLatency.LogReport( "Cached Memory Bus", 2*gtGlobal.dwL2Latency, INT_MAX );
    PerfReport( _T("Cached MBus Read ps"), 10*tMBLatency.dwLatency100() );
    if ( 0 == tMBLatency.dwLatency100() ) fRet = false;
    gtGlobal.dwMBLatency    = tMBLatency.dwLatency100();

    // ****************************************************************************************
    // Fine tune (disambiguate) L2 Replacement Policy
    // ****************************************************************************************
    if ( eReplacePLRU==gtGlobal.iL2ReplacementPolicy && fL2RPConfidence < 0.75 && gtGlobal.iL2SizeBytes >= gtGlobal.iL1SizeBytes )
    {
        // Tie between PRLU and PerSetRoundRobin above.  For accuracy, disambiguate
        const int ciL1Ways     = gtGlobal.bThisCacheInfo ? gtGlobal.tThisCacheInfo.dwL1ICacheNumWays : 4;
        const int ciL1SetAlias = gtGlobal.iL1SizeBytes/ciL1Ways;
        const int ciL2Ways     = gtGlobal.bThisCacheInfo ? gtGlobal.tThisCacheInfo.dwL2ICacheNumWays : 8;
        const int ciL2SetAlias = gtGlobal.iL2SizeBytes/ciL2Ways;
        const int ciZStrides   = ciL2SetAlias/(2*ciL1SetAlias);
        tStridePattern tspLPRUvsPerSet = { 
                            eReadLoop3D, ciL2Ways, ciL2SetAlias/sizeof(DWORD), 
                                         (3*ciL1Ways)/2, ciL1SetAlias/sizeof(DWORD), 
                            0.0f,        ciZStrides, ciL2SetAlias/sizeof(DWORD) };
        if (  gtGlobal.bExtraPrints )
        {
            tspLPRUvsPerSet.ShowAccessPattern( "PLRU vs PSRR", eShowPatternStatistics, 0 );
        }
        // first predict what it will be if PLRU
        int iL1Hits = 0, iL2Hits=0, iMBHits=0, iPageWalks=0, iRASChanges=0;
        tspLPRUvsPerSet.AnalyzeAccessPattern( &iL1Hits, &iL2Hits, &iMBHits, &iPageWalks, &iRASChanges );
        double dL1HitRatio = ((double)iL1Hits)/(iL1Hits+iL2Hits+iMBHits);
        double dL2HitRatio = ((double)iL2Hits)/(iL1Hits+iL2Hits+iMBHits);
        double dPLRUPredicted = dL1HitRatio*tL1Latency.dwLatency100() + dL2HitRatio*tL2Latency.dwLatency100() + (1-(dL1HitRatio+dL2HitRatio))*tMBLatency.dwLatency100();
        // now predict what it will be if PSRR
        gtGlobal.iL2ReplacementPolicy = ePerSetRoundRobin;
        if (  gtGlobal.bExtraPrints )
        {
            tspLPRUvsPerSet.ShowAccessPattern( "PLRU vs PSRR", eShowGrid, 5*tspLPRUvsPerSet.m_iYStrides*tspLPRUvsPerSet.m_iXStrides );
            tspLPRUvsPerSet.ShowAccessPattern( "PLRU vs PSRR", eShowPatternStatistics, 5*tspLPRUvsPerSet.m_iYStrides*tspLPRUvsPerSet.m_iXStrides );
        }
        iL1Hits = iL2Hits = iMBHits = iPageWalks = iRASChanges = 0;
        tspLPRUvsPerSet.AnalyzeAccessPattern( &iL1Hits, &iL2Hits, &iMBHits, &iPageWalks, &iRASChanges );
        dL1HitRatio = ((double)iL1Hits)/(iL1Hits+iL2Hits+iMBHits);
        dL2HitRatio = ((double)iL2Hits)/(iL1Hits+iL2Hits+iMBHits);
        double dPSRRPredicted = dL1HitRatio*tL1Latency.dwLatency100() + dL2HitRatio*tL2Latency.dwLatency100() + (1-(dL1HitRatio+dL2HitRatio))*tMBLatency.dwLatency100();
        // restore
        gtGlobal.iL2ReplacementPolicy = eReplacePLRU;
        if ( fabs( dPLRUPredicted-dPSRRPredicted ) > 0.2*max(dPLRUPredicted,dPSRRPredicted) )
        {
            // now measure it.
            tLatency tLPRUvsPerSet = tLatency( gtGlobal.iL2LineSize, tspLPRUvsPerSet, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
            tLPRUvsPerSet.SetExpected(ciFastestMemory);
            tLPRUvsPerSet.Measure( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
            tLPRUvsPerSet.LogReport( "PLRU vs PSRR" );
            if ( 0 == tLPRUvsPerSet.dwLatency100() ) fRet = false;
            double dPLRUDiff = tLPRUvsPerSet.dwLatency100() - dPLRUPredicted;
            if ( dPLRUDiff < 0 ) dPLRUDiff = -dPLRUDiff;
            double dPSRRDiff = tLPRUvsPerSet.dwLatency100() - dPSRRPredicted;
            if ( dPSRRDiff < 0 ) dPSRRDiff = -dPSRRDiff;
            if ( dPLRUDiff*2 < dPSRRDiff )
            {
                LogPrintf( "L2 P-LRU Selected because measured %6.2f is much closer to PRLU predicted %6.2f than PSRR %6.2f\r\n",
                            0.01*tLPRUvsPerSet.dwLatency100(),
                            0.01*dPLRUPredicted,
                            0.01*dPSRRPredicted
                            );
            }
            else if ( dPSRRDiff*2 < dPLRUDiff )
            {
                LogPrintf( "L2 PerSetRoundRobin Selected because measured %6.2f is much closer to PSRR predicted %6.2f than PLRU %6.2f\r\n",
                            0.01*tLPRUvsPerSet.dwLatency100(),
                            0.01*dPSRRPredicted,
                            0.01*dPLRUPredicted
                            );
                gtGlobal.iL2ReplacementPolicy = ePerSetRoundRobin;
            }
        }
    }

    // ****************************************************************************************
    // Full Sequential Memory Bus Latency
    // Sequentail bus latency is much faster than bus latency because most of the accesses hit L1
    // and when L2 Linesize is larger than L1 Linesize, some of them hit L2.
    // ****************************************************************************************
    const int ciFSMBStrides = max( 1024*ci1K/sizeof(DWORD), 2*gtGlobal.iL2SizeBytes/sizeof(DWORD));                     // span more than L2
    const tStridePattern ctspFSMBLatency = { eReadLoop1D, ciFSMBStrides, 1 };
    tLatency tFSMBLatency = tLatency( gtGlobal.iL2LineSize, ctspFSMBLatency, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
    tFSMBLatency.SetExpected(ciFastestMemory);
    tFSMBLatency.Measure( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
    tFSMBLatency.LogReport( "Cached Full Sequential Memory Bus" );
    PerfReport( _T("Cached Full Seq MBus Read ps"), 10*tFSMBLatency.dwLatency100() );
    if ( 0 == tFSMBLatency.dwLatency100() ) fRet = false;
    gtGlobal.dwSeqLatency = tFSMBLatency.dwLatency100();

    // ****************************************************************************************
    // Now try to observe SDRAM RAS Timing 
    // The pattern for this is tricky because we need to access a lot of memory to 
    // make sure the Virtual-to-physical remap does not allow L1 and L2 to effect the results.
    // main idea is to a 2D pattern with large steps in X and a lot of little steps in Y to really over-fill the cache
    // But the test is fragile and only gives reliable results when the system has been recently booted and nothing else but this test has been run.
    // Explanation of frangibility of test
    //   when the sequential virtual address pages map simply to physical pages, then the long strides required 
    //   by this test will also be long strides to physical memory.
    //   But when the virtual address map is random, then the long virtual strides are random physical strides.
    //   Getting real 1MB Section mapped memory would help a lot.
    // Best results will be seen by running this test right after the system is booted
    // ****************************************************************************************
    const int ciRASStepSize = 2*ci1K;
    int iMinRASXStride = ciRASStepSize;
    int iMaxRASXStride = (2*max(gtGlobal.iL2SizeBytes,gtGlobal.iL1SizeBytes))/5;
    int iRASYStrides   = 2*max( (gtGlobal.iL1SizeBytes/gtGlobal.iL1LineSize), (gtGlobal.iL2SizeBytes/gtGlobal.iL2LineSize) );
    iRASYStrides       = max( iRASYStrides, 20 );
    int iRASXStrides   = 2*gtGlobal.iTLBSize;
    // Try to make each XStride hit the same cache set enough times to always miss the cache
    if ( gtGlobal.bThisCacheInfo )
    {   
        int iMaxWays = max( gtGlobal.tThisCacheInfo.dwL1DCacheNumWays, gtGlobal.tThisCacheInfo.dwL2DCacheNumWays );
        if ( iRASYStrides < (iMaxWays+4) )
            iRASYStrides = iMaxWays+4;
    }
    int iXStrides2MissL2 = (3*max(gtGlobal.iL2SizeBytes,gtGlobal.iL1SizeBytes)/2)/iMinRASXStride;
    if ( iXStrides2MissL2 > iRASXStrides )
        iRASXStrides = iXStrides2MissL2;
    int iXSpan = min( 16*ci1M, (gtGlobal.pdw1MBTop+1-gtGlobal.pdw1MBBase)*sizeof(DWORD) );
    while ( iRASXStrides*iMaxRASXStride > iXSpan )
    {   // avoid trying to test too large span first by reducing the top-size and then by reducing the bottom
        iMaxRASXStride /= 2;
        if ( iRASXStrides*iMaxRASXStride <= iXSpan )
            break;
        iMinRASXStride *= 2;
        iRASXStrides   /=  2;
    }
    iRASYStrides = 20;
    tStridePattern tspRASOverview = { eReadLoop2D, iRASYStrides, gtGlobal.iL1LineSize/sizeof(DWORD), iRASXStrides, iMinRASXStride/sizeof(DWORD)  };
    tPartition tRASOverview = tPartition( gtGlobal.iL2LineSize, 
                                          iMinRASXStride/sizeof(DWORD), iMaxRASXStride/sizeof(DWORD), ciRASStepSize/sizeof(DWORD),
                                          ePower2, eStepXStride, tspRASOverview, sizeof(DWORD) );
    tRASOverview.SetExpected(ciFastestMemory);
    tRASOverview.SetPageWalkLatency( gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
    tRASOverview.EffectiveSize( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
    tRASOverview.LogReport( "RAS 2K^i Scan SDRAM Row", -((int)sizeof(DWORD)) );
    int iSDRAMRowLatency = max( tRASOverview.dwHighLatency()-tRASOverview.dwLowLatency(), 0 );

    // ****************************************************************************************
    // because of the fragility of the RAS 2L^i test above, test 16K and 32K strides across number of strides and look for the max of the medians
    // Rows within 1Gb (aka 256Mx4) DRAM are typically 16K in size, so step by 16K to reduce DRAM RAS tricks
    // This should also work for next generation 2 GB RAM and of course smaller RAMS.  
    // Specifically, access within about 16KB allows the memory controller to hold RAS between accesses that fall within the same Row, 
    // significantly shortening write times.  So striding 16KB should make each access suffer the full RAS time and precharge time.
    // Use a 2-D pattern where the 16KB steps happen in the X (most rapidly changing dimension) and Y steps are small, overlaying the same pages
    // If possible, span less than the max number of TLB registers since we don't really test Section mapped memory.    
    // A simple Latency measurement is fast to measure but can be confounded by different DRAM bank organizations as well as different memory controller policies.
    // ****************************************************************************************
    const int ci16KXdwStride = (16*ci1K + gtGlobal.iL2LineSize);
    const int ci16KYStrides  = ci16KXdwStride/gtGlobal.iL2LineSize;
    tStridePattern tsp16KScan = { eReadLoop2D, ci16KYStrides, gtGlobal.iL2LineSize/sizeof(DWORD), 4, ci16KXdwStride/sizeof(DWORD)  };
    tPartition t16KScan = tPartition( gtGlobal.iL2LineSize, 
                                      gtGlobal.iTLBSize/8, 4*gtGlobal.iTLBSize, gtGlobal.iTLBSize/4,
                                      eSequential, eStepXStrides, tsp16KScan, 0 );
    t16KScan.SetPageWalkLatency( gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
    t16KScan.EffectiveSize( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
    t16KScan.LogReport( "Read 16K+Line Stride Memory" );
    if ( 0 == t16KScan.dwMaxMedianLatency() ) fRet = false;
    if ( (int)( t16KScan.dwMaxMedianLatency()-tMBLatency.dwLatency100() ) > iSDRAMRowLatency )
    {
        iSDRAMRowLatency = t16KScan.dwMaxMedianLatency()-tMBLatency.dwLatency100();
    }
    if ( gtGlobal.bChartDetails )
    {
        // show this pattern at least once in a way one can see the gradual shift up on the accesses by 128.
        t16KScan.ShowAccessPattern( "Read 16K+Line Stride Memory", gtGlobal.iTLBSize/8, gtGlobal.iTLBSize/8,  eShowGrid, 50*gtGlobal.iTLBSize/8 );
    }


    // ****************************************************************************************
    // Forward looking test for 4 GB RAM memories needing 32KB strides to cover an additional two generations
    // The above simple method is fast to measure but can be confounded by different DRAM bank organizations as well as different memory controller policies.
    // ****************************************************************************************
    const int ci32KXdwStride = (32*ci1K + gtGlobal.iL2LineSize);
    const int ci32KYStrides = ci32KXdwStride/gtGlobal.iL2LineSize;
    tStridePattern tsp32KScan = { eReadLoop2D, ci32KYStrides, gtGlobal.iL2LineSize/sizeof(DWORD), 4, ci32KXdwStride/sizeof(DWORD)  };
    tPartition t32KScan = tPartition( gtGlobal.iL2LineSize, 
                                      gtGlobal.iTLBSize/8, 4*gtGlobal.iTLBSize, gtGlobal.iTLBSize/4,
                                      eSequential, eStepXStrides, tsp32KScan, 0 );
    t32KScan.SetPageWalkLatency( gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
    t32KScan.EffectiveSize( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
    t32KScan.LogReport( "Read 32K+Scan Stride" );
    if ( (int)( t32KScan.dwMaxMedianLatency()-tMBLatency.dwLatency100() ) > iSDRAMRowLatency )
    {
        iSDRAMRowLatency = t32KScan.dwMaxMedianLatency()-tMBLatency.dwLatency100();
    }
    PerfReport( _T("SDRAM Row Extra Latency ps"),  10*iSDRAMRowLatency );
    gtGlobal.dwRASExtraLatency = (DWORD)(iSDRAMRowLatency>0 ? iSDRAMRowLatency : 0);


    // ****************************************************************************************
    // Write L1 Latency
    // This latency will depend on whether L1 is Allocate-On-Write-Miss or not.
    // So measure both with and without preloading the cache lines from memory to distinquish
    // ****************************************************************************************
    const int ciL1Span = (gtGlobal.iL1SizeBytes/2)/gtGlobal.iL1LineSize;
    const tStridePattern ctspL1Write = { eWriteLoop1D, ciL1Span, gtGlobal.iL1LineSize/sizeof(DWORD) };
    tLatency tL1WriteLatency = tLatency( gtGlobal.iL1LineSize, ctspL1Write );
    tL1WriteLatency.Measure( gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
    tL1WriteLatency.LogReport( "L1" );
    PerfReport( _T("L1 Write Latency ps"),  10*tL1WriteLatency.dwLatency100() );
    gtGlobal.dwL1WriteLatency = tL1WriteLatency.dwLatency100();
    if ( 0 == tL1WriteLatency.dwLatency100() ) fRet = false;

    const tStridePattern ctspL1WritePreloaded = { eWriteLoop1D, ciL1Span, gtGlobal.iL1LineSize/sizeof(DWORD), 0, 0, 100.0f };
    tLatency tL1WritePreloadedLatency = tLatency( gtGlobal.iL1LineSize, ctspL1WritePreloaded );
    tL1WritePreloadedLatency.Measure( gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
    tL1WritePreloadedLatency.LogReport( "L1 Preloaded ps", 0, (21*tL1WriteLatency.dwLatency100())/20 );
    PerfReport( _T("L1 Preload-Write Latency ps"),  10*tL1WritePreloadedLatency.dwLatency100() );
    if ( 0 == tL1WritePreloadedLatency.dwLatency100() ) fRet = false;

    // ****************************************************************************************
    // L2 Write Latency
    // ****************************************************************************************
    const int ciL2Span = max( ((gtGlobal.iL2SizeBytes/4)/gtGlobal.iL2LineSize), ((2*gtGlobal.iL1SizeBytes)/gtGlobal.iL2LineSize) );
    const tStridePattern ctspL2Write = { eWriteLoop1D, ciL2Span, gtGlobal.iL2LineSize/sizeof(DWORD) };
    tLatency tL2WriteLatency = tLatency( gtGlobal.iL2LineSize, ctspL2Write, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
    tL2WriteLatency.Measure( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
    tL2WriteLatency.LogReport( "L2", gtGlobal.dwL1WriteLatency, INT_MAX );
    PerfReport( _T("L2 Write Latency ps"),  10*tL2WriteLatency.dwLatency100() );
    if ( 0 == tL2WriteLatency.dwLatency100() ) fRet = false;
    gtGlobal.dwL2WriteLatency = tL2WriteLatency.dwLatency100();

    const tStridePattern ctspL2WritePreloaded = { eWriteLoop1D, ciL2Span, gtGlobal.iL2LineSize/sizeof(DWORD), 0, 0, 100.0f };
    tLatency tL2WritePreloadedLatency = tLatency( gtGlobal.iL2LineSize, ctspL2WritePreloaded, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
    tL2WritePreloadedLatency.Measure( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
    tL2WritePreloadedLatency.LogReport( "L2 Preloaded", tL1WritePreloadedLatency.dwLatency100(), (21*gtGlobal.dwL1WriteLatency)/20 );
    PerfReport( _T("L2 Preload-Write Latency ps"),  10*tL2WritePreloadedLatency.dwLatency100() );
    if ( 0 == tL2WritePreloadedLatency.dwLatency100() ) fRet = false;

    // ****************************************************************************************
    // Memory Bus Write Latency through Cache
    // Writing cached memory, either through the cache or through store-buffers (No-Allocate-On-Write-Miss) can be surprisingly fast, e.g. much
    // faster than reading.
    // ****************************************************************************************
    const int ciMBWStrides = max( 1024*ci1K/gtGlobal.iL2LineSize, 2*gtGlobal.iL2SizeBytes/gtGlobal.iL2LineSize);         // span more than L2
    const tStridePattern ctspMBWrite = { eWriteLoop1D, ciMBWStrides, (gtGlobal.iL2LineSize+sizeof(DWORD))/sizeof(DWORD) };
    tLatency tMBWriteLatency = tLatency( gtGlobal.iL2LineSize, ctspMBWrite, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
    tMBWriteLatency.SetExpected(ciFastestMemory/4);
    tMBWriteLatency.Measure( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
    tMBWriteLatency.LogReport( "Cached Memory Bus" );
    gtGlobal.dwMBWriteLatency = tMBWriteLatency.dwLatency100();
    PerfReport( _T("Cached MBus Write Lat ps"),  10*tMBWriteLatency.dwLatency100() );
    if ( 0 == tMBWriteLatency.dwLatency100() ) fRet = false;

    // ****************************************************************************************
    // Rows within 1Gb (aka 256Mx4) DRAM are typically about 16K in size, so step by 16K to reduce DRAM tricks
    // This should also work for next generation 2 GB RAM and of course smaller RAMS.
    // Specifically, accesses within about 16KB allows the memory controller to hold RAS between accesses that fall within the same Row, 
    // significantly shortening write times.  So striding 16KB should make each access suffer the full RAS time and precharge time.
    // Use a 2-D pattern where the 16KB steps happen in the X (most rapidly changing dimension) and Y steps are small, overlaying the same pages
    // ****************************************************************************************
    const int ci16KWriteXdwStride = (16*ci1K + gtGlobal.iL2LineSize);
    const int ci16KWriteXStrides = max( (gtGlobal.iTLBSize-4), 2*gtGlobal.iL2SizeBytes/ci16KWriteXdwStride);         // span more than L2
    const int ci16KWriteYStrides = ci16KWriteXdwStride/gtGlobal.iL2LineSize;
    const tStridePattern ctsp16KWrite = { eWriteLoop2D, ci16KWriteYStrides, gtGlobal.iL2LineSize/sizeof(DWORD), ci16KWriteXStrides, ci16KWriteXdwStride/sizeof(DWORD) };
    tLatency t16KWriteLatency = tLatency( gtGlobal.iL2LineSize, ctsp16KWrite, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
    t16KWriteLatency.SetExpected(ciFastestMemory);
    t16KWriteLatency.Measure( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
    t16KWriteLatency.LogReport( "Cached 16K-Row Memory Bus" );
    if ( 0 == t16KWriteLatency.dwLatency100() ) fRet = false;

    // ****************************************************************************************
    // Forward looking test for 4 GB RAMs
    // ****************************************************************************************
    const int ci32KWriteXdwStride = (32*ci1K + gtGlobal.iL2LineSize);
    const int ci32KWriteXStrides = max( (gtGlobal.iTLBSize-4), 2*gtGlobal.iL2SizeBytes/ci32KWriteXdwStride);         // span more than L2
    const int ci32KWriteYStrides = ci32KWriteXdwStride/gtGlobal.iL2LineSize;
    const tStridePattern ctsp32KWrite = { eWriteLoop2D, ci32KWriteYStrides, gtGlobal.iL2LineSize/sizeof(DWORD), ci32KWriteXStrides, ci32KWriteXdwStride/sizeof(DWORD) };
    tLatency t32KWriteLatency = tLatency( gtGlobal.iL2LineSize, ctsp32KWrite, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
    t32KWriteLatency.SetExpected(ciFastestMemory);
    t32KWriteLatency.Measure( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
    t32KWriteLatency.LogReport( "Cached 32K-Row Memory Bus" );
    if ( 0 == t32KWriteLatency.dwLatency100() ) fRet = false;

    int iSDRAMRowWriteLatency = max( t16KWriteLatency.dwLatency100(), t32KWriteLatency.dwLatency100() ) - tMBWriteLatency.dwLatency100();
    if ( iSDRAMRowWriteLatency < 0 ) iSDRAMRowWriteLatency = 0;
    PerfReport( _T("SDRAM Row Extra WriteLatency ps"),  10*iSDRAMRowWriteLatency );


    // ****************************************************************************************
    // Full Sequential Memory Bus Write Latency
    // ****************************************************************************************
    const int ciFSMBWStrides = max( 1024*ci1K/sizeof(DWORD), 2*gtGlobal.iL2SizeBytes/sizeof(DWORD));                     // span more than L2
    const tStridePattern ctspFSMBWLatency = { eWriteLoop1D, ciFSMBWStrides, 1 };
    tLatency tFSMBWLatency = tLatency( gtGlobal.iL2LineSize, ctspFSMBWLatency, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
    tFSMBWLatency.SetExpected(ciFastestMemory);
    tFSMBWLatency.Measure( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
    tFSMBWLatency.LogReport( "Cached Full Sequential Memory Bus" );
    PerfReport( _T("Cached Full Seq MBus Write ps"), 10*tFSMBWLatency.dwLatency100() );
    if ( 0 == tFSMBWLatency.dwLatency100() ) fRet = false;
    gtGlobal.dwSeqWriteLatency = tFSMBWLatency.dwLatency100();

    DWORD * pdw1MBHalfTop = gtGlobal.pdw1MBBase + ( gtGlobal.pdw1MBTop+1-gtGlobal.pdw1MBBase )/2 - 1;

    // ****************************************************************************************
    // L1 Span MemCpy 
    // memcpy() is a highly optimized ARM routine which utilizes LDM/STM with up-to 8 registers 
    // and so it can quite a bit faster than single DWORD LDR/STR
    // ****************************************************************************************
    const int ciL1SMCStrides = (gtGlobal.iL1SizeBytes/sizeof(DWORD))/4;
    const tStridePattern ctspL1SMCLatency = { eMemCpyLoop, ciL1SMCStrides, 1 };
    tLatency tL1SMCLatency = tLatency( gtGlobal.iL2LineSize, ctspL1SMCLatency, gtGlobal.iTLBSize, 2*gtGlobal.dwPWLatency );
    tL1SMCLatency.Measure( gtGlobal.pdw1MBBase, pdw1MBHalfTop, pdw1MBHalfTop+1, gtGlobal.pdw1MBTop, &iGrandTotal );
    tL1SMCLatency.LogReport( "Cached L1-Span MemCpy" );
    PerfReport( _T("Cached L1-Span MemCpy ps"), 10*tL1SMCLatency.dwLatency100() );
    if ( 0 == tL1SMCLatency.dwLatency100() ) fRet = false;

    // ****************************************************************************************
    // L2 Span MemCpy 
    // ****************************************************************************************
    const int ciL2SMCStrides = (max(gtGlobal.iL2SizeBytes,gtGlobal.iL1SizeBytes)/sizeof(DWORD))/4;
    const tStridePattern ctspL2SMCLatency = { eMemCpyLoop, ciL2SMCStrides, 1 };
    tLatency tL2SMCLatency = tLatency( gtGlobal.iL2LineSize, ctspL2SMCLatency, gtGlobal.iTLBSize, 2*gtGlobal.dwPWLatency );
    tL2SMCLatency.Measure( gtGlobal.pdw1MBBase, pdw1MBHalfTop, pdw1MBHalfTop+1, gtGlobal.pdw1MBTop, &iGrandTotal );
    tL2SMCLatency.LogReport( "Cached L2-Span MemCpy" );
    PerfReport( _T("Cached L2-Span MemCpy ps"), 10*tL2SMCLatency.dwLatency100() );
    if ( 0 == tL2SMCLatency.dwLatency100() ) fRet = false;

    // ****************************************************************************************
    // Memory Span MemCpy 
    // ****************************************************************************************
    const int ciMSMCStrides = max( 1024*ci1K/sizeof(DWORD), 2*gtGlobal.iL2SizeBytes/sizeof(DWORD));                     // span more than L2
    const tStridePattern ctspMSMCLatency = { eMemCpyLoop, ciMSMCStrides, 1 };
    tLatency tMSMCLatency = tLatency( gtGlobal.iL2LineSize, ctspMSMCLatency, gtGlobal.iTLBSize, 2*gtGlobal.dwPWLatency );
    tMSMCLatency.SetExpected(2*ciFastestMemory/(gtGlobal.iL2LineSize/sizeof(DWORD)));
    tMSMCLatency.Measure( gtGlobal.pdw1MBBase, pdw1MBHalfTop, pdw1MBHalfTop+1, gtGlobal.pdw1MBTop, &iGrandTotal );
    tMSMCLatency.LogReport( "Cached Memory-Span MemCpy" );
    PerfReport( _T("Cached Memory-Span MemCpy ps"), 10*tMSMCLatency.dwLatency100() );
    if ( 0 == tMSMCLatency.dwLatency100() ) fRet = false;



/*  ******************************************************************************************************************************
    Expected L1 and L2 Latency relationships
    symbology:  ~ means equal within 5%.  << means more than 5% less than.  <<< means more than 50% less than.

    L1-No-Write-Miss-Allocate       L1Pld_W <<< L1_W                
    L1-Write-Thru                   L1Pld_W ~ L1_W   &&  L1_W  ~  L2_W          
    L1-Write-Behind                 L1Pld_W ~ L1_W   &&  L1_W <<< L2_W          
    unexpected                      L1Pld_W >> L1_W             
    unexpected                      L1_W >> L2_W            
                        
    L2-No-Write-Miss-Allocate       L2Pld_W <<< L2_W        
    L2-Write-Through                L2Pld_W ~ L2_W   &&  L2_W  ~  MB_W  
    L2-Write-Behind                 L2Pld_W ~ L2_W   &&  L2_W <<< MB_W  
    unexpected                      L2Pld_W >> L2_W     
    unexpected                      L2_W >> MB_W    
                    
    Cached Memory Bus Write Latency can be much faster than Read Latency.  
    1.  Writing to a SDRAM is faster than reading to it.  There are a wide variety of mechanisms in the CPU, 
        the memory controller and the RAM itself that can make this true.
    2.  if line strided MB_W << MB_R, then Store buffers may have come into play and allow only the actual words written
        to go on the memory bus rather than the whole cache line.  This can be true for No-Allocate-On-Write-Miss.  Unclear if
        this might be true for Allocate-on-write-miss unless the CPU noticed the stride pattern and avoided allocating.  
    3.  Some of this is an artifact of the test using one write per cache line length.  So the 2-D 16K and 32K versions of the tests 
        which stride 16K or 32K per access force the RAM to do a full Row select (RAS) for each access.
    ******************************************************************************************************************************
*/

    // ****************************************************************************************
    // Issue any relevant L1 Warnings.
    // ****************************************************************************************
    if ( bMuchLessThan( tL1WritePreloadedLatency.dwLatency100(), tL1WriteLatency.dwLatency100(), 0.5f ) )
    {   // No Allocate
        LogPrintf( "Info: L1 Write Latencies indicate No-Allocate-On-Write-Miss policy is in force for L1.  Consider evaluating an Allocate policy.\r\n" );
        gtGlobal.bL1WriteAllocateOnMiss = false;
    } 
    else if ( bApproximatelyEqual( tL1WritePreloadedLatency.dwLatency100(), tL1WriteLatency.dwLatency100(), ciTolerance ) &&
              bApproximatelyEqual( tL1WriteLatency.dwLatency100(), tL2WriteLatency.dwLatency100(), ciTolerance )
             )
    {   // Write-Through
        if ( gtGlobal.bThisCacheInfo && 0==(gtGlobal.tThisCacheInfo.dwL1Flags&CF_WRITETHROUGH) )
        {
            LogPrintf( "Warning: CacheInfo claims L1 is Write-Behind but latencies indicate it is Write-Through.  Consider evaluating the Write-Behind Policy if available.\r\n" );
        }
        else
        {
            LogPrintf( "Info: L1 Write Latencies indicate Write-Through policy is in force for L1.  Consider evaluating the Write-Behind Policy if available.\r\n" );
        }
    } 
    else if ( bApproximatelyEqual( tL1WritePreloadedLatency.dwLatency100(), tL1WriteLatency.dwLatency100(), ciTolerance ) &&
              bMuchLessThan( tL1WriteLatency.dwLatency100(), tL2WriteLatency.dwLatency100(), 0.75f )
             )
    {   // Write-Behind (aka Write-Back)
        if ( gtGlobal.bThisCacheInfo && 0!=(gtGlobal.tThisCacheInfo.dwL1Flags&CF_WRITETHROUGH) )
        {
            LogPrintf( "Warning: CacheInfo claims L1 is Write-Through but latencies indicate it is Write-Behind.\r\n" );
        }
    }
    else 
    {   
        LogPrintf( "Warning: Unexpected pattern of L1 Latencies.\r\n" );
    }

    // ****************************************************************************************
    // Issue any relevant L2 Warnings.
    // ****************************************************************************************
    if ( bMuchLessThan( tL2WritePreloadedLatency.dwLatency100(), tL2WriteLatency.dwLatency100(), 0.5f ) )
    {   // No Allocate
        LogPrintf( "Info: L2 Write Latencies indicate No-Allocate-On-Write-Miss policy is in force for L2.  Consider evaluating an Allocate policy.\r\n" );
        gtGlobal.bL2WriteAllocateOnMiss = false;
    } 
    else if ( bApproximatelyEqual( tL2WritePreloadedLatency.dwLatency100(), tL2WriteLatency.dwLatency100(), ciTolerance ) &&
              bApproximatelyEqual( tL1WriteLatency.dwLatency100(), tL2WriteLatency.dwLatency100(), ciTolerance )
             )
    {   // Write-Through
        if ( gtGlobal.bThisCacheInfo && 0==(gtGlobal.tThisCacheInfo.dwL2Flags&CF_WRITETHROUGH) )
        {
            LogPrintf( "Warning: CacheInfo claims L2 is Write-Behind but latencies indicate it is Write-Through.  Consider evaluating the Write-Behind Policy if available.\r\n" );
        }
        else
        {
            LogPrintf( "Info: L2 Write Latencies indicate Write-Through policy is in force for L2.  Consider evaluating the Write-Behind Policy if available.\r\n" );
        }
    } 
    else if ( bApproximatelyEqual( tL2WritePreloadedLatency.dwLatency100(), tL2WriteLatency.dwLatency100(), ciTolerance ) &&
              bMuchLessThan( tL2WriteLatency.dwLatency100(), t16KWriteLatency.dwLatency100(), 0.75f )
             )
    {   // Write-Behind (aka Write-Back)
        if ( gtGlobal.bThisCacheInfo && 0!=(gtGlobal.tThisCacheInfo.dwL2Flags&CF_WRITETHROUGH) )
        {
            LogPrintf( "Warning: CacheInfo claims L2 is Write-Through but latencies indicate it is Write-Behind.\r\n" );
        }
    }
    else 
    {   
        LogPrintf( "Warning: Unexpected pattern of L2 Write Latencies.\r\n" );
    }

    // ****************************************************************************************
    // Next create performance graphs representing an overview of memory performance
    // ****************************************************************************************

    if ( gtGlobal.bChartOverview )
    {   
        // -o generates several additional overviews of memory access in various ways with output to the CSV file.
        gtGlobal.bChartOverviewDetails = true;

        // ****************************************************************************************
        // Now generate an overview of the memory hierarchy latencies
        // ****************************************************************************************
        const int ciMinOVStrides = ( gtGlobal.iL1SizeBytes/4 )/gtGlobal.iL2LineSize;
        const int ciMaxOVStrides = max( 4*gtGlobal.iL2SizeBytes, ci1M ) / gtGlobal.iL2LineSize;
        tStridePattern tspOverview = { eReadLoop1D, ciMinOVStrides, gtGlobal.iL2LineSize/sizeof(DWORD) };
        tPartition tOverview = tPartition( gtGlobal.iL2LineSize, 
                                           ciMinOVStrides, ciMaxOVStrides, ciMinOVStrides,
                                           eSequential, eStepYStrides, tspOverview, 0 );
        tOverview.SetPageWalkLatency( gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
        tOverview.EffectiveSize( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
        tOverview.LogReport( "LineSize Stride Overview" );
        if ( gtGlobal.bExtraPrints )
        {
            tOverview.ShowAccessPattern( "LineSize Stride Overview", ciMinOVStrides, ciMinOVStrides, eShowGrid, 5*tspRASOverview.m_iXStrides );
            int iL1Strides = gtGlobal.iL1SizeBytes/gtGlobal.iL2LineSize;
            tOverview.ShowAccessPattern( "LineSize Stride Overview", iL1Strides-1*ciMinOVStrides, iL1Strides+1*ciMinOVStrides, eShowStatistics, 0 );
            int iL2Strides = gtGlobal.iL2SizeBytes/gtGlobal.iL2LineSize;
            if ( iL2Strides>0 )
            {
                tOverview.ShowAccessPattern( "LineSize Stride Overview", iL2Strides-1*ciMinOVStrides, iL2Strides+1*ciMinOVStrides, eShowStatistics, 0 );
            }
        }

        // ****************************************************************************************
        // Another overview with long strides trying to hit memory
        // ****************************************************************************************
        const int ciRowXStride = tRASOverview.fHighConfidence() > .60f ? tRASOverview.iMeasuredSize() : 16*ci1K;
        const int ciRowYStrides = ciRowXStride/gtGlobal.iL2LineSize;
        tStridePattern tspRowOverview = { eReadLoop2D, ciRowYStrides, gtGlobal.iL2LineSize/sizeof(DWORD), 4, ciRowXStride/sizeof(DWORD)  };
        tPartition tRowOverview = tPartition( gtGlobal.iL2LineSize, 
                                              4, 4*gtGlobal.iTLBSize, 2,
                                              eSequential, eStepXStrides, tspRowOverview, 0 );
        tRowOverview.SetPageWalkLatency( gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
        tRowOverview.EffectiveSize( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
        tRowOverview.LogReport( "SDRAM Row Stride Overview" );
        if ( gtGlobal.bExtraPrints )
        {
            tRowOverview.ShowAccessPattern( "SDRAM Row Stride Overview", gtGlobal.iTLBSize+2, gtGlobal.iTLBSize+2, eShowGrid, 10*gtGlobal.iTLBSize );
            tRowOverview.ShowAccessPattern( "SDRAM Row Stride Overview", gtGlobal.iTLBSize-4, gtGlobal.iTLBSize+4, eShowStatistics, 0 );
            tRowOverview.ShowAccessPattern( "SDRAM Row Stride Overview", (5*gtGlobal.iTLBSize/4)-4, (5*gtGlobal.iTLBSize/4)+4, eShowStatistics, 0 );
        }

        gtGlobal.bChartOverviewDetails = false;
    }

    gtGlobal.bCachedMemoryTested = fRet;

    return fRet;
}


// ***************************************************************************************************************
// 
// Main Uncached Memory Performance Tests - UnCachedMemoryPerf
//
// ***************************************************************************************************************

extern bool UnCachedMemoryPerf()
{
    bool fRet = true;       // assume all the tests will pass
    
    int iGrandTotal = 0;    // Keep very clever optimizing compilers at bay from trivializing the tests.

    if ( (!gtGlobal.bCachedMemoryTested) && 
          NULL != gtGlobal.pdwBase && 
          (gtGlobal.pdwTop-gtGlobal.pdwBase) > (1024-128)*ci1K/sizeof(DWORD) 
        )
    {
        // Repeat some measurement here that were primarily measured in the cached test

        // ****************************************************************************************
        // Cached Memory Bus Latency
        // ****************************************************************************************
        const int ciMBStrides = (1024-128)*ci1K/gtGlobal.iL2LineSize;
        const tStridePattern ctspMBLatency = { eReadLoop1D, ciMBStrides, gtGlobal.iL2LineSize/sizeof(DWORD) };
        tLatency tMBLatency = tLatency( gtGlobal.iL2LineSize, ctspMBLatency );
        tMBLatency.SetExpected(ciFastestMemory);
        tMBLatency.Measure( gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
        tMBLatency.LogReport( "Cached Memory Bus" );
        PerfReport( _T("Cached MBus Read ps"), 10*tMBLatency.dwLatency100() );
        gtGlobal.dwMBLatency    = tMBLatency.dwLatency100();
        gtGlobal.dwMBMinLatency = tMBLatency.dwLatency100();
        gtGlobal.dwMBMaxLatency = tMBLatency.dwLatency100();

        // ****************************************************************************************
        // Memory Bus Write Latency through Cache
        // ****************************************************************************************
        const int ciMBWStrides = (1024-128)*ci1K/gtGlobal.iL2LineSize;         // span more than L2
        const tStridePattern ctspMBWrite = { eWriteLoop1D, ciMBWStrides, gtGlobal.iL2LineSize/sizeof(DWORD) };
        tLatency tMBWriteLatency = tLatency( gtGlobal.iL2LineSize, ctspMBWrite  );
        tMBWriteLatency.SetExpected(ciFastestMemory/4);
        tMBWriteLatency.Measure( gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
        tMBWriteLatency.LogReport( "Cached Memory Bus" );
        gtGlobal.dwMBWriteLatency = tMBWriteLatency.dwLatency100();
        PerfReport( _T("Cached MBus Write Lat ps"),  10*tMBWriteLatency.dwLatency100() );
        gtGlobal.dwMBWriteLatency = tMBLatency.dwLatency100();
        gtGlobal.dwMBMinLatency = min( gtGlobal.dwMBMinLatency, tMBLatency.dwLatency100() );
        gtGlobal.dwMBMaxLatency = max( gtGlobal.dwMBMaxLatency, tMBLatency.dwLatency100() );
    }

    if ( NULL != gtGlobal.pdwUncached )
    {
        // test both small span (L1 size) and large span (>L2 size) to look for unexpected differences

        // ****************************************************************************************
        // Small Span Uncached Sequential Read Performance
        // ****************************************************************************************
        const tStridePattern ctspUCRead1 = { eReadLoop1D, gtGlobal.iL1SizeBytes/sizeof(DWORD), 1 };
        tLatency tUCRead1Latency = tLatency( gtGlobal.iL2LineSize, ctspUCRead1, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
        tUCRead1Latency.SetExpected(ciFastestMemory);
        tUCRead1Latency.Measure( gtGlobal.pdwUncached, gtGlobal.pdwUncachedTop, &iGrandTotal );
        tUCRead1Latency.LogReport( "Uncached Small-Span DWORD-Sequential Memory Bus", gtGlobal.dwMBLatency/(gtGlobal.iL2LineSize/sizeof(DWORD)), 4*gtGlobal.dwMBLatency );
        PerfReport( _T("Uncached DW SS Read Lat ps"), 10*tUCRead1Latency.dwLatency100());
        if ( 0 == tUCRead1Latency.dwLatency100() ) fRet = false;
        gtGlobal.dwMBMinLatency = min( gtGlobal.dwMBMinLatency, tUCRead1Latency.dwLatency100() );
        gtGlobal.dwMBMaxLatency = max( gtGlobal.dwMBMaxLatency, tUCRead1Latency.dwLatency100() );
        gtGlobal.dwUCMBLatency = tUCRead1Latency.dwLatency100();

        // ****************************************************************************************
        // Large Span Uncached Sequential Read Performance
        // ****************************************************************************************
        const int ciUCRead2Strides = max( 1024*ci1K/sizeof(DWORD), 2*gtGlobal.iL2SizeBytes/sizeof(DWORD));
        const tStridePattern ctspUCRead2 = { eReadLoop1D, ciUCRead2Strides, 1 };
        tLatency tUCRead2Latency = tLatency( gtGlobal.iL2LineSize, ctspUCRead2, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
        tUCRead2Latency.SetExpected(ciFastestMemory);
        tUCRead2Latency.Measure( gtGlobal.pdwUncached, gtGlobal.pdwUncachedTop, &iGrandTotal );
        tUCRead2Latency.LogReport( "Uncached Large-Span DWORD-Sequential Memory Bus", tUCRead1Latency.dwLatency100() );
        PerfReport( _T("Uncached DW LS Read Lat ps"),  10*tUCRead2Latency.dwLatency100());
        if ( 0 == tUCRead2Latency.dwLatency100() ) fRet = false;
        gtGlobal.dwMBMinLatency = min( gtGlobal.dwMBMinLatency, tUCRead2Latency.dwLatency100() );
        gtGlobal.dwMBMaxLatency = max( gtGlobal.dwMBMaxLatency, tUCRead2Latency.dwLatency100() );
        gtGlobal.dwUCMBLatency  = max( gtGlobal.dwUCMBLatency,  tUCRead2Latency.dwLatency100() );

        // ****************************************************************************************
        // Large Span Uncached Strided Read Performance
        // ****************************************************************************************
        const int ciUCRead3Strides = max( 1024*ci1K/gtGlobal.iL2LineSize, 2*gtGlobal.iL2SizeBytes/gtGlobal.iL2LineSize);
        const tStridePattern ctspUCRead3 = { eReadLoop1D, ciUCRead3Strides, gtGlobal.iL2LineSize/sizeof(DWORD) };
        tLatency tUCRead3Latency = tLatency( gtGlobal.iL2LineSize, ctspUCRead3, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
        tUCRead3Latency.SetExpected(ciFastestMemory);
        tUCRead3Latency.Measure( gtGlobal.pdwUncached, gtGlobal.pdwUncachedTop, &iGrandTotal );
        tUCRead3Latency.LogReport( "Uncached Large-Span LineSize-Strided Memory Bus", tUCRead1Latency.dwLatency100() );
        PerfReport( _T("Uncached Stride LS Read Lat ps"),  10*tUCRead3Latency.dwLatency100());
        if ( 0 == tUCRead3Latency.dwLatency100() ) fRet = false;
        gtGlobal.dwMBMinLatency = min( gtGlobal.dwMBMinLatency, tUCRead3Latency.dwLatency100() );
        gtGlobal.dwMBMaxLatency = max( gtGlobal.dwMBMaxLatency, tUCRead3Latency.dwLatency100() );
        gtGlobal.dwUCMBLatency  = max( gtGlobal.dwUCMBLatency,  tUCRead3Latency.dwLatency100() );

        // ****************************************************************************************
        // Small Span Uncached Sequential Write Performance
        // ****************************************************************************************
        const tStridePattern ctspUCWrite1 = { eWriteLoop1D, gtGlobal.iL1SizeBytes/sizeof(DWORD), 1 };
        tLatency tUCWrite1Latency = tLatency( gtGlobal.iL2LineSize, ctspUCWrite1, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
        tUCWrite1Latency.SetExpected(ciFastestMemory/4);
        tUCWrite1Latency.Measure( gtGlobal.pdwUncached, gtGlobal.pdwUncachedTop, &iGrandTotal );
        tUCWrite1Latency.LogReport( "Uncached Small-Span DWORD-Sequential Memory Bus", gtGlobal.dwMBWriteLatency/(gtGlobal.iL2LineSize/sizeof(DWORD)), 4*gtGlobal.dwMBWriteLatency );
        PerfReport( _T("Uncached DW SS Write Lat ps"),  10*tUCWrite1Latency.dwLatency100());
        if ( 0 == tUCWrite1Latency.dwLatency100() ) fRet = false;
        gtGlobal.dwMBMinWriteLatency = min( gtGlobal.dwMBMinWriteLatency, tUCWrite1Latency.dwLatency100() );
        gtGlobal.dwMBMaxWriteLatency = max( gtGlobal.dwMBMaxWriteLatency, tUCWrite1Latency.dwLatency100() );
        gtGlobal.dwUCMBWriteLatency = tUCWrite1Latency.dwLatency100();

        // ****************************************************************************************
        // Large Span Uncached Sequential Write Performance
        // ****************************************************************************************
        const int ciUCWrite2Strides = max( 1024*ci1K/sizeof(DWORD), 2*gtGlobal.iL2SizeBytes/sizeof(DWORD));
        const tStridePattern ctspUCWrite2 = { eWriteLoop1D, ciUCWrite2Strides, 1 };
        tLatency tUCWrite2Latency = tLatency( gtGlobal.iL2LineSize, ctspUCWrite2, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
        tUCWrite2Latency.SetExpected(ciFastestMemory/4);
        tUCWrite2Latency.Measure( gtGlobal.pdwUncached, gtGlobal.pdwUncachedTop, &iGrandTotal );
        tUCWrite2Latency.LogReport( "Uncached Large-Span DWORD-Sequential Memory Bus", tUCWrite1Latency.dwLatency100() );
        PerfReport( _T("Uncached DW LS Write Lat ps"),  10*tUCWrite2Latency.dwLatency100());
        if ( 0 == tUCWrite2Latency.dwLatency100() ) fRet = false;
        gtGlobal.dwMBMinWriteLatency = min( gtGlobal.dwMBMinWriteLatency, tUCWrite2Latency.dwLatency100() );
        gtGlobal.dwMBMaxWriteLatency = max( gtGlobal.dwMBMaxWriteLatency, tUCWrite2Latency.dwLatency100() );
        gtGlobal.dwUCMBWriteLatency  = max( gtGlobal.dwUCMBWriteLatency,  tUCWrite2Latency.dwLatency100() );

        // ****************************************************************************************
        // Large Span Uncached Strided Write Performance
        // ****************************************************************************************
        const int ciUCWrite3Strides = max( 1024*ci1K/gtGlobal.iL2LineSize, 2*gtGlobal.iL2SizeBytes/gtGlobal.iL2LineSize);
        const tStridePattern ctspUCWrite3 = { eWriteLoop1D, ciUCWrite3Strides, gtGlobal.iL2LineSize/sizeof(DWORD) };
        tLatency tUCWrite3Latency = tLatency( gtGlobal.iL2LineSize, ctspUCWrite3, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
        tUCWrite3Latency.SetExpected(ciFastestMemory/4);
        tUCWrite3Latency.Measure( gtGlobal.pdwUncached, gtGlobal.pdwUncachedTop, &iGrandTotal );
        tUCWrite3Latency.LogReport( "Uncached Large-Span LineSize-Strided Memory Bus", tUCWrite1Latency.dwLatency100() );
        PerfReport( _T("Uncached Stride LS Write Lat ps"),  10*tUCWrite3Latency.dwLatency100());
        if ( 0 == tUCWrite3Latency.dwLatency100() ) fRet = false;
        gtGlobal.dwMBMinWriteLatency = min( gtGlobal.dwMBMinWriteLatency, tUCWrite3Latency.dwLatency100() );
        gtGlobal.dwMBMaxWriteLatency = max( gtGlobal.dwMBMaxWriteLatency, tUCWrite3Latency.dwLatency100() );
        gtGlobal.dwUCMBWriteLatency  = max( gtGlobal.dwUCMBWriteLatency,  tUCWrite3Latency.dwLatency100() );

        // ****************************************************************************************
        // Memory Span Uncached MemCpy 
        // ****************************************************************************************
        DWORD * pdwUncachedHalfTop = gtGlobal.pdwUncached + ( gtGlobal.pdwUncachedTop+1-gtGlobal.pdwUncached )/2 - 1;

        const int ciUMSMCStrides = max( 768*ci1K/sizeof(DWORD), 2*gtGlobal.iL2SizeBytes/sizeof(DWORD));                     // span more than L2
        const tStridePattern ctspUMSMCLatency = { eMemCpyLoop, ciUMSMCStrides, 1 };
        tLatency tUMSMCLatency = tLatency( gtGlobal.iL2LineSize, ctspUMSMCLatency, gtGlobal.iTLBSize, 2*gtGlobal.dwPWLatency );
        tUMSMCLatency.SetExpected(2*ciFastestMemory/(gtGlobal.iL1LineSize/sizeof(DWORD)));
        tUMSMCLatency.Measure( gtGlobal.pdwUncached, pdwUncachedHalfTop, pdwUncachedHalfTop+1, gtGlobal.pdwUncachedTop, &iGrandTotal );
        tUMSMCLatency.LogReport( "UnCached Memory-Span MemCpy" );
        PerfReport( _T("UnCached Memory-Span MemCpy ps"), 10*tUMSMCLatency.dwLatency100() );
        if ( 0 == tUMSMCLatency.dwLatency100() ) fRet = false;

        if ( NULL != gtGlobal.pdwBase && gtGlobal.pdwUncached != gtGlobal.pdwBase )
        {
            // ****************************************************************************************
            // MemCpy from cached memory to uncached memory
            // ****************************************************************************************
            tLatency tC2UMCLatency = tLatency( gtGlobal.iL2LineSize, ctspUMSMCLatency, gtGlobal.iTLBSize, 2*gtGlobal.dwPWLatency );
            tC2UMCLatency.SetExpected(2*ciFastestMemory/(gtGlobal.iL1LineSize/sizeof(DWORD)));
            tC2UMCLatency.Measure( gtGlobal.pdwUncached, gtGlobal.pdwUncachedTop, gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
            tC2UMCLatency.LogReport( "Cached=>UnCached Memory-Span MemCpy" );
            PerfReport( _T("Cached=>UnCached MemCpy ps"), 10*tC2UMCLatency.dwLatency100() );
            if ( 0 == tC2UMCLatency.dwLatency100() ) fRet = false;

            // ****************************************************************************************
            // MemCpy from uncached memory to cached memory
            // ****************************************************************************************
            tLatency tU2CMCLatency = tLatency( gtGlobal.iL2LineSize, ctspUMSMCLatency, gtGlobal.iTLBSize, 2*gtGlobal.dwPWLatency );
            tU2CMCLatency.SetExpected(2*ciFastestMemory/(gtGlobal.iL1LineSize/sizeof(DWORD)));
            tU2CMCLatency.Measure( gtGlobal.pdwUncached, gtGlobal.pdwUncachedTop, gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
            tU2CMCLatency.LogReport( "UnCached=>Cached Memory-Span MemCpy" );
            PerfReport( _T("UnCached=>Cached MemCpy ps"), 10*tU2CMCLatency.dwLatency100() );
            if ( 0 == tU2CMCLatency.dwLatency100() ) fRet = false;
        }


        gtGlobal.bUncachedMemoryTested = fRet;
    }
    else
    {
        fRet = false;
    }

    return fRet;
}


// ***************************************************************************************************************
// 
// Main DDraw Surface Memory Performance Tests - DDrawMemoryPerf
//
// DDraw surfaces may be mapped into user space as cached, uncached strongly-ordered, or uncached un-ordered (also known as uncached buffered)
// much better performance is achieved by buffered or cached than strongly-ordered
// however, it is often easier to map something as strongly-ordered and forget it, so this is important for graphics performance
//
// ***************************************************************************************************************


extern bool DDrawMemoryPerf()
{
    bool fRet = true;       // assume all the tests will pass
    int iGrandTotal = 0;        // Keep very clever optimizing compilers at bay from trivializing the tests.

    #ifdef BUILD_WITH_DDRAW
        if ( gtGlobal.bVideoTest && ((gtGlobal.pdwTop-gtGlobal.pdwBase)*sizeof(DWORD))>=ciMinAllocSize )
        {
            if ( bDDSurfaceInitApp(0) )
            {
                // DDraw surfaces allow us to test uncached-buffered == uncached-unordered memory access
                DWORD * pdwDDSRead = NULL;
                DWORD * pdwDDSWrite = NULL;
                DWORD dwDDSReadSize = 0, dwDDSWriteSize = 0;
                if ( (!gtGlobal.bCachedMemoryTested) && 
                     NULL != gtGlobal.pdwBase && 
                     (gtGlobal.pdwTop-gtGlobal.pdwBase) > (1024-128)*ci1K/sizeof(DWORD) 
                    )
                {
                    // set thresholds for warnings if these tests have not already been run
                    // ****************************************************************************************
                    // Cached Memory Bus Latency
                    // ****************************************************************************************
                    const int ciMBStrides = (1024-128)*ci1K/gtGlobal.iL2LineSize;
                    const tStridePattern ctspMBLatency = { eReadLoop1D, ciMBStrides, gtGlobal.iL2LineSize/sizeof(DWORD) };
                    tLatency tMBLatency = tLatency( gtGlobal.iL2LineSize, ctspMBLatency );
                    tMBLatency.SetExpected(ciFastestMemory);
                    tMBLatency.Measure( gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
                    tMBLatency.LogReport( "Cached Memory Bus" );
                    PerfReport( _T("Cached MBus Read ps"), 10*tMBLatency.dwLatency100() );
                    if ( 0 == tMBLatency.dwLatency100() ) fRet = false;
                    gtGlobal.dwMBLatency    = tMBLatency.dwLatency100();
                    gtGlobal.dwMBMinLatency = tMBLatency.dwLatency100();
                    gtGlobal.dwMBMaxLatency = 4*tMBLatency.dwLatency100();

                    // ****************************************************************************************
                    // Memory Bus Write Latency through Cache
                    // ****************************************************************************************
                    const int ciMBWStrides = (1024-128)*ci1K/gtGlobal.iL2LineSize;         // span more than L2
                    const tStridePattern ctspMBWrite = { eWriteLoop1D, ciMBWStrides, gtGlobal.iL2LineSize/sizeof(DWORD) };
                    tLatency tMBWriteLatency = tLatency( gtGlobal.iL2LineSize, ctspMBWrite  );
                    tMBWriteLatency.SetExpected(ciFastestMemory/4);
                    tMBWriteLatency.Measure( gtGlobal.pdwBase, gtGlobal.pdwTop, &iGrandTotal );
                    tMBWriteLatency.LogReport( "Cached Memory Bus" );
                    gtGlobal.dwMBWriteLatency = tMBWriteLatency.dwLatency100();
                    PerfReport( _T("Cached MBus Write Lat ps"),  10*tMBWriteLatency.dwLatency100() );
                    if ( 0 == tMBWriteLatency.dwLatency100() ) fRet = false;
                    gtGlobal.dwMBWriteLatency = tMBLatency.dwLatency100();
                    gtGlobal.dwMBMinLatency = min( gtGlobal.dwMBMinLatency, tMBLatency.dwLatency100() );
                    gtGlobal.dwMBMaxLatency = max( gtGlobal.dwMBMaxLatency, 4*tMBLatency.dwLatency100() );
                }
                gtGlobal.dwMBMinLatency = (7*gtGlobal.dwMBMinLatency)/8;
                gtGlobal.dwMBMaxLatency = (9*gtGlobal.dwMBMaxLatency)/8;
                gtGlobal.dwMBMinWriteLatency = (7*gtGlobal.dwMBMinWriteLatency)/8;
                gtGlobal.dwMBMaxWriteLatency = (9*gtGlobal.dwMBMaxWriteLatency)/8;

                // ****************************************************************************************
                // lock the surface gives us a read pointer to it.
                // ****************************************************************************************
                if ( bDDSurfaceLock( eLockReadOnly, &pdwDDSRead, &dwDDSReadSize ) )
                {
                    TestCPULatency( &iGrandTotal );

                    // ****************************************************************************************
                    // DDraw Read-only Surface Sequential Read
                    // ****************************************************************************************
                    const tStridePattern ctspDDSRead1 = { eReadLoop1D, dwDDSReadSize/sizeof(DWORD) -1, 1 };
                    tLatency tDDS1Latency =  tLatency( gtGlobal.iL2LineSize, ctspDDSRead1, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
                    tDDS1Latency.SetExpected(ciFastestMemory/4);
                    tDDS1Latency.Measure( pdwDDSRead, pdwDDSRead + dwDDSReadSize/sizeof(DWORD) -1, &iGrandTotal );
                    tDDS1Latency.LogReport( "DDraw-RO DWORD-Sequential Memory Bus", gtGlobal.dwMBMinLatency, gtGlobal.dwMBMaxLatency  );
                    PerfReport( _T("DDraw-ROSuf Seq Read Lat ps"),  10*tDDS1Latency.dwLatency100());
                    if ( 0 == tDDS1Latency.dwLatency100() ) fRet = false;
                    gtGlobal.dwDSMBLatency = tDDS1Latency.dwLatency100();

                    // ****************************************************************************************
                    // DDraw Read-only Surface Strided Read
                    // ****************************************************************************************
                    const tStridePattern ctspDDSRead3 = { eReadLoop1D, dwDDSReadSize/gtGlobal.iL2LineSize -1, gtGlobal.iL2LineSize/sizeof(DWORD) };
                    tLatency tDDS3Latency = tLatency( gtGlobal.iL2LineSize, ctspDDSRead3, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
                    tDDS3Latency.SetExpected(ciFastestMemory/4);
                    tDDS3Latency.Measure( pdwDDSRead, pdwDDSRead + dwDDSReadSize/sizeof(DWORD) -1, &iGrandTotal );
                    tDDS3Latency.LogReport( "DDraw-RO LineSize-Strided Memory Bus", (tDDS1Latency.dwLatency100()*7)/8, gtGlobal.dwMBMaxLatency  );
                    PerfReport( _T("DDraw-ROSuf Strided Read Lat ps"),  10*tDDS3Latency.dwLatency100());
                    if ( 0 == tDDS3Latency.dwLatency100() ) fRet = false;

                    // ****************************************************************************************
                    // DDraw Read-only Surface MemCpy to cached memory
                    // ****************************************************************************************
                    const tStridePattern ctspDSMCLatency = { eMemCpyLoop, dwDDSReadSize/sizeof(DWORD) - 1, 1 };
                    tLatency tDSMCLatency = tLatency( gtGlobal.iL2LineSize, ctspDSMCLatency, gtGlobal.iTLBSize, 2*gtGlobal.dwPWLatency );
                    tDSMCLatency.SetExpected(2*ciFastestMemory/(gtGlobal.iL1LineSize/sizeof(DWORD)));
                    tDSMCLatency.Measure( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, pdwDDSRead, pdwDDSRead + dwDDSReadSize/sizeof(DWORD) -1, &iGrandTotal );
                    tDSMCLatency.LogReport( "DDraw-RO to Cached MemCpy" );
                    PerfReport( _T("DDraw-RO to Cached MemCpy ps"), 10*tDSMCLatency.dwLatency100() );
                    if ( 0 == tDSMCLatency.dwLatency100() ) fRet = false;

                    DDSurfaceUnLock();
                    gtGlobal.bDDrawMemoryTested = fRet;
                }

                // ****************************************************************************************
                // lock the surface gives us a write pointer to it.
                // ****************************************************************************************
                if ( bDDSurfaceLock( eLockWriteDiscard, &pdwDDSWrite, &dwDDSWriteSize ) )
                {
                    TestCPULatency( &iGrandTotal );

                    // ****************************************************************************************
                    // DDraw Write-only Surface Sequential Write
                    // ****************************************************************************************
                    const tStridePattern ctspDDSWrite2 = { eWriteLoop1D, dwDDSWriteSize/sizeof(DWORD) -1, 1 };
                    tLatency tDDS2Latency = tLatency( gtGlobal.iL2LineSize, ctspDDSWrite2, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
                    tDDS2Latency.SetExpected(ciFastestMemory/4);
                    tDDS2Latency.Measure( pdwDDSWrite, pdwDDSWrite + dwDDSWriteSize/sizeof(DWORD) -1, &iGrandTotal );
                    tDDS2Latency.LogReport( "DDraw-WO DWORD-Sequential Memory Bus", gtGlobal.dwMBMinWriteLatency, gtGlobal.dwMBMaxWriteLatency );
                    PerfReport( _T("DDraw-WO Seq Write Lat ps"),  10*tDDS2Latency.dwLatency100());
                    if ( 0 == tDDS2Latency.dwLatency100() ) fRet = false;
                    gtGlobal.dwDSMBWriteLatency = tDDS2Latency.dwLatency100();

                    // ****************************************************************************************
                    // DDraw Write-only Surface Strided Write
                    // ****************************************************************************************
                    const tStridePattern ctspDDSWrite4 = { eWriteLoop1D, dwDDSWriteSize/gtGlobal.iL2LineSize -2, gtGlobal.iL1LineSize/sizeof(DWORD) };
                    tLatency tDDS4Latency = tLatency( gtGlobal.iL2LineSize, ctspDDSWrite4, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
                    tDDS4Latency.SetExpected(ciFastestMemory/4);
                    tDDS4Latency.Measure( pdwDDSWrite, pdwDDSWrite + dwDDSWriteSize/sizeof(DWORD) -1, &iGrandTotal );
                    tDDS4Latency.LogReport( "DDraw-WO LineSize-Strided Memory Bus", (tDDS2Latency.dwLatency100()*7)/8, gtGlobal.dwMBMaxWriteLatency );
                    PerfReport( _T("DDraw-WO Strided Write Lat ps"),  10*tDDS4Latency.dwLatency100());
                    if ( 0 == tDDS4Latency.dwLatency100() ) fRet = false;

                    // ****************************************************************************************
                    // DDraw Write-only Surface MemCpy from cached memory
                    // ****************************************************************************************
                    const tStridePattern ctspDSMCLatency = { eMemCpyLoop, dwDDSWriteSize/sizeof(DWORD) - 1, 1 };
                    tLatency tDSMCLatency = tLatency( gtGlobal.iL2LineSize, ctspDSMCLatency, gtGlobal.iTLBSize, 2*gtGlobal.dwPWLatency );
                    tDSMCLatency.SetExpected(2*ciFastestMemory/(gtGlobal.iL1LineSize/sizeof(DWORD)));
                    tDSMCLatency.Measure( pdwDDSWrite, pdwDDSWrite + dwDDSWriteSize/sizeof(DWORD) -1, gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop,  &iGrandTotal );
                    tDSMCLatency.LogReport( "DDraw-WO from Cached MemCpy" );
                    PerfReport( _T("DDraw-WO from Cached MemCpy ps"), 10*tDSMCLatency.dwLatency100() );
                    if ( 0 == tDSMCLatency.dwLatency100() ) fRet = false;

                    DDSurfaceUnLock();
                    gtGlobal.bDDrawMemoryTested = fRet;
                }
                else
                {
                    gtGlobal.bDDrawMemoryTested = false;
                }

                // ****************************************************************************************
                // lock the surface gives us a read-write pointer to it.
                // ****************************************************************************************
                if ( bDDSurfaceLock( eLockReadWrite, &pdwDDSWrite, &dwDDSWriteSize ) )
                {
                    TestCPULatency( &iGrandTotal );

                    // ****************************************************************************************
                    // DDraw Read-Write Surface Sequential Read
                    // ****************************************************************************************
                    const tStridePattern ctspDDSRead1 = { eReadLoop1D, dwDDSWriteSize/sizeof(DWORD) -1, 1 };
                    tLatency tDDS1Latency =  tLatency( gtGlobal.iL2LineSize, ctspDDSRead1, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
                    tDDS1Latency.SetExpected(ciFastestMemory/4);
                    tDDS1Latency.Measure( pdwDDSWrite, pdwDDSWrite + dwDDSWriteSize/sizeof(DWORD) -1, &iGrandTotal );
                    tDDS1Latency.LogReport( "DDraw-RW DWORD-Sequential Memory Bus", gtGlobal.dwMBMinLatency, gtGlobal.dwMBMaxLatency  );
                    PerfReport( _T("DDraw-RW Seq Read Lat ps"),  10*tDDS1Latency.dwLatency100());
                    if ( 0 == tDDS1Latency.dwLatency100() ) fRet = false;
                    gtGlobal.dwDSMBLatency = tDDS1Latency.dwLatency100();

                    // ****************************************************************************************
                    // DDraw Read-Write Surface Sequential Write
                    // ****************************************************************************************
                    const tStridePattern ctspDDSWrite2 = { eWriteLoop1D, dwDDSWriteSize/sizeof(DWORD) -1, 1 };
                    tLatency tDDS2Latency = tLatency( gtGlobal.iL2LineSize, ctspDDSWrite2, gtGlobal.iTLBSize, gtGlobal.dwPWLatency );
                    tDDS2Latency.SetExpected(ciFastestMemory/4);
                    tDDS2Latency.Measure( pdwDDSWrite, pdwDDSWrite + dwDDSWriteSize/sizeof(DWORD) -1, &iGrandTotal );
                    tDDS2Latency.LogReport( "DDraw-RW DWORD-Sequential Memory Bus", gtGlobal.dwMBMinWriteLatency, gtGlobal.dwMBMaxWriteLatency );
                    PerfReport( _T("DDraw-RW Seq Write Lat ps"),  10*tDDS2Latency.dwLatency100());
                    if ( 0 == tDDS2Latency.dwLatency100() ) fRet = false;
                    gtGlobal.dwDSMBWriteLatency = tDDS2Latency.dwLatency100();

                    // ****************************************************************************************
                    // DDraw Read-Write Surface MemCpy from and to the surface
                    // ****************************************************************************************
                    const tStridePattern ctspDSMCLatency = { eMemCpyLoop, (dwDDSWriteSize/sizeof(DWORD))/2 - 65, 1 };
                    tLatency tDSMCLatency = tLatency( gtGlobal.iL2LineSize, ctspDSMCLatency, gtGlobal.iTLBSize, 2*gtGlobal.dwPWLatency );
                    tDSMCLatency.SetExpected(2*ciFastestMemory/(gtGlobal.iL1LineSize/sizeof(DWORD)));
                    tDSMCLatency.Measure( pdwDDSWrite, 
                                          pdwDDSWrite + (dwDDSWriteSize/sizeof(DWORD))/2 - 1, 
                                          pdwDDSWrite + (dwDDSWriteSize/sizeof(DWORD))/2, 
                                          pdwDDSWrite + dwDDSWriteSize/sizeof(DWORD) -1, 
                                          &iGrandTotal 
                                         );
                    tDSMCLatency.LogReport( "DDraw-RW Memory-Span MemCpy" );
                    PerfReport( _T("DDraw-RW memory-Span MemCpy ps"), 10*tDSMCLatency.dwLatency100() );
                    if ( 0 == tDSMCLatency.dwLatency100() ) fRet = false;

                    if ( NULL != gtGlobal.pdw1MBBase )
                    {
                        // ****************************************************************************************
                        // DDraw Read-Write Surface MemCpy to cached memory
                        // ****************************************************************************************
                        const tStridePattern ctspDSMCLatency1 = { eMemCpyLoop, dwDDSWriteSize/sizeof(DWORD) - 1, 1 };
                        tLatency tDSMCLatency1 = tLatency( gtGlobal.iL2LineSize, ctspDSMCLatency1, gtGlobal.iTLBSize, 2*gtGlobal.dwPWLatency );
                        tDSMCLatency1.SetExpected(2*ciFastestMemory/(gtGlobal.iL1LineSize/sizeof(DWORD)));
                        tDSMCLatency1.Measure( gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, pdwDDSWrite, pdwDDSWrite + dwDDSWriteSize/sizeof(DWORD) -1, &iGrandTotal );
                        tDSMCLatency1.LogReport( "DDraw-RW to Cached MemCpy" );
                        PerfReport( _T("DDraw-RW to Cached MemCpy ps"), 10*tDSMCLatency1.dwLatency100() );
                        if ( 0 == tDSMCLatency1.dwLatency100() ) fRet = false;

                        // ****************************************************************************************
                        // DDraw Read-Write Surface MemCpy from cached memory
                        // ****************************************************************************************
                        const tStridePattern ctspDSMCLatency2 = { eMemCpyLoop, dwDDSWriteSize/sizeof(DWORD) - 1, 1 };
                        tLatency tDSMCLatency2 = tLatency( gtGlobal.iL2LineSize, ctspDSMCLatency2, gtGlobal.iTLBSize, 2*gtGlobal.dwPWLatency );
                        tDSMCLatency2.SetExpected(2*ciFastestMemory/(gtGlobal.iL1LineSize/sizeof(DWORD)));
                        tDSMCLatency2.Measure( pdwDDSWrite, pdwDDSWrite + dwDDSWriteSize/sizeof(DWORD) -1, gtGlobal.pdw1MBBase, gtGlobal.pdw1MBTop, &iGrandTotal );
                        tDSMCLatency2.LogReport( "DDraw-RW from Cached MemCpy" );
                        PerfReport( _T("DDraw-RW from Cached MemCpy ps"), 10*tDSMCLatency2.dwLatency100() );
                        if ( 0 == tDSMCLatency2.dwLatency100() ) fRet = false;
                    }

                    DDSurfaceUnLock();
                    gtGlobal.bDDrawMemoryTested = fRet;
                }
                else
                {
                    gtGlobal.bDDrawMemoryTested = false;
                }

                DDSurfaceClose();
            }
        }
    #endif // BUILD_WITH_DDRAW

    return fRet;
}



// ***************************************************************************************************************
// some utility comparison functions needed only in this file
// ***************************************************************************************************************

bool bApproximatelyEqual( const DWORD cdw1, const DWORD cdw2, const float cfTol )
{
    int iDiff = cdw1 - cdw2;
    if ( iDiff < 0 ) 
        iDiff = - iDiff;
    int iThreshold = (int)( cfTol*min(cdw1,cdw2) );
    return iDiff < iThreshold;
}

bool bMuchLessThan( const DWORD cdw1, const DWORD cdw2, const float cfHowMuch )
{
    return ((float)cdw1) < (cfHowMuch*cdw2);
}

bool bApproximatelyLessThan( const DWORD cdw1, const DWORD cdw2, const float cfTol )
{
    return cdw1 < (cdw2*(1+cfTol));
}


// ***************************************************************************************************************
// WarmUpCPU is used to convince the power manager that it should ramp up the voltage and frequency
// ***************************************************************************************************************

void WarmUpCPU( const DWORD *pdwBase, const int ciMilliSeconds )
{   
    // right before we get started, create a big CPU load to ramp the CPU speed up but do so at "normal" priority level.
    DWORD dwStartTime = GetTickCount();
    const int ciLoadStrides = 1024*ci1K/64;         // span more than L2 we expect
    const tStridePattern ctspLoad = { eCPULoop, ciLoadStrides, 64/sizeof(DWORD) };
    int iCountLoadLoops = 0;
    do {
        iCPULoad( pdwBase, 25, ctspLoad );
        iCountLoadLoops++;
    } while ( (GetTickCount() - dwStartTime) < (DWORD)ciMilliSeconds );
    LogPrintf( "CPULoad ran %d loops.\r\n", iCountLoadLoops );
}


// ***************************************************************************************************************
// TestCPULatency is used several times above and encapsulates the work needed to measure the CPU speed
// ***************************************************************************************************************

void TestCPULatency( int * piGrandTotal )
{
    // repeat RAW CPU speed test looking for CPU frequency changes.
    tLatency tCPULatency = tLatency( ciAssumedLineSize, ctspCPULatency );
    tCPULatency.SetExpected(1);
    if ( NULL != gtGlobal.pdwBase )
    {
        tCPULatency.Measure( gtGlobal.pdwBase, gtGlobal.pdwTop, piGrandTotal );
    }
    else if ( NULL != gtGlobal.pdwUncached )
    {
        tCPULatency.Measure( gtGlobal.pdwUncached, gtGlobal.pdwUncachedTop, piGrandTotal );
    }
    else
    {
        // This can happen on a failure to allocate memory
        return;
    }
    tCPULatency.LogReport( "RAW Instruction" );
    PerfReport( _T("RAW Instruction Latency ps"),  10*tCPULatency.dwLatency100() );
    int iCPULatencyDiff =  tCPULatency.dwLatency100() - gtGlobal.iLastCPULatency;
    if ( iCPULatencyDiff < 0 ) 
        iCPULatencyDiff = -iCPULatencyDiff;
    if ( tCPULatency.dwLatency100() < gtGlobal.dwCPULatencyMin )
        gtGlobal.dwCPULatencyMin  = tCPULatency.dwLatency100();
    if ( iCPULatencyDiff > (int)(gtGlobal.dwCPULatencyMin/5) && gtGlobal.iLastCPULatency > 0 )
    {
        LogPrintf( "Warning: CPU Frequency Change Detected of size %.2f%% at check 1.\r\n", 
                    (100.0f*iCPULatencyDiff)/gtGlobal.dwCPULatencyMin
                  );
        gtGlobal.gdwCountFreqChanges++;
        if ( gtGlobal.bWarmUpCPU ) 
        {
            if ( NULL != gtGlobal.pdwBase )
            {
                WarmUpCPU( gtGlobal.pdwBase, 3000 );
                tCPULatency.Measure( gtGlobal.pdwBase, gtGlobal.pdwTop, piGrandTotal );
            }
            else if ( NULL != gtGlobal.pdwUncached )
            {
                WarmUpCPU( gtGlobal.pdwUncached, 3000 );
                tCPULatency.Measure( gtGlobal.pdwUncached, gtGlobal.pdwUncachedTop, piGrandTotal );
            }
            tCPULatency.LogReport( "RAW Instruction Post Warm-up" );
        }
    }
    gtGlobal.iLastCPULatency = tCPULatency.dwLatency100();
}


// ***************************************************************************************************************
// MemPerfBlockAlloc allocates the main buffers used by the test and initializes the global state gtGlobal
// ***************************************************************************************************************

bool MemPerfBlockAlloc( bool bNeedCached, bool bNeedUncached )
{
    // set up to allocate a very large buffer - sections of it will be reused for various individual tests
    // Size of region available for accessing
    const int ciAccessRange = 16*ci1M;
    const int ciTotalRange = ciAccessRange + (ciAccessRange/2);
    const int ciAllocSize  = ciTotalRange + 2*ci1M + 2*ciPageSize; 
    const int ciUncachedRange = 2*ci1M;
    // check that no allocated memory will get leaked
    assert( NULL == gtGlobal.pdwBase && NULL == gtGlobal.pdwUncached  );
    // start in a clean state
    gtGlobal.pdwBase = NULL;
    gtGlobal.pdwTop  = NULL;
    gtGlobal.pdwUncached = NULL;
    gtGlobal.pdwUncachedTop = NULL;
    gtGlobal.pdw1MBBase = NULL;
    UINT uiRand1 = 0;
    UINT uiRand2 = 0;

    if ( bNeedCached )
    {
        gtGlobal.pdwBase = (DWORD*) LocalAlloc(LMEM_FIXED, ciAllocSize);
        if (NULL == gtGlobal.pdwBase )
        {
            NKDbgPrintfW( L"First LocalAlloc(%d) failed\r\n", ciAllocSize);
            for( int i=2; i<=16 && (ciAllocSize/i)>=ciMinAllocSize; i *= 2 )
            {
                // try allocating smaller buffers for small systems
                gtGlobal.pdwBase = (DWORD*) LocalAlloc(LMEM_FIXED, ciAllocSize/i );
                if (NULL != gtGlobal.pdwBase )
                {
                    gtGlobal.pdwTop = gtGlobal.pdwBase + ((ciTotalRange/i)/sizeof(DWORD)) -1;
                    break;
                }
                NKDbgPrintfW( L"Secondary LocalAlloc(%d) failed\r\n", ciAllocSize/i );
            }
            if (NULL == gtGlobal.pdwBase )
                return false;
        }
        else
        {
            gtGlobal.pdwTop = gtGlobal.pdwBase + (ciTotalRange/sizeof(DWORD)) -1;
        }
    }

    if ( bNeedUncached )
    {
        gtGlobal.pdwUncached = (DWORD*) VirtualAlloc( NULL, ciUncachedRange+(ciUncachedRange/2), MEM_COMMIT, PAGE_READWRITE | PAGE_NOCACHE );
        if (NULL == gtGlobal.pdwUncached)
        {
            NKDbgPrintfW(L"VirtualAlloc Uncached failed\r\n");
            if ( !bNeedCached )
                return false;
        }
        else
        {
            gtGlobal.pdwUncachedTop = gtGlobal.pdwUncached + (ciUncachedRange/sizeof(DWORD)) -1;
        }
    }

    if (NULL == gtGlobal.pdwBase )
    {
        // We always need some memory for misc tests, use a small buffer in cached range
        gtGlobal.pdwBase = (DWORD*) LocalAlloc(LMEM_FIXED, ciMinAllocSize+100*sizeof(DWORD) );
        if (NULL == gtGlobal.pdwBase )
        {
            NKDbgPrintfW(L"Final attempt LocalAlloc failed\r\n");
            return false;
        }
        gtGlobal.pdwTop = gtGlobal.pdwBase + (ciMinAllocSize/sizeof(DWORD));
    }
    
    if ( gtGlobal.bSectionMapTest )
    {
        // Add code to get section mapped R/W memory here
        // Need to create a special driver which would get loaded into 0x80000000 - 0xBFFFFFFF and which would contain adequate memory in 1MB (contiguous) sections so that it could be made available
        // Note ce.bib mapping would need to look like nk.exe, kernel.dll and kitl.dll - e.g. NK  SHZ
        gtGlobal.bSectionMapTest = false;
        gtGlobal.pdw1MBBase = gtGlobal.pdwBase;
        gtGlobal.pdw1MBTop  = gtGlobal.pdwTop;
    }
    else
    {
        gtGlobal.pdw1MBBase = gtGlobal.pdwBase;
        gtGlobal.pdw1MBTop  = gtGlobal.pdwTop;
    }

    bool bFirstTime = !( gtGlobal.bCachedMemoryTested || gtGlobal.bUncachedMemoryTested || gtGlobal.bDDrawMemoryTested);

    if ( bFirstTime && gtGlobal.bFillFirst )
    {
        // fill with random junk
        int iTop = 0;
        if ( NULL != gtGlobal.pdwBase )
            iTop = gtGlobal.pdwTop + 1 - gtGlobal.pdwBase;
        else if ( NULL != gtGlobal.pdwUncached )
            iTop = gtGlobal.pdwUncachedTop + 1 - gtGlobal.pdwUncached;
        for( int dwIdx = 0; dwIdx<iTop; dwIdx++ )
        {
            rand_s(&uiRand1);
            rand_s(&uiRand2);
            DWORD dw = ((uiRand1<<17) | uiRand2) ^ (dwIdx<<15);
            if ( NULL != gtGlobal.pdwBase )
            {
                gtGlobal.pdwBase[dwIdx] = dw;
            }
            if (NULL != gtGlobal.pdwUncached && dwIdx<(ciUncachedRange/sizeof(DWORD)))
            {
                gtGlobal.pdwUncached[dwIdx] = dw;
            }
            if ( gtGlobal.bSectionMapTest )
            {
                gtGlobal.pdw1MBBase[dwIdx] = dw;
            }
        }
    }

    if ( bFirstTime )
    {
        // Use KernelIoControl to ask the ARM processor for its clock speed.
        PROCESSOR_INFO procInfo = {0};
        DWORD dwProcInfoSize;
        DWORD dwProcSpeed = 0;

        if ( KernelIoControl( IOCTL_PROCESSOR_INFORMATION, NULL, 0, &procInfo, sizeof(PROCESSOR_INFO), &dwProcInfoSize) )
        {
            LogPrintf( "ProcessorInfo: %S, %d, %S, %d, %S, %S, %d, %d\r\n",
                        procInfo.szProcessCore,
                        procInfo.wCoreRevision,
                        procInfo.szProcessorName,
                        procInfo.wProcessorRevision,
                        procInfo.szCatalogNumber,
                        procInfo.szVendor,
                        procInfo.dwInstructionSet,
                        procInfo.dwClockSpeed
                     );
            dwProcSpeed = procInfo.dwClockSpeed;
            if (dwProcSpeed > 1000000)
                dwProcSpeed /= 1000000;
        }

        gtGlobal.dwCPUMHz = dwProcSpeed;

        // get info about the system as reference
        SYSTEM_INFO tSystemInfo = {0};
        GetSystemInfo( &tSystemInfo );
        LogPrintf( "GetSystemInfo %d Processors 0x%x ActiveMask %s %d Level %d Revision %d\r\n",
                    tSystemInfo.dwNumberOfProcessors,
                    tSystemInfo.dwActiveProcessorMask,
                    PROCESSOR_ARCHITECTURE_ARM == tSystemInfo.wProcessorArchitecture ? "ARM" : "Other",
                    tSystemInfo.wProcessorArchitecture,
                    tSystemInfo.wProcessorLevel,
                    tSystemInfo.wProcessorRevision
                  );
                    
        // start out assume defaults
        gtGlobal.iL1LineSize  = ciAssumedLineSize;
        gtGlobal.iL1SizeBytes =  32*ci1K;
        gtGlobal.iL2LineSize  = ciAssumedLineSize;
        gtGlobal.iL2SizeBytes = 256*ci1K;

        if ( CeGetCacheInfo( sizeof(CacheInfo), &gtGlobal.tThisCacheInfo ) )
        {
            gtGlobal.bThisCacheInfo = true;
            gtGlobal.iL1LineSize  = gtGlobal.tThisCacheInfo.dwL1DCacheLineSize;
            gtGlobal.iL1SizeBytes = gtGlobal.tThisCacheInfo.dwL1DCacheSize;
            gtGlobal.iL2LineSize  = gtGlobal.tThisCacheInfo.dwL2DCacheLineSize;
            gtGlobal.iL2SizeBytes = gtGlobal.tThisCacheInfo.dwL2DCacheSize;
            LogPrintf( "CeGetCacheInfo L1 {%s,%s,%s} I: %d with %d byte lines and %d ways.  D: %d with %d byte lines and %d ways.\r\n",
                        gtGlobal.tThisCacheInfo.dwL1Flags&CF_UNIFIED ? "Unified" : "Separate",
                        gtGlobal.tThisCacheInfo.dwL1Flags&CF_WRITETHROUGH ? "Write-Through" : "Write-Behind",
                        gtGlobal.tThisCacheInfo.dwL1Flags&CF_COHERENT ? "Coherent" : "Flushes_Needed",
                        gtGlobal.tThisCacheInfo.dwL1ICacheSize,
                        gtGlobal.tThisCacheInfo.dwL1ICacheLineSize,
                        gtGlobal.tThisCacheInfo.dwL1ICacheNumWays,
                        gtGlobal.tThisCacheInfo.dwL1DCacheSize,
                        gtGlobal.tThisCacheInfo.dwL1DCacheLineSize,
                        gtGlobal.tThisCacheInfo.dwL1DCacheNumWays 
                      );
            LogPrintf( "CeGetCacheInfo L2 {%s,%s,%s} I: %d with %d byte lines and %d ways.  D: %d with %d byte lines and %d ways.\r\n",
                        gtGlobal.tThisCacheInfo.dwL2Flags&CF_UNIFIED ? "Unified" : "Separate",
                        gtGlobal.tThisCacheInfo.dwL2Flags&CF_WRITETHROUGH ? "Write-Through" : "Write-Behind",
                        gtGlobal.tThisCacheInfo.dwL2Flags&CF_COHERENT ? "Coherent" : "Flushes_Needed",
                        gtGlobal.tThisCacheInfo.dwL2ICacheSize,
                        gtGlobal.tThisCacheInfo.dwL2ICacheLineSize,
                        gtGlobal.tThisCacheInfo.dwL2ICacheNumWays,
                        gtGlobal.tThisCacheInfo.dwL2DCacheSize,
                        gtGlobal.tThisCacheInfo.dwL2DCacheLineSize,
                        gtGlobal.tThisCacheInfo.dwL2DCacheNumWays 
                      );
        }

        // assume these since CacheInfo does not tell us.
        gtGlobal.bL1WriteAllocateOnMiss = true;
        gtGlobal.bL2WriteAllocateOnMiss = true;
        gtGlobal.iL1ReplacementPolicy = 0;
        gtGlobal.iL2ReplacementPolicy = 0;


        // init these to values which will suppress the warnings.
        gtGlobal.dwMBMinLatency = 0;
        gtGlobal.dwMBMaxLatency = INT_MAX/16;
        gtGlobal.dwMBMinWriteLatency = 0;
        gtGlobal.dwMBMaxWriteLatency = INT_MAX/16;
        
        // init these to reasonable defaults
        gtGlobal.dwPWLatency = 0;
        gtGlobal.dwMBLatency = 300;
        gtGlobal.dwL1Latency = 1;
        gtGlobal.dwL1WriteLatency = 1;

        gtGlobal.dwCPULatencyMin = INT_MAX/16;
    }

    gtGlobal.gdTotalTestLoopUS = 0;

    if ( gtGlobal.bWarmUpCPU ) 
    {
        WarmUpCPU( NULL!=gtGlobal.pdwBase ? gtGlobal.pdwBase : gtGlobal.pdwUncached, 4000 );
    }

    int iGrandTotal = 0;
    TestCPULatency( &iGrandTotal );

    // knowing the QPC overhead is nice info - largely a single kernel thunk unless the QPC counters can be mapped to user memory
    DWORD dwStart = GetTickCount();
    int iCnt = 0;
    while( (GetTickCount() - dwStart) < 3000 )
    {
        for( int i = 0; i<1000; i++, iCnt++ )
        {
            __int64 i64Ticks = 0;       
            QueryPerformanceCounter( (LARGE_INTEGER*)&i64Ticks );
        }
    }
    LogPrintf( "%6.2f ns QPC Overhead per call\r\n", 3000000000.0f/iCnt );


    return true;
}


// ***************************************************************************************************************
// MemPerfBlockFree fress memory and can print some warnings about the CPU frequency changing
// ***************************************************************************************************************

void MemPerfBlockFree()
{
    // repeat RAW CPU speed test looking for CPU frequency changes.
    int iGrandTotal = 0;
    TestCPULatency( &iGrandTotal );

    if ( 0 == gtGlobal.gdwCountFreqChanges )
    {
        // if we have not noticed a change in frequency, try sleeping half-a-second and measuring one last time.
        Sleep(500);    
        TestCPULatency( &iGrandTotal );
        if ( gtGlobal.gdwCountFreqChanges > 0 )
        {
            // don't count this one preceded by a 1/2 second sleep if it is the only one.  Just note it.
            gtGlobal.gdwCountFreqChanges = 0;
        }
    }
    if ( gtGlobal.gdwCountFreqChanges > 0 && gtGlobal.bWarnings)
    {
        LogPrintf( "Warning: These tests depend on a constant CPU clock speed for accuracy.\r\n");
        LogPrintf( "Warning: QPC Frequency or Measured RAW Instruction Rate changed %d times during these tests.\r\n", gtGlobal.gdwCountFreqChanges );
        LogPrintf( "Warning: Measurements taken during QPC changes were discarded but all results should be considered suspect.\r\n" );
        LogPrintf( "Warning: It is recommended to run these tests with power management disabled.\r\n");
        LogPrintf( "Warning: If Power Management must be enabled, for more reliable results,\r\n");
        LogPrintf( "Warning:   try -l 152 or -l 224 to allow Power Management to respond to CPU load.\r\n");
        LogPrintf( "Warning:   and try -s 0 to avoid short sleeps\r\n" );
    }

    if ( NULL != gtGlobal.pdwBase )
    {
        LocalFree( gtGlobal.pdwBase );
        gtGlobal.pdwBase = NULL;
        gtGlobal.pdwTop  = NULL;
        gtGlobal.pdw1MBBase = NULL;
        gtGlobal.pdw1MBTop  = NULL;
    }
    if (NULL != gtGlobal.pdwUncached)
    {
        VirtualFree( gtGlobal.pdwUncached, 0, MEM_RELEASE );
        gtGlobal.pdwUncached = NULL;
        gtGlobal.pdwUncachedTop = NULL;
    }

    return;
}

