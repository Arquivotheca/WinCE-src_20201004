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
//------------------------------------------------------------------------------
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//
//------------------------------------------------------------------------------

#ifndef _OSBENCHPSL_HPP_
#define _OSBENCHPSL_HPP_

typedef DWORD (*PAPIFN)(...);

#define SMALL_BUF_SIZE        128
#define LARGE_BUF_SIZE        4096

// To run the same binary on both Yamazaki and Bowmore/Macallan
// We need to duplicate the API signature defines. These are taken
// from Bowmore pkfuncs.h
#define BOWMORE_ARG_DW          0
#define BOWMORE_ARG_PTR         1
#define BOWMORE_ARG_I64         2
#define BOWMORE_ARG_TYPE_MASK   0x03
#define BOWMORE_ARG_TYPE_BITS   2

#define BM_ARG(arg, inx)  (BOWMORE_ARG_ ## arg << ARG_TYPE_BITS*inx)
#define BOWMORE_FNSIG0()    0
#define BOWMORE_FNSIG1(a0)      BM_ARG(a0,0)
#define BOWMORE_FNSIG3(a0, a1, a2)  (BM_ARG(a0,0)|BM_ARG(a1,1)|BM_ARG(a2,2))
#define BOWMORE_FNSIG6(a0, a1, a2, a3, a4, a5) \
    (BM_ARG(a0,0)|BM_ARG(a1,1)|BM_ARG(a2,2)|BM_ARG(a3,3)|BM_ARG(a4,4) \
    |BM_ARG(a5,5))
#define BOWMORE_FNSIG12(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
    (BM_ARG(a0,0)|BM_ARG(a1,1)|BM_ARG(a2,2)|BM_ARG(a3,3)|BM_ARG(a4,4) \
     |BM_ARG(a5,5)|BM_ARG(a6,6)|BM_ARG(a7,7)|BM_ARG(a8,8)|BM_ARG(a9,9) \
     |BM_ARG(a10,10)|BM_ARG(a11,11)) 

#if defined(MIPS)
    #define FIRST_METHOD_BOWMORE    0xFFFFFC02
    #define APICALL_SCALE_BOWMORE   4
#elif defined(x86)
    #define FIRST_METHOD_BOWMORE    0xFFFFFE00
    #define APICALL_SCALE_BOWMORE   2
#elif defined(ARM)
    #define FIRST_METHOD_BOWMORE    0xF0010000
    #define APICALL_SCALE_BOWMORE   4
#elif defined(SHx)
    #define FIRST_METHOD_BOWMORE    0xFFFFFE01
    #define APICALL_SCALE_BOWMORE   2
#else
    #error "Unknown CPU type"
#endif

#define HANDLE_SHIFT_BOWMORE    8


#define PRIV_IMPLICIT_CALL_BOWMORE(hid, mid) (FIRST_METHOD_BOWMORE - ((hid)<<HANDLE_SHIFT_BOWMORE | (mid))*APICALL_SCALE_BOWMORE)
#define IMPLICIT_DECL_BOWMORE(type, hid, mid, args) (*(type (*)args)PRIV_IMPLICIT_CALL_BOWMORE(hid, mid))

typedef BOOL (*PFN_REGISTER_DIRECT) ( HANDLE, PFNVOID* );
typedef HANDLE (*PFN_CREATEAPISET) ( char*, USHORT, const PFNVOID*, const ULONGLONG* );

#endif // _OSBENCHPSL_HPP_
