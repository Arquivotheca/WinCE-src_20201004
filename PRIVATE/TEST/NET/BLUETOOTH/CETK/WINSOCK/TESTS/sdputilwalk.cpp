//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
