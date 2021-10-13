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

void *realloc(void *OldPtr, size_t NewSize)
{
    HLOCAL hLocal;

    if (!OldPtr)
    {
        return malloc(NewSize);
    }

    if (!NewSize)
    {
        LocalFree(OldPtr);
        return NULL;
    }

    for(;;)
    {
        hLocal =  LocalReAlloc(OldPtr, NewSize, LMEM_MOVEABLE);

        if(!hLocal && _newmode == 1)
        {
            if(!_callnewh(NewSize))
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

void *_recalloc(void *OldPtr, size_t NewCount, size_t NewUnit)
{
    size_t NewSize;
    HLOCAL hLocal;

    /* ensure that (size * count) does not overflow */
    if (NewCount > 0)
    {
        _VALIDATE_RETURN((_HEAP_MAXREQ / NewCount) >= NewUnit, ENOMEM, NULL);
    }

    if (!OldPtr)
    {
        return calloc(NewCount, NewUnit);
    }

    NewSize = NewUnit * NewCount;

    if (!NewSize)
    {
        LocalFree(OldPtr);
        return NULL;
    }

    for(;;)
    {
        hLocal =  LocalReAlloc(OldPtr, NewSize, LMEM_MOVEABLE | LMEM_ZEROINIT);

        if(!hLocal && _newmode == 1)
        {
            if(!_callnewh(NewSize))
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

