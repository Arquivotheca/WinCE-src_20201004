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
// class tLatency encapsulates the methods need to properly measure latency
//
// ***************************************************************************************************************

#pragma once

#include "MemoryPerf.h"

class tLatency {
    public:
        tLatency();
        tLatency( const int ciLineSize, 
                  const tStridePattern &ctsp 
                 );
        tLatency( const int ciLineSize, 
                  const tStridePattern &ctsp, 
                  const int ciTLBSize, 
                  const DWORD cdwPWLatency );
        void Measure( DWORD * pdwBase, DWORD * pdwTop, int * piGrandTotal );
        void Measure( DWORD * pdwDstBase, DWORD * pdwDstTop, DWORD * pdwSrcBase, DWORD * pdwSrcTop, int * piGrandTotal );
        void LogReport( const char * pstrName );
        void LogReport( const char * pstrName, DWORD dwCompareable );
        void LogReport( const char * pstrName, DWORD dwOKLow, DWORD dwOKHigh );
        void SetExpected( const DWORD cdwExpected ) { m_dwExpected = cdwExpected>1 ? cdwExpected : 1; };
        DWORD dwLatency100() const { return m_dwLatency100; };
        void CopyLatencies( _Out_bytecap_(cbDst) void *pDst, const unsigned int cbDst, const int iStride ) const;
        int iValidMeasurements() const { return m_iValidMeasurements; };
        tStridePattern* ptStridePattern() { return &m_tsp; };
    private:
        void ChartDetails( const char * pstrName );
        void MedianLatency100( const int ciLots, DWORD *pdwBase1, DWORD *pdwTop1, DWORD *pdwBase2, DWORD *pdwTop2, const int ciRepeats, int *pGrandTotal );
        int m_iLineSize;
        tStridePattern m_tsp;
        DWORD * m_pdwBase;
        DWORD * m_pdwTop;
        DWORD m_dwLatency100;
        DWORD m_dwMeanLatency100;
        DWORD m_dwStdDevLatency100;
        DWORD m_dwTotalAccesses;
        int   m_iTLBSize;
        DWORD m_dwPageWalkLatency;
        DWORD m_dwPWCorrection;
        DWORD m_dwExpected;
        int   m_iValidMeasurements;
        DWORD m_pdwLatencies[ciTestLotSize];
};


// Return the index to the Media of an array of DWORDs.
int iMedianDWA( const DWORD* pdw, int iN, int* pMedianScore );


