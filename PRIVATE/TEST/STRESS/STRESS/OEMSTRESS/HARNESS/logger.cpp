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

Module Name: logger.cpp

Abstract: Contains logging routines (history, result)

Notes: None

________________________________________________________________________________
*/

#include <windows.h>
#include <stdio.h>
#include <stressutils.h>
#include <stressutilsp.h>
#include "helpers.h"
#include "logger.h"
#include "stresslist.h"


/**** @devnote: Following tags must be untabified to maintain formatting in the
output and text results file ****/

// tags related to hangs

#define	TAG_PROCHANDLE              L"Hung Process Handle               :   "
#define TAG_HUNGMODNAME             L"Hung Module Name                  :   "
#define TAG_NOTEXITEDFOR            L"Continusly Running For            :   "
#define TAG_CMDLINE                 L"Command Line                      :   "

// tags related to launch parameters

#define TAG_DEBUGGERPRESENT         L"Debugger Attached:                :   "
#define TAG_HANGTIME                L"Hang Detection Time               :   "
#define TAG_HANGOPTION              L"Hang Detection Option             :   "
#define TAG_TOTALRUNTIME            L"Requested Run Time                :   "
#define TAG_WAITMODCOMPLETE         L"Wait for Module Completion        :   "
#define TAG_REFRESHINTERVAL         L"Result Refresh Interval           :   "
#define TAG_MODULEDURATION          L"Module Duration                   :   "
#define TAG_MODULECOUNT             L"Module Count                      :   "
#define TAG_RANDOMSEED              L"Random Seed                       :   "
#define TAG_AUTOOOM                 L"Auto OOM                          :   "
#define TAG_MEMTRACK                L"Memory Tracking                   :   "
#define TAG_RESULTSFILE             L"Results File                      :   "
#define TAG_HISTORYFILE             L"History File                      :   "
#define TAG_INIFILE                 L"Initialization File               :   "
#define TAG_SERVER                  L"Server                            :   "
#define TAG_VERBOSITY               L"Logging Verbosity                 :   "

// module related tags

#define TAG_MODULENAME              L"Module                            :   "
#define TAG_LAUNCHED                L"Launched                          :   "
#define TAG_ITERATIONS              L"Iterations                        :   "
#define TAG_PASSED                  L"Passed                            :   "
#define TAG_WARNINGS1               L"Warnings (level 1)                :   "
#define TAG_WARNINGS2               L"Warnings (level 2)                :   "
#define TAG_FAILED                  L"Failed                            :   "
#define TAG_ABORTED                 L"Aborted                           :   "
#define TAG_TERMINATED              L"Terminated                        :   "
#define TAG_AVGDURATION             L"Avg Duration                      :   "
#define TAG_MAXDURATION             L"Max Duration                      :   "
#define TAG_MINDURATION             L"Min Duration                      :   "

// system details tags

#define TAG_CPU                     L"Target CPU                        :   "
#define TAG_PLATFORM                L"Target Platform                   :   "
#define TAG_OSVERSION               L"Windows CE Version                :   "
#define TAG_BUILDNO                 L"Build No.                         :   "
#define TAG_ADDITIONALINFO          L"Additional Information            :   "
#define TAG_LOCALE                  L"Locale                            :   "

// memory details tags

#define TAG_MEMORYLOAD              L"Overall Memory Usage              :   "
#define TAG_TOTALPHYSICALMEM        L"Total Physical Memory (bytes)     :   "
#define TAG_AVAILABLEPHYSICALMEM    L"Available Physical Memory (bytes) :   "
#define TAG_TOTALPAGEFILE           L"Total Page File (bytes)           :   "
#define TAG_AVAILABLEPAGEFILE       L"Available Page File (bytes)       :   "
#define TAG_TOTALVIRTUALMEM         L"Total Virtual Memory (bytes)      :   "
#define TAG_AVAILABLEVIRTUALMEM     L"Available Virtual Memory (bytes)  :   "

// object store tags

#define TAG_TOTALOBJSTORE           L"Total Object Store Size (bytes)   :   "
#define TAG_FREEOBJSTORE            L"Free Object Store Size (bytes)    :   "

// failure tags

#define TAG_MISSINGFILE             L"Missing Helper File               :   "


#define	OSLE_ROOT				_T("result")
#define	OSLE_COMPLETED				_T("completed")
#define	OSLE_RUNTIME				_T("runtime")
#define	OSLE_MEMTHRESHOLD_HIT		_T("memtrack_threshold_hit")

#define	OSLE_ERROR					_T("error")
#define	OSLE_NO_RUNNABLE_MODULES		_T("no_runnable_modules")
#define	OSLE_MISSING_FILES				_T("missing_files")
#define	OSLE_MISSING_FILENAME				_T("filename")

#define	OSLE_STARTUP_PARAMS			_T("params")
#define	OSLE_DEBUGGER					_T("debugger")
#define	OSLE_HANG_TIME					_T("hang_time")
#define	OSLE_HANG_OPTION				_T("hang_option")
#define	OSLE_TOTAL_RUNTIME				_T("total_runtime")
#define	OSLE_WAITFORMODULECOMPLETION	_T("wait_modcompletion")
#define	OSLE_REFRESH_INTERVAL			_T("refresh_interval")
#define	OSLE_MODULE_DURATION			_T("module_duration")
#define	OSLE_MODULE_COUNT				_T("module_count")
#define	OSLE_RANDOM_SEED				_T("random_seed")
#define	OSLE_AUTOOOM					_T("autooom")
#define	OSLE_MEMTHRESHOLD				_T("memtrack")
#define	OSLE_MEMTHRESHOLD_PERCENT		_T("usage_percent")
#define	OSLE_RESULTS_FILE				_T("results_file")
#define	OSLE_HISTORY_FILE				_T("history_file")
#define	OSLE_INI_FILE					_T("ini_file")
#define	OSLE_SERVER						_T("server")
#define	OSLE_VERBOSITY					_T("verbosity")

#define	OSLE_SYSTEM					_T("system")
#define	OSLE_CPU						_T("cpu")
#define	OSLE_PLATFORM					_T("platform")
#define	OSLE_OSVERSION					_T("osversion")
#define	OSLE_BUILDNO					_T("buildno")
#define	OSLE_ADDITIONAL					_T("additional")
#define	OSLE_LOCALE						_T("locale")

#define	OSLE_MEMORY						_T("memory")
#define	OSLE_TOTALPHYSICAL					_T("totalphys")
#define	OSLE_TOTALPAGEFILE					_T("totalpagefile")
#define	OSLE_TOTALVIRTUAL					_T("totalvirtual")

#define	OSLE_OBJSTORE					_T("objstore")
#define	OSLE_STORESIZE						_T("storesize")

#define	OSLE_ATSTART					_T("start")
#define	OSLE_NOW						_T("now")

#define	OSLE_LOAD								_T("load")
#define	OSLE_AVAILABLEPHYSICAL					_T("availphys")
#define	OSLE_AVAILABLEPAGEFILE					_T("availpagefile")
#define	OSLE_AVAILABLEVIRTUAL					_T("availvirtual")

#define	OSLE_FREESIZE							_T("freesize")

#define	OSLE_HANGSTATUS			_T("hangstatus")
#define	OSLE_HUNGMODULE				_T("hung_module")
#define	OSLE_HPROCESS						_T("hproc")
#define	OSLE_PROCESSNAME					_T("name")
#define	OSLE_PROCESSCMDLINE					_T("cmdline")
#define	OSLE_PROCESSRUNNINGFOR				_T("notexitedfor")

#define	OSLE_MODULES				_T("modules")
#define	OSLE_MODULE						_T("module")
#define	OSLE_MODULENAME						_T("name")
#define	OSLE_LAUNCHCOUNT					_T("launch")
#define	OSLE_ITERATIONS						_T("iteration")
#define	OSLE_PASSCOUNT						_T("pass")
#define	OSLE_WARNING1						_T("warning1")
#define	OSLE_WARNING2						_T("warning2")
#define	OSLE_FAILCOUNT						_T("fail")
#define	OSLE_ABORTCOUNT						_T("abort")
#define	OSLE_TERMINATECOUNT					_T("terminate")
#define	OSLE_AVGDURATION					_T("avg_duration")
#define	OSLE_MAXDURATION					_T("max_duration")
#define	OSLE_MINDURATION					_T("min_duration")


extern CRITICAL_SECTION g_csModList;
extern _modulelist_t g_ModList;
extern DWORD g_dwTickCountAtStart;
extern MEMORYSTATUS g_msAtStart;
extern STORE_INFORMATION g_siAtStart;


extern "C" int U_ropen(const WCHAR *, UINT);
extern "C" int U_rread(int, BYTE *, int);
extern "C" int U_rwrite(int, BYTE *, int);
extern "C" int U_rlseek(int, int, int);
extern "C" int U_rclose(int);


//______________________________________________________________________________


_ceshfile_t::_ceshfile_t(IN LPCWSTR lpszFileName, IN UINT uOpenFlags)
{
	_fd = 0;
	memset(_szFileName, 0, sizeof _szFileName);

	if (lpszFileName)
	{
		wcscpy(_szFileName, lpszFileName);
	}

	_uOpenFlags = uOpenFlags;
	_fFileOpen = false;
}


_ceshfile_t::~_ceshfile_t(void)
{
	if (_fFileOpen)
	{
		U_rclose(_fd);
		_fd = 0;
		_fFileOpen = false;
	}
}


bool _ceshfile_t::Open()
{
	_fd = U_ropen(_szFileName, _uOpenFlags);

	if (_fd == 0 || _fd == -1)
	{
		_fFileOpen = false;
		return false;
	}

	_fFileOpen = true;
	return true;
}


bool _ceshfile_t::Close()
{
	if (_fFileOpen)
	{
		U_rclose(_fd);
		_fd = 0;
		_fFileOpen = false;
	}
	else
	{
		return false;
	}

	return true;
}


bool _ceshfile_t::Truncate()
{
	bool fOk = true;

	if (!_fFileOpen)
	{
		_fd = U_ropen(_szFileName, U_O_WRONLY | U_O_CREAT | U_O_APPEND | U_O_TRUNC);

		if (_fd != 0 && _fd != -1)
		{
			_fFileOpen = true;
			fOk = Close();
		}
		else
		{
			fOk = false;
		}
	}
	else
	{
		fOk = false;
	}

	return fOk;
}


DWORD _ceshfile_t::Read(IN OUT void* lpBuf, IN DWORD dwCount)
{
	if (_fFileOpen)
	{
		return U_rread(_fd, (BYTE *)lpBuf, dwCount);
	}

	return -1;
}


bool _ceshfile_t::WriteWideStringInANSI(IN LPCWSTR lpszStr, IN bool fAppend)
{
	char szBuffer[MAX_BUFFER_SIZE];

	if (lpszStr && *lpszStr && (wcslen(lpszStr) < MAX_BUFFER_SIZE) &&
		(wcstombs(NULL, lpszStr, 0) < MAX_BUFFER_SIZE))
	{
		memset(szBuffer, 0, sizeof szBuffer);
		wcstombs(szBuffer, lpszStr, MAX_BUFFER_SIZE);
		if (fAppend)
		{
			SeekToEnd();
		}
		return Write((void *)szBuffer, strlen(szBuffer));
	}

	return false;
}


bool _ceshfile_t::Write(IN const void* lpBuf, IN DWORD dwCount)
{
	if (_fFileOpen)
	{
		if (U_rwrite(_fd, (BYTE *)lpBuf, dwCount) != (INT)dwCount)
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	return true;
}


bool _ceshfile_t::Seek(IN INT nOffset, IN INT nOrigin)
{
	if (_fFileOpen)
	{
		if (U_rlseek(_fd, nOffset, nOrigin) == -1L)
		{
			return false;
		}
	}
	else
	{
		return false;
	}
	return true;
}


DWORD _ceshfile_t::GetLength(void)
{
	DWORD dwLength = -1;

	if (_fFileOpen)
	{
		if (SeekToBegin())
		{
			dwLength = U_rlseek(_fd, 0, U_SEEK_END);
		}
	}

	return dwLength;
}


//______________________________________________________________________________


_file_t::_file_t(IN LPCWSTR lpszFileName, IN LPCWSTR lpszMode)
{
	_stream = 0;

	memset(_szFileName, 0, sizeof _szFileName);

	if (lpszFileName)
	{
		wcscpy(_szFileName, lpszFileName);
	}

	memset(_szMode, 0, sizeof _szMode);

	if (lpszMode)
	{
		wcscpy(_szMode, lpszMode);
	}
}


_file_t::~_file_t(void)
{
	if (_stream)
	{
		fclose(_stream);
		_stream = 0;
	}
}


bool _file_t::Open()
{
	_stream = _wfopen(_szFileName, _szMode);

	if (!_stream)
	{
		return false;
	}

	return true;
}


bool _file_t::Close()
{
	if (_stream)
	{
		fclose(_stream);;
		_stream = 0;
	}
	else
	{
		return false;
	}

	return true;
}


bool _file_t::Truncate()
{
	bool fOk = true;

	if (!_stream)
	{
		_stream = _wfopen(_szFileName, L"w+");

		if (_stream)
		{
			fOk = Close();
		}
		else
		{
			fOk = false;
		}
	}
	else
	{
		fOk = false;
	}

	return fOk;
}


DWORD _file_t::Read(IN OUT void* lpBuf, IN DWORD dwCount)
{
	if (_stream)
	{
		return fread(lpBuf, (size_t)sizeof(BYTE), (size_t)dwCount, _stream);

	}

	return -1;
}


bool _file_t::WriteWideStringInANSI(IN LPCWSTR lpszStr, IN bool fAppend)
{
	char szBuffer[MAX_BUFFER_SIZE];

	if (lpszStr && *lpszStr && (wcslen(lpszStr) < MAX_BUFFER_SIZE) &&
		(wcstombs(NULL, lpszStr, 0) < MAX_BUFFER_SIZE))
	{
		memset(szBuffer, 0, sizeof szBuffer);
		wcstombs(szBuffer, lpszStr, MAX_BUFFER_SIZE);
		if (fAppend)
		{
			SeekToEnd();
		}
		return Write((void *)szBuffer, strlen(szBuffer));
	}

	return false;
}


bool _file_t::Write(IN const void* lpBuf, IN DWORD dwCount)
{
	if (_stream)
	{
		if (dwCount != fwrite(lpBuf, (size_t)sizeof(BYTE), (size_t)dwCount, _stream))
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	return true;
}


bool _file_t::Seek(IN INT nOffset, IN INT nOrigin)
{
	if (_stream)
	{
		if (fseek(_stream, nOffset, nOrigin))
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	return true;
}


//______________________________________________________________________________


_history_t::_history_t(IN IFile *pFile, IN bool fByRef)
{
	memset(_szBuffer, 0, sizeof _szBuffer);
	_pFile = pFile;
	_fByRef = fByRef;
	_fFileAlreadyAccessed = false;
}


_history_t::~_history_t()
{
	memset(_szBuffer, 0, sizeof _szBuffer);

	if (_pFile && !_fByRef)
	{
		delete _pFile;
	}
}


bool _history_t::LogResults(IN LPCRITICAL_SECTION lpcs)
{
	bool fOk = true;
	MEMORYSTATUS ms;
	STORE_INFORMATION si;

	if (lpcs)
	{
		EnterCriticalSection(lpcs);
	}

	ms.dwLength = sizeof MEMORYSTATUS;

	// get memory & storage status

	GlobalMemoryStatus(&ms);
	GetStoreInformation(&si);

	if (!_fFileAlreadyAccessed)
	{
		if (!_pFile->Truncate())	// file must already not be opened
		{
			DebugDump(L"#### Unable to empty existing history file ####\r\n");
		}
	}

	if (!_pFile->Open())
	{
		fOk = false;
		goto done;
	}

	//__________________________________________________________________________

	if (!_fFileAlreadyAccessed)
	{
		DumpHeader();
		_fFileAlreadyAccessed = true;
	}

	// seconds since start

	memset(_szBuffer, 0, sizeof _szBuffer);
	#ifdef REVIEW
	wsprintf(_szBuffer, L"%u, %u, %u, %u, %u, %u, %u, %u, %u",
		(GetTickCount() - g_dwTickCountAtStart) / SECOND, ms.dwTotalPhys, ms.dwAvailPhys,
		ms.dwTotalPageFile, ms.dwAvailPageFile, ms.dwTotalVirtual, ms.dwAvailVirtual,
		si.dwStoreSize, si.dwFreeSize);
	#endif /* REVIEW */
	wsprintf(_szBuffer, L"%u, %u, %u, %u, %u",
		(GetTickCount() - g_dwTickCountAtStart) / SECOND, ms.dwAvailPhys, ms.dwAvailPageFile,
		ms.dwAvailVirtual, si.dwFreeSize);

	_pFile->WriteWideStringInANSI(_szBuffer);

	EnterCriticalSection(&g_csModList);

	g_ModList.Reset();

	do
	{
		if (g_ModList.Current())
		{
			// modulename is already part of the header

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %u", ((PMODULE_DETAIL)g_ModList.Current())->nLaunchCount);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %u", ((PMODULE_DETAIL)g_ModList.Current())->nIterations);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %u", ((PMODULE_DETAIL)g_ModList.Current())->nPassCount);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %u", ((PMODULE_DETAIL)g_ModList.Current())->nLevel1WarningCount);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %u", ((PMODULE_DETAIL)g_ModList.Current())->nLevel2WarningCount);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %u", ((PMODULE_DETAIL)g_ModList.Current())->nFailCount);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %u", ((PMODULE_DETAIL)g_ModList.Current())->nAbortCount);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %u", ((PMODULE_DETAIL)g_ModList.Current())->nTerminateCount);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %.2f",
				((double)((double)((PMODULE_DETAIL)g_ModList.Current())->dwAverageDuration / SECOND)));
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %.2f",
				((double)((double)((PMODULE_DETAIL)g_ModList.Current())->dwMaxDuration / SECOND)));
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %.2f",
				(UINT_MAX == ((PMODULE_DETAIL)g_ModList.Current())->dwMinDuration) ? 0 : ((double)((double)((PMODULE_DETAIL)g_ModList.Current())->dwMinDuration / SECOND)));
			_pFile->WriteWideStringInANSI(_szBuffer);

			#if 0
			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %u",
				((PMODULE_DETAIL)g_ModList.Current())->dwTickCountFirstLaunched);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L"%u\r\n",
				((PMODULE_DETAIL)g_ModList.Current())->dwTickCountLastLaunched);
			_pFile->WriteWideStringInANSI(_szBuffer);
			#endif /* 0 */
		}
	}
	while (g_ModList.Next());

	LeaveCriticalSection(&g_csModList);

	memset(_szBuffer, 0, sizeof _szBuffer);
	wsprintf(_szBuffer, L"\r\n");
	_pFile->WriteWideStringInANSI(_szBuffer);

	if (!_pFile->Close())
	{
		fOk = false;
	}

done:
	if (lpcs)
	{
		LeaveCriticalSection(lpcs);
	}
	return fOk;
}


void _history_t::DumpHeader()
{
	// assumes file is already opened and to be closed by the caller

	if (_fFileAlreadyAccessed)
	{
		return;
	}

	memset(_szBuffer, 0, sizeof _szBuffer);
	#ifdef REVIEW
	wsprintf(_szBuffer, L"Elasped Time (sec), Total Physical Memory, Available Physical Memory, Total Page File, Available Page File, Total Virtual Memory, Available Virtual Memory, Total Objectstore, Available Objectstore");
	#endif /* REVIEW */
	wsprintf(_szBuffer, L"Elasped Time (sec), Available Physical Memory, Available Page File, Available Virtual Memory, Available Objectstore");
	_pFile->WriteWideStringInANSI(_szBuffer);

	EnterCriticalSection(&g_csModList);

	g_ModList.Reset();

	do
	{
		if (g_ModList.Current())
		{
			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %s(Launched)", ((PMODULE_DETAIL)g_ModList.Current())->pModuleName);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %s(Iterations)", ((PMODULE_DETAIL)g_ModList.Current())->pModuleName);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %s(Passed)", ((PMODULE_DETAIL)g_ModList.Current())->pModuleName);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %s(Level 1 Warnings)", ((PMODULE_DETAIL)g_ModList.Current())->pModuleName);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %s(Level 2 Warnings)", ((PMODULE_DETAIL)g_ModList.Current())->pModuleName);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %s(Failed)", ((PMODULE_DETAIL)g_ModList.Current())->pModuleName);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %s(Aborted)", ((PMODULE_DETAIL)g_ModList.Current())->pModuleName);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %s(Terminated)", ((PMODULE_DETAIL)g_ModList.Current())->pModuleName);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %s(Average Duration)", ((PMODULE_DETAIL)g_ModList.Current())->pModuleName);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %s(Maximum Duration)", ((PMODULE_DETAIL)g_ModList.Current())->pModuleName);
			_pFile->WriteWideStringInANSI(_szBuffer);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L", %s(Minimum Duration)", ((PMODULE_DETAIL)g_ModList.Current())->pModuleName);
			_pFile->WriteWideStringInANSI(_szBuffer);
		}
	}
	while (g_ModList.Next());

	memset(_szBuffer, 0, sizeof _szBuffer);
	wsprintf(_szBuffer, L"\r\n");
	_pFile->WriteWideStringInANSI(_szBuffer);

	LeaveCriticalSection(&g_csModList);

	return;
}


//______________________________________________________________________________


_result_t::_result_t(IN IFile *pFile, IN bool fXML, IN bool fByRef)
{
	memset(_szBuffer, 0, sizeof _szBuffer);
	_pFile = pFile;
	_fXML = fXML;
	_fByRef = fByRef;
}


_result_t::~_result_t()
{
	memset(_szBuffer, 0, sizeof _szBuffer);

	if (_pFile && !_fByRef)
	{
		delete _pFile;
	}
}


bool _result_t::DumpXmlTag(IN LPCWSTR lpszTag, IN XmlTagType type, IN bool fTrailingNewline)
{
	WCHAR szBuffer[MAX_BUFFER_SIZE];

	if (!_pFile && _fXML)
	{
		return false;
	}

	if (!lpszTag || !*lpszTag)
	{
		return false;
	}

	memset(szBuffer, 0, sizeof szBuffer);
	switch (type)
	{
	case BEGIN:
		wsprintf(szBuffer, L"<%s>", lpszTag);
		break;
	case END:
		wsprintf(szBuffer, L"</%s>", lpszTag);
		break;
	case EMPTY:
		wsprintf(szBuffer, L"<%s />", lpszTag);
		break;
	default:
		return false;
	}

	if (fTrailingNewline)
	{
		wcscat(szBuffer, L"\r\n");
	}

	return _pFile->WriteWideStringInANSI(szBuffer);
}


bool _result_t::LogResults(IN LPCRITICAL_SECTION lpcs, IN BOOL fHangStatus,
	IN BOOL fStressCompleted, IN BOOL fMemTrackThresholdHit)
{
	bool fOk = true;
	MEMORYSTATUS ms;
	STORE_INFORMATION si;

	if (!_pFile && _fXML)
	{
		return false;
	}

	if (lpcs)
	{
		EnterCriticalSection(lpcs);
	}

	ms.dwLength = sizeof MEMORYSTATUS;

	// get memory & storage status

	GlobalMemoryStatus(&ms);
	GetStoreInformation(&si);

	if (_pFile && !_pFile->Truncate())	// file must already not be opened
	{
		DebugDump(L"#### Unable to empty existing result file ####\r\n");
	}

	if (_pFile && !_pFile->Open())
	{
		fOk = false;
		goto done;
	}

	//__________________________________________________________________________

	// headers

	if (_pFile && _fXML)
	{
		_pFile->WriteWideStringInANSI(L"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n");
		_pFile->WriteWideStringInANSI(L"<?xml-stylesheet type=\"text/xsl\" href=\"results.xslt\"?>\r\n");
		BeginXmlTag(OSLE_ROOT, true);
	}
	DumpTextToFileAndOutput(L"================================================================================\r\n");

	if (fStressCompleted)
	{
		if (_pFile && _fXML)
		{
			EmptyXmlTag(OSLE_COMPLETED);
		}

		DumpTextToFileAndOutput(L"\r\nOEMStress was completed successfully.\r\n");
	}

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_RUNTIME);
	}
	DumpTextToFileAndOutput(L"\r\n******** OEMStress has run for ");

	DWORD dwTickCountNow;
	dwTickCountNow = (GetTickCount() - g_dwTickCountAtStart);

	if (_pFile && _fXML)	// special case: XML file holds duration in seconds
	{
		memset(_szBuffer, 0, sizeof _szBuffer);
		wsprintf(_szBuffer, L"%u", (dwTickCountNow / SECOND));
		_pFile->WriteWideStringInANSI(_szBuffer);
	}
	swprintf(_szBuffer, L"%u:%u:%u:%u",
		(dwTickCountNow / HOUR),
		((dwTickCountNow % HOUR) / MINUTE),
		(((dwTickCountNow % HOUR) % MINUTE) / SECOND),
		(((dwTickCountNow % HOUR) % MINUTE) % SECOND));
	wcscat(_szBuffer, L" (hh:mm:ss:ms)");
	DumpTextToFileAndOutput(_szBuffer);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_RUNTIME, true);
	}
	DumpTextToFileAndOutput(L" ********\r\n\r\n");

	if (fMemTrackThresholdHit)
	{
		if (_pFile && _fXML)
		{
			EmptyXmlTag(OSLE_MEMTHRESHOLD_HIT);
		}

		if (Options::g_fMemLevelPercent)
		{
			DumpTextToFileAndOutput(L"\r\n#### Device memory usage has exceeded specified value ####\r\n");
		}
		else
		{
			DumpTextToFileAndOutput(L"\r\n#### Available physical memory is less then specified value ####\r\n");
		}

		if (Options::g_fDebuggerPresent)
		{
			DumpTextToFileAndOutput(L"\r\n\r\nOEMStress: Breaking into debugger...\r\n\r\n");
		}
	}

	LogLaunchParams();					// dump launch params
	DumpTextToFileAndOutput(L"\r\n");
	LogSystemDetails();					// dump system details
	if (fHangStatus)
	{
		LogHangStatus();
		DumpTextToFileAndOutput(L"\r\n");
	}

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_MODULES, true);
	}

	DumpTextToFileAndOutput(L"\r\nM O D U L E  D E T A I L S\r\n");
	DumpTextToFileAndOutput(L"==========================\r\n\r\n");

	EnterCriticalSection(&g_csModList);

	g_ModList.Reset();

	do
	{
		if (g_ModList.Current())
		{
			// start of the module

			if (_pFile && _fXML)
			{
				BeginXmlTag(OSLE_MODULE, true);
			}

			// name of the module

			if (_pFile && _fXML)
			{
				BeginXmlTag(OSLE_MODULENAME);
			}
			DumpTextToFileAndOutput(TAG_MODULENAME);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wcscpy(_szBuffer, ((PMODULE_DETAIL)g_ModList.Current())->pModuleName);
			DumpTextToFileAndOutput(_szBuffer, true);

			if (_pFile && _fXML)
			{
				EndXmlTag(OSLE_MODULENAME, true);
			}
			DumpTextToFileAndOutput(L"\r\n");

			// launched

			if (_pFile && _fXML)
			{
				BeginXmlTag(OSLE_LAUNCHCOUNT);
			}
			DumpTextToFileAndOutput(TAG_LAUNCHED);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L"%u", ((PMODULE_DETAIL)g_ModList.Current())->nLaunchCount);
			DumpTextToFileAndOutput(_szBuffer, true);

			if (_pFile && _fXML)
			{
				EndXmlTag(OSLE_LAUNCHCOUNT, true);
			}
			DumpTextToFileAndOutput(L"\r\n");

			// iterations

			if (_pFile && _fXML)
			{
				BeginXmlTag(OSLE_ITERATIONS);
			}
			DumpTextToFileAndOutput(TAG_ITERATIONS);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L"%u", ((PMODULE_DETAIL)g_ModList.Current())->nIterations);
			DumpTextToFileAndOutput(_szBuffer, true);

			if (_pFile && _fXML)
			{
				EndXmlTag(OSLE_ITERATIONS, true);
			}
			DumpTextToFileAndOutput(L"\r\n");

			// pass count

			if (_pFile && _fXML)
			{
				BeginXmlTag(OSLE_PASSCOUNT);
			}
			DumpTextToFileAndOutput(TAG_PASSED);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L"%u", ((PMODULE_DETAIL)g_ModList.Current())->nPassCount);
			DumpTextToFileAndOutput(_szBuffer, true);

			if (_pFile && _fXML)
			{
				EndXmlTag(OSLE_PASSCOUNT, true);
			}
			DumpTextToFileAndOutput(L"\r\n");

			// level 1 warning count

			if (_pFile && _fXML)
			{
				BeginXmlTag(OSLE_WARNING1);
			}
			DumpTextToFileAndOutput(TAG_WARNINGS1);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L"%u", ((PMODULE_DETAIL)g_ModList.Current())->nLevel1WarningCount);
			DumpTextToFileAndOutput(_szBuffer, true);

			if (_pFile && _fXML)
			{
				EndXmlTag(OSLE_WARNING1, true);
			}
			DumpTextToFileAndOutput(L"\r\n");

			// level 2 warning count

			if (_pFile && _fXML)
			{
				BeginXmlTag(OSLE_WARNING2);
			}
			DumpTextToFileAndOutput(TAG_WARNINGS2);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L"%u", ((PMODULE_DETAIL)g_ModList.Current())->nLevel2WarningCount);
			DumpTextToFileAndOutput(_szBuffer, true);

			if (_pFile && _fXML)
			{
				EndXmlTag(OSLE_WARNING2, true);
			}
			DumpTextToFileAndOutput(L"\r\n");

			// fail count

			if (_pFile && _fXML)
			{
				BeginXmlTag(OSLE_FAILCOUNT);
			}
			DumpTextToFileAndOutput(TAG_FAILED);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L"%u", ((PMODULE_DETAIL)g_ModList.Current())->nFailCount);
			DumpTextToFileAndOutput(_szBuffer, true);

			if (_pFile && _fXML)
			{
				EndXmlTag(OSLE_FAILCOUNT, true);
			}
			DumpTextToFileAndOutput(L"\r\n");

			// abort count

			if (_pFile && _fXML)
			{
				BeginXmlTag(OSLE_ABORTCOUNT);
			}
			DumpTextToFileAndOutput(TAG_ABORTED);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L"%u", ((PMODULE_DETAIL)g_ModList.Current())->nAbortCount);
			DumpTextToFileAndOutput(_szBuffer, true);

			if (_pFile && _fXML)
			{
				EndXmlTag(OSLE_ABORTCOUNT, true);
			}
			DumpTextToFileAndOutput(L"\r\n");

			// terminate count

			if (_pFile && _fXML)
			{
				BeginXmlTag(OSLE_TERMINATECOUNT);
			}
			DumpTextToFileAndOutput(TAG_TERMINATED);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L"%u", ((PMODULE_DETAIL)g_ModList.Current())->nTerminateCount);
			DumpTextToFileAndOutput(_szBuffer, true);

			if (_pFile && _fXML)
			{
				EndXmlTag(OSLE_TERMINATECOUNT, true);
			}
			DumpTextToFileAndOutput(L"\r\n");

			// average module runtime in seconds

			if (_pFile && _fXML)
			{
				BeginXmlTag(OSLE_AVGDURATION);
			}
			DumpTextToFileAndOutput(TAG_AVGDURATION);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L"%.2f",
				((double)((double)((PMODULE_DETAIL)g_ModList.Current())->dwAverageDuration / SECOND)));
			DumpTextToFileAndOutput(_szBuffer, true);

			if (_pFile && _fXML)
			{
				EndXmlTag(OSLE_AVGDURATION, true);
			}
			DumpTextToFileAndOutput(L" second(s)\r\n");

			// maximum module runtime in seconds

			if (_pFile && _fXML)
			{
				BeginXmlTag(OSLE_MAXDURATION);
			}
			DumpTextToFileAndOutput(TAG_MAXDURATION);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L"%.2f",
				((double)((double)((PMODULE_DETAIL)g_ModList.Current())->dwMaxDuration / SECOND)));
			DumpTextToFileAndOutput(_szBuffer, true);

			if (_pFile && _fXML)
			{
				EndXmlTag(OSLE_MAXDURATION, true);
			}
			DumpTextToFileAndOutput(L" second(s)\r\n");

			// minimum module runtime in seconds

			if (_pFile && _fXML)
			{
				BeginXmlTag(OSLE_MINDURATION);
			}
			DumpTextToFileAndOutput(TAG_MINDURATION);

			memset(_szBuffer, 0, sizeof _szBuffer);
			wsprintf(_szBuffer, L"%.2f",
				(UINT_MAX == ((PMODULE_DETAIL)g_ModList.Current())->dwMinDuration) ? 0 :
				((double)((double)((PMODULE_DETAIL)g_ModList.Current())->dwMinDuration / SECOND)));
			DumpTextToFileAndOutput(_szBuffer, true);

			if (_pFile && _fXML)
			{
				EndXmlTag(OSLE_MINDURATION, true);
			}
			DumpTextToFileAndOutput(L" second(s)\r\n");

			// end of a module

			if (_pFile && _fXML)
			{
				EndXmlTag(OSLE_MODULE, true);
			}
			DumpTextToFileAndOutput(L"\r\n\r\n");
		}
	}
	while (g_ModList.Next());

	LeaveCriticalSection(&g_csModList);

	// footers

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_MODULES, true);
		EndXmlTag(OSLE_ROOT, true);
	}
	DumpTextToFileAndOutput(L"================================================================================\r\n");

	if (_pFile && !_pFile->Close())
	{
		fOk = false;
	}

done:
	if (lpcs)
	{
		LeaveCriticalSection(lpcs);
	}
	return fOk;
}


void _result_t::LogSystemDetails()
{
	MEMORYSTATUS ms;
	STORE_INFORMATION si;

	if (!_pFile && _fXML)
	{
		return;
	}

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_SYSTEM, true);
	}
	DumpTextToFileAndOutput(L"\r\nS Y S T E M  D E T A I L S\r\n");
	DumpTextToFileAndOutput(L"==========================\r\n\r\n");

	// CPU (TARGET_CPU)

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_CPU);
	}
	DumpTextToFileAndOutput(TAG_CPU);

	memset(_szBuffer, 0, sizeof _szBuffer);
	mbstowcs(_szBuffer, TARGET_CPU, strlen(TARGET_CPU));			// TARGET_CPU is in ANSI
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_CPU, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// Platform (TARGET_PLATFORM)

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_PLATFORM);
	}
	DumpTextToFileAndOutput(TAG_PLATFORM);

	memset(_szBuffer, 0, sizeof _szBuffer);
	mbstowcs(_szBuffer, TARGET_PLATFORM, strlen(TARGET_PLATFORM));	// TARGET_PLATFORM is in ANSI
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_PLATFORM, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// GetVersionEx

	OSVERSIONINFO osvi;
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	if (GetVersionEx(&osvi))
	{
		// value of the dwPlatformID will be VER_PLATFORM_WIN32_CE

		// OS Version

		if (_pFile && _fXML)
		{
			BeginXmlTag(OSLE_OSVERSION);
		}
		DumpTextToFileAndOutput(TAG_OSVERSION);

		memset(_szBuffer, 0, sizeof _szBuffer);
		wsprintf(_szBuffer, L"%u.%u", osvi.dwMajorVersion, osvi.dwMinorVersion);
		DumpTextToFileAndOutput(_szBuffer, true);

		if (_pFile && _fXML)
		{
			EndXmlTag(OSLE_OSVERSION, true);
		}
		DumpTextToFileAndOutput(L"\r\n");

		// Build No.

		if (_pFile && _fXML)
		{
			BeginXmlTag(OSLE_BUILDNO);
		}
		DumpTextToFileAndOutput(TAG_BUILDNO);

		memset(_szBuffer, 0, sizeof _szBuffer);
		wsprintf(_szBuffer, L"%u", osvi.dwBuildNumber);
		DumpTextToFileAndOutput(_szBuffer, true);

		if (_pFile && _fXML)
		{
			EndXmlTag(OSLE_BUILDNO, true);
		}
		DumpTextToFileAndOutput(L"\r\n");

		// Additional Information

		if (_pFile && _fXML)
		{
			BeginXmlTag(OSLE_ADDITIONAL);
		}
		DumpTextToFileAndOutput(TAG_ADDITIONALINFO);

		DumpTextToFileAndOutput(osvi.szCSDVersion, true);

		if (_pFile && _fXML)
		{
			EndXmlTag(OSLE_ADDITIONAL, true);
		}
		DumpTextToFileAndOutput(L"\r\n");
	}

	// GetLocaleInfo

	memset(_szBuffer, 0, sizeof _szBuffer);
   	INT nRetVal = GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_SABBREVLANGNAME,
   		_szBuffer, (sizeof _szBuffer / sizeof _szBuffer[0]));

   	if (nRetVal && (ERROR_INSUFFICIENT_BUFFER != nRetVal) &&
   		(ERROR_INVALID_FLAGS != nRetVal) && (ERROR_INVALID_PARAMETER != nRetVal))
   	{
		// Language (locale)

		if (_pFile && _fXML)
		{
			BeginXmlTag(OSLE_LOCALE);
		}
		DumpTextToFileAndOutput(TAG_LOCALE);

		DumpTextToFileAndOutput(_szBuffer, true);

		if (_pFile && _fXML)
		{
			EndXmlTag(OSLE_LOCALE, true);
		}
		DumpTextToFileAndOutput(L"\r\n");
   	}

	// get current memory & storage status

	ms.dwLength = sizeof MEMORYSTATUS;
	GlobalMemoryStatus(&ms);
	GetStoreInformation(&si);

	// total memory parameters

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_MEMORY, true);
	}

	// total physical memory

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_TOTALPHYSICAL);
	}
	DumpTextToFileAndOutput(TAG_TOTALPHYSICALMEM);

	memset(_szBuffer, 0, sizeof _szBuffer);
	wsprintf(_szBuffer, L"%u", ms.dwTotalPhys);
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_TOTALPHYSICAL, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// total page file

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_TOTALPAGEFILE);
	}
	DumpTextToFileAndOutput(TAG_TOTALPAGEFILE);

	memset(_szBuffer, 0, sizeof _szBuffer);
	wsprintf(_szBuffer, L"%u", ms.dwTotalPageFile);
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_TOTALPAGEFILE, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// total virtual memory

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_TOTALVIRTUAL);
	}
	DumpTextToFileAndOutput(TAG_TOTALVIRTUALMEM);

	memset(_szBuffer, 0, sizeof _szBuffer);
	wsprintf(_szBuffer, L"%u", ms.dwTotalVirtual);
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_TOTALVIRTUAL, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_MEMORY, true);
	}

	// total object store parameters

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_OBJSTORE, true);
	}

	// store size

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_STORESIZE);
	}
	DumpTextToFileAndOutput(TAG_TOTALOBJSTORE);

	memset(_szBuffer, 0, sizeof _szBuffer);
	wsprintf(_szBuffer, L"%u", si.dwStoreSize);
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_STORESIZE, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_OBJSTORE, true);
	}

	//__________________________________________________________________________

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_ATSTART, true);
	}
	DumpTextToFileAndOutput(L"\r\n________AT START________\r\n\r\n");

	LogMemoryAndObjectStoreValues(g_msAtStart, g_siAtStart);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_ATSTART, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	//__________________________________________________________________________

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_NOW, true);
	}
	DumpTextToFileAndOutput(L"\r\n________CURRENT________\r\n\r\n");

	LogMemoryAndObjectStoreValues(ms, si);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_NOW, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	//__________________________________________________________________________

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_SYSTEM, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	return;
}


void _result_t::LogMemoryAndObjectStoreValues(IN MEMORYSTATUS& ms, IN STORE_INFORMATION& si)
{
	if (!_pFile && _fXML)
	{
		return;
	}

	//__________________________________________________________________________
	// memory

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_MEMORY, true);
	}
	#ifdef NEVER
	DumpTextToFileAndOutput(L"\r\nMEMORY STATUS\r\n");
	#endif /* NEVER */

	// memory load

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_LOAD);
	}
	DumpTextToFileAndOutput(TAG_MEMORYLOAD);

	memset(_szBuffer, 0, sizeof _szBuffer);
	wsprintf(_szBuffer, L"%u", ms.dwMemoryLoad);
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_LOAD, true);
	}
	DebugDump(L"%%");	
	DumpTextToFileAndOutput(L"%\r\n");

	// available physical memory

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_AVAILABLEPHYSICAL);
	}
	DumpTextToFileAndOutput(TAG_AVAILABLEPHYSICALMEM);

	memset(_szBuffer, 0, sizeof _szBuffer);
	wsprintf(_szBuffer, L"%u", ms.dwAvailPhys);
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_AVAILABLEPHYSICAL, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// available page file

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_AVAILABLEPAGEFILE);
	}
	DumpTextToFileAndOutput(TAG_AVAILABLEPAGEFILE);

	memset(_szBuffer, 0, sizeof _szBuffer);
	wsprintf(_szBuffer, L"%u", ms.dwAvailPageFile);
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_AVAILABLEPAGEFILE, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// available virtual memory

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_AVAILABLEVIRTUAL);
	}
	DumpTextToFileAndOutput(TAG_AVAILABLEVIRTUALMEM);

	memset(_szBuffer, 0, sizeof _szBuffer);
	wsprintf(_szBuffer, L"%u", ms.dwAvailVirtual);
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_AVAILABLEVIRTUAL, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_MEMORY, true);
	}

	//__________________________________________________________________________
	// object store

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_OBJSTORE, true);
	}
	#ifdef NEVER
	DumpTextToFileAndOutput(L"\r\nOBJECT STORE STATUS\r\n");
	#endif /* NEVER */

	// free store size

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_FREESIZE);
	}
	DumpTextToFileAndOutput(TAG_FREEOBJSTORE);

	memset(_szBuffer, 0, sizeof _szBuffer);
	wsprintf(_szBuffer, L"%u", si.dwFreeSize);
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_FREESIZE, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_OBJSTORE, true);
	}

	return;
}


void _result_t::LogLaunchParams()
{
	if (!_pFile && _fXML)
	{
		return;
	}

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_STARTUP_PARAMS, true);
	}
	DumpTextToFileAndOutput(L"\r\nL A U N C H  P A R A M E T E R S\r\n");
	DumpTextToFileAndOutput(L"================================\r\n\r\n");

	// debugger status

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_DEBUGGER);
	}
	DumpTextToFileAndOutput(TAG_DEBUGGERPRESENT);

	DumpTextToFileAndOutput(Options::g_fDebuggerPresent ? L"YES" : L"NO", true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_DEBUGGER, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// hang time

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_HANG_TIME);
	}
	DumpTextToFileAndOutput(TAG_HANGTIME);

	memset(_szBuffer, 0, sizeof _szBuffer);
	if (Options::g_nHangTimeoutMinutes)
	{
		wsprintf(_szBuffer, L"%u", Options::g_nHangTimeoutMinutes);
		DumpTextToFileAndOutput(_szBuffer, true);
	}
	else
	{
		DumpTextToFileAndOutput(L"DISABLED");	// special case: xml tag left blank
	}

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_HANG_TIME, true);
	}
	DumpTextToFileAndOutput(Options::g_nHangTimeoutMinutes ? L" minute(s)\r\n" : L"\r\n");

	// hang option

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_HANG_OPTION);
	}
	DumpTextToFileAndOutput(TAG_HANGOPTION);

	DumpTextToFileAndOutput(Options::g_fAllModulesHang ? L"ALL MODULES" : L"ANY MODULE", true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_HANG_OPTION, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// requested run time (harness)

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_TOTAL_RUNTIME);
	}
	DumpTextToFileAndOutput(TAG_TOTALRUNTIME);

	memset(_szBuffer, 0, sizeof _szBuffer);
	if (!Options::g_nRunTimeMinutes || (UINT_MAX == Options::g_nRunTimeMinutes))
	{
		DumpTextToFileAndOutput(L"FOREVER"); // special case: xml tag left blank
	}
	else
	{
		wsprintf(_szBuffer, L"%u", Options::g_nRunTimeMinutes);
		DumpTextToFileAndOutput(_szBuffer, true);
	}

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_TOTAL_RUNTIME, true);
	}
	DumpTextToFileAndOutput((!Options::g_nRunTimeMinutes || (UINT_MAX == Options::g_nRunTimeMinutes)) ? L"\r\n" : L" minute(s)\r\n");

	// wait for running module completion

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_WAITFORMODULECOMPLETION);
	}
	DumpTextToFileAndOutput(TAG_WAITMODCOMPLETE);

	if ((Options::INDEFINITE == Options::g_fModuleDuration) || (Options::RANDOM == Options::g_fModuleDuration))
	{
		DumpTextToFileAndOutput(L"DISABLED", true);
	}
	else
	{
		DumpTextToFileAndOutput(Options::g_fWaitForModuleCompletion ? L"YES" : L"NO", true);
	}

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_WAITFORMODULECOMPLETION, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// result refresh interval

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_REFRESH_INTERVAL);
	}
	DumpTextToFileAndOutput(TAG_REFRESHINTERVAL);

	memset(_szBuffer, 0, sizeof _szBuffer);
	wsprintf(_szBuffer, L"%u", Options::g_nRefreshResultsIntervalMinutes);
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_REFRESH_INTERVAL, true);
	}
	DumpTextToFileAndOutput(L" minute(s)\r\n");

	// module duration

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_MODULE_DURATION);
	}
	DumpTextToFileAndOutput(TAG_MODULEDURATION);

	memset(_szBuffer, 0, sizeof _szBuffer);

	if (Options::INDEFINITE == Options::g_fModuleDuration)
	{
		wsprintf(_szBuffer, L"INDEFINITE");
	}
	else if (Options::RANDOM == Options::g_fModuleDuration)
	{
		wsprintf(_szBuffer, L"RANDOM");
	}
	else if (Options::MINIMUM == Options::g_fModuleDuration)
	{
		wsprintf(_szBuffer, L"MINIMUM");
	}
	else	// Options::NORMAL
	{
		wsprintf(_szBuffer, L"%u", Options::g_nModuleDurationMinutes);
	}
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_MODULE_DURATION, true);
	}
	DumpTextToFileAndOutput((Options::NORMAL == Options::g_fModuleDuration) ? L" minute(s)\r\n" : L"\r\n");

	// module count

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_MODULE_COUNT);
	}
	DumpTextToFileAndOutput(TAG_MODULECOUNT);

	memset(_szBuffer, 0, sizeof _szBuffer);
	wsprintf(_szBuffer, L"%u", Options::g_nProcessSlots);
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_MODULE_COUNT, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// random seed

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_RANDOM_SEED);
	}
	DumpTextToFileAndOutput(TAG_RANDOMSEED);

	memset(_szBuffer, 0, sizeof _szBuffer);
	wsprintf(_szBuffer, L"%u", Options::g_nRandomSeed);
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_RANDOM_SEED, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// auto oom

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_AUTOOOM);
	}
	DumpTextToFileAndOutput(TAG_AUTOOOM);

	DumpTextToFileAndOutput(Options::g_fAutoOOM ? L"ENABLED" : L"DISABLED", true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_AUTOOOM, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// memory tracking

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_MEMTHRESHOLD);
	}
	DumpTextToFileAndOutput(TAG_MEMTRACK);

	memset(_szBuffer, 0, sizeof _szBuffer);
	wsprintf(_szBuffer, L"%u", Options::g_nMemThreshold);
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_MEMTHRESHOLD, true);
	}

	if (Options::g_fMemLevelPercent)
	{
		if (_pFile && _fXML)
		{
			EmptyXmlTag(OSLE_MEMTHRESHOLD_PERCENT);
		}
		DebugDump(L"%% ");	
		DumpTextToFileAndOutput(L"% (when memory usage exceeds)");
	}
	else
	{
		DumpTextToFileAndOutput(L" bytes (when available memory falls below)");
	}
	DumpTextToFileAndOutput(L"\r\n");

	// results filename

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_RESULTS_FILE);
	}
	DumpTextToFileAndOutput(TAG_RESULTSFILE);

	DumpTextToFileAndOutput(Options::g_szResultFile, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_RESULTS_FILE, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// history filename

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_HISTORY_FILE);
	}
	DumpTextToFileAndOutput(TAG_HISTORYFILE);

	DumpTextToFileAndOutput(Options::g_szHistoryFile, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_HISTORY_FILE, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// initialization filename

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_INI_FILE);
	}
	DumpTextToFileAndOutput(TAG_INIFILE);

	DumpTextToFileAndOutput(Options::g_szIniFile, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_INI_FILE, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// server name

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_SERVER);
	}
	DumpTextToFileAndOutput(TAG_SERVER);

	DumpTextToFileAndOutput(Options::g_szServer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_SERVER, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// verbosity

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_VERBOSITY);
	}
	DumpTextToFileAndOutput(TAG_VERBOSITY);

	memset(_szBuffer, 0, sizeof _szBuffer);

	switch (Options::g_Verbosity.GetValue())	// OEMStress does not use zones etc.
	{
	case SLOG_ABORT:
		wcscpy(_szBuffer, L"Abort");
		break;
	case SLOG_FAIL:
		wcscpy(_szBuffer, L"Failure and Abort");
		break;
	case SLOG_WARN1:
		wcscpy(_szBuffer, L"Failure, Abort and Warning1");
		break;
	case SLOG_WARN2:
		wcscpy(_szBuffer, L"Failure, Abort, Warning1 and Warning2");
		break;
	case SLOG_COMMENT:
		wcscpy(_szBuffer, L"Failure, Abort, Warning1, Warning2 and Comment");
		break;
	case SLOG_VERBOSE:
		wcscpy(_szBuffer, L"Verbose");
		break;
	case SLOG_ALL:
		wcscpy(_szBuffer, L"All");
		break;
	default:
		wcscpy(_szBuffer, L"UNDEFINED");
		break;
	}
	DumpTextToFileAndOutput(_szBuffer, true);

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_VERBOSITY, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_STARTUP_PARAMS, true);
	}

	return;
}


void _result_t::LogHangStatus()
{
	if (!_pFile && _fXML)
	{
		return;
	}

	if (Options::g_nHangTimeoutMinutes)	// zero value disables the hang detection & hang logging
	{
		if (_pFile && _fXML)
		{
			BeginXmlTag(OSLE_HANGSTATUS, true);
		}
		DumpTextToFileAndOutput(L"\r\nH A N G  S T A T U S\r\n");
		DumpTextToFileAndOutput(L"====================\r\n\r\n");

		CRITICAL_SECTION_TAKER RunListCS(&_runlist_t::csRunList);

		for (INT i = 0; i < _runlist_t::nRunning; i++)
		{
			if (_runlist_t::ppmd[i])
			{
				if ((GetTickCount() - _runlist_t::pdwTickCountAtLaunch[i]) >= (Options::g_nHangTimeoutMinutes * MINUTE))
				{
					// current module is hanging

					if (_pFile && _fXML)
					{
						BeginXmlTag(OSLE_HUNGMODULE, true);
					}

					// handle to the process

					if (_pFile && _fXML)
					{
						BeginXmlTag(OSLE_HPROCESS);
					}
					DumpTextToFileAndOutput(TAG_PROCHANDLE);

					memset(_szBuffer, 0, sizeof _szBuffer);
					wsprintf(_szBuffer, L"0x%08X", (DWORD)_runlist_t::GetProcessHandle(i));
					DumpTextToFileAndOutput(_szBuffer, true);

					if (_pFile && _fXML)
					{
						EndXmlTag(OSLE_HPROCESS, true);
					}
					DumpTextToFileAndOutput(L"\r\n");

					// module name

					if (_pFile && _fXML)
					{
						BeginXmlTag(OSLE_PROCESSNAME);
					}
					DumpTextToFileAndOutput(TAG_HUNGMODNAME);

					EnterCriticalSection(&g_csModList);
					DumpTextToFileAndOutput(_runlist_t::ppmd[i]->pModuleName, true);
					LeaveCriticalSection(&g_csModList);

					if (_pFile && _fXML)
					{
						EndXmlTag(OSLE_PROCESSNAME, true);
					}
					DumpTextToFileAndOutput(L"\r\n");

					// command line

					if (_pFile && _fXML)
					{
						BeginXmlTag(OSLE_PROCESSCMDLINE);
					}
					DumpTextToFileAndOutput(TAG_CMDLINE);

					EnterCriticalSection(&g_csModList);
					DumpTextToFileAndOutput(_runlist_t::ppmd[i]->pCommandLine, true);
					LeaveCriticalSection(&g_csModList);

					if (_pFile && _fXML)
					{
						EndXmlTag(OSLE_PROCESSCMDLINE, true);
					}
					DumpTextToFileAndOutput(L"\r\n");

					// continuously running for (notexitedfor)

					if (_pFile && _fXML)
					{
						BeginXmlTag(OSLE_PROCESSRUNNINGFOR);
					}
					DumpTextToFileAndOutput(TAG_NOTEXITEDFOR);

					memset(_szBuffer, 0, sizeof _szBuffer);
					wsprintf(_szBuffer, L"%u", (GetTickCount() - _runlist_t::pdwTickCountAtLaunch[i]) / MINUTE);
					DumpTextToFileAndOutput(_szBuffer, true);

					if (_pFile && _fXML)
					{
						EndXmlTag(OSLE_PROCESSRUNNINGFOR, true);
					}
					DumpTextToFileAndOutput(L" minute(s)\r\n");

					if (_pFile && _fXML)
					{
						EndXmlTag(OSLE_HUNGMODULE, true);
					}
					DumpTextToFileAndOutput(L"\r\n\r\n");
				}
			}
		}

		if (_pFile && _fXML)
		{
			EndXmlTag(OSLE_HANGSTATUS, true);
		}
	}

	return;
}


/*

@func:	_result_t.DumpTextToFileAndOutput, Dumps information to both text file
(not xml) and the debug output

@rdesc:	void

@param:	IN LPCWSTR lpszStr: The buffer to dump

@param:	IN bool fBypassXmlModeCheck: If true, the function does not internally
checks _fXML flag

@fault:	None

@pre:	None

@post:	None

@note:	None

*/

void _result_t::DumpTextToFileAndOutput(IN LPCWSTR lpszStr, IN bool fBypassXmlModeCheck)
{
	if (fBypassXmlModeCheck && _pFile)
	{
		_pFile->WriteWideStringInANSI(lpszStr);
	}
	else if (!_fXML && _pFile)
	{
		_pFile->WriteWideStringInANSI(lpszStr);
	}

	DebugDump(lpszStr);

	return;
}


bool _result_t::LogFailure(IN bool fNoRunnableModules)
{
	bool fOk = true;

	if (!_pFile && _fXML)
	{
		return false;
	}

	if (_pFile && !_pFile->Truncate())	// file must already not be opened
	{
		DebugDump(L"#### Unable to empty existing result file ####\r\n");
	}

	if (_pFile && !_pFile->Open())
	{
		fOk = false;
		goto done;
	}

	//__________________________________________________________________________

	// headers

	if (_pFile && _fXML)
	{
		_pFile->WriteWideStringInANSI(L"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n");
		_pFile->WriteWideStringInANSI(L"<?xml-stylesheet type=\"text/xsl\" href=\"results.xslt\"?>\r\n");
		BeginXmlTag(OSLE_ROOT, true);
	}
	DumpTextToFileAndOutput(L"================================================================================\r\n");

	LogLaunchParams();					// dump launch params
	DumpTextToFileAndOutput(L"\r\n");

	if (_pFile && _fXML)
	{
		BeginXmlTag(OSLE_ERROR, true);
	}

	if (fNoRunnableModules)
	{
		if (_pFile && _fXML)
		{
			EmptyXmlTag(OSLE_NO_RUNNABLE_MODULES);
		}
		DumpTextToFileAndOutput(L"\r\n\r\n#### NONE OF THE AVAILABLE MODULES CAN RUN ON CURRENT PLATFORM ####\r\n\r\n");
	}

	if (CETK::g_hMemMissingFiles)	// CETK only
	{
		if (_pFile && _fXML)
		{
			BeginXmlTag(OSLE_MISSING_FILES);
		}
		DumpTextToFileAndOutput(L"\r\n\r\n#### FOLLOWING FILES ARE MISSING FORM THE ROOT DIRECTORY ####\r\n");
		DumpTextToFileAndOutput(L"=============================================================\r\n\r\n");

		LPWSTR lpszw = (WCHAR *)LocalLock(CETK::g_hMemMissingFiles);	// ref: +0

		INT nSize = (wcslen(lpszw) + 1) * sizeof(WCHAR);
		HLOCAL hMemFileList = LocalAlloc(LMEM_ZEROINIT, nSize);

		if (!hMemFileList)
		{
			LocalUnlock(CETK::g_hMemMissingFiles);						// ref: -0
			fOk = false;
			goto done;
		}

		LPWSTR lpszFileList = (WCHAR *)LocalLock(hMemFileList);			// ref:	+1
		wcscpy(lpszFileList, lpszw);

		LocalUnlock(CETK::g_hMemMissingFiles);							// ref: -0

		LPTSTR lpwszToken;
		lpwszToken = wcstok(lpszFileList, L"*"); // read the value

		while (lpwszToken)
		{
			if (_pFile && _fXML)
			{
				BeginXmlTag(OSLE_MISSING_FILENAME);
			}
			DumpTextToFileAndOutput(TAG_MISSINGFILE);

			DumpTextToFileAndOutput(lpwszToken, true);

			if (_pFile && _fXML)
			{
				EndXmlTag(OSLE_MISSING_FILENAME, true);
			}
			DumpTextToFileAndOutput(L"\r\n");

			lpwszToken = wcstok(NULL, L"*"); // read the value
		}
		LocalUnlock(hMemFileList);										// ref: -1

		if (_pFile && _fXML)
		{
			EndXmlTag(OSLE_MISSING_FILES, true);
		}
	}

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_ERROR, true);
	}
	DumpTextToFileAndOutput(L"\r\n");

	// footers

	if (_pFile && _fXML)
	{
		EndXmlTag(OSLE_ROOT, true);
	}
	DumpTextToFileAndOutput(L"================================================================================\r\n");

	if (_pFile && !_pFile->Close())
	{
		fOk = false;
	}

done:
	return fOk;
}
