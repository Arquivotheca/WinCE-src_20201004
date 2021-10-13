//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

#define COM_NO_WINDOWS_H
#define RPC_NO_WINDOWS_H
//#define NOCOMM
#define NOCRYPT
//#define NOGDI
//#define NOICONS
#define NOIME
//#define NOMCX
//#define NOMDI
//#define NOMENUS
//#define NOMETAFILE
#define NOSOUND
//#define NOSYSPARAMSINFO
//#define NOWH
//#define NOWINABLE
//#define NOWINRES

//#include <nt.h>
//#include <ntrtl.h>
//#include <nturtl.h>

// This avoids duplicate definitions with Shell PIDL functions
// and MUST BE DEFINED!
#define AVOID_NET_CONFIG_DUPLICATES

#include <windows.h>
#include <objbase.h>

#ifndef UNDER_CE
#include <devguid.h>
#include <wchar.h>
#else
#include <cedefs.h>
// TODO: figure out what this is
#ifndef ZONE_DELAY
#define ZONE_DELAY 0
#endif
#endif

#include <tchar.h>

//#include "ncmem.h"
//#include "ncbase.h"
#include "ncdefine.h"
#include "ncdebug.h"
