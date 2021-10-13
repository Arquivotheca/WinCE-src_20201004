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
/*
 * randomNumbers.cpp
 */

#include "randomNumbers.h"
#include "..\..\..\common\commonUtils.h"

/*
 * Add one to a DWORD, but in the mixed up number system.
 *
 * If carry needs to occur, this function returns true.  Else it
 * return false.
 *
 * dwVal is the value to be incremented.  The inbound value is the
 * value to changed and the output value has this changed value.  Much
 * like count++.
 *
 * dwMaskBitsWeWant is the bits in dwVal that are valid.  Currently,
 * our addition algorithm works only with the first bit or first two
 * bits.  This function needs to be extended if we ever change how we
 * do addition (move to different bit widths).
 *
 * We don't carry in the one bit case, since this is used only for
 * overflow and we don't carry in the overflow.  It makes the caller
 * simpler to not return carry in this case.
 *
 * From a coding perspective, a look up table would be cleaner.  This
 * would have to be stored in memory, though, and this is used for the
 * cache tests, so we would like to keep as much stuff as possible out
 * of memory.  The switch statements below should be converted to
 * opcodes using only registers, so it works better in this situation.
 */

// Complaining about code not reachable (the return FALSE statement)
#pragma warning(push)
#pragma warning( disable: 4702)
inline
BOOL
mixedAddOne (DWORD * dwVal, DWORD dwMaskBitsWeWant)
{
    if (!((dwVal != NULL) && (*dwVal >= 0) && (*dwVal <= 3)))
    {
        IntErr (_T("mixedAddOne: ")
          _T("!((dwVal != NULL) && (*dwVal >= 0) && (*dwVal <= 3))"));
        return FALSE;
    }

    if (!((dwMaskBitsWeWant == 1) || (dwMaskBitsWeWant == 3)))
    {
        IntErr (_T("mixedAddOne: ")
          _T("!((dwMaskBitsWeWant == 1) || (dwMaskBitsWeWant == 3))"));
        return FALSE;
    }

    switch (dwMaskBitsWeWant)
    {
    case 1:
        /*
       * if only want one bit then just impose an ordering
       */
        switch (*dwVal)
        {
        case 0:
            *dwVal = 1;
            return (FALSE);
        case 1:
            *dwVal = 0;
            /*
             * we don't carry because it makes the caller simpler
             */
            return (FALSE);
        default:
            *dwVal = BAD_VAL;
            return (FALSE);
        }
    case 3:
        /* the meat of the addition */
        switch (*dwVal)
        {
        case 0:
            *dwVal = 3;
            return (FALSE);
        case 1:
            *dwVal = 2;
            return (FALSE);
        case 2:
            *dwVal = 0;
            /* we carry */
            return (TRUE);
        case 3:
            *dwVal = 1;
            return (FALSE);
        default:
            *dwVal = BAD_VAL;
            return (FALSE);
        }
    default:
        *dwVal = BAD_VAL;
        return (FALSE);
    }

    /* can never get here */
    return (FALSE);
}
#pragma warning(pop)


/*
 * get the next value in a simi random way.  This randomizes values by
 * counting differently.
 *
 * dwCurrent is the current value from which the next one is supposed
 * to be calculated.  dwNumBits is the number of bits in this value
 * that is valid.
 *
 * This returns the next number in the sequence.  These values will
 * overflow, and overflow is allowed to happen, much like standard alu
 * operations.
 */
DWORD
getNextMixed (DWORD dwCurrent, DWORD dwNumBits)
{
    if (!(dwNumBits <= 32))
    {
        IntErr (_T("getNextMixed: !(dwNumBits <= 32)"));
        return BAD_VAL;
    }

    /* our addition has four digits */
    DWORD dwMask = 0x3;

    /* where we currently are working in dwCurrent */
    DWORD dwCurrPlace = 0;

    /* a mask denoting which bits we want */
    DWORD dwBitsWeWantMask;

    if (dwNumBits == 32)
    {
        /*
       * have to work around the fact that left shift is undefined
       * for values >= 32 on DWORDs.  See C standard.
       */
        dwBitsWeWantMask = 0xffffffff;
    }
    else
    {
        dwBitsWeWantMask = ~(0xffffffff << dwNumBits);
    }

    while ((dwMask & dwBitsWeWantMask) != 0)
    {
        /* grab the two (or one) bits that we will be working with */
        DWORD dwSect = dwMask & dwBitsWeWantMask & dwCurrent;

        /* shift the bits to the ones spot */
        dwSect >>= dwCurrPlace;

        /*
       * the second arg is the bit mask denoting which bits we are adding
       * it will be either 1 or 3.
       */
        BOOL carry = mixedAddOne (&dwSect, (dwMask & dwBitsWeWantMask) >> dwCurrPlace);

        /* dwSect has been increased.  Put it back in the correct place */
        dwSect <<= dwCurrPlace;

        /* clear bits at this place */
        dwCurrent &= ~dwMask;

        /* set new bits at this place */
        dwCurrent |= dwSect;

        if (!carry)
    {
      /* all done */
      break;
    }

        /* update the vars used in the loop */
        dwMask <<= 2;
        dwCurrPlace += 2;
    }

    if ((dwMask & dwBitsWeWantMask) == 0)
    {
        /* we overflowed */
    }

    return (dwCurrent);
}
