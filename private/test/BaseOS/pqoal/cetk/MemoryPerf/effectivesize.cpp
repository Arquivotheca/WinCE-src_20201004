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
// These functions implement the behavior of class tPartition
// Intent is to provide methods which measure latencies at many different variations of a basic access pattern
// And in so doing, expose the underlying configuration of a particular part of the memory hierarchy
//
// ***************************************************************************************************************

#include <math.h>
#include "memoryPerf.h"
#include "Latency.h"
#include "EffectiveSize.h"
#include "cache.h"
#include "Globals.h"

// ***************************************************************************************************************
// Forward declarations
// ***************************************************************************************************************
float fSlopeInRange( const DWORD *pdw, int iMin, int iMax );
bool bMeanStdDev( float &fMean, float &fStdDev, const DWORD *pdw, int iMin, int iMax );
DWORD dwMedianInRange( const DWORD *pdw, int iMin, int iMax );
DWORD dwMaxInRange(  const DWORD *pdw, int iMin, int iMax );
DWORD dwMinInRange(  const DWORD *pdw, int iMin, int iMax );
static int CompareValues( const void *arg1, const void *arg2 );
static int CompareInputValues( const void *arg1, const void *arg2 );
bool bIsSignificant( 
            tRanking *ptRankings, 
            const int ciMinIdx, 
            const int ciLowIdx, 
            const int ciHighIdx, 
            const int ciMaxIdx,
            float &fLowConfidence,
            float &fHighConfidence );

// ***************************************************************************************************************
// main constructor
// ***************************************************************************************************************
tPartition::tPartition( 
            const int iLineSize,
            const int iMinSize, 
            const int iMaxSize, 
            const int iStepSize,
            const int eSeq, 
            const int eStep, 
            const tStridePattern &tStrideInfo,
            const int iCsvScale
            )
{
    assert( 0 <= iMinSize && iMinSize <= iMaxSize );
    m_iMinSize = iMinSize;
    m_iLowSize = 0;
    m_iHighSize = 0;
    m_iMaxSize = iMaxSize;
    m_iMeasuredSize = 0;
    m_eSequencer = eSeq;
    m_eStepping = eStep;
    m_tStrideInfo = tStrideInfo;
    m_iCsvScale = iCsvScale;
    m_iStepSize = iStepSize;
    m_iBaseStrides = 0;
    m_iConstSpan = 0;
    m_iReduce256ths = 0;
    m_iLineSize = iLineSize;
    m_dwExpected = 0;
    m_pdwBase = m_pdwTop = NULL;
    m_dwLowLatency = 0;
    m_dwHighLatency = 0;
    m_iTLBSize = 0;
    m_dwPageWalkLatency = 0;
    m_bCorrected = false;
    m_bNotAllocated = true;
    m_bPartitioned = false;
    m_bMeasured = false;
    m_pdwLatencies = NULL;
    m_ptRankings = NULL;
    m_ptLatencies = NULL;
    m_dwSign = PARTITIONSIGN;
    m_fLowConfidence = 0;
    m_fHighConfidence = 0;
    m_tMaxMedianLatency = tLatency( iLineSize, tStrideInfo );
}


// ***************************************************************************************************************
// destructor
// ***************************************************************************************************************
tPartition::~tPartition()
{
    if ( PARTITIONSIGN == m_dwSign && !m_bNotAllocated )
    {
        if ( NULL != m_pdwLatencies )
        {
            delete [] m_pdwLatencies;
        }
        if ( NULL != m_ptRankings )
        {
            delete [] m_ptRankings;
        }
        if ( NULL != m_ptLatencies )
        {
            delete [] m_ptLatencies;
        }
    }
}

// ***************************************************************************************************************
// Main Idea:
//
// partition a range of latencies from iMinSize to iMaxSize into three zones
//   iMinSize   .. iLowSize    is the region with a good match to trEffectiveSize.dwLowLatency
//   iLowSize+1 .. iHighSize-1 is the transition region
//   iHighSize  .. iMaxSize-1  is the region with a good match to trEffectiveSize.dwHighLatency
//  
// In a Stride Pattern, we have four parameters which we may want to vary.
// We also want to vary Sequential or Power2 variation
// not all combinations make sense but these have a chance
// 1D x (S|P) x (Strides|stride) = 4 combinations
// 2D x (S|P) x (YStrides|YStride|XStrides|XStride) = 8 combinations.
// ***************************************************************************************************************


// ***************************************************************************************************************
// EffectiveSize is the work-horse method for the class
// ***************************************************************************************************************

void tPartition::EffectiveSize( DWORD *pdwBase, DWORD *pdwTop, int *pGrandTotal )
{
    m_bPartitioned = false;
    m_bMeasured = false;
    m_fLowConfidence  = 0;
    m_fHighConfidence  = 0;
    if ( m_iMinSize > m_iMaxSize || m_iMaxSize <= 0 )
        return;
    if ( m_iStepSize < 1 )
        m_iStepSize = 1;
    #ifdef SIMULATE
        m_dwLowLatency = m_dwHighLatency = 0;
    #endif
    m_pdwBase = pdwBase;
    m_pdwTop  = pdwTop;

    // ***************************************************************************************************************
    // first set up things to make a number of measurements at different variations on the pattern
    //
    // Sizes are mapped from their natural units to indexes through the StepSize and either a linear or Power/Log function
    // ***************************************************************************************************************

    int iMinIdx, iMaxIdx;

    // set up size to index mapping parameters
    switch( m_eSequencer )
    {
    case eSequential:
        iMinIdx = m_iMinSize/m_iStepSize;
        m_iBaseStrides = m_iMinSize%m_iStepSize;
        iMaxIdx = m_iMaxSize/m_iStepSize;
        break;
    case ePower2:
        m_iBaseStrides = m_iMinSize%m_iStepSize;
        iMinIdx = iLog2(m_iMinSize/m_iStepSize);
        iMaxIdx = iLog2(m_iMaxSize/m_iStepSize);
        break;
    default:
        assert(0);
        return;
    }

    // set up how we will vary the pattern by stepping through the number or strides or the size of the stride
    // used for X or Y
    switch( m_eStepping )
    {
    case eStepYStrides:
        break;
    case eStepYStride:
        break;
    case eStepConstYSpan:
        m_iConstSpan = m_tStrideInfo.m_iYStrides * iStrideFromIndex( iMinIdx, iMaxIdx+iMinIdx );
        break;
    case eStepXStrides:
        break;
    case eStepXStride:
        assert( eReadLoop2D==m_tStrideInfo.m_eTL || eWriteLoop2D==m_tStrideInfo.m_eTL || eReadLoop3D==m_tStrideInfo.m_eTL );
        break;
    case eStepConstXSpan:
        assert( eReadLoop2D==m_tStrideInfo.m_eTL || eWriteLoop2D==m_tStrideInfo.m_eTL || eReadLoop3D==m_tStrideInfo.m_eTL );
        m_iConstSpan = m_tStrideInfo.m_iXStrides * iStrideFromIndex( iMinIdx, iMaxIdx+iMinIdx );
        break;
    default:
        assert(0);
        return;
    }
      

    // set up the memory we will need
    int iLowIdx = 0, iHighIdx = 0;

    DWORD *pdwLatency100 = new DWORD[iMaxIdx+1];
    if ( NULL == pdwLatency100 )
        return;
    if ( NULL != m_pdwLatencies )
        delete []m_pdwLatencies;
    tLatency * ptLats = new tLatency[iMaxIdx+1];
    if ( NULL == ptLats )
    {
        delete [] pdwLatency100;
        return;
    }
    m_ptLatencies = ptLats;
    m_bNotAllocated = false;
    m_pdwLatencies = pdwLatency100;
    tRanking *ptRankings = new tRanking[ ciTestLotSize*(iMaxIdx+1) ];
    if ( NULL != m_ptRankings )
        delete [] m_ptRankings;
    m_ptRankings = ptRankings;
    if ( NULL != m_ptRankings )
    {
        memset( m_ptRankings, 0, ciTestLotSize*(iMaxIdx+1)*sizeof(tRanking) );
    }
    DWORD dwMaxMedianLatency = 0;
    DWORD dwMinMedianLatency = 0xFFFFFFFF;

    // ***************************************************************************************************************
    // Main loop to collect data
    // ***************************************************************************************************************
    int iIdx;
    for( iIdx = iMinIdx; iIdx <= iMaxIdx; iIdx++ )
    {
        int iStrides = iStrideFromIndex( iIdx, iMaxIdx+iMinIdx );
        if ( iStrides <= 0 )
            goto ESCleanup;
        switch( m_eStepping )
        {
        case eStepYStrides:
            m_tStrideInfo.m_iYStrides   = (iStrides*(256-m_iReduce256ths))/256;
            break;
        case eStepYStride:
            m_tStrideInfo.m_iYdwStride  = iStrides;
            break;
        case eStepConstYSpan:
            // keep span the same
            m_tStrideInfo.m_iYdwStride  = iStrides;
            m_tStrideInfo.m_iYStrides   = m_iConstSpan / iStrides;
            if ( m_tStrideInfo.m_iYStrides < 2 )
                m_tStrideInfo.m_iYStrides = 2;
            break;
        case eStepXStrides:
            m_tStrideInfo.m_iXStrides   = (iStrides*(256-m_iReduce256ths))/256;
            break;
        case eStepXStride:
            m_tStrideInfo.m_iXdwStride  = iStrides;
            break;
        case eStepConstXSpan:
            m_tStrideInfo.m_iXdwStride  = iStrides;
            m_tStrideInfo.m_iXStrides   = m_iConstSpan / iStrides;
            if ( m_tStrideInfo.m_iXStrides  < 2 )
                m_tStrideInfo.m_iXStrides = 2;
            break;
        default:
            assert(0);
            break;
        }
        if ( !m_tStrideInfo.bVerifyAccessRange( pdwTop-pdwBase ) )
        {
            LogPrintf( "Warning: Stride Pattern out of range {%d,%d, %d,%d} for buffer size %d\r\n",
                        m_tStrideInfo.m_iYStrides, m_tStrideInfo.m_iYdwStride*sizeof(DWORD), 
                        m_tStrideInfo.m_iXStrides, m_tStrideInfo.m_iXdwStride*sizeof(DWORD),
                        (pdwTop-pdwBase)*sizeof(DWORD)
                       );
            if ( iIdx > (iMinIdx+1) )
            {
                // not a large enough buffer to run this test.  Reduce the range.
                iMaxIdx = iIdx-1;
                break;
            }
            else
            {   // don't reduce to a single size
                assert( 0 );
                goto ESCleanup;
            }
        }
        // set up to do the latency measurement at one point and then measure it
        tLatency tESLatency = tLatency( m_iLineSize, m_tStrideInfo, m_iTLBSize,  m_dwPageWalkLatency );
        if ( iIdx >= (iMinIdx+3) )
        {   // this can speed up the test, particular for the TLB size which tends to test a very large number of points in the high region
            tESLatency.SetExpected( ((pdwLatency100[iIdx-3]+pdwLatency100[iIdx-2]+pdwLatency100[iIdx-1])/300)/2 );
        }
        else if ( m_dwExpected > 0 )
        {
            tESLatency.SetExpected( m_dwExpected );
        }
        tESLatency.Measure( pdwBase, pdwTop, NULL );
        pdwLatency100[iIdx] = tESLatency.dwLatency100();
        // keep some statistics of these measurements
        m_ptLatencies[iIdx] = tESLatency;
        if ( dwMaxMedianLatency < pdwLatency100[iIdx] )
        {
            dwMaxMedianLatency = pdwLatency100[iIdx];
            m_tMaxMedianLatency = tESLatency;
        }
        if ( dwMinMedianLatency > pdwLatency100[iIdx] )
        {
            dwMinMedianLatency = pdwLatency100[iIdx];
        }
        if ( NULL != ptRankings )
        {   // ciTestLotSize*(iMaxIdx+1)
            tESLatency.CopyLatencies( ptRankings+iIdx*ciTestLotSize, 
                                      sizeof(tRanking)*ciTestLotSize*(iMaxIdx+1-iIdx), 
                                      sizeof(tRanking)/sizeof(DWORD) );
            int iInput = m_tStrideInfo.uiSpanSizeBytes();
            if ( m_iCsvScale>0 )
            {
                switch( m_eStepping )
                {
                case eStepYStrides:            
                case eStepXStrides:
                    iInput =  m_iCsvScale*(iStrides*(256-m_iReduce256ths))/256;
                    break;
                case eStepYStride:
                case eStepXStride:
                case eStepConstYSpan:
                case eStepConstXSpan:
                    iInput = iStrides*m_iCsvScale;
                    break;
                default:
                    assert(0);
                    iInput = iStrides*m_iCsvScale;
                    break;
                }
            }
            DWORD dwMinLatency = 0xFFFFFFFF;
            for( int j = 0; j < ciTestLotSize; j++ )
            {
                if ( ptRankings[ iIdx*ciTestLotSize + j ].m_dwValue )
                {
                    ptRankings[ iIdx*ciTestLotSize + j ].m_iInput = iInput;
                    ptRankings[ iIdx*ciTestLotSize + j ].m_iIdx = iIdx;
                    if ( ptRankings[ iIdx*ciTestLotSize + j ].m_dwValue < dwMinLatency )
                    {
                        dwMinLatency = ptRankings[ iIdx*ciTestLotSize + j ].m_dwValue;
                    }
                }
            }
        }
    }
    m_bMeasured = true;

    // ***************************************************************************************************************
    // now analyze the data to partition it into three groups low, transition and high
    // ***************************************************************************************************************

    // initialize Low and High values from measured values unless they were established at initialization
    if ( m_dwLowLatency <= 0 )
        m_dwLowLatency = dwMinMedianLatency;
    if ( m_dwHighLatency <= m_dwLowLatency )
        m_dwHighLatency = dwMaxMedianLatency;
    
    // ***************************************************************************************************************
    // define the effective Low size as the largest number of strides which consistently has latency close to Low Latency Target
    // more specifically, it is the largest value for which all pdwLatency100[1..iLowSize] < dwLowThreshold
    // ***************************************************************************************************************

    DWORD dwLowThreshold = m_dwLowLatency + ((m_dwHighLatency - m_dwLowLatency)/3);  
    // Find the first index from the bottom which is above the threshold 
    for( iLowIdx = iMinIdx; iLowIdx < iMaxIdx; iLowIdx++ )
    {
        if ( pdwLatency100[iLowIdx+1] > dwLowThreshold )
            break;
    }
    // Now walk size back down if the slope is steep.
    while( iLowIdx > (iMinIdx+2) && pdwLatency100[iLowIdx-1] < pdwLatency100[iLowIdx] )
    {
        float fMean, fStdDev;
        if ( !bMeanStdDev( fMean, fStdDev, pdwLatency100, iMinIdx, iLowIdx-2 ) )
            break;      // sadly, needing two points for the StdDev limits how close we can get to the lower end :o(...
        if ( (fMean+fStdDev/2) < pdwLatency100[iLowIdx-1] )
        {
            // Define Steep as twice slope of trend line in remaining points below our test index
            float fSteepSlope = 2.0f*fSlopeInRange( pdwLatency100, iMinIdx, iLowIdx-2 );
            float fMinSteepSlope = 2.0f*fStdDev/(iLowIdx-2+1-iMinIdx);
            if ( fMinSteepSlope < (.025f*fMean) )
                fMinSteepSlope =  (.025f*fMean);
            if ( fSteepSlope < fMinSteepSlope )
                fSteepSlope = fMinSteepSlope;
            if ( (pdwLatency100[iLowIdx]-pdwLatency100[iLowIdx-1]) > fSteepSlope )
            {
                iLowIdx--;
            }
            else break;
        }
        else
            break;
    }
    if ( !(iMinIdx <= iLowIdx && iLowIdx <= iMaxIdx) )
    {
        assert( iMinIdx <= iLowIdx && iLowIdx <= iMaxIdx );
        goto ESCleanup;
    }

    // ***************************************************************************************************************
    // define the effective high size as the smallest number of strides which consistently has latency close to High Latency Target
    // ***************************************************************************************************************

    DWORD dwHighThreshold = m_dwHighLatency - ((m_dwHighLatency - m_dwLowLatency)/3);
    for( iHighIdx = iMaxIdx; iHighIdx > iLowIdx; iHighIdx-- )
    {   // first ignore points near the top lower than the threshold
        // this is primarily needed when a series has a hat shape because the series tested too many points and is into another zone of behavior
        if ( pdwLatency100[iHighIdx] > dwHighThreshold )
            break;
    }
    for( ; iHighIdx > iLowIdx; iHighIdx-- )
    {   // ignore further points above the threshold
        if ( pdwLatency100[iHighIdx-1] < dwHighThreshold )
            break;
    }
    // Walk size back up a steep slope...
    while( iHighIdx < (iMaxIdx-1) && pdwLatency100[iHighIdx] < pdwLatency100[iHighIdx+1] )
    {
        float fMean, fStdDev;
        if ( !bMeanStdDev( fMean, fStdDev, pdwLatency100, iHighIdx+2, iMaxIdx  ) )
            break;      // this limits how close to the top end I can walk to :o(...
        if ( pdwLatency100[iHighIdx+1] < (fMean-fStdDev/2) )
        {
            // Define Steep as twice slope of trend line in remaining points above our test index
            float fSteepSlope = 2.0f*fSlopeInRange( pdwLatency100, iHighIdx+2, iMaxIdx );
            float fMinSteepSlope = 2.0f*(fStdDev/(iMaxIdx+1-(iHighIdx+2)));
            if ( fMinSteepSlope < (.025f*fMean) )
                fMinSteepSlope =  (.025f*fMean);
            if ( fSteepSlope < fMinSteepSlope )
                fSteepSlope = fMinSteepSlope;
            if ( (pdwLatency100[iHighIdx+1]-pdwLatency100[iHighIdx]) >  fSteepSlope )
            {
                iHighIdx++;
            }
            else 
                break;
        }
        else
            break;
    }
    if ( !( 0 <= iHighIdx && iLowIdx <= iHighIdx && iHighIdx <= iMaxIdx ) )
    {
        assert( 0 <= iHighIdx && iLowIdx <= iHighIdx && iHighIdx <= iMaxIdx );
        goto ESCleanup;
    }

    // ***************************************************************************************************************
    // update latencies with medians from low range and high range  (these are medians of medians)
    // ***************************************************************************************************************

    m_dwLowLatency  = dwMedianInRange( pdwLatency100, iMinIdx, iLowIdx );
    m_dwHighLatency = dwMedianInRange( pdwLatency100, iHighIdx, iMaxIdx );

    // ***************************************************************************************************************
    // establish a base confidence assuming the range of data between low and high regions should be significantly different
    // ***************************************************************************************************************

    m_fLowConfidence = m_fHighConfidence = 1.0;
    if ( m_dwLowLatency>0 && 
        (m_dwHighLatency-m_dwLowLatency)<2000 && 
        (3*m_dwLowLatency) > (2*m_dwHighLatency) 
       )
    {
        // limit our confidence when high-low < 20 ns and the high/low ratio is less than 1.5.
        m_fLowConfidence = m_fHighConfidence = (2.0f*m_dwHighLatency)/(3*m_dwLowLatency);      // base confidence in range 0..1
    }
    
    // now determine if populations of low range and high range come from different distributions
    if ( !bIsSignificant( ptRankings, iMinIdx, iLowIdx, iHighIdx, iMaxIdx, m_fLowConfidence, m_fHighConfidence ) )
    {
        // volatile int xxx = 1;       // break-point when interested in low confidence results
    }

    // update effective sizes in caller's units
    m_iLowSize  = iStrideFromIndex( iLowIdx,  iMinIdx+iMaxIdx );
    m_iHighSize = iStrideFromIndex( iHighIdx, iMinIdx+iMaxIdx );

    m_bPartitioned = true;

ESCleanup:

    return;
}

// ***************************************************************************************************************
// Several different LogReport functions will format the data in different ways
// ***************************************************************************************************************

// ***************************************************************************************************************
// LogReport with just a string name indicates we want to report the maximum of the medians of all the measurements
// used primarily in RAS measurements where we don't know the configuration of memory so we don't know where the maximum
// will occur.
// ***************************************************************************************************************

void tPartition::LogReport( const char * pstrName )
{
    LogReport( pstrName, ciMaxMedianReport );
}

#pragma warning ( push )
#pragma warning ( disable : 6326 )

// ***************************************************************************************************************
// LogReport with a Name and a Scale indicates a broad range of report where the Scale is encodes various details 
// of the style needed
// ***************************************************************************************************************

void tPartition::LogReport( const char * pstrName, int iScale )
{
    bool bSectionMapTest = (DWORD*)0x80000000 <= m_pdwBase && m_pdwTop < (DWORD*)0xC0000000;
    int iValue = m_iLowSize;
    if ( iScale < 0 )
    {
        iScale = -iScale;
        if ( 16 <= iScale && iScale <= 256 )
        {   // Assume L2 rule of highsize/2 
            if ( m_fHighConfidence > cfConfidenceThreshold )
            {
                iValue = m_iHighSize/2;
            }
        }
        else
        {   // Otherwise assume HighSize
                iValue = m_iHighSize;
        }
    }
    switch (iScale)
    {
    case 0:
        iValue = (m_iLowSize+sizeof(DWORD)-1)*sizeof(DWORD);
        // round up to nearest power of 2.
        if ( iValue & (iValue-1) )
        {
            iValue = 1<<(iLog2(iValue)+1);
        }
        iScale = sizeof(DWORD);
        break;
    case ciMaxMedianReport:
    case ciCSVReportOnly:
    case 1:
        break;
    default:
        if ( 3 < iScale && iScale <= ci1M )
        {
            iValue *= iScale;
        }
        break;
    }
    if ( ciCSVReportOnly == iScale && m_iLineSize > 0 && m_bMeasured )
    {
        // special case where we only want the CSV report below.
    }
    else if ( ciMaxMedianReport == iScale && m_iLineSize > 0 && m_bMeasured )
    {
        // Special case indicates we want to report the MaxMedian instead of the Size.
        m_tMaxMedianLatency.LogReport( pstrName );
    }
    else if ( 0==m_iLineSize || !bIsPartitioned() )
    {
        LogPrintf( "Warning: Effective %s Size not measured.\r\n", pstrName );
    }
    else if ( bSectionMapTest || 0==m_dwPageWalkLatency )
    {
        m_iMeasuredSize = iValue;
        LogPrintf( "%6d Effective %s Size.  Low Region [%d..%d] with %.2f Confidence and %.2f ns Latency.  High Region [%d..%d] with %.2f Confidence and %.2f ns Latency.\r\n", 
                    iValue, pstrName,
                    m_iMinSize*iScale,  m_iLowSize*iScale,  m_fLowConfidence,  0.01f*m_dwLowLatency, 
                    m_iHighSize*iScale, m_iMaxSize*iScale,  m_fHighConfidence, 0.01f*m_dwHighLatency
                  );
    }
    else
    {
        // Stride is, for example, 64 bytes so every 64 accesses, we will always have a page walk unless TLS can accommodate all the pages we touch
        // 2 registers spans 8KB, 4 registers spans 16KB, 8 registers spans 32KB, 16 registers spans 64KB, 32 registers spans 128KB
        // A slow page walk costs 210 cycles but happens every 64 accesses, so averages about 3 cycles or less than 600 ns per 100 accesses
        DWORD dwPWCorrection = m_dwPageWalkLatency/(ciPageSize/m_iLineSize); 
        // test reported sizes relative to TLB size - correct as appropriate
        if ( m_iLowSize*m_iLineSize > m_iTLBSize*ciPageSize &&
             m_dwLowLatency > dwPWCorrection )
        {
            m_dwLowLatency -= dwPWCorrection;
            m_bCorrected = true;
        }
        if ( m_iHighSize*m_iLineSize > m_iTLBSize*ciPageSize &&
             m_dwHighLatency > dwPWCorrection )
        {
            m_dwHighLatency -= dwPWCorrection;
            m_bCorrected = true;
        }
        if ( m_bCorrected && dwPWCorrection > 0 )
        {
            m_iMeasuredSize = iValue;
            LogPrintf( "%6d Effective %s Size.  Low Region [%d..%d] with %.2f Confidence and %.2f ns Latency.  High Region [%d..%d] with %.2f Confidence and %.2f ns Latency. (latencies corrected by - %.2f)\r\n", 
                        iValue, pstrName,
                        m_iMinSize*iScale,  m_iLowSize*iScale,  m_fLowConfidence,  0.01f*m_dwLowLatency, 
                        m_iHighSize*iScale, m_iMaxSize*iScale,  m_fHighConfidence, 0.01f*m_dwHighLatency,
                        0.01f*dwPWCorrection
                      );
        }
        else
        {
            m_iMeasuredSize = iValue;
            LogPrintf( "%6d Effective %s Size.  Low Region [%d..%d] with %.2f Confidence and %.2f ns Latency.  High Region [%d..%d] with %.2f Confidence and %.2f ns Latency.\r\n", 
                        iValue, pstrName,
                        m_iMinSize*iScale,  m_iLowSize*iScale,  m_fLowConfidence,  0.01f*m_dwLowLatency, 
                        m_iHighSize*iScale, m_iMaxSize*iScale,  m_fHighConfidence, 0.01f*m_dwHighLatency
                      );
        }
    }
    // Now output CSV output if requested
    if ( ( gtGlobal.bChartDetails || gtGlobal.bChartOverviewDetails )&&
         ( (m_iLineSize>0 && bIsMeasured()) || ciMaxMedianReport==iScale || ciCSVReportOnly==iScale )
        )
    {
        int iMaxIdx = 1;
        int iMinIdx = 0;
        switch( m_eSequencer )
        {
        case eSequential:
            iMinIdx = m_iMinSize/m_iStepSize;
            iMaxIdx = m_iMaxSize/m_iStepSize;
            break;
        case ePower2:
            iMinIdx = iLog2(m_iMinSize/m_iStepSize);
            iMaxIdx = iLog2(m_iMaxSize/m_iStepSize);
            break;
        default:
            assert(0);
            break;;
        }
        int iMaxSampleIdx = ciTestLotSize*(iMaxIdx+1);
        if ( ciMaxMedianReport != iScale )
        {
            CSVPrintf( "\r\n\"Recommendation: Look at the following columns D E F using a 'Scatter with only Markers' Chart.  Blue is Latency and Red is the grouping into Low-Transition-High partitions.\"\r\n");
            CSVPrintf( "Size, %s, Index, Input, Latency, Group, Size=%d, Low=%d..%d(%.2f)[%.2f], High=%d..%d(%.2f)[%.2f]\r\n",
                        pstrName, iValue,
                        m_iMinSize*iScale,  m_iLowSize*iScale,  m_fLowConfidence,  0.01f*m_dwLowLatency, 
                        m_iHighSize*iScale, m_iMaxSize*iScale,  m_fHighConfidence, 0.01f*m_dwHighLatency
                      );
            DWORD dwMaxValue = 0;
            int i;
            for( i = 0; i < iMaxSampleIdx; i++ )
            {
                if ( 0 < m_ptRankings[i].m_iGroup && dwMaxValue < m_ptRankings[i].m_dwValue )
                    dwMaxValue = m_ptRankings[i].m_dwValue;
            }
            int iGroupScale = dwMaxValue/400;
            iGroupScale = iRoundUp( iGroupScale, iGroupScale<50 ? 10 : 100 );
            if ( iGroupScale < 1 )
                iGroupScale = 1;
            for( i = 0; i < iMaxSampleIdx; i++ )
            {
                if ( 0 < m_ptRankings[i].m_iGroup && 0 < m_ptRankings[i].m_dwValue )
                {
                    CSVPrintf( "Size, %s, %d, %d, %.2f, %d\r\n", 
                                pstrName, 
                                i,
                                m_ptRankings[i].m_iInput,
                                0.01f * m_ptRankings[i].m_dwValue,
                                iGroupScale * m_ptRankings[i].m_iGroup
                             );
                }
            }
        }
        // now sort first on Input and then on Values
        qsort( m_ptRankings, iMaxSampleIdx, sizeof(tRanking), CompareInputValues );

        CSVPrintf( "\r\n\
\"Recommendation: Look at the following columns M..%c%c using a 'Line with Markers' Chart.\"\r\n",
                    (char)( ciTestLotSize>14 ? 'A' : '.' ),
                    (char)( ciTestLotSize>14 ? 'A'+ciTestLotSize-15 : 'M'+ciTestLotSize-1 ) 
                  );
        CSVPrintf( "\
\"Each set of latencies using the same access pattern are sorted from low to high.\"\r\n\
\"Consider Switching Row and Column to see latencies as a function of span and use column D or F for X-axis labels.\"\r\n"
                  );
        CSVPrintf( "Latencies, %s, Index, YStrides, YStride, XStrides, XStride, L1Hits%%, L2Hits%%, MBHits%%, PageWalks%%, RASChanges%%, Latencies...\r\n",
                    pstrName
                  );
        int i, j;
        for( i = iMinIdx; i <= iMaxIdx; i++ )
        {
            bool bAnyValid = false;
            for( j = 0; j < ciTestLotSize; j++ )
            {
                int idx = i*ciTestLotSize + j;
                if ( 0 < m_ptRankings[idx].m_iGroup && 0 < m_ptRankings[idx].m_dwValue )
                {
                    tStridePattern * ptSP = m_ptLatencies[ m_ptRankings[idx].m_iIdx ].ptStridePattern();
                    if ( !bAnyValid )
                    {
                        int iL1Hits=0, iL2Hits=0, iMBHits=0, iPageWalks=0, iRASChanges=0;
                        ptSP->AnalyzeAccessPattern( &iL1Hits, &iL2Hits, &iMBHits, &iPageWalks, &iRASChanges );
                        // show the access stats as percentages without a % so the scale roughly matches the latencies in ns.
                        double dFactor = 100.0/(iL1Hits + iL2Hits + iMBHits);
                        CSVPrintf( "Latencies, %s, %d, %d, %d, %d, %d, %.2f, %.2f, %.2f, %.2f, %.2f", 
                                    pstrName, 
                                    idx,
                                    ptSP->m_iYStrides,
                                    ptSP->m_iYdwStride*sizeof(DWORD),
                                    ptSP->m_iXStrides,
                                    ptSP->m_iXdwStride*sizeof(DWORD),
                                    dFactor*iL1Hits, 
                                    dFactor*iL2Hits, 
                                    dFactor*iMBHits, 
                                    dFactor*iPageWalks,
                                    dFactor*iRASChanges
                                  );
                        bAnyValid = true;
                    }
                    else if ( 0 < m_ptRankings[idx-1].m_iGroup && 0 < m_ptRankings[idx-1].m_dwValue )
                    {
                        assert( ptSP->m_iYStrides  == ptSP->m_iYStrides );
                        assert( ptSP->m_iYdwStride == ptSP->m_iYdwStride );
                        assert( ptSP->m_iXStrides  == ptSP->m_iXStrides );
                        assert( ptSP->m_iXdwStride == ptSP->m_iXdwStride );                        
                    }
                    CSVPrintf( ", %.2f", 0.01f * m_ptRankings[idx].m_dwValue );
                }
            }
            if ( bAnyValid )
            {
                CSVPrintf("\r\n");
            }
        }

        // now re-sort as it was just to be clean
        qsort( m_ptRankings, iMaxSampleIdx, sizeof(tRanking), CompareValues );
    }
}

#pragma warning ( pop )

// ***************************************************************************************************************
// LogReport with three additional args indicate we know an expected range for the result and want a warning if outside that range
// ***************************************************************************************************************

void tPartition::LogReport( const char * pstrName, const int ciScale, const int ciExpectedMinSize, const int ciExpectedMaxSize )
{
    LogReport( pstrName, ciScale );
    if ( m_iMeasuredSize > 0 && ( m_iMeasuredSize < ciExpectedMinSize || ciExpectedMaxSize < m_iMeasuredSize ) && gtGlobal.bWarnings )
    {
        LogPrintf( "Warning: %s = %d not in expected range %d..%d\r\n",
                   pstrName, 
                   m_iMeasuredSize, 
                   ciExpectedMinSize, 
                   ciExpectedMaxSize
                  );
    }
}

// ***************************************************************************************************************
// LogReport with this variation of args is when we know an expected value and a tolerance
// ***************************************************************************************************************

void tPartition::LogReport( const char * pstrName, const int ciScale, const int ciExpectedSize, const float cfTol )
{
    LogReport( pstrName, ciScale, (int)(ciExpectedSize*(1-cfTol)), (int)(ciExpectedSize*(1+cfTol)) );
}

// ***************************************************************************************************************
// Utility functions needed by the code above
// ***************************************************************************************************************

// ***************************************************************************************************************
// fSlopeInRange returns the slope of the linear model estimated from the data points 
// See, for example, any work on the univariate linear case of linear regression such as http://en.wikipedia.org/wiki/Linear_regression
// ***************************************************************************************************************
 
float fSlopeInRange( const DWORD *pdw, int iMin, int iMax )
{
    // slope = sum( (xi - xm )*(yi-ym) ) / sum( (xi-xm)^2 )
    // which reduces to ( sum( xi*yi ) - xm*sum(yi) ) / ( sum(xi^2) - n*xm^2 )
    // since xm = sum( i ) from iMin to iMax, xm = (iMax*(iMax+1) - (iMin-1)*iMin)/2
    // and sum(xi^2) = ( iMax*(iMax+1)*(2*iMax+1) - (iMin-1)*iMin*(2*iMin-1) ) / 6
    int iN = iMax+1-iMin;
    if ( iN < 3 )
        return 0;   // If only 1-2 points, we can't have a very good estimate of slope
    // sum(i^2) = n*(n+1)*(2*n+1)/6 [ avoid division and need for floating point until end ]
    int i6SumXSqr = (iMax*(iMax+1)*(2*iMax+1)) - ((iMin-1)*iMin*(2*iMin-1));
    // mean(x) = (iMax+iMin)/2 [ avoid division for now ]
    int i2Xm = iMax+iMin;
    DWORD dwXYTotal = 0, dwYTotal = 0;
    for( int i = iMin; i<=iMax; i++ )
    {
        assert( (dwYTotal + pdw[i]) >= dwYTotal && (dwXYTotal + i*pdw[i]) >= dwXYTotal );
        dwYTotal  += pdw[i];
        dwXYTotal += i*pdw[i];
    }
    float fXm = i2Xm/2.0f;
    float fN  = dwXYTotal - fXm*dwYTotal;
    float fD  = (i6SumXSqr/6.0f) - (fXm*fXm*iN);
    return (0.0==fD) ? ( fN > 0 ? FLT_MAX/2.0f : -FLT_MAX/2.0f ) : (fN / fD );
}

// ***************************************************************************************************************
// bMeanStdDev returns the mean and standard deviation of a range of DWORD latencies
// ***************************************************************************************************************

bool bMeanStdDev( float &fMean, float &fStdDev, const DWORD *pdw, int iMin, int iMax )
{
    // return the mean and standard deviation of the range
    int iN = iMax+1-iMin;
    if ( iN < 2 )
    {
        fMean = (float)pdw[iMin];
        fStdDev = 0;
        return false;
    }
    DWORD dwTotal = 0;
    __int64 i64TotalSqr = 0;
    for( int i = iMin; i <= iMax; i++ )
    {
        DWORD dw = pdw[i];
        dwTotal += dw;
        __int64 i64dw = dw;
        i64TotalSqr += i64dw*i64dw;
    }
    fMean = ((float)dwTotal)/iN;
    float fVar  = (((float)i64TotalSqr) - (fMean*fMean*iN))/(iN-1);     // use unbiased (iN-1) formula since iN is often small
    fStdDev = fVar > 0 ? (float)sqrt( fVar ) : (float)0;
    return true;
}


// ***************************************************************************************************************
// dwMedianInRange returns the median of a range of DWORD latencies
// ***************************************************************************************************************

DWORD dwMedianInRange( const DWORD *pdw, int iMin, int iMax )
{
    if ( iMin < 0 || iMax < iMin )
        return 0xFFFFFFFF;
    if ( iMin==iMax )
        return pdw[iMin];
    int iMedian = INT_MAX;
    iMedianDWA( pdw+iMin, iMax+1-iMin, &iMedian );
    return (DWORD)iMedian;
}


// ***************************************************************************************************************
// dwMaxInRange returns the maximum of a range of DWORD latencies
// ***************************************************************************************************************

DWORD dwMaxInRange( const DWORD *pdw, int iMin, int iMax )
{
    if ( iMin < 0 || iMax < iMin )
        return 0xFFFFFFFF;
    DWORD dwMx = pdw[iMin];
    for( int i=iMin+1; i<=iMax; i++ )
    {
        if ( pdw[i] > dwMx )
            dwMx = pdw[i];
    }
    return dwMx;
}


// ***************************************************************************************************************
// dwMinInRange returns the minimum of a range of DWORD latencies
// ***************************************************************************************************************

DWORD dwMinInRange( const DWORD *pdw, int iMin, int iMax )
{
    if ( iMin < 0 || iMax < iMin )
        return 0;
    DWORD dwMn = pdw[iMin];
    for( int i=iMin+1; i<=iMax; i++ )
    {
        if ( pdw[i] < dwMn )
            dwMn = pdw[i];
    }
    return dwMn;
}

// ***************************************************************************************************************
// dCorrelation returns the correlation between two vectors
// ***************************************************************************************************************

// Calculate the correlation of vectors X[iN1..iN2] and Y[iN1..iN2]
double dCorrelation( const double * pdX, const double * pdY, const int ciN1, const int ciN2, const int ciStride )
{
    int iN = ciN2+1-ciN1;
    if ( iN <= 1 )
        return -1.0;
    double dSumSqrX = 0;
    double dSumSqrY = 0;
    double dSumCoProduct = 0;
    double dMeanX = pdX[ciN1*ciStride];
    double dMeanY = pdY[ciN1*ciStride];
    for( int i=1; i<iN; i++ )
    {
        double dSweep = i / (i+1.0);
        double dDeltaX = pdX[(i+ciN1)*ciStride] - dMeanX;
        double dDeltaY = pdY[(i+ciN1)*ciStride] - dMeanY;
        dSumSqrX += dDeltaX * dDeltaX * dSweep;
        dSumSqrY += dDeltaY * dDeltaY * dSweep;
        dSumCoProduct += dDeltaX * dDeltaY * dSweep;
        dMeanX += dDeltaX / (i+1);
        dMeanY += dDeltaY / (i+1);
    }
    double dStdDevX = dSumSqrX > 0 ? sqrt( dSumSqrX/iN ) : 0.0;
    double dStdDevY = dSumSqrY > 0 ? sqrt( dSumSqrY/iN ) : 0.0;
    double dCovXY  = dSumCoProduct/iN;
    if ( dStdDevX <= 0 || dStdDevY<= 0 )
        return 0.0;
    return dCovXY / (dStdDevX * dStdDevY);
}


// ***************************************************************************************************************
// iStrideFromIndex maps from Index to Stride
// ***************************************************************************************************************

int tPartition::iStrideFromIndex( const int iIdx, const int iIdxTop ) const
{
    int iStrides;
    switch( m_eSequencer )
    {
    case eSequential:
        iStrides = m_iStepSize*iIdx + m_iBaseStrides;
        break;
    case ePower2:
        iStrides = m_iStepSize*(1<<iIdx) + m_iBaseStrides;
        break;
    default:
        assert( 0 );
        iStrides = 0;
    }
    return iStrides;
}


// ***************************************************************************************************************
// iIndexFromStride maps from Stride to Index
// ***************************************************************************************************************

int tPartition::iIndexFromStride( const int iStrides, const int iIdxTop ) const
{
    int iIdx;
    switch( m_eSequencer )
    {
    case eSequential:
        iIdx = (iStrides-m_iBaseStrides)/m_iStepSize;
        break;
    case ePower2:
        iIdx = iLog2((iStrides-m_iBaseStrides)/m_iStepSize);
        break;
    default:
        assert( 0 );
        iIdx = 0;
    }
    return iIdx;
}


// ***************************************************************************************************************
// comparison routines needed by qsort
// ***************************************************************************************************************

static
int CompareValues( const void *arg1, const void *arg2 )
{
    if ( NULL==arg1 || NULL==arg2 )
        return 0;
    DWORD dwValue1 = ((tRanking *)arg1)->m_dwValue;
    DWORD dwValue2 = ((tRanking *)arg2)->m_dwValue;
    if ( dwValue1 < dwValue2 )
        return -1;
    if ( dwValue1 == dwValue2 )
        return 0;
    return +1;
}

static
int CompareInputValues( const void *arg1, const void *arg2 )
{
    if ( NULL==arg1 || NULL==arg2 )
        return 0;
    int iValue1 = ((tRanking *)arg1)->m_iInput;
    int iValue2 = ((tRanking *)arg2)->m_iInput;
    if ( iValue1 < iValue2 )
        return -1;
    if ( iValue1 > iValue2 )
        return +1;
    DWORD dwValue1 = ((tRanking *)arg1)->m_dwValue;
    DWORD dwValue2 = ((tRanking *)arg2)->m_dwValue;
    if ( dwValue1 < dwValue2 )
        return -1;
    if ( dwValue1 == dwValue2 )
        return 0;
    return +1;
}


// ***************************************************************************************************************
// bIsSignificant evaluates the partition of the measurements into Low, Medium and High regions and estimates
// whether this partition if significant as well as the degree of confidence these results express
//
// Use Max-Whitney U-Test to evaluate significance in a non-parametric set of measurements.
// evaluate the confidence based on the p-statistic of the U-Test transformed to a range [0..1]
// Consider The Wilcoxon Signed-Rank Test as an alternative.
// ***************************************************************************************************************

bool bIsSignificant( 
            tRanking *ptRankings, 
            const int ciMinIdx, 
            const int ciLowIdx, 
            const int ciHighIdx, 
            const int ciMaxIdx,
            float &fLowConfidence,
            float &fHighConfidence
            )
{
    if ( NULL == ptRankings )
        return false;
    int i, j;
    if ( ciMinIdx > 0 )
    {   // cleanup unused portion below ciMinIdx 
        memset( ptRankings, 0, ciMinIdx*ciTestLotSize*sizeof(tRanking) );
    }
    // int iN1 = ciTestLotSize*(ciLowIdx+1-ciMinIdx);
    int iN2 = 0;
    // int iN3 = ciLowIdx<ciMaxIdx ? ciTestLotSize*(ciMaxIdx+1-ciHighIdx) : 0;
    // before sorting, identify by filling in the group fields
    for( i = ciMinIdx; i <= ciMaxIdx; i++ )
    {
        int iGroup = 0;
        if ( i <= ciLowIdx )
            iGroup = 1;             // ciMinIdx <= i <= ciLowIdx
        else if ( i < ciHighIdx )
        {
            iGroup = 2;             // transition zone
            iN2 += ciTestLotSize;
        }
        else if ( i <= ciMaxIdx )
            iGroup = 3;             // ciHighIdx <= i <= ciMaxIdx
        for( j = 0; j < ciTestLotSize; j++ )
        {
            if ( ptRankings[ i*ciTestLotSize + j ].m_dwValue > 0 )
                ptRankings[ i*ciTestLotSize + j ].m_iGroup = iGroup;
        }
    }

    // turn collection of scores into scores in rank order.
    qsort( ptRankings, (ciMaxIdx+1)*ciTestLotSize, sizeof(tRanking), CompareValues );
    // now compare group 1 to group 3;
    int i2U1 = 0, i2U3 = 0;     // accumulate twice U value 
    const int ciN = (ciMaxIdx+1)*ciTestLotSize;
    for( i = 0; i < ciN; i++ )
    {
        if ( 1 == ptRankings[i].m_iGroup )
        {
            for( j = i+1; j < ciN; j++ )
            {
                if ( 3 == ptRankings[j].m_iGroup )
                    i2U1 += (ptRankings[i].m_dwValue==ptRankings[j].m_dwValue) ? 1 : 2;
            }
        }
        else if ( 3 == ptRankings[i].m_iGroup )
        {
            for( j = i+1; j < ciN; j++ )
            {
                if ( 1 == ptRankings[j].m_iGroup )
                    i2U3 += (ptRankings[i].m_dwValue==ptRankings[j].m_dwValue) ? 1 : 2;
            }
        }
    }
    if ( 0 == (i2U1+i2U3) )
    {
        // degenerate case where there is only one group.
        fLowConfidence = fHighConfidence = 0;
        return false;
    }
    // assert( (i2U1+i2U3) == (2*iN1*iN3) );  // cannot assert because ties only add 1/2.
    float fPStat = ((float)i2U1) / (i2U1+i2U3);
    float fConfidence = (fPStat > 0.5f ? fPStat : (1.0f-fPStat));      // find and use a better definition of confidence.
    fConfidence = 2.0f*(fConfidence-0.5f);          // remap from 1..0.5 to 1..0
    fLowConfidence  *= fConfidence;
    fHighConfidence *= fConfidence;
    if ( (ciHighIdx-ciLowIdx) > 1 && iN2 > 0)
    {
        // we need to reduce the confidence by some factor if the population in the middle region is similar to either end region
        // turn collection of scores into scores in rank order.
        int i2U_12 = 0, i2U_21 = 0, i2U_23 = 0, i2U_32 = 0;     // accumulate twice U value 
        for( i = 0; i <= ciN; i++ )
        {
            switch( ptRankings[i].m_iGroup )
            {
            case 1:
                for( j = i+1; j <= ciN; j++ )
                {
                    if ( 2 == ptRankings[j].m_iGroup )
                        i2U_12 += (ptRankings[i].m_dwValue==ptRankings[j].m_dwValue) ? 1 : 2;
                }
                break;
            case 2:
                for( j = i+1; j <= ciN; j++ )
                {
                    if ( 1 == ptRankings[j].m_iGroup )
                        i2U_21 += (ptRankings[i].m_dwValue==ptRankings[j].m_dwValue) ? 1 : 2;
                    else if ( 2 == ptRankings[j].m_iGroup )
                        i2U_23 += (ptRankings[i].m_dwValue==ptRankings[j].m_dwValue) ? 1 : 2;

                }
                break;
            case 3:
                for( j = i+1; j <= ciN; j++ )
                {
                    if ( 2 == ptRankings[j].m_iGroup )
                        i2U_32 += (ptRankings[i].m_dwValue==ptRankings[j].m_dwValue) ? 1 : 2;
                }
                break;
            default:
                break;
            }
        }
        if ( 0 != (i2U_12+i2U_21) )
        {
            float fPStat12 = ((float)i2U_12) / (i2U_12+i2U_21);
            float fConfidence12 = (fPStat12 > 0.5f ? fPStat12 : (1.0f-fPStat12));      // find and use a better definition of confidence.
            fConfidence12 = 2.0f*(fConfidence12-0.5f);          // remap from 1..0.5 to 1..0
            fLowConfidence *= fConfidence12;
        }
        if ( 0 != (i2U_23+i2U_32) )
        {
            float fPStat23 = ((float)i2U_23) / (i2U_23+i2U_32);
            float fConfidence23 = (fPStat23 > 0.5f ? fPStat23 : (1.0f-fPStat23));      // find and use a better definition of confidence.
            fConfidence23 = 2.0f*(fConfidence23-0.5f);          // remap from 1..0.5 to 1..0
            fHighConfidence *= fConfidence23;
        }
    }
    return cfConfidenceThreshold < fLowConfidence || cfConfidenceThreshold < fHighConfidence;    // extreme probabilities implies separate distributions.
}

// ***************************************************************************************************************
// The cache replacement policy is the little tricky to evaluate
// basically we are going to correlate the measured latencies with each of the know cache replacement policies
// ***************************************************************************************************************

int tPartition::iCacheReplacementPolicy( int iCacheLevel, float &fConfidence )
{
    fConfidence = 0;
    if ( m_iLineSize <= 0 )
        return -1;
    int iMaxPolicy = -1;
    int iMinIdx = 0, iMaxIdx = 1;
    switch( m_eSequencer )
    {
    case eSequential:
        iMinIdx = m_iMinSize/m_iStepSize;
        iMaxIdx = m_iMaxSize/m_iStepSize;
        break;
    case ePower2:
        iMinIdx = iLog2(m_iMinSize/m_iStepSize);
        iMaxIdx = iLog2(m_iMaxSize/m_iStepSize);
        break;
    default:
        assert(0);
        break;;
    }
    int iMaxSampleIdx = ciTestLotSize*(iMaxIdx+1);
    // now sort first on Input and then on Values
    qsort( m_ptRankings, iMaxSampleIdx, sizeof(tRanking), CompareInputValues );
    const int ciNPolicies = 3;
    double* padHitRatio = new double[(ciNPolicies+1)*(iMaxIdx+1)];
    if ( NULL == padHitRatio )
        goto Exit;
    for( int i = iMinIdx; i <= iMaxIdx; i++ )
    {
        int idx = i*ciTestLotSize;
        if ( 0 < m_ptRankings[idx].m_dwValue )
        {
            tStridePattern * ptSP = m_ptLatencies[ m_ptRankings[idx].m_iIdx ].ptStridePattern();
            ptSP->SearchReplacementPolicy( iCacheLevel, ciNPolicies, padHitRatio+((ciNPolicies+1)*i) );
            int iValid;
            for( iValid = 1; iValid<ciTestLotSize; iValid++ )
            {
                if (0== m_ptRankings[idx].m_dwValue )
                    break;
            }
            // put median latency in top vector
            padHitRatio[((ciNPolicies+1)*i)+ciNPolicies] = 0.01*(m_ptRankings[idx+(iValid/2)].m_dwValue);
        }
        else
        {
            if ( i == iMinIdx )
                iMinIdx = i+1;
            else 
                iMaxIdx = i-1;
        }
    }
    int iRP;
    if ( gtGlobal.bChartDetails )
    {
        CSVPrintf( "Cache L%d, Replacement, %d, %d, Latency", 
                    iCacheLevel, 
                    iMinIdx*m_iStepSize, 
                    iMaxIdx*m_iStepSize 
                  );
        for( int i = iMinIdx; i <= iMaxIdx; i++ )
        {
            CSVPrintf( ", %6.2f", padHitRatio[ciNPolicies+((ciNPolicies+1)*i)] );
        }
        CSVPrintf( "\r\n" );
    }
    double dMaxRxy    = 0;
    iMaxPolicy = 0;  // assume 0
    for( iRP = 0; iRP<ciNPolicies; iRP++ )
    {
        double dRxy = dCorrelation( padHitRatio+iRP, padHitRatio+ciNPolicies, iMinIdx, iMaxIdx, (ciNPolicies+1) );
        if ( gtGlobal.bChartDetails )
        {
            CSVPrintf( "Policy, %d, Correl, %9.6f, Hit%%", iRP, dRxy );
            for( int i = iMinIdx; i <= iMaxIdx; i++ )
            {
                CSVPrintf( ", %7.2f", 100*padHitRatio[iRP+((ciNPolicies+1)*i)] );
            }
            CSVPrintf( "\r\n" );
        }
        double dARxy = fabs(dRxy);
        if ( dARxy > .75 )
        {   // don't believe we have a match unless we have correlation near 1
            if ( fabs( dARxy - dMaxRxy ) < 0.05*dMaxRxy )
            {
                if ( ( 2==iCacheLevel && 4==gtGlobal.tThisCacheInfo.dwL2ICacheNumWays ) ||
                     ( 1==iCacheLevel && 4==gtGlobal.tThisCacheInfo.dwL1ICacheNumWays )
                    )
                {
                    // prefer PLRU for 4-way caches
                    fConfidence = fConfidence*(2.0f/3.0f);  // two-way tie reduces confidence
                }
                else
                {
                    fConfidence /= 2;   // two-way tie reduces confidence
                }
            }
            else if ( dARxy > dMaxRxy )
            {
                dMaxRxy = dARxy;
                iMaxPolicy = iRP*eReplacePLRU;
                fConfidence = (float)dARxy;
            }
        }
    }

Exit:
    // now re-sort as it was just to be clean
    qsort( m_ptRankings, iMaxSampleIdx, sizeof(tRanking), CompareValues );
    delete [] padHitRatio;
    return iMaxPolicy;
}


// ***************************************************************************************************************
// Visualizing the Access Pattern helps us understand why it behaves the way it does.
// ***************************************************************************************************************
void tPartition::ShowAccessPattern( const char * pstrName, const int ciFromSize, const int ciToSize, const int tShowPatternOptions, const int ciMaxShow )
{
    if ( m_iLineSize>0 )
    {
        int iTopIdx = 1;
        int iMaxIdx = 1;
        int iMinIdx = 0;
        int iMinSize = (m_iMinSize<=ciFromSize && ciFromSize<=m_iMaxSize) ? ciFromSize : m_iMinSize;
        int iMaxSize = (m_iMinSize<=ciToSize   && ciToSize<=m_iMaxSize)   ? ciToSize   : m_iMaxSize;
        switch( m_eSequencer )
        {
        case eSequential:
            iMinIdx = iMinSize/m_iStepSize;
            iMaxIdx = iMaxSize/m_iStepSize;
            iTopIdx = m_iMaxSize/m_iStepSize;
            break;
        case ePower2:
            iMinIdx = iLog2(iMinSize/m_iStepSize);
            iMaxIdx = iLog2(iMaxSize/m_iStepSize);
            iTopIdx = iLog2(m_iMaxSize/m_iStepSize);
            break;
        default:
            assert(0);
            break;;
        }
        int iMaxSampleIdx = ciTestLotSize*(iTopIdx+1);
        // sort first on Input and then on Values
        qsort( m_ptRankings, iMaxSampleIdx, sizeof(tRanking), CompareInputValues );
        int i, j;
        for( i = iMinIdx; i <= iMaxIdx; i++ )
        {
            for( j = 0; j < ciTestLotSize; j++ )
            {
                int idx = i*ciTestLotSize + j;
                if ( 0 < m_ptRankings[idx].m_iGroup && 0 < m_ptRankings[idx].m_dwValue )
                {
                    tStridePattern * ptSP = m_ptLatencies[ m_ptRankings[idx].m_iIdx ].ptStridePattern();
                    ptSP->ShowAccessPattern( pstrName, tShowPatternOptions, ciMaxShow );
                    break;
                }
            }
        }
        // now re-sort as it was just to be clean
        qsort( m_ptRankings, iMaxSampleIdx, sizeof(tRanking), CompareValues );
    }
}



