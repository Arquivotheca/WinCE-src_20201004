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
*
****/

#include "rthelper.h"


void
HELPERFNAPI
__ehvec_ctor_vb(
    _In_bytecount_x_(size*count) void *         ptr,            // Pointer to array to destruct
    _In_                        unsigned        size,           // Size of each element (including padding)
    _In_                        int             count,          // Number of elements in the array
    _In_                        void(CALLTYPE * pCtor)(void *), // Constructor to call
    _In_                        void(CALLTYPE * pDtor)(void *)  // Destructor to call should exception be thrown
    )
{
    int i = 0;                      // Count of elements constructed

    __try
    {
        // Construct the elements of the array

        for(; i < count; i++)
        {
            (*(void(CALLTYPE *)(void *, int))pCtor)(ptr, 1);
            ptr = (char *)ptr + size;
        }
    }
    __finally
    {
        if (_abnormal_termination())
        {
            __ArrayUnwind(ptr, size, i, pDtor);
        }
    }
}

