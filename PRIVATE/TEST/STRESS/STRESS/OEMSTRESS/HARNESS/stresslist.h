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

Module Name: stresslist.h

Abstract: Contains core harness functions and class declarations including
module list and run list

Notes: None

________________________________________________________________________________
*/

#ifndef __STRESSLIST_H__


#define MAX_CMDLINE					(MAX_PATH+60)
#define	CMDLINE_HARNESS_RESERVED	(50)
#define	THREAD_COMPLETION_DELAY		(200)


/*

@class:	CRITICAL_SECTION_TAKER, Wrapper class for EnterCriticalSection &
LeaveCriticalSection

@note:	The critical section must be created and destroyed elsewhere.

@fault:	None

@pre:	None

@post:	None

*/

struct CRITICAL_SECTION_TAKER
{
	LPCRITICAL_SECTION _lpcs;

	CRITICAL_SECTION_TAKER(LPCRITICAL_SECTION lpCriticalSection)
	{
		_lpcs = lpCriticalSection;
		EnterCriticalSection(_lpcs);
	}

	~CRITICAL_SECTION_TAKER()
	{
		LeaveCriticalSection(_lpcs);
	}
};


/*

@class:	MODULE_DETAIL, Node structure containing all module related information

@note:	None

@fault:	None

@pre:	None

@post:	None

*/

struct MODULE_DETAIL
{
	MODULE_DETAIL *pNext;

	LPWSTR pModuleName;
	LPWSTR pCommandLine;
	LPWSTR pDependency;		// dependent modules are separated by '*'
	INT nDependencyCount;
	LPWSTR pOSComponents;	// required OS component names are separated by ' '
	INT nOSCompCount;

	UINT nMinRunTimeMinutes;

	UINT nLaunchCount;
	UINT nPassCount;
	UINT nIterations;
	UINT nLevel1WarningCount;
	UINT nLevel2WarningCount;
	UINT nFailCount;
	UINT nAbortCount;

	UINT nTerminateCount;

	DWORD dwAverageDuration;
	DWORD dwMaxDuration;
	DWORD dwMinDuration;
	DWORD dwTickCountFirstLaunched;
	DWORD dwTickCountLastLaunched;


	MODULE_DETAIL(const MODULE_DETAIL& md) // default copy constructor
	{
		this->Init(md.pModuleName, md.pCommandLine, md.nMinRunTimeMinutes,
			md.pDependency, md.pOSComponents);

		this->nDependencyCount = md.nDependencyCount;
		this->nOSCompCount = md.nOSCompCount;

		this->nMinRunTimeMinutes = md.nMinRunTimeMinutes;
		this->nLaunchCount = md.nLaunchCount;
		this->nPassCount = md.nPassCount;
		this->nIterations = md.nIterations;
		this->nLevel1WarningCount = md.nLevel1WarningCount;
		this->nLevel2WarningCount = md.nLevel2WarningCount;
		this->nFailCount = md.nFailCount;
		this->nAbortCount = md.nAbortCount;
		this->nTerminateCount = md.nTerminateCount;
		this->dwAverageDuration = md.dwAverageDuration;
		this->dwMaxDuration = md.dwMaxDuration;
		this->dwMinDuration = md.dwMinDuration;
		this->dwTickCountFirstLaunched = md.dwTickCountFirstLaunched;
		this->dwTickCountLastLaunched = md.dwTickCountLastLaunched;
	}


	MODULE_DETAIL& operator=(const MODULE_DETAIL& md) // default assignment operator
	{
		if (this == &md)
		{
			return *this;
		}
		else
		{
			this->Init(md.pModuleName, md.pCommandLine, md.nMinRunTimeMinutes,
				md.pDependency, md.pOSComponents);

			this->nDependencyCount = md.nDependencyCount;
			this->nOSCompCount = md.nOSCompCount;

			this->nMinRunTimeMinutes = md.nMinRunTimeMinutes;
			this->nLaunchCount = md.nLaunchCount;
			this->nPassCount = md.nPassCount;
			this->nIterations = md.nIterations;
			this->nLevel1WarningCount = md.nLevel1WarningCount;
			this->nLevel2WarningCount = md.nLevel2WarningCount;
			this->nFailCount = md.nFailCount;
			this->nAbortCount = md.nAbortCount;
			this->nTerminateCount = md.nTerminateCount;
			this->dwAverageDuration = md.dwAverageDuration;
			this->dwMaxDuration = md.dwMaxDuration;
			this->dwMinDuration = md.dwMinDuration;
			this->dwTickCountFirstLaunched = md.dwTickCountFirstLaunched;
			this->dwTickCountLastLaunched = md.dwTickCountLastLaunched;
		}

		return *this;
	}


	BOOL Init(IN char *pName, IN char *pCmdLine, IN UINT nMinRunTime,
		IN char *pDepends = NULL, IN char *pOSComp = NULL)
	{
		BOOL fOk = true;
		memset(this, 0, sizeof MODULE_DETAIL);
		dwMinDuration = UINT_MAX;

		// module name

		pModuleName = new WCHAR [strlen(pName) + 1];

		if (!pModuleName)
		{
			fOk = false;
			goto cleanup;
		}

		memset(pModuleName, 0, sizeof(WCHAR) * (strlen(pName) + 1));

		mbstowcs(pModuleName, pName, strlen(pName));

		// command line

		pCommandLine = new WCHAR [strlen(pCmdLine) + 1];

		if (!pCommandLine)
		{
			fOk = false;
			goto cleanup;
		}

		memset(pCommandLine, 0, sizeof(WCHAR) * (strlen(pCmdLine) + 1));

		mbstowcs(pCommandLine, pCmdLine, strlen(pCmdLine));

		// minimum runtime

		nMinRunTimeMinutes = nMinRunTime;

		// dependency

		if (pDepends && *pDepends)
		{
			pDependency = new WCHAR [strlen(pDepends) + 1];

			if (!pDependency)
			{
				fOk = false;
				goto cleanup;
			}

			memset(pDependency, 0, sizeof(WCHAR) * (strlen(pDepends) + 1));

			mbstowcs(pDependency, pDepends, strlen(pDepends));
		}
		else
		{
			nDependencyCount = 0;
		}

		// os components

		if (pOSComp && *pOSComp)
		{
			pOSComponents = new WCHAR [strlen(pOSComp) + 1];

			if (!pOSComponents)
			{
				fOk = false;
				goto cleanup;
			}

			memset(pOSComponents, 0, sizeof(WCHAR) * (strlen(pOSComp) + 1));

			mbstowcs(pOSComponents, pOSComp, strlen(pOSComp));
		}
		else
		{
			nOSCompCount = 0;
		}

	cleanup:
		return fOk;
	}


	BOOL Init(IN WCHAR *pName, IN WCHAR *pCmdLine, IN UINT nMinRunTime,
		IN WCHAR *pDepends = NULL, IN WCHAR *pOSComp = NULL)
	{
		char szModName[MAX_FILE_NAME];
		char szCmdLine[MAX_BUFFER_SIZE];
		char szDepends[MAX_BUFFER_SIZE];
		char szOSComps[MAX_BUFFER_SIZE];

		memset(szModName, 0, sizeof szModName);
		memset(szCmdLine, 0, sizeof szCmdLine);
		memset(szDepends, 0, sizeof szDepends);
		memset(szOSComps, 0, sizeof szOSComps);

		wcstombs(szModName, pName, MAX_FILE_NAME);

		if (pCmdLine && *pCmdLine)
		{
			wcstombs(szCmdLine, pCmdLine, MAX_BUFFER_SIZE);
		}

		if (pDepends && *pDepends)
		{
			wcstombs(szDepends, pDepends, MAX_BUFFER_SIZE);
		}

		if (pOSComp && *pOSComp)
		{
			wcstombs(szOSComps, pOSComp, MAX_BUFFER_SIZE);
		}

		return Init(szModName, szCmdLine, nMinRunTime, szDepends, szOSComps);
	}

	MODULE_DETAIL()
	{
		memset(this, 0, sizeof MODULE_DETAIL);
	}

	~MODULE_DETAIL()
	{
		if (pModuleName)
		{
			delete [] pModuleName;
		}

		if (pCommandLine)
		{
			delete [] pCommandLine;
		}

		if (pDependency)
		{
			delete [] pDependency;
		}

		if (pOSComponents)
		{
			delete [] pOSComponents;
		}
	}
};


typedef MODULE_DETAIL *PMODULE_DETAIL;
typedef MODULE_DETAIL *PNODE;
typedef MODULE_DETAIL NODE;


/*

@class:	_modulelist_t, Implements routines to handle module list containing
all available stress modules (only the runnable ones)

@note:	None

@fault:	None

@pre:	None

@post:	None

*/

class _modulelist_t
{
	PNODE _pHead, _pCurrent;
	INT _nLength;

public:
	_modulelist_t();
	~_modulelist_t();
	VOID Add(IN PNODE pNode);
	PNODE RandomNode();
	VOID Reset();
	PNODE Next();
	PNODE Current() { return _pCurrent; }
	inline PNODE operator ++ () { return Next(); }
	#ifdef DEBUG
	VOID DumpStatus();
	#endif /* DEBUG */
	inline INT Length() { return _nLength; }
};


extern HANDLE CreateRandomProcess(OUT PMODULE_DETAIL *ppmd, IN OUT LPDWORD lpdwTickCountAtLaunch, IN INT iSlot);


/*

@class:	_runlist_t, Implements routines to handle runlist containing currently
running stress modules

@note:	All the members of the class are static and naturally creation of an
object of this type is not possible.

@fault:	None

@pre:	None

@post:	None

*/

struct _runlist_t
{
	static _result_t *_pResult;
	static PMODULE_DETAIL *ppmd;					// points to the nodes in the module list, use csRunList & g_csModList to access
	static INT nRunning;							// use csRunList to access
	static INT nLength;								// use csRunList to access
	static DWORD *pdwTickCountAtLaunch;				// use csRunList to access
	static CRITICAL_SECTION csRunList;

protected:
	static HANDLE *_phProcess;						// don't need csRunList to access (used only in RefreshThread)
	static bool _fLaunchNewModule;
	static bool _fInitializedCriticalSection;

public:
	static BOOL Init(IN INT nMaxProcessCount, IN _result_t *_pResult = NULL);
	static DWORD WINAPI RefreshThreadProc(IN LPVOID lpParam);
	static DWORD WINAPI HangDetectorThreadProc(IN LPVOID lpParam);
	static BOOL Uninit();
	static HANDLE GetProcessHandle(IN INT iSlot);

	static inline INT RunningModules()
	{
		CRITICAL_SECTION_TAKER csRunList(&csRunList);
		return nRunning;
	}

	static bool& LaunchNewModule()
	{
		CRITICAL_SECTION_TAKER csRunList(&csRunList);
		return _fLaunchNewModule;
	}

private:
	static HTHREAD _hThread;
	static DWORD _dwThreadID;
	static HTHREAD _hHangDetectorThread;
	static DWORD _dwHangDetectorThreadID;
	static HANDLE _hHangEvent;						// triggers when hang condition is satisfied

private:
	static inline bool SlotAvailable()
	{
		CRITICAL_SECTION_TAKER csRunList(&csRunList);
		return nRunning < nLength;
	}

	static void RearrangeLists(IN INT iSlot);

private:
	_runlist_t();									// default constructor NOT implemented
	~_runlist_t();									// default destructor NOT implemented
	_runlist_t(const _runlist_t&);					// default copy constructor NOT implemented
	_runlist_t& operator = (const _runlist_t&);		// default assignment operator NOT implemented
};


#define __STRESSLIST_H__
#endif /* __STRESSLIST_H__ */
