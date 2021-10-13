//--------------------------------------------------------------------------------------------
//
//	THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//	ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//	THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//	PARTICULAR PURPOSE.
//
//	  Copyright (c) Microsoft Corporation. All rights reserved.
//
//--------------------------------------------------------------------------------------------
//
// May 2004 - Michael Rockhold (michrock)
//
//--------------------------------------------------------------------------------------------

#ifndef __CETKCONNLIB_H__
#define __CETKCONNLIB_H__

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

enum CetkConnectionKinds {
	CCK_UNKNOWN = 0,
	CCK_PLATMAN,
	CCK_SOCKETS,
	CCK_CORECON
};

typedef unsigned long CetkConnection;

void DestroyCetkConn(CetkConnection conn);
CetkConnection CreateCetkConn_Sockets(LPCSTR pServer, USHORT port);
#ifdef UNDER_CE
CetkConnection CreateCetkConn_Platman(GUID* platmanStreamID);
#else
CetkConnection CreateCetkConn_Platman(GUID* devID, GUID* platmanStreamID);
CetkConnection CreateCetkConn_PlatmanPickDevice(GUID* platmanStreamID);
#endif
CetkConnection CreateCetkConn_CoreCon(void);

BOOL CetkConn_IsConstructedOK(CetkConnection conn);
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

BOOL CetkConn_ListenForClient(CetkConnection conn);
BOOL CetkConn_ConnectToService(CetkConnection conn);
VOID CetkConn_Disconnect(CetkConnection conn);

#ifdef UNDER_NT
LPCTSTR CetkConn_GetDeviceName(CetkConnection conn);
BOOL CetkConn_ConfigureDevice(CetkConnection conn);
BOOL CetkConn_InstallAndLaunchServer(CetkConnection conn, LPCWSTR pszDeviceSideServerName);
#endif

#ifdef __cplusplus
}
#endif




#ifdef __cplusplus

class CCetkConnection
{
	
protected:
	
	CCetkConnection()
		: m_OK(true)
	{
		::ZeroMemory(szOtherName, sizeof(WCHAR)*(MAX_PATH+1));
	}
	

	bool m_OK;
	WCHAR szOtherName[MAX_PATH+1];

public:
		
	virtual ~CCetkConnection() {}

	virtual bool IsConstructedOK() { return m_OK; } // phooey. This is so ugly. Blech.
	virtual bool IsConnected() = 0;
	virtual bool IsListening() = 0;
	
	virtual int Receive(char FAR *Buffer, int Length)=0;
	virtual int Send(char FAR *pData, int cbData)=0;
	
	BOOL GetDWORD(DWORD *pdw);
	BOOL SendDWORD(DWORD dw);
	
	BOOL GetStringA(LPSTR *ppszString);
	BOOL SendStringA(LPCSTR pszString);
	
	BOOL GetStringW(LPWSTR *ppwszString);
	BOOL SendStringW(LPCWSTR pwszString);

	virtual BOOL ListenForClient()=0;
	virtual BOOL ConnectToService()=0;
	virtual VOID Disconnect()=0;

#ifdef UNDER_NT
	virtual BOOL ConfigureDevice()=0;
	virtual BOOL InstallAndLaunchServer(LPCWSTR lpwszDeviceEXEName) = 0;

	LPCWSTR GetDeviceName()
		{
			return &szOtherName[0];
		}
#endif


};


#endif


#endif // __CETKCONNLIB_H__
