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
// Contains Window Names, file names, test message defns etc..,
// used by all tests

#define APPNAME1	TEXT("nfyApp.exe")

#define MUTEX1		TEXT("SOMEUNIQUENAME")
#define MEMORYMAPFILENAME1	TEXT("SOMEUNIQUEMEMORYMAPFILENAME")

// This info is SHARED between tux test and launched app
typedef struct NotificationTestData {
	int count;						//How many apps have been launched
	SYSTEMTIME	LaunchTime[100];	//When was App launched. We'll currently store 100 entries only.
	TCHAR CmdLine[100][MAX_PATH];	//Command line of each app launched
} NOTIFTESTDATA;
