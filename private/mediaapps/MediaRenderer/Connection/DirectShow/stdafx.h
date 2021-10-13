//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
// stdafx.h : include file for standard system include files,
//      or project specific include files that are used frequently,
//      but are changed infrequently

#pragma once

#include <atlbase.h>
extern CComModule _Module;
#include <atlcom.h>

#include <upnphost.h>
#include <mmsystem.h>
#include <streams.h>
#include <playlist.h>
#include <auto_xxx.hxx>
#include <hash_map.hxx>
#include <memory>
#include <algorithm>
#include <av_upnp.h>
#include <assert.h>
#include <ehm.h>

#include "RenderMp2Ts.h"
#include "IConnection.h"
#include "utilities.h"
#include "Connection.h"

#define Chk( x ) { if( FAILED( hr = ( x ))) { goto Cleanup; }}
#define ChkBool( x, err ) { if(( x ) != TRUE ) { hr = err; goto Cleanup; }}
#define ChkUPnP( x, err ) { if( FAILED( x )) { hr = err; goto Cleanup; }}