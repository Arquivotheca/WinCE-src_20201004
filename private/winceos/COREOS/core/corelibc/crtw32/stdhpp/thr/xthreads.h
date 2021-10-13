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
/* xthreads.h -- internal header for threads library */
#pragma once
#ifndef _THR_XTHREADS_H
#define _THR_XTHREADS_H
#ifndef RC_INVOKED
#include <thr/xthrcommon.h>
#include <thr/xtimec.h>
#include <limits.h>

 #pragma pack(push,_CRT_PACKING)
 #pragma warning(push,3)
 #pragma push_macro("new")
 #undef new

_C_LIB_DECL
enum {	/* return codes */
	_Thrd_success,
	_Thrd_nomem,
	_Thrd_timedout,
	_Thrd_busy,
	_Thrd_error
	};

	/* threads */
typedef _Thrd_imp_t _Thrd_t;
typedef int (*_Thrd_start_t)(void *);

_CRTIMP2_PURE int __cdecl _Thrd_create(_Thrd_t *, _Thrd_start_t, void *);

_CRTIMP2_PURE int __cdecl _Thrd_detach(_Thrd_t);
_CRTIMP2_PURE void __cdecl _Thrd_exit(int);
_CRTIMP2_PURE int __cdecl _Thrd_join(_Thrd_t, int *);
_CRTIMP2_PURE void __cdecl _Thrd_sleep(const xtime*);
_CRTIMP2_PURE void __cdecl _Thrd_yield(void);
_CRTIMP2_PURE int __cdecl _Thrd_equal(_Thrd_t, _Thrd_t);
_CRTIMP2_PURE int __cdecl _Thrd_lt(_Thrd_t, _Thrd_t);
_CRTIMP2_PURE _Thrd_t __cdecl _Thrd_current(void);

	/* mutexes */
typedef _Mtx_imp_t _Mtx_t;
enum {	/* mutex types */
	_Mtx_plain = 0x01,
	_Mtx_try	= 0x02,
	_Mtx_timed	= 0x04,
	_Mtx_recursive = 0x100
	};

_CRTIMP2_PURE int __cdecl _Mtx_init(_Mtx_t *, int);
_CRTIMP2_PURE void __cdecl _Mtx_destroy(_Mtx_t *);
_CRTIMP2_PURE int __cdecl _Mtx_current_owns(_Mtx_t *);
_CRTIMP2_PURE int __cdecl _Mtx_lock(_Mtx_t *);
_CRTIMP2_PURE int __cdecl _Mtx_trylock(_Mtx_t *);
_CRTIMP2_PURE int __cdecl _Mtx_timedlock(_Mtx_t *, const xtime *);
_CRTIMP2_PURE int __cdecl _Mtx_unlock(_Mtx_t *);

_CRTIMP2_PURE void *__cdecl _Mtx_getconcrtcs(_Mtx_t *);
_CRTIMP2_PURE void __cdecl _Mtx_clear_owner(_Mtx_t *);
_CRTIMP2_PURE void __cdecl _Mtx_reset_owner(_Mtx_t *);

	/* condition variables */
typedef _Cnd_imp_t _Cnd_t;

_CRTIMP2_PURE int __cdecl _Cnd_init(_Cnd_t *);
_CRTIMP2_PURE void __cdecl _Cnd_destroy(_Cnd_t *);
_CRTIMP2_PURE int __cdecl _Cnd_wait(_Cnd_t *, _Mtx_t *);
_CRTIMP2_PURE int __cdecl _Cnd_timedwait(_Cnd_t *, _Mtx_t *, const xtime *);
_CRTIMP2_PURE int __cdecl _Cnd_broadcast(_Cnd_t *);
_CRTIMP2_PURE int __cdecl _Cnd_signal(_Cnd_t *);
_CRTIMP2_PURE int __cdecl _Cnd_register_at_thread_exit(_Cnd_t*, _Mtx_t*, int*);
_CRTIMP2_PURE int __cdecl _Cnd_unregister_at_thread_exit(_Mtx_t *);
_CRTIMP2_PURE int __cdecl _Cnd_do_broadcast_at_thread_exit(void);

	/* thread specific storage */
typedef _Tss_imp_t _Tss_t;
typedef void (*_Tss_dtor_t)(void *);
_CRTIMP2_PURE int __cdecl _Tss_create(_Tss_t *, _Tss_dtor_t);

_CRTIMP2_PURE int __cdecl _Tss_delete(_Tss_t);
_CRTIMP2_PURE int __cdecl _Tss_set(_Tss_t, void *);
_CRTIMP2_PURE void *__cdecl _Tss_get(_Tss_t);

#define _TSS_DTOR_ITERATIONS	_TSS_DTOR_ITERATIONS_IMP

 #if _TSS_USE_MACROS
  #define _Tss_create(key, dtor)    _TSS_CREATE(key, dtor)
  #define _Tss_delete(key)		    _TSS_DELETE(key)
  #define _Tss_set(key, value)	    _TSS_SET(key, value)
  #define _Tss_get(key)			    _TSS_GET(key)
 #endif	/* TSS_USE_MACROS */

	/* once functions */
#define _ONCE_FLAG_INIT		_ONCE_FLAG_INIT_IMP
#define _ONCE_FLAG_CPP_INIT	0

typedef _Once_flag_imp_t _Once_flag;
typedef int _Once_flag_cpp;
typedef void (__cdecl *_Once_callback_t)(void);
typedef void (__cdecl *_Once_callbackEx_t)(void *);

_CRTIMP2_PURE void __cdecl _Call_once(_Once_flag *, _Once_callback_t);
_CRTIMP2_PURE void __cdecl _Call_onceEx(_Once_flag_cpp *, _Once_callbackEx_t,
	void *);

	/* utility functions */
_CRTIMP2_PURE void __cdecl _Thrd_abort(const char *);
_END_C_LIB_DECL
 #pragma pop_macro("new")
 #pragma warning(pop)
 #pragma pack(pop)
#endif /* RC_INVOKED */
#endif	/* _THR_XTHREADS_H */

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
