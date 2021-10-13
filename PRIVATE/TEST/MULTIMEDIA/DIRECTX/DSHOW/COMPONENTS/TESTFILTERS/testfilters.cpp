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
#include <streams.h>

#include "ColorTestUids.h"
#include "ColorTestSource.h"
#include "ColorTestSink.h"

#include "TeeTestUids.h"
//#include "TeeTestSource.h"
#include "TeeTestSink.h"

//
//  Object creation template
//
CFactoryTemplate g_Templates[] = {
    { L"Color Test Source"
    , &CLSID_ColorTestSource
    , CColorTestSource::CreateInstance
    , NULL
    , &ColorTestSourceDesc },
    { L"Color Test Sink"
    , &CLSID_ColorTestSink
    , CColorTestSink::CreateInstance
    , NULL
    , &ColorTestSinkDesc },

	{ L"Tee Test Sink"
    , &CLSID_TeeTestSink
    , CTeeTestSink::CreateInstance
    , NULL
    , &TeeTestSinkDesc }
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);


////////////////////////////////////////////////////////////////////////
//
// Exported entry points for registration and unregistration 
// (in this case they only call through to default implementations).
//
////////////////////////////////////////////////////////////////////////

STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2(FALSE);
}

//
// DllEntryPoint
//
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, 
                      DWORD  dwReason, 
                      LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

//HRESULT DllCanUnloadNow()