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

#include <stdlib.h>
#include <tchar.h>

#include <ncdefine.h>
#include <ncbase.h>
#include <ncdebug.h>
#include <upnpdevapi.h>
#include "ssdp.h"
#include "ssdpapi.h"
#include "common.h"
#include "ssdptypes.h"
#include "ssdpfunc.h"
#include "ssdpioctl.h"
#include "event.h"
#include "list.h"
#include "msgqueue.h"

#include "auto_xxx.hxx"
#include "HttpRequest.h"
#include <svsutil.hxx>

extern SVSThreadPool* g_pThreadPool;
