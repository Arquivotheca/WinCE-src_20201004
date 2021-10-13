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
////////////////////////////////////////////////////////////////////////////////
//
//  File Logger 
//
//
////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <tchar.h>
#include "logging.h"
#include "ILogger.h"

FileLogger::FileLogger()
{
	m_hFile = INVALID_HANDLE_VALUE;
	InitializeCriticalSection(&m_cs);
}

FileLogger::~FileLogger()
{
	Reset();
	// Delete the critical section
	DeleteCriticalSection(&m_cs);
}

HRESULT FileLogger::Init(TCHAR* szFileName)
{
	HRESULT hr = S_OK;
	
	// Save the remote server and port
	_tcsncpy(m_szFileName, szFileName, sizeof(m_szFileName)/sizeof(m_szFileName[0]));
	
	// Create the HANDLE descriptor
	m_hFile = CreateFile(m_szFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
	if (m_hFile == INVALID_HANDLE_VALUE)
	{
		return E_FAIL;
	}

	return hr;
}

void FileLogger::Reset()
{
	// Close the HANDLE
	if (m_hFile != INVALID_HANDLE_VALUE)
	{
		FlushFileBuffers(m_hFile);
		CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
	}
}

HRESULT FileLogger::Log(BYTE* pData, int datalen)
{
	EnterCriticalSection(&m_cs);
	HRESULT hr = S_OK;

	static int nBytes = 0;

	if (!pData || (datalen <= 0))
		hr = E_INVALIDARG;

	if (SUCCEEDED(hr))
	{
		DWORD nBytesWritten = 0;
		int ret = WriteFile(m_hFile, pData, datalen, &nBytesWritten, NULL);
		if (ret == 0)
		{
			hr = E_FAIL;
		}
		if (SUCCEEDED(hr) && (nBytesWritten == datalen))
		{
			nBytes += datalen;
		}
	}

	LeaveCriticalSection(&m_cs);
	return hr;
}