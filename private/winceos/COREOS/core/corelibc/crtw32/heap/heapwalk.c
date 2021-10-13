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
*heapwalk.c - walk the heap
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	Defines the _heapwalk() function
*
*Revision History:
*	07-05-89  JCR	Module created.
*	11-13-89  GJF	Added MTHREAD support, also fixed copyright.
*	11-14-89  JCR	Fixed bug -- returned address was off by HDRSIZE
*	12-18-89  GJF	Removed DEBUG286 stuff, also some tuning, cleaned up
*			format a bit, changed header file name to heap.h, added
*			explicit _cdecl to function definition
*	12-20-89  GJF	Removed references to plastdesc
*	03-11-90  GJF	Replaced _cdecl with _CALLTYPE1, added #include
*			<cruntime.h> and removed #include <register.h>.
*	09-28-90  GJF	New-style function declarator.
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*	05-01-95  GJF	Spliced on winheap version.
*	05-11-95  GJF	Added code to detect end-of-heap.
*	05-26-95  GJF	Validate _pentry field with HeapValidate call.
*	06-14-95  GJF	Fix from Bryan Tuttle to avoid calling HeapValidate
*			on unallocated block. Also fixed error return.
*	06-14-95  GJF	Better version of above fix.
*	07-18-95  GJF	If the HeapWalk() API is not implemented, return
*	             	_HEAPEND instead of _HEAPOK.
*	10-10-03  AC	Added validation.
*
*******************************************************************************/

#include <cruntime.h>
#include <windows.h>
#include <errno.h>
#include <malloc.h>
#include <mtdll.h>
#include <stddef.h>
#include <stdlib.h>
#include <winheap.h>
#include <internal.h>

#ifndef _POSIX_

/***
*int _heapwalk() - Walk the heap
*
*Purpose:
*	Walk the heap returning information on one entry at a time.
*
*Entry:
*	struct _heapinfo {
*		int * _pentry;	heap entry pointer
*		size_t size;	size of heap entry
*		int _useflag;	free/inuse flag
*		} *entry;
*
*Exit:
*	Returns one of the following values:
*
*		_HEAPOK 	- completed okay
*		_HEAPEMPTY	- heap not initialized
*		_HEAPBADPTR	- _pentry pointer is bogus
*		_HEAPBADBEGIN	- can't find initial header info
*		_HEAPBADNODE	- malformed node somewhere
*		_HEAPEND	- end of heap successfully reached
*
*Uses:
*
*Exceptions:
*	Input parameters are validated. Refer to the validation section of the function. 
*
*******************************************************************************/

int __cdecl _heapwalk (
	struct _heapinfo *_entry
	)
{
	PROCESS_HEAP_ENTRY Entry;
	DWORD errval;
	int errflag;
	int retval = _HEAPOK;

	/* validation section */
	_VALIDATE_RETURN(_entry != NULL, EINVAL, _HEAPBADPTR);

	Entry.wFlags = 0;
	Entry.iRegionIndex = 0;
	if ( (Entry.lpData = _entry->_pentry) == NULL ) {
		if ( !HeapWalk( _crtheap, &Entry ) ) {
		return _HEAPBADBEGIN;
		}
	}
	else {
		if ( _entry->_useflag == _USEDENTRY ) {
			if ( !HeapValidate( _crtheap, 0, _entry->_pentry ) )
				return _HEAPBADNODE;
		Entry.wFlags = PROCESS_HEAP_ENTRY_BUSY;
		}
nextBlock:
		/*
		 * Guard the HeapWalk call in case we were passed a bad pointer
		 * to an allegedly free block.
		 */
		__try {
			errflag = 0;
			if ( !HeapWalk( _crtheap, &Entry ) )
				errflag = 1;
		}
		__except( GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ) {
			errflag = 2;
		}

		/*
		 * Check errflag to see how HeapWalk fared...
		 */
		if ( errflag == 1 ) {
			/*
			 * HeapWalk returned an error.
			 */
			if ( (errval = GetLastError()) == ERROR_NO_MORE_ITEMS ) {
			return _HEAPEND;
			}
			return _HEAPBADNODE;
		}
		else if ( errflag == 2 ) {
			/*
			 * Exception occurred during the HeapWalk!
			 */
			return _HEAPBADNODE;
		}
	}

	if ( Entry.wFlags & (PROCESS_HEAP_REGION |
		 PROCESS_HEAP_UNCOMMITTED_RANGE) )
	{
		goto nextBlock;
	}

	_entry->_pentry = Entry.lpData;
	_entry->_size = Entry.cbData;
	if ( Entry.wFlags & PROCESS_HEAP_ENTRY_BUSY ) {
		_entry->_useflag = _USEDENTRY;
	}
	else {
		_entry->_useflag = _FREEENTRY;
	}

	return( retval );
}

#endif  /* !_POSIX_ */
