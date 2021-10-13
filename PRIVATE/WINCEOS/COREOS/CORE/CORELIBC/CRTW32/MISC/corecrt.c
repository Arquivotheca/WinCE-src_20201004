//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifdef _CRTBLD
#undef COREDLL
#endif

#include <corecrtstorage.h>

crtGlob_t*
GetCRTStorage(
    void
    )
{
    crtGlob_t *pcrtg;

    pcrtg = TlsGetValue(TLSSLOT_RUNTIME);
    if ( ((DWORD)pcrtg) < 0x10000 )
        {
        CALLBACKINFO cbi;
        cbi.hProc = GetOwnerProcess();
        if ( cbi.hProc != (HANDLE)GetCurrentProcessId() )
            {
            cbi.pfn = (FARPROC)LocalAlloc;
            cbi.pvArg0 = (LPVOID)LPTR;
            pcrtg = (crtGlob_t *)PerformCallBack4(&cbi, (void*)sizeof(crtGlob_t), 0, 0);
            }
        else
            {
            pcrtg = LocalAlloc(LPTR,sizeof(crtGlob_t));
            }

        if ( pcrtg )
            {
            pcrtg = MapPtrToProcess(pcrtg,GetOwnerProcess());
            pcrtg -> dwFlags = (DWORD)TlsGetValue(TLSSLOT_RUNTIME);
            pcrtg->nexttoken = 0;
            pcrtg->wnexttoken = (wchar_t) 0;
            pcrtg->dwHimcDefault = pcrtg->dwHwndDefaultIme = 0;
            TlsSetValue(TLSSLOT_RUNTIME,pcrtg);
            pcrtg->rand = 1;
            }
        }
    return pcrtg;
}



crtGlob_t*
GetCRTStorageEx(
    BOOL    bCreateIfNecessary
    )
{
    crtGlob_t*pcrtg;
    pcrtg = TlsGetValue(TLSSLOT_RUNTIME);
    if ( ( ((DWORD)pcrtg) < 0x10000 ) && bCreateIfNecessary )
        {
        CALLBACKINFO cbi;
        cbi.hProc = GetOwnerProcess();
        if ( cbi.hProc != (HANDLE)GetCurrentProcessId() )
            {
            cbi.pfn = (FARPROC)LocalAlloc;
            cbi.pvArg0 = (LPVOID)LPTR;
            pcrtg = (crtGlob_t *)PerformCallBack4(&cbi, (void*)sizeof(crtGlob_t), 0, 0);
            }
        else
            {
            pcrtg = LocalAlloc(LPTR,sizeof(crtGlob_t));
            }

        if ( pcrtg )
            {
            pcrtg = MapPtrToProcess(pcrtg,GetOwnerProcess());
            pcrtg -> dwFlags = (DWORD)TlsGetValue(TLSSLOT_RUNTIME);
            pcrtg->nexttoken = 0;
            pcrtg->wnexttoken = (wchar_t) 0;
            pcrtg->dwHimcDefault = pcrtg->dwHwndDefaultIme = 0;
            TlsSetValue(TLSSLOT_RUNTIME,pcrtg);
            pcrtg->rand = 1;
            }
        }
    else
        {
        if ( ((DWORD)pcrtg) < 0x10000 )
            {
            pcrtg = NULL;
            }
        }
    return pcrtg;
}



DWORD
GetCRTFlags(
    void
    )
{
    crtGlob_t *pcrtg;
    pcrtg = TlsGetValue(TLSSLOT_RUNTIME);
    if ( ((DWORD)pcrtg) < 0x10000 )
        {
        return (DWORD)pcrtg;
        }
    return pcrtg -> dwFlags;
}



void
SetCRTFlag(
    DWORD   dwFlag
    )
{
    crtGlob_t *pcrtg;

    pcrtg = TlsGetValue(TLSSLOT_RUNTIME);
    if ( ((DWORD)pcrtg) < 0x10000 )
        {
        TlsSetValue(TLSSLOT_RUNTIME,(void*)(((DWORD)pcrtg)|dwFlag));
        }
    else
        {
        pcrtg -> dwFlags |= dwFlag;
        }

    return;
}



void
ClearCRTFlag(
    DWORD   dwFlag
    )
{
    crtGlob_t *pcrtg;

    pcrtg = TlsGetValue(TLSSLOT_RUNTIME);
    if ( ((DWORD)pcrtg) < 0x10000 )
        {
        TlsSetValue(TLSSLOT_RUNTIME, (void*)(((DWORD)pcrtg) & ~dwFlag));
        }
    else
        {
        pcrtg -> dwFlags &= ~dwFlag;
        }

    return;
}


