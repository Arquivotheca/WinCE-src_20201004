//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// SDPUtilWalk.h

#ifndef __SDPUTILWALK_H__
#define __SDPUTILWALK_H__

#include <bthapi.h>

class CSDPUtilWalk:public ISdpWalk
{
public:

	STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject);
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();

	STDMETHODIMP CSDPUtilWalk::WalkStream(UCHAR elementType, ULONG elementSize, UCHAR *pStream);
    STDMETHOD(WalkNode)(NodeData *pData, ULONG state);
};

#endif //__SDPUTILWALK_H__