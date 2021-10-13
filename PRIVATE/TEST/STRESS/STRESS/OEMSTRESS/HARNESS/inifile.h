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

Module Name: inifile.h

Abstract: Contains class declaration for _inifile_t (initialization file parsing
routines for OEM stress harness)

Notes: None

________________________________________________________________________________
*/


#ifndef __INIFILE_H__


#define	MOD_NAME				"mod_name"
#define	CMD_LINE				"cmd_line"
#define	DEPENDENCY				"dependency"
#define	OS_COMP					"os_comp"
#define	MIN_RUNTIME				"min_runtime"


struct _inifile_t
{
	IFile *_pFile;
	bool _fFileOpened;
	char *_pLine;
	char *_pBuffer;
	bool _fByRef;
	UINT _nCurrentLineNo;

	char _szModuleName[MAX_PATH + 1];
	char _szCmdLine[MAX_CMDLINE + 1];
	HANDLE _hMemOSComponents;
	HLOCAL _hMemDependency;
	UINT _nMinRunTimeMinutes;

	_inifile_t(IN IFile *pFile, IN bool fByRef = false);
	~_inifile_t();
	HLOCAL ReadLine(IN UINT uLine, OUT bool &fEOF);
	bool NextModuleDetails(OUT LPSTR& pszModuleName, OUT LPSTR& pszCmdLine,
		OUT UINT& nMinRunTimeMinutes, OUT HLOCAL& hMemDependency, OUT HLOCAL& hMemOSComponents);

private:
    _inifile_t();									// default constructor NOT implemented
	_inifile_t(const _inifile_t&);					// default copy constructor NOT implemented
	_inifile_t& operator = (const _inifile_t&);		// default assignment operator NOT implemented
};


#define __INIFILE_H__
#endif /* __INIFILE_H__ */
