//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <corecrt.h>
extern _PVFV __xi_a[], __xi_z[];    /* C initializers */
extern _PVFV __xc_a[], __xc_z[];    /* C++ initializers */
extern _PVFV __xp_a[], __xp_z[];    /* C pre-terminators */
extern _PVFV __xt_a[], __xt_z[];    /* C terminators */

_PVFV *__onexitbegin;
_PVFV *__onexitend;

static void _initterm(_PVFV *, _PVFV *);

void _cinit (void) {
    _initterm( __xi_a, __xi_z );
    _initterm( __xc_a, __xc_z );
}

char _exitflag;

static void doexit (int code, int quick, int retcaller) {
    /* save callable exit flag (for use by terminators) */
    _exitflag = (char) retcaller;  /* 0 = term, !0 = callable exit */
    if (!quick) {
        if (__onexitbegin) {
            _PVFV * pfend = __onexitend;
            while (--pfend >= __onexitbegin)
                if (*pfend)
                    (**pfend)();

            LocalFree (__onexitbegin);
            __onexitbegin = __onexitend = NULL;         
        }
        _initterm(__xp_a, __xp_z);
    }
    _initterm(__xt_a, __xt_z);
    if (!retcaller)
        TerminateProcess(GetCurrentProcess(),code);
}

static void _initterm (_PVFV * pfbegin, _PVFV * pfend) {
    while ( pfbegin < pfend ) {
        if (*pfbegin)
            (**pfbegin)();
        ++pfbegin;
    }
}

void exit (int code) {
        doexit(code, 0, 0); /* full term, kill process */
}

void _exit (int code) {
        doexit(code, 1, 0); /* quick term, kill process */
}

void _cexit(void) {
        doexit(0, 0, 1);    /* full term, return to caller */
}

void _c_exit (void) {
        doexit(0, 1, 1);    /* quick term, return to caller */
}


