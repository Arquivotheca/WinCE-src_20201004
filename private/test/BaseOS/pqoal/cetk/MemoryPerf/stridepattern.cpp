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
// These functions implement the behavior of class tStridePattern
//
// ***************************************************************************************************************


#include <math.h>
#include "MemoryPerf.h"
#include "cache.h"


// ***************************************************************************************************************
// randomly offset from the base of a possibly large buffer taking into account the size of the pattern and 
// aligning the offset to a multiple of the stride
// ***************************************************************************************************************

DWORD * const tStridePattern::pdwOffsetBase( DWORD * const pdwBase, const DWORD * const pdwTop )
{
    // first establish a base offset to vary the virtual-to-physical mapping
    DWORD dwStride = 1;
    DWORD dwSpan   = 1;
    UINT uiRand = 0;
    switch( m_eTL )
    {
    case eReadLoop1D: case eWriteLoop1D: case eCPULoop: case eMemCpyLoop:
            dwStride = m_iYdwStride;
            dwSpan   = (m_iYStrides-1)*m_iYdwStride + 1;
            break;
    case eReadLoop2D: case eWriteLoop2D:
            if ( m_iYdwStride < m_iXdwStride )
                dwStride = m_iYdwStride;
            else
                dwStride = m_iXdwStride;
            dwSpan = (m_iYStrides-1)*m_iYdwStride + (m_iXStrides-1)*m_iXdwStride + 1;
            break;
    case eReadLoop3D:
            if ( m_iZdwStride < m_iXdwStride )
                dwStride = m_iZdwStride;
            else
                dwStride = m_iXdwStride;
            dwSpan = (m_iZStrides-1)*m_iZdwStride + (m_iXStrides-1)*m_iXdwStride + 1;
            break;
    default:
        assert(0);
        return pdwBase;
    }
    rand_s(&uiRand);
    DWORD dwDWBaseOffset =  (DWORD)((((pdwTop - dwSpan) - pdwBase )*((__int64)uiRand))/UINT_MAX);
    dwDWBaseOffset = (dwDWBaseOffset/dwStride)*dwStride;       // make multiple of stride
    return pdwBase + dwDWBaseOffset;
}

// ***************************************************************************************************************
// verify the pattern will fit in this available size
// ***************************************************************************************************************

bool tStridePattern::bVerifyAccessRange( const int ciDWSize ) const
{
    switch( m_eTL )
    {
    case eReadLoop1D: case eWriteLoop1D: case eCPULoop: case eMemCpyLoop:
        {
            return ( (m_iYStrides-1) * m_iYdwStride + 1) <= ciDWSize;
        }
    case eReadLoop2D:  case eWriteLoop2D:
        {
            return ( ((m_iYStrides-1) * m_iYdwStride) + ((m_iXStrides-1) * m_iXdwStride) + 1) <= ciDWSize;
        }
    case eReadLoop3D:  
        {
            return ( ((m_iZStrides-1) * m_iZdwStride) + ((m_iXStrides-1) * m_iXdwStride) + 1) <= ciDWSize;
        }
    default:
        assert(0);
        break;
    }
    return false;
}


// ***************************************************************************************************************
// return the size or range of memory accessed by this pattern
// ***************************************************************************************************************

unsigned int tStridePattern::uiSpanSizeBytes( ) const
{
    switch( m_eTL )
    {
    case eReadLoop1D: case eWriteLoop1D: case eCPULoop: case eMemCpyLoop:
        {
            return ( ( (m_iYStrides-1) * m_iYdwStride + 1 )*sizeof(DWORD) );
        }
    case eReadLoop2D:  case eWriteLoop2D:  
        {
            return ( ( ((m_iYStrides-1) * m_iYdwStride) + ((m_iXStrides-1) * m_iXdwStride) + 1 )*sizeof(DWORD) );
        }
    case eReadLoop3D:
        {
            return ( ( ((m_iZStrides-1) * m_iZdwStride) + ((m_iXStrides-1) * m_iXdwStride) + 1 )*sizeof(DWORD) );
        }
    default:
        assert(0);
        return 0;
    }
}


// ***************************************************************************************************************
// return the number of DWORD accesses per repetition of the pattern
//
// note memcpy returns the number of transfers not the number of accesses since each transfer involves two accesses
// ***************************************************************************************************************

unsigned int tStridePattern::uiAccessesPerPattern( ) const
{
    switch( m_eTL )
    {
    case eReadLoop1D: case eWriteLoop1D: case eCPULoop:  case eMemCpyLoop:
        {
            return ( m_iYStrides );
        }
    case eReadLoop2D:  case eWriteLoop2D: 
        {
            return ( m_iYStrides * m_iXStrides );
        }
    case eReadLoop3D: 
        {
            return ( (1 + m_iZStrides) * m_iYStrides * m_iXStrides );
        }
    default:
        assert(0);
        return 0;
    }
}


// ***************************************************************************************************************
// returns the ratio of page walks to accesses using a simulation of the cache 
// where cache behavior is based on whatever sizes and policies that have been discovered or assumed up until this time
// ***************************************************************************************************************

double tStridePattern::dPageWalksRatio( const int ciPageOrSectionSize, const int ciTLBSize )
{
    SetofCaches * pSC = new SetofCaches( 0, 0, 0, 0, 0, 0, 0, 0, ciTLBSize );
    if ( NULL == pSC )
        return 0;
    if ( ! ( pSC->IsTLB() ) )
    {
        delete pSC;
        return 0;
    }

    // ***************************************************************************************************
    // step through the pattern a number of times (counted by iPhase) 
    int eType = (eWriteLoop1D==m_eTL || eWriteLoop2D==m_eTL || eMemCpyLoop==m_eTL) ? eWrite : eRead;
    int iTopPhase = (eReadLoop2D==m_eTL || eWriteLoop2D==m_eTL || eReadLoop3D==m_eTL) ? 15 : 1;     // need several passes before cache accesses become stable
    int iPriorPageWalks = -1, iTotalPageWalks = 0;
    int iAccessesPerPattern = uiAccessesPerPattern();
    for( int iPhase = 0; iPhase <= iTopPhase; iPhase++ )
    {
        int iOffset, i;
        for( iOffset = iFirstOffset(), i=0; iOffset>=0; iOffset = iNextOffset(), i++ )
        {
            pSC->CacheAccess( eType, iOffset*sizeof(DWORD), 1 );
            if ( (32*ci1K) < i )
            {   // early exit to avoid wasteful calculations for very long access patterns
                // we may need to revise if huge TLB become reality but this should accommodate most patterns with 128 TLB registers
                iAccessesPerPattern = i;
                break;
            }
        }
        int iPageWalks = 0;
        pSC->AccessInfo( NULL, NULL, NULL, &iPageWalks, NULL );
        if ( iPhase > 0 )
        {   // after preloading phase, clear so we return stats for typical accesses
            iTotalPageWalks += iPageWalks;
            if ( iPriorPageWalks==iPageWalks )
            {   // early exit if stable cache access stats
                iTopPhase = iPhase;
                break;
            }
        }
        iPriorPageWalks = iPageWalks;
        pSC->ClearCounts();
    }
    delete pSC;
    return ((double)iTotalPageWalks)/(iAccessesPerPattern*iTopPhase);
}


// ***************************************************************************************************************
// Analyze the Access Pattern helps us understand why the pattern behaves the way it does.
// useful in diagnoses issues as well as designing patterns to accomplish their intended task
// ***************************************************************************************************************

void tStridePattern::AnalyzeAccessPattern( int* piL1Hits, int* piL2Hits, int* piMBHits, int* piPageWalks, int* piRASChanges )
{
    if ( !( NULL!=piL1Hits && NULL!=piL2Hits && NULL!=piMBHits && NULL!=piPageWalks && NULL!=piRASChanges ) )
    {
        assert( NULL!=piL1Hits && NULL!=piL2Hits && NULL!=piMBHits && NULL!=piPageWalks && NULL!=piRASChanges );
        return;
    }
    int iTLBSize = gtGlobal.iTLBSize > 0 ? gtGlobal.iTLBSize : 32;
    int iL1LineSize  = gtGlobal.iL1LineSize>0 ? gtGlobal.iL1LineSize : 32;
    int iL1SizeBytes = gtGlobal.iL1SizeBytes;
    int iL2LineSize  = gtGlobal.iL2LineSize>0 ? gtGlobal.iL2LineSize : 32;
    int iL2SizeBytes = gtGlobal.iL2SizeBytes;
    int iL1Ways = 4;
    int iL2Ways = 4;
    if ( gtGlobal.bThisCacheInfo )
    {
        iL1Ways = gtGlobal.tThisCacheInfo.dwL1DCacheNumWays>0 ? gtGlobal.tThisCacheInfo.dwL1DCacheNumWays : 4;
        iL2Ways = gtGlobal.tThisCacheInfo.dwL2DCacheNumWays>0 ? gtGlobal.tThisCacheInfo.dwL2DCacheNumWays : 4;
    }
    int iL1Policy = ((gtGlobal.tThisCacheInfo.dwL1Flags&CF_WRITETHROUGH) ? eWriteThrough : eWriteBehind)
                           | (gtGlobal.bL1WriteAllocateOnMiss ? eWriteMissAllocate : eWriteMissNoAllocate)
                           | gtGlobal.iL1ReplacementPolicy;
    int iL2Policy = ((gtGlobal.tThisCacheInfo.dwL2Flags&CF_WRITETHROUGH) ? eWriteThrough : eWriteBehind)
                           | (gtGlobal.bL2WriteAllocateOnMiss ? eWriteMissAllocate : eWriteMissNoAllocate)
                           | gtGlobal.iL2ReplacementPolicy;
    SetofCaches * pSC = new SetofCaches( iL1LineSize, iL1Ways, iL1SizeBytes/(iL1LineSize*iL1Ways), iL1Policy,
                                         iL2LineSize, iL2Ways, iL2SizeBytes/(iL2LineSize*iL2Ways), iL2Policy,
                                         iTLBSize
                                      );
    if ( NULL == pSC )
        return;
    if ( ! ( pSC->AreCachesValid() && pSC->IsTLB() ) )
    {
        delete pSC;
        return;
    }

    // ***************************************************************************************************
    // step through the pattern a number of times (counted by iPhase)
    int eType = (eWriteLoop1D==m_eTL || eWriteLoop2D==m_eTL || eMemCpyLoop==m_eTL ) ? eWrite : eRead;
    int iTopPhase = (eReadLoop2D==m_eTL || eWriteLoop2D==m_eTL || eReadLoop3D==m_eTL) ? 15 : 1;
    int iPriorL1Hits=0, iPriorL2Hits=0, iPriorMBHits=0, iPriorPageWalks=0, iPriorRASChanges=0;
    int iTotalL1Hits=0, iTotalL2Hits=0, iTotalMBHits=0, iTotalPageWalks=0, iTotalRASChanges=0;
    for( int iPhase = 0; iPhase <= iTopPhase; iPhase++ )
    {
        int iter = 0;   // just for debugging so you know where you are
        int iOffset;
        for( iOffset = iFirstOffset(); iOffset>=0; iOffset = iNextOffset(), iter++ )
        {
            pSC->CacheAccess( eType, iOffset*sizeof(DWORD), 1 );
        }
        pSC->AccessInfo( piL1Hits, piL2Hits, piMBHits, piPageWalks, piRASChanges );
        if ( iPhase > 0 )
        {   
            iTotalL1Hits += *piL1Hits;
            iTotalL2Hits += *piL2Hits;
            iTotalMBHits += *piMBHits;
            iTotalPageWalks += *piPageWalks;
            iTotalRASChanges += *piRASChanges;
            if ( iPriorL1Hits==*piL1Hits && iPriorL2Hits==*piL2Hits && iPriorMBHits==*piMBHits &&
                 iPriorPageWalks==*piPageWalks && iPriorRASChanges==*piRASChanges )
            {   // early exit if stable cache access stats
                iTopPhase = iPhase;
                break;
            }
        }
        iPriorL1Hits = *piL1Hits;
        iPriorL2Hits = *piL2Hits;
        iPriorMBHits = *piMBHits;
        iPriorPageWalks = *piPageWalks;
        iPriorRASChanges = *piRASChanges;
        pSC->ClearCounts();
    }
    *piL1Hits     = iTotalL1Hits/iTopPhase;
    *piL2Hits     = iTotalL2Hits/iTopPhase;
    *piMBHits     = iTotalMBHits/iTopPhase;
    *piPageWalks  = iTotalPageWalks/iTopPhase;
    *piRASChanges = iTotalRASChanges/iTopPhase;
    delete pSC;
}


// ***************************************************************************************************************
// a helper function used by ShowAccessPatterns below
// ***************************************************************************************************************

void tStridePattern::ShowParameters()
{
    if ( m_iZStrides>0 )
    {
        CSVPrintf( " {, %s, %d, %d, %d, %d, %d, %d, %f%%, },",
            pstrTests[m_eTL],
            m_iZStrides, m_iZdwStride*sizeof(DWORD), 
            m_iYStrides, m_iYdwStride*sizeof(DWORD), 
            m_iXStrides, m_iXdwStride*sizeof(DWORD),
            m_fPreloadPercent
            );
    }
    else if ( m_iXStrides>0 )
    {
        CSVPrintf( " {, %s, %d, %d, %d, %d, %f%%, },",
            pstrTests[m_eTL],
            m_iYStrides, m_iYdwStride*sizeof(DWORD), 
            m_iXStrides, m_iXdwStride*sizeof(DWORD),
            m_fPreloadPercent
            );
    }
    else
    {
        CSVPrintf( " {, %s, %d, %d, %f%%, },",
            pstrTests[m_eTL],
            m_iYStrides, m_iYdwStride*sizeof(DWORD),
            m_fPreloadPercent
            );
    }
}


// ***************************************************************************************************************
// Visualizing the Access Pattern helps understand why it behaves the way it does.
// ***************************************************************************************************************

void tStridePattern::ShowAccessPattern( const char * pstrName, const int tShowPatternOptions, const int ciMaxShow )
{
    int iTLBSize = gtGlobal.iTLBSize > 0 ? gtGlobal.iTLBSize : 32;
    int iL1LineSize  = gtGlobal.iL1LineSize>0 ? gtGlobal.iL1LineSize : 32;
    int iL1SizeBytes = gtGlobal.iL1SizeBytes;
    int iL2LineSize  = gtGlobal.iL2LineSize>0 ? gtGlobal.iL2LineSize : 32;
    int iL2SizeBytes = gtGlobal.iL2SizeBytes;
    int iL1Ways = 4;
    int iL2Ways = 4;
    if ( gtGlobal.bThisCacheInfo )
    {
        iL1Ways = gtGlobal.tThisCacheInfo.dwL1DCacheNumWays>0 ? gtGlobal.tThisCacheInfo.dwL1DCacheNumWays : 4;
        iL2Ways = gtGlobal.tThisCacheInfo.dwL2DCacheNumWays>0 ? gtGlobal.tThisCacheInfo.dwL2DCacheNumWays : 4;
    }
    int iL1Policy = ((gtGlobal.tThisCacheInfo.dwL1Flags&CF_WRITETHROUGH) ? eWriteThrough : eWriteBehind)
                           | (gtGlobal.bL1WriteAllocateOnMiss ? eWriteMissAllocate : eWriteMissNoAllocate)
                           | gtGlobal.iL1ReplacementPolicy;
    int iL2Policy = ((gtGlobal.tThisCacheInfo.dwL2Flags&CF_WRITETHROUGH) ? eWriteThrough : eWriteBehind)
                           | (gtGlobal.bL2WriteAllocateOnMiss ? eWriteMissAllocate : eWriteMissNoAllocate)
                           | gtGlobal.iL2ReplacementPolicy;
    SetofCaches * pSC = new SetofCaches( iL1LineSize, iL1Ways, iL1SizeBytes/(iL1LineSize*iL1Ways), iL1Policy,
                                         iL2LineSize, iL2Ways, iL2SizeBytes/(iL2LineSize*iL2Ways), iL2Policy,
                                         iTLBSize
                                      );
    if ( NULL == pSC )
        return;
    if ( ! ( pSC->AreCachesValid() && pSC->IsTLB() ) )
    {
        delete pSC;
        return;
    }

    // ***************************************************************************************************
    // step through the pattern multiple times counted by iPhase
    int iX, iY, iPhase; 
    bool b3D = eReadLoop3D==m_eTL;
    int iXStrides = (eReadLoop2D==m_eTL || eWriteLoop2D==m_eTL || eReadLoop3D==m_eTL) ? m_iXStrides : 1;
    int eType = (eWriteLoop1D==m_eTL || eWriteLoop2D==m_eTL || eMemCpyLoop==m_eTL ) ? eWrite : eRead;
    int iLastPhase = iXStrides>1 ? 15 : 1;
    const int ciExtraReportPhase = 1;
    int iL1TotalHits=0, iL2TotalHits=0, iMBTotalHits=0, iPageWalksTotal=0, iRASChangesTotal=0;
    __int64 i64L1SqrHits=0, i64L2SqrHits=0, i64MBSqrHits=0, i64PageWalksSqr=0, i64RASChangesSqr=0;
    int iPriorL1Hits=0, iPriorL2Hits=0, iPriorMBHits=0, iPriorPageWalks=0, iPriorRASChanges=0;
    if ( tShowPatternOptions != eShowGrid )
    {
        for( iPhase = 0; iPhase <= iLastPhase; iPhase++ )
        {
            bool bCSVPhase = ciExtraReportPhase == iPhase || iLastPhase == iPhase;
            if ( bCSVPhase && (tShowPatternOptions&eShowPattern) )
            {
                CSVPrintf( "\r\nStride Pattern, %s, ", pstrName );
                ShowParameters();
                CSVPrintf( " for repeat, %d\r\n", iPhase );
                CSVPrintf( "Stride Pattern, hex-flags, L1 Hit, 10000000, L2 Hit, 20000000, MB, 40000000, PageWalk, 80000000\r\n" );
                if ( iXStrides>1 )
                {
                    if ( b3D )
                    {
                        CSVPrintf( "Stride Pattern, %s, Z, Y\\X", pstrName );
                    }
                    else
                    {
                        CSVPrintf( "Stride Pattern, %s, Y\\X", pstrName );
                    }
                    for( iX = 0;  iX < iXStrides;  iX++ )
                    {
                        CSVPrintf( ", %d", iX );
                    }
                }
                else
                {
                    CSVPrintf( "Stride Pattern, %s, Y", pstrName );
                    for( iY = 0;  iY < m_iYStrides;  iY++ )
                    {
                        CSVPrintf( ", %d", iY );
                    }
                }
                CSVPrintf( "\r\n" );
            }
            int iOffset, i;
            for( iOffset = iFirstOffset(), i=0; iOffset>=0; iOffset = iNextOffset() )
            {
                if ( 0!=m_iROFlags && bCSVPhase && (tShowPatternOptions&eShowPattern) )
                {
                    if ( 0!=(m_iROFlags&eL12) )
                    {
                        CSVPrintf( "\r\n" );
                        if ( 0 < ciMaxShow && ciMaxShow < ++i )
                        {
                            CSVPrintf( "Stride Pattern, %s, ...\r\n", pstrName );
                            break;  // don't show more too much
                        }
                    }
                    if ( b3D )
                    {
                        CSVPrintf( "Stride Pattern, %s, %d, %d", pstrName, m_iZ, m_iY);
                    }
                    else
                    {
                        CSVPrintf( "Stride Pattern, %s, %d", pstrName, m_iY);
                    }
                }
                int iL1MissCount=0, iL2MissCount=0, iPageWalkCount=0, iRASChanges=0;
                pSC->CacheAccess( eType, iOffset*sizeof(DWORD), 1, &iL1MissCount, &iL2MissCount, &iPageWalkCount, &iRASChanges );
                if ( bCSVPhase && (tShowPatternOptions&eShowPattern) )
                {
                    DWORD dwFlag = 0;
                    if (iL2MissCount>0) 
                        dwFlag |= 0x40000000;   // MB hit
                    else if (iL1MissCount>0) 
                        dwFlag |= 0x20000000;   // L2 hit
                    else
                        dwFlag |= 0x10000000;   // L1 hit
                    if (iPageWalkCount>0) dwFlag |= 0x80000000;
                    CSVPrintf( ", %08x", dwFlag | (iOffset*sizeof(DWORD)) );
                }
            }
            if ( bCSVPhase && (tShowPatternOptions&eShowPattern) )
            {
                CSVPrintf( "\r\n" );
            }

            int iL1Hits=0, iL2Hits=0, iMBHits=0, iPageWalks=0, iRASChanges=0;
            pSC->AccessInfo( &iL1Hits, &iL2Hits, &iMBHits, &iPageWalks, &iRASChanges );
            if ( iPhase>0 && (tShowPatternOptions & eShowEachSummary) )
            {
                CSVPrintf( "Stride Pattern, %s", pstrName );
                ShowParameters();
                CSVPrintf( " Repeat, %d, Summary, L1, %d, L2, %d, MB, %d, PW, %d, RAS, %d\r\n",
                            iPhase,
                            iL1Hits, 
                            iL2Hits, 
                            iMBHits, 
                            iPageWalks,
                            iRASChanges
                            );
                if ( tShowPatternOptions & eShowCacheDetails )
                {
                    pSC->CSVCacheInfo();
                }
            }
            if ( iPhase > 0 )
            {
                iL1TotalHits += iL1Hits;
                i64L1SqrHits   += ((_int64)iL1Hits)*iL1Hits;
                iL2TotalHits += iL2Hits;
                i64L2SqrHits   += ((_int64)iL2Hits)*iL2Hits;
                iMBTotalHits += iMBHits;
                i64MBSqrHits   += ((_int64)iMBHits)*iMBHits;
                iPageWalksTotal += iPageWalks;
                i64PageWalksSqr   += ((_int64)iPageWalks)*iPageWalks;
                iRASChangesTotal += iRASChanges;
                i64RASChangesSqr += ((_int64)iRASChanges)*iRASChanges;
            }
            if ( iLastPhase != iPhase )
            {   // clear so we return stats for typical accesses
                pSC->ClearCounts();
            }
            if ( 0 == iPhase )
            {
                pSC->ClearSetCounts();
            }
            // check for early exit due to stable cache access statistics
            if ( iPhase > 0 && iPriorL1Hits==iL1Hits && iPriorL2Hits==iL2Hits && iPriorMBHits==iMBHits &&
                 iPriorPageWalks==iPageWalks && iPriorRASChanges==iRASChanges )
            {
                if ( iPhase >= ciExtraReportPhase )
                {
                    iLastPhase = iPhase;
                    break;
                }
                iLastPhase = iPhase+1;
            }
            iPriorL1Hits     = iL1Hits;
            iPriorL2Hits     = iL2Hits;
            iPriorMBHits     = iMBHits;
            iPriorPageWalks  = iPageWalks;
            iPriorRASChanges = iRASChanges;

        }
    }
    // ***************************************************************************************************
    // show statistics
    if (  tShowPatternOptions & eShowStatistics )
    {
        double dL1Mean   =  ((double)iL1TotalHits)/iLastPhase;
        double dVar      = (((double)i64L1SqrHits) - (dL1Mean*dL1Mean*iLastPhase))/(iLastPhase-1);
        double dL1StdDev = dVar > 0 ? sqrt( dVar ) : 0.0;
        double dL2Mean   =  ((double)iL2TotalHits)/iLastPhase;
               dVar      = (((double)i64L2SqrHits) - (dL2Mean*dL2Mean*iLastPhase))/(iLastPhase-1);
        double dL2StdDev = dVar > 0 ? sqrt( dVar ) : 0.0;
        double dMBMean   =  ((double)iMBTotalHits)/iLastPhase;
               dVar      = (((double)i64MBSqrHits) - (dMBMean*dMBMean*iLastPhase))/(iLastPhase-1);
        double dMBStdDev = dVar > 0 ? sqrt( dVar ) : 0.0;
        double dPWMean   =  ((double)iPageWalksTotal)/iLastPhase;
               dVar      = (((double)i64PageWalksSqr) - (dPWMean*dPWMean*iLastPhase))/(iLastPhase-1);
        double dPWStdDev = dVar > 0 ? sqrt( dVar ) : 0.0;
        double dRASMean   =  ((double)iRASChangesTotal)/iLastPhase;
               dVar       = (((double)i64RASChangesSqr) - (dRASMean*dRASMean*iLastPhase))/(iLastPhase-1);
        double dRASStdDev = dVar > 0 ? sqrt( dVar ) : 0.0;

        CSVPrintf( "Stride Pattern, %s, ", pstrName);
        ShowParameters();
        CSVPrintf( " Repeats, %d, Mean, StdDev, L1, %6.2f, %6.2f, L2, %6.2f, %6.2f, MB, %6.2f, %6.2f, PW, %6.2f, %6.2f, RAS, %6.2f, %6.2f\r\n",
                    iLastPhase,
                    dL1Mean, dL1StdDev,
                    dL2Mean, dL2StdDev,
                    dMBMean, dMBStdDev,
                    dPWMean, dPWStdDev,
                    dRASMean, dRASStdDev
                    );
        pSC->CSVSetCounts( 3, iLastPhase );
    }
    // ***************************************************************************************************
    // another way to show the pattern is as a series of lines on a address grid
    if ( (tShowPatternOptions & eShowGrid ) && m_iXStrides>1 )
    {
        // select the Y spacing since it can be very dense
        // Useful for showing patterns where m_iXdwStride >> m_iYStrides.
        CSVPrintf( "\r\n\
Chart to visualize access pattern sequence of %s with time\r\n\
Recommendation: Select column D using a \"Lines and Markers\" Chart.\r\n\
Then choose \"Select Data\" and \"edit horizontal axis\" and select column C as the X-axis\r\n", pstrName );
        CSVPrintf( "Stride Grid, %s", pstrName );
        ShowParameters();
        CSVPrintf( "\r\n" );
        int iOffset, i;
        for( iOffset = iFirstOffset(), i=0; iOffset>=0; iOffset = iNextOffset(), i++ )
        {
            CSVPrintf( "Stride Grid, %s, %11.6f, %d\r\n", 
                        pstrName, 
                        ((double)i)/m_iXStrides,
                        iOffset*sizeof(DWORD) 
                      );
            if ( 0 < ciMaxShow && ciMaxShow < i )
            {
                CSVPrintf( "Stride Grid, %s, ...\r\n", pstrName );
                break;  // don't show more too much
            }
        }
        CSVPrintf( "\r\n" );
    }
    delete pSC;
}


// ***************************************************************************************************************
// Find CacheReplacement Policy
// ***************************************************************************************************************

void tStridePattern::SearchReplacementPolicy( const int ciCacheLevel, const int ciNPolicies, double* padHitRatio )
{
    if ( !( 1 <= ciCacheLevel && ciCacheLevel <= 2 ) )
    {
        assert( 1 <= ciCacheLevel && ciCacheLevel <= 2 );
        return;
    }
    if ( NULL == padHitRatio )
    {
        assert( NULL != padHitRatio );
        return;
    }
    int iTLBSize = gtGlobal.iTLBSize > 0 ? gtGlobal.iTLBSize : 32;
    int iL1LineSize  = gtGlobal.iL1LineSize>0 ? gtGlobal.iL1LineSize : 32;
    int iL1SizeBytes = gtGlobal.iL1SizeBytes;
    int iL2LineSize  = gtGlobal.iL2LineSize>0 ? gtGlobal.iL2LineSize : 32;
    int iL2SizeBytes = gtGlobal.iL2SizeBytes;
    int iL1Ways = 4;
    int iL2Ways = 4;
    if ( gtGlobal.bThisCacheInfo )
    {
        iL1Ways = gtGlobal.tThisCacheInfo.dwL1DCacheNumWays>0 ? gtGlobal.tThisCacheInfo.dwL1DCacheNumWays : 4;
        iL2Ways = gtGlobal.tThisCacheInfo.dwL2DCacheNumWays>0 ? gtGlobal.tThisCacheInfo.dwL2DCacheNumWays : 4;
    }
    int iL1Policy = ((gtGlobal.tThisCacheInfo.dwL1Flags&CF_WRITETHROUGH) ? eWriteThrough : eWriteBehind)
                           | (gtGlobal.bL1WriteAllocateOnMiss ? eWriteMissAllocate : eWriteMissNoAllocate);
    int iL2Policy = ((gtGlobal.tThisCacheInfo.dwL2Flags&CF_WRITETHROUGH) ? eWriteThrough : eWriteBehind)
                           | (gtGlobal.bL2WriteAllocateOnMiss ? eWriteMissAllocate : eWriteMissNoAllocate);
    switch ( ciCacheLevel )
    {
    case 1:
            iL2Policy |= gtGlobal.iL2ReplacementPolicy;
            break;
    case 2:
            iL1Policy |= gtGlobal.iL1ReplacementPolicy;
            break;
    default:
            break;
    }
    SetofCaches * pSC = new SetofCaches( iL1LineSize, iL1Ways, iL1SizeBytes/(iL1LineSize*iL1Ways), iL1Policy,
                                         iL2LineSize, iL2Ways, iL2SizeBytes/(iL2LineSize*iL2Ways), iL2Policy,
                                         iTLBSize
                                      );
    if ( NULL == pSC )
        return;
    if ( ! ( pSC->AreCachesValid() && pSC->IsTLB() ) )
    {
        delete pSC;
        return;
    }

    // ***************************************************************************************************
    // step through each policy
    for( int iRP = 0; iRP < ciNPolicies; iRP++ )
    {
        pSC->ClearCaches();
        switch ( ciCacheLevel )
        {
        case 1:
                pSC->SetPolicy( iL1Policy | (iRP*eReplacePLRU), iL2Policy );
                break;
        case 2:
                pSC->SetPolicy( iL1Policy, iL2Policy | (iRP*eReplacePLRU) );
                break;
        default:
                break;
        }
        // step through the pattern a number of times counted by iPhase
        int eType = (eWriteLoop1D==m_eTL || eWriteLoop2D==m_eTL || eMemCpyLoop==m_eTL ) ? eWrite : eRead;
        const int ciTopPhase = (eReadLoop2D==m_eTL || eWriteLoop2D==m_eTL || eReadLoop3D==m_eTL) ? 15 : 1;
        for( int iPhase = 0; iPhase <= ciTopPhase; iPhase++ )
        {
            int iOffset;
            for( iOffset = iFirstOffset(); iOffset>=0; iOffset = iNextOffset() )
            {
                pSC->CacheAccess( eType, iOffset*sizeof(DWORD), 1 );
            }
            if ( 0 == iPhase )
            {   // don't include the first pass which happens in the preload phase of the real test
                pSC->ClearCounts();
                pSC->ClearSetCounts();
            }
        }
        int iL1Hits=0, iL2Hits=0, iMBHits=0;
        pSC->AccessInfo( &iL1Hits, &iL2Hits, &iMBHits, NULL, NULL );
        switch ( ciCacheLevel )
        {
        case 1:
                padHitRatio[iRP] = ((double)iL1Hits)/(iL1Hits+iL2Hits+iMBHits);
                break;
        case 2:
                padHitRatio[iRP] = ((double)iL2Hits)/(iL1Hits+iL2Hits+iMBHits);
                break;
        default:
                padHitRatio[iRP] = 0;
                break;
        }
    }
    delete pSC;
}


// ***************************************************************************************************************
// return an offset to the first DWORD in the pattern - used by the cache simulation
// ***************************************************************************************************************

int tStridePattern::iFirstOffset()
{
    m_iX = m_iY = 0;
    m_iZ = -1;
    m_iROFlags = eL1;
    return 0;
}


// ***************************************************************************************************************
// return the next offset in the pattern - used by the cache simulation of the pattern
// note doing this as a separate iterator is not particularly efficient but it decouples the pattern from the 
// collecting of information about the pattern which happens in the caller and greatly simplifies the caller
// ***************************************************************************************************************

int tStridePattern::iNextOffset()
{
    int iScamble;
    m_iROFlags = 0;
    switch ( m_eTL )
    {
    case eReadLoop1D: case eWriteLoop1D:  case eMemCpyLoop:
        m_iY++;
        if ( m_iY < m_iYStrides )
            return m_iY*m_iYdwStride;
        m_iROFlags = eL1;
        return -1;
    case eReadLoop2D: case eWriteLoop2D:
        m_iX++;
        if ( m_iX >= m_iXStrides )
        {
            m_iX = 0;
            m_iY++;
            m_iROFlags = eL1;
        }
        if ( m_iY < m_iYStrides )
            return m_iY*m_iYdwStride + m_iX*m_iXdwStride;
        m_iROFlags = eL1 | eL2;
        return -1;
    case eReadLoop3D:
        m_iX++;
        if ( m_iX >= m_iXStrides )
        {
            m_iX = 0;
            m_iY++;
            m_iROFlags = eL1;
        }
        if ( m_iZ < 0 )
        {
            // this for for the preliminary phase
            if ( m_iY < m_iYStrides )
                return m_iY*m_iYdwStride + m_iX*m_iXdwStride;
            m_iX = m_iY = m_iZ = 0;
            m_iROFlags = eL1 | eL2;
        }
        // rest of phases are somewhat strange to show difference between PLRU and per Set Round Robin
        if ( m_iY >= m_iYStrides )
        {
            m_iY = 0;
            m_iZ++;
            if ( m_iZ >= m_iZStrides )
            {
                m_iROFlags = eL1 | eL2 | eL3;
                return -1;
            }
            m_iROFlags = eL1 | eL2;
        }
        if ( m_iY == (m_iYStrides-1) )
        {
            return ((m_iZ+m_iYStrides) * m_iZdwStride) + (m_iX * m_iXdwStride);
        }
        iScamble = (4==m_iYStrides) ? 0x3120 : ((2==m_iYStrides) ? 0x0 : 0x73516240);
        return (m_iYdwStride * ( 0xF & (iScamble>>(4*m_iY)))) + (m_iX * m_iXdwStride);
    default:
        assert(0);
        return -1;
    }
}
