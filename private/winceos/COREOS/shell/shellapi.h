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
/*---------------------------------------------------------------------------*\
 *
 *  module: shellapi.h
 *
\*---------------------------------------------------------------------------*/
#ifndef __SHELLAPI_H__
#define __SHELLAPI_H__

#ifdef __cplusplus
extern "C" {
#endif

#define EOF	(-1)

int      _crtinit(void);
int       getCHR(void);
TCHAR   * Gets(TCHAR *s, int cch);
int       Puts(const TCHAR *s);

// Sachin
// Function prototypes for temp functions
#if 1
int UU_ropen(const WCHAR *name, int mode);
int UU_rclose(int fd); 
int UU_rread(int fd, void *buf, int cnt); 
int UU_rwrite(int fd, const void *buf, int cnt);
#endif


#ifdef __cplusplus
}
#endif

#endif
