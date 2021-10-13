//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
________________________________________________________________________________
THIS  CODE AND  INFORMATION IS  PROVIDED "AS IS"  WITHOUT WARRANTY  OF ANY KIND,
EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.


Module Name: stressrun.cpp

Abstract: records stress run status, implementation

Notes: none

________________________________________________________________________________
*/

#include <windows.h>
#include <tchar.h>
#include <stressutils.h>
#include "stressrun.h"


/*

@func:	_stressrun_t._stressrun_t, default constructor; use it to create testrun
objects for unnamed code blocks, such as in global context

@rdesc:	void

@param:	void

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

_stressrun_t::_stressrun_t()
{
	InitializeCriticalSection(&_cs);

	reset(true);

	return;
}


/*

@func:	_stressrun_t._stressrun_t, accepts code block name as a parameter; use
it to create testrun object for named code blocks, mainly functions

@rdesc:	void

@param:	[in] LPTSTR lpsz: name of the code block, e.g. function name

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

_stressrun_t::_stressrun_t(LPTSTR lpsz)
{
	InitializeCriticalSection(&_cs);

	reset(true);

	EnterCriticalSection(&_cs);

	if (_tcslen(lpsz) < MAX_PATH)
	{
		_tcscpy(szCodeBlock, lpsz);
	}

	if (_tcslen(szCodeBlock))
	{
		LogVerbose(L"Entering %s\r\n", szCodeBlock);
	}

	LeaveCriticalSection(&_cs);

	return;
}


/*

@func:	_stressrun_t.~_stressrun_t, destructor

@rdesc:	void

@param:	void

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

_stressrun_t::~_stressrun_t(void)
{
	EnterCriticalSection(&_cs);

	if (_tcslen(szCodeBlock))
	{
		LogVerbose(L"Exiting %s\r\n", szCodeBlock);
	}

	LeaveCriticalSection(&_cs);

	DeleteCriticalSection(&_cs);

	return;
}


/*

@func:	_stressrun_t.fail, increments failcount

@rdesc:	void

@param:	void

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

void _stressrun_t::fail()
{
	_critical_section_t cs(&_cs);
	_fail_count++;
	return;
}


/*

@func:	_stressrun_t.pass, increments passcount

@rdesc:	void

@param:	void

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

void _stressrun_t::pass()
{
	_critical_section_t cs(&_cs);
	_pass_count++;
	return;
}


/*

@func:	_stressrun_t.warning1, increments level 1 warning count

@rdesc:	void

@param:	void

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

void _stressrun_t::warning1()
{
	_critical_section_t cs(&_cs);
	_warn1_count++;
	return;
}


/*

@func:	_stressrun_t.warning2, increments level2 warning count

@rdesc:	void

@param:	void

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

void _stressrun_t::warning2()
{
	_critical_section_t cs(&_cs);
	_warn2_count++;
	return;
}


/*

@func:	_stressrun_t.reset, resets the object

@rdesc:	void

@param:	[in] bool fResetBlockName: true value resets the code block name as
well. defaulted to false

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

void _stressrun_t::reset(bool fResetBlockName)
{
	_critical_section_t cs(&_cs);

	_fail_count = _pass_count = _warn1_count = _warn2_count = 0;

	if (fResetBlockName)
	{
		memset(szCodeBlock, 0, sizeof szCodeBlock);
	}

	return;
}


/*

@func:	_stressrun_t.status, returns stress run status according to the correct
precedence (CESTRESS_FAIL > CESTRESS_WARN1 > CESTRESS_WARN2 > CESTRESS_PASS)

@rdesc:	one of the following stress status values

								- CESTRESS_FAIL
								- CESTRESS_WARN1
								- CESTRESS_WARN2
								- CESTRESS_PASS

@param:	void

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

DWORD _stressrun_t::status()
{
	_critical_section_t cs(&_cs);
	DWORD dwReturn = CESTRESS_PASS;

	if (_fail_count)
	{
		dwReturn = CESTRESS_FAIL;
		goto cleanup;
	}
	if (_warn1_count)
	{
		dwReturn = CESTRESS_WARN1;
		goto cleanup;
	}
	if (_warn2_count)
	{
		dwReturn = CESTRESS_WARN2;
		goto cleanup;
	}

cleanup:
	return dwReturn;
}


/*

@func:	_stressrun_t.test, increment proper statuscount after identifying the s
status code

@rdesc:	one of the stress status values, same as param

@param:	[in] DWORD dwStatus: stress status code

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

DWORD _stressrun_t::test(DWORD dwStatus)
{
	_critical_section_t cs(&_cs);

	switch (dwStatus)
	{
		case CESTRESS_FAIL:
			_fail_count++;
			break;
		case CESTRESS_WARN1:
			_warn1_count++;
			break;
		case CESTRESS_WARN2:
			_warn2_count++;
			break;
		case CESTRESS_PASS:
			_pass_count++;
			break;
		default:
			LogFail(_T("#### _stressrun_t: %s: status code unknown ####"),
				szCodeBlock);
			break;
	}

	return dwStatus;
}
