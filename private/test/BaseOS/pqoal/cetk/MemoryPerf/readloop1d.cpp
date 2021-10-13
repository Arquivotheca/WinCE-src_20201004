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
// These functions provide a measureable 1-D read loop 
// the loop is unrolled in a variety of special cases to provide low overhead measurements.
//
// ***************************************************************************************************************
//
// Warning - if you change this file, you must enable VERIFY_OPTIMIZED and run 
//   s memoryperfexe -d -c -o
// without getting a "Warning: iReadLoop2D mismatch" in the log file
//
// Do not check-in with VERIFY_OPTIMIZED defined.


#include "memoryPerf.h"

// ***************************************************************************************************************
// optimize this function even when debugging 
#if defined(DEBUG) || defined(_DEBUG)
#pragma optimize("gty",on)
#endif


// the following code structure of having multiple loops within a function is not ameanable to
// PGO optimization since currently PGO only aligns one loop per function :(.
// Replace with a function pointer to multiple functions but leave this for reference 
// and leave it because it is still used for CPU loading.

// ***************************************************************************************************************
// Main Read test function
// implement main test loops in a separate function (e.g. don't inline) so the compiler will do better register allocations
// ***************************************************************************************************************

int iReadLoop1D( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo )
{
    const int ciStrideCount = ctStrideInfo.m_iYStrides;
    const int ciDWStride    = ctStrideInfo.m_iYdwStride;

    // extreme unrolling is because of loop overhead at ciStrideCount <= 8 which contaminates measurements, so unroll simple cases to 16.
    // use volatile below to keep compiler from optimizing the loop away and reading once followed by a multiply.
    switch (ciStrideCount) 
    {
    case 0:
        assert( ciStrideCount > 0 );
        return 0;
    case 1:
        {
            int iTotal = 0; 
            int iRpt = ciRepeats;
            for ( ; iRpt >= 16; iRpt -= 16)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;
                iTotal += *pdwT;
                iTotal += *pdwT;
                iTotal += *pdwT;
                iTotal += *pdwT;
                iTotal += *pdwT;
                iTotal += *pdwT;
                iTotal += *pdwT;
                iTotal += *pdwT;
                iTotal += *pdwT;
                iTotal += *pdwT;
                iTotal += *pdwT;
                iTotal += *pdwT;
                iTotal += *pdwT;
                iTotal += *pdwT;
                iTotal += *pdwT;
            }
            for ( ; iRpt >= 4; iRpt -= 4)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;
                iTotal += *pdwT;
                iTotal += *pdwT;
                iTotal += *pdwT;
            }
            for ( ; iRpt >= 0; iRpt--)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;
            }
            return iTotal;
        }
    case 2:
        {
            int iTotal = 0; 
            int iRpt = ciRepeats;
            for ( ; iRpt >= 4; iRpt -= 4)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;            }
            for ( ; iRpt > 0; iRpt-- )
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
            }
            return iTotal;
        }
    case 3:
        {
            int iTotal = 0; 
            int iRpt = ciRepeats;
            for ( ; iRpt >= 4; iRpt -= 4)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
            }
            for ( ; iRpt > 0; iRpt-- )
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
            }
            return iTotal;
        }
    case 4:
        {
            int iTotal = 0; 
            int iRpt = ciRepeats;
            for ( ; iRpt >= 4; iRpt -= 4)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
            }
            for ( ; iRpt > 0; iRpt-- )
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
            }
            return iTotal;
        }
    case 5:
        {
            int iTotal = 0; 
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
            }
            return iTotal;
        }
    case 6:
        {
            int iTotal = 0; 
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
            }
            return iTotal;
        }
    case 7:
        {
            int iTotal = 0; 
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
            }
            return iTotal;
        }
    case 8:
        {
            int iTotal = 0; 
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
            }
            return iTotal;
        }
    case 9:
        {
            int iTotal = 0; 
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
            }
            return iTotal;
        }
    case 10:
        {
            int iTotal = 0; 
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
           }
            return iTotal;
        }
    case 11:
        {
            int iTotal = 0; 
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
            }
            return iTotal;
        }
    case 12:
        {
            int iTotal = 0; 
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
            }
            return iTotal;
        }
    case 13:
        {
            int iTotal = 0; 
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
            }
            return iTotal;
        }
    case 14:
        {
            int iTotal = 0; 
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
            }
            return iTotal;
        }
    case 15:
        {
            int iTotal = 0; 
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
            }
            return iTotal;
        }
    case 16:
        {
            int iTotal = 0; 
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile const DWORD* pdwT = pdwBase;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;                pdwT += ciDWStride;
                iTotal += *pdwT;
            }
            return iTotal;
        }
    default:
        {
            int iTotal = 0; 
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile const DWORD* pdwT = pdwBase;
                int i = ciStrideCount;
                for ( ; i >= 16; i -= 16)
                {
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                }
                for ( ; i >= 4; i -= 4)
                {
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                    iTotal += *pdwT;            pdwT += ciDWStride;
                }
                for ( ; i > 0; i-- )
                {
                    iTotal += *pdwT;            pdwT += ciDWStride;
                }
            }
            return iTotal;
        }
    }
}


// -------------------------------------------------------------------------------------

#ifdef _ARM_ 
extern int iRead1D1( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo );
#else
int iRead1D1( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo )
{
    const int ciDWStride    = ctStrideInfo.m_iYdwStride;
    assert( 1 == ctStrideInfo.m_iYStrides );

    int iTotal = 0; 
    int iRpt = ciRepeats;
    for ( ; iRpt >= 16; iRpt -= 16)
    {   
        volatile const DWORD* pdwT = pdwBase;
        iTotal += *pdwT;
        iTotal += *pdwT;
        iTotal += *pdwT;
        iTotal += *pdwT;
        iTotal += *pdwT;
        iTotal += *pdwT;
        iTotal += *pdwT;
        iTotal += *pdwT;
        iTotal += *pdwT;
        iTotal += *pdwT;
        iTotal += *pdwT;
        iTotal += *pdwT;
        iTotal += *pdwT;
        iTotal += *pdwT;
        iTotal += *pdwT;
        iTotal += *pdwT;
    }
    for ( ; iRpt >= 4; iRpt -= 4)
    {   
        volatile const DWORD* pdwT = pdwBase;
        iTotal += *pdwT;
        iTotal += *pdwT;
        iTotal += *pdwT;
        iTotal += *pdwT;
    }
    for ( ; iRpt >= 0; iRpt--)
    {   
        volatile const DWORD* pdwT = pdwBase;
        iTotal += *pdwT;
    }
    return iTotal;
}
#endif


#ifdef _ARM_ 
extern int iRead1D2( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo );
#else
int iRead1D2( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo )
{
    const int ciDWStride    = ctStrideInfo.m_iYdwStride;
    assert( 2 == ctStrideInfo.m_iYStrides );
    int iTotal = 0; 
    int iRpt = ciRepeats;
    for ( ; iRpt >= 4; iRpt -= 4)
    {   
        volatile const DWORD* pdwT = pdwBase;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT = pdwBase;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT = pdwBase;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT = pdwBase;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;            }
    for ( ; iRpt > 0; iRpt-- )
    {   
        volatile const DWORD* pdwT = pdwBase;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;
    }
    return iTotal;
}
#endif


#ifdef _ARM_ 
extern int iRead1D4( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo );
#else
int iRead1D4( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo )
{
    const int ciDWStride    = ctStrideInfo.m_iYdwStride;
    assert( 4 == ctStrideInfo.m_iYStrides );
    int iTotal = 0; 
    int iRpt = ciRepeats;
    for ( ; iRpt >= 4; iRpt -= 4)
    {   
        volatile const DWORD* pdwT = pdwBase;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT = pdwBase;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT = pdwBase;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT = pdwBase;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;
    }
    for ( ; iRpt > 0; iRpt-- )
    {   
        volatile const DWORD* pdwT = pdwBase;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;
    }
    return iTotal;
}
#endif


#ifdef _ARM_ 
extern int iRead1D8( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo );
#else
int iRead1D8( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo )
{
    const int ciDWStride    = ctStrideInfo.m_iYdwStride;
    assert( 8 == ctStrideInfo.m_iYStrides );
    int iTotal = 0; 
    for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
    {   
        volatile const DWORD* pdwT = pdwBase;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;
    }
    return iTotal;
}
#endif


#ifdef _ARM_ 
extern int iRead1D12( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo );
#else
int iRead1D12( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo )
{
    const int ciDWStride    = ctStrideInfo.m_iYdwStride;
    assert( 12 == ctStrideInfo.m_iYStrides );
    int iTotal = 0; 
    for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
    {   
        volatile const DWORD* pdwT = pdwBase;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;
    }
    return iTotal;
}
#endif


#ifdef _ARM_ 
extern int iRead1D16( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo );
#else
int iRead1D16( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo )
{
    const int ciDWStride    = ctStrideInfo.m_iYdwStride;
    assert( 16 == ctStrideInfo.m_iYStrides );
    int iTotal = 0; 
    for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
    {   
        volatile const DWORD* pdwT = pdwBase;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;                pdwT += ciDWStride;
        iTotal += *pdwT;
    }
    return iTotal;
}
#endif

#ifdef _ARM_ 
extern int iRead1DBig( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo );
#else
int iRead1DBig( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo )
{
    const int ciStrideCount = ctStrideInfo.m_iYStrides;
    const int ciDWStride    = ctStrideInfo.m_iYdwStride;
    assert( 15 < ctStrideInfo.m_iYStrides );
    int iTotal = 0; 
    for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
    {   
        volatile const DWORD* pdwT = pdwBase;
        int i = ciStrideCount;
        for ( ; i >= 16; i -= 16)
        {
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
        }
        for ( ; i >= 4; i -= 4)
        {
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
            iTotal += *pdwT;            pdwT += ciDWStride;
        }
        for ( ; i > 0; i-- )
        {
            iTotal += *pdwT;            pdwT += ciDWStride;
        }
    }
    return iTotal;
}
#endif

#pragma warning( disable: 22110 )  // global function pointers are not a security concern in this app

extern int (* pfiReadLoop)( const DWORD *, const int, const tStridePattern & ) = &iReadLoop1D;


void ReadLoopSetup( const tStridePattern &ctStrideInfo )
{
    const int eTL = ctStrideInfo.m_eTL;
    const int ciYStrideCount = ctStrideInfo.m_iYStrides;
    const int ciYdwStride    = ctStrideInfo.m_iYdwStride;
    const int ciXStrideCount = ctStrideInfo.m_iXStrides;
    const int ciXdwStride    = ctStrideInfo.m_iXdwStride;
    // interesting cases for cache line testing is 2==ciXStrideCount so special case that
    // use volatile below to keep compiler from optimizing the loop away and reading once followed by a multiply.

    switch( eTL )
    {
        case eReadLoop1D:
            switch (ciYStrideCount) 
            {
                case 0:
                    assert( ciYStrideCount > 0 );
                    pfiReadLoop = &iReadLoop1D;
                    break;
                case 1:
                    pfiReadLoop = &iRead1D1;
                    break;
                case 2:
                    pfiReadLoop = &iRead1D2;
                    break;
                case 3: case 5: case 6: case 7: case 9: case 10: case 11: case 13: case 14: case 15:
                    pfiReadLoop = &iReadLoop1D;
                    break;
                case 4:
                    pfiReadLoop = &iRead1D4;
                    break;
                case 8:
                    pfiReadLoop = &iRead1D8;
                    break;
                case 12:
                    pfiReadLoop = &iRead1D12;
                    break;
                case 16:
                    pfiReadLoop = &iRead1D16;
                    break;
                default:
                    pfiReadLoop = &iRead1DBig;
                    break;
                }
#if defined(VERIFY_OPTIMIZED)
                // Make sure the optimized and unoptimized return the same iTotal;
                // but only when the pattern changes.
                if ( pfiReadLoop != &iReadLoop1D )
                {
                    static bool bWarned[18] = {false};
                    static tStridePattern tspPrior = { 0 };
                    if ( ( tspPrior.m_eTL != ctStrideInfo.m_eTL ||
                           tspPrior.m_iYStrides  != ctStrideInfo.m_iYStrides ||
                           tspPrior.m_iYdwStride != ctStrideInfo.m_iYdwStride 
                           ) && 
                         !bWarned[min(17,ciYStrideCount)] 
                        )
                    {
                        const int ciRepeats = (ciYStrideCount<=4) ? 24 : 3;
                        int iRegular   = iReadLoop1D( gtGlobal.pdw1MBBase, ciRepeats, ctStrideInfo );
                        int iOptimized = pfiReadLoop( gtGlobal.pdw1MBBase, ciRepeats, ctStrideInfo );
                        if ( iRegular != iOptimized )
                        {
                            // do not report the exact mismatch values since they are meaningless.
                            LogPrintf( "Warning: iReadLoop1D mismatch for {, %s, %d, %d, %f%% }\r\n,",
                                        pstrTests[ctStrideInfo.m_eTL],
                                        ctStrideInfo.m_iYStrides, ctStrideInfo.m_iYdwStride*sizeof(DWORD),
                                        ctStrideInfo.m_fPreloadPercent
                                        );
                            bWarned[min(17,ciYStrideCount)] = true;
                        }
                        tspPrior = ctStrideInfo;
                    }
                }
#endif
                break;
        case eReadLoop2D:
                switch( ciXStrideCount )
                {
                case 0:
                    assert( ciXStrideCount > 0 );
                    pfiReadLoop = &iReadLoop1D;
                    break;
                case 2:
                    pfiReadLoop = &iReadLoop1D;
                    break;
                case 4:
                    pfiReadLoop = &iRead2D4;
                    break;
                case 1:
                default:
                    pfiReadLoop = &iRead2DBig;
                    break;
                }
#if defined(VERIFY_OPTIMIZED)
                // Make sure the optimized and unoptimized return the same iTotal
                // but only when the pattern changes.
                if ( pfiReadLoop != &iReadLoop2D )
                {
                    static bool bWarned[6] = {false};
                    static tStridePattern tspPrior = { 0 };
                    if ( ( tspPrior.m_eTL != ctStrideInfo.m_eTL ||
                           tspPrior.m_iYStrides  != ctStrideInfo.m_iYStrides  ||
                           tspPrior.m_iYdwStride != ctStrideInfo.m_iYdwStride ||
                           tspPrior.m_iXStrides  != ctStrideInfo.m_iXStrides  ||
                           tspPrior.m_iXdwStride != ctStrideInfo.m_iXdwStride 
                           ) && 
                         !bWarned[min(5,ciYStrideCount)] 
                        )
                    {
                        int iRegular   = iReadLoop2D( gtGlobal.pdw1MBBase, 3, ctStrideInfo );
                        int iOptimized = pfiReadLoop( gtGlobal.pdw1MBBase, 3, ctStrideInfo );
                        if ( iRegular != iOptimized )
                        {
                            // do not report the exact mismatch values since they are meaningless.
                            LogPrintf( "Warning: iReadLoop2D mismatch for {, %s, %d, %d, %d, %d, %f%% }\r\n",
                                        pstrTests[ctStrideInfo.m_eTL],
                                        ctStrideInfo.m_iYStrides, ctStrideInfo.m_iYdwStride*sizeof(DWORD), 
                                        ctStrideInfo.m_iXStrides, ctStrideInfo.m_iXdwStride*sizeof(DWORD),
                                        ctStrideInfo.m_fPreloadPercent
                                      );
                            bWarned[min(5,ciYStrideCount)] = true;
                        }
                        tspPrior = ctStrideInfo;
                    }
                }
#endif
                break;
        case eWriteLoop1D:
        case eWriteLoop2D:
        case eMemCpyLoop:
        case eReadLoop3D:
        case eCPULoop:
                break;
        default:
                break;
    }
}
