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

// Handle to the rendercode instance loaded in user space
// Note that rendercore is kept loaded for performance reasons.
static HMODULE s_hmodUserRenderCore = NULL;


HMODULE
WINAPI
InitializeUserRenderCore(
    __in HPROCESS hprocDest,
    __in_ecount(cbCallbacks) FARPROC* prgpfnCallbacks,
    UINT cbCallbacks
    )
{
    HMODULE hModUserRenderCore = NULL;

#ifdef KCOREDLL
    if (GetCurrentProcessId()!=(DWORD)hprocDest)
    {
        if ((g_UsrCoredllCallbackArray != NULL) &&
            g_UsrCoredllCallbackArray[USRCOREDLLCB_InitializeUserRenderCore])
        {
            CALLBACKINFO cbi;
            cbi.hProc = hprocDest;
            cbi.pfn = g_UsrCoredllCallbackArray[USRCOREDLLCB_InitializeUserRenderCore];
            cbi.pvArg0 = hprocDest;
            __try
            {
                hModUserRenderCore = (HMODULE)PerformCallBack(&cbi, prgpfnCallbacks, cbCallbacks);
                if (cbi.dwErrRtn) {
                    hModUserRenderCore = NULL;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
    }
#else
    HRESULT hr;
    HRESULT (*pfnRenderCoreInitialize)();
    BOOL fCleanUpNeeded = TRUE;

    // This needs to be in sync with RenderCore thunks code
    // See \private\winceos\COREOS\gwe\rcthunk\RenderCoreThunk.cpp
    enum USRRC_CB
    {
        USRRC_CB_CreateSceneFromXML,
        USRRC_CB_CreateSceneFromXMLResource,
        USRRC_CB_IRS_CreateElementTreeFromXML,
        USRRC_CB_IRS_CreateElementTreeFromXMLResource,
        USRRC_CB_IRS_GetElementByUniqueName,
        USRRC_CB_IRS_HitTest,
        USRRC_CB_IRS_SetSize,
        USRRC_CB_IRS_UpdateFromXML,
        USRRC_CB_IRS_UpdateFromXMLResource,
        USRRC_CB_IRS_ProcessWindowMessage,
        USRRC_CB_IRS_BeginStoryboard,
        USRRC_CB_IRS_StopStoryboard,
        USRRC_CB_IRS_SetStoryboardCallbackInfo,
        USRRC_CB_IRS_Release,

        USRRC_CB_IRE_CloneElementTree,
        USRRC_CB_IRE_GetElementByUniqueName,
        USRRC_CB_IRE_GetChildCount,
        USRRC_CB_IRE_GetChild,
        USRRC_CB_IRE_IndexOf,
        USRRC_CB_IRE_InsertChild,
        USRRC_CB_IRE_RemoveChild,
        USRRC_CB_IRE_BeginStoryboard,
        USRRC_CB_IRE_StopStoryboard,
        USRRC_CB_IRE_SetAttributeEx,
        USRRC_CB_IRE_GetAttributeEx,
        USRRC_CB_IRE_SetAttributeByIdEx,
        USRRC_CB_IRE_GetAttributeByIdEx,        
        USRRC_CB_IRE_Release,

        USRRC_CB_OpenPathAsStream,
        USRRC_CB_IS_Release,

        USRRC_CB_MAXCALLBACKS,
    };

    // See if RenderCore has been loaded in this process (user mode)
    if (NULL == s_hmodUserRenderCore)
    {
        // Load the rendercore library. This could happen more than once in
        // the worst case scenario; but that is OK. For most cases, the library
        // should already be loaded.
        hModUserRenderCore = LoadLibrary(TEXT("xgf.dll"));
        if (hModUserRenderCore == NULL)
        {
            goto Error;
        }

        if (NULL != InterlockedCompareExchangePointer((PVOID*) &s_hmodUserRenderCore, hModUserRenderCore, NULL))
        {
            // Another thread set it already.
            FreeLibrary(hModUserRenderCore);
        }
    }

    hModUserRenderCore = s_hmodUserRenderCore;

#define GetUserRenderCoreCallbackEntryByOrdinal(Index, Ordinal) \
        prgpfnCallbacks[Index]= (FARPROC)GetProcAddress(hModUserRenderCore, (LPCTSTR)Ordinal); \
        if (prgpfnCallbacks[Index] == NULL)                                                  \
        {                                                                                       \
            ERRORMSG(1, (TEXT("Could not find callback @") __TEXT(#Ordinal) __TEXT(".\r\n")));  \
            SetLastError(ERROR_PROC_NOT_FOUND); \
            goto Error;                         \
        }

    // Scene and IRenderScene APIs
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_CreateSceneFromXML, 20);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_CreateSceneFromXMLResource, 21);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRS_CreateElementTreeFromXML, 22);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRS_CreateElementTreeFromXMLResource, 23);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRS_GetElementByUniqueName, 24);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRS_HitTest, 25);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRS_SetSize, 26);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRS_UpdateFromXML, 27);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRS_UpdateFromXMLResource, 28);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRS_ProcessWindowMessage, 29);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRS_BeginStoryboard, 30);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRS_StopStoryboard, 31);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRS_SetStoryboardCallbackInfo, 32);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRS_Release, 33);
        
    // IRenderElement APIs
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRE_SetAttributeEx, 34);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRE_GetAttributeEx, 35);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRE_GetElementByUniqueName, 36);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRE_GetChildCount, 37);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRE_GetChild, 38);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRE_IndexOf, 39);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRE_InsertChild, 40);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRE_RemoveChild, 41);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRE_BeginStoryboard, 42);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRE_StopStoryboard, 43);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRE_Release, 44);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRE_CloneElementTree, 45);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRE_SetAttributeByIdEx, 46);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IRE_GetAttributeByIdEx, 47);

    // Helpers
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_OpenPathAsStream, 50);
    GetUserRenderCoreCallbackEntryByOrdinal(USRRC_CB_IS_Release, 51);

    // Finally call RenderCoreInitialize

    pfnRenderCoreInitialize = (HRESULT (*)())GetProcAddress(hModUserRenderCore, (LPCTSTR)1);
    if (pfnRenderCoreInitialize == NULL)
    {
        ERRORMSG(1, (TEXT("Could not find RenderCoreInitialize.\r\n")));
        SetLastError(ERROR_PROC_NOT_FOUND);
        goto Error;
    }

    hr = pfnRenderCoreInitialize();
    if (FAILED(hr))
    {
        ERRORMSG(1, (TEXT("Failed to initialize RenderCore.\r\n")));
        goto Error;
    }

    fCleanUpNeeded = FALSE;

Error:
    if (fCleanUpNeeded)
    {
        // Do not free the library, just return NULL on failure
        hModUserRenderCore = NULL;
    }
#endif

    return hModUserRenderCore;
}

void
WINAPI
UninitializeUserRenderCore(
    __in HPROCESS hprocDest,
    __in HMODULE hModUserRenderCore
    )
{
#ifdef KCOREDLL
    if (GetCurrentProcessId()!=(DWORD)hprocDest)
    {
        if ((g_UsrCoredllCallbackArray != NULL) &&
            g_UsrCoredllCallbackArray[USRCOREDLLCB_UninitializeUserRenderCore])
        {
            CALLBACKINFO cbi;
            cbi.hProc = hprocDest;
            cbi.pfn = g_UsrCoredllCallbackArray[USRCOREDLLCB_UninitializeUserRenderCore];
            cbi.pvArg0 = hprocDest;
            __try
            {
                PerformCallBack(&cbi, hModUserRenderCore);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
    }
#else
    if (hModUserRenderCore != NULL)
    {
        //ASSERT(hModUserRenderCore == s_hmodUserRenderCore);

        // Call RenderCoreUninitialize
        HRESULT (*pfnRenderCoreUninitialize)();

        pfnRenderCoreUninitialize = (HRESULT (*)())GetProcAddress(hModUserRenderCore, (LPCTSTR)2);
        if (pfnRenderCoreUninitialize == NULL)
        {
            ERRORMSG(1, (TEXT("Could not find RenderCoreUninitialize.\r\n")));
        }
        else
        {
            pfnRenderCoreUninitialize();
        }
    }
#endif
}

