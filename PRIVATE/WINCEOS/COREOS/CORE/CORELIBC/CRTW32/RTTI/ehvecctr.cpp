//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*ehvecctr.cpp - EH-aware version of constructor iterator helper function
*
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
#include "kernel.h"
#include "excpt.h"
#include "corecrt.h"
#include <exception>
#include <coredll.h>

/* The compiler expects and demands a particular calling convention
 *  for these helper functions.  The x86 compiler demands __stdcall.
 *  Windows CE RISC compilers demand __cdecl, which is the CE standard.
 *
 * The calling convention of the ctors and dtors also differs between
 *  x86 and RISC.  For x86, the type is specified with the __thiscall
 *  keyword, available only when compiling with "-d1Binl" (see comment
 *  above.)  For RISC, the convention should be __cdecl.
 */
#undef __stdcall                            // #define'd in windef.h for CE

#ifdef _M_IX86
#define HELPERFNAPI __stdcall
#define CALLTYPE __thiscall
#else
#define HELPERFNAPI __cdecl
#define CALLTYPE __cdecl
#endif

void __ArrayUnwind(
        void*           ptr,            // Pointer to array to destruct
        unsigned        size,           // Size of each element (including padding)
        int             count,          // Number of elements in the array
        void(CALLTYPE *pDtor)(void*)    // The destructor to call
        );


void HELPERFNAPI __ehvec_ctor(
        void*           ptr,            // Pointer to array to destruct
        unsigned        size,           // Size of each element (including padding)
        int             count,          // Number of elements in the array
        void(CALLTYPE *pCtor)(void*),   // Constructor to call
        void(CALLTYPE *pDtor)(void*)    // Destructor to call should exception be thrown
){
    int i;      // Count of elements constructed
    int success = 0;

    __try
    {
        // Construct the elements of the array
        for( i = 0;  i < count;  i++ )
        {
            (*pCtor)( ptr );
            ptr = (char*)ptr + size;
        }
        success = 1;
    }
    __finally
    {
        if (!success)
        {
              DEBUGMSG(DBGEH,(TEXT("caught exception in __ehvec_ctor\r\n"))) ;
            __ArrayUnwind(ptr, size, i, pDtor);
        }
    }
}

