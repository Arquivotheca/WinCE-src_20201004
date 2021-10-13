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
#pragma once

#include <dshow.h>

class CDevices
{
public:
    CDevices(void);
    ~CDevices(void);
#ifndef _WIN32_WCE
    static HRESULT Select(HINSTANCE hInst, HWND hWndParent, REFCLSID clsidDeviceClass, LPCTSTR pszCaption, IBaseFilter ** ppBaseFilter);
#endif
private:
    static void Enumerate(HWND hWndList, void * pData);
    static void OnRemoveItem(void * pData, BOOL bSelected);
};

