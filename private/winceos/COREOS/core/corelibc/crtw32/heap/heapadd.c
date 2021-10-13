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
*heapadd.c - Add a block of memory to the heap
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	Add a block of memory to the heap.
*
*Revision History:
*	07-07-89  JCR	Module created.
*	07-20-89  JCR	Re-use dummy descriptor on exact fit (dummy collection)
*	11-09-89  JCR	Corrected plastdesc updating code
*	11-13-89  GJF	Added MTHREAD support, also fixed copyright
*	11-15-89  JCR	Minor improvement (got rid of a local variable)
*	11-16-89  JCR	Update - squirrly case in _HEAPFIND_EXACT
*	12-04-89  GJF	A little tuning and cleanup. Also, changed header file
*			name to heap.h.
*	12-18-89  GJF	Removed DEBUG286 stuff. Also, added explicit _cdecl to
*			function definitions.
*	12-19-89  GJF	Removed references and uses of plastdesc (revising
*			code as necessary)
*	03-09-90  GJF	Replaced _cdecl with _CALLTYPE1, added #include
*			<cruntime.h> and removed #include <register.h>.
*	03-29-90  GJF	Made _before() _CALLTYPE4.
*	07-24-90  SBM	Compiles cleanly with -W3 (tentatively removed
*			unreferenced label)
*	09-27-90  GJF	New-style function declarators.
*	03-05-91  GJF	Changed strategy for rover - old version available
*			by #define-ing _OLDROVER_.
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*	12-10-93  GJF	Test alignment of user's pointer and block size
*			against _GRANULARITY.
*	01-03-94  SKS	Fix bug where sentinel gets out of sync with dummy
*			blocks and large allocations.  _heapmin was likely
*			to cause the situation that showed the bug.
*	03-03-94  GJF	Revised to provide for graceful failure in the event
*			there aren't enough empty descriptors.
*	02-08-95  GJF	Removed obsolete _OLDROVER_ support.
*	04-29-95  GJF	Spliced on winheap version.
*
*******************************************************************************/

#include <cruntime.h>
#include <errno.h>
#include <malloc.h>
#include <winheap.h>

int __cdecl _heapadd (
	void * block,
	size_t size
	)
{
	errno = ENOSYS;
	return(-1);
}
