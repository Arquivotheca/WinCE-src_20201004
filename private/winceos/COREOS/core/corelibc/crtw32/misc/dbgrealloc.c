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
*dbgrecalloc.c - Debug CRT Heap Function of recalloc()
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines debug versions of heap functions recalloc().
*
*Revision History:
*       02-13-07  WL    Module created. Moved from dbgheap.c
*
*******************************************************************************/

#ifdef  _DEBUG

#include <windows.h>
#include <winheap.h>
#include <ctype.h>
#include <dbgint.h>
#include <crtdbg.h>
#include <rtcsup.h>
#include <internal.h>
#include <limits.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <mtdll.h>
#include <setlocal.h>

#pragma warning(disable:4390)

/***
*void * realloc() - reallocate a block of memory in the heap
*
*Purpose:
*       Re-allocates a block in the heap to nNewSize bytes. nNewSize may be
*       either greater or less than the original size of the block. The
*       re-allocation may result in moving the block as well as changing
*       the size. If the block is moved, the contents of the original block
*       are copied over.
*
*       Re-allocates 'normal' memory block.
*
*Entry:
*       void *          pUserData   - pointer to previously allocated block
*       size_t          nNewSize    - requested size for the re-allocated block
*
*Exit:
*       Success:  Pointer to (user portion of) memory block
*       Failure:  NULL
*
*Exceptions:
*
*******************************************************************************/

extern "C" _CRTIMP void * __cdecl realloc(
        void * pUserData,
        size_t nNewSize
        )
{
        void *res = _realloc_dbg(pUserData, nNewSize, _NORMAL_BLOCK, NULL, 0);

        return res;
}

#endif  /* _DEBUG */
