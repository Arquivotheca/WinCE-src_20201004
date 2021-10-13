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
*recalloc.c - implementation of _recalloc
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the _recalloc()
*
*Revision History:
*       02-13-07  WL    Module created. Moved from realloc.c
*
*******************************************************************************/

#include <cruntime.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <winheap.h>
#include <windows.h>
#include <internal.h>
#include <mtdll.h>
#include <dbgint.h>
#include <rtcsup.h>

void * __cdecl _realloc_base (void * pBlock, size_t newsize);

/***
*void *_recalloc_base(pblock, count, newsize) - reallocate a block of memory in the heap
*
*Purpose:
*       Reallocates a block in the heap to count*newsize bytes. newsize may be
*       either greater or less than the original size of the block. The
*       reallocation may result in moving the block as well as changing
*       the size. If the block is moved, the contents of the original block
*       are copied over.
*
*Entry:
*       void *pblock    - pointer to block in the heap previously allocated
*                         by a call to malloc(), realloc() or _expand().
*
*       size_t count    - count of items
*       size_t newsize  - requested size for the re-allocated items
*
*Exit:
*       Success:  Pointer to the re-allocated memory block
*       Failure:  NULL
*
*Uses:
*
*Exceptions:
*       If pblock does not point to a valid allocation block in the heap,
*       realloc() will behave unpredictably and probably corrupt the heap.
*
*******************************************************************************/

void * __cdecl _recalloc_base(void * memblock, size_t count, size_t size)
{
    void * retp = NULL;
    size_t  size_orig = 0, old_size = 0;

    /* ensure that (size * count) does not overflow */
    if (count > 0)
    {
        _VALIDATE_RETURN_NOEXC((_HEAP_MAXREQ / count) >= size, ENOMEM, NULL);
    }
    size_orig = size * count;
    if (memblock != NULL)
        old_size = _msize(memblock);
    retp = _realloc_base(memblock, size_orig);
    if (retp != NULL && old_size < size_orig)
    {
        memset ((char*)retp + old_size, 0, size_orig - old_size);
    }
    return retp;
}
