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
*ehveccvb.cpp - EH c-tor iterator helper function for class w/ virtual bases
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       EH-aware version of constructor iterator helper function for class
*       with virtual bases
*
*       These functions are called when constructing and destructing arrays of
*       objects.  Their purpose is to assure that constructed elements get
*       destructed if the constructor for one of the elements throws.
*
*       These functions are called when constructing and destructing arrays of
*       objects.  Their purpose is to assure that constructed elements get
*       destructed if the constructor for one of the elements throws.
*
*       Must be compiled using "-d1Binl" to be able to specify __thiscall
*       at the user level
****/

#include <cruntime.h>
#include <eh.h>

#if defined (_M_CEE)
#define CALLTYPE __clrcall
#define CALEETYPE __clrcall
#define __RELIABILITY_CONTRACT \
    [System::Runtime::ConstrainedExecution::ReliabilityContract( \
        System::Runtime::ConstrainedExecution::Consistency::WillNotCorruptState, \
        System::Runtime::ConstrainedExecution::Cer::Success)]
#else
#define CALEETYPE __stdcall
#define __RELIABILITY_CONTRACT
#if defined (_M_IX86)
#define CALLTYPE __thiscall
#else
#define CALLTYPE __stdcall
#endif
#endif


#ifdef _WIN32
#ifndef _WIN32_WCE
#if defined (_M_CEE)
__RELIABILITY_CONTRACT
void __ArrayUnwind(
#else
void __stdcall __ArrayUnwind(
#endif
    void*       ptr,                // Pointer to array to destruct
    size_t      size,               // Size of each element (including padding)
    int         count,              // Number of elements in the array
    void(CALLTYPE *pDtor)(void*)    // The destructor to call
);
#else
#undef CALLTYPE
#include "rthelper.h"
#endif

__RELIABILITY_CONTRACT
void CALEETYPE __ehvec_ctor_vb(
    void*       ptr,                // Pointer to array to destruct
    size_t      size,               // Size of each element (including padding)
    int         count,              // Number of elements in the array
    void(CALLTYPE *pCtor)(void*),   // Constructor to call
    void(CALLTYPE *pDtor)(void*)    // Destructor to call should exception be thrown
){
    int i;  // Count of elements constructed
    int success = 0;

    __try
    {
        // Construct the elements of the array
        for( i = 0;  i < count;  i++ )
        {

#pragma warning(disable:4191)

            (*(void(CALLTYPE*)(void*,int))pCtor)( ptr, 1 );

#pragma warning(default:4191)

            ptr = (char*)ptr + size;
        }
        success = 1;
    }
    __finally
    {
        if (!success)
            __ArrayUnwind(ptr, size, i, pDtor);
    }
}

#else

__RELIABILITY_CONTRACT
void CALEELTYPE __ehvec_ctor_vb(
    void*       ptr,                // Pointer to array to destruct
    unsigned    size,               // Size of each element (including padding)
    int         count,              // Number of elements in the array
    void(CALLTYPE *pCtor)(void*),   // Constructor to call
    void(CALLTYPE *pDtor)(void*)    // Destructor to call should exception be thrown
){
    int i;  // Count of elements constructed

    try
    {
        // Construct the elements of the array
        for( i = 0;  i < count;  i++ )
        {
            (*(void(CALLTYPE*)(void*,int))pCtor)( ptr, 1 );
            ptr = (char*)ptr + size;
        }
    }
    catch(...)
    {
        // If a constructor throws, unwind the portion of the array thus
        // far constructed.
        for( i--;  i >= 0;  i-- )
        {
            ptr = (char*)ptr - size;
            try {
                (*pDtor)(ptr);
            }
            catch(...) {
                // If the destructor threw during the unwind, quit
                terminate();
            }
        }

        // Propagate the exception to surrounding frames
        throw;
    }
}

#endif
