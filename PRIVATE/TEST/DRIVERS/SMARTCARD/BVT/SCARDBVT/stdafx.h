//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  
    StdAfx.h

Abstract:  
    Smart Card Usage Test
    
Notes: 

--*/
#ifndef __STDAFX_H_
#define __STDAFX_H_
#include <windows.h>
#ifndef UNDER_CE

#define ASSERT _ASSERT

#ifdef DEBUG
#define _DEBUG
#endif

#include <stdio.h>
#include <crtdbg.h>

#endif

#endif