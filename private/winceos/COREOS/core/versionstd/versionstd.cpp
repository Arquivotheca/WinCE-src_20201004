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
#include <windows.h>
#include <bldver.h>

/*
    @doc BOTH EXTERNAL

    @func BOOL | GetVersionEx | Returns version information for the OS.
    @parm LPOSVERSIONINFO | lpver | address of structure to fill in.

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
extern "C"
BOOL GetVersionEx(LPOSVERSIONINFO lpver) 
{
    BOOL fRet = (lpver->dwOSVersionInfoSize == sizeof(OSVERSIONINFO));
    DEBUGCHK(lpver->dwOSVersionInfoSize == sizeof(OSVERSIONINFO));

    if (fRet) {
        // do not set the version info size as it is filled by the caller
        //lpver->dwOSVersionInfoSize = sizeof(OSVERSIONINFO);        
        lpver->dwMajorVersion = CE_MAJOR_VER;
        lpver->dwMinorVersion = CE_MINOR_VER;
        lpver->dwBuildNumber = CE_BUILD_VER;
        lpver->dwPlatformId = VER_PLATFORM_WIN32_CE;
        lpver->szCSDVersion[0] = '\0';
    }

    return fRet;
}