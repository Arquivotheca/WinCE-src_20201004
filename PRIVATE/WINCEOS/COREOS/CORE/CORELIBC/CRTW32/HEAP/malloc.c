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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <corecrt.h>
#include <internal.h>
#include <malloc.h>


void * __cdecl _nh_malloc (size_t size, int nhFlag, int zeroint)
{
    HLOCAL hLocal;

    for(;;)
    {
        hLocal = LocalAlloc(zeroint, size);

        if (!hLocal && nhFlag == 1)
        {
            if (!_callnewh(size))
            {
                return NULL;
            }
        }
        else
        {
            return hLocal;
        }
    }
}

void *malloc(size_t size)
{
    return _nh_malloc(size, _newmode, 0);
}

void *calloc(size_t num, size_t size)
{
    /* ensure that (size * num) does not overflow */
    if (num > 0)
    {
        _VALIDATE_RETURN((_HEAP_MAXREQ / num) >= size, ENOMEM, NULL);
    }

    return _nh_malloc(num*size, _newmode,  LMEM_ZEROINIT);
}


