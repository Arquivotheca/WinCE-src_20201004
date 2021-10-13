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
////////////////////////////////////////////////////////////////////////////////
#include "TuxGraphTest.h"
#include "logging.h"
#include "BaseVRTestDesc.h"

BaseVRTestDesc::BaseVRTestDesc()
{
	m_State = State_Stopped;
	m_bStatePresent = false;

	m_bSrcRect = false;
	m_bDestRect = false;
	m_eTest = AllTest;

	memset( &m_rectSrc, 0, sizeof(RECT) );
	memset( &m_rectDest, 0, sizeof(RECT) );

	m_bUseVR = false;
}

BaseVRTestDesc::~BaseVRTestDesc()
{
	m_lstWndPos.clear();
	m_lstWndScaleLists.clear();
	m_lstTestWinPos.clear();
	m_lstStates.clear();
	m_lstSrcRects.clear();
	m_lstDestRects.clear();
}

RECT *
BaseVRTestDesc::GetSrcDestRect( bool bSrcRect )
{
	if ( bSrcRect )
		return &m_rectSrc;
	else
		return &m_rectDest;
}

WndRectList *
BaseVRTestDesc::GetSrcDestRectList( bool bSrcRect )
{
	if ( bSrcRect )
		return &m_lstSrcRects;
	else
		return &m_lstDestRects;
}

void
BaseVRTestDesc::SetSrcDestRectFlag( bool data, bool bSrcRect )
{
	if ( bSrcRect )
		m_bSrcRect = data;
	else
		m_bDestRect = data;
}