//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef __SCROLLBAR_HPP__
#define __SCROLLBAR_HPP__

enum SCROLL_STATE;
struct ScrollBarInfoInternal;

class ScrollBar
{
public:
	
	static
	ScrollBarInfoInternal *
	InitSBInfoInternal(
		HWND	hwnd,
		BOOL	fVert,
		int		fnBar
		);
	
	static
	BOOL
	CeSetScrollInfo(
		HWND		hwndSB,
		int			fnBar,
		SCROLLINFO	siNew
		);

	static
	BOOL
	CeGetScrollInfo(
		HWND					hwnd,
		ScrollBarInfoInternal	*psbii,
		SCROLLINFO				*psi
		);
	
	static
	void
	CALLBACK
	ScrollTimer(
		HWND	hwnd,
		UINT	uMsg,
		UINT	idEvent,
		DWORD	dwTime
		);
	
    static
	unsigned 
	int
	SbStateToCmd(
		SCROLL_STATE	state
		);

	static
	int
        SetScrollInfo_Worker(
        HWND        hwnd,
        int         fnBar,
        __in const  SCROLLINFO*    psi,
        BOOL        bRedraw
        );

	static
	int
	SetScrollInfo_I(
				HWND		hwnd,
				int			fnBar,
		const	SCROLLINFO*	psi,
				BOOL		bRedraw
		);
	
	static
	int
	SetScrollPos_I(
		HWND	hwnd,
		int		fnBar,
		int		nPos,
		BOOL	bRedraw
		);
	
	static
	BOOL
	SetScrollRange_I(
		HWND	hwnd,
		int		fnBar,
		int		nMinPos,
		int		nMaxPos,
		BOOL	bRedraw
		);
	
	static
	BOOL
	GetScrollInfo_I(
		HWND		hwnd,
		int			fnBar,
		SCROLLINFO	*psi
		);      
	
	static
	void
	SBTrackLoop(
		HWND	hwnd,
		HDC		hdc,
		LPARAM	lp,
		BOOL fVert
		);      
};

#endif // !__SCROLLBAR_H__
