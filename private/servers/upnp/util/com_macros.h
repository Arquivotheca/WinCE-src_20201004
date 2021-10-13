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
#ifndef __COM_MACROS__
#define __COM_MACROS__

#define CHECK(x)                {HRESULT hr; if(FAILED(hr = (x))) {return hr;} }

#define CHECK_POINTER(p)        if(!p) { return E_POINTER; }

#define CHECK_OUT_POINTER(p)    CHECK_POINTER(p); Assert(*p == NULL);

#endif // __COM_MACROS__


