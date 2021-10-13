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
/***
*winapisupp.c - helper functions for Windows Apis
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Contains functions to work with Windows API
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>
#include <stdlib.h>
#include <Windows.h>
#ifndef _WIN32_WCE
#include <AppModel.h>

/*
 * GetCurrentPackageId retrieves the current package id, if the app is deployed via a package.
 * ARM has this API on all Windows versions.
 *
 */
#if defined(_M_IX86) || defined(_M_X64) || defined(_CORESYS)
typedef BOOL (WINAPI *PFN_GetCurrentPackageId)(UINT32*, BYTE*);
#endif

#if defined(_CORESYS)

// Copy the prototype for the dependent function from "kernel32packagecurrentext.h"

BOOL
WINAPI
IsGetCurrentPackageIdPresent(
    VOID
    );

static int __IsPackagedAppHelper(void)
{
    LONG retValue = APPMODEL_ERROR_NO_PACKAGE;
    UINT32 bufferLength = 0;

    PFN_GetCurrentPackageId pfn = NULL;


    // Ensure that GetCurrentPackageId is exported by the dll. This allows breaking changes to result
    // in a build break rather than a silent failure at runtime. Note that this function is implemented
    // in the import lib. That is, we do not have an implicit load dependency on the 
    // ext-ms-win-kerne32-package-current-l1-1-0.dll.
    if (IsGetCurrentPackageIdPresent())
    {
        // This dll need not be present on all systems
        HMODULE hPackageDll = LoadLibraryExW(L"ext-ms-win-kernel32-package-current-l1-1-0.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);

        if (hPackageDll)
        {
            if (pfn = (PFN_GetCurrentPackageId)(GetProcAddress(hPackageDll, "GetCurrentPackageId")))
            {
                retValue = pfn(&bufferLength, NULL);

                if (retValue == ERROR_INSUFFICIENT_BUFFER)
                {
                    // This is a packaged application
                    return 1;
                }
            }

            FreeLibrary(hPackageDll);
        }
    }

    // Either the app is not packaged or we cannot determine if the app is packaged
    // Return false by default.
    return 0;
}

#else

static int __IsPackagedAppHelper(void)
{
    LONG retValue = APPMODEL_ERROR_NO_PACKAGE;
    UINT32 bufferLength = 0;  

#if defined(_M_IX86) || defined(_M_X64)
    PFN_GetCurrentPackageId pfn = NULL;
      

    if (pfn = (PFN_GetCurrentPackageId)(GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "GetCurrentPackageId")))
    {
        retValue = pfn(&bufferLength, NULL);
    }
#else
    retValue = GetCurrentPackageId(&bufferLength, NULL);
#endif

    if (retValue == ERROR_INSUFFICIENT_BUFFER)
    {
        return 1;
    }

    /* If GetCurrentPackageId was not found, or it returned a different error,
       then this is NOT a Packaged app */
    return 0;
}
#endif

/*******************************************************************************
*__crtIsPackagedApp() - Check if the current app is a Packaged app
*
*Purpose:
*       Check if the current application was started through a package
*       This determines if the app is a Packaged app or not.
*
*       This function uses a new Windows 8 API, GetCurrentPackageId, to detect 
*       if the application is deployed via a package.
*
*Entry:
*       None
*
*Exit:
*       TRUE if Packaged app, FALSE if not.
*
*Exceptions:
*
*******************************************************************************/
_CRTIMP BOOL __cdecl __crtIsPackagedApp (
        void
        )
{
    static int isPackaged = -1; /* initialize to undefined state */

    /* If we've already made this check, just return the prev result */
    if (isPackaged < 0)
    {
        isPackaged = __IsPackagedAppHelper();
    }

    return (isPackaged > 0) ? TRUE : FALSE;
}


/*******************************************************************************
*__crtGetShowWindowMode() - Return the show window mode in a window app.
*
*Purpose:
*       Check if startup info, wShowWindow member contains additional information
*       and if so, returns wShowWindow.
*       Otherwise return SW_SHOWDEFAULT.
*
*Entry:
*       None
*
*Exit:
*       The current show window mode.
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP WORD __cdecl __crtGetShowWindowMode(
        void
        )
{
    STARTUPINFOW startupInfo;

    GetStartupInfoW( &startupInfo );
    return startupInfo.dwFlags & STARTF_USESHOWWINDOW
                        ? startupInfo.wShowWindow
                        : SW_SHOWDEFAULT;
}

/*******************************************************************************
*__crtSetUnhandledExceptionFilter() - Sets the unhandled exception fitler.
*
*Purpose:
*       Used to set the unhandled exception filter.
*
*Entry:
*       A pointer to a top-level exception filter function that will be called 
*       whenever the UnhandledExceptionFilter function gets control,
*
*Exit:
*       None.
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP void __cdecl __crtSetUnhandledExceptionFilter(
        LPTOP_LEVEL_EXCEPTION_FILTER exceptionFilter
        )
{
    /* Simply call the Win32 API. We don't need the previous exception filter, 
       so the return value is ignored.
     */
    SetUnhandledExceptionFilter(exceptionFilter);
}

#endif // _WIN32_WCE

/*******************************************************************************
*__crtTerminateProcess() - Terminates the current process.
*
*Purpose:
*       Terminates the current process.
*       This function is not needed for ARM and Win8, which call __fastfail() instead.
*Entry:
*       The exit code to be used by the process and threads terminated
*
*Exit:
*       None
*
*Exceptions:
*
*******************************************************************************/
#if defined(_M_IX86) || defined(_M_X64)
_CRTIMP void __cdecl __crtTerminateProcess (
        UINT uExitCode
        )
{
    /* Terminate the current process - the return code is currently unusable in 
      the CRT, so we ignore it. */
    TerminateProcess(GetCurrentProcess(), uExitCode);
}
#endif /* defined(_M_IX86) || defined(_M_X64) */

/*******************************************************************************
*__crtUnhandledException() - Raise an exception that will override all other exception filters.
*
*Purpose:
*       Raise an exception that will override all other exception filters.
*       In most cases, if the process is not being debugged, the function 
*       displays an error message box (Watson box). 
*
*       This function is not needed for ARM and Win8, which call __fastfail() instead.
*
*Entry:
*       pExceptionRecord: A pointer to an EXCEPTION_RECORD structure that contains 
*       a machine-independent description of the exception
*
*Exit:
*       Returns EXCEPTION_CONTINUE_SEARCH or EXCEPTION_EXECUTE_HANDLER 
*       if SEM_NOGPFAULTERRORBOX flag was specified in a previous call to SetErrorMode.
*
*Exceptions:
*
*******************************************************************************/
#if defined(_M_IX86) || defined(_M_X64)
_CRTIMP LONG __cdecl __crtUnhandledException (
        EXCEPTION_POINTERS *exceptionInfo
        )
{
//CE doesn't support Set/UnhandledException apis
#ifndef _WIN32_WCE 
    /* specifies default handling within UnhandledExceptionFilter */
    SetUnhandledExceptionFilter(NULL);

    /* invoke default exception filter */
    return UnhandledExceptionFilter(exceptionInfo);
#else
    return 0; 
#endif 
}
#endif /* defined(_M_IX86) || defined(_M_X64) */

/*******************************************************************************
*__crtCapturePreviousContext() - Capture the context record of the frame "preceding" the
      caller of this function.
*
*     Only available for x64.
*
*Exit:
*       pContextRecord have the context information of the frame preceding the 
*       caller.
*
*Exceptions:
*
*******************************************************************************/
#if defined(_M_X64)
_CRTIMP void __cdecl __crtCapturePreviousContext (
        CONTEXT *pContextRecord
        )
{
    ULONG64 ControlPc;
    ULONG64 EstablisherFrame;
    ULONG64 ImageBase;
    PRUNTIME_FUNCTION FunctionEntry;
    PVOID HandlerData;
    INT frames;

    RtlCaptureContext(pContextRecord);

    ControlPc = pContextRecord->Rip;

    /* Unwind "twice" to get the contxt of the caller to the "previous" caller. */
    for (frames = 0; frames < 2; ++frames)   
    {
        FunctionEntry = RtlLookupFunctionEntry(ControlPc, &ImageBase, NULL);

        if (FunctionEntry != NULL) {
            RtlVirtualUnwind(0, /* UNW_FLAG_NHANDLER */
                             ImageBase,
                             ControlPc,
                             FunctionEntry,
                             pContextRecord,
                             &HandlerData,
                             &EstablisherFrame,
                             NULL);
        } else {
            break;
        }
    }
}
#endif /* defined(_M_X64)*/

/*******************************************************************************
*__crtCaptureCurrentContext() - Capture the context record of the caller of this function.
*
*     Only available for x64.
*
*Exit:
*       pContextRecord have the context information of the  caller.
*
*Exceptions:
*
*******************************************************************************/
#if defined(_M_X64)
_CRTIMP void __cdecl __crtCaptureCurrentContext (
        CONTEXT *pContextRecord
        )
{

    ULONG64 ControlPc;
    ULONG64 EstablisherFrame;
    ULONG64 ImageBase;
    PRUNTIME_FUNCTION FunctionEntry;
    PVOID HandlerData;

    RtlCaptureContext(pContextRecord);

    ControlPc = pContextRecord->Rip;
    FunctionEntry = RtlLookupFunctionEntry(ControlPc, &ImageBase, NULL);

    if (FunctionEntry != NULL) {
        RtlVirtualUnwind(0, /* UNW_FLAG_NHANDLER */
                            ImageBase,
                            ControlPc,
                            FunctionEntry,
                            pContextRecord,
                            &HandlerData,
                            &EstablisherFrame,
                            NULL);
    }
}
#endif /* defined(_M_X64)*/

/*******************************************************************************
* Wrappers for Win32 thread/fiber-level storage APIs.
*
*******************************************************************************/

#ifndef _WIN32_WCE
DWORD __crtFlsAlloc(
  PFLS_CALLBACK_FUNCTION lpCallback)
{
    return FlsAlloc(lpCallback);
}

BOOL __crtFlsFree(
  DWORD dwFlsIndex)
{
    return FlsFree(dwFlsIndex);
}

PVOID __crtFlsGetValue(
  DWORD dwFlsIndex)
{
    return FlsGetValue(dwFlsIndex);
}

BOOL __crtFlsSetValue(
  DWORD dwFlsIndex,
  PVOID lpFlsData)
{
    return FlsSetValue(dwFlsIndex, lpFlsData);
}
#endif
