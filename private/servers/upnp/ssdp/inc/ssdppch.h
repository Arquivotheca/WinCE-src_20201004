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
#pragma once

#define COM_NO_WINDOWS_H

#include <windows.h>
#include <cedefs.h>
#include <stdlib.h>
#include <tchar.h>
#include <ncdefine.h>
#include <ncbase.h>
#include <ncdebug.h>
#include <upnpdevapi.h>
#include <ssdp.h>
#include <ssdpapi.h>
#include <common.h>
#include <ssdptypes.h>
#include <ssdpfunc.h>
#include <ssdpioctl.h>
#include <event.h>
#include <list.h>
#include <msgqueue.h>
#include <auto_xxx.hxx>
#include <HttpChannel.h>
#include <HttpRequest.h>
#include <upnpmem.h>
#include <svsutil.hxx>

extern SVSThreadPool* g_pThreadPool;
