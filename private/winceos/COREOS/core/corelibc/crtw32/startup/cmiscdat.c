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
*cmiscdat.c - miscellaneous C run-time data
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Includes floating point conversion table (for C float output).
*
*       When floating point I/O conversions are done, but no floating-point
*       variables or expressions are used in the C program, we use the
*       _cfltcvt_tab[] to map these cases to the _fptrap entry point,
*       which prints "floating point not loaded" and dies.
*
*       This table is initialized to six copies of _fptrap by default.
*       If floating-point is linked in (_fltused), these table entries
*       are reset (see input.c, output.c, fltused.asm, and fltuseda.asm).
*
*Revision History:
*       06-29-89  PHG   module created, based on asm version
*       04-06-90  GJF   Added #include <cruntime.h>. Also, fixed the copyright
*                       and cleaned up the formatting a bit.
*       07-31-90  SBM   Updated comments slightly
*       08-29-90  SBM   Added #include <internal.h> and <fltintrn.h>,
*                       removed _fptrap() prototype
*       04-19-93  SKS   Remove obsolete variable _sigintoff
*       11-30-95  SKS   Removed obsolete comments about 16-bit functionality.
*       10-31-04  MSL   Floating point conversion with locales
*       05-25-05  PML   Encode func-ptrs, and no longer needed by CRT DLL
*                       (vsw#191114)
*
*******************************************************************************/

#if !defined(CRTDLL)

#include <cruntime.h>
#include <internal.h>
#include <stdlib.h>
#include <fltintrn.h>
#include <mbdata.h>
#include <setlocal.h>

#ifndef _WCE_BOOTCRT
/*-
 *  ... table of (model-dependent) code pointers ...
 *
 *  Entries all point to _fptrap by default,
 *  but are changed to point to the appropriate
 *  routine if the _fltused initializer (_cfltcvt_init)
 *  is linked in.
 *
 *  if the _fltused modules are linked in, then the
 *  _cfltcvt_init initializer sets the entries of
 *  _cfltcvt_tab.
-*/

PFV _cfltcvt_tab[10] = {
    _fptrap,    /*  _cfltcvt */
    _fptrap,    /*  _cropzeros */
    _fptrap,    /*  _fassign */
    _fptrap,    /*  _forcdecpt */
    _fptrap,    /*  _positive */
    _fptrap,    /*  _cldcvt */
    _fptrap,    /*  _cfltcvt_l */
    _fptrap,    /*  _fassign_l */
    _fptrap,    /*  _cropzeros_l */
    _fptrap     /*  _forcdecpt_l */
};

void __cdecl _initp_misc_cfltcvt_tab()
{
    int i;
    for (i = 0; i < _countof(_cfltcvt_tab); ++i)
    {
        _cfltcvt_tab[i] = (PFV) EncodePointer(_cfltcvt_tab[i]);
    }
}
#endif

#ifdef _WIN32_WCE
threadmbcinfo __initialmbcinfo =
{
    0,                       /* refcount */
    _CLOCALECP,              /* mbcodepage: _MB_CP_ANSI */
    0,                       /* ismbcodepage */
    { 0, 0, 0, 0, 0, 0 },    /* mbulinfo[6] */
    {                        /* mbctype[257] */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00 /* rest is zero */
    },
    {     /* mbcasemap[256] */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73,
    0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b,
    0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00 /* rest is zero */
    },
    NULL /* mblocalename */
};

/* global pointer to the current per-thread mbc information structure. */
pthreadmbcinfo __ptmbcinfo = &__initialmbcinfo;

/* Buffer pointers for stdout and stderr */
void *_stdbuf[2] = { NULL, NULL};

#endif // _WIN32_WCE
#endif /* ndef CRTDLL */
