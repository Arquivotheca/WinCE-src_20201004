//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

