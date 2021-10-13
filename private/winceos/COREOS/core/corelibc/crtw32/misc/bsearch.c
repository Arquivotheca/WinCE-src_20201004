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
*   Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*   defines bsearch() - do a binary search an an array
*
*Revision History:
*       07-05-84  RN    initial version
*       06-19-85  TC    put in ifdefs to handle case of multiplication
*                       in large/huge model.
*       04-13-87  JCR   added const to declaration
*       08-04-87  JCR   Added "long" cast to mid= assignment for large/huge
*                       model.
*       11-10-87  SKS   Removed IBMC20 switch
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       01-21-88  JCR   Backed out _LOAD_DS...
*       02-22-88  JCR   Added cast to get rid of cl const warning
*       10-20-89  JCR   Added _cdecl to prototype, changed 'char' to 'void'
*       03-14-90  GJF   Replaced _cdecl with _CALLTYPE1, added #include
*                       <cruntime.h>, removed #include <register.h> and fixed
*                       the copyright. Also, cleaned up the formatting a bit.
*       04-05-90  GJF   Added #include <stdlib.h> and #include <search.h>.
*                       Fixed some resulting compiler warnings (at -W3).
*                       Also, removed #include <sizeptr.h>.
*       07-25-90  SBM   Removed redundant include (stdio.h), made args match
*                       prototype
*       10-04-90  GJF   New-style function declarator.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       12-18-98  GJF   Changes for 64-bit size_t.
*       08-11-03  AC    Added safe version (bsearch_s)
*       10-31-04  MSL   Enable __clrcall safe version
*       05-13-05  MSL   Allow NULL key to help SAP
*
*******************************************************************************/

#include <cruntime.h>
#include <stdlib.h>
#include <search.h>
#include <internal.h>

#if defined(_M_CEE)
#define __fileDECL  __clrcall
#else
#define __fileDECL  __cdecl
#endif
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

#if !defined(_M_CEE)
_CRTIMP 
#endif

SECURITYSAFECRITICAL_ATTRIBUTE
#ifdef __USE_CONTEXT
void * __fileDECL bsearch_s (
    const void *key,
    const void *base,
    size_t num,
    size_t width,
    int (__fileDECL *compare)(void *, const void *, const void *),
    void *context
    )
#else
void * __fileDECL bsearch (
    const void *key,
    const void *base,
    size_t num,
    size_t width,
    int (__fileDECL *compare)(const void *, const void *)
    )
#endif
{
    char *lo = (char *)base;
    char *hi = (char *)base + (num - 1) * width;
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
            mid = lo + (num & 1 ? half : (half - 1)) * width;
            if (!(result = __COMPARE(context, key, mid)))
                return(mid);
            else if (result < 0)
            {
                hi = mid - width;
                num = num & 1 ? half : half-1;
            }
            else
            {
                lo = mid + width;
                num = half;
            }
        }
        else if (num)
            return (__COMPARE(context, key, lo) ? NULL : lo);
        else
            break;
    }

    return NULL;
}

#undef __fileDECL
#undef __COMPARE
