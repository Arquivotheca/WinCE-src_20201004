//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>

extern _PVFV *__onexitbegin;
extern _PVFV *__onexitend;

#define ONEXITTBLINCR   4

_onexit_t _onexit(_onexit_t func) {
    _PVFV   *p;
    // First, make sure the table has room for a new entry
    if (!__onexitbegin || (LocalSize(__onexitbegin) < ((unsigned)__onexitend - (unsigned)__onexitbegin + sizeof(_PVFV)))) {
        // not enough room, try to grow the table
        if (!__onexitbegin)
        {
            if (!(p = (_PVFV *)LocalAlloc(0,ONEXITTBLINCR * sizeof(_PVFV))))
                return NULL;
        }
        else if (!(p = (_PVFV *)LocalReAlloc(__onexitbegin, LocalSize(__onexitbegin) + ONEXITTBLINCR * sizeof(_PVFV), 2)))
          return NULL;
        __onexitend = p + (__onexitend - __onexitbegin);
        __onexitbegin = p;
    }
    *(__onexitend++) = (_PVFV)func;
    return func;
}

int atexit(_PVFV func) {
    return (_onexit((_onexit_t)func) ? 0 : -1);
}

