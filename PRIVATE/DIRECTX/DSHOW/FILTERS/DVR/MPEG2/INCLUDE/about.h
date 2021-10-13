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
//
// About.h
//
#ifndef __MM_AM_ABOUT_H__
#define __MM_AM_ABOUT_H__

// {ABB56A40-7098-11d0-8510-00A024D181B8}
DEFINE_GUID(CLSID_MMAboutBox, 
0xabb56a40, 0x7098, 0x11d0, 0x85, 0x10, 0x0, 0xa0, 0x24, 0xd1, 0x81, 0xb8);

#ifndef UNDER_CE
//
// CAbout
//
// Provides an 'about' property page
class CAbout : public CBasePropertyPage {
public:
    static WCHAR    szName[];
    static CLSID    ClsID;
    static CUnknown *CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);

    virtual HRESULT OnActivate(void);

private:

#ifdef __IFilter_INTERFACE_DEFINED__
    CAbout(LPUNKNOWN lpunk, HRESULT *phr);
#else
    CAbout(LPUNKNOWN lpunk);
#endif
    virtual ~CAbout() {}
    
    void SetVerString(int nID);

    CAbout(const CAbout&);
    CAbout& operator=(const CAbout&);
};
#endif // !UNDER_CE

#endif //__MM_AM_ABOUT_H__
