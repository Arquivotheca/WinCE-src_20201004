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
#ifndef __TEST_wavein_H__
#define __TEST_wavein_H__

#define HANDLE_LIMIT 257
#define MaxErrorText 80

struct LAUNCHCAPTURESOUNDTHREADPARMS{
    HANDLE       hCompletion;
    HWAVEIN      hwi;
    WAVEFORMATEX wfx;
};

TESTPROCAPI CaptureCapabilities		(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Capture			(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CaptureNotifications	(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CaptureVirtualFree	(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CaptureInitialLatency(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CaptureInitialLatencySeries(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CaptureMixing(UINT , TPPARAM , LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI CaptureMultipleStreams( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

#endif
