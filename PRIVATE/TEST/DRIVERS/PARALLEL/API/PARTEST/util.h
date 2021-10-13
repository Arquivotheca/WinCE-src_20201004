//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __UTIL_H__
#define __UTIL_H__

#include <windows.h>
#include <kato.h>
#include <main.h>

extern CKato* g_pKato;

void PrintCommStatus( DWORD );
void PrintCommTimeouts( LPCOMMTIMEOUTS );
void PrintCommProperties( LPCOMMPROP );
void ErrorMessage( DWORD );

#endif
