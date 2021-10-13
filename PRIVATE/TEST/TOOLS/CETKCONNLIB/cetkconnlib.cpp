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

#include "CetkConnLib.h"

#define INITGUID
#include <windows.h>
#include <tchar.h>
#include <winsock.h>

#ifdef UNDER_CE
#include "tldevice.h"
#else
#include <strsafe.h>
#include "cemgr.h"
#include "cemgrui.h"
#include <cemgr_i.c>
#include <cemgrui_i.c>

#endif


#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH( array )	( sizeof( array ) / sizeof( array[ 0 ] ) )
#endif //#ifndef ARRAY_LENGTH

#define ERROR_EXIT(c,e,l)	if (c) { errnum = e; goto l; }

#include <stdarg.h>
#include <wtypes.h>

void
MyMessageBox(const TCHAR* caption, int style, const TCHAR* fmt, ...)
{
	int nSize = 0;
	TCHAR buff[255];
	va_list args;
	va_start(args, fmt);
	
	nSize = _vsntprintf(buff, ARRAY_LENGTH(buff), fmt, args);

#ifdef UNDER_CE
	OutputDebugString(buff);
#else
	MessageBox(NULL, buff, caption, style);
#endif
}


bool
GetCurrentProcessInfo(DWORD* pdwCurrentProceessID, LPWSTR* ppModuleFileName)
{
	TCHAR szTitle[MAX_PATH];
	::GetModuleFileName( NULL, szTitle, MAX_PATH - 1);
	szTitle[MAX_PATH - 1] = _T('\0');
	
	*ppModuleFileName = new WCHAR[MAX_PATH];
	if ( NULL == *ppModuleFileName )
		return false;
	
#ifdef UNICODE
	wcscpy( *ppModuleFileName, szTitle);
#else
	mbstowcs( *ppModuleFileName, szTitle, strlen(szTitle)+1);
#endif

	return true;
}

BOOL CCetkConnection::GetDWORD(DWORD *pdw)
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

BOOL CCetkConnection::SendDWORD (DWORD dw)
{
	if (this->Send ((char *) & dw, sizeof (DWORD)) == sizeof (DWORD)) {
		return TRUE;
	}

	return FALSE;
}

BOOL CCetkConnection::GetStringA (LPSTR *ppszString)
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

BOOL CCetkConnection::SendStringA (LPCSTR pszString)
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


BOOL CCetkConnection::GetStringW (LPWSTR *ppwszString)
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

BOOL CCetkConnection::SendStringW (LPCWSTR pwszString)
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



// Socket connection
class CSocketCetkConnection : public CCetkConnection
{
	WSADATA m_WSAData;
	u_short Port;
	CHAR server[MAX_PATH+1];
	SOCKET s;
	SOCKET slisten;
	SOCKADDR_IN addr;
	IN_ADDR in;
	
	bool fWaitAborted;
	
	friend CetkConnection CreateCetkConn_Sockets(LPCSTR pServer, u_short port);
	CSocketCetkConnection(
		LPCSTR pServer,
		u_short port
		) 
		: CCetkConnection(), Port(port), s(INVALID_SOCKET), slisten(INVALID_SOCKET), fWaitAborted(false)
	{
		if (pServer)
		{
			::ZeroMemory(server, MAX_PATH+1);
			::strncpy(server, pServer, MAX_PATH);
		}
		
		// Initiate use of winsock
		if (0 != WSAStartup (MAKEWORD (1, 1), & m_WSAData))
			return;
		
		// Confirm that the WinSock DLL supports 1.1.
		// Note that if the DLL supports versions greater
		// than 1.1 in addition to 1.1, it will still return
		// 1.1 in wVersion since that is the version we
		// requested.
		
		if (LOBYTE (m_WSAData.wVersion) != 1 || HIBYTE (m_WSAData.wVersion) != 1)
		{
			MyMessageBox(_T("Connect to Device"), MB_OK | MB_ICONEXCLAMATION, _T("Incorrect Windows Sockets Specification (%u.%u) should be (1.1)"), LOBYTE (m_WSAData.wVersion), HIBYTE (m_WSAData.wVersion));
			return;
		}
		
		m_OK = true;
	}

	BOOL CancelConnect(VOID)
	{
		if (INVALID_SOCKET != slisten)
		{
			fWaitAborted = TRUE;
			closesocket (slisten);
			slisten = INVALID_SOCKET;
		}
		return TRUE;
	}

		
public:
		
	virtual ~CSocketCetkConnection()
	{
		this->Disconnect();
		WSACleanup ();
	}
	
	virtual bool IsConnected()
	{
		return (INVALID_SOCKET != s);
	}
	
	virtual bool IsListening()
	{
		return (INVALID_SOCKET != slisten);
	}

	virtual bool CanDoServerOnHost()
	{
		return true;
	}
	
	virtual int Receive(char FAR *Buffer, int Length)
	{
		int ret, nLeft, idx;
	
		if (INVALID_SOCKET == s)
		{
			return -1;
		}
	
		nLeft = Length;
		idx = 0;
	
		while (nLeft > 0) {
			ret = recv (s, & Buffer [idx], nLeft, 0);
			// 0 indicates a "graceful" closing of the socket
			if (ret == SOCKET_ERROR || ret == 0)
				return -1;
			nLeft -= ret;
			idx += ret;
		}
	
		return Length;
	}
		
		
	virtual int Send(char FAR *pData, int cbData)
	{
		int ret, nLeft, idx;
	
		if (INVALID_SOCKET == s)
		{
			return SOCKET_ERROR;
		}
	
		nLeft = cbData;
		idx = 0;
	
		while (nLeft > 0) {
			ret = send (s, & pData [idx], nLeft, 0);
			if (ret == SOCKET_ERROR)
				return SOCKET_ERROR;
			nLeft -= ret;
			idx += ret;
		}
	
		return cbData;
	}


	virtual BOOL ListenForClient()
	{
		PHOSTENT phostent;
		int nlen;
		TCHAR szTemp [MAX_PATH];
		bool ok = false;
		
	
		if (INVALID_SOCKET != slisten)
		{
			// We're already listening.
			return false;
		}
		
		// Create a socket and prepare it for receiving data
		if (INVALID_SOCKET == (slisten = socket (AF_INET, SOCK_STREAM, 0)))
			goto exit_socket_connect_to_host;
	
		// Bind the socket
		addr.sin_family = AF_INET;
		addr.sin_port = htons (Port);
		addr.sin_addr.s_addr = INADDR_ANY;
	
		if (SOCKET_ERROR == bind (slisten, (LPSOCKADDR) & addr, sizeof (SOCKADDR_IN)))
			goto exit_socket_connect_to_host;
	
		// Listen for a connection
		if (SOCKET_ERROR == listen (slisten, SOMAXCONN))
			goto exit_socket_connect_to_host;
	
		// Wait for an incoming connection
		fWaitAborted = FALSE;
		nlen = sizeof (SOCKADDR_IN);
		if (INVALID_SOCKET == (s = accept (slisten, (LPSOCKADDR) & addr, & nlen)))
			goto exit_socket_connect_to_host;
	
		// Close the listening socket
		closesocket (slisten);
		slisten = INVALID_SOCKET;
	
		// Copy the four byte IP address into an IP address structure
		memcpy (& in, & addr.sin_addr.s_addr, 4);
		ok = true;
	
		// Update the device name
		phostent = gethostbyaddr ((char *) & in.s_addr, sizeof (in.s_addr), AF_INET);
		if ( phostent )
		{
			mbstowcs (szTemp, phostent->h_name, MAX_PATH);
		}
		else
		{
			mbstowcs (szTemp, inet_ntoa(in), MAX_PATH);
		}
		(void) (void) StringCchCopy (szOtherName, MAX_PATH, szTemp);

exit_socket_connect_to_host:
		if ( ok )
		{
			//Log(_T("Connected to %s\r\n"), szOtherName);
		}
		else
		{
			if ( !fWaitAborted )
				MyMessageBox(_T("ListenForClient"), MB_OK | MB_ICONEXCLAMATION, _T("Failed to connect to any host."));
		}
		
		return	ok;
	}


	virtual BOOL ConnectToService()
	{
		PHOSTENT phostent = NULL;
		TCHAR szTemp[MAX_PATH];

		OutputDebugString(_T("Connecting to Winsock service..."));
 		
		if ( INVALID_SOCKET == (s = socket (AF_INET, SOCK_STREAM, 0)) )
		{
			MyMessageBox(_T("Connect to Service"), MB_OK | MB_ICONEXCLAMATION, _T("Couldn't create socket (%u)"), WSAGetLastError ());
			return FALSE;
		}
	
		OutputDebugString(_T("...socket created..."));
		
		addr.sin_family = AF_INET;
		addr.sin_port = htons (Port);
	
		OutputDebugString(_T("...try dotted ip address..."));
		// First try a dotted ip address
		addr.sin_addr.s_addr = inet_addr (server);
		if ( INADDR_NONE == addr.sin_addr.s_addr )
		{
			// Try a server name
			OutputDebugString(_T("...try server name..."));
			phostent = gethostbyname (server);
			if (!phostent)
			{
				TCHAR wserver [MAX_PATH];
				mbstowcs (wserver, server, MAX_PATH);
				MyMessageBox(_T("Connect to Service"), MB_OK | MB_ICONEXCLAMATION, _T("Couldn't connect to server (%s)"), wserver);
				return FALSE;
			}
	
			memcpy (& addr.sin_addr.s_addr, phostent->h_addr_list[0], sizeof (addr.sin_addr.s_addr));
		}
		
		OutputDebugString(_T("...got addr.sin_addr.s_addr! Connect?..."));
		memcpy (& in, & addr.sin_addr.s_addr, 4);
	
		OutputDebugString(_T("...connect..."));
		if ( SOCKET_ERROR == connect (s, (LPSOCKADDR) & addr, sizeof (SOCKADDR_IN)) )
		{
			MyMessageBox(_T("Connect to Service"), MB_OK | MB_ICONEXCLAMATION, _T("connect failed (%u)"), WSAGetLastError());
			return FALSE;
		}
		OutputDebugString(_T("...connected!..."));


		// Update the device name
		if ( phostent && phostent->h_name )
		{
			mbstowcs (szTemp, phostent->h_name, MAX_PATH);
		}
		else
		{
			mbstowcs (szTemp, inet_ntoa(in), MAX_PATH);
		}
		(void) (void) StringCchCopy (szOtherName, MAX_PATH, szTemp);

		OutputDebugString(_T("...Connected! to Winsock service!"));
		return TRUE;
	}


	virtual VOID Disconnect()
	{				
		if (INVALID_SOCKET != slisten)
		{
			//shutdown(slisten, SD_BOTH);
			closesocket (slisten);
			slisten = INVALID_SOCKET;
		}
		
		if (INVALID_SOCKET != s)
		{
			//shutdown(s, SD_BOTH);
			closesocket (s);
			s = INVALID_SOCKET;
		}
	}

#ifdef UNDER_NT
	// I know what this looks like. But I'm really pressed for time, and, well, you know....
	
	virtual BOOL ConfigureDevice()
	{
		return true;
	}

	virtual BOOL InstallAndLaunchServer(LPCWSTR lpwszDeviceEXEName)
	{
		return false;
	}

#endif

};


#ifdef UNDER_CE
typedef CConnectionStream* CPlatmanStreamPtr;
#else
typedef IConnectionStream* CPlatmanStreamPtr;
#endif

// Platman connection
class CPlatManCetkConnection : public CCetkConnection
{
private:
	CPlatmanStreamPtr pStream;
	GUID streamGUID;
	
#ifdef UNDER_CE
	HMODULE platModule;
	
	friend CetkConnection CreateCetkConn_Platman(GUID* platmanStreamID);
	CPlatManCetkConnection(GUID* platmanStreamID)
	: CCetkConnection(), streamGUID(*platmanStreamID), platModule(NULL), pStream(NULL)
	{
	}
	
#endif

#ifdef UNDER_NT
	IPlatformManager *piMgr;
	IPlatformManagerUI *piMgrUI;
	IPlatformCE *piPlatform;
	IRemoteDevice *piDevice;
	IConnection *piConnection;
	
	friend CetkConnection CreateCetkConn_Platman(GUID* devID, GUID* platmanStreamID);
	friend CetkConnection CreateCetkConn_PlatmanPickDevice(GUID* platmanStreamID);
	CPlatManCetkConnection(GUID* devID, GUID* platmanStreamID) 
	: CCetkConnection(), piMgr(NULL), piMgrUI(NULL), piPlatform(NULL), piDevice(NULL), piConnection(NULL), pStream(NULL), streamGUID(*platmanStreamID)
	{		
		int errnum = 0;
		
		::CoInitialize (NULL);

		//------------------------------------------------------------------
		// Instantiate the IPlatformManager COM object
		//------------------------------------------------------------------

		HRESULT hr = ::CoCreateInstance(CLSID_PlatformManager,
									  NULL,
									  CLSCTX_LOCAL_SERVER,
									  IID_IPlatformManager,
									  (void**) & piMgr);
		ERROR_EXIT((FAILED(hr)), 1, ConstructorErrorExit);

		if ( devID )
		{
			IEnumPlatformCE *piEnum;
			hr = piMgr->EnumPlatforms(&piEnum);
			ERROR_EXIT((FAILED(hr)), 2, ConstructorErrorExit);
			
			DWORD dwCount;
			piEnum->GetCount( &dwCount);
			if ( dwCount )
			{
				IPlatformCE **ppiPlatform = new IPlatformCE *[dwCount];
				DWORD dwRead;
				piEnum->Next( dwCount, ppiPlatform, &dwRead);
				
				for (dwCount=0; (dwCount < dwRead) && (NULL == piConnection); dwCount++)
				{
					IEnumDevice *piEnumDevice;
					if ( SUCCEEDED(ppiPlatform[dwCount]->EnumDevices(&piEnumDevice)) )
					{
						DWORD dwDevCount;
						piEnumDevice->GetCount(&dwDevCount);
						if ( dwDevCount )
						{
							IRemoteDevice **ppiRemoteDevice = new IRemoteDevice *[dwDevCount];
							DWORD dwDevRead;
							piEnumDevice->Next(dwDevCount, ppiRemoteDevice, &dwDevRead);
							
							for (dwDevCount=0; (dwDevCount < dwDevRead) && (NULL == piConnection); dwDevCount++)
							{
								IRemoteDevice* theDevice = ppiRemoteDevice[dwDevCount];

								DebugBreak();
								GUID deviceID;
								DEVICE_TYPE dt;
								LPOLESTR deviceName;
								LPWSTR moduleName;
								
								theDevice->GetDeviceId(&deviceID);
								theDevice->GetDeviceType(&dt);
								theDevice->GetDeviceName(&deviceName);


								if ( *devID == deviceID )
								{
									DWORD dwCurrentProcessID;
									if ( GetCurrentProcessInfo(&dwCurrentProcessID, &moduleName) )
									{
										if ( FAILED(theDevice->Attach(dwCurrentProcessID, moduleName, 5000, &piConnection, NULL)) )
										{
											piConnection = NULL;
										}
										delete [] moduleName;
									}
								}

								::CoTaskMemFree((void*)deviceName);
								theDevice->Release();
							}
						}
					}
					ppiPlatform[dwCount]->Release();
				}
				delete [] ppiPlatform;
			}
			piEnum->Release();

			ERROR_EXIT((NULL==piConnection), 3, ConstructorErrorExit);
		}
		else
		{
			//------------------------------------------------------------------
			// Instantiate the IPlatformManagerUI COM object
			//------------------------------------------------------------------
			
			hr = CoCreateInstance(CLSID_PlatformManagerUI,
								  NULL,
								  CLSCTX_INPROC_SERVER,
								  IID_IPlatformManagerUI,
								  (void**) & piMgrUI);
			ERROR_EXIT((FAILED(hr)), 4, ConstructorErrorExit);
			
			//------------------------------------------------------------------
			// Popup a dialog to allow the user to select the device
			//------------------------------------------------------------------
			
			hr = piMgrUI->GetDevice(NULL, piMgr, &piPlatform, &piDevice);
			ERROR_EXIT((FAILED(hr) || !piDevice), 5, ConstructorErrorExit);
			
						
			//------------------------------------------------------------------
			// Connect to the device
			//------------------------------------------------------------------
			
			hr = piMgrUI->Connect(NULL, piDevice, &piConnection);
			ERROR_EXIT((FAILED(hr) || !piConnection), 6, ConstructorErrorExit);

		}
		
		m_OK = true; // Ick! Ick!
		return;


	ConstructorErrorExit:
		
		MyMessageBox(_T("PlatMan CETK Connection"), MB_OK, _T("Resource Consumer was unable to form a connection to the device. Error=%d"), errnum);
		
	}

#endif

	
public:
		
	virtual ~CPlatManCetkConnection()
	{
#ifdef UNDER_NT
		if ( piConnection )
			piConnection->Release ();
		
		if ( piDevice )
			piDevice->Release ();
		
		if ( piPlatform )
			piPlatform->Release ();
		
		if ( piMgrUI )
			piMgrUI->Release ();

		if ( piMgr )
			piMgr->Release ();

		::CoUninitialize();
#endif
	}
	
	virtual bool IsConnected()
	{
		return (NULL != pStream);
	}
	
	virtual bool IsListening()
	{
#ifdef UNDER_CE
		return (NULL != pStream);
#else
		return false;
#endif
	}

	virtual bool CanDoServerOnHost()
	{
		return false;
	}

	virtual int Receive(char FAR *Buffer, int Length)
	{
		HRESULT hr = S_OK;
		DWORD dwSizeRecvd;
	
		if (!(pStream && Buffer)) {
			return -1;
		}
	
#ifdef UNDER_CE
		hr = pStream->ReadBytes ((unsigned char*)Buffer, Length, &dwSizeRecvd);
#else
		hr = pStream->ReadBytes (Length, (unsigned char*)Buffer, &dwSizeRecvd);
#endif
		if (SUCCEEDED(hr) && (dwSizeRecvd == (DWORD) Length)) {
			return Length;
		}
	
		return -1;
	}
	

	virtual int Send(char FAR *pData, int cbData)
	{
		HRESULT hr = S_OK;
	
		if (!(pStream && pData)) {
			return -1;
		}

#ifdef UNDER_CE
		hr = pStream->WriteBytes(pData, cbData);
		if (SUCCEEDED(hr))
		{
			return cbData;
		}
#else
		DWORD dwSizeSent;
		hr = pStream->Send(cbData, (BYTE *)pData, & dwSizeSent);

		if (SUCCEEDED(hr) && (dwSizeSent == (DWORD)cbData))
		{
			return cbData;
		}
#endif
	
		return -1;
	}
	

#ifdef UNDER_CE
	virtual BOOL ListenForClient()
	{
		BOOL brc = FALSE;
		CREATESTREAMFUNC pfnCreateStream = NULL;
	
		OutputDebugString(_T("Initializing Platman connection...\r\n"));
	
		// Load the Transport Library
		platModule = ::LoadLibrary (L"CETLSTUB.DLL");
	
		if ( platModule )
		{
			pfnCreateStream = (CREATESTREAMFUNC) GetProcAddress (platModule, L"CreateStream");
	
			/*--------------------------------------------------------------*/
			// Create the stream that matches up to the desktop (GUID)
			/*--------------------------------------------------------------*/
	
			pStream = NULL;
			pStream = pfnCreateStream (streamGUID, 0);
	
			if ( pStream )
			{
				brc = TRUE;
			}
		}
	
		return brc;
	}

	virtual BOOL ConnectToService()
	{
		// The only thing that works for Platman is server-on-device-client-on-host.
		return FALSE;
	}

	virtual VOID Disconnect()
	{
		if ( pStream )
		{
			
			pStream->Close();
			delete pStream;
			pStream = NULL;
		}

		if ( platModule )
		{
			::FreeLibrary (platModule);
		}

	}

#else
	virtual BOOL ListenForClient()
	{
		// The only thing that works for Platman is server-on-device-client-on-host.
		return FALSE;
	}
	
	virtual BOOL ConnectToService()
	{
		pStream = NULL;
		

		//------------------------------------------------------------------
		// Remember the name of the platform, in case you ever want to mention it.
		//------------------------------------------------------------------
		
		LPOLESTR pszName;
		piPlatform->GetPlatformName (& pszName);
		(void) StringCchCopy(szOtherName, ARRAY_LENGTH(szOtherName), pszName);


		//------------------------------------------------------------------
		// Create the stream (Synchronous)
		//------------------------------------------------------------------
		
		HRESULT hr = piConnection->CreateStream(streamGUID,
										0,
										& pStream,
										NULL);		
		if ( FAILED(hr) )
		{
			MessageBox (NULL,
					   _T("Unable to create a stream"),
					   _T("PlatMan Connect-To-Service"),
					   MB_OK);
			pStream = NULL;
		}

			
		return NULL != pStream;
	}

	virtual VOID Disconnect()
	{
		if ( pStream )	// We have an open connection
		{
			//--------------------------------------------------------------
			// Close the stream
			//--------------------------------------------------------------

			pStream->Close();
			pStream->Release();
			pStream = NULL;
		}
	}

#endif


#ifdef UNDER_NT
		virtual BOOL ConfigureDevice()
		{
			//------------------------------------------------------------------
			// Popup a dialog to allow the user to configure platman
			//------------------------------------------------------------------
			
			HRESULT hr = piMgrUI->Configure(NULL);

			return !FAILED(hr);
		}

		virtual BOOL InstallAndLaunchServer(LPCWSTR lpwszDeviceEXEName)
		{			
				//------------------------------------------------------------------
				// Queue up the the package
				//------------------------------------------------------------------
			
			HRESULT hr = piConnection->QueuePackage(streamGUID, L"\\Windows", FALSE);
			if ( FAILED(hr) )
			{
				/* Don't put up a message box if this fails. The files might be available through \release
					MessageBox (hwnd,
							   _T("Unable to Queue Package"),
							   _T("Connect To Device"),
							   MB_OK);
					goto InstallAndLaunchError;
				 */
			}
			
				//------------------------------------------------------------------
				// Copy the package file(s) to the device
				//------------------------------------------------------------------
			
			DWORD dwSize = 0;
			hr = piConnection->CopyQueuedFiles(NULL, &dwSize);
			if ( FAILED(hr) && hr != STG_E_FILEALREADYEXISTS )
			{
				/* Don't put up a message box if this fails. The files might be available through \release
				MessageBox (hwnd,
						   _T("Unable to copy AppVerifierCE files to device"),
						   _T("Connect To Device"),
						   MB_OK);
				goto InstallAndLaunchError;
				*/
			}
						
			//------------------------------------------------------------------
			// Start up the device side executable
			//------------------------------------------------------------------
		
			hr = piConnection->Launch(lpwszDeviceEXEName, NULL);
			if ( FAILED(hr) )
			{
				MessageBox (NULL,
						   _T("Unable to launch device EXE"),
						   _T("Connect To Device"),
						   MB_OK);
				goto InstallAndLaunchError;
			}
			return true;

InstallAndLaunchError:
			
			return false;

		}
#endif

};


// ----------------------------------------------------------------------------
//

extern "C" {
	
	void DestroyCetkConn(CetkConnection conn)
	{
		delete reinterpret_cast<CCetkConnection*>(conn);
	}
	
	CetkConnection CreateCetkConn_Sockets(LPCSTR pServer, u_short port)
	{
		return reinterpret_cast<CetkConnection>(new CSocketCetkConnection(pServer, port));
	}
	
#ifdef UNDER_CE
	CetkConnection CreateCetkConn_Platman(GUID* platmanStreamID)
	{
		return reinterpret_cast<CetkConnection>(new CPlatManCetkConnection(platmanStreamID));
	}

#else
	CetkConnection CreateCetkConn_Platman(GUID* devID, GUID* platmanStreamID)
	{
		return reinterpret_cast<CetkConnection>(new CPlatManCetkConnection(devID, platmanStreamID));
	}

	CetkConnection CreateCetkConn_PlatmanPickDevice(GUID* platmanStreamID)
	{
		return reinterpret_cast<CetkConnection>(new CPlatManCetkConnection(NULL, platmanStreamID));
	}
	
#endif

	CetkConnection CreateCetkConn_CoreCon()
	{
		// NOT IMPLEMENTED
		return NULL;
	}
	
	
	BOOL CetkConn_IsConstructedOK(CetkConnection conn)
	{
		return reinterpret_cast<CCetkConnection*>(conn)->IsConstructedOK();
	}

	BOOL CetkConn_IsConnected(CetkConnection conn)
	{
		return reinterpret_cast<CCetkConnection*>(conn)->IsConnected();
	}
	
	BOOL CetkConn_IsListening(CetkConnection conn)
	{
		return reinterpret_cast<CCetkConnection*>(conn)->IsListening();
	}

	
	int CetkConn_Send (CetkConnection conn, char FAR *pData, int cbData)	
	{
		return reinterpret_cast<CCetkConnection*>(conn)->Send(pData, cbData);
	}
	
	int CetkConn_Receive (CetkConnection conn, char FAR *Buffer, int Length)	
	{
		return reinterpret_cast<CCetkConnection*>(conn)->Receive(Buffer, Length);
	}
	
	BOOL CetkConn_GetDWORD(CetkConnection conn, DWORD *pdw)	
	{
		return reinterpret_cast<CCetkConnection*>(conn)->GetDWORD(pdw);
	}
	
	BOOL CetkConn_SendDWORD(CetkConnection conn, DWORD dw)
	{
		return reinterpret_cast<CCetkConnection*>(conn)->SendDWORD(dw);
	}
	
	BOOL CetkConn_GetStringA(CetkConnection conn, LPSTR *ppszString)
	{
		return reinterpret_cast<CCetkConnection*>(conn)->GetStringA(ppszString);
	}
	
	BOOL CetkConn_SendStringA(CetkConnection conn, LPCSTR pszString)
	{
		return reinterpret_cast<CCetkConnection*>(conn)->SendStringA(pszString);
	}
	
	BOOL CetkConn_GetStringW(CetkConnection conn, LPWSTR *ppszString)
	{
		return reinterpret_cast<CCetkConnection*>(conn)->GetStringW(ppszString);
	}
	
	BOOL CetkConn_SendStringW(CetkConnection conn, LPCWSTR pszString)
	{
		return reinterpret_cast<CCetkConnection*>(conn)->SendStringW(pszString);
	}


	
	BOOL CetkConn_ListenForClient(CetkConnection conn)
	{
		return reinterpret_cast<CCetkConnection*>(conn)->ListenForClient();
	}
	
	BOOL CetkConn_ConnectToService(CetkConnection conn)	
	{
		return reinterpret_cast<CCetkConnection*>(conn)->ConnectToService();
	}
	
	VOID CetkConn_Disconnect(CetkConnection conn)
	{
		reinterpret_cast<CCetkConnection*>(conn)->Disconnect();
	}

	

#ifdef UNDER_NT
	LPCWSTR CetkConn_GetDeviceName(CetkConnection conn)
	{
		return reinterpret_cast<CCetkConnection*>(conn)->GetDeviceName();
	}

	BOOL CetkConn_ConfigureDevice(CetkConnection conn)
	{
		return reinterpret_cast<CCetkConnection*>(conn)->ConfigureDevice();
	}
	
	BOOL CetkConn_InstallAndLaunchServer(CetkConnection conn, LPCWSTR lpwszDeviceEXEName)
	{
		return reinterpret_cast<CCetkConnection*>(conn)->InstallAndLaunchServer(lpwszDeviceEXEName);
	}
	
#endif

}
