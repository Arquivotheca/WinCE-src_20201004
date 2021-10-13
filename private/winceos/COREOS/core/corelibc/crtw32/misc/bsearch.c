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
*bsearch.c - do a binary search
*
*Purpose:
*   defines bsearch() - do a binary search an an array
*
*******************************************************************************/

#include <stdlib.h>
#include <internal.h>

/***
*char *bsearch() - do a binary search on an array
*
*Purpose:
*   Does a binary search of a sorted array for a key.
*
*Entry:
*   const char *key    - key to search for
*   const char *base   - base of sorted array to search
*   unsigned int num   - number of elements in array
*   unsigned int width - number of bytes per element
*   int (*compare)()   - pointer to function that compares two array
*           elements, returning neg when #1 < #2, pos when #1 > #2, and
*           0 when they are equal. Function is passed pointers to two
*           array elements.
*
*Exit:
*   if key is found:
*           returns pointer to occurrence of key in array
*   if key is not found:
*           returns NULL
*
*Exceptions:
*   Input parameters are validated. Refer to the validation section of the function. 
*
*******************************************************************************/

#ifdef __USE_CONTEXT
#define __COMPARE(context, p1, p2) (*compare)(context, p1, p2)
#else
#define __COMPARE(context, p1, p2) (*compare)(p1, p2)
#endif

_CRTIMP 
#ifdef __USE_CONTEXT
void * __cdecl bsearch_s (
    const void *key,
    const void *base,
    size_t num,
    size_t width,
    int (__cdecl *compare)(void *, const void *, const void *),
    void *context
    )
#else
void * __cdecl bsearch (
    const void *key,
    const void *base,
    size_t num,
    size_t width,
    int (__cdecl *compare)(const void *, const void *)
    )
#endif
{
    const char *hi = (const char *)base + (num - 1) * width;
    char *lo = (char *)base;
    char *mid;
    size_t half;
    int result;

    /* validation section */
    _VALIDATE_RETURN(base != NULL || num == 0, EINVAL, NULL);
    _VALIDATE_RETURN(width > 0, EINVAL, NULL);
    _VALIDATE_RETURN(compare != NULL, EINVAL, NULL);

    /* 
    We allow a NULL key here because it breaks some older code and because we do not dereference 
    this ourselves so we can't be sure that it's a problem for the comparison function 
    */

    while (lo <= hi)
    {
        if ((half = num / 2) != 0)
        {
            mid = lo + ((num & 1) ? half : (half - 1)) * width;
            result = __COMPARE(context, key, mid);
            if (!result)
            {
                return(mid);
            }
            else if (result < 0)
            {
                hi = mid - width;
                num = (num & 1) ? half : (half - 1);
            }
            else
            {
                lo = mid + width;
                num = half;
            }
        }
        else if (num)
        {
            return (__COMPARE(context, key, lo) ? NULL : lo);
        }
        else
        {
            break;
        }
    }

    return NULL;
}

#undef __COMPARE

