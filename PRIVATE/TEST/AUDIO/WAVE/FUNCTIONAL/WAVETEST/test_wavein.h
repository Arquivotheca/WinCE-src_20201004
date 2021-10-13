//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once
#ifndef __TEST_wavein_H__
#define __TEST_wavein_H__

TESTPROCAPI CaptureCapabilities		(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Capture			(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CaptureNotifications	(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);

#endif
