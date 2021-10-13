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
// These functions provide a measureable 2-D write loop 
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
// Main 2D Write test function
// implement main test loops in a separate function (e.g. don't inline) so the compiler will do better register allocations
// ***************************************************************************************************************

int iWriteLoop2D( _Out_cap_(cbBase) DWORD *pdwBase, const unsigned int cbBase, const int ciRepeats,  const tStridePattern &ctStrideInfo, const DWORD cdw )
{
    const int ciYStrideCount = ctStrideInfo.m_iYStrides;
    const int ciYdwStride    = ctStrideInfo.m_iYdwStride;
    const int ciXStrideCount = ctStrideInfo.m_iXStrides;
    const int ciXdwStride    = ctStrideInfo.m_iXdwStride;
    // interesting cases for cache line testing is 2==ciXStrideCount so special case that
    // use volatile below to keep compiler from optimizing the loop away and just writing once.

    switch( ciXStrideCount )
    {
    case 0:  // let this degenerate case get handled by the 1D case below            
        assert( ciXStrideCount > 0 );
        return 0;
    case 1:
        {  // Really the 1D case
           // (not actualy used in -c -o but leave to allow future experimental use)
           return iWriteLoop1D( pdwBase, cbBase, ciRepeats, ctStrideInfo, cdw );
        }
    case 2:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                // these pointers should be volatile but the current compiler adds three stack references per loop
                // which defeats the purpose of this function.
                // after building, examine the code in LibBld\Obj\Arm*\ReadLoop2D.asm
                DWORD* pdwY = pdwBase;
                DWORD* pdwY2 = pdwBase+ciXdwStride;
                int iY = ciYStrideCount;
                for ( ; iY >= 8; iY -= 8)
                {
                    *pdwY = cdw;    *pdwY2 = cdw;   pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    *pdwY = cdw;    *pdwY2 = cdw;   pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    *pdwY = cdw;    *pdwY2 = cdw;   pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    *pdwY = cdw;    *pdwY2 = cdw;   pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    *pdwY = cdw;    *pdwY2 = cdw;   pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    *pdwY = cdw;    *pdwY2 = cdw;   pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    *pdwY = cdw;    *pdwY2 = cdw;   pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    *pdwY = cdw;    *pdwY2 = cdw;   pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                }
                for ( ; iY >= 4; iY -= 4)
                {
                    *pdwY = cdw;    *pdwY2 = cdw;   pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    *pdwY = cdw;    *pdwY2 = cdw;   pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    *pdwY = cdw;    *pdwY2 = cdw;   pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                    *pdwY = cdw;    *pdwY2 = cdw;   pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                }
                for ( ; iY > 0; iY-- )
                {
                    *pdwY = cdw;    *pdwY2 = cdw;   pdwY += ciYdwStride;     pdwY2 += ciYdwStride;
                }
            }
            return cdw*ciRepeats;
        }
    // add any interesting ciXStrideCount cases here for small ciXStrideCount
    default:
        {
            // if needed in the future, switch on interesting values of ciYStrideCount but not needed yet
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile DWORD* pdwY = pdwBase;
                int iY = ciYStrideCount;
                for( ; iY > 0; iY--, pdwY += ciYdwStride )
                {
                    int iX = ciXStrideCount;
                    volatile DWORD* pdwX = pdwY;
                    for ( ; iX >= 16; iX -= 16)
                    {
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                    }
                    for ( ; iX >= 4; iX -= 4)
                    {
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                        *pdwX = cdw;                pdwX += ciXdwStride;
                    }
                    for ( ; iX > 0; iX-- )
                    {
                        *pdwX = cdw;                pdwX += ciXdwStride;
                    }
                }
            }
            return cdw*ciRepeats;
        }
    }
}