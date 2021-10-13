//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*ehvecdtr.cpp - EH-aware version of destructor iterator helper function
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


#include <windows.h>
#include <ehdata.h>
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
    void*           ptr,                            // Pointer to array to destruct
    unsigned        size,                           // Size of each element (including padding)
    int             count,                          // Number of elements in the array
    void(CALLTYPE *pDtor)(void*)    // The destructor to call
    );


void HELPERFNAPI __ehvec_dtor(
    void*           ptr,                            // Pointer to array to destruct
    unsigned        size,                           // Size of each element (including padding)
    int             count,                          // Number of elements in the array
    void(CALLTYPE *pDtor)(void*)    // The destructor to call
    ) {
    int success = 0;

    // Advance pointer past end of array
    ptr = (char*)ptr + size*count;

    __try
    {
        // Destruct elements
        while       ( --count >= 0 )
        {
            ptr = (char*)ptr - size;
            (*pDtor)(ptr);
        }
        success = 1;
    }
    __finally
    {
        if (!success)
            __ArrayUnwind(ptr, size, count, pDtor);
    }
}

static int ArrayUnwindFilter(EXCEPTION_POINTERS* pExPtrs)
{
    EHExceptionRecord *pExcept = (EHExceptionRecord*)pExPtrs->ExceptionRecord;

    switch(PER_CODE(pExcept))
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

void __ArrayUnwind(
    void*           ptr,            // Pointer to array to destruct
    unsigned        size,           // Size of each element (including padding)
    int             count,          // Number of elements in the array
    void(CALLTYPE *pDtor)(void*)    // The destructor to call
    )
{
    // 'unwind' rest of array
    DEBUGMSG(DBGEH, (TEXT("__ArrayUnwind: ptr=0x%x size=0x%x count=0x%x pDtor=0x%x\r\n"), ptr, size, count, pDtor)) ;
    __try
    {
        while ( --count >= 0 )
        {
            ptr = (char*) ptr - size;
            (*pDtor)(ptr);
        }
    }
    __except( ArrayUnwindFilter(exception_info()) )
    {
    }
}

