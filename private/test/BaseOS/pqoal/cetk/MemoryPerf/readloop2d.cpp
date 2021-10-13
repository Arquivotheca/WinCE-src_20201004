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
// These functions provide a measureable 2-D read loop 
// the loop is unrolled in a variety of special cases to provide low overhead measurements.
//
// ***************************************************************************************************************


#include "memoryPerf.h"


// ******************************************
// optimize this function even when debugging 
#if defined(DEBUG) || defined(_DEBUG)
#pragma optimize("gty",on)
#endif


// ***************************************************************************************************************
// Main Read test function
// implement main test loops in a separate function (e.g. don't inline) so the compiler will do better register allocations
// ***************************************************************************************************************
//
// Warning - if you change this file, you must enable VERIFY_OPTIMIZED and run 
//   s memoryperfexe -d -c -o
// without getting a "Warning: iReadLoop2D mismatch" in the log file
//
// Do not check-in with VERIFY_OPTIMIZED defined.

int iReadLoop2D( const DWORD *pdwBase, const int ciRepeats,  const tStridePattern &ctStrideInfo )
{
    const int ciYStrideCount = ctStrideInfo.m_iYStrides;
    const int ciYdwStride    = ctStrideInfo.m_iYdwStride;
    const int ciXStrideCount = ctStrideInfo.m_iXStrides;
    const int ciXdwStride    = ctStrideInfo.m_iXdwStride;
    // interesting cases for cache line testing is 2==ciXStrideCount so special case that
    // use volatile below to keep compiler from optimizing the loop away and reading once followed by a multiply.

    switch( ciXStrideCount )
    {
    case 0:  // let this degenerate case get handled by the 1D case below            
        assert( ciXStrideCount > 0 );
        return 0;
    case 1:
        {   // Really the 1D case
            return iReadLoop1D( pdwBase, ciRepeats, ctStrideInfo );
        }
    case 2:
        {
            int iTotal = 0; 
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                // these pointers should be volatile but the current compiler adds three stack references per loop
                // which defeats the purpose of this function.
                // after building, examine the code in LibBld\Obj\Arm*\ReadLoop2D.asm
                const DWORD* pdwY = pdwBase;
                const DWORD* pdwY2 = pdwBase+ciXdwStride;
                int iY = ciYStrideCount;
                for ( ; iY >= 8; iY -= 8)
                {
                    iTotal += *pdwY + *pdwY2;      pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    iTotal += *pdwY + *pdwY2;      pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    iTotal += *pdwY + *pdwY2;      pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    iTotal += *pdwY + *pdwY2;      pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    iTotal += *pdwY + *pdwY2;      pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    iTotal += *pdwY + *pdwY2;      pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    iTotal += *pdwY + *pdwY2;      pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    iTotal += *pdwY + *pdwY2;      pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                }
                for ( ; iY >= 4; iY -= 4)
                {
                    iTotal += *pdwY + *pdwY2;      pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    iTotal += *pdwY + *pdwY2;      pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    iTotal += *pdwY + *pdwY2;      pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    iTotal += *pdwY + *pdwY2;      pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                }
                for ( ; iY > 0; iY-- )
                {
                    iTotal += *pdwY + *pdwY2;      pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                }
            }
            return iTotal;
        }
    // add any interesting ciXStrideCount cases here for small ciXStrideCount
    default:
        {
            // if needed in the future, switch on interesting values of ciYStrideCount but not needed yet
            int iTotal = 0; 
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile const DWORD* pdwY = pdwBase;
                int iY = ciYStrideCount;
                for( ; iY > 0; iY--, pdwY += ciYdwStride )
                {
                    int iX = ciXStrideCount;
                    volatile const DWORD* pdwX = pdwY;
                    for ( ; iX >= 16; iX -= 16)
                    {
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                    }
                    for ( ; iX >= 4; iX -= 4)
                    {
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                    }
                    for ( ; iX > 0; iX-- )
                    {
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                    }
                }
            }
            return iTotal;
        }
    }
}


// ***************************************************************************************************************
// 2-D Read test functions
// implement main test loops in a separate function (e.g. don't inline) so the compiler will do better register allocations
// but ended up putting them in assembly to avoid the compiler over-optimizing and adding some stack accesses
// ***************************************************************************************************************

#ifndef _ARM_ 
int iRead2D4( const DWORD *pdwBase, const int ciRepeats,  const tStridePattern &ctStrideInfo )
{
    const int ciYStrideCount = ctStrideInfo.m_iYStrides;
    const int ciYdwStride    = ctStrideInfo.m_iYdwStride;
    const int ciXStrideCount = ctStrideInfo.m_iXStrides;
    const int ciXdwStride    = ctStrideInfo.m_iXdwStride;
    const int ciYdwDelta4    = ciYdwStride-3*ciXdwStride;
    // interesting cases for some testing is 4==ciXStrideCount so special case that
    assert( 4 == ciXStrideCount );
    int iTotal = 0; 
    for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
    {   
        // these pointers should be volatile but the current compiler adds three stack references per loop
        // which defeats the purpose of this function.
        // after building, examine the code in LibBld\Obj\Arm*\ReadLoop2D.asm
        const DWORD* pdwY = pdwBase;
        int iY = ciYStrideCount;
        for ( ; iY >= 4; iY -= 4)
        {
            iTotal += *pdwY;      pdwY += ciXdwStride;
            iTotal += *pdwY;      pdwY += ciXdwStride;
            iTotal += *pdwY;      pdwY += ciXdwStride;
            iTotal += *pdwY;      pdwY += ciYdwDelta4;
            iTotal += *pdwY;      pdwY += ciXdwStride;
            iTotal += *pdwY;      pdwY += ciXdwStride;
            iTotal += *pdwY;      pdwY += ciXdwStride;
            iTotal += *pdwY;      pdwY += ciYdwDelta4;
            iTotal += *pdwY;      pdwY += ciXdwStride;
            iTotal += *pdwY;      pdwY += ciXdwStride;
            iTotal += *pdwY;      pdwY += ciXdwStride;
            iTotal += *pdwY;      pdwY += ciYdwDelta4;
            iTotal += *pdwY;      pdwY += ciXdwStride;
            iTotal += *pdwY;      pdwY += ciXdwStride;
            iTotal += *pdwY;      pdwY += ciXdwStride;
            iTotal += *pdwY;      pdwY += ciYdwDelta4;
        }
        for ( ; iY > 0; iY-- )
        {
            iTotal += *pdwY;      pdwY += ciXdwStride;
            iTotal += *pdwY;      pdwY += ciXdwStride;
            iTotal += *pdwY;      pdwY += ciXdwStride;
            iTotal += *pdwY;      pdwY += ciYdwDelta4;
        }
    }
    return iTotal;
}
#endif

#ifdef _ARM_ 
int iRead2DBig( const DWORD *pdwBase, const int ciRepeats,  const tStridePattern &ctStrideInfo );
#else
int iRead2DBig( const DWORD *pdwBase, const int ciRepeats,  const tStridePattern &ctStrideInfo )
{
    const int ciYStrideCount = ctStrideInfo.m_iYStrides;
    const int ciYdwStride    = ctStrideInfo.m_iYdwStride;
    const int ciXStrideCount = ctStrideInfo.m_iXStrides;
    const int ciXdwStride    = ctStrideInfo.m_iXdwStride;
    assert( 2 < ciXStrideCount && 4 != ciXStrideCount );
    int iTotal = 0; 
    for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
    {   
        volatile const DWORD* pdwY = pdwBase;
        int iY = ciYStrideCount;
        for( ; iY > 0; iY--, pdwY += ciYdwStride )
        {
            int iX = ciXStrideCount;
            volatile const DWORD* pdwX = pdwY;
            for ( ; iX >= 16; iX -= 16)
            {
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
            }
            for ( ; iX >= 4; iX -= 4)
            {
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
                iTotal += *pdwX;            pdwX += ciXdwStride;
            }
            for ( ; iX > 0; iX-- )
            {
                iTotal += *pdwX;            pdwX += ciXdwStride;
            }
        }
    }
    return iTotal;
}
#endif



// ***************************************************************************************************************
// a really weird pattern meant to separate cache replacement policies PLRU from per-set-round-robin
// see visualization output to the CSV file with -c -p and search for "grid"
// ***************************************************************************************************************

int iReadLoop3D( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo )
{

    switch( ctStrideInfo.m_iXStrides )
    {
    case 0:  // let this degenerate case get handled by the 1D case below            
    case 1:
        {   // Really the 1D case
            return iReadLoop1D( pdwBase, ciRepeats, ctStrideInfo );
        }
    // add any interesting ciXStrideCount cases here for small ciXStrideCount
    default:
        {
            const int ciYdwStride    = ctStrideInfo.m_iYdwStride;           // 64K      // L2 set alias
            const int ciXStrideCount = ctStrideInfo.m_iXStrides;            // 6-32     // use up all the ways of the L1
            const int ciXdwStride    = ctStrideInfo.m_iXdwStride;           // 2048     // L1 Set alias
            int iTotal = 0; 
            const int ciScamble = 2==ctStrideInfo.m_iYStrides ? 0x0 : ( 4==ctStrideInfo.m_iYStrides ? 0x0213 : 0x04261537);
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                // prime things by accessing in Y order first so have a known contents of the ways.
                int iY = ctStrideInfo.m_iYStrides;
                volatile const DWORD* pdwY = pdwBase;
                for( ; iY > 0; iY--, pdwY += ciYdwStride )
                {
                    volatile const DWORD* pdwX = pdwY;
                    int iX = ciXStrideCount;
                    for ( ; iX >= 4; iX -= 4)
                    {
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                    }
                    for ( ; iX > 0; iX-- )
                    {
                        iTotal += *pdwX;            pdwX += ciXdwStride;
                    }
                }
                // Now access using the {0,2,1,new} pattern for 4-way
                volatile const DWORD* pdwZ = pdwBase;
                int iZ = ctStrideInfo.m_iZStrides;
                for( ; iZ > 0; iZ--, pdwZ += ctStrideInfo.m_iZdwStride )
                {
                    int iScamble = ciScamble;
                    for( iY = ctStrideInfo.m_iYStrides-1; iY >= 0; iY--, iScamble >>= 4 )
                    {
                        volatile const DWORD* pdwX;
                        if ( iY > 0 )
                        {
                            pdwX = pdwBase + ( ciYdwStride * (0xF&iScamble) );
                        }
                        else
                        {
                            pdwX = pdwZ;
                        }
                        int iX = ciXStrideCount;
                        for ( ; iX >= 4; iX -= 4)
                        {
                            iTotal += *pdwX;            pdwX += ciXdwStride;
                            iTotal += *pdwX;            pdwX += ciXdwStride;
                            iTotal += *pdwX;            pdwX += ciXdwStride;
                            iTotal += *pdwX;            pdwX += ciXdwStride;
                        }
                        for ( ; iX > 0; iX-- )
                        {
                            iTotal += *pdwX;            pdwX += ciXdwStride;
                        }
                    }
                }
            }
            return iTotal;
        }
    }
}