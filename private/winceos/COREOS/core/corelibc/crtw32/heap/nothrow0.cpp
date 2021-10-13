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
*nothrow0.cpp - defines object std::nothrow_t for placement new
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*       Derived from code Copyright (c) 1992-2001 by P.J. Plauger.
*
*Purpose:
*       Defines the object std::nothrow which can be used as a placement
*       new tag to call the form of operator new which is guaranteed to
*       return NULL on an allocation failure instead of raising an
*       exception of std::bad_alloc.
*
*Revision History:
*       06-15-01  PML   Initial version.
*
*******************************************************************************/

#ifdef CRTDLL
#undef CRTDLL
#endif

#ifdef MRTDLL
#undef MRTDLL
#endif

#include <new.h>

namespace std {

    const nothrow_t nothrow = nothrow_t();

};
