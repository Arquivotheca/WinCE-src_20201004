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
#pragma once

typedef struct _FRECT { 
  FLOAT left; 
  FLOAT top; 
  FLOAT right; 
  FLOAT bottom; 
} FRECT, *LPFRECT; 

typedef struct _PARTIALPRESENTTEST {
	FRECT fRectSrc;
	FRECT fRectDst;
} PARTIALPRESENTTEST, *LPPARTIALPRESENTTEST;


static struct _PARTIALPRESENTCONSTS {
	static const DWORD TestCaseCount = 13;
} PARTIALPRESENTCONSTS;


CONST PARTIALPRESENTTEST RelativePartialViewports[PARTIALPRESENTCONSTS.TestCaseCount] ={

	{{ 0.00f, 0.00f, 1.00f, 1.00f }, { 0.00f, 0.00f, 1.00f, 1.00f }}, // Present whole RECT

	{{ 0.00f, 0.00f, 0.50f, 1.00f }, { 0.00f, 0.00f, 0.50f, 1.00f }}, // Present left-side   to left-side   
	{{ 0.50f, 0.00f, 1.00f, 1.00f }, { 0.50f, 0.00f, 1.00f, 1.00f }}, // Present right-side  to right-side  
	{{ 0.00f, 0.00f, 1.00f, 0.50f }, { 0.00f, 0.00f, 1.00f, 0.50f }}, // Present top-side    to top-side    
	{{ 0.00f, 0.50f, 1.00f, 1.00f }, { 0.00f, 0.50f, 1.00f, 1.00f }}, // Present bottom-side to bottom-side 

	{{ 0.00f, 0.00f, 0.50f, 1.00f }, { 0.50f, 0.00f, 1.00f, 1.00f }}, // Present left-side   to right-side
	{{ 0.50f, 0.00f, 1.00f, 1.00f }, { 0.00f, 0.00f, 0.50f, 1.00f }}, // Present right-side  to left-side
	{{ 0.00f, 0.00f, 1.00f, 0.50f }, { 0.00f, 0.50f, 1.00f, 1.00f }}, // Present top-side    to bottom-side
	{{ 0.00f, 0.50f, 1.00f, 1.00f }, { 0.00f, 0.00f, 1.00f, 0.50f }}, // Present bottom-side to top-side

	{{ 0.00f, 0.00f, 0.50f, 1.00f }, { 0.00f, 0.00f, 1.00f, 1.00f }}, // Present left-side   to whole viewport
	{{ 0.50f, 0.00f, 1.00f, 1.00f }, { 0.00f, 0.00f, 1.00f, 1.00f }}, // Present right-side  to whole viewport
	{{ 0.00f, 0.00f, 1.00f, 0.50f }, { 0.00f, 0.00f, 1.00f, 1.00f }}, // Present top-side    to whole viewport
	{{ 0.00f, 0.50f, 1.00f, 1.00f }, { 0.00f, 0.00f, 1.00f, 1.00f }}, // Present bottom-side to whole viewport

};

