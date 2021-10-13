//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

// Utilities: Implementation of utilities

#include "stdafx.h"

DWORD DBToAmpFactor(LONG lDB)
{
    double dAF;

    // REMIND hack to make mixer code work- it only handles 16-bit factors and
    //  cannot amplify
    if (0 <= lDB) return 0x0000FFFF;

    // input lDB is 100ths of decibels

    dAF = pow(10.0, (0.5+((double)lDB))/2000.0);

    // This gives me a number in the range 0-1
    // normalise to 0-65535

    return (DWORD)(dAF*65535);
}


LONG AmpFactorToDB(DWORD dwFactor)
{
    if(1>=dwFactor)
    {
        return -10000;
    }
    else if (0xFFFF <= dwFactor) 
    {
        return 0;    // This puts an upper bound - no amplification
    }
    else
    {
        return (LONG)(2000.0 * log10((-0.5+(double)dwFactor)/65536.0));
    }
}


LONG VolumeLinToLog(short LinKnobValue)
{
    LONG lLinMin = DBToAmpFactor(AX_MIN_VOLUME);
    LONG lLinMax = DBToAmpFactor(AX_MAX_VOLUME);

    LONG lLinTemp = (LONG) (LinKnobValue - MIN_VOLUME_RANGE) * (lLinMax - lLinMin)
        / (MAX_VOLUME_RANGE - MIN_VOLUME_RANGE) + lLinMin;

    LONG LogValue = AmpFactorToDB( lLinTemp );

    return( LogValue );
}
