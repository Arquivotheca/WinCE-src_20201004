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
// This module is implementing User Mode Driver Host.
//

#include <windows.h>
#include <devload.h>
#include <pwindbas.h>
#include <extfile.h>
#include "reflector.h"
#include "Udevice.hpp"

// This routine is the entry point for the device manager.  It simply calls the device
// management DLL's entry point.
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPWSTR lpCmdLine, int nCmdShow)
{
    int status=-1;
    DEBUGMSG(1,(TEXT("udevice.exe %s \r\n"),lpCmdLine));    
    DEBUGREGISTER(NULL);

    if (RegisterAFSAPI(lpCmdLine)) {
        BOOL bRet = (WaitForPrimaryThreadExit(INFINITE) ==  WAIT_OBJECT_0) ;
        ASSERT(bRet);
    }
    else
        ASSERT(FALSE);
    DEBUGMSG(1,(TEXT("exiting udevice.exe\r\n")));    
    UnRegisterAFSAPI();
    return status;
}
UserDriver * CreateUserModeDriver(FNDRIVERLOAD_PARAM& fnDriverLoadParam)
{
    return new UserDriver(fnDriverLoadParam,NULL);
}

BOOL HandleServicesIOCTL(FNIOCTL_PARAM& fnIoctlParam) {
    return FALSE;
}

