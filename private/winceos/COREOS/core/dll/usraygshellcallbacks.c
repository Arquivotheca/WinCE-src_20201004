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

typedef BOOL (*SHCheckForContextMenuProc)(HWND, LPARAM, BOOL);
typedef void (*DoEditContextMenuProc)(HWND, BOOL);

// Handle to the aygshell instance loaded in user space
// Note that aygshell is kept loaded for performance reasons.
static HMODULE s_hmodUserAygshell = NULL;

// Function LoadAygshell loads the aygshell.dll if necessary. On failure, it returns NULL.
// On success, a handle to the DLL
HMODULE LoadAygshell()
{
    HMODULE hModUserAygshell= NULL;
    
    // See if aygshell has been loaded in this process (user mode)
    if (NULL == s_hmodUserAygshell)
    {
        // Load the aygshell library. This could happen more than once in
        // the worst case scenario; but that is OK. For most cases, the library
        // should already be loaded.
        hModUserAygshell = LoadLibrary(TEXT("aygshell.dll"));
        if (hModUserAygshell != NULL &&
            NULL != InterlockedCompareExchangePointer((PVOID*) &s_hmodUserAygshell, hModUserAygshell, NULL))
        {
            // Another thread set it already.
            FreeLibrary(hModUserAygshell);
        }
    }

    hModUserAygshell = s_hmodUserAygshell;

    return hModUserAygshell;
}


//User version of BOOL SHCheckForContextMenu(HWND hwnd, LPARAM lParam, BOOL fComboboxEdit)
BOOL
WINAPI
UserSHCheckForContextMenu(
    __in HPROCESS hprocDest,
    HWND hWnd,
    LPARAM lParam,
    BOOL fComboboxEdit
    )
{
    BOOL fResult = FALSE;
    
#ifdef KCOREDLL
    if (GetCurrentProcessId()!=(DWORD)hprocDest)
    {
        if ((g_UsrCoredllCallbackArray != NULL) &&
            g_UsrCoredllCallbackArray[USRCOREDLLCB_SHCheckForContextMenu])
        {
            CALLBACKINFO cbi;
            cbi.hProc = hprocDest;
            cbi.pfn = g_UsrCoredllCallbackArray[USRCOREDLLCB_SHCheckForContextMenu];
            cbi.pvArg0 = hprocDest;
            __try
            {
                fResult = (LRESULT)PerformCallBack(&cbi, hWnd, lParam, fComboboxEdit) && !cbi.dwErrRtn;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
    }
#else
    HMODULE hModUserAygshell= NULL;
    SHCheckForContextMenuProc pfnSHCheckForContextMenu = NULL;
    UNREFERENCED_PARAMETER(hprocDest);

    hModUserAygshell = LoadAygshell();
    if( hModUserAygshell != NULL )
    {
        pfnSHCheckForContextMenu = (SHCheckForContextMenuProc)GetProcAddress(hModUserAygshell, (LPCTSTR)48);
        if (NULL == pfnSHCheckForContextMenu)
        {
            ERRORMSG(1, (TEXT("Could not find SHCheckForContextMenu.\r\n")));
            SetLastError(ERROR_PROC_NOT_FOUND);
        }
        else
        {
            // Call SHCheckForContextMenu in Aygshell
            fResult = pfnSHCheckForContextMenu(hWnd, lParam, fComboboxEdit);
        }
    }
#endif

    return fResult;
}

//User version of void DoEditContextMenu(HWND hwnd, BOOL fComboboxEdit)
void
WINAPI
UserDoEditContextMenu(
    __in HPROCESS hprocDest,
    HWND hWnd,
    BOOL fComboboxEdit
    )
{
#ifdef KCOREDLL
    if (GetCurrentProcessId()!=(DWORD)hprocDest)
    {
        if ((g_UsrCoredllCallbackArray != NULL) &&
            g_UsrCoredllCallbackArray[USRCOREDLLCB_DoEditContextMenu])
        {
            CALLBACKINFO cbi;
            cbi.hProc = hprocDest;
            cbi.pfn = g_UsrCoredllCallbackArray[USRCOREDLLCB_DoEditContextMenu];
            cbi.pvArg0 = hprocDest;
            __try
            {
                (void)PerformCallBack(&cbi, hWnd, fComboboxEdit);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
    }
#else
    HMODULE hModUserAygshell= NULL;
    DoEditContextMenuProc pfnDoEditContextMenu = NULL;
    UNREFERENCED_PARAMETER(hprocDest);

    hModUserAygshell = LoadAygshell();
    if( hModUserAygshell != NULL )
    {
        pfnDoEditContextMenu = (DoEditContextMenuProc)GetProcAddress(hModUserAygshell, (LPCTSTR)226);
        if (NULL == pfnDoEditContextMenu)
        {
            ERRORMSG(1, (TEXT("Could not find DoEditContextMenu.\r\n")));
            SetLastError(ERROR_PROC_NOT_FOUND);
        }
        else
        {
            // Call DoEditContextMenuu in Aygshell
            pfnDoEditContextMenu(hWnd, fComboboxEdit);
        }
    }
#endif
}

