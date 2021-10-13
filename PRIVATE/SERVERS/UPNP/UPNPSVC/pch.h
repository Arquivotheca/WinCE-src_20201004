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
#include <windows.h>
#include <winsock.h>
#include <upnpdevapi.h>
#include <httpext.h>
#include <ssdpapi.h>
#include <ssdpioctl.h>
#include "HttpRequest.h"
#include "upnpsvc.h"
#include <ncdefine.h>
#include <ncbase.h>
#include <ncdebug.h>
