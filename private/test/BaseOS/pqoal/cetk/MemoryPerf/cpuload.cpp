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
// These functions provide a mixed CPU bound and memory bound measureable test used to warm up the CPU out of
// low power modes.
//
// ***************************************************************************************************************

#include "memoryPerf.h"

#if defined(DEBUG) || defined(_DEBUG)
// optimize this function even when debugging 
#pragma optimize("gty",on)
#endif


// the following is just intended to load the CPU to ramp up the speed.
int iCPULoad( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo )
{
    int iTotal = 0; 
    const int iUnroll = 2;
    const int iCPURepeats = max(ctStrideInfo.m_iYStrides/16,2);
    int iRpt;
    for ( iRpt = 0; (iRpt+iUnroll) < ciRepeats; iRpt += iUnroll)
    {
        iTotal += iCPULoop( iUnroll*iCPURepeats );
        iTotal += iReadLoop1D( pdwBase, iUnroll, ctStrideInfo );
    }
    for ( ; iRpt < ciRepeats; iRpt += 1)
    {
        iTotal += iCPULoop( iCPURepeats );
        iTotal += iReadLoop1D( pdwBase, 1, ctStrideInfo );
    }
    return iTotal;
}