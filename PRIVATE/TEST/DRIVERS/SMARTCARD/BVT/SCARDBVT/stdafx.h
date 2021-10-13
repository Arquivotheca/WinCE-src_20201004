/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.
Copyright (c) 1998,1999  Microsoft Corporation

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