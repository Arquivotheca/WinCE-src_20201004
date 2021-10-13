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
#include <corecrt.h>
#include <internal.h>
#include <malloc.h>

_CRTIMP
_CRTNOALIAS
_CRTRESTRICT
__checkReturn
__bcount_opt(_NewSize)
void *
__cdecl
realloc(
    __in_opt void * _Memory,
    __in size_t _NewSize
    )
{
    HLOCAL hLocal;

    if (!_Memory)
    {
        return malloc(_NewSize);
    }

    if (!_NewSize)
    {
        LocalFree(_Memory);
        return NULL;
    }

    for(;;)
    {
        hLocal = LocalReAlloc(_Memory, _NewSize, LMEM_MOVEABLE);

        if (!hLocal && (_newmode == 1))
        {
            if (!_callnewh(_NewSize))
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


_CRTIMP
_CRTNOALIAS
_CRTRESTRICT
__checkReturn
__bcount_opt(_Size*_Count)
void *
__cdecl
_recalloc(
    __in_opt void * _Memory,
    __in size_t _Count,
    __in size_t _Size
    )
{
    size_t NewSize;
    HLOCAL hLocal;

    /* ensure that (size * count) does not overflow */
    if (_Count > 0)
    {
        _VALIDATE_RETURN((_HEAP_MAXREQ / _Count) >= _Size, ENOMEM, NULL);
    }

    if (!_Memory)
    {
        return calloc(_Count, _Size);
    }

    NewSize = _Size * _Count;

    if (!NewSize)
    {
        LocalFree(_Memory);
        return NULL;
    }

    for(;;)
    {
        hLocal =  LocalReAlloc(_Memory, NewSize, LMEM_MOVEABLE | LMEM_ZEROINIT);

        if (!hLocal && (_newmode == 1))
        {
            if (!_callnewh(NewSize))
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

