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
// handler.cpp -- set_new_handler for Microsoft
#include <new>

typedef int (__cdecl *new_hand)(size_t);
new_hand __cdecl _set_new_handler(new_hand);

_STD_BEGIN
static new_handler _New_handler;

int __cdecl _New_handler_interface(size_t) _THROW1(std::bad_alloc)
	{	// interface to existing Microsoft _callnewh mechanism
	_New_handler();
	return (1);
	}

_CRTIMP2 new_handler __cdecl set_new_handler(new_handler pnew) _THROW0()
	{	// remove current handler
	_BEGIN_LOCK(_LOCK_MALLOC)	// lock thread to ensure atomicity
		new_handler pold = _New_handler;
		_New_handler = pnew;
		_set_new_handler(pnew ? _New_handler_interface : 0);
		return (pold);
	_END_LOCK()
	}

_CRTIMP2 new_handler __cdecl get_new_handler() _THROW0()
	{	// get current new handler
	_BEGIN_LOCK(_LOCK_MALLOC)	// lock thread to ensure atomicity
		return (_New_handler);
	_END_LOCK()
	}

_STD_END

/*
 * Copyright (c) 1992-2007 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V5.03:0009 */
