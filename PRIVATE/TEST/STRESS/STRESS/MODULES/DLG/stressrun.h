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


Module Name: stressrun.h

Abstract: records stress run status, header

Notes: none

________________________________________________________________________________
*/

#ifndef __STRESSRUN_H__


/*

@class:	_stressrun_t, records and reports stress run status in a thread safe 
manner

@note:	none

@fault:	none

@pre:	none

@post:	none

*/

class _stressrun_t
{
public:
	TCHAR szCodeBlock[MAX_PATH];
	DWORD _fail_count;
	DWORD _pass_count;
	DWORD _warn1_count;
	DWORD _warn2_count;
	CRITICAL_SECTION _cs;

public:
	_stressrun_t();
	_stressrun_t(LPTSTR lpsz);
	~_stressrun_t(void);
	void fail();
	void pass();
	void warning1();
	void warning2();
	void reset(bool fResetBlockName = false);
	DWORD status();
	DWORD test(DWORD dwStatus);

private:
	struct _critical_section_t
	{
		LPCRITICAL_SECTION _lpcs;
		
		_critical_section_t(LPCRITICAL_SECTION lpCriticalSection)
		{
			_lpcs = lpCriticalSection;
			EnterCriticalSection(_lpcs);
		}

		~_critical_section_t()
		{
			LeaveCriticalSection(_lpcs);
		}
	};

	private:
		_stressrun_t(const _stressrun_t&);				// default copy constructor NOT implemented
		_stressrun_t& operator= (const _stressrun_t&);	// default assignment operator NOT 
};


#define __STRESSRUN_H__
#endif /* __STRESSRUN_H__ */
