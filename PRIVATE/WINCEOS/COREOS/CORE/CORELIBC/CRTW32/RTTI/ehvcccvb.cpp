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
/***
*ehvcccvb.cpp - EH copy-ctor iterator helper function for class w/ virtual bases
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
    void*       ptr,                // Pointer to array to destruct
    size_t      size,               // Size of each element (including padding)
    int         count,              // Number of elements in the array
    void(CALLTYPE *pDtor)(void*)    // The destructor to call
);


void HELPERFNAPI __ehvec_copy_ctor_vb(
    void*       dst,                // Pointer to destination array
    void*       src,                // Pointer to source array
    size_t      size,               // Size of each element (including padding)
    int         count,              // Number of elements in the array
    void(CALLTYPE *pCopyCtor)(void*,void*),   // Constructor to call
    void(CALLTYPE *pDtor)(void*)    // Destructor to call should exception be thrown
){
    int i;      // Count of elements constructed
    int success = 0;

    __try
    {
        // Construct the elements of the array
        for( i = 0;  i < count;  i++ )
        {
#pragma warning(disable:4191)

            (*(void(CALLTYPE*)(void*,void*,int))pCopyCtor)( dst, src, 1 );

#pragma warning(default:4191)

            dst = (char*)dst + size;
            src = (char*)src + size;
        }
        success = 1;
    }
    __finally
    {
        if (!success)
            __ArrayUnwind(dst, size, i, pDtor);
    }
}
