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

#ifndef _CEFS_H
#define _CEFS_H

#include <windows.h>
#include <tchar.h>
#include <types.h>
#include <ethdbg.h>
#include <halether.h>
#include <ppfs.h>

// KITL's definition of _WIN32_FIND_DATAW
typedef struct _WIN32_FIND_DATAW_KITL {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    WCHAR cFileName[ MAX_PATH ];
    WCHAR cAlternateFileName[ 14 ];
} WIN32_FIND_DATAW_KITL, *PWIN32_FIND_DATAW_KITL, *LPWIN32_FIND_DATAW_KITL;


int ropen(const WCHAR *name, int mode);
int rread(int fd, char *buf, int cnt);
int rreadseek(int fd, char *buf, int cnt, int off);
int rlseek(int fd, int off, int mode);
int rclose(int fd);
int rwrite(int fd, char *buf, int cnt);
BOOL rcreatedir(const WCHAR* pwsPathName);
BOOL rremovedir(const WCHAR* pwsPathName);
int rfindfirst  (const WCHAR* pwsPathName, PWIN32_FIND_DATAW pFindData);
BOOL rfindnext (SearchState *pSearch, PWIN32_FIND_DATAW pfd);
BOOL rfindclose (int fs);
BOOL rdelete (const WCHAR* pwsPathName);
int rgetfileattrib (const WCHAR *);
BOOL rsetfileattrib (const WCHAR *, int);
BOOL rmove (const WCHAR *, const WCHAR *);
BOOL rdeleteandrename (const WCHAR *, const WCHAR *);
BOOL rgetdiskfree (const WCHAR*, DWORD*, DWORD*, DWORD*, DWORD*);
BOOL rgettime (int, PFILETIME, PFILETIME, PFILETIME);
BOOL rsettime (int, PFILETIME, PFILETIME, PFILETIME);
BOOL rsetendoffile (int);

int rRegOpen(DWORD hKey, CHAR *szName, LPDWORD lphKey); 
int rRegClose(DWORD hKey);
int rRegGet(DWORD hKey, CHAR *szName, LPDWORD lpdwType, LPBYTE lpbData, LPDWORD lpdwSize);
int rRegEnum(DWORD hKey, DWORD dwIndex, LPBYTE lpbData, LPDWORD lpdwSize); 

void PpfsWriteDebugStringW(unsigned short *name);
void KUnicodeToAscii(LPCHAR chptr, LPCWSTR wchptr, int maxlen);
unsigned int strlenW(const wchar_t *wcs);
BOOL  PPSHConnect(void); 

BOOL _cdecl EdbgSend(UCHAR Id, UCHAR *pUserData, DWORD dwUserDataLen);
BOOL  _cdecl EdbgRecv(UCHAR Id, UCHAR *pRecvBuf, DWORD *pdwLen, DWORD Timeout);

extern HANDLE g_pcsCefs;

#endif /* _CEFS_H */

