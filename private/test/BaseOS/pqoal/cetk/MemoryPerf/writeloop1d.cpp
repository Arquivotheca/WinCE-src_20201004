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
// These functions provide a measureable 1-D write loop 
// the loop is unrolled in a variety of special cases to provide low overhead measurements.
//
// ***************************************************************************************************************


#include "MemoryPerf.h"

// ******************************************
// optimize this function even when debugging 
#if defined(DEBUG) || defined(_DEBUG)
#pragma optimize("gty",on)
#endif


// ***************************************************************************************************************
// Main Write test function
// implement main test loops in a separate function (e.g. don't inline) so the compiler will do better register allocations
// ***************************************************************************************************************

int iWriteLoop1D( _Out_cap_(cbBase) DWORD *pdwBase, const unsigned int cbBase, const int ciRepeats, const tStridePattern &ctStrideInfo, const DWORD cdw )
{
    const int ciStrideCount = ctStrideInfo.m_iYStrides;
    const int ciDWStride    = ctStrideInfo.m_iYdwStride;

    // extreme unrolling is because we expect and see some other phenomena at 8, so unroll simple cases to 16.
    // use volatile below to keep compiler from optimizing the loop away and just writing once

    switch (ciStrideCount) 
    {
    case 0:
        assert( ciStrideCount > 0 );
        return 0;
    case 1:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            int iRpt = ciRepeats;
            for ( ; iRpt >= 16; iRpt -= 16)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;
                *pdwT = cdw;
                *pdwT = cdw;
                *pdwT = cdw;
                *pdwT = cdw;
                *pdwT = cdw;
                *pdwT = cdw;
                *pdwT = cdw;
                *pdwT = cdw;
                *pdwT = cdw;
                *pdwT = cdw;
                *pdwT = cdw;
                *pdwT = cdw;
                *pdwT = cdw;
                *pdwT = cdw;
                *pdwT = cdw;
            }
            for ( ; iRpt >= 4; iRpt -= 4)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;
                *pdwT = cdw;
                *pdwT = cdw;
                *pdwT = cdw;
            }
            for ( ; iRpt >= 0; iRpt--)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;
            }
            return cdw*ciRepeats;
        }
    case 2:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            int iRpt = ciRepeats;
            for ( ; iRpt >= 4; iRpt -= 4)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            for ( ; iRpt > 0; iRpt-- )
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            return cdw*ciRepeats;
        }
    case 3:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            int iRpt = ciRepeats;
            for ( ; iRpt >= 4; iRpt -= 4)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            for ( ; iRpt > 0; iRpt-- )
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            return cdw*ciRepeats;
        }
    case 4:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            int iRpt = ciRepeats;
            for ( ; iRpt >= 4; iRpt -= 4)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            for ( ; iRpt > 0; iRpt-- )
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            return cdw*ciRepeats;
        }
    case 5:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            return cdw*ciRepeats;
        }
    case 6:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            return cdw*ciRepeats;
        }
    case 7:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            return cdw*ciRepeats;
        }
    case 8:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            return cdw*ciRepeats;
        }
    case 9:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            return cdw*ciRepeats;
        }
    case 10:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
           }
            return cdw*ciRepeats;
        }
    case 11:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            return cdw*ciRepeats;
        }
    case 12:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            return cdw*ciRepeats;
        }
    case 13:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            return cdw*ciRepeats;
        }
    case 14:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            return cdw*ciRepeats;
        }
    case 15:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            return cdw*ciRepeats;
        }
    case 16:
        {
            // (not actualy used in -c -o but leave to allow future experimental use)
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile DWORD* pdwT = pdwBase;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;    pdwT += ciDWStride;
                *pdwT = cdw;
            }
            return cdw*ciRepeats;
        }
    default:
        {
            for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
            {   
                volatile DWORD* pdwT = pdwBase;
                int i = ciStrideCount;
                for ( ; i >= 16; i -= 16)
                {
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                }
                for ( ; i >= 4; i -= 4)
                {
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                    *pdwT = cdw;        pdwT += ciDWStride;
                }
                for ( ; i > 0; i-- )
                {
                    *pdwT = cdw;        pdwT += ciDWStride;
                }
            }
            return cdw*ciRepeats;
        }
    }
}


// ***************************************************************************************************************
// Memcpy calls the memcpy() function which is typically highly optimized arm code
// ***************************************************************************************************************

int iMemCpyLoop( _Out_cap_(cbBase) DWORD *pdwDstBase, const unsigned int cbBase, const DWORD *pdwSrcBase, const int ciRepeats, const tStridePattern &tStrideInfo )
{
    assert( 1 == tStrideInfo.m_iYdwStride );

    const int ciStrideCount = tStrideInfo.m_iYStrides;
    const int ciByteCount = ciStrideCount*sizeof(DWORD);

    for (int iRpt = 0; iRpt < ciRepeats; iRpt++)
    {   
        memcpy_s( pdwDstBase, ciByteCount, pdwSrcBase, ciByteCount );
    }
    return ciByteCount*iRpt;
}

