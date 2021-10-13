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

#include <corecrt.h>
#include <internal.h>
#include "__ascii.h"

/***
*strtol, strtoul(nptr,endptr,ibase) - Convert ascii string to long un/signed
*       int.
*
*Purpose:
*       Convert an ascii string to a long 32-bit value.  The base
*       used for the caculations is supplied by the caller.  The base
*       must be in the range 0, 2-36.  If a base of 0 is supplied, the
*       ascii string must be examined to determine the base of the
*       number:
*               (a) First char = '0', second char = 'x' or 'X',
*                   use base 16.
*               (b) First char = '0', use base 8
*               (c) First char in range '1' - '9', use base 10.
*
*       If the 'endptr' value is non-NULL, then strtol/strtoul places
*       a pointer to the terminating character in this value.
*       See ANSI standard for details
*
*Entry:
*       nptr == NEAR/FAR pointer to the start of string.
*       endptr == NEAR/FAR pointer to the end of the string.
*       ibase == integer base to use for the calculations.
*
*       string format: [whitespace] [sign] [0] [x] [digits/letters]
*
*Exit:
*       Good return:
*               result
*
*       Overflow return:
*               strtol -- LONG_MAX or LONG_MIN
*               strtoul -- ULONG_MAX
*               strtol/strtoul -- errno == ERANGE
*
*       No digits or bad base return:
*               0
*               endptr = nptr*
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function. 
*
*******************************************************************************/

/* flag values */
#define FL_UNSIGNED   1       /* strtoul called */
#define FL_NEG        2       /* negative sign found */
#define FL_OVERFLOW   4       /* overflow occured */
#define FL_READDIGIT  8       /* we've read at least one correct digit */


static unsigned long __cdecl strtoxl (
        const char *nptr,
        const char **endptr,
        int ibase,
        int flags
        )
{
        const char *p;
        char c;
        unsigned long number;
        unsigned digval;
        unsigned long maxval;

        /* validation section */
        if (endptr != NULL)
        {
            /* store beginning of string in endptr */
            *endptr = nptr;
        }
        _VALIDATE_RETURN(nptr != NULL, EINVAL, 0L);
        _VALIDATE_RETURN(ibase == 0 || (2 <= ibase && ibase <= 36), EINVAL, 0L);

        p = nptr;                       /* p is our scanning pointer */
        number = 0;                     /* start with zero */

        c = *p++;                       /* read char */
        while ( __ascii_isspace(c) )
            c = *p++;               /* skip whitespace */

        if (c == '-') {
            flags |= FL_NEG;        /* remember minus sign */
            c = *p++;
        }
        else if (c == '+')
            c = *p++;               /* skip sign */

        if (ibase == 0) {
            /* determine base free-lance, based on first two chars of
               string */
            if (c != '0')
                ibase = 10;
            else if (*p == 'x' || *p == 'X')
                ibase = 16;
            else
                ibase = 8;
        }

        if (ibase == 16) {
            /* we might have 0x in front of number; remove if there */
            if (c == '0' && (*p == 'x' || *p == 'X')) {
                ++p;
                c = *p++;       /* advance past prefix */
            }
        }

        /* if our number exceeds this, we will overflow on multiply */
        maxval = ULONG_MAX / ibase;


        for (;;) {      /* exit in middle of loop */
            /* convert c to value */
            if ( __ascii_isdigit(c) )
                digval = c - '0';
            else if ( __ascii_isalpha(c) )
                digval = __ascii_toupper(c) - 'A' + 10;
            else
                break;
            if (digval >= (unsigned)ibase)
                break;          /* exit loop if bad digit found */

            /* record the fact we have read one digit */
            flags |= FL_READDIGIT;

            /* we now need to compute number = number * base + digval,
               but we need to know if overflow occured.  This requires
               a tricky pre-check. */

            if (number < maxval || (number == maxval &&
                        (unsigned long)digval <= ULONG_MAX % ibase)) {
                /* we won't overflow, go ahead and multiply */
                number = number * ibase + digval;
            }
            else {
                /* we would have overflowed -- set the overflow flag */
                flags |= FL_OVERFLOW;
                if (endptr == NULL) {
                    /* no need to keep on parsing if we
                       don't have to return the endptr. */
                    break;
                }
            }

            c = *p++;               /* read next digit */
        }

        --p;                            /* point to place that stopped scan */

        if (!(flags & FL_READDIGIT)) {
            /* no number there; return 0 and point to beginning of
               string */
            if (endptr)
                /* store beginning of string in endptr later on */
                p = nptr;
            number = 0L;            /* return 0 */
        }
        else if ( (flags & FL_OVERFLOW) ||
                ( !(flags & FL_UNSIGNED) &&
                  ( ( (flags & FL_NEG) && (number > -LONG_MIN) ) ||
                    ( !(flags & FL_NEG) && (number > LONG_MAX) ) ) ) )
        {
            /* overflow or signed overflow occurred */
            //errno = ERANGE;
            if ( flags & FL_UNSIGNED )
                number = ULONG_MAX;
            else if ( flags & FL_NEG )
                number = (unsigned long)(-LONG_MIN);
            else
                number = LONG_MAX;
        }

        if (endptr != NULL)
            /* store pointer to char that stopped the scan */
            *endptr = p;

        if (flags & FL_NEG)
            /* negate result if there was a neg sign */
            number = (unsigned long)(-(long)number);

        return number;                  /* done. */
}

long __cdecl strtol (
        const char *nptr,
        char **endptr,
        int ibase
        )
{
    return (long) strtoxl(nptr, (const char **)endptr, ibase, 0);
}

unsigned long __cdecl strtoul (
        const char *nptr,
        char **endptr,
        int ibase
        )
{
    return strtoxl(nptr, (const char **)endptr, ibase, FL_UNSIGNED);
}
