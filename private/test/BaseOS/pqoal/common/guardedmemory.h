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
 * guardedMemory.h
 */

#ifndef GUARDED_MEMORY_H
#define GUARDED_MEMORY_H

/*
 * Use this to guard memory allocations.  Allows on to easily
 * determine if he/she is writing outside of a given memory allocation.
 */

#include <windows.h>

/*
 * behaves exactly like the standard malloc, except that the values
 * are guarded.  The alignment is the standard malloc alignment.
 */
void *
guardedMalloc (DWORD dwSize);

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
guardedMalloc (DWORD dwSize, void * origPtr);

/*
 * free a memory allocation performed by guardedAlloc.  The pointer
 * returned from guardAlloc and the allocation size are needed to
 * successfully free the pointer.
 *
 * returns true on success and false on failure.
 */  
BOOL
guardedFree (__bcount (dwSize) void * pMem, DWORD dwSize);

/*
 * Check the guard bytes for a given allocation.  pMem is the memory
 * pointer returned by guardedAlloc and dwSize is the size of the
 * allocation.
 *
 * Returns true on success and false otherwise.
 */
BOOL
guardedCheck (__bcount (dwSize) void * pMem, DWORD dwSize);

#endif /* GUARDED_MEMORY_H */
