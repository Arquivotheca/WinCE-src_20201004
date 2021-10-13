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
/* xonce.cpp -- _Call_onceEx function */
#include <thr/xthreads.h>
#include <stdlib.h>

	// common code for extended call once
static _Mtx_t ext_once_mtx;
static _Once_flag ext_once_state = _ONCE_FLAG_INIT;

struct _Mtx_guard
	{	// locks and unlocks a mutex
	_Mtx_guard()
		{
		_Mtx_lock(&ext_once_mtx);
		}

	~_Mtx_guard() _NOEXCEPT
		{
		_Mtx_unlock(&ext_once_mtx);
		}
	};

static void ext_cleanup()
	{	/* clean up resources for extended call once */
	_Mtx_destroy(&ext_once_mtx);
	}

static void init_ext_mutex()
	{	/* initialize resources for extended call once */
	if (_Mtx_init(&ext_once_mtx, _Mtx_plain) != _Thrd_success)
		_Thrd_abort("unable to initialize call-once mutex");
	_CSTD atexit(ext_cleanup);
	}

_EXTERN_C
// TODO: need fast implementation of _Call_onceEx (N2444)
void _Call_onceEx(_Once_flag_cpp *cntrl, void (*func)(void*), void *arg)
	{	/* execute func(arg) exactly one time */
	_Call_once(&ext_once_state, init_ext_mutex);
	_Mtx_guard guard;
	if (*cntrl == _ONCE_FLAG_CPP_INIT)
		{	/* call func(arg) and mark as called */
		func((void *)arg);
		*cntrl = !_ONCE_FLAG_CPP_INIT;
		}
	}
_END_EXTERN_C

/*
 * This file is derived from software bearing the following
 * restrictions:
 *
 * (c) Copyright William E. Kempf 2001
 *
 * Permission to use, copy, modify, distribute and sell this
 * software and its documentation for any purpose is hereby
 * granted without fee, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation. William E. Kempf makes no representations
 * about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 */

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
