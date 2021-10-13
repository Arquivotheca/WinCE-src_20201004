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
*heapused.c - 
*
*   Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*
*Revision History:
*	12-13-93  SKS	Create _heapused() to return the number of bytes
*			in used malloc blocks, committed memory for the heap,
*			and reserved for the heap.  The bytes in malloc-ed
*			blocks includes the overhead of 4 bytes preceding
*			the entry and the 8 bytes in the descriptor list.
*	04-30-95  GJF	Spliced on winheap version (which is just a stub).
*
*******************************************************************************/

#include <cruntime.h>
#include <malloc.h>
#include <errno.h>

size_t __cdecl _heapused(
	size_t *pUsed,
	size_t *pCommit
	)
{
	errno = ENOSYS;
	return( 0 );
}
