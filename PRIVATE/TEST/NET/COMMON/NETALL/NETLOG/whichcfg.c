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

/*
	Module Name:

		whichcfg.c
	 
	Abstract:

		WhichConfig() will return an index into an array of strings, indicating
		the standard Windows CE configuration currently running.

	Functions:

		WinMain( ..... )
		BOOL WhichConfig( LPTSTR lpConfigString )
		BOOL WhichVersion( LPTSTR lpVersionString )
		DWORD WhichBuild( void )
	
	Notes:

-------------------------------------------------------------------------------
*/

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#if defined(STANDALONE)
#include "whichcfg.h"

#else // STANDALONE
#include "whichcfg.h"
#include "_whchcfg.h"

#endif // STANDALONE

// ----------------------------------------------------------------------------
// Macros
// ----------------------------------------------------------------------------

#define countof(x) (sizeof(x)/sizeof(*(x)))

#if defined(STANDALONE)

// ----------------------------------------------------------------------------
// int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow )
// ----------------------------------------------------------------------------

//
// Simple app to demonstrate how to use the cfginfo library
//

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow )
{
	TCHAR szTemp[1024];
	TCHAR szReturnString[MAX_CONFIG_STRING_LENGTH];

	if( WhichConfig(szReturnString) )
	{
		_stprintf( szTemp, TEXT("Config: %s\r\n"), szReturnString );
		OutputDebugString( szTemp );	
	}
	
	if( WhichVersion(szReturnString) )
	{
		_stprintf( szTemp, TEXT("Version: %s\r\n"), szReturnString );
		OutputDebugString( szTemp );	
	}

	_stprintf( szTemp, TEXT("Build: %d\r\n"), WhichBuild() );
	OutputDebugString( szTemp );	
	
	return 0;
}

#else // STANDALONE

//
// Here is the actual library
//

// ----------------------------------------------------------------------------
// BOOL WhichConfig( LPTSTR lpConfigString )
// ----------------------------------------------------------------------------

BOOL WhichConfig( LPTSTR lpConfigString )
{
	CONFIGS cfg;
	HKEY hKey;
	TCHAR szConfigName[MAX_CONFIG_STRING_LENGTH];
	DWORD dwType;
	DWORD dwSize;
	LONG lResult;
	TCHAR szTemp[1024];

	// Open registry key
	lResult = RegOpenKeyEx( HKEY_LOCAL_MACHINE, TEXT("\\Ident"), 0, 0, &hKey );
	if( ERROR_SUCCESS != lResult )
	{
		_stprintf( szTemp, TEXT("Unable to open registry key. Error: %d\r\n"), lResult );
		OutputDebugString( szTemp );
		return FALSE;
	}

	// Read the contents of HKLM\Ident\Desc
	dwSize = sizeof( szConfigName );
	lResult = RegQueryValueEx( hKey, TEXT("Desc"), 0, &dwType, (LPBYTE)szConfigName, &dwSize );
	if( ERROR_SUCCESS != lResult )
	{
		_stprintf( szTemp, TEXT("Unable to query registry value. Error: %d\r\n"), lResult );
		OutputDebugString( szTemp );
		return FALSE;
	}

	// Close the key
	RegCloseKey( hKey );

	// Which config are we running on?
	for( cfg = UNKNOWN; cfg < MAXIMUM_CONFIGS; cfg++ )
	{
		if( 0 == _tcscmp(szConfigName, cszKeyValues[cfg]) )
		{
			_tcsncpy( lpConfigString, cszFriendlyNames[cfg], MAX_CONFIG_STRING_LENGTH );
			lpConfigString[MAX_CONFIG_STRING_LENGTH - 1] = _T('\0');
			return TRUE;
		}
	}
	
	// Couldn't tell.
	_stprintf( lpConfigString, TEXT("UNKNOWN") );
	return FALSE;
}

// ----------------------------------------------------------------------------
// DWORD WhichBuild( void )
// ----------------------------------------------------------------------------

DWORD WhichBuild( void )
{
	OSVERSIONINFO sVersionInfo;
	TCHAR szTemp[1024];

	sVersionInfo.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
	if( GetVersionEx(&sVersionInfo) )
	{
		return sVersionInfo.dwBuildNumber;
	}
	else
	{
		_stprintf( szTemp, TEXT("GetVersionEx failed. Error: %d\r\n"),
			GetLastError() );
		OutputDebugString( szTemp );
		return 0;
	}
}

// ----------------------------------------------------------------------------
// BOOL WhichVersion( LPTSTR lpVersionString )
// ----------------------------------------------------------------------------

BOOL WhichVersion( LPTSTR lpVersionString )
{
	OSVERSIONINFO sVersionInfo;
	TCHAR szTemp[1024];

	sVersionInfo.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
	if( GetVersionEx(&sVersionInfo) )
	{
		_stprintf( lpVersionString, TEXT("%d.%d"), sVersionInfo.dwMajorVersion, sVersionInfo.dwMinorVersion );
		return TRUE;
	}
	else
	{
		_stprintf( szTemp, TEXT("GetVersionEx failed. Error: %d\r\n"),
			GetLastError() );
		OutputDebugString( szTemp );
		return FALSE;
	}
}

#endif // STANDALONE