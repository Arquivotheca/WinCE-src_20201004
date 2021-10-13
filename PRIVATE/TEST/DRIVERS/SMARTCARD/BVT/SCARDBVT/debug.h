//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __DEBUG_H_
#define __DEBUG_H_

extern "C" void TRACE(LPCTSTR szFormat, ...);

#ifndef UNDER_CE
#include <crtdbg.h>
#define ASSERT _ASSERT
#endif

#endif


