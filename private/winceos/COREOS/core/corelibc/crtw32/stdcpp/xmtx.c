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
/*	xmtx.c -- mutex support for VC++ */
#include "xmtx.h"

 #if !_MULTI_THREAD
 #else /* !_MULTI_THREAD */
/* Win32 critical sections are recursive, but
	Win32 does not have once-function */

void  __CLRCALL_PURE_OR_CDECL _Once(_Once_t *_Cntrl, void (*_Func)(void))
	{	/* execute _Func exactly one time */
	_Once_t old;
	if (*_Cntrl == 2)
		;
	else if ((old = InterlockedExchange(_Cntrl, 1)) == 0)
		{	/* execute _Func, mark as executed */
		_Func();
		*_Cntrl = 2;
		}
	else if (old == 2)
		*_Cntrl = 2;
	else
		while (*_Cntrl != 2)
			Sleep(1);
	}

void  __CLRCALL_PURE_OR_CDECL _Mtxinit(_Rmtx *_Mtx)
	{	/* initialize mutex */
#ifdef _WIN32_WCE // no spin count in CE
	InitializeCriticalSection(_Mtx);
#else
	InitializeCriticalSectionEx(_Mtx, 4000, 0);
#endif // _WIN32_WCE
	}

void  __CLRCALL_PURE_OR_CDECL _Mtxdst(_Rmtx *_Mtx)
	{	/* delete mutex */
	DeleteCriticalSection(_Mtx);
	}

_RELIABILITY_CONTRACT
void  __CLRCALL_PURE_OR_CDECL _Mtxlock(_Rmtx *_Mtx)
	{	/* lock mutex */
#ifdef _M_CEE
	System::Threading::Thread::BeginThreadAffinity();
#endif /* _M_CEE */
	EnterCriticalSection(_Mtx);
	}

_RELIABILITY_CONTRACT
void  __CLRCALL_PURE_OR_CDECL _Mtxunlock(_Rmtx *_Mtx)
	{	/* unlock mutex */
	LeaveCriticalSection(_Mtx);
#ifdef _M_CEE
	System::Threading::Thread::EndThreadAffinity();
#endif /* _M_CEE */
	}
 #endif /* !_MULTI_THREAD */

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V6.00:0009 */
