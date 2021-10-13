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
// SDPUtilWalk.cpp

#include <winsock2.h>
#include "SDPUtilWalk.h"
#include "util.h"

STDMETHODIMP CSDPUtilWalk::QueryInterface(REFIID iid, void **ppvObject)
{
	return NULL;
}

ULONG STDMETHODCALLTYPE CSDPUtilWalk::AddRef()
{
	return 2;
}

ULONG STDMETHODCALLTYPE CSDPUtilWalk::Release()
{
	return 1;
}

/////////////////////////////////////////////////////////////////////////////
// CSdpUtilWalk

STDMETHODIMP CSDPUtilWalk::WalkNode(NodeData *pData, ULONG state)
{
	OutStr(TEXT("Thread 0x%08x Type = 0x%04x, specific type = 0x%04x state = 0x%08x\n"), GetCurrentThreadId(), pData->type, pData->specificType, state);

	return S_OK;
}

STDMETHODIMP CSDPUtilWalk::WalkStream(UCHAR elementType, ULONG elementSize, UCHAR *pStream)
{
	OutStr(TEXT("Thread 0x%08x elementType = 0x%02x, elementSize = 0x%08x\n"), GetCurrentThreadId(), elementType, elementSize);

	return S_OK;
}
