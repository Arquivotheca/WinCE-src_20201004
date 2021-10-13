//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef	_RESULTSMMF_HXX


#define DEFAULT_RESULTS_FILE_SIZE	(sizeof(STRESS_RESULTS) * 32)


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

};


 
CResultsMMFile::CResultsMMFile(LPTSTR lpszName, UINT uSize)
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
	}

	_hMutex = CreateMutex(NULL, FALSE, lpszName);	
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
	
	memcpy((void*) &_pResults[iSlot], (void*) pRes, sizeof(STRESS_RESULTS));

	return;
}



void CResultsMMFile::Read(STRESS_RESULTS* pRes, UINT iSlot)
{
	/* Gain access to the mutex before accessing the shared data. Creating a local 
	object of CAccessMutex ensures that the constructor and destructor get called 
	before and after the return statement, respectively. */

	CAccessMutex objAccessMutex(_hMutex);
	
	memcpy((void*) pRes, (void*) &_pResults[iSlot], sizeof(STRESS_RESULTS));

	return;
}



void CResultsMMFile::Clear(UINT iSlot)
{
	/* Gain access to the mutex before accessing the shared data. Creating a local 
	object of CAccessMutex ensures that the constructor and destructor get called 
	before and after the return statement, respectively. */

	CAccessMutex objAccessMutex(_hMutex);
	
	ZeroMemory((void*) &_pResults[iSlot], sizeof(STRESS_RESULTS));

	return;
}

#define	_RESULTSMMF_HXX
#endif
