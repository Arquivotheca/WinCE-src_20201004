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
// enums, structs, and a class to encapsulate how process of measuring a series of latencies and estimating a size
// 
// Intent is to provide methods which measure latencies at many different variations of a basic access pattern
// And in so doing, expose the underlying configuration of a particular part of the memory hierarchy
//
// ***************************************************************************************************************

#pragma once

#include "MemoryPerf.h"
#include "Latency.h"


// ***************************************************************************************************************
// control enums to describe the variation method
// ***************************************************************************************************************

enum { eSequential, ePower2 };
enum { eStepYStrides, eStepYStride, eStepConstYSpan, eStepXStrides, eStepXStride, eStepConstXSpan };

// ***************************************************************************************************************
// In a Stride Pattern, we have four parameters which we may want to vary.
// We also want to vary Sequential or Power2 variation
// not all combinations make sense but these have a chance
// 1D x (S|P) x (Strides|stride) = 4 combinations
// 2D x (S|P) x (YStrides|YStride|XStrides|XStride) = 8 combinations.
// ***************************************************************************************************************

// ***************************************************************************************************************
// tRanking is used to keep info around from each set of measurements
// ***************************************************************************************************************

typedef struct {
    DWORD m_dwValue;      // Latency
    int   m_iGroup;       // classification group 1..3 or 0 empty
    int   m_iInput;       // control or input info
    int   m_iIdx;         // index into tPartition.m_ptLatencies[]
} tRanking;

// special scale arguments for LogReport
const int ciMaxMedianReport = 2;
const int ciCSVReportOnly   = 3;


// ***************************************************************************************************************
// the tPartition Class
// The name tPartition refers to the process of partition a set of mesaurements into regions where the transitions 
// between the regions expose the underlying size of interest
// ***************************************************************************************************************

class tPartition {
public:
    tPartition( 
        const int iLineSize,
        const int iMinSize, 
        const int iMaxSize, 
        const int iStepSize,
        const int eSeq, 
        const int eStep, 
        const tStridePattern &tStrideInfo,
        const int iCsvScale
        );
    ~tPartition();
    void EffectiveSize( DWORD *pdwBase, DWORD *pdwTop, int *pGrandTotal );
    int  iCacheReplacementPolicy( int iCacheLevel, float &fConfidence );
    void LogReport( const char * pstrName );
    void LogReport( const char * pstrName, const int ciScale );
    void LogReport( const char * pstrName, const int ciScale, const int ciExpectedMinSize, const int ciExpectedMaxSize );
    void LogReport( const char * pstrName, const int ciScale, const int ciExpectedSize, const float fTol );
    void SetExpected( const DWORD cdwExpected ) { m_dwExpected = cdwExpected>1 ? cdwExpected : 1; };
    void SetLatency( const DWORD dwLow, const DWORD dwHigh ) { m_dwLowLatency = dwLow;  m_dwHighLatency = dwHigh; };
    void SetPageWalkLatency( const int iTLBSize, const DWORD dwPWLatency ) { m_iTLBSize = iTLBSize; m_dwPageWalkLatency = dwPWLatency; };
    void SetReduce256ths( const int iReduce256ths ) { m_iReduce256ths = iReduce256ths; };
    int iMinSize()  const { return m_iMinSize; };
    int iLowSize()  const { return m_bPartitioned ? m_iLowSize : 0; };
    int iHighSize() const { return m_bPartitioned ? m_iHighSize : 0; };
    int iMaxSize()  const { return m_iMaxSize; };
    int iMeasuredSize() const { return m_iMeasuredSize; };
    DWORD dwLowLatency()  const { return m_bPartitioned ? m_dwLowLatency : 0; };
    DWORD dwHighLatency() const { return m_bPartitioned ? m_dwHighLatency : 0; };
    bool bIsPartitioned() const { return m_bPartitioned; };
    bool bIsMeasured() const { return m_bMeasured; };
    float fLowConfidence()  const { return m_fLowConfidence; };
    float fHighConfidence() const { return m_fHighConfidence; };
    DWORD dwMaxMedianLatency() const { return m_tMaxMedianLatency.dwLatency100(); };
    void ShowAccessPattern( const char * pstrName, const int ciFromSize, const int ciToSize, const int tShowPatternOptions, const int ciMaxShow );
private:
    int m_iMinSize;               // lower bound of range
    int m_iLowSize;               // end of low region - region where latency is close to dwLowLatency
    int m_iHighSize;              // start of high region - region where latency is close to dwHighLatency
    int m_iMaxSize;               // upper bound of range
    int m_iMeasuredSize;
    int m_eSequencer;
    int m_eStepping;
    tStridePattern m_tStrideInfo;
    int m_iCsvScale;
    int m_iStepSize;              // Scale factor to use - normally 1
    int m_iBaseStrides;
    int m_iConstSpan;
    int m_iReduce256ths;          // mechanism to reduce stride count slightly to avoid full cache effects
    int m_iLineSize;
    DWORD m_dwExpected;
    DWORD * m_pdwBase;
    DWORD * m_pdwTop;
    DWORD m_dwLowLatency;
    DWORD m_dwHighLatency;
    tLatency m_tMaxMedianLatency;
    int m_iTLBSize;
    DWORD m_dwPageWalkLatency;
    bool m_bCorrected;
    bool m_bNotAllocated;
    bool m_bPartitioned;
    bool m_bMeasured;
    DWORD * m_pdwLatencies;
    tRanking * m_ptRankings;
    tLatency * m_ptLatencies;
    DWORD m_dwSign;
    float m_fLowConfidence;
    float m_fHighConfidence;
private:
    int iStrideFromIndex( const int idx, const int iIdxTop ) const;
    int iIndexFromStride( const int iStrides, const int iIdxTop ) const;
};


// ***************************************************************************************************************
// a couple of useful constants used many places
// ***************************************************************************************************************

const float cfConfidenceThreshold = 0.9f;
const int ciFastestMemory = 70;


// ***************************************************************************************************************
// signing objects are only to reduce issues when debugging
// commonly, I want to abort the program after a break-point to make a change and rebuild.
// I get back to the main winmain function, then <set next statement> to near the end of the function.
// junk objects which were never created seem to be deleted on WinMain exit.
// ***************************************************************************************************************

#define PARTITIONSIGN 0xFAB71710

