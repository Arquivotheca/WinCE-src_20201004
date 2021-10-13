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
 * guardedMemory.cpp
 */

#include "guardedMemory.h"

/* for IntErr routines (and the like) */
#include "commonUtils.h"

#define ALIGNMENT_MASK ((byte) 3)
#define GUARD_VAL ((byte) 0xCC)

/*
 * behaves exactly like the standard malloc, except that the values
 * are guarded.  The alignment is the standard malloc alignment.
 */
void *
guardedMalloc (DWORD dwSize)
{
    return (guardedMalloc (dwSize, NULL));
}

/*
 * Do a malloc, except put guard values on the beginning and end of
 * the allocation.
 *
 * This function places the allocation on the alignment dictated by
 * origPtr.  The number of valid bits in this value is given by
 * ALIGNMENT_MASK.  Bytes are used for guard values, and these are put
 * immediately before and immediately after the data.
 *
 * This function behaves exactly like malloc.  dwSize = 0 is supported.
 *
 * return value is a pointer to the data (not the guard values; these
 * are hidden from the user) or NULL if it could not be successully
 * allocated.
 */
void *
guardedMalloc (DWORD dwSize, void * origPtr)
{

    /*
     * This is an error value as well as the allocation.  Assume it
     * failed until proven otherwise (NULL means failure)
     */
    byte * pFirstByte = NULL;

    /* the alignment that we want to set the memory on */
    DWORD dwAlignment = (DWORD)origPtr & ALIGNMENT_MASK;

    /* overflow check */
    if (DWORD_MAX - (2 * sizeof (DWORD)) < dwSize)
    {
        IntErr (_T("Overflow for array size."));
        goto cleanAndReturn;
    }

    /*
     * malloc the memory, plus the guard bytes.  Use 8 because all
     * mallocs should be on DWORD boundaries anyhow.  This gives us two
     * too much but will work fine.
     *
     * This is guaranteed to be aligned on at least a DWORD.
     */
    byte * pGuardAlloc = (byte *) malloc (dwSize + (2 * sizeof (DWORD)) );

    if (pGuardAlloc == NULL)
    {
        goto cleanAndReturn;
    }

    if (dwAlignment == 0)
    {
        pFirstByte = pGuardAlloc + sizeof (DWORD);
    }
    else
    {
        pFirstByte = (byte *) ((((DWORD) pGuardAlloc) & ~dwAlignment) | dwAlignment);
    }

    /* set the guard value at the beginning */
    *(pFirstByte - 1) = GUARD_VAL;

    /* set the ending guard value */
    *(pFirstByte + dwSize) = GUARD_VAL;

cleanAndReturn:
    return (pFirstByte);
}

/*
 * free a memory allocation performed by guardedAlloc.  The pointer
 * returned from guardAlloc and the allocation size are needed to
 * successfully free the pointer.
 *
 * returns true on success and false on failure.
 */
BOOL
guardedFree (__bcount (dwSize) void * pMem, DWORD dwSize)
{
    BOOL returnVal = TRUE;

    /* allow caller to free null, like free does */
    if (pMem == NULL)
    {
        goto cleanAndReturn;
    }

    /* grab the first non-guard byte */
    byte * pFirstByte = (byte *) pMem;

    /* pointer returned from the original malloc */
    byte * pAllocPtr = NULL;

    /* find the ptr used for the allocation */
    if ((((DWORD) pFirstByte) & ALIGNMENT_MASK) == 0)
    {
        /* allocation was one DWORD behind */
        pAllocPtr = pFirstByte - sizeof (DWORD);
    }
    else
    {
        pAllocPtr = (byte *) (((DWORD) pFirstByte) & ~ALIGNMENT_MASK);
    }

    /* check beginning guard byte */
    if (*(pFirstByte - 1) != GUARD_VAL)
    {
        Error (_T("Beginning guard value at ptr %u is %u instead of %u."),
             (DWORD) (pFirstByte - 1), (DWORD) (*(pFirstByte - 1)),
             GUARD_VAL);
        returnVal = FALSE;
    }

    /* check end guard byte */
    // original allocation had extra bytes, so this is safe
    PREFAST_SUPPRESS(26000, TEXT("Allocation has 2 extra bytes on either end\r\n"));
    if (*(pFirstByte + dwSize) != GUARD_VAL)
    {
        Error (_T("Ending guard value at ptr %u is %u instead of %u."),
         (DWORD) (pFirstByte + dwSize), (DWORD) (*(pFirstByte + dwSize)),
         GUARD_VAL);
        returnVal = FALSE;
    }

    /* free memory, even if we had an error */
    if(pAllocPtr) free (pAllocPtr);

cleanAndReturn:

    return (returnVal);
}

/*
 * Check the guard bytes for a given allocation.  pMem is the memory
 * pointer returned by guardedAlloc and dwSize is the size of the
 * allocation.
 *
 * Returns true on success and false otherwise.
 */
BOOL
guardedCheck (__bcount (dwSize) void * pMem, DWORD dwSize)
{
    BOOL returnVal = TRUE;

    /* a check to null succeeds */
    if (pMem == NULL)
    {
        goto cleanAndReturn;
    }

    /* grab the first non-guard byte */
    const byte * pFirstByte = (byte *) pMem;

    /* check beginning guard byte */
    if (*(pFirstByte - 1) != GUARD_VAL)
    {
        Error (_T("Beginning guard value at ptr %u is %u instead of %u."),
         (DWORD) (pFirstByte - 1), (DWORD) (*(pFirstByte - 1)),
         GUARD_VAL);
        returnVal = FALSE;
    }

    /* check end guard byte */
    // original allocation had extra bytes, so this is safe
    PREFAST_SUPPRESS(26000, TEXT("Allocation has 2 extra bytes on either end\r\n"));
    if (*(pFirstByte + dwSize) != GUARD_VAL)
    {
        Error (_T("Ending guard value at ptr %u is %u instead of %u."),
         (DWORD) (pFirstByte + dwSize), (DWORD) (*(pFirstByte + dwSize)),
         GUARD_VAL);
        returnVal = FALSE;
        }

cleanAndReturn:

    return (returnVal);
}
