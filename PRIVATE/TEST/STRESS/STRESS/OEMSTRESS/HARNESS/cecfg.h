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

Module Name: cecfg.h

Abstract: Contains class declaration for _ceconfig_t (routines for OS component
matching)

Notes:

________________________________________________________________________________
*/

#ifndef __CECFG_H__


class _ceconfig_t
{
	char _szBuffer[MAX_BUFFER_SIZE + 1];
	IFile *_pFile;
	bool _fByRef;
	HLOCAL _hMemOSComps;

public:
	bool _fCeCfgFileLoaded;

public:
	_ceconfig_t(IN IFile *pFile, IN bool fByRef = false);
	~_ceconfig_t();
	bool IsRunnable(IN LPSTR pszModOSComp);
	#ifdef DEBUG
	void DumpConfigInfo();
	#endif /* DEBUG */

private:
	bool IsComponentAvailable(IN LPSTR lpszComponent);
	bool LoadCeconfigFile();

	friend struct logicalexpression_t;
};

#define __CECFG_H__
#endif /* __CECFG_H__ */
