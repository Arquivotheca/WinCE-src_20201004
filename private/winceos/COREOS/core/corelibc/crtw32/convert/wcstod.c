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

int _wchartodigit(wchar_t);

_CRTIMP __checkReturn double __cdecl wcstod(__in_z const wchar_t * nptr, __deref_opt_out_z wchar_t ** endptr)
{
    double tmp;
    const wchar_t *ptr = nptr;
    char *cptr;
    size_t len;
    _LDBL12 ld12;
    const char *EndPtr;
    DWORD flags;
    INTRNCVT_STATUS intrncvt;
    size_t cch;
    char buffer[16];

    /* validation section */
    if (endptr != NULL)
    {
        /* store beginning of string in endptr */
        *endptr = (wchar_t*)nptr;
    }
    _VALIDATE_RETURN(nptr != NULL, EINVAL, 0.0);

    /* scan past leading space/tab characters */
    while (iswspace(*ptr))
    {
        ptr++;
    }

    cch = wcslen(ptr);
    if (cch < sizeof(buffer))
    {
        cptr = buffer;
    }
    else
    {
        cptr = (char*)malloc(cch + 1);
        if (!cptr)
        {
            return 0.0;
        }
    }

    for (len = 0; len < cch; len++)
    {
        wchar_t ch = ptr[len];
        int digit = _wchartodigit(ch);
        if (digit >= 0)
        {
            cptr[len] = (char)(digit + '0');
        }
        else if (ch < 128)
        {
            cptr[len] = (char)ch;
        }
        else
        {
            break;
        }
    }

    cptr[len] = 0;
    flags = __strgtold12(&ld12, &EndPtr, cptr);

    if (cptr != buffer)
    {
        free(cptr);
    }

    if (flags & SLD_NODIGITS)
    {
        if (endptr)
        {
            *endptr = (wchar_t *)nptr;
        }

        return 0.0;
    }

    if (endptr)
    {
        *endptr = (wchar_t *)ptr + (EndPtr - cptr);
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

