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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef	_RESULTSMMF_HXX

#define DEFAULT_RESULTS_FILE_COUNT	(32)
#define DEFAULT_RESULTS_FILE_SIZE	(sizeof(STRESS_RESULTS) * DEFAULT_RESULTS_FILE_COUNT)


class CAccessMutex {
protected:
	HANDLE _hAccessMutex;

public:
	CAccessMutex::CAccessMutex(HANDLE hMutex)
	{
		_hAccessMutex = hMutex;
		
		WaitForSingleObject(_hAccessMutex, INFINITE);
	};

	CAccessMutex::~CAccessMutex(void) 
	{
		ReleaseMutex(_hAccessMutex);
	};
};


class CResultsMMFile {

public:
	CResultsMMFile (LPTSTR lpszName, UINT uSize);
	~CResultsMMFile (void);

	void Record (STRESS_RESULTS* pRes, UINT iSlot);
	void Read (STRESS_RESULTS* pRes, UINT iSlot);
	void Clear (UINT iSlot);

protected:
	HANDLE			_hMap;
	STRESS_RESULTS *_pResults;
	HANDLE			_hMutex;
	DWORD			_dwInitError;
	DWORD			_dwMutexError;

};


 
CResultsMMFile::CResultsMMFile(LPTSTR lpszName, UINT uSize) :
	_dwInitError(ERROR_SUCCESS),
	_dwMutexError(ERROR_SUCCESS)
{
	_hMap = CreateFileMapping (
							(void *)0xFFFFFFFF, 
							NULL, 
							PAGE_READWRITE, 
							0x00, 
							//sizeof (STRESS_RESULTS) * uSize,
							DEFAULT_RESULTS_FILE_SIZE,
							lpszName
							);

	if (_hMap) {
			
		_pResults = (STRESS_RESULTS *) MapViewOfFile (
													_hMap, 
													FILE_MAP_ALL_ACCESS, 
													0x00, 
													0x00, 
													//sizeof (STRESS_RESULTS) * uSize
													DEFAULT_RESULTS_FILE_SIZE
													); 

		if( _pResults == NULL )
		{
			_dwInitError = GetLastError();
			LogFail(_T("Failed to create view of MemoryMapped file (GLE=%d)."),
				_dwInitError);
		}
	}
	else
	{
		_dwInitError = GetLastError();
		LogFail(_T("Failed to create MemoryMapped file (GLE=%d)."),
			_dwInitError);
	}

	_hMutex = CreateMutex(NULL, FALSE, lpszName);
	if( _hMutex == NULL )
	{
		_dwMutexError = GetLastError();
		LogFail(_T("Failed to create mutex for MemoryMapped file (GLE=%d)."),
			_dwMutexError);		
	}
}


 
CResultsMMFile::~CResultsMMFile(void)
{

	if (_pResults) {

		UnmapViewOfFile(_pResults);
		_pResults = NULL;
	}

	if (_hMap) {

		CloseHandle(_hMap);
		_hMap = NULL;
	}

	if (_hMutex) {

		CloseHandle(_hMutex);
		_hMutex = NULL;
	}
}



void CResultsMMFile::Record(STRESS_RESULTS* pRes, UINT iSlot)
{
	/* Gain access to the mutex before accessing the shared data. Creating a local 
	object of CAccessMutex ensures that the constructor and destructor get called 
	before and after the return statement, respectively. */

	CAccessMutex objAccessMutex(_hMutex);

	if( _hMap != NULL && 
		_pResults != NULL )
	{
		if( iSlot < DEFAULT_RESULTS_FILE_COUNT )
		{
			memcpy((void*) &_pResults[iSlot], (void*) pRes, sizeof(STRESS_RESULTS));
		}
		else
		{
			LogFail(_T("Invalid RECORD location %d in MemoryMapped file (Max=%d)."),
				iSlot,
				DEFAULT_RESULTS_FILE_COUNT);
		}
	}
	else
		LogFail(_T("Error recording to MemoryMapped file (GLE=%d)."), _dwInitError);

	return;
}



void CResultsMMFile::Read(STRESS_RESULTS* pRes, UINT iSlot)
{
	/* Gain access to the mutex before accessing the shared data. Creating a local 
	object of CAccessMutex ensures that the constructor and destructor get called 
	before and after the return statement, respectively. */

	CAccessMutex objAccessMutex(_hMutex);
	
	if( _hMap != NULL && 
		_pResults != NULL )
	{
		if( iSlot < DEFAULT_RESULTS_FILE_COUNT )
		{
			memcpy((void*) pRes, (void*) &_pResults[iSlot], sizeof(STRESS_RESULTS));
		}
		else
		{
			LogFail(_T("Invalid READ location %d in MemoryMapped file (Max=%d)."),
				iSlot,
				DEFAULT_RESULTS_FILE_COUNT);
		}
	}
	else
		LogFail(_T("Error reading from MemoryMapped file (GLE=%d)."), _dwInitError);
}



void CResultsMMFile::Clear(UINT iSlot)
{
	/* Gain access to the mutex before accessing the shared data. Creating a local 
	object of CAccessMutex ensures that the constructor and destructor get called 
	before and after the return statement, respectively. */

	CAccessMutex objAccessMutex(_hMutex);
	
	if( _hMap != NULL && 
		_pResults != NULL )
	{
		if( iSlot < DEFAULT_RESULTS_FILE_COUNT )
		{
			ZeroMemory((void*) &_pResults[iSlot], sizeof(STRESS_RESULTS));
		}
		else
		{
			LogFail(_T("Invalid CLEAR location %d in MemoryMapped file (Max=%d)."),
				iSlot,
				DEFAULT_RESULTS_FILE_COUNT);
		}

		
	}
	else
		LogFail(_T("Error clearing MemoryMapped file (GLE=%d)."), _dwInitError);

	return;
}

#define	_RESULTSMMF_HXX
#endif
