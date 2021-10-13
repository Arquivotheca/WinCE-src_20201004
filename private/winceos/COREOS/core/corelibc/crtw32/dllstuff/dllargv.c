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
*dllargv.c - Dummy _setargv() routine for use with C Run-Time as a DLL (CRTDLL)
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This object goes into CRTDLL.LIB, which is linked with user programs
*       to use CRTDLL.DLL for C run-time library functions.  If the user
*       program links explicitly with SETARGV.OBJ, this object will not be
*       linked in, and the _setargv() that does get called with set the flag
*       that will enable wildcard expansion.  If SETARGV.OBJ is not linked
*       into the EXE, this object will get called by the CRT start-up stub
*       and the flag to enable wildcard expansion will not be set.
*
*Revision History:
*       10-19-92  SKS   Initial version
*       04-06-93  SKS   Replace _CRTAPI* with _cdecl
*       11-24-93  CFW   Wide argv.
*       03-27-01  PML   Now return an int (vs7#231220)
*
*******************************************************************************/

#ifndef _POSIX_

#if defined(CRTDLL) || defined(MRTDLL)

#ifdef CRTDLL
#undef CRTDLL
#endif

#ifdef MRTDLL
#undef MRTDLL
#endif

#include <cruntime.h>
#include <internal.h>

/***
*_setargv - dummy version for CRTDLL.DLL model only
*
*Purpose:
*       This routine gets called by the C Run-Time start-up code in CRTEXE.C
*       which gets linked into an EXE file linked with CRTDLL.LIB.  It does
*       nothing, but if the user links the EXE with SETARGV.OBJ, this routine
*       will not get called but instead __setargv() will be called.  (In the
*       CRTDLL model, it will set the variable that is passed to _GetMainArgs
*       and enable wildcard expansion in the command line arguments.)
*
*Entry:
*
*Exit:
*       Always return 0 (full version in DLL code returns -1 on error)
*
*Exceptions:
*
*******************************************************************************/

#ifdef WPRFLAG
int __CRTDECL _wsetargv ( void )
#else
int __CRTDECL _setargv ( void )
#endif
{
        return 0;
}

#endif  /* CRTDLL */

#endif  /* !_POSIX_ */
