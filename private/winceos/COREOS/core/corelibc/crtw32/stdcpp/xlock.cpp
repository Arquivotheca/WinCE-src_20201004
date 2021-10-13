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
// xlock.cpp -- global lock for locales, etc.
#include <stdlib.h>
#include <yvals.h>

 #if _MULTI_THREAD
  #include <mtdll.h>
  #include <xmtx.h>
_STD_BEGIN

  #define MAX_LOCK	4	/* must be power of two */

#pragma warning(disable: 4074)
#pragma init_seg(compiler)

static _Rmtx mtx[MAX_LOCK];
static long init = -1;

#if !defined(MRTDLL)

__thiscall _Init_locks::_Init_locks()
	{	// initialize locks
	if (InterlockedIncrement(&init) == 0)
		for (int count = 0; count < MAX_LOCK; ++count)
			_Mtxinit(&mtx[count]);
	}

__thiscall _Init_locks::~_Init_locks() _NOEXCEPT
	{	// clean up locks
	if (InterlockedDecrement(&init) < 0)
		for (int count = 0; count < MAX_LOCK; ++count)
			_Mtxdst(&mtx[count]);
	}
#endif

_MRTIMP2_NPURE void __cdecl _Init_locks::_Init_locks_ctor(_Init_locks * _This)
	{	// initialize locks
	if (InterlockedIncrement(&init) == 0)
		for (int count = 0; count < MAX_LOCK; ++count)
			_Mtxinit(&mtx[count]);
	}

_MRTIMP2_NPURE void __cdecl _Init_locks::_Init_locks_dtor(_Init_locks * _This)
	{	// clean up locks
	if (InterlockedDecrement(&init) < 0)
		for (int count = 0; count < MAX_LOCK; ++count)
			_Mtxdst(&mtx[count]);
	}

static _Init_locks initlocks;

#if !defined(MRTDLL)

__thiscall _Lockit::_Lockit()
	: _Locktype(0)
	{	// lock default mutex
	if (_Locktype == _LOCK_LOCALE)
		_mlock(_SETLOCALE_LOCK);
	else
		_Mtxlock(&mtx[0]);
	}

__thiscall _Lockit::_Lockit(int kind)
	: _Locktype(kind)
	{	// lock the mutex
	if (_Locktype == _LOCK_LOCALE)
		_mlock(_SETLOCALE_LOCK);
	else if (_Locktype < MAX_LOCK)
		_Mtxlock(&mtx[_Locktype]);
	}

__thiscall _Lockit::~_Lockit() _NOEXCEPT
	{	// unlock the mutex
	if (_Locktype == _LOCK_LOCALE)
		_munlock(_SETLOCALE_LOCK);
	else if (_Locktype < MAX_LOCK)
		_Mtxunlock(&mtx[_Locktype]);
	}
#endif

_MRTIMP2_NPURE void __cdecl _Lockit::_Lockit_ctor(_Lockit * _This)
	{	// lock default mutex
	_Mtxlock(&mtx[0]);
	}

_MRTIMP2_NPURE void __cdecl _Lockit::_Lockit_ctor(_Lockit * _This, int kind)
	{	// lock the mutex
	if (kind == _LOCK_LOCALE)
		_mlock(_SETLOCALE_LOCK);
	else
		{
		_This->_Locktype = kind & (MAX_LOCK - 1);
		_Mtxlock(&mtx[_This->_Locktype]);
		}
	}

_MRTIMP2_NPURE void __cdecl _Lockit::_Lockit_dtor(_Lockit * _This)
	{	// unlock the mutex
	_Mtxunlock(&mtx[_This->_Locktype]);
	}

_RELIABILITY_CONTRACT
_MRTIMP2_NPURE void __cdecl _Lockit::_Lockit_ctor(int kind)
	{	// lock the mutex
	if (kind == _LOCK_LOCALE)
		_mlock(_SETLOCALE_LOCK);
	else
		_Mtxlock(&mtx[kind & (MAX_LOCK - 1)]);
	}

_RELIABILITY_CONTRACT
_MRTIMP2_NPURE void __cdecl _Lockit::_Lockit_dtor(int kind)
	{	// unlock the mutex
	if (kind == _LOCK_LOCALE)
		_munlock(_SETLOCALE_LOCK);
	else
		_Mtxunlock(&mtx[kind & (MAX_LOCK - 1)]);
	}

_STD_END
 #endif /* _MULTI_THREAD */

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V6.00:0009 */
