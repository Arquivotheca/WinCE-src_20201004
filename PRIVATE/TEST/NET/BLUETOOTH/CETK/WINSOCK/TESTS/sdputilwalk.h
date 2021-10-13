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