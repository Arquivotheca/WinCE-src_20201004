//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>
#include <internal.h>



void * __cdecl _nh_malloc (size_t size, int nhFlag, int zeroint)
{
    HLOCAL hLocal;

    for(;;)
    {
        hLocal = LocalAlloc(zeroint, size);

        if(!hLocal && nhFlag == 1)
        {
            if(!_callnewh(size))
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
    return _nh_malloc(num*size, _newmode,  LMEM_ZEROINIT);
}


