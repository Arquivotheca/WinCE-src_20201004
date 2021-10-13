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
#pragma once


// ***************************************************************************************************************
//
// struct tStridePattern encapsulates the behavour of strided patterns which are used to access memory 
// in regular controlled ways to access a particular level of the memory hierarchy
//
// ***************************************************************************************************************

// ***************************************************************************************************************
// some constants
// note the valuse in the pstrTests and pstrReadWrite must correspond to the corresponding enum
// ***************************************************************************************************************
enum { eReadLoop1D=0, eWriteLoop1D, eReadLoop2D, eWriteLoop2D, eReadLoop3D, eMemCpyLoop, eCPULoop, eTopTest };
extern const __declspec(selectany) LPCSTR pstrTests[] = { "ReadLoop1D", "WriteLoop1D", "ReadLoop2D", "WriteLoop2D", "ReadLoop3D", "MemCopy", "CPULoop" };
extern const __declspec(selectany) LPCSTR pstrReadWrite[] = { "Read", "Write", "Read", "Write", "Read", "Read-Write", "CPU" };

// ShowPatternOptions
enum { eShowGrid=1, 
       eShowPattern=2, 
       eShowEachSummary=4, 
       eShowCacheDetails=8, 
       eShowStatistics=16,
       // some useful combinations
       eShowEachWithStatistics=20, 
       eShowPatternStatistics=22, 
       eShowPatternAll=30 
     }; 

enum { eL1=1, eL2=2, eL3=4, eL12=3, eL23=6  };  // RollOverFlags

// ***************************************************************************************************************
// common structure to specify a 1D or 2D memory access patter.
// for 1D, use the Y values.
// WARNING:  The first five of these values are accessed from assembly code
// If their offsets ever change, one will need to update the offsets in the .s files!!!
// ***************************************************************************************************************
typedef struct {
    int m_eTL;
    int m_iYStrides;
    int m_iYdwStride;
    int m_iXStrides;
    int m_iXdwStride;
    float m_fPreloadPercent;
    int m_iZStrides;
    int m_iZdwStride;
    // following needed for the iterator iFirstOffset & iNextOffset used in cache simulations
    int m_iX;
    int m_iY;
    int m_iZ;
    int m_iROFlags; // only used for CSV output.
public:
    DWORD * const pdwOffsetBase( _Inout_ptrdiff_count_(pdwTop) DWORD * const pdwBase, const DWORD * const pdwTop );
    bool bVerifyAccessRange( const int ciDWSize ) const;
    unsigned int uiSpanSizeBytes() const;
    unsigned int uiAccessesPerPattern() const ;
    double dPageWalksRatio( const int ciPageOrSectionSize, const int ciTLBSize );
    void SearchReplacementPolicy( const int ciCacheLevel, const int ciNPolicies, double* padHitRatio );
    void AnalyzeAccessPattern( int* piL1Hits, int* piL2Hits, int* piMBHits, int* piPageWalks, int* piRASChanges );
    void ShowAccessPattern( const char * pstrName, const int tShowPatternOptions, const int ciMaxShow );
    void ShowParameters();
    int iFirstOffset();
    int iNextOffset();
} tStridePattern;

