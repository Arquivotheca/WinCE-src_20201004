//
// Copyright (c) 2009 Microsoft Corporation.  All rights reserved.
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  herr.h
//    Provides HRESULT error handling macros.
//
//  Revision History:
//      Jonathan Leonard (a-joleo) : 11/17/2009 - Created.
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

#define HERR(hr, msg, handler)  HERR3(hr, msg, 0, 0,handler)
#define HERR2(hr, msg, handler1,handler2)  HERR3(hr, msg, 0, handler1,handler2)
#define HERR3(hr, msg, handler1,handler2,handler3)  if (FAILED(hr)){g_pKato->Log(LOG_FAIL, msg);handler1;handler2;handler3;}