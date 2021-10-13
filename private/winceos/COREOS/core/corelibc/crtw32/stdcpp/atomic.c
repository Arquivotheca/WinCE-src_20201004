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
/* atomic.c -- implement shared_ptr spin lock */
#include <yvals.h>
#include <intrin.h>

#pragma warning(disable: 4793)

_STD_BEGIN
		/* SPIN LOCK FOR shared_ptr ATOMIC OPERATIONS */
volatile long _Shared_ptr_flag;

_CRTIMP2_PURE void __cdecl _Lock_shared_ptr_spin_lock()
	{	/* spin until _Shared_ptr_flag successfully set */
  #ifdef _M_ARM
	while (_InterlockedExchange_acq(&_Shared_ptr_flag, 1))
		__yield();
  #else /* _M_ARM */
	while (_interlockedbittestandset(&_Shared_ptr_flag, 0))	/* set bit 0 */
		;
  #endif /* _M_ARM */
	}

_CRTIMP2_PURE void __cdecl _Unlock_shared_ptr_spin_lock()
	{	/* release previously obtained lock */
  #ifdef _M_ARM
	__dmb(_ARM_BARRIER_ISH);
	__iso_volatile_store32((volatile int *) &_Shared_ptr_flag, 0);
  #else /* _M_ARM */
	_interlockedbittestandreset(&_Shared_ptr_flag, 0);	/* reset bit 0 */
  #endif /* _M_ARM */
	}
_STD_END

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
