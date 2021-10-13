//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
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