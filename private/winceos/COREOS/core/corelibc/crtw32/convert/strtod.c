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
#define _CRTIMP_DATA
#include <corecrt.h>
#include <internal.h>
#include "__ascii.h"

_CRTIMP __checkReturn double __cdecl strtod(__in_z const char * nptr, __deref_opt_out_z char ** endptr)
{
    double tmp;
    const char *ptr = nptr;
    _LDBL12 ld12;
    const char *EndPtr;
    DWORD flags;
    INTRNCVT_STATUS intrncvt;

    /* validation section */
    if (endptr != NULL)
    {
        *endptr = (char*)nptr;
    }
    _VALIDATE_RETURN(nptr != NULL, EINVAL, 0.0);

    /* scan past leading space/tab characters */
    while (__ascii_isspace(*ptr))
    {
        ptr++;
    }

    flags = __strgtold12(&ld12, &EndPtr, ptr);
    if (flags & SLD_NODIGITS)
    {
        if (endptr)
        {
            *endptr = (char *)nptr;
        }

        return 0.0;
    }
    if (endptr)
    {
        *endptr = (char *)EndPtr;
    }

    intrncvt = _ld12tod(&ld12, &tmp);
    if (flags & SLD_OVERFLOW  || intrncvt == INTRNCVT_OVERFLOW)
    {
        return (*ptr == '-') ? -HUGE_VAL : HUGE_VAL; /* negative or positive overflow */
    }

    if (flags & SLD_UNDERFLOW || intrncvt == INTRNCVT_UNDERFLOW)
    {
        return 0.0; /* underflow */
    }

    return tmp;
}

