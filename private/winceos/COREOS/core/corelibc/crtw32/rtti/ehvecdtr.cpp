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
*ehvecdtr.cpp - EH-aware version of destructor iterator helper function
*
*Purpose:
*       These functions are called when constructing and destructing arrays of
*       objects.  Their purpose is to assure that constructed elements get
*       destructed if the constructor for one of the elements throws.
*       
*       Must be compiled using "-d1Binl" to be able to specify __thiscall
*       at the user level
*
****/

#include <ehdata.h>
#include <exception>
#include "rthelper.h"


void
HELPERFNAPI
__ehvec_dtor(
    _In_bytecount_x_(size*count) void *         ptr,            // Pointer to array to destruct
    _In_                        unsigned        size,           // Size of each element (including padding)
    _In_                        int             count,          // Number of elements in the array
    _In_                        void(CALLTYPE * pDtor)(void *)  // The destructor to call
    )
{
    // Advance pointer past end of array

    ptr = (char *)ptr + size*count;

    __try
    {
        // Destruct elements

        while (--count >= 0)
        {
            ptr = (char *)ptr - size;
            (*pDtor)(ptr);
        }
    }
    __finally
    {
        if (_abnormal_termination())
        {
            __ArrayUnwind(ptr, size, count, pDtor);
        }
    }
}


static
int
ArrayUnwindFilter(
    EXCEPTION_POINTERS const * pExPtrs
    )
{
    EHExceptionRecord const * pExcept = (EHExceptionRecord const *)pExPtrs->ExceptionRecord;

    switch (PER_CODE(pExcept))
    {
        case EH_EXCEPTION_NUMBER:
            std::terminate();
#ifdef ALLOW_UNWIND_ABORT
        case EH_ABORT_FRAME_UNWIND_PART:
            return EXCEPTION_EXECUTE_HANDLER;
#endif
        default:
            return EXCEPTION_CONTINUE_SEARCH;
    }
}


void
__ArrayUnwind(
    _In_bytecount_x_(size*count) void *         ptr,            // Pointer to array to destruct
    _In_                        unsigned        size,           // Size of each element (including padding)
    _In_                        int             count,          // Number of elements in the array
    _In_                        void(CALLTYPE * pDtor)(void *)  // The destructor to call
    )
{
    // 'unwind' rest of array

    __try
    {
        __analysis_assume(count >= 0);
        while (--count >= 0)
        {
            ptr = (char *)ptr - size;
            (*pDtor)(ptr);
        }
    }
    __except(ArrayUnwindFilter(exception_info()))
    {
    }
}

