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

// constant values for PCL

#define PCL_MAX_MAJORVERSION 6
#define PCL_TITLEBAR_HEIGHT 26
#define PCL_WORKAREA_PORTRAITBOTTOM 320
#define PCL_WORKAREA_LANDSCAPEBOTTOM 240

// NOTE NOTE NOTE - legacy PPC tahoma font has been removed from the system
// and PCL has been parmanently disabled. This code will not get executed and will soon be removed.
// facename of the font used in PPC v6 used for PCLed applications (this font NO LONGER EXISTS IN THE SYSTEM)
static const TCHAR gc_szLegacyPPCFont[] = TEXT("Legacy_PPC_Tahoma");

// Registry Path for PCL mode
static const TCHAR gc_szPCLRegPath[]      = TEXT("SYSTEM\\PCL");

// given the passed in rc, determine if portrait or landscape.  Works best of actual working area rc is passed in
inline BOOL GetPclWorkAreaBottom(RECT rc) 
{
    return rc.right >= rc.bottom? PCL_WORKAREA_LANDSCAPEBOTTOM : PCL_WORKAREA_PORTRAITBOTTOM;
}

// window property set when running under PCL.  
// if property doesn't exist, window is not running under PCL
// if property set to 1, then running under PCL
const WCHAR c_szPclEnabled[] = L"PclEnabled";

inline BOOL IsWindowPclEnabled(HWND hWnd)  
{
    return GetProp(hWnd, c_szPclEnabled) != FALSE;
}

inline BOOL IsHierarchyWindowPclEnabled(HWND hWnd)  
{
    HWND hTempWindow = hWnd; 
    HWND hTopWindow = hWnd; 
    while(hTempWindow)
    {
        hTopWindow = hTempWindow;
        hTempWindow = GetParent(hTempWindow);
    }

    return IsWindowPclEnabled(hTopWindow);
}


