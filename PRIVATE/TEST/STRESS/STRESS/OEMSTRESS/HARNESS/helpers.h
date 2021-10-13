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

Module Name: helpers.h

Abstract: Useful defines, extern'ed variables to be used in the OEM stress
harness

Notes: None

________________________________________________________________________________
*/

#ifndef __HELPERS_H__


#define	MAX_FILE_NAME				(MAX_PATH + 1)
#define	RANDOM_INT(max, min)    	((INT)((rand() % ((INT)max - (INT)min + 1)) + (INT)min))
#define	SECOND						(1000)
#define	MINUTE						(60 * SECOND)
#define	HOUR						(60 * MINUTE)
#define	MAX_BUFFER_SIZE				(0x400)


/*

@class:	_shared_t, Templated wrapper for sharing atomic variables acrosses
processes using memory mapped file

@note:	Valid only for the atomic data types as the class uses Interlocked
APIs to get/set values

@fault:	None

@pre:	None

@post:	None

*/

template <class TYPE>
struct _shared_t
{
	HANDLE _hMap;
	TYPE *_pShared;

	_shared_t(LPCWSTR lpszName)
	{
		_hMap = CreateFileMapping(
			(void *)0xFFFFFFFF,
			NULL,
			PAGE_READWRITE,
			0x00,
			sizeof(TYPE),
			lpszName
		);

		if (_hMap)
		{
			_pShared = (TYPE *)MapViewOfFile(
				_hMap,
				FILE_MAP_ALL_ACCESS,
				0x00,
				0x00,
				sizeof(TYPE)
			);
		}
	}

	~_shared_t(void)
	{
		if (_pShared)
		{
			UnmapViewOfFile(_pShared);
			_pShared = NULL;
		}

		if (_hMap)
		{
			CloseHandle(_hMap);
			_hMap = NULL;
		}
	}

	inline LONG GetValue()
	{
		return InterlockedExchange((LPLONG)_pShared, *_pShared);
	}

	inline LONG SetValue(LONG lNew)
	{
		return InterlockedExchange((LPLONG)_pShared, lNew);
	}

private:
	_shared_t();										// default constructor NOT implemented
	_shared_t(const _shared_t&);						// default copy constructor NOT implemented
	_shared_t& operator = (const _shared_t&);			// default assignment operator NOT implemented
};


namespace Options
{
	enum MODULE_DURATION {NORMAL, RANDOM, INDEFINITE, MINIMUM};

	// options read from cmdline

	extern UINT g_nHangTimeoutMinutes;
	extern BOOL	g_fAllModulesHang;

	extern UINT	g_nRunTimeMinutes;
	extern UINT	g_nRefreshResultsIntervalMinutes;

	extern UINT g_nModuleDurationMinutes;
	extern MODULE_DURATION g_fModuleDuration;

	extern UINT	g_nProcessSlots;
	extern UINT	g_nRandomSeed;

	extern WCHAR g_szResultFile[MAX_FILE_NAME];
	extern WCHAR g_szHistoryFile[MAX_FILE_NAME];
	extern WCHAR g_szIniFile[MAX_FILE_NAME];

	extern WCHAR g_szServer[MAX_FILE_NAME];

	extern _shared_t<DWORD> g_Verbosity;

	extern BOOL g_fWaitForModuleCompletion;

	extern BOOL g_fAutoOOM;
	extern UINT g_nMemThreshold;
	extern BOOL g_fMemLevelPercent;

	// other options

	extern BOOL	g_fDebuggerPresent;
};


extern void DebugDump(const wchar_t *szFormat, ...);

namespace CETK
{
	extern HLOCAL g_hMemMissingFiles;				// available only when launching from CETK
};


#define __HELPERS_H__
#endif /* __HELPERS_H__ */
