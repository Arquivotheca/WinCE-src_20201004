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
// These functions implement the behavior of class tLatency
// intent is to encapsulate the methods need to properly measure latency
//
// ***************************************************************************************************************


#pragma warning( disable : 25032 25033 )  // prefast level 3 with false positives in several places below

#include <math.h>
#include "MemoryPerf.h"
#include "Latency.h"
#include "DDrawSurface.h"
#include "cache.h"


// ***************************************************************************************************************
// forward declarations
// ***************************************************************************************************************

DWORD dwDuration( const tStridePattern &ctStrideInfo, 
                  _Inout_ptrdiff_count_(pdwTop1) DWORD * const pdwBase1, 
                  DWORD * const pdwTop1, 
                  _In_opt_ptrdiff_count_(pdwTop2) const DWORD * pdwBase2, 
                  const DWORD * pdwTop2, 
                  const int ciRepeats, 
                  int *piGrandTotal );


// ***************************************************************************************************************
// constructors
// ***************************************************************************************************************
tLatency::tLatency()
{
    m_iLineSize = 0;
    m_tsp.m_eTL = eReadLoop1D;
    m_tsp.m_iYStrides = 0;
    m_tsp.m_iYdwStride = 0;
    m_tsp.m_iXStrides = 0;
    m_tsp.m_iXdwStride = 0;
    m_tsp.m_fPreloadPercent = 0;
    m_pdwBase = m_pdwTop = NULL;
    m_dwLatency100 = 0;
    m_dwMeanLatency100 = 0;
    m_dwStdDevLatency100 = 0;
    m_dwTotalAccesses = 0;
    m_iTLBSize = 0;
    m_dwPageWalkLatency = 0;
    m_dwPWCorrection = 0;
    m_dwExpected = 2;
    m_iValidMeasurements = 0;
}

tLatency::tLatency( const int ciLineSize, const tStridePattern &ctsp )
{
    m_iLineSize = ciLineSize;
    m_tsp = ctsp;
    m_pdwBase = m_pdwTop = NULL;
    m_dwLatency100 = 0;
    m_dwMeanLatency100 = 0;
    m_dwStdDevLatency100 = 0;
    m_dwTotalAccesses = 0;
    m_iTLBSize = 0;
    m_dwPageWalkLatency = 0;
    m_dwPWCorrection = 0;
    m_dwExpected = 2;
    m_iValidMeasurements = 0;
}

tLatency::tLatency( const int ciLineSize, const tStridePattern &ctsp, const int ciTLBSize, const DWORD cdwPWLatency )
{
    m_iLineSize = ciLineSize;
    m_tsp = ctsp;
    m_pdwBase = m_pdwTop = NULL;
    m_dwLatency100 = 0;
    m_dwMeanLatency100 = 0;
    m_dwStdDevLatency100 = 0;
    m_dwTotalAccesses = 0;
    m_iTLBSize = ciTLBSize;
    m_dwPageWalkLatency = cdwPWLatency;
    m_dwPWCorrection = 0;
    m_dwExpected = 2;
    m_iValidMeasurements = 0;
}

// ***************************************************************************************************************
// a utility method needed by effective size routines.
// ***************************************************************************************************************
void tLatency::CopyLatencies( _Out_bytecap_(cbDst) void *pDst, const unsigned int cbDst, const int iStride ) const
{
    if ( NULL == pDst || cbDst < ciTestLotSize*iStride*sizeof(DWORD))
        return;
    DWORD* pdwDst = (DWORD*) pDst;
    for( int i = 0; i < ciTestLotSize; i++ )
    {
        memset( &pdwDst[ iStride*i ], 0, iStride*sizeof(DWORD) );
        DWORD dwLatency = (m_pdwLatencies[i] > m_dwPWCorrection) ? (m_pdwLatencies[i] - m_dwPWCorrection) : m_pdwLatencies[i];
        if ( i >= m_iValidMeasurements )
            dwLatency = 0;
        pdwDst[ iStride*i ] = m_dwTotalAccesses > 0 ? (DWORD)(((100.0f*1000.0f) * dwLatency) / m_dwTotalAccesses) : dwLatency;
    }
}


// ***************************************************************************************************************
// top-level method for measuring latency
// first one is used for unidirectional tests like read or write
// ***************************************************************************************************************

void tLatency::Measure( DWORD * pdwBase, DWORD * pdwTop, int * piGrandTotal )
{
    if ( m_dwExpected < 1 )
    {
        m_dwExpected = 1;
    }
    if ( m_tsp.m_iYStrides <= 0 )
    {
        assert( m_tsp.m_iYStrides > 0 );
        m_dwLatency100 = 0;
        m_dwPWCorrection = 0;
        return;
    }
    m_pdwBase = pdwBase;
    m_pdwTop  = pdwTop;
    int iRepeats = 2000000 / ( m_dwExpected * m_tsp.uiAccessesPerPattern() );
    if ( iRepeats <= 2 )
        iRepeats = 2;
    if ( iRepeats > 5 )
    {
        iRepeats = iRoundUp( iRepeats, iRepeats <= 25 ? 5 : ( iRepeats <= 100 ? 10 : 50 ) );
    }
    MedianLatency100( ciTestLotSize, pdwBase, pdwTop, NULL, NULL, iRepeats, piGrandTotal );
    m_dwPWCorrection = 0;
    if ( m_dwPageWalkLatency > 0 && m_iTLBSize > 0)
    {
        const int ciPageOrSectionSize = ((DWORD*)0x80000000 <= pdwBase && pdwBase < (DWORD*)0xC0000000) ? ci1M : ciPageSize;
        double dPWR = m_tsp.dPageWalksRatio( ciPageOrSectionSize, m_iTLBSize );
        if ( dPWR > 0.0f )
        {
            DWORD dwPWCorrection = (DWORD)(dPWR*m_dwPageWalkLatency); 
            if ( m_dwLatency100 > dwPWCorrection )
            {
                m_dwLatency100 -= dwPWCorrection;
                m_dwPWCorrection = dwPWCorrection;
                if ( m_dwMeanLatency100 > dwPWCorrection )
                {   // only correct the mean if also correcting the median
                    m_dwMeanLatency100 -= dwPWCorrection;
                }
            }
        }
    }
}


// ***************************************************************************************************************
// top-level method for measuring latency
// this one is used for bi-directional tests like memcpy which access two regions of memory
// ***************************************************************************************************************

void tLatency::Measure( DWORD * pdwDstBase, DWORD * pdwDstTop, DWORD * pdwSrcBase, DWORD * pdwSrcTop, int * piGrandTotal )
{
    if ( m_dwExpected < 1 )
    {
        m_dwExpected = 1;
    }
    if ( m_tsp.m_iYStrides <= 0 )
    {
        assert( m_tsp.m_iYStrides > 0 );
        m_dwLatency100 = 0;
        m_dwPWCorrection = 0;
        return;
    }
    m_pdwBase = pdwDstBase;
    m_pdwTop  = pdwDstTop;
    int iRepeats = 2000000 / ( m_dwExpected * m_tsp.uiAccessesPerPattern() );
    if ( iRepeats <= 2 )
        iRepeats = 2;
    if ( iRepeats > 5 )
    {
        iRepeats = iRoundUp( iRepeats, iRepeats <= 25 ? 5 : ( iRepeats <= 100 ? 10 : 50 ) );
    }
    MedianLatency100( ciTestLotSize, pdwDstBase, pdwDstTop, pdwSrcBase, pdwSrcTop, iRepeats, piGrandTotal );
    m_dwPWCorrection = 0;
    if ( m_dwPageWalkLatency > 0 && m_iTLBSize > 0)
    {
        const int ciPageOrSectionSize = ((DWORD*)0x80000000 <= pdwDstBase && pdwDstBase < (DWORD*)0xC0000000) ? ci1M : ciPageSize;
        double dPWR = m_tsp.dPageWalksRatio( ciPageOrSectionSize, m_iTLBSize );
        if ( dPWR > 0.0f )
        {
            DWORD dwPWCorrection = (DWORD)(dPWR*m_dwPageWalkLatency); 
            if ( m_dwLatency100 > dwPWCorrection )
            {
                m_dwLatency100 -= dwPWCorrection;
                m_dwPWCorrection = dwPWCorrection;
                if ( m_dwMeanLatency100 > dwPWCorrection )
                {   // only correct the mean if also correcting the median
                    m_dwMeanLatency100 -= dwPWCorrection;
                }
            }
        }
    }
}


// ***************************************************************************************************************
// Simple and most commonly used LogReport method
// ***************************************************************************************************************

void tLatency::LogReport( const char * pstrName )
{
    if ( 0 == m_dwLatency100 )
    {
        LogPrintf( "Error: %s %s Latency not measured.\r\n",
                    pstrName, 
                    pstrReadWrite[m_tsp.m_eTL]
                  );
    }
    else if ( eCPULoop == m_tsp.m_eTL )
    {
        // different CPUs have different loop execution characteristics for the 16 dependent add loop.  Include a few typical ones.
        LogPrintf( "%6.2f ns %s %s Latency [Mean = %.2f, SD = %.2f, Cnt = %d] (17/16)-MHz = %.1f, (10/16)-MHz = %.1f, (9/16)-MHz = %.1f \r\n", 
                    0.01f*m_dwLatency100,
                    pstrName, 
                    pstrReadWrite[m_tsp.m_eTL], 
                    0.01f*m_dwMeanLatency100,
                    0.01f*m_dwStdDevLatency100,
                    m_iValidMeasurements,
                    (17*100000.0f/16)/m_dwLatency100,
                    (10*100000.0f/16)/m_dwLatency100,
                    ( 9*100000.0f/16)/m_dwLatency100
                  );
        ChartDetails( pstrName );
    }
    else if ( m_dwPWCorrection > 0 )
    {
        LogPrintf( "%6.2f ns %s %s Latency for Span %u B (corrected by - %.2f) [Mean = %.2f, SD = %.2f, Cnt = %d ]\r\n", 
                    0.01f*m_dwLatency100, 
                    pstrName, 
                    pstrReadWrite[m_tsp.m_eTL], 
                    m_tsp.uiSpanSizeBytes(),
                    0.01f*m_dwPWCorrection,
                    0.01f*m_dwMeanLatency100,
                    0.01f*m_dwStdDevLatency100,
                    m_iValidMeasurements
                  );
        ChartDetails( pstrName );
    }
    else
    {
        LogPrintf( "%6.2f ns %s %s Latency for Span %u B [Mean = %.2f, SD = %.2f, Cnt = %d ]\r\n", 
                    0.01f*m_dwLatency100,
                    pstrName, 
                    pstrReadWrite[m_tsp.m_eTL], 
                    m_tsp.uiSpanSizeBytes(),
                    0.01f*m_dwMeanLatency100,
                    0.01f*m_dwStdDevLatency100,
                    m_iValidMeasurements
                  );
        ChartDetails( pstrName );
    }
}


// ***************************************************************************************************************
// LogReport function which can also print a warning if the value is not close to an expected value
// ***************************************************************************************************************

void tLatency::LogReport( const char * pstrName, DWORD dwCompareable )
{
    LogReport( pstrName );
    DWORD dwAllowDev = m_dwStdDevLatency100>0 ? m_dwStdDevLatency100 : (dwCompareable+m_dwLatency100)/20;
    dwAllowDev = 2*dwAllowDev;
    if ( dwAllowDev < (m_dwLatency100/8) )
        dwAllowDev = (m_dwLatency100/8);
    if ( ( m_dwLatency100 < (dwCompareable-dwAllowDev) || (dwCompareable+dwAllowDev) < m_dwLatency100 ) && gtGlobal.bWarnings )
    {
        LogPrintf( "Warning: %s Difference between %.2f and %.2f larger than %.2f\r\n",
                   pstrName,
                   0.01f*dwCompareable,
                   0.01f*m_dwLatency100,
                   0.01f*dwAllowDev
                  );
    }
}


// ***************************************************************************************************************
// LogReport method which can print a warning if the value is not within an expected range
// ***************************************************************************************************************

void tLatency::LogReport( const char * pstrName, DWORD dwOKLow, DWORD dwOKHigh )
{
    LogReport( pstrName );
    if ( ( m_dwLatency100 < dwOKLow || dwOKHigh < m_dwLatency100 ) && gtGlobal.bWarnings )
    {
        if ( dwOKHigh < INT_MAX/16 )
        {
            LogPrintf( "Warning: %s = %.2f not in expected range %.2f..%.2f\r\n",
                       pstrName,
                       0.01f*m_dwLatency100,
                       0.01f*dwOKLow,
                       0.01f*dwOKHigh
                      );
        }
        else
        {
            LogPrintf( "Warning: %s = %.2f not in expected range above %.2f\r\n",
                       pstrName,
                       0.01f*m_dwLatency100,
                       0.01f*dwOKLow
                      );
        }
    }
}


// ***************************************************************************************************************
// Method to output details to the CSV file for charting or analysis
// ***************************************************************************************************************

void tLatency::ChartDetails( const char * pstrName )
{
    if ( 0 == m_dwLatency100 )
    {
        CSVPrintf( "Error: %s %s Latency not measured.\r\n",
                    pstrName, 
                    pstrReadWrite[m_tsp.m_eTL]
                  );
    }
    else 
    {
        CSVPrintf( "Latency, %s, %s, Run, Latency, Median=%.2f, Mean=%.2f, SD=%.2f, Cnt=%d, Correction=%.2f\r\n",
                    pstrName, 
                    pstrReadWrite[m_tsp.m_eTL], 
                    0.01f*m_dwLatency100, 
                    0.01f*m_dwMeanLatency100,
                    0.01f*m_dwStdDevLatency100,
                    m_iValidMeasurements,
                    0.01f*m_dwPWCorrection
                  );
        for( int i = 0; i < m_iValidMeasurements; i++ )
        {
            DWORD dwLatency100 = m_dwTotalAccesses > 0 ? (DWORD)(((100.0f*1000.0f) * m_pdwLatencies[i]) / m_dwTotalAccesses) : m_pdwLatencies[i];
            CSVPrintf( "Latency, %s, %s, %d, %.2f\r\n", 
                    pstrName, 
                    pstrReadWrite[m_tsp.m_eTL],
                    i,
                    0.01f*dwLatency100-0.01f*m_dwPWCorrection
                    );
        }
    }
}


// ***************************************************************************************************************
// MedianLatency100 will measure the latency ciLots==16 times and evaluate the median of that set of measurements
//
// Calculates the Median Latency in nanoseconds per 100 accesses
//
// The Test Loop being measured is specified by eTL using the stride access pattern ctStrideInfo which allows for 1D and 2D access patterns.
// ciRepeats specifies the number of times the same Access Pattern is repeated so the measurement time should be > 1000 us.
// ciLots specifies the number of independent tests to run to estimate the Median.
// ***************************************************************************************************************

#pragma warning( push )
#pragma warning( disable : 4127 )
void tLatency::MedianLatency100( const int ciLots, DWORD *pdwBase1, DWORD *pdwTop1, DWORD *pdwBase2, DWORD *pdwTop2, const int ciRepeats, int *pGrandTotal )
{
    const int eTL = m_tsp.m_eTL;
    if ( ciLots <= 0 )
    {
        assert( ciLots > 0 );
        return;
    }
    if ( !( m_tsp.bVerifyAccessRange( pdwTop1-pdwBase1 ) && 
            ( NULL==pdwBase2 || m_tsp.bVerifyAccessRange( pdwTop2-pdwBase2 ) )
           )
        )
    {
        LogPrintf( "Error: Stride Pattern out of range {%d,%d, %d,%d} for buffer size %d\r\n",
                    m_tsp.m_iYStrides, m_tsp.m_iYdwStride*sizeof(DWORD), 
                    m_tsp.m_iXStrides, m_tsp.m_iXdwStride*sizeof(DWORD),
                    (pdwTop1-pdwBase1)*sizeof(DWORD)
                   );
        assert( 0 );
        return;
    }
    assert( fabs( (ciRepeats * m_tsp.uiAccessesPerPattern()) - (((double)ciRepeats) * m_tsp.uiAccessesPerPattern()) ) < 1.1 );
    DWORD dwTotalAccesses = ciRepeats * m_tsp.uiAccessesPerPattern();
    if ( dwTotalAccesses <= 0 )
    {
        assert( 0 );
        return;
    }

    double dTotalDur = 0, dTotalDurSqr = 0;         // float does not handle enough precision 
    int l;
    int iRepeats = ciRepeats;
    int iValidIdx = 0;

    do
    {
        for ( l = 0; l < ciLots; l++ )
        {
            // rebase each iteration to hit cache differently - e.g. different virtual to physical mapping
            DWORD * const pdwThisBase1 =  m_tsp.pdwOffsetBase( pdwBase1, pdwTop1 );
            DWORD * const pdwThisBase2 =  NULL!=pdwBase2 ? m_tsp.pdwOffsetBase( pdwBase2, pdwTop2 ) : NULL;

            m_pdwLatencies[iValidIdx] = dwDuration( m_tsp, pdwThisBase1, pdwTop1, pdwThisBase2, pdwTop2, iRepeats, pGrandTotal );

            if ( m_pdwLatencies[iValidIdx] > 0 )
            {
                #ifdef SIMULATE
                    // increase the durations according to the desired simulation
                    switch( eTL )
                    {
                    case eReadLoop2D:  case eWriteLoop2D:  case eReadLoop3D:
                        if ( ( gtspThreshold.m_iXStrides  > 0 && m_tsp.m_iXStrides  > gtspThreshold.m_iXStrides  ) ||
                             ( gtspThreshold.m_iXdwStride > 0 && m_tsp.m_iXdwStride > gtspThreshold.m_iXdwStride ) )
                            m_pdwLatencies[iValidIdx] += m_pdwLatencies[iValidIdx]/2;
                        // fall through
                    default:
                        if ( ( gtspThreshold.m_iYStrides  > 0 && m_tsp.m_iYStrides  > gtspThreshold.m_iYStrides  ) ||
                             ( gtspThreshold.m_iYdwStride > 0 && m_tsp.m_iYdwStride > gtspThreshold.m_iYdwStride ) )
                            m_pdwLatencies[iValidIdx] += m_pdwLatencies[iValidIdx]/2;
                    }
                #endif

                dTotalDur += m_pdwLatencies[iValidIdx];
                dTotalDurSqr += ((double)m_pdwLatencies[iValidIdx])*m_pdwLatencies[iValidIdx];
                iValidIdx++;
            }
        }
        m_iValidMeasurements = iValidIdx;
        if ( m_iValidMeasurements <= 0 )
            break;
        DWORD dwMean = (DWORD)(dTotalDur/m_iValidMeasurements);
        if ( dwMean >= 1000 )
            break;
        // duration is not large enough so we must increase the number of repeats.
        int iDelta = dwMean > 0 ? (2000 / dwMean) : 2;
        if ( iDelta < (iRepeats/10) )
            iDelta = iRepeats/10;
        if ( iDelta <= 1 )
            iDelta = 2;
        iRepeats += iDelta;
        // Ideally, we should not have to adapt ciRepeats, but just in case issue a warning.
        if ( gtGlobal.bWarnings )
        {
            LogPrintf( "Warning: dwLatency() duration too small %d us. Increasing Repeats by %d to %d for %s %d %d %d %d\r\n",
                       dwMean, iDelta, iRepeats, pstrTests[eTL],
                       m_tsp.m_iYStrides, m_tsp.m_iYdwStride*sizeof(DWORD), 
                       m_tsp.m_iXStrides, m_tsp.m_iXdwStride*sizeof(DWORD) 
                       );
        }
        assert( fabs( (iRepeats * m_tsp.uiAccessesPerPattern()) - (((double)iRepeats) * m_tsp.uiAccessesPerPattern()) ) < 1.1 );
        dwTotalAccesses = iRepeats * m_tsp.uiAccessesPerPattern();
        dTotalDur = 0;
        dTotalDurSqr = 0;
        iValidIdx = 0;
    } while (1);
    
    int iMedianIdx = iMedianDWA( m_pdwLatencies, m_iValidMeasurements, NULL );

    // duration is in microseconds for ciRepeat*ciCountStride accesses.
    // normalize from microseconds per total accesses to nanoseconds per 100 accesses.
    DWORD dwMedian100 = (DWORD)(((100.0f*1000.0f) * m_pdwLatencies[iMedianIdx]) / dwTotalAccesses);
    assert( fabs( dwMedian100 - (((100.0f*1000.0f) * m_pdwLatencies[iMedianIdx]) / dwTotalAccesses) ) <= 1.1 );
    double dMean = dTotalDur/(m_iValidMeasurements);
    double dVar  = (dTotalDurSqr/(m_iValidMeasurements)) - (dMean*dMean);
    double dStdDev = dVar > 0 ? (double)sqrt( dVar ) : 0.0;
    DWORD dwMean100   = (DWORD)( (100000.0*dMean) / dwTotalAccesses );
    DWORD dwStdDev100 = (DWORD)( (100000.0*dStdDev) / dwTotalAccesses );

    m_dwLatency100 = dwMedian100;
    m_dwMeanLatency100 = dwMean100;
    m_dwStdDevLatency100 = dwStdDev100;
    m_dwTotalAccesses = dwTotalAccesses;
}
#pragma warning( pop )


// ***************************************************************************************************************
// dwDuration measures the duration of a single test loop executed ciRepeats times over the same ctStrideInfo access Pattern.
// 
// sets things up including raising this thread's priority very high
// preloads the caches by executing the pattern once
// 
// ***************************************************************************************************************
DWORD dwDuration( const tStridePattern &ctStrideInfo, 
                  _Inout_ptrdiff_count_(pdwTop1) DWORD * const pdwBase1, 
                  DWORD * const pdwTop1, 
                  _In_opt_ptrdiff_count_(pdwTop2) const DWORD * pdwBase2, 
                  const DWORD * pdwTop2, 
                  const int ciRepeats, 
                  int *piGrandTotal )
{
    UINT uiRand1 = 0;
    UINT uiRand2 = 0;
    UINT uiRand3 = 0;
    const int eTL = ctStrideInfo.m_eTL;
    if ( !( ctStrideInfo.bVerifyAccessRange( pdwTop1-pdwBase1 ) && 
            ( NULL==pdwBase2 || ctStrideInfo.bVerifyAccessRange( pdwTop2-pdwBase2 ) )
           )
        )
    {
        LogPrintf( "Error: Stride Pattern out of range {%d,%d, %d,%d} for buffer size %d\r\n",
                    ctStrideInfo.m_iYStrides, ctStrideInfo.m_iYdwStride*sizeof(DWORD), 
                    ctStrideInfo.m_iXStrides, ctStrideInfo.m_iXdwStride*sizeof(DWORD),
                    (pdwTop1-pdwBase1)*sizeof(DWORD)
                   );
        assert(0);
        return 0;
    }
    const unsigned int uiSpan = ctStrideInfo.uiSpanSizeBytes();
    tStridePattern tPreloadInfo = ctStrideInfo;
    if ( 0.0f <= tPreloadInfo.m_fPreloadPercent && tPreloadInfo.m_fPreloadPercent <= 100.0f )
    {
        float fRatio = 0.0f == tPreloadInfo.m_fPreloadPercent ? 1.0f : tPreloadInfo.m_fPreloadPercent/100.0f;
        tPreloadInfo.m_iYStrides = (int)(fRatio*tPreloadInfo.m_iYStrides);
    }
    const HTHREAD chThread = GetCurrentThread();
    const int ciPriorPriority = CeGetThreadPriority( chThread );
    assert( 0 <= gtGlobal.giCEPriorityLevel && gtGlobal.giCEPriorityLevel <= 255 );
    if ( FALSE == CeSetThreadPriority( chThread, gtGlobal.giCEPriorityLevel ) )     // try to get above the drivers
    {   
        // otherwise, raise as much as possible
        SetThreadPriority( chThread, THREAD_PRIORITY_TIME_CRITICAL );       // 8 original levels map to a lowly 248 - 255
    }
    if ( gtGlobal.giRescheduleSleep > 0 )
    {
        Sleep(gtGlobal.giRescheduleSleep);  // allow set-priority to get a chance to apply.
    }

    int iGrandTotal = 0;
    DWORD dwFill = 0;
    DWORD uiTstTicks = 0;
    __int64 i64TstTicks = 0;
    LARGE_INTEGER liBeginFreq, liEndFreq;
    liBeginFreq.QuadPart = 0;
    liEndFreq.QuadPart = 0;
    
    // run test once to establish an initial condition within the caches and TLB for the test.
    // but when 0 == giRescheduleSleep, warm up the CPU to have a more accurate and consistent measurements.
    const DWORD dwMilliSeconds = 0==gtGlobal.giRescheduleSleep ? 30 : 0;
    const int iPrePasses = 0==gtGlobal.giRescheduleSleep ? min(25,ciRepeats): 1;
    const DWORD dwStartTime = GetTickCount();
    do
    {
        switch( eTL )
        {
        case eReadLoop1D:
        case eReadLoop2D:
                ReadLoopSetup(tPreloadInfo);
                iGrandTotal += pfiReadLoop( pdwBase1, iPrePasses, tPreloadInfo );
                ReadLoopSetup(ctStrideInfo);
                break;
        case eWriteLoop1D:
                rand_s(&uiRand1);
                rand_s(&uiRand2);
                rand_s(&uiRand3);
                dwFill = (uiRand1<<17) ^ uiRand2<<2 ^ uiRand3;
                iGrandTotal += iWriteLoop1D( pdwBase1, uiSpan, iPrePasses, ctStrideInfo, dwFill );
                if ( tPreloadInfo.m_fPreloadPercent > 0.0f )
                {
                    // read to allocate in case no-allocate-on-write
                    iGrandTotal += iReadLoop1D(  pdwBase1, 1, tPreloadInfo );
                }
                break;
        case eWriteLoop2D:
                rand_s(&uiRand1);
                rand_s(&uiRand2);
                rand_s(&uiRand3);
                dwFill = (uiRand1<<17) ^ uiRand2<<2 ^ uiRand3;
                iGrandTotal += iWriteLoop2D( pdwBase1, uiSpan, iPrePasses, ctStrideInfo, dwFill );
                if ( tPreloadInfo.m_fPreloadPercent > 0.0f )
                {
                    // read to allocate in case no-allocate-on-write
                    iGrandTotal += iReadLoop2D(  pdwBase1, 1, tPreloadInfo );
                }
                break;
        case eMemCpyLoop:
                if ( NULL == pdwBase2 )
                {
                    pdwBase2 = pdwBase1+ctStrideInfo.m_iYStrides+64;
                    pdwTop2  = pdwTop1;
                    if ( (pdwBase2+ctStrideInfo.m_iYStrides+1) > pdwTop2 )
                        pdwBase2 = pdwTop2 - (ctStrideInfo.m_iYStrides+64);
                    if ( pdwBase2 < pdwBase1 )
                    {
                        LogPrintf( "Error: Conflicting MemCpy pointers 1: %p  should be less than 2: %p\r\n",
                                    pdwBase1, pdwBase2 );
                        assert(pdwBase1 >= pdwBase2 );
                        pdwBase2 = pdwBase1;
                    }
                }
                iGrandTotal += iMemCpyLoop( pdwBase1, uiSpan, pdwBase2, iPrePasses, ctStrideInfo );
                break;
        case eReadLoop3D:
                // don't preload this pattern
                // iGrandTotal += iReadLoop3D(  pdwBase1, 1, tPreloadInfo );
                __fallthrough;  // fall through to warm-up if necessary
        case eCPULoop:
                // only run this when we want to warm-up rather than preload
                if ( 0==gtGlobal.giRescheduleSleep )
                {
                    iGrandTotal += iCPULoop( iPrePasses );
                }
                break;
        default:
                DebugBreak();
        }
    } while ( (GetTickCount() - dwStartTime) < dwMilliSeconds );

    // ready to actually run the test
    if ( gtGlobal.giUseQPC )
    {
        liBeginFreq.QuadPart = 0;
        QueryPerformanceFrequency( &liBeginFreq );
        QueryPerformanceCounter( (LARGE_INTEGER*)&i64TstTicks );
    }
    else
    {
        uiTstTicks = GetTickCount();
    }

    switch( eTL )
    {
    case eReadLoop1D: 
    case eReadLoop2D:
            iGrandTotal += pfiReadLoop(  pdwBase1, ciRepeats, ctStrideInfo );
            break;
    case eWriteLoop1D:
            iGrandTotal += iWriteLoop1D( pdwBase1, uiSpan, ciRepeats, ctStrideInfo, dwFill  );
            break;
    case eWriteLoop2D:
            iGrandTotal += iWriteLoop2D( pdwBase1, uiSpan, ciRepeats, ctStrideInfo, dwFill  );
            break;
    case eMemCpyLoop:
            iGrandTotal += iMemCpyLoop( pdwBase1, uiSpan, pdwBase2, ciRepeats, ctStrideInfo );
            break;
    case eReadLoop3D:
            iGrandTotal += iReadLoop3D(  pdwBase1, ciRepeats, ctStrideInfo );
            break;
    case eCPULoop:
            iGrandTotal += iCPULoop( ciRepeats );
            break;
    default:
            DebugBreak();
    }

    // see how long it took
    if ( gtGlobal.giUseQPC )
    {   LARGE_INTEGER llEndTicks;       
        liEndFreq.QuadPart = 1;
        QueryPerformanceCounter( &llEndTicks );
        QueryPerformanceFrequency( &liEndFreq );
        i64TstTicks = llEndTicks.QuadPart - i64TstTicks;
    }
    else
    {
        i64TstTicks = GetTickCount() - uiTstTicks;
    }

    // return to normal priority so other processes get to run when this is not in a timed test loop.
    if ( THREAD_PRIORITY_ERROR_RETURN == ciPriorPriority  || 
         FALSE == CeSetThreadPriority( chThread, ciPriorPriority ) 
        )
    {   
        // return to normal
        SetThreadPriority( chThread, THREAD_PRIORITY_NORMAL );   // otherwise, just return to normal.
    }
    if ( gtGlobal.giRescheduleSleep > 0 )
    {
        Sleep(gtGlobal.giRescheduleSleep);
    }

    if ( NULL != piGrandTotal )
    {
        *piGrandTotal += iGrandTotal;
    }

    if ( gtGlobal.giUseQPC )
    {
        #ifdef SIMULATE
            rand_s(&uiRand1);
            if ( 0 && (uiRand1 < (UINT_MAX/8)) )
            {   // simulate frequency changes in DE
                liEndFreq.QuadPart = ( gtGlobal.gliFreq.QuadPart == gtGlobal.gliOrigFreq.QuadPart ) ? gtGlobal.gliOrigFreq.QuadPart/2 : gtGlobal.gliOrigFreq.QuadPart;
            }
        #endif
        if ( liBeginFreq.QuadPart != liEndFreq.QuadPart || liEndFreq.QuadPart != gtGlobal.gliFreq.QuadPart )
        {
            gtGlobal.gdwCountFreqChanges++;
            if ( liEndFreq.QuadPart > 1000 )
            {
                gtGlobal.gliFreq.QuadPart = liEndFreq.QuadPart;
                gtGlobal.gdTickScale2us = 1000000.0/gtGlobal.gliFreq.QuadPart;
            }
            // return 0 duration to mark this as an invalid duration since CPU speed changed during it.
            // Note - if liBeginFreq == liEndFreq, then the frequency may have been stable during this measurement
            //        but it has changed since the last time set gliFreq (e.g. the last call to duration.
            //        so don't trust it.
            return 0;
        }
    }

    double dUS = gtGlobal.gdTickScale2us * i64TstTicks;
    gtGlobal.gdTotalTestLoopUS += dUS;
    return (DWORD)( dUS );
}

// ***************************************************************************************************************
// Comparison for qsort
// ***************************************************************************************************************

static
int Compare4Median( const void *arg1, const void *arg2 )
{
    if ( NULL==arg1 || NULL==arg2 )
        return 0;
    const DWORD dwValue1 = *((const DWORD *)arg1);
    const DWORD dwValue2 = *((const DWORD *)arg2);
    if ( dwValue1 < dwValue2 )
        return -1;
    if ( dwValue1 == dwValue2 )
        return 0;
    return +1;
}

// ***************************************************************************************************************
// find the median by sorting a copy of the results
// ***************************************************************************************************************

int iMedianDWA( const DWORD* pDWA, int iN, int* pMedianScore )
{
    if ( iN <= 0 || iN>=(INT_MAX/sizeof(DWORD)) )
    {
        if ( NULL != pMedianScore )
            *pMedianScore = 0;
        return 0;
    }
    DWORD *pdwCopy = new DWORD[iN];
    if ( NULL == pdwCopy )
    {
        if ( NULL != pMedianScore )
            *pMedianScore = pDWA[0];
        return 0;
    }
    memcpy( pdwCopy, pDWA, iN*sizeof(DWORD) );
    qsort( pdwCopy, iN, sizeof(DWORD), Compare4Median );
    int iMedianIdx = iN/2;
    // now find one of the Median values in the unsorted array
    for( int iMatchingIdx = 0; iMatchingIdx<iN; iMatchingIdx++ )
    {
        if ( pdwCopy[iMedianIdx] == pDWA[iMatchingIdx] )
            break;
    }
    if ( NULL != pMedianScore && 0 <= iMatchingIdx && iMatchingIdx < iN )
    {
        if ((iN&1) && iN>1 && (iMedianIdx+1)<iN )
        {   // median is average of two adjacent values when iN is odd
            *pMedianScore = ( pdwCopy[iMedianIdx] + pdwCopy[iMedianIdx+1] )/2;
        }
        else
        {
            *pMedianScore = pDWA[iMatchingIdx];
        }
    }
    delete [] pdwCopy;
    return iMatchingIdx;
}


// ***************************************************************************************************************
// Log to the base 2 of a power of 2 integer returned as an integer
// ***************************************************************************************************************

int iLog2( int iX )
{
    int iExp = 0;
    assert( iX > 0 );
    if ( iX <= 1 )
        return 0;
    while( iX >= (1<<4) )
    {
        iExp += 4;
        iX >>= 4;
    }
    while( iX > 1 )
    {
        iExp++;
        iX >>= 1;
    }
    __analysis_assert( 0 < iExp && iExp < 32 );
    return iExp;
}

