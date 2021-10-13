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
/***
*wmemmove_s.c - contains wmemmove_s routine
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       wmemmove_s() copies a source memory buffer to a destination buffer.
*       Overlapping buffers are treated specially, to avoid propagation.
*
*Revision History:
*
*******************************************************************************/

#include <cruntime.h>
#include <wchar.h>
#include <internal.h>

/***
*wmemmove - Copy source buffer to destination buffer in units of wchar_t
*
*Purpose:
*       wmemmove() copies a source memory buffer to a destination memory buffer.
*       This routine recognize overlapping buffers to avoid propagation.
*
*       For cases where propagation is not a problem, wmemcpy_s() can be used.
*
*Entry:
*       wchar_t *dst = pointer to destination buffer
*       size_t sizeInWchar_ts = size in wchar_ts of the destination buffer
*       const wchar_t *src = pointer to source buffer
*       size_t count = number of wchar_ts to copy
*
*Exit:
*       Returns 0 if everything is ok, else return the error code.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*       On error, the error code is returned. Nothing is written to the destination buffer.
*
*******************************************************************************/

errno_t __cdecl wmemmove_s(
    wchar_t * dst,
    size_t sizeInWchar_ts,
    const wchar_t * src,
    size_t count
)
{
    if (count == 0)
    {
        /* nothing to do */
        return 0;
    }

    /* validation section */
    _VALIDATE_RETURN_ERRCODE(dst != NULL, EINVAL);
    _VALIDATE_RETURN_ERRCODE(src != NULL, EINVAL);
    _VALIDATE_RETURN_ERRCODE(sizeInWchar_ts >= count, ERANGE);

    wmemmove(dst, src, count);
    return 0;
}
