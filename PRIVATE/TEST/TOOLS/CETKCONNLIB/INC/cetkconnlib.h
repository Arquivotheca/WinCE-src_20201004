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
//--------------------------------------------------------------------------------------------
//
//	THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//	ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//	THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//	PARTICULAR PURPOSE.
//
//
//--------------------------------------------------------------------------------------------
//
// May 2004 - Michael Rockhold (michrock)
//
//--------------------------------------------------------------------------------------------

#ifndef __CETKCONNLIB_H__
#define __CETKCONNLIB_H__

#include <Windows.h>
#include <tchar.h>

#ifdef __cplusplus
extern "C" {
#endif

enum CetkConnectionKinds {
	CCK_UNKNOWN = 0,
	CCK_PLATMAN,
	CCK_SOCKETS,
	CCK_CORECON,
	CCK_KITL
};

enum CetkConnErr {
	CETKCONNERR_NOERROR = 0,
	CETKCONNERR_UNIMPLEMENTED,
	CETKCONNERR_INVALID_ARGUMENT,
	CETKCONNERR_PARSE_ERROR,
	CETKCONNERR_GUID_PARSE_ERROR,
	CETKCONNERR_CONSTRUCTION_ERROR,
	CETKCONNERR_BUFFER_NOT_BIG_ENOUGH,
	CETKCONNERR_DEVICEID_NOT_FOUND,
	CETKCONNERR_ENUMPLATFORMS
	};

typedef unsigned long CetkConnection;

void DestroyCetkConn(CetkConnection conn);

BOOL CetkConn_IsConnected(CetkConnection conn);
BOOL CetkConn_IsListening(CetkConnection conn);

int CetkConn_Send (CetkConnection conn, char FAR *pData, int cbData);
int CetkConn_Receive (CetkConnection conn, char FAR *Buffer, int Length);

BOOL CetkConn_GetDWORD (CetkConnection conn, DWORD *pdw);
BOOL CetkConn_SendDWORD (CetkConnection conn, DWORD dw);
BOOL CetkConn_GetStringA (CetkConnection conn, LPSTR *ppszString);
BOOL CetkConn_SendStringA (CetkConnection conn, LPCSTR pszString);
BOOL CetkConn_GetStringW (CetkConnection conn, LPWSTR *ppszString);
BOOL CetkConn_SendStringW (CetkConnection conn, LPCWSTR pszString);

BOOL CetkConn_Connect(CetkConnection conn);

VOID CetkConn_Disconnect(CetkConnection conn);

typedef unsigned long CetkChannel;
extern "C"
{
	typedef int (CALLBACK *EnumAvailableDevicesCB)(LPCTSTR, LPCTSTR, unsigned int); // deviceID, deviceName, refcon
}

CetkChannel CreateCetkChannel(LPCTSTR pszChannelName);
void DestroyCetkChannel(CetkChannel chan);

#ifdef UNDER_NT
BOOL CetkChannel_BrowseForDevice(CetkChannel chan, LPTSTR pszDeviceInfo, size_t cbDeviceInfoBufferSize);
int CetkChannel_EnumAvailableDevices(CetkChannel chan, EnumAvailableDevicesCB cb, unsigned int refcon);
#endif

CetkConnection CetkChannel_CreateCetkListener(CetkChannel chan, LPCTSTR pszStreamInfo);
CetkConnection CetkChannel_CreateCetkConnector(CetkChannel chan, LPCTSTR pszStreamInfo, LPCTSTR pszDeviceInfo);

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus

class CCetkChannel;

class CCetkConnection
{
public:
	bool m_OK;

protected:
	bool m_listener;
	
	CCetkChannel* m_pChannel;
	
	CCetkConnection(
		CCetkChannel* pChannel,
		bool listener
		)
		: m_pChannel(pChannel), m_listener(listener), m_OK(true)
	{
	}

public:
	virtual ~CCetkConnection()
	{
	}

	virtual bool IsConnected() = 0;
	virtual bool IsListening() = 0;
	
	virtual int Receive(char FAR *Buffer, int Length)=0;
	virtual int Send(char FAR *pData, int cbData)=0;
	
	virtual BOOL Connect()=0;
	virtual VOID Disconnect()=0;

	// convenience methods, all defined in terms of the virtual abstract methods above
	
	BOOL GetDWORD(DWORD *pdw)
	{
		DWORD dw;
	
		if (!pdw) {
			return FALSE;
		}
	
		if (this->Receive ((char *) & dw, sizeof (DWORD)) == sizeof (DWORD)) {
			*pdw = dw;
			return TRUE;
		}
	
		return FALSE;
	}
	
	BOOL SendDWORD (DWORD dw)
	{
		if (this->Send ((char *) & dw, sizeof (DWORD)) == sizeof (DWORD)) {
			return TRUE;
		}
	
		return FALSE;
	}
	
	BOOL GetStringA (LPSTR *ppszString)
	{
		DWORD dwSize;
	
		if (!ppszString) {
			return FALSE;
		}
	
		// Strings are counted strings (the first DWORD sent is the size in chars, then the
		// string.
		if (!this->GetDWORD (& dwSize)) {
			return FALSE;
		}
	
		// Make sure the size is valid
		if ((DWORD) -1 == dwSize) {
			return FALSE;
		}
	
		// Alloc a local buffer to receive the string
		*ppszString = (LPSTR) LocalAlloc (LMEM_ZEROINIT, (dwSize + 1) * sizeof (char));
	
		if (0 == dwSize) {
			return TRUE;
		}
	
		if (dwSize * sizeof (char) != (DWORD) this->Receive ((char *) *ppszString, dwSize * sizeof (char))) {
			return FALSE;
		}
	
		return TRUE;
	}
	
	BOOL SendStringA (LPCSTR pszString)
	{
		DWORD cchString;
	
		cchString = pszString ? strlen (pszString) : 0;
	
		// Strings are counted strings (the first DWORD sent is the size, then the
		// string.
		if (!this->SendDWORD (cchString)) {
			return FALSE;
		}
	
		if (0 == cchString) {
			return TRUE;
		}
	
		if (cchString * sizeof (char) != (DWORD) this->Send((char *) pszString, cchString * sizeof (char))) {
			return FALSE;
		}
	
		return TRUE;
	}
	
	
	BOOL GetStringW (LPWSTR *ppwszString)
	{
		DWORD dwSize;
	
		if (!ppwszString) {
			return FALSE;
		}
	
		// Strings are counted strings (the first DWORD sent is the size in chars, then the
		// string.
		if (!this->GetDWORD (& dwSize)) {
			return FALSE;
		}
	
		// Make sure the size is valid
		if ((DWORD) -1 == dwSize) {
			return FALSE;
		}
	
		// Alloc a local buffer to receive the string
		*ppwszString = (LPWSTR) LocalAlloc (LMEM_ZEROINIT, (dwSize + 1) * sizeof (WCHAR));
	
		if (0 == dwSize) {
			return TRUE;
		}
	
		if (dwSize * sizeof (WCHAR) != (DWORD) this->Receive ((char *) *ppwszString, dwSize * sizeof (WCHAR))) {
			return FALSE;
		}
	
		return TRUE;
	}
	
	BOOL SendStringW (LPCWSTR pwszString)
	{
		DWORD cchString;
	
		cchString = pwszString ? wcslen (pwszString) : 0;
	
		// Strings are counted strings (the first DWORD sent is the size, then the
		// string.
		if (!this->SendDWORD (cchString)) {
			return FALSE;
		}
	
		if (0 == cchString) {
			return TRUE;
		}
	
		if (cchString * sizeof (WCHAR) != (DWORD) this->Send((char *) pwszString, cchString * sizeof (WCHAR))) {
			return FALSE;
		}
	
		return TRUE;
	}
};


class CCetkChannel
{
protected:
	HMODULE m_hmPluginLib;

	CCetkChannel(HMODULE hmPluginLib)
		: m_hmPluginLib(hmPluginLib)
	{}

public:
	static CCetkChannel* Create(LPCTSTR pszConnectionKind);
	virtual ~CCetkChannel()
	{
		if (m_hmPluginLib)
		{
			// is this really the greatest idea?
			//::FreeLibrary(m_hmPluginLib);
		}
	}

#if defined(UNDER_NT)
	virtual CetkConnErr BrowseForDevice(LPTSTR pszDeviceInfo, size_t cbDeviceInfoBufferSize) = 0;
	virtual int EnumAvailableDevices(EnumAvailableDevicesCB cb, unsigned int refcon) = 0;
#endif
	
	virtual CCetkConnection* CreateCetkConnection(bool listener, LPCTSTR pszStreamInfo, LPCTSTR pszDeviceInfo) = 0;
};



#endif

#endif // __CETKCONNLIB_H__
