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
// 
// Shared STL version header, included by yvals.h in all STL versions. 
// 


#ifndef _INTERNAL_IFSTRIP_
// Detect mismatched STL versions in the windows build environment. 
//  Published version of crtdefs.h, when compiled with STL enabled,
//  will emit version mismatch checks.
#endif

#ifndef _INTERNAL_IFSTRIP_
#ifndef _CRTBLD_REAL
#error "CRT should build with _CRTBLD_REAL set
#endif
#endif

#ifdef _INTERNAL_IFSTRIP_
// CRT binaries will not have version tags, use
//  IFSTRIP to ensure CRT does not compile this code

#ifdef _CRTBLD_REAL
#error "CRT should be built against pre-ifstripped headers"
#endif

// WINCRT: detect mismatches between STL versions.  
//   grandfathering in projects that mixed
//   STL70 and STL60
#ifndef WIN_DETECT_STL_MISMATCH
    #if defined(WIN_FORCE_IGNORE_STL_MISMATCH)
        // DISABLE version check for just this compiland        
    #elif defined(_STL60_) || defined(_STL70_)
        #define WIN_DETECT_STL_MISMATCH  "60_or_70"
    #elif defined(_STL100_)
        #define WIN_DETECT_STL_MISMATCH  "100"
    #endif
#endif
#if defined(WIN_DETECT_STL_MISMATCH) && (_MSC_FULL_VER >= 160000000)
    #pragma detect_mismatch("WIN_DETECT_STL_MISMATCH", WIN_DETECT_STL_MISMATCH)
#endif

#endif

