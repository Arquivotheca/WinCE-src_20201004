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

Module Name: inifile.cpp

Abstract: Contains initialization file parsing routines for OEM stress harness

Notes: None

________________________________________________________________________________
*/

#include <windows.h>
#include <stdio.h>
#include "helpers.h"
#include "logger.h"
#include "stresslist.h"
#include "inifile.h"


namespace Utilities
{
	extern LPSTR RemoveTrailingWhitespaceANSI(IN OUT LPSTR lpsz);
}


_inifile_t::_inifile_t(IN IFile *pFile, IN bool fByRef)
{
	_pBuffer =  new char[MAX_BUFFER_SIZE+1];
	_pFile = pFile;
	_fByRef = fByRef;
	_nCurrentLineNo = 1;
	_fFileOpened = false;
	_hMemOSComponents = NULL;
	_hMemDependency = NULL;
	_pLine = NULL;

	if (_pBuffer && _pFile->Open())
	{
		_fFileOpened = true;
	}
}


_inifile_t::~_inifile_t()
{
	if (_pBuffer)
	{
		delete [] _pBuffer;
	}

	if (_fFileOpened)
	{
		_pFile->Close();
	}

	if (!_fByRef)
	{
		delete _pFile;
	}

	if (_hMemOSComponents)
	{
		LocalFree(_hMemOSComponents);
		_hMemOSComponents = NULL;
	}

	if (_hMemDependency)
	{
		LocalFree(_hMemDependency);
		_hMemDependency = NULL;
	}
}


/*

@func:	_inifile_t.ReadLine, Reads a specific line from a file

@rdesc:	Handle to the allocated block containing the line if successful, NULL
otherwise

@param:	IN UINT uLine: Specific line number to read, can't be 'zero'

@param:	OUT bool &fEOF: Set to true if the function encounters end of the file

@fault:	None

@pre:	None

@post:	**** Returned buffer must be deallocated by the caller ****

@note:	Only to be used internally by NextModuleDetails, ANSI only

*/

HLOCAL _inifile_t::ReadLine(IN UINT uLine, OUT bool &fEOF)
{
	UINT uCurLine = 1;
	bool fTerminateOperation = false;
	bool fNewlineInNext = false;
	DWORD dwBytesRead = 0;
	char *pBufferTemp = NULL;
	char *pBufferTemp2 = NULL;
	bool fNoMoreRead = false;
	bool fNewlineInNextBuffer = false;
	bool fReturn = false;
	HLOCAL hMemReturn = NULL;
	HLOCAL hMemLine = NULL;

	if (!uLine)
	{
		hMemReturn = NULL;
		goto done;
	}

	if (!_fFileOpened && !_pFile->Open())
	{
		hMemReturn = NULL;
		goto done;
	}

	fEOF = false;

	if (!_pFile->SeekToBegin())
	{
		hMemReturn = NULL;
		goto done;
	}

	while (!fTerminateOperation)
	{
		memset(_pBuffer, L'\0', (MAX_BUFFER_SIZE+1));
		dwBytesRead = _pFile->Read(_pBuffer, MAX_BUFFER_SIZE);	// read a buffer from the file
		pBufferTemp = _pBuffer;

		if (!dwBytesRead)
		{
			fTerminateOperation = true;
			fEOF = true;
		}

		while (*pBufferTemp)	// for each buffer
		{
			if (uCurLine == uLine)	// found uLine'th line
			{
				pBufferTemp2 = pBufferTemp;	// line available @ pBufferTemp

				while (true)
				{
					if (fNewlineInNextBuffer)
					{
						if (*pBufferTemp2 == '\n')
						{
							fNoMoreRead = true;
							fNewlineInNextBuffer = true;
							fReturn = true;
						}
					}
					else
					{
						while (true)
						{
							if (!*pBufferTemp2 || (*(pBufferTemp2) == '\r'))
							{
								break;
							}
							pBufferTemp2++;
						}

						// *pBufferTemp2 points to either \0 or \r

						if (*pBufferTemp2 == '\r')
						{
							if (*(pBufferTemp2 + 1) == '\n')	// stop reading from the file
							{
								fNoMoreRead = true;
								fNewlineInNextBuffer = false;
								fReturn = true;
							}
							else if (*(pBufferTemp2 + 1) == '\0')	// read next buffer and wait for the \n
							{
								fNewlineInNextBuffer = true;
								fNoMoreRead = false;
								fReturn = false;
							}
							else	// was a false alarm
							{
								fNewlineInNextBuffer = false;
								fNoMoreRead = false;
								fReturn = false;
								continue;
							}
						}

						// size of the buffer to be copied is (pBufferTemp2 - pBufferTemp)

						INT nBytesCopied = 0;

						if (hMemLine)
						{
							char *psz = (char *)LocalLock(hMemLine);
							nBytesCopied = strlen(psz);
							LocalUnlock(hMemLine);
							HLOCAL hMemTmp = LocalReAlloc(hMemLine, (nBytesCopied + abs(pBufferTemp2 - pBufferTemp) + 1),
								LMEM_MOVEABLE | LMEM_ZEROINIT);

							if (!hMemTmp)
							{
								LocalFree(hMemLine);
								hMemReturn = hMemLine = NULL;
								goto done;
							}

							hMemLine = hMemTmp;
						}
						else
						{
							hMemLine = LocalAlloc(LMEM_ZEROINIT, (abs(pBufferTemp2 - pBufferTemp) + 1));

							if (!hMemLine)
							{
								hMemReturn = NULL;
								goto done;
							}
						}

						char *psz = (char *)LocalLock(hMemLine);
						strncat(psz, (pBufferTemp < pBufferTemp2) ? pBufferTemp : pBufferTemp2, abs(pBufferTemp2 - pBufferTemp));
						LocalUnlock(hMemLine);
					}

					if (fReturn)
					{
						hMemReturn = hMemLine;
						goto done;
					}

					if (!fNoMoreRead)	// read another buffer from the file
					{
						memset(_pBuffer, '\0', (MAX_BUFFER_SIZE+1));
						dwBytesRead = _pFile->Read(_pBuffer, MAX_BUFFER_SIZE);

						if (!dwBytesRead)
						{
							fReturn = true;
							hMemReturn = hMemLine;
							fEOF = true;
							goto done;
						}
						pBufferTemp = _pBuffer;
					}
					pBufferTemp2 = pBufferTemp;
				}
			}
			else
			{
				// scanning pBufferTemp for 0x0D, 0x0A

				if (!fNewlineInNext)
				{
					pBufferTemp2 = strstr(pBufferTemp, "\r");

					// if '\r' is found and next character is '\n'

					if (pBufferTemp2)
					{
						if (*(pBufferTemp2 + 1) == '\n')
						{
							uCurLine++;
							pBufferTemp = pBufferTemp2 + 2; // size of  0x0D, 0x0A
						}

						// end of buffer, '\r' in this buffer, '\n' can come in the next buffer

						else if (!*(pBufferTemp2 + 1))
						{
							fNewlineInNext = true;
							fTerminateOperation = false;
							break;
						}
					}
					else	// 0xD, 0xA not found
					{
						// read another buffer from the file
						fTerminateOperation = false;
						break;
					}
				}
				else // fNewlineInNext
				{
					// '\r' found in the last buffer, first byte of this one must be '\n'

					if (*pBufferTemp == '\n')
					{
						uCurLine++;
						pBufferTemp++;
					}

					fNewlineInNext = false;
				}
			}
			pBufferTemp2 = NULL;
		}
	}

	if (!hMemLine)
	{
		LocalFree(hMemLine);
		hMemLine = NULL;
	}

done:
	if (!_fFileOpened)
	{
		_pFile->Close();
	}
	return hMemReturn;
}


/*

@func:	_inifile_t.NextModuleDetails, Parses the initialization file to read
next module details

@rdesc: true if successful, false if there are no more modules to read or in
case of any errors

@param:	OUT LPSTR& pszModuleName: Name of the module

@param:	OUT LPSTR& pszCmdLine: Module command line

@param:	OUT UINT& nMinRunTimeMinutes: Minimum runtime of the module

@param:	OUT HLOCAL& hMemDependency: Handle to the memory block containing
filenames necessary to run this module

@param:	OUT HLOCAL& hMemOSComponents: Handle to the memory block containing
OS component names the module is dependent on

@fault: None

@pre: None

@post: None

@note: **** Deallocates buffer returned by ReadLine ****

*/

bool _inifile_t::NextModuleDetails(OUT LPSTR& pszModuleName, OUT LPSTR& pszCmdLine,
		OUT UINT& nMinRunTimeMinutes, OUT HLOCAL& hMemDependency, OUT HLOCAL& hMemOSComponents)
{
	bool fOk = false;
	char *pToken = NULL;

	pszModuleName = pszCmdLine = 0;
	nMinRunTimeMinutes = 0;

	memset(_szModuleName, 0, sizeof _szModuleName);
	memset(_szCmdLine, 0, sizeof _szCmdLine);
	_nMinRunTimeMinutes = 0;

	/*
	mod_name=one.exe
	cmd_line=cmdline
	dependency=tux.exe*kato.dll*tooltalk.dll
	os_comp=test1 test2 test3 test4
	min_runtime=1
	*/

	bool fEOF = false;

	do
	{
		HLOCAL hMemLine = ReadLine(_nCurrentLineNo, fEOF);

		char *pszLine = (char *)LocalLock(hMemLine);

		Utilities::RemoveTrailingWhitespaceANSI(pszLine);

		if (!pszLine || !*pszLine)
		{
			_nCurrentLineNo++;
			LocalUnlock(hMemLine);
			continue;
		}

		if (pszLine && *pszLine)
		{
			pToken = strtok(pszLine, "=");

			if (pToken)
			{
				if (!strcmp(pToken, MOD_NAME))
				{
					pToken = strtok(NULL, "\n\r"); // read the value
					memset(_szModuleName, 0, sizeof _szModuleName);

					if (pToken)
					{
						strncpy(_szModuleName, pToken, (strlen(pToken) < MAX_PATH) ? strlen(pToken) : MAX_PATH);
					}

					LocalUnlock(hMemLine);

					if (hMemLine)
					{
						LocalFree(hMemLine);
						hMemLine = NULL;
					}

					_nCurrentLineNo++;
				}
				else if (!strcmp(pToken, CMD_LINE))
				{
					pToken = strtok(NULL, "\n\r"); // read the value
					memset(_szCmdLine, 0, sizeof _szCmdLine);

					if (pToken)
					{
						strncpy(_szCmdLine, pToken, (strlen(pToken) < MAX_CMDLINE) ? strlen(pToken) : MAX_CMDLINE);
					}

					LocalUnlock(hMemLine);

					if (hMemLine)
					{
						LocalFree(hMemLine);
						hMemLine = NULL;
					}

					_nCurrentLineNo++;
				}
				else if (!strcmp(pToken, DEPENDENCY))
				{
					pToken = strtok(NULL, "\n\r"); // read the value

					if (pToken)
					{
						if (_hMemDependency)
						{
							LocalFree(_hMemDependency);
							_hMemDependency = NULL;
						}

						INT nLength = strlen(pToken) + 1;

						_hMemDependency = LocalAlloc(LMEM_ZEROINIT, nLength);

						if (!_hMemDependency)
						{
							fOk = false;
							goto done;
						}

						char *psz = (char *)LocalLock(_hMemDependency);
						strcpy(psz, pToken);
						LocalUnlock(_hMemDependency);

						hMemDependency = _hMemDependency;
					}

					LocalUnlock(hMemLine);

					if (hMemLine)
					{
						LocalFree(hMemLine);
						hMemLine = NULL;
					}

					_nCurrentLineNo++;
				}
				else if (!strcmp(pToken, OS_COMP))
				{
					pToken = strtok(NULL, "\n\r"); // read the value

					if (pToken)
					{
						if (_hMemOSComponents)
						{
							LocalFree(_hMemOSComponents);
							_hMemOSComponents = NULL;
						}

						INT nLength = strlen(pToken) + 1;

						_hMemOSComponents = LocalAlloc(LMEM_ZEROINIT, nLength);

						if (!_hMemOSComponents)
						{
							fOk = false;
							goto done;
						}

						char *psz = (char *)LocalLock(_hMemOSComponents);
						strcpy(psz, pToken);
						LocalUnlock(_hMemOSComponents);

						hMemOSComponents = _hMemOSComponents;
					}

					LocalUnlock(hMemLine);

					if (hMemLine)
					{
						LocalFree(hMemLine);
						hMemLine = NULL;
					}

					_nCurrentLineNo++;
				}
				else if (!strcmp(pToken, MIN_RUNTIME))
				{
					pToken = strtok(NULL, "\n\r"); // read the value
					_nMinRunTimeMinutes = atoi(pToken);

					LocalUnlock(hMemLine);

					if (hMemLine)
					{
						LocalFree(hMemLine);
						hMemLine = NULL;
					}

					_nCurrentLineNo++;

					pszModuleName = _szModuleName;
					pszCmdLine = _szCmdLine;
					hMemDependency = _hMemDependency;
					hMemOSComponents = _hMemOSComponents;
					nMinRunTimeMinutes = _nMinRunTimeMinutes;
					fOk = true;
					goto done;
				}
				else
				{
					DebugDump(L"#### _inifile_t::NextModuleDetails - badly formatted intitalization file ####\r\n");
					fOk = false;
					goto done;
				}
			}
		}
	}
	while (!fEOF);

done:
	return fOk;
}
