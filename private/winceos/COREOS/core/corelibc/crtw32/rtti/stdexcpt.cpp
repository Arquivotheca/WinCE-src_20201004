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
*stdexcpt.cpp - defines C++ standard exception classes
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Implementation of C++ standard exception classes which must live in
*       the main CRT, not the C++ CRT, because they are referenced by RTTI
*       support in the main CRT.
*
*        exception
*          bad_cast
*          bad_typeid
*            __non_rtti_object
*******************************************************************************/

#define _DEFINE_EXCEPTION_MEMBER_FUNCTIONS

#define _USE_ANSI_CPP   /* Don't emit /lib:libcp directive */

#include <stdlib.h>
#include <string.h>
#include <eh.h>
#ifndef _SYSCRT
#include <exception>
#include <typeinfo>
#else
#include <stdexcpt.h>
#include <typeinfo.h>
#endif

#pragma warning(disable:4439)   // C4439: function with a managed parameter must have a __clrcall calling convention

#if defined(MRTDLL)
#define __CLR_OR_THIS_CALL  __clrcall
#else
#define __CLR_OR_THIS_CALL
#endif

#ifndef _SYSCRT
_STD_BEGIN
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Implementation of class "bad_cast"
//

__CLR_OR_THIS_CALL bad_cast::bad_cast(const char * _Message)
    : exception(_Message)
{
}

__CLR_OR_THIS_CALL bad_cast::bad_cast(const bad_cast & that)
    : exception(that)
{
}

__CLR_OR_THIS_CALL bad_cast::~bad_cast()
{
}

#ifdef CRTDLL
//
// This is a dummy constructor.  Previously, the only bad_cast ctor was
// bad_cast(const char * const &).  To provide backwards compatibility
// for std::bad_cast, we want the main ctor to be bad_cast(const char *)
// instead.  Since you can't have both bad_cast(const char * const &) and
// bad_cast(const char *), we define this bad_cast(const char * const *),
// which will have the exact same codegen as bad_cast(const char * const &),
// and alias the old form with a .def entry.
//
__CLR_OR_THIS_CALL bad_cast::bad_cast(const char * const * _PMessage)
    : exception(*_PMessage)
{
}
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Implementation of class "bad_typeid"
//

__CLR_OR_THIS_CALL bad_typeid::bad_typeid(const char * _Message)
    : exception(_Message)
{
}

__CLR_OR_THIS_CALL bad_typeid::bad_typeid(const bad_typeid & that)
    : exception(that)
{
}

__CLR_OR_THIS_CALL bad_typeid::~bad_typeid()
{
}

////////////////////////////////////////////////////////////////////////////////
//
// Implementation of class "__non_rtti_object"
//

__CLR_OR_THIS_CALL __non_rtti_object::__non_rtti_object(const char * _Message)
    : bad_typeid(_Message)
{
}

__CLR_OR_THIS_CALL __non_rtti_object::__non_rtti_object(const __non_rtti_object & that)
    : bad_typeid(that)
{
}

__CLR_OR_THIS_CALL __non_rtti_object::~__non_rtti_object()
{
}


#ifndef _SYSCRT
_STD_END
#endif


