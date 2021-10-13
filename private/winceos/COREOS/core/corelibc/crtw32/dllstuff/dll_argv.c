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
*dll_argv.c - __setargv() routine for use with C Run-Time as a DLL (CRTDLL)
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This object is part of the start-up code for EXE's linked with
*       CRTDLL.LIB/MSVCRT.LIB.  This object will be linked into the user
*       EXE if and only if the user explicitly links with SETARGV.OBJ.
*       The code in this object sets the flag that is passed to the
*       C Run-Time DLL to enable wildcard expansion of the argv[] vector.
*
*Revision History:
*       03-04-94  SKS   Initial version
*       03-27-01  PML   Now return an int (vs7#231220)
*
*******************************************************************************/

#ifndef _POSIX_

#if defined(CRTDLL) ||defined(MRTDLL)

#ifdef CRTDLL
#undef CRTDLL
#endif

#ifdef MRTDLL
#undef MRTDLL
#endif

#include <cruntime.h>
#include <internal.h>

/***
*__setargv - dummy version (for wildcard expansion) for CRTDLL.DLL model only
*
*Purpose:
*       If the EXE that is linked with CRTDLL.LIB is linked explicitly with
*       SETARGV.OBJ, the call to _setargv() in the C Run-Time start-up code
*       (above) will call this routine, instead of calling a dummy version of
*       _setargv() which will do nothing.  This will set to one the static
*       variable which is passed to __getmainargs(), thus enabling wildcard
*       expansion of the command line arguments.
*
*       In the statically-linked C Run-Time models, _setargv() and __setargv()
*       are the actual routines that do the work, but this code exists in
*       CRTDLL.DLL and so some tricks have to be played to make the same
*       SETARGV.OBJ work for EXE's linked with both LIBC.LIB and CRTDLL.LIB.
*
*Entry:
*       The static variable _dowildcard is zero (presumably).
*
*Exit:
*       The static variable _dowildcard is set to one, meaning that the
*       routine __getmainargs() in CRTDLL.DLL *will* do wildcard expansion on
*       the command line arguments.  (The default behavior is that it won't.)
*       Always return 0 (full version in DLL code returns -1 on error)
*
*Exceptions:
*
*******************************************************************************/

extern int _dowildcard; /* should be in <internal.h> */

#ifdef WPRFLAG
int __CRTDECL __wsetargv ( void )
#else
int __CRTDECL __setargv ( void )
#endif
{
        _dowildcard = 1;
        return 0;
}

#endif  /* CRTDLL */

#endif  /* !_POSIX_ */
