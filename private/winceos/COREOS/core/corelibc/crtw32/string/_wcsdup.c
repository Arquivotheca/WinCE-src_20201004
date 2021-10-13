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

#include <windows.h>
#include <stdlib.h>

/***
*wchar_t *_wcsdup(string) - duplicate string into malloc'd memory
*
*Purpose:
*   Allocates enough storage via malloc() for a copy of the
*   string, copies the string into the new memory, and returns
*   a pointer to it (wide-character).
*
*Entry:
*   wchar_t *string - string to copy into new memory
*
*Exit:
*   returns a pointer to the newly allocated storage with the
*   string in it.
*
*   returns NULL if enough memory could not be allocated, or
*   string was NULL.
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

wchar_t * _wcsdup (const wchar_t * string)
{
    wchar_t *memory = NULL;
    size_t cb = 0;

    if (!string)
    {
        return NULL;
    }

    cb = (wcslen(string) + 1) * sizeof(wchar_t);

    memory = (wchar_t *)malloc(cb);
    if (memory != NULL)
    {
        memcpy(memory, string, cb);
    }

    return memory;
}

