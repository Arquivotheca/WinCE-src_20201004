//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __COM_MACROS__
#define __COM_MACROS__

#define CHECK(x)                {HRESULT hr; if(FAILED(hr = (x)))return hr; }

#define CHECK_POINTER(p)        if(!p)return E_POINTER;

#define CHECK_OUT_POINTER(p)	CHECK_POINTER(p); Assert(*p == NULL);

#endif // __COM_MACROS__
