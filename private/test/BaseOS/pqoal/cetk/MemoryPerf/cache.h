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
// These classes, Cache and SetofCaches, provide a model of the set of caches used in a typical device
// and enable simulations to be compared to actual measurements as well as reference charts to be generated
//
// ***************************************************************************************************************

#include "EffectiveSize.h"

enum { eInst=1, eRead=2, eWrite=4, eCountTypes=3, eAllTypes=eInst|eRead|eWrite, eTotalTypes=eAllTypes+1 };
enum { eTLBUnknown=0, eTLBARM11, eTLBFlat };

// not eWriteBehind => Write-Through (but we don't actually model the difference)
// The difference between WriteMissNoAllocate and WriteMissAllocate is modeled
enum { eWriteThrough=0, eWriteBehind=2, eWriteMissNoAllocate=0, eWriteMissAllocate=4, eGlobalRoundRobin=0, eReplacePLRU=8, ePerSetRoundRobin=16 };     

typedef struct {
    unsigned int uiAddr : 28;
    unsigned int uiPLRU : 4;
} tCacheInfo;
typedef union {
    tCacheInfo tCI;
    DWORD dwCI;
} tuCacheInfo;

typedef struct {
    int ciL1ISize;
    int ciL1DSize;
    int ciL2CSize;
    int ciL1LineSize;
    int ciL2LineSize;
    int ciL1Ways;
    int ciL2Ways;
    int ciL1Policy;
    int ciL2Policy;
    int ceTLBType;
} tCacheSetup;


// ***************************************************************************************************************
// cache Class
// ***************************************************************************************************************

class Cache
{
public:
    Cache() : 
      m_iLineSize(0), 
          m_iWays(0), 
          m_iSets(0), 
          m_iSetMask(0), 
          m_ptuCache(NULL), 
          m_iLineShift(0), 
          m_iReplaceWay(0), 
          m_pdwAccessSetCount(NULL),
          m_pdwReplaceSetCount(NULL), 
          m_pdwReplaceWayCount(NULL),
          m_dwAccesses(0),
          m_dwMisses(0),
          m_dwReplacements(0)
      {};
    Cache( int iLineSize, int iWays, int iSets, int iPolicy );
    ~Cache();
    int IsValid() const { return NULL != m_ptuCache; };
    int Access(  const int eType, const DWORD dwAddress );
    int Access(  const int eType, const DWORD dwAddress, const DWORD dwDWCount, Cache * pcL2, int * piMiss2 );
    int MissTest( const DWORD dwAddress, const DWORD dwPageBits );
    void Replace( const DWORD dwAddress, const DWORD dwPageBits );
    void CSVCacheInfo( const char * str );
    DWORD AccessCount() const { return m_dwAccesses; }
    void AccessAdd( const DWORD dw ) { m_dwAccesses += dw; }
    DWORD MissesCount() const { return m_dwMisses; }
    void MissesAdd( const DWORD dw ) { m_dwMisses += dw; }
    DWORD ReplacementCount() const { return m_dwReplacements; }
    void ReplacementCountIncrement() { m_dwReplacements++; }
    DWORD RASChangeCount() const { return m_dwRASChanges; }
    bool bRASChangeAdd( const DWORD dwAddress, const DWORD dw ) 
    {   // assume RAS changes on 16KB boundaries which is a good assumption for 1Gb and 2Gb SDRAM (256MB) 
        // unless banks addresses are lower than row address 
        bool bRet = false;
        if ( (dwAddress&0xFFFFC000)!=(m_dwPriorAddress&0xFFFFC000)) 
        {
            m_dwRASChanges += dw; 
            bRet = true;
        }
        m_dwPriorAddress = dwAddress;
        return bRet;
    }
    void ClearCounts();
    void ClearSetCounts();
    void CSVSetCounts( const char * str, const int ciRepeats );
    void ClearCache();
    void SetPolicy( int iPolicy ) { m_iPolicy = iPolicy; };

private:
    int m_iLineSize;
    int m_iWays;
    int m_iSets;
    int m_iPolicy;
    int m_iSetMask;
    tuCacheInfo * m_ptuCache;
    int m_iLineShift;
    int m_iReplaceWay;
    DWORD * m_pdwAccessSetCount;
    DWORD * m_pdwReplaceSetCount;
    DWORD * m_pdwReplaceWayCount;
    DWORD m_dwAccesses;
    DWORD m_dwMisses;
    DWORD m_dwReplacements;
    DWORD m_dwRASChanges;
    DWORD m_dwPriorAddress;
};


// ***************************************************************************************************************
// SetofCaches class which includes L1 Data, L2 combined, and up to three caches used in the TLB.
// ***************************************************************************************************************

class SetofCaches
{
public:
    SetofCaches() : m_pcL1D(NULL), 
                    m_pcL2C(NULL), 
                    m_iAreCachesValid(-1),  
                    m_pcTLB1D(NULL), 
                    m_pcTLB2C(NULL), 
                    m_pcTLB3C(NULL), 
                    m_eTLBStyle(eTLBUnknown),
                    m_iTLB2Replace(0),
                    m_dwCountWalkSpansPages(0),
                    m_dwMaxBlockSize(0)
    {};
    SetofCaches( 
                  const int ciL1LineSize, 
                  const int ciL1Ways, 
                  const int ciL1Sets, 
                  const int ciL1Policy,
                  const int ciL2LineSize, 
                  const int ciL2Ways, 
                  const int ciL2Sets, 
                  const int ciL2Policy,
                  const int eTLBStyle
                  );
    ~SetofCaches();
    void CacheAccess(const int eType, const DWORD dwAddress, const DWORD dwDWCount, int *piL1MissCount, int *piL2MissCount, int *piPageWalkCount, int *piRASChanges );
    void CacheAccess(const int eType, const DWORD dwAddress, const DWORD dwDWCount );
    int AreCachesValid() {
        if ( m_iAreCachesValid<0 ) 
        {
            m_iAreCachesValid = NULL!=m_pcL1D && m_pcL1D->IsValid() && NULL!=m_pcL2C && m_pcL2C->IsValid() ;
        }
        return m_iAreCachesValid;
    };
    int IsTLB() const { return eTLBARM11==m_eTLBStyle || eTLBFlat==m_eTLBStyle; };
    void WalkTLB( const int eType, const DWORD dwAddress, const DWORD dwDWCount, int *piPageWalkCount );
    void CSVCacheInfo();
    void AccessInfo( int* piL1Hits, int* piL2Hits, int* piMBHits, int* piPageWalks, int* piRASChanges ) const;
    void ClearCounts();
    void ClearSetCounts();
    void CSVSetCounts( const int ciFlags, const int ciRepeats );
    void ClearCaches();
    void SetPolicy( int iL1Policy, int iL2Policy )
    {
        if ( NULL != m_pcL1D )
            m_pcL1D->SetPolicy( iL1Policy );
        if ( NULL != m_pcL2C )
            m_pcL2C->SetPolicy( iL2Policy );
    };

private:
    Cache *m_pcL1D;
    Cache *m_pcL2C;
    int m_iAreCachesValid;
    Cache *m_pcTLB1D;
    Cache *m_pcTLB2C;
    Cache *m_pcTLB3C;
    int m_eTLBStyle;
    int m_iTLB2Replace;
    DWORD m_dwCountWalkSpansPages;
    DWORD m_dwMaxBlockSize;
};


extern const char * strCacheReplacementPolicy( int iPolicy );
