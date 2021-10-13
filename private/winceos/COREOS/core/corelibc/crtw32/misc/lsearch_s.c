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
*lsearch_s.c - linear search of an array
*
*   Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*   contains the _lsearch_s() function - linear search of an array
*
*Revision History:
*   08-11-03  AC    Module created
*
*******************************************************************************/

/***
*char *_lsearch_s(key, base, num, width, compare, context) - do a linear search
*
*Purpose:
*   Performs a linear search on the array, looking for the value key
*   in an array of num elements of width bytes in size.  Returns
*   a pointer to the array value if found; otherwise adds the
*   key to the end of the list.
*
*Entry:
*   char *key - key to search for
*   char *base - base of array to search
*   unsigned *num - number of elements in array
*   int width - number of bytes in each array element
*   int (*compare)() - pointer to function that compares two
*       array values, returning 0 if they are equal and non-0
*       if they are different. Two pointers to array elements
*       are passed to this function, together with a pointer to
*       a context.
*   void *context - pointer to the context in which the function is
*       called. This context is passed to the comparison function.
*
*Exit:
*   if key found:
*       returns pointer to array element
*   if key not found:
*       adds the key to the end of the list, and increments
*       *num.
*       returns pointer to new element.
*
*Exceptions:
*   Input parameters are validated. Refer to the validation section of the function. 
*
*******************************************************************************/

#ifdef __USE_CONTEXT
#error __USE_CONTEXT should be undefined
#endif

#define __USE_CONTEXT
#include "lsearch.c"
