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
#include <coredll.h>
#include <UsrCoredllCallbacks.h>

FARPROC * g_UsrCoredllCallbackArray = NULL;

BOOL WINAPI InitializeUsrCoredllCallbacks()
{
    BOOL bRet = FALSE;
#ifdef KCOREDLL
    HMODULE hModUSerCoredll = NULL;

    // The callback array has already been initialized by a previous module.
    if( g_UsrCoredllCallbackArray )
        {
        bRet = TRUE;
        goto exit;
        }

    // when coredll module is built, there are two versions of it built - one is coredll.dll which is the one used by all the
    // user mode applications/dlls while the 2nd version is kcoredll.dll which is used by the kernel. So, when we need to
    // callback into user process's coredll fucntions, we need to get the pointers to the user mode coredll.dll functions rather
    // than using the fucntion names directly which gets us the pointers to similar fucntions in kcoredll.dll we load user mode
    // coredll get function poiters from there. As coredll.dll is loaded by the kernel during bootup & stays at the same 
    // location all the time, we can use the pointers for any user process/dll.
    hModUSerCoredll = (HMODULE) (*g_pfnKLibIoctl) ((HANDLE) KMOD_CORE, IOCTL_KLIB_GETUSRCOREDLL, NULL, 0, NULL, 0, NULL);

    // If user mode coredll exists, allocate space for the pointers only if we belong to kcoredll.dll
    if( !hModUSerCoredll )
        {
        goto exit;
        }

    g_UsrCoredllCallbackArray = (FARPROC*) LocalAlloc(0, sizeof(FARPROC)*USRCOREDLLCB_MAXCALLBACKS);

    if( !g_UsrCoredllCallbackArray )
        {
        goto exit;
        }

    // Initializing (zeroing) of the allocated memory
    memset( g_UsrCoredllCallbackArray, 0, sizeof(FARPROC)*USRCOREDLLCB_MAXCALLBACKS);

//    Get the callback function addresses 
#define GetUsrCoredllCallbackEntry(Name)                                                                                     \
            g_UsrCoredllCallbackArray[USRCOREDLLCB_##Name]= (FARPROC)GetProcAddress( hModUSerCoredll, __TEXT(#Name));        \
            if ( g_UsrCoredllCallbackArray[USRCOREDLLCB_##Name] == NULL )                                          \
                {                                                                                                                       \
                LocalFree(g_UsrCoredllCallbackArray);                                                                    \
                g_UsrCoredllCallbackArray = NULL;                                                                  \
                                                                                                                                         \
                ERRORMSG(1, (TEXT("Could not find ") __TEXT(#Name) __TEXT(".\r\n")));         \
                SetLastError(ERROR_PROC_NOT_FOUND);                                                         \
                goto exit;                                                                                                         \
                }

#define GetUsrCoredllCallbackEntryByOrdinal_Opt(Name, Ordinal) \
            g_UsrCoredllCallbackArray[USRCOREDLLCB_##Name]= (FARPROC)GetProcAddress( hModUSerCoredll, (LPCTSTR)Ordinal);

    // Get pointers to usermode coredll callback functions from user mode coredll
    
    GetUsrCoredllCallbackEntry(IsForcePixelDoubling);
    
    GetUsrCoredllCallbackEntryByOrdinal_Opt(InitializeUserRenderCore, 2733);
    GetUsrCoredllCallbackEntryByOrdinal_Opt(UninitializeUserRenderCore, 2734);
    GetUsrCoredllCallbackEntryByOrdinal_Opt(CallWindowProcW, 2880);
    GetUsrCoredllCallbackEntryByOrdinal_Opt(SHCheckForContextMenu, 2892);
    GetUsrCoredllCallbackEntryByOrdinal_Opt(DoEditContextMenu, 2893);
    GetUsrCoredllCallbackEntryByOrdinal_Opt(SHSipPreference, 2911);

    bRet = TRUE;
    
exit:
#endif
    return bRet;
}

