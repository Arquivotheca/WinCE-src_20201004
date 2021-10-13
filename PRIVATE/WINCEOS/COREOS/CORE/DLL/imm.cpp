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

#include <..\imm\imm.hpp>

void
ImeInfoEntry::
LoadIme(
    void
    )
{
#ifdef KCOREDLL

    m_bImeLoadFail = true;

#else // KCOREDLL

    WCHAR   szImeFile[IM_FILE_SIZE];



    EnterCriticalSection(&s_csLoading);
    if ( m_bImeLoadSuccess ||
         m_bImeLoadFail )
        {
        goto leave;
        }

//  GetSystemPathName(wszImeFile, piiex->wszImeFile, IM_FILE_SIZE);
    wcsncpy(szImeFile, m_szImeFileName, IM_FILE_SIZE - 1);
    szImeFile[IM_FILE_SIZE-1] = L'\0';
    DEBUGMSG(DBGIMM,
        (TEXT("LoadIme:  szImeFile %s.\r\n"),
            szImeFile));

    m_hInst = LoadLibraryW(szImeFile);
    if ( !m_hInst )
        {
        ERRORMSG(1,
            (TEXT("LoadIme: Could not LoadLibrary ime %s.\r\n"),
                szImeFile));
//      ASSERT(0);
        goto leave_fail;
        }

#define GET_IMEPROC(name_m, type_m) \
    do  {               \
        if ( !(m_pfn##name_m = (##type_m)GetProcAddress(m_hInst, TEXT(#name_m))) ) \
            {           \
            goto leave_fail; \
            }           \
        } while (0)

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
    m_bImeLoadSuccess = true;

leave:
    LeaveCriticalSection(&s_csLoading);
    return;



leave_fail:
    ERRORMSG(1,
        (TEXT("LoadIme: failed to load %s.\r\n"),
            szImeFile));

    m_bImeLoadFail = true;
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
        }
    goto leave;

#endif // KCOREDLL
}
