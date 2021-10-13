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
*free.c - free an entry in the heap
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the following functions:
*           freea()     - free a memory block in the heap
*           This has been moved to inline, but we need this for backwards bin compat
*
*Revision History:
*       05-11-05  MSL   Created
*
*******************************************************************************/

#define _CRT_NOFREEA
#include <malloc.h>
#include <stdlib.h>

/***
*void _freea(pblock) - 
*
*Purpose:
*       Frees only block allocated on the heap, and not the ones
*       allocated on the stack.
*       See the implementation of _malloca
*
*******************************************************************************/

void __cdecl _freea(void *ptr)
{
    if (ptr != NULL)
    {
        ptr = (char*)ptr - _ALLOCA_S_MARKER_SIZE;
        if (*((unsigned int*)ptr) == _ALLOCA_S_HEAP_MARKER)
        {
            free(ptr);
        }
    }
}

