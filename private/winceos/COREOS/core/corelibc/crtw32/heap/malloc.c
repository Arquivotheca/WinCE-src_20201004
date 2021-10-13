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


void * __cdecl _nh_malloc (size_t size, int nhFlag, int zeroint) _THROW1(std::bad_alloc)
{
    HLOCAL hLocal;

    for(;;)
    {
        hLocal = LocalAlloc(zeroint, size);

        if (!hLocal && (nhFlag == 1))
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

_CRTIMP
_CRTNOALIAS
_CRTRESTRICT
__checkReturn
__bcount_opt(_Size)
void *
__cdecl
malloc(
    __in size_t _Size
    )
{
    return _nh_malloc(_Size, _newmode, 0);
}

_CRTIMP
_CRTNOALIAS
_CRTRESTRICT
__checkReturn
__bcount_opt(_NumOfElements*_SizeOfElements)
void *
__cdecl
calloc(
    __in size_t _NumOfElements,
    __in size_t _SizeOfElements
    )
{
    // Ensure that (size * num) does not overflow.

    if (_NumOfElements > 0)
    {
        _VALIDATE_RETURN((_HEAP_MAXREQ / _NumOfElements) >= _SizeOfElements, ENOMEM, NULL);
    }

    return _nh_malloc(_NumOfElements*_SizeOfElements, _newmode, LMEM_ZEROINIT);
}
