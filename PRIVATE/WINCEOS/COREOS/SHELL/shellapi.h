//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
int       Puts(TCHAR *s);

// Sachin
// Function prototypes for temp functions
#if 1
int UU_ropen(WCHAR *name, int mode);
int UU_rclose(int fd); 
int UU_rread(int fd, PBYTE buf, int cnt); 
int UU_rwrite(int fd, PBYTE buf, int cnt);
#endif


#ifdef __cplusplus
}
#endif

#endif
