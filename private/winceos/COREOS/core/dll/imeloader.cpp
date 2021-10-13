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

#include <..\imm\imm.hpp>

void
ImeInfoEntry::
LoadIme(
    void
    )
{
#ifdef KCOREDLL

    m_bImeLoadFail = TRUE;

#else // KCOREDLL

    WCHAR   szImeFile[IM_FILE_SIZE];
    WCHAR   szLibPath[MAX_PATH];

    DWORD   dwPathLen = 0;


    if ( m_bImeLoadSuccess ||
         m_bImeLoadFail )
        {
        goto leave;
        }

    szImeFile[0] = L'\0';
    szLibPath[0] = L'\0';

    EnterCriticalSection(&s_csLoading);

//  GetSystemPathName(wszImeFile, piiex->wszImeFile, IM_FILE_SIZE);
    VERIFY (SUCCEEDED (StringCchCopyW (szImeFile, IM_FILE_SIZE, m_szImeFileName)));
    DEBUGMSG(DBGIMM,
        (TEXT("LoadIme:  szImeFile %s.\r\n"),
            szImeFile));

    m_hInst = LoadLibraryW(szImeFile);
    if ( !m_hInst )
        {
        ERRORMSG(1,
            (TEXT("LoadIme: Could not LoadLibrary ime %s. Last Error: %u\r\n"),
                szImeFile, GetLastError()));
//      ASSERT(0);
        goto leave_fail;
        }

    dwPathLen = GetModuleFileName(m_hInst, szLibPath, MAX_PATH);
    if (dwPathLen == 0 || 
        wcsnicmp(szImeFile, szLibPath, MAX_PATH))
    {
        ERRORMSG(1,
            (TEXT("LoadIme: could not load in expected ime %s.\r\n"),
                szLibPath));

        FreeLibrary(m_hInst);
        m_hInst = NULL;
        SetLastError(ERROR_INVALID_DLL);
        goto leave_fail;
    }

#define GET_IMEPROC(name_m, type_m) \
    do  {               \
        if ( NULL == (m_pfn##name_m = (##type_m)GetProcAddress(m_hInst, TEXT(#name_m))) ) \
        {           \
            goto leave_fail; \
        }           \
    } while (0,0)

//  GET_IMEPROC(ImeInquireW,            PFNINQUIREW);
    m_pfnImeInquireW = (PFNINQUIREW)GetProcAddress(m_hInst, TEXT("ImeInquire"));
//  GET_IMEPROC(ImeConversionListW,     PFNCONVLISTW);
    m_pfnImeConversionListW = (PFNCONVLISTW)GetProcAddress(m_hInst, TEXT("ImeConversionList"));
//  GET_IMEPROC(ImeRegisterWordW,       PFNREGWORDW);
    m_pfnImeRegisterWordW = (PFNREGWORDW)GetProcAddress(m_hInst, TEXT("ImeRegisterWord"));
//  GET_IMEPROC(ImeUnregisterWordW,     PFNUNREGWORDW);
    m_pfnImeUnregisterWordW = (PFNUNREGWORDW)GetProcAddress(m_hInst, TEXT("ImeUnregisterWord"));
//  GET_IMEPROC(ImeGetRegisterWordStyleW, PFNGETREGWORDSTYW);
    m_pfnImeGetRegisterWordStyleW = (PFNGETREGWORDSTYW)GetProcAddress(m_hInst, TEXT("ImeGetRegisterWordStyle"));
//  GET_IMEPROC(ImeEnumRegisterWordW,   PFNENUMREGWORDW);
    m_pfnImeEnumRegisterWordW = (PFNENUMREGWORDW)GetProcAddress(m_hInst, TEXT("ImeEnumRegisterWord"));
    GET_IMEPROC(ImeConfigure,           PFNCONFIGURE);
    GET_IMEPROC(ImeDestroy,             PFNDESTROY);
//  GET_IMEPROC(ImeEscapeW,             PFNESCAPEW);
    m_pfnImeEscapeW = (PFNESCAPEW)GetProcAddress(m_hInst, TEXT("ImeEscape"));
    GET_IMEPROC(ImeProcessKey,          PFNPROCESSKEY);
    GET_IMEPROC(ImeSelect,              PFNSELECT);
    GET_IMEPROC(ImeSetActiveContext,    PFNSETACTIVEC);
    GET_IMEPROC(ImeToAsciiEx,           PFNTOASCEX);
    GET_IMEPROC(NotifyIME,              PFNNOTIFY);
    GET_IMEPROC(ImeSetCompositionString, PFNSETCOMPSTR);

    // 4.0 IMEs don't have this entry.  Could be NULL.
    m_pfnImeGetImeMenuItems = (PFNGETIMEMENUITEMS)GetProcAddress(m_hInst, TEXT("ImeGetImeMenuItems"));

#undef GET_IMEPROC

    if ( !InquireImeW() )
        {
        ERRORMSG(1,
            (TEXT("LoadIme: Could not InquireImeW failed.\r\n")));
        goto leave_fail;
        }


    DEBUGMSG(DBGIMM,
        (TEXT("LoadIme:  %s loaded.\r\n"),
            szImeFile));

    m_piieLoaded = this;
    m_bImeLoadSuccess = TRUE;

    LeaveCriticalSection(&s_csLoading);

leave:

    return;



leave_fail:
    LeaveCriticalSection(&s_csLoading);

    ERRORMSG(1,
        (TEXT("LoadIme: failed to load %s.\r\n"),
            szImeFile));

    m_bImeLoadFail = TRUE;
    UnloadIme();
    goto leave;

#endif // KCOREDLL
}

void
ImeInfoEntry::
UnloadIme(
    void
    )
{
#ifdef KCOREDLL

#else // KCOREDLL

    EnterCriticalSection(&s_csLoading);

    if ( m_pUserThreadHead != NULL )
        {
        goto leave;
 // IME cannot unload. Because there are still some user threads.
        }


    if ( m_hInst )
        {
        FreeLibrary(m_hInst);
        m_hInst = 0;
        m_pfnImeInquireW = 0;
        m_pfnImeConversionListW = 0;
        m_pfnImeRegisterWordW = 0;
        m_pfnImeUnregisterWordW = 0;
        m_pfnImeGetRegisterWordStyleW = 0;
        m_pfnImeEnumRegisterWordW = 0;
        m_pfnImeConfigure = 0;
        m_pfnImeDestroy = 0;
        m_pfnImeEscapeW = 0;
        m_pfnImeProcessKey = 0;
        m_pfnImeSelect = 0;
        m_pfnImeSetActiveContext = 0;
        m_pfnImeToAsciiEx = 0;
        m_pfnNotifyIME = 0;
        m_pfnImeSetCompositionString = 0;
        m_pfnImeGetImeMenuItems = 0;

        m_piieLoaded = NULL;
        m_bImeLoadSuccess = FALSE;
        }

leave:
    LeaveCriticalSection(&s_csLoading);

#endif // KCOREDLL
}
