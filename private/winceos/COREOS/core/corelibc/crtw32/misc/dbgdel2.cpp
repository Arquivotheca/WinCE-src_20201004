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
*dbgdel2.cpp - defines C++ scalar and array delete operators, debug versions
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Defines C++ scalar and array delete operators, debug versions.
*
*Revision History:
*
*******************************************************************************/

#include <cruntime.h>
#include <malloc.h>
#include <dbgint.h>

/* These versions are declared in crtdbg.h.
 * Implementing there operators in the CRT libraries makes it easier to override them.
 */

void operator delete(void * pUserData, int, const char *, int)
{ 
    ::operator delete(pUserData); 
}

void operator delete[](void * pUserData, int, const char *, int)
{ 
    ::operator delete[](pUserData); 
}
