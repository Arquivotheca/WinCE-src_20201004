//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include <tchar.h>
#include <ws2bth.h>
#include <stressutils.h>
#include "util.h"

int GetBA (WCHAR *pp, BT_ADDR *pba) {
	while (*pp == ' ')
		++pp;

	for (int i = 0 ; i < 4 ; ++i, ++pp) {
		if (! iswxdigit (*pp))
			return FALSE;

		int c = *pp;
		if (c >= 'a')
			c = c - 'a' + 0xa;
		else if (c >= 'A')
			c = c - 'A' + 0xa;
		else c = c - '0';

		if ((c < 0) || (c > 16))
			return FALSE;

		*pba = *pba * 16 + c;
	}

	for (i = 0 ; i < 8 ; ++i, ++pp) {
		if (! iswxdigit (*pp))
			return FALSE;

		int c = *pp;
		if (c >= 'a')
			c = c - 'a' + 0xa;
		else if (c >= 'A')
			c = c - 'A' + 0xa;
		else c = c - '0';

		if ((c < 0) || (c > 16))
			return FALSE;

		*pba = *pba * 16 + c;
	}

	if ((*pp != ' ') && (*pp != '\0'))
		return FALSE;

	return TRUE;
}

BOOL PrintBuffer(BYTE * pBuffer, DWORD dwSize)
{
	TCHAR szLine1[32];
	TCHAR szLine2[32];
	TCHAR szLine[64];

	for (DWORD i = 0; i < dwSize; i ++)
	{
		if (i % 8 == 0)
		// New line
		{
			_tcscpy(szLine1, TEXT(""));
			_tcscpy(szLine2, TEXT(""));
			_tcscpy(szLine, TEXT(""));
		}
		_stprintf (szLine1, TEXT("%s %02x"), szLine1, pBuffer[i]);
		if ((pBuffer[i] > '!') && (pBuffer[i] < '~'))
			_stprintf(szLine2, TEXT("%s %c"), szLine2, pBuffer[i]);
		else
			_stprintf(szLine2, TEXT("%s %c"), szLine2, TEXT('.'));
		if (i % 8 == 7)
		// End of line
		{
			_stprintf (szLine, TEXT("      %03x %s | %s"), i - 7, szLine1, szLine2);
			LogFail(TEXT("%s"), szLine);
		}
	}
	if ((i % 8 != 7) && (i % 8 != 0))
	// End of line
	{
		for (DWORD j = 0; j < 8 - i % 8; j ++)
		{
			_stprintf (szLine1, TEXT("%s   "), szLine1);
		}
		_stprintf (szLine, TEXT("      %03x %s | %s"), i - 7, szLine1, szLine2);
		LogFail(TEXT("%s"), szLine);
	}

	return TRUE;
}

BOOL FormatBuffer(BYTE * pBuffer, DWORD dwSize, DWORD dwIndex)
{
	for (DWORD i = 0; i < dwSize; i ++)
	{
		pBuffer[i] = (BYTE)((dwIndex + i) & 0xFF);
	}

	return TRUE;
}

BOOL VerifyBuffer(BYTE * pBuffer, DWORD dwSize, DWORD dwIndex)
{
	for (DWORD i = 0; i < dwSize; i ++)
	{
		if (pBuffer[i] != ((dwIndex + i) & 0xFF))
		{
			LogFail(TEXT("Invalid data received"));
			LogFail(TEXT("Expecting buffer contains bytes sequencially increasing from 0x%02x"), dwIndex & 0xFF);
			LogFail(TEXT("Got:"));
			PrintBuffer(pBuffer, dwSize);
			return FALSE;
		}
	}

	return TRUE;
}