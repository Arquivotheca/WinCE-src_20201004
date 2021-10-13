//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// SdpWalk.cpp : Implementation of CSdpWalk
#include "stdafx.h"
#include "BthAPI.h"
#include "SdpWalk.h"

/////////////////////////////////////////////////////////////////////////////
// CSdpWalk


STDMETHODIMP CSdpWalk::WalkNode(NodeData *pData, ULONG state)
{
	// TODO: Add your implementation code here

	return S_OK;
}

STDMETHODIMP CSdpWalk::WalkStream(USHORT type, USHORT specificType, UCHAR *pStream, ULONG streamSize)
{
	// TODO: Add your implementation code here

	return S_OK;
}
