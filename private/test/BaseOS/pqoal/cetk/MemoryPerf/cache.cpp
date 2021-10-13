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
// These functions implement the behavior of class Cache and SetofCaches
// which provide a simulation of the cache behavior which is used to improve the readability of some of the charts
// as well as to verify some aspects of the cache's behavior
//
// ***************************************************************************************************************


#include "cache.h"

const int caiPLRUMask[4] = { 1, 1, 2, 2 };
const int caiPLRUUpdate[4] = { 6, 4, 1, 0 };
const int caiPLRUWay[8] = { 0, 0, 1, 1, 2, 3, 2, 3 };


// ***************************************************************************************************************
// constructor
// ***************************************************************************************************************

Cache::Cache( int iLineSize, int iWays, int iSets, int iPolicy )
{   // constructor for cache object
    if ( !( ( 0 < iLineSize && iLineSize <= 1024 ) &&
            ( 0 < iWays && iWays <= 64 ) &&
            ( 0 < iSets && iSets <= 4096 ) &&
            ( 0==(iPolicy&eReplacePLRU) || 4==iWays ) ) )
    {
        assert( 0 < iLineSize && iLineSize <= 1024 );
        assert( 0 < iWays && iWays <= 64 );
        assert( 0 < iSets && iSets <= 4096 );
        if ( !( 0 < iSets && iSets <= 4096 ) )
        {
            NKDbgPrintfW( L"iLineSize=%d, iWays=%d, iSets=%d\r\n", iLineSize, iWays, iSets );
        }
        assert( 0==(iPolicy&eReplacePLRU) || 4==iWays );
        m_iLineSize = 0;
        m_ptuCache = NULL;
        return;
    }
    m_iLineSize = iLineSize;
    m_iWays     = iWays;
    m_iSets     = iSets;
    m_iPolicy   = iPolicy;
    m_iSetMask  = m_iSets-1;
    m_ptuCache = new tuCacheInfo[ iWays*iSets ];
    for( m_iLineShift=0; m_iLineShift<31; m_iLineShift++ )
    {
        if ( (1<<m_iLineShift) >= iLineSize )
            break;
    }
    m_iReplaceWay = 0;
    if ( m_iSets > 1 )
    {
        m_pdwAccessSetCount  = new DWORD[ iSets ];
        m_pdwReplaceSetCount = new DWORD[ iSets ];
    }
    else
    {
        m_pdwAccessSetCount  = NULL;
        m_pdwReplaceSetCount = NULL;
    }
    if ( iWays > 1 )
    {
        m_pdwReplaceWayCount = new DWORD[ iWays ];
    }
    else
    {
        m_pdwReplaceWayCount = NULL;
    }
    ClearCache();
}


// ***************************************************************************************************************
// destructor
// ***************************************************************************************************************

Cache::~Cache()
{
    delete [] m_ptuCache;
    m_ptuCache = NULL;
    delete [] m_pdwAccessSetCount;
    delete [] m_pdwReplaceSetCount;
    delete [] m_pdwReplaceWayCount;
}

// ***************************************************************************************************************
// clearing the cache so it starts in a known clean state
// ***************************************************************************************************************

void Cache::ClearCache()
{
    if ( NULL != m_ptuCache )
    {
        // init cache info so nothing will match and eReplacePLRU is zeroed
        tuCacheInfo tuInit;
        tuInit.tCI.uiPLRU = 0;
        tuInit.tCI.uiAddr = 0x0FFFFFFF;
        for( int i = 0; i < (m_iWays*m_iSets); i++ )
        {
            m_ptuCache[i].dwCI = tuInit.dwCI;
        }
    }
    ClearCounts();
    ClearSetCounts();
}


// ***************************************************************************************************************
// return 0 if hits, else 1 if misses but is replaced
// This function is generally called for L2 cache
// ***************************************************************************************************************

int Cache::Access( const int eType, const DWORD dwAddress )
{
    int iMatch = dwAddress >> m_iLineShift;
    int idxSet = iMatch & m_iSetMask;
    if ( m_iLineShift>12 )
    {
        assert( m_iLineShift <= 12 );
        iMatch |= 0x10000000;   // mark this can only match a 1MB section "page"
    }
    m_dwAccesses++;
    for( int i = 0; i < m_iWays; i++ )
    {
        if ( (int)m_ptuCache[ idxSet*m_iWays + i ].tCI.uiAddr == iMatch )
        {
            if ( NULL != m_pdwAccessSetCount )
            {
                m_pdwAccessSetCount[idxSet]++;
            }
            if ( eReplacePLRU & m_iPolicy ) 
            {   
                // update the PLRU bits which are stored in way 0
                m_ptuCache[ idxSet*m_iWays ].tCI.uiPLRU = (m_ptuCache[ idxSet*m_iWays ].tCI.uiPLRU & caiPLRUMask[i]) | caiPLRUUpdate[i];
            }
            return 0;
        }
    }
    m_dwMisses++;
    if ( eWrite != eType || (m_iPolicy & eWriteMissAllocate) )
    {
        // allocate on read or instruction miss or on a write-miss if the cache policy allows
        ReplacementCountIncrement();
        if ( eReplacePLRU & m_iPolicy )
        {
            int iPLRU = m_ptuCache[ idxSet*m_iWays ].tCI.uiPLRU;
            int iReplaceWay = caiPLRUWay[iPLRU];
            iPLRU = (iPLRU & caiPLRUMask[iReplaceWay]) | caiPLRUUpdate[iReplaceWay];
            if ( 0==iReplaceWay )
            {
                tuCacheInfo tuCI;
                tuCI.tCI.uiPLRU = iPLRU;
                tuCI.tCI.uiAddr = iMatch;
                m_ptuCache[ idxSet*m_iWays ].dwCI = tuCI.dwCI;

            }
            else
            {
                m_ptuCache[ idxSet*m_iWays ].tCI.uiPLRU = iPLRU;
                m_ptuCache[ idxSet*m_iWays + iReplaceWay ].tCI.uiAddr = iMatch;
            }
        }
        else if ( ePerSetRoundRobin & m_iPolicy )
        {
            int iReplaceWay = m_ptuCache[ idxSet*m_iWays ].tCI.uiPLRU;
            m_ptuCache[ idxSet*m_iWays + iReplaceWay ].dwCI = iMatch;
            m_ptuCache[ idxSet*m_iWays ].tCI.uiPLRU = ((iReplaceWay+1)%m_iWays);
        }
        else
        {
            m_ptuCache[ idxSet*m_iWays + m_iReplaceWay ].dwCI = iMatch;
        }
        if ( NULL != m_pdwAccessSetCount )
        {
            m_pdwAccessSetCount[idxSet]++;
        }
        if ( NULL != m_pdwReplaceSetCount )
        {
            m_pdwReplaceSetCount[idxSet]++;
        }
        if ( NULL != m_pdwReplaceWayCount )
        {
            m_pdwReplaceWayCount[m_iReplaceWay]++;
        }
        if ( (++m_iReplaceWay) >= m_iWays )
            m_iReplaceWay = 0;
    }
    return 1;
}


// ***************************************************************************************************************
// return 0 if hits, else 1 if misses but is replaced
// This function is generally called for L1 cache
// ***************************************************************************************************************

int Cache::Access(  const int eType, const DWORD dwAddress, const DWORD dwDWCount, Cache * pcL2, int * piMiss2 )
{
    int iMisses = 0;
    DWORD dwCnt = dwDWCount;
    DWORD dwAdr = dwAddress;
    m_dwAccesses += dwDWCount;
    while ( dwCnt > 0 )
    {
        int iMatch = dwAdr >> m_iLineShift;
        int idxSet = iMatch & m_iSetMask;
        if ( m_iLineShift>=16 )
        {
            iMatch |= (m_iLineShift-15)<<28;   // mark this can only match a 1MB section "page"
        }
        int idxWay;
        for( idxWay = 0; idxWay < m_iWays; idxWay++ )
        {
            if ( (int)m_ptuCache[ idxSet*m_iWays + idxWay ].tCI.uiAddr == iMatch )
            {
                if ( NULL != m_pdwAccessSetCount )
                {
                    m_pdwAccessSetCount[idxSet]++;
                }
                if ( eReplacePLRU & m_iPolicy ) 
                {   
                    // update the PLRU bits which are stored in way 0
                    m_ptuCache[ idxSet*m_iWays ].tCI.uiPLRU = (m_ptuCache[ idxSet*m_iWays ].tCI.uiPLRU & caiPLRUMask[idxWay]) | caiPLRUUpdate[idxWay];
                }
                break;
            }
        }
        if ( idxWay >= m_iWays )
        {
            iMisses++;
            m_dwMisses++;
            if ( eWrite != eType || (m_iPolicy & eWriteMissAllocate) )
            {
                ReplacementCountIncrement();

                if ( eReplacePLRU & m_iPolicy )
                {
                    int iPLRU = m_ptuCache[ idxSet*m_iWays ].tCI.uiPLRU;
                    int iReplaceWay = caiPLRUWay[iPLRU];
                    iPLRU = (iPLRU & caiPLRUMask[iReplaceWay]) | caiPLRUUpdate[iReplaceWay];
                    if ( 0==iReplaceWay )
                    {
                        tuCacheInfo tuCI;
                        tuCI.tCI.uiPLRU = iPLRU;
                        tuCI.tCI.uiAddr = iMatch;
                        m_ptuCache[ idxSet*m_iWays ].dwCI = tuCI.dwCI;

                    }
                    else
                    {
                        m_ptuCache[ idxSet*m_iWays ].tCI.uiPLRU = iPLRU;
                        m_ptuCache[ idxSet*m_iWays + iReplaceWay ].tCI.uiAddr = iMatch;
                    }
                }
                else if ( ePerSetRoundRobin & m_iPolicy )
                {
                    int iReplaceWay = m_ptuCache[ idxSet*m_iWays ].tCI.uiPLRU;
                    m_ptuCache[ idxSet*m_iWays + iReplaceWay ].dwCI = iMatch;
                    m_ptuCache[ idxSet*m_iWays ].tCI.uiPLRU = ((iReplaceWay+1)%m_iWays);
                }
                else
                {
                    m_ptuCache[ idxSet*m_iWays + m_iReplaceWay ].dwCI = iMatch;
                }

                if ( NULL != m_pdwAccessSetCount )
                {
                    m_pdwAccessSetCount[idxSet]++;
                }
                if ( NULL != m_pdwReplaceSetCount )
                {
                    m_pdwReplaceSetCount[idxSet]++;
                }
                if ( NULL != m_pdwReplaceWayCount )
                {
                    m_pdwReplaceWayCount[m_iReplaceWay]++;
                }
                if ( (++m_iReplaceWay) >= m_iWays )
                    m_iReplaceWay = 0;
            }
            if ( NULL != pcL2 )
            {
                int iMiss2 = pcL2->Access( eType, dwAdr, 1, NULL, NULL );
                if ( NULL != piMiss2 ) 
                {
                    *piMiss2 = iMiss2;
                }
            }
        }
        // remaining access of this cache line are hits.
        do {
            dwAdr += 4;
            dwCnt--;
        } while ( dwCnt > 0 && (int)( dwAdr >> m_iLineShift ) == (iMatch&0x0FFFFFFF) );
    }
    return iMisses;
}


// ***************************************************************************************************************
// Separate out into two parts for TLB caches
// return 0 if hits, else 1 if misses -- no replacement is done
// ignore PLRU policy for the TLB for now.
// ***************************************************************************************************************

int Cache::MissTest( const DWORD dwAddress, const DWORD dwPageBits )
{
    int iMatch = dwAddress >> dwPageBits;
    int idxSet = iMatch & m_iSetMask;
    if ( dwPageBits>=16 )
    {
        iMatch |= (dwPageBits-15)<<28;   // mark this can only match a 1MB section "page"
    }
    for( int i = 0; i < m_iWays; i++ )
    {
        if ( (int)m_ptuCache[ idxSet*m_iWays + i ].dwCI == iMatch )
        {
            if ( NULL != m_pdwAccessSetCount )
            {
                m_pdwAccessSetCount[idxSet]++;
            }
            return 0;
        }
    }
    m_dwMisses++;
    return 1;
}


// ***************************************************************************************************************
// replace a cache line with the missed address
// ***************************************************************************************************************

void Cache::Replace( const DWORD dwAddress, const DWORD dwPageBits )
{
    int iMatch = dwAddress >> dwPageBits;
    int idxSet = iMatch & m_iSetMask;
    if ( dwPageBits>=16 )
    {
        iMatch |= (dwPageBits-15)<<28;   // mark this can only match a 1MB section "page"
    }
    ReplacementCountIncrement();
    m_ptuCache[ idxSet*m_iWays + m_iReplaceWay ].dwCI = iMatch;
    if ( NULL != m_pdwAccessSetCount )
    {
        m_pdwAccessSetCount[idxSet]++;
    }
    if ( NULL != m_pdwReplaceSetCount )
    {
        m_pdwReplaceSetCount[idxSet]++;
    }
    if ( NULL != m_pdwReplaceWayCount )
    {
        m_pdwReplaceWayCount[m_iReplaceWay]++;
    }
    if ( (++m_iReplaceWay) >= m_iWays )
        m_iReplaceWay = 0;
}


// ***************************************************************************************************************
// constructor for the set of caches needed to simulate this device
// ***************************************************************************************************************

SetofCaches::SetofCaches(
                          const int ciL1LineSize, 
                          const int ciL1Ways, 
                          const int ciL1Sets, 
                          const int ciL1Policy,
                          const int ciL2LineSize, 
                          const int ciL2Ways, 
                          const int ciL2Sets, 
                          const int ciL2Policy,
                          const int ciTLBSize
                         )
{
    m_pcL1D = m_pcL2C = NULL;
    m_iAreCachesValid = -1;
    m_pcTLB1D = m_pcTLB2C = m_pcTLB3C = NULL;
    m_eTLBStyle = eTLBUnknown;
    m_iTLB2Replace = 0;
    m_dwCountWalkSpansPages = 0;
    m_dwMaxBlockSize = 0;
    if ( (ciL1Ways>0 && ciL1Sets>0) && (ciL2Ways>0 && ciL2Sets>0) )
    {
        m_pcL1D = new Cache( ciL1LineSize, ciL1Ways, ciL1Sets, ciL1Policy );
        m_pcL2C = new Cache( ciL2LineSize, ciL2Ways, ciL2Sets, ciL2Policy );
        if ( !AreCachesValid() )
        {
            assert( AreCachesValid() );     // something is wrong if we can't allocate
            return;
        }
    }
    m_eTLBStyle = eTLBUnknown;
    if ( 0 == (ciTLBSize%32) )
    {   // assume a single level fully associative cache is used for the TLB.
        // for our purposes we can ignore if there is, as well, a smaller cache closer to the core
        m_pcTLB1D = new Cache( 4, ciTLBSize, 1, eWriteMissAllocate );
        if ( NULL != m_pcTLB1D && m_pcTLB1D->IsValid() )
        {
            m_eTLBStyle = eTLBFlat;
        }
    }
    else
    {   // assume two level cache is used like described for the ARM11
        m_pcTLB1D = new Cache( 4,  8,  1, eWriteMissAllocate );
        m_pcTLB2C = new Cache( 4, (ciTLBSize%32),  1, eWriteMissAllocate );
        m_pcTLB3C = new Cache( 4,  2, ciTLBSize>32 ? (ciTLBSize-(ciTLBSize%32))/2 : 8, eWriteMissAllocate );
        if ( NULL != m_pcTLB1D && m_pcTLB1D->IsValid() && 
             NULL != m_pcTLB2C && m_pcTLB2C->IsValid() &&
             NULL != m_pcTLB3C && m_pcTLB3C->IsValid() )
        {
            m_eTLBStyle = eTLBARM11;
        }
    }
    if ( !IsTLB() )
    {
        assert( IsTLB() );      // something is wrong if we can't allocate
    }
}


// ***************************************************************************************************************
// destructor
// ***************************************************************************************************************

SetofCaches::~SetofCaches()
{
    delete m_pcL1D;   m_pcL1D = NULL;
    delete m_pcL2C;   m_pcL2C = NULL;
    m_iAreCachesValid = -1;
    delete m_pcTLB1D; m_pcTLB1D = NULL;
    delete m_pcTLB2C; m_pcTLB2C = NULL;
    delete m_pcTLB3C; m_pcTLB3C = NULL;
    m_eTLBStyle = eTLBUnknown;
}


// ***************************************************************************************************************
// access the caches and TLB ignoring the various return values
// ***************************************************************************************************************

void SetofCaches::CacheAccess(const int eType, const DWORD dwAddress, const DWORD dwDWCount )
{
    // only interested in summary statistics.
    int iL1MissCount, iL2MissCount, iPageWalkCount, iRASChanges;
    CacheAccess( eType, dwAddress, dwDWCount, &iL1MissCount, &iL2MissCount, &iPageWalkCount, &iRASChanges );
}


// ***************************************************************************************************************
// access the caches and TLB returning misses and walks
// ***************************************************************************************************************

void SetofCaches::CacheAccess( const int eType, const DWORD dwAddress, const DWORD dwDWCount, int *piL1MissCount, int *piL2MissCount, int *piPageWalkCount, int *piRASChanges )
{
    assert( !(NULL == piL1MissCount || NULL==piL2MissCount) );
    assert( eRead==eType || eWrite==eType );
    if ( NULL == piL1MissCount || NULL==piL2MissCount )
        return;
    if ( 0 == dwDWCount )
        return;
    *piL1MissCount = 0;
    *piL2MissCount = 0;
    *piRASChanges = 0;
    if ( AreCachesValid() )
    {
        int iL1Miss = m_pcL1D->Access( eType, dwAddress, dwDWCount, m_pcL2C, piL2MissCount );
        *piL1MissCount = iL1Miss;
        if ( (*piL2MissCount) > 0 )
        {
            if ( m_pcL2C->bRASChangeAdd( dwAddress, 1 ) )
            {
                *piRASChanges = 1;
            }
        }
    }
    WalkTLB( eType, dwAddress, dwDWCount, piPageWalkCount );

}


// ***************************************************************************************************************
// simulate accessing the TLB
// ***************************************************************************************************************

void SetofCaches::WalkTLB( const int eType, const DWORD dwAddress, const DWORD dwDWCount, int *piPageWalkCount )
{
    assert( !(NULL == piPageWalkCount) );
    assert( eRead==eType || eWrite==eType );
    if ( NULL == piPageWalkCount )
        return;
    *piPageWalkCount = 0;
    if ( IsTLB() )
    {
        DWORD dwAdr = dwAddress;
        DWORD dwCnt = dwDWCount;
        int dwPageBits = (0x80000000 <= dwAddress && dwAddress < 0xC0000000) ? 20 : 12;
        DWORD dwPageMask = (1<<dwPageBits)-1;
        if ( m_dwMaxBlockSize < dwCnt )
        {
            m_dwMaxBlockSize = dwCnt;
        }
        while (dwCnt>0) 
        {
            DWORD dwNextPage = (dwAdr + (1<<dwPageBits)) & ~dwPageMask;
            DWORD dwThisCount = (dwNextPage-dwAdr) >= dwCnt ? dwCnt : (dwNextPage-dwAdr);
            switch( m_eTLBStyle )
            {
            case eTLBARM11:
                    m_pcTLB1D->AccessAdd( dwThisCount );
                    if ( m_pcTLB1D->MissTest( dwAddress, dwPageBits ) )
                    {
                        m_pcTLB1D->Replace( dwAddress, dwPageBits );       // always replace in level 1
                        m_pcTLB2C->AccessAdd( 1 );                      // follow tlbplotter's logical convention of only counting one access here
                        if ( m_pcTLB2C->MissTest( dwAddress, dwPageBits ) )
                        {
                            m_pcTLB3C->AccessAdd( 1 );
                            if ( m_pcTLB3C->MissTest( dwAddress, dwPageBits ) )
                            {
                                (*piPageWalkCount)++;
                                if ( ++m_iTLB2Replace >= 9 )
                                {
                                    m_iTLB2Replace = 0;
                                    m_pcTLB2C->Replace( dwAddress, dwPageBits );
                                }
                                else
                                {
                                    m_pcTLB3C->Replace( dwAddress, dwPageBits );
                                }
                            }
                        }                    
                    }
                    break;
            case eTLBFlat:
                    m_pcTLB1D->AccessAdd( dwThisCount );
                    if ( m_pcTLB1D->MissTest( dwAddress, dwPageBits ) )
                    {
                        (*piPageWalkCount)++;
                        m_pcTLB1D->Replace( dwAddress, dwPageBits );
                    }
                    // note that typical implementation have separate small micro-TLB's.
                    // The instruction micro-TLB should keep the unified TLB from needing entries for instructions
                    // so ignore instruction's use of the TLB.
                    break;
            default:
                    break;
            }
            if ( (dwNextPage-dwAdr) > dwCnt )
            {
                break;
            }
            m_dwCountWalkSpansPages++;
            dwCnt -= dwThisCount;
            dwAdr = dwNextPage;
        }
    }
}


// ***************************************************************************************************************
// print cache info to the CSV file
// ***************************************************************************************************************

void Cache::CSVCacheInfo( const char * str )
{
    int i;
    CSVPrintf( "%s Linesize= %d, %d ways, %3d sets, Size= %3d KB Accesses %d Misses %d Replacements %d", 
        str, m_iLineSize, m_iWays, m_iSets, m_iLineSize*m_iWays*m_iSets/1024, m_dwAccesses, m_dwMisses, m_dwReplacements );
    if ( m_dwRASChanges > 0 )
        CSVPrintf( " RASChanges %d", m_dwRASChanges );
    CSVPrintf("\r\n" );
    if ( NULL != m_pdwReplaceWayCount )
    {
        CSVPrintf( "%s ReplaceWays", str );
        for( i = 0; i < m_iWays; i++ )
        {
            CSVPrintf( " %u%", m_pdwReplaceWayCount[i] );
            while( (i+1)<m_iWays && m_pdwReplaceWayCount[i]==m_pdwReplaceWayCount[i+1] )
            {
                CSVPrintf( ".");
                i++;
            }
        }
        CSVPrintf( "\r\n" );
    }
    if ( NULL != m_pdwAccessSetCount )
    {
        int iSetsUsed = 0;
        for( i = 1; i < m_iSets; i++ )
        {
            if ( m_pdwAccessSetCount[i]>0 )
                iSetsUsed++;
        }
        CSVPrintf( "%s, Sets Used, %6.2f%%, Accessed", str, (100.0*iSetsUsed)/m_iSets );
        int iStartSame = 0;
        for( i = 1; i < m_iSets; i++ )
        {
            if ( m_pdwAccessSetCount[i] != m_pdwAccessSetCount[iStartSame] || (i+1)==m_iSets )
            {
                if ( iStartSame < (i-1) )
                {
                    CSVPrintf( ", [%d..%d]%d", iStartSame, i-1, m_pdwAccessSetCount[iStartSame] );
                }
                else
                {
                    CSVPrintf( ", [%d]%d", iStartSame, m_pdwAccessSetCount[iStartSame] );
                }
                iStartSame = i;
            }
        }
        CSVPrintf( "\r\n" );
    }
    if ( NULL != m_pdwReplaceSetCount )
    {
        CSVPrintf( "%s ReplaceSets", str );
        int iStartSame = 0;
        for( i = 1; i < m_iSets; i++ )
        {
            if ( m_pdwReplaceSetCount[i] != m_pdwReplaceSetCount[iStartSame] || (i+1)==m_iSets )
            {
                if ( iStartSame < (i-1) )
                {
                    CSVPrintf( ", [%d..%d]%d", iStartSame, i-1, m_pdwReplaceSetCount[iStartSame] );
                }
                else
                {
                    CSVPrintf( ", [%d]%d", iStartSame, m_pdwReplaceSetCount[iStartSame] );
                }
                iStartSame = i;
            }
        }
        CSVPrintf( "\r\n" );
    }
}


// ***************************************************************************************************************
// qsort comparison function for sorting values and determining quantiles
// ***************************************************************************************************************

static
int Compare4Qunatile( const void *arg1, const void *arg2 )
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
// show quantile info in the CSV file
// ***************************************************************************************************************

void CSVQuantile( const int ciTiles, 
                  const DWORD * pdwCounts, 
                  const int ciSize, 
                  const char * pstrName, 
                  const char * pstrType, 
                  const int ciRepeats 
                 )
{   // min, q1, q2, ... q(ciTitle), max
    if ( ciTiles <= 1 || ciSize <= 1 || ciSize>(INT_MAX/sizeof(DWORD)) || ciRepeats <= 0 )
        return;
    DWORD * pdwSort = new DWORD[ciSize];
    if ( NULL == pdwSort )
        return;
    memcpy( pdwSort, pdwCounts, ciSize*sizeof(DWORD) );
    qsort( pdwSort, ciSize, sizeof(DWORD), Compare4Qunatile );
    int iTotal = 0, iUnUsed = 0;
    int i;
    for( i=0; i<ciSize; i++ )
    {
        iTotal += pdwSort[i];
        if ( 0 == pdwSort[i] )
            iUnUsed++;
    }
    if ( iTotal > 0 )
    {
        float fPercent = 100.0f/iTotal;
        int iTiles = min( ciTiles, ciSize );
        CSVPrintf( "%s, %s, unused, %.2f%%, min, %d, dist", 
                    pstrName, 
                    pstrType, 
                    (100.0f*iUnUsed)/ciSize, 
                    pdwSort[0]/ciRepeats 
                  );
        int iIdx = 0;
        for( i = 0; i<iTiles; i++ )
        {
            int iTileTop = (ciSize*(i+1)+(iTiles/2))/iTiles;
            if ( iTileTop >= ciSize )
                iTileTop = ciSize;
            int iSum = 0;
            for( ; iIdx < iTileTop; iIdx++ )
                iSum += pdwSort[iIdx];
            CSVPrintf( ", %.2f%%", fPercent*iSum );
        }
        CSVPrintf( ", max, %d\r\n", pdwSort[ciSize-1]/ciRepeats );
    }
    delete [] pdwSort;
}


// ***************************************************************************************************************
// show cache-set accesses per quantile
// ***************************************************************************************************************

void Cache::CSVSetCounts( const char * str, const int ciRepeats )
{
    if ( NULL != m_pdwAccessSetCount )
    {
        CSVQuantile( 5, m_pdwAccessSetCount, m_iSets, "Set Access", str, ciRepeats );
    }
    if ( NULL != m_pdwReplaceSetCount )
    {
        CSVQuantile( 5, m_pdwReplaceSetCount, m_iSets, "Set Replace", str, ciRepeats );
    }
}


// ***************************************************************************************************************
// clear all the caches
// ***************************************************************************************************************

void SetofCaches::ClearCaches()
{
    if ( NULL != m_pcL1D ) m_pcL1D->ClearCache();
    if ( NULL != m_pcL2C ) m_pcL2C->ClearCache();
    if ( NULL != m_pcTLB1D ) m_pcTLB1D->ClearCache();
    if ( NULL != m_pcTLB2C ) m_pcTLB2C->ClearCache();
    if ( NULL != m_pcTLB3C ) m_pcTLB3C->ClearCache();
}


// ***************************************************************************************************************
// show all the cache info
// ***************************************************************************************************************

void SetofCaches::CSVCacheInfo()
{
    if ( NULL != m_pcL1D ) m_pcL1D->CSVCacheInfo( "L1D" );
    if ( NULL != m_pcL2C ) m_pcL2C->CSVCacheInfo( "L2C" );
    if ( NULL != m_pcTLB1D ) m_pcTLB1D->CSVCacheInfo( "TLB1D" );
    if ( NULL != m_pcTLB2C ) m_pcTLB2C->CSVCacheInfo( "TLB2C" );
    if ( NULL != m_pcTLB3C ) m_pcTLB3C->CSVCacheInfo( "TLB3C" );
}


// ***************************************************************************************************************
// show all the set statistics for all the caches
// ***************************************************************************************************************

void SetofCaches::CSVSetCounts( const int ciFlags, const int ciRepeats )
{
    if ( NULL != m_pcL1D   && 1&ciFlags ) m_pcL1D->CSVSetCounts( "L1D", ciRepeats );
    if ( NULL != m_pcL2C   && 2&ciFlags ) m_pcL2C->CSVSetCounts( "L2C", ciRepeats );
    if ( NULL != m_pcTLB1D && 4&ciFlags ) m_pcTLB1D->CSVSetCounts( "TLB1D", ciRepeats );
    if ( NULL != m_pcTLB2C && 4&ciFlags ) m_pcTLB2C->CSVSetCounts( "TLB2C", ciRepeats );
    if ( NULL != m_pcTLB3C && 4&ciFlags ) m_pcTLB3C->CSVSetCounts( "TLB3C", ciRepeats );
}


// ***************************************************************************************************************
// return the current hit and walk statistics for the cache simulation
// ***************************************************************************************************************

void SetofCaches::AccessInfo( int* piL1Hits, int* piL2Hits, int* piMBHits, int* piPageWalks, int* piRASChanges ) const
{
    if ( NULL != piL1Hits ) 
        *piL1Hits = 0;
    if ( NULL != piL2Hits ) 
        *piL2Hits = 0;
    if ( NULL != piMBHits ) 
        *piMBHits = 0;
    if ( NULL != piPageWalks ) 
        *piPageWalks = 0;
    if ( NULL != piRASChanges ) 
        *piRASChanges = 0;
    if ( NULL != m_pcL1D ) 
    {
        if ( NULL != piL1Hits )
            *piL1Hits = m_pcL1D->AccessCount() - m_pcL1D->MissesCount();
        else if ( NULL == m_pcL2C )
        {
            if ( NULL != piMBHits ) 
                *piMBHits = m_pcL1D->MissesCount();
            if ( NULL != piRASChanges ) 
                *piRASChanges = m_pcL1D->RASChangeCount();
        }
    }
    if ( NULL != m_pcL2C ) 
    {
        if ( NULL != piL2Hits ) 
            *piL2Hits = m_pcL2C->AccessCount() - m_pcL2C->MissesCount();
        if ( NULL != piMBHits ) 
            *piMBHits = m_pcL2C->MissesCount();
        if ( NULL != piRASChanges ) 
            *piRASChanges = m_pcL2C->RASChangeCount();
    }
    if ( NULL != m_pcTLB3C )
    {
        if ( NULL != piPageWalks ) 
            *piPageWalks = m_pcTLB3C->MissesCount();
    }
    else if ( NULL != m_pcTLB1D )
    {
        if ( NULL != piPageWalks ) 
            *piPageWalks = m_pcTLB1D->MissesCount();
    }
}


// ***************************************************************************************************************
// clear the cache state
// ***************************************************************************************************************

void Cache::ClearCounts()
{ 
    m_dwAccesses = m_dwMisses = m_dwReplacements = 0; 
    m_dwRASChanges = 0;
    m_dwPriorAddress = 0xFFFFFFFF;
}


// ***************************************************************************************************************
// clear all the caches 
// ***************************************************************************************************************

void SetofCaches::ClearCounts()
{
    if ( NULL != m_pcL1D ) m_pcL1D->ClearCounts();
    if ( NULL != m_pcL2C ) m_pcL2C->ClearCounts();
    if ( NULL != m_pcTLB1D ) m_pcTLB1D->ClearCounts();
    if ( NULL != m_pcTLB2C ) m_pcTLB2C->ClearCounts();
    if ( NULL != m_pcTLB3C ) m_pcTLB3C->ClearCounts();
}


// ***************************************************************************************************************
// clear this cache's Set and Way counts
// ***************************************************************************************************************

void Cache::ClearSetCounts()
{ 
    if ( NULL != m_pdwAccessSetCount )
    {
        memset( m_pdwAccessSetCount, 0, sizeof(DWORD)*m_iSets );
    }
    if ( NULL != m_pdwReplaceSetCount )
    {
        memset( m_pdwReplaceSetCount, 0, sizeof(DWORD)*m_iSets );
    }
    if ( NULL != m_pdwReplaceWayCount )
    {
        memset( m_pdwReplaceWayCount, 0, sizeof(DWORD)*m_iWays );
    }
}


// ***************************************************************************************************************
// clear all the caches set counts
// ***************************************************************************************************************

void SetofCaches::ClearSetCounts()
{
    if ( NULL != m_pcL1D ) m_pcL1D->ClearSetCounts();
    if ( NULL != m_pcL2C ) m_pcL2C->ClearSetCounts();
    if ( NULL != m_pcTLB1D ) m_pcTLB1D->ClearSetCounts();
    if ( NULL != m_pcTLB2C ) m_pcTLB2C->ClearSetCounts();
    if ( NULL != m_pcTLB3C ) m_pcTLB3C->ClearSetCounts();
}


// ***************************************************************************************************************
// show the cache replacement policy
// ***************************************************************************************************************

const char * strCacheReplacementPolicy( int iPolicy )
{
    static const char * astrReplace[3] = { "GobalRoundRobin", "P-LRU", "PerSetRoundRobin" };
    return astrReplace[ (iPolicy/eReplacePLRU)%3 ];
}

