//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// constants, enums, etc. for diners.cpp

#define FAILURE 0xFFFFFFFF
#define MAXTRIES 100
#define MAXWAIT 10000L
#define MINUTE 60000L
#define RANDOM_SECONDS Random()>>24
// #define DP_API extern "C" __declspec(dllexport)
#define THREAD_DEATH 15000L		// wait 15 seconds before calling a thread 'hung'

// How long do they eat?

enum DinerDineType
{
	DINE_RANDOM,	// random eating time
	DINE_FIXED		// eat for this many msec
};

enum DinerStartType
{
	START_TOGETHER,	// everyone just dives in
	START_RANDOM	// stagger the starting times
};

