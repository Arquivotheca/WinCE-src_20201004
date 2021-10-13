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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef _ILOGGER_H
#define _ILOGGER_H

#include <winsock2.h>
#include <windows.h>

// Simple interface for a logger
class ILogger
{
public:
	virtual ~ILogger() {}
	virtual HRESULT Log(BYTE* pData, int datalen) = 0;
};

// This class logs to a file given the file name
class FileLogger : public ILogger
{
public:
	FileLogger();
	HRESULT Init(TCHAR* szLogFile);
	virtual ~FileLogger();
	virtual HRESULT Log(BYTE* pData, int datalen);

private:
	void Reset();

private:
	CRITICAL_SECTION m_cs;
	TCHAR m_szFileName[MAX_PATH];
	HANDLE m_hFile;
};

// This class logs data to a network socket
class NetworkLogger : public ILogger
{
public:
	NetworkLogger();
	HRESULT Init(TCHAR* szServer, unsigned short port);
	virtual ~NetworkLogger();
	virtual HRESULT Log(BYTE* pData, int datalen);

private:
	void Reset();

	TCHAR m_szServer[MAX_PATH];
	unsigned short m_Port;
	SOCKET m_Socket;

};

#endif
