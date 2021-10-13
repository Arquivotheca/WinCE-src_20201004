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
//
// Prototypes of functions exported by wininet.dll and declared in wininet.h
//

//
// preferable to include wininet.h. However wininet.h is generated after this directory is built so we use a private version.
//
//#include <wininet.h>
#include <dubinet.h>

typedef HINTERNET (STDAPICALLTYPE *PInternetOpen)
	( IN LPCWSTR lpszAgent, IN DWORD dwAccessType, IN LPCWSTR lpszProxy OPTIONAL, IN LPCWSTR lpszProxyBypass OPTIONAL, IN DWORD dwFlags);

typedef HINTERNET (STDAPICALLTYPE *PInternetConnect)
	( IN HINTERNET hInternet, IN LPCWSTR lpszServerName, IN INTERNET_PORT nServerPort, IN LPCWSTR lpszUserName OPTIONAL, IN LPCWSTR lpszPassword OPTIONAL, IN DWORD dwService, IN DWORD dwFlags, IN DWORD dwContext); 

typedef HINTERNET ( STDAPICALLTYPE *PHttpOpenRequest)
	( IN HINTERNET hConnect, IN LPCWSTR lpszVerb, IN LPCWSTR lpszObjectName, IN LPCWSTR lpszVersion, IN LPCWSTR lpszReferrer OPTIONAL, IN LPCWSTR FAR * lplpszAcceptTypes OPTIONAL, IN DWORD dwFlags, IN DWORD dwContext);


typedef BOOL (STDAPICALLTYPE  *PHttpSendRequest)
	( IN HINTERNET hRequest, IN LPCWSTR lpszHeaders OPTIONAL, IN DWORD dwHeadersLength, IN LPVOID lpOptional OPTIONAL, IN DWORD dwOptionalLength);

typedef BOOL (STDAPICALLTYPE *PHttpQueryInfo)
	( IN HINTERNET hRequest, IN DWORD dwInfoLevel, IN OUT LPVOID lpBuffer OPTIONAL, IN OUT LPDWORD lpdwBufferLength, IN OUT LPDWORD lpdwIndex OPTIONAL);

typedef BOOL (STDAPICALLTYPE *PInternetReadFile)
	( IN HINTERNET hFile, IN LPVOID lpBuffer, IN DWORD dwNumberOfBytesToRead, OUT LPDWORD lpdwNumberOfBytesRead);

typedef BOOL ( STDAPICALLTYPE *PInternetCloseHandle)
	( IN HINTERNET hInternet);

typedef DWORD (STDAPICALLTYPE  *PInternetErrorDlg)(
     IN HWND hWnd, IN OUT HINTERNET hRequest, IN DWORD dwError, IN DWORD dwFlags, IN OUT LPVOID * lppvData);

typedef BOOL (STDAPICALLTYPE *PInternetSetOption)(
    IN HINTERNET hInternet OPTIONAL, IN DWORD dwOption, IN LPVOID lpBuffer, IN DWORD dwBufferLength);
