//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __COMMON_H__


#define	ARRAY_SIZE(rg)			((sizeof rg) / (sizeof rg[0]))
#define	RANDOM_INT(max, min)	((INT)((rand() % ((INT)max - (INT)min + 1)) + (INT)min))
#define	RANDOM_CHOICE			((BOOL)RANDOM_INT(TRUE, FALSE))
#define	NAME_VALUE_PAIR(tok)	{tok, L#tok}
#define	RECT_WIDTH(rc)			(abs((LONG)rc.right - (LONG)rc.left))
#define	RECT_HEIGHT(rc)			(abs((LONG)rc.bottom - (LONG)rc.top))
#define	DumpRECT(rc)			LogComment(L#rc L".left = %d " L#rc L".top = %d " L#rc L".right = %d " L#rc L".bottom = %d\r\n", rc.left, rc.top, rc.right, rc.bottom)
#define	DumpFailRECT(rc)		LogFail(L#rc L".left = %d " L#rc L".top = %d " L#rc L".right = %d " L#rc L".bottom = %d\r\n", rc.left, rc.top, rc.right, rc.bottom)
#define	DumpWarn1RECT(rc)		LogWarn1(L#rc L".left = %d " L#rc L".top = %d " L#rc L".right = %d " L#rc L".bottom = %d\r\n", rc.left, rc.top, rc.right, rc.bottom)
#define	DumpWarn2RECT(rc)		LogWarn2(L#rc L".left = %d " L#rc L".top = %d " L#rc L".right = %d " L#rc L".bottom = %d\r\n", rc.left, rc.top, rc.right, rc.bottom)
#define	DumpPOINT(pt)			LogComment(L#pt L".x = %d " L#pt L".y = %d\r\n", pt.x, pt.y)
#define	DumpFailPOINT(pt)		LogFail(L#pt L".x = %d " L#pt L".y = %d\r\n", pt.x, pt.y)
#define	DumpWarn2POINT(pt)		LogWarn2(L#pt L".x = %d " L#pt L".y = %d\r\n", pt.x, pt.y)
#define	DumpRGB(cr)				LogComment(L#cr L":Red = %d " L#cr L":Green = %d " L#cr L":Blue = %d\r\n", GetRValue(cr), GetGValue(cr), GetBValue(cr))
#define	DumpFailRGB(cr)			LogFail(L#cr L":Red = %d " L#cr L":Green = %d " L#cr L":Blue = %d\r\n", GetRValue(cr), GetGValue(cr), GetBValue(cr))

#define	_COLORREF_RANDOM_		RGB(rand() % 256, rand() % 256, rand() % 256)

#define	LOOPCOUNT_MIN			(0x10)
#define	LOOPCOUNT_MAX			(0x20)

#define	NAME_VALUE_PAIR(tok)	{tok, L#tok}

struct FLAG_DETAILS
{
	DWORD dwFlag;
	LPTSTR lpszFlag;
};


#define __COMMON_H__
#endif /* __COMMON_H__ */
