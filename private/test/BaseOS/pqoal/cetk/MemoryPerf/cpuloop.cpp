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
// This function shows a C-equivalent to CPULoop.s but the code in CPULoop.s is generated and optimized in a 
// particular way to allow multiple execution pipelines a good chance of being active at the same time.
//
// ***************************************************************************************************************


#include "memoryPerf.h"

#if defined(DEBUG) || defined(_DEBUG)
// optimize this function even when debugging 
#pragma optimize("gty",on)
#endif

#ifndef _ARM_  
// Main CPU test function is actually implemented in assembly for ARM - see CPULoopASM.s
int iCPULoop( const int ciRepeats )
{

    // create a dependent calculation which the compiler can't over-optimize and the CPU can retire more than 1 instruction per cycle
    // perhaps the only retiring 1 instruction per cycle is too optimistic.

    // The volatile and calls to rand_s() keeps the compiler from optimizing these loops completely away.
    // generate an .asm file from this .cpp, then create a .s from the .asm which will build in place of this .cpp 
    // edit that .s to remove the ldr and str from the stack of iTotal
    // for platforms without the an assembly file to build, there will be stack accesses in this loop
    // so it will at least waste time and warm up the CPU if it needs it,
    // but it will not be useful for measuing the speed of the CPU.
        volatile int iTotal = 0;
        UINT uiX1 = 0;
        UINT uiX2 = 0;
        rand_s(&uiX1);
        rand_s(&uiX2);
        int iRpt = ciRepeats;
        for ( ; iRpt >= 16; iRpt -= 16)
        {   
            iTotal += uiX1+uiX2;
            iTotal += uiX1+uiX2;
            iTotal += uiX1+uiX2;
            iTotal += uiX1+uiX2;
            iTotal += uiX1+uiX2;
            iTotal += uiX1+uiX2;
            iTotal += uiX1+uiX2;
            iTotal += uiX1+uiX2;
        }
        for ( ; iRpt >= 2; iRpt -= 2)
        {   
            iTotal += uiX1+uiX2;
        }
        for ( ; iRpt >= 0; iRpt--)
        {   
            iTotal += uiX1;
        }
        return iTotal;
}

DWORD dwCallerAligned8(const DWORD *pdw)
{
    if ( NULL!=pdw ) return 0;
    // return ( _ReturnAddress()&4 ? 5 : 1 );  // see CPULoopASM.s for this for ARM.
    return 1;       // alignment does not matter on X86 that much
}
#endif

