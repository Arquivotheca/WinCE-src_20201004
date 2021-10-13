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
/*++


Module Name:

    wsocknt.h

Abstract:

    Contains various definitions, macros, prototypes for modules derived
    from NT

--*/

//
// redefinitions to maintain common (as possible) source code
//

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef OPTIONAL
#define OPTIONAL
#endif

#ifndef BOOLEAN
#define BOOLEAN BOOL
#endif

#ifndef CHAR
#define CHAR char
#endif

#ifndef UCHAR
#define UCHAR BYTE
#endif

#ifndef USHORT
#define USHORT WORD
#endif

#ifndef PCHAR
#define PCHAR LPBYTE
#endif

#ifndef PVOID
#define PVOID LPVOID
#endif

#ifndef LPCHAR
#define LPCHAR CHAR FAR*
#endif

#ifndef PUCHAR
#define PUCHAR LPBYTE
#endif

#ifndef PULONG
#define PULONG LPDWORD
#endif

#ifndef NTSTATUS
#define NTSTATUS UINT
#endif

#ifndef NT_SUCCESS
#define NT_SUCCESS(s) (s == 0)
#endif

#ifndef NO_ERROR
#define NO_ERROR 0
#endif

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS                          ((NTSTATUS)0x00000000L)
#endif

#ifndef RtlCopyMemory
#define RtlCopyMemory memcpy
#endif

#ifndef RtlZeroMemory
#define RtlZeroMemory(p, l) memset(((LPVOID)(p)), 0, (l))
#endif

#define WS_ENTER(a, b, c, d, e) /* NOTHING */
#define WS_EXIT(x, y, z)        /* NOTHING */
//#define WS_PRINT                DLL_PRINT
#define WS_ASSERT(x)            DLL_ASSERT(x)

//
// map enterAPI to SockEnterApi. They take the same parameters except enterAPI
// returns a pointer to the per-thread bucket o' variables (always use pThread)
//

#define SockEnterApi(x, y, z)   enterAPI(x, y, z, &pThread)

//
// berkeley memory functions (?)
//

#define bcopy(s, d, c)  memcpy((u_char *)(d), (u_char *)(s), (c))
#define bzero(d, l)     memset((d), '\0', (l))
#define bcmp(s1, s2, l) memcmp((s1), (s2), (l))

//
// Resolver subroutines.
//

int
dn_expand(
    IN  unsigned char *msg,
    IN  unsigned char *eomorig,
    IN  unsigned char *comp_dn,
    OUT unsigned char *exp_dn,
    IN  int            length
    );

static
int
dn_find(
    unsigned char  *exp_dn,
    unsigned char  *msg,
    unsigned char **dnptrs,
    unsigned char **lastdnptr
    );

int
dn_skipname(
    unsigned char *comp_dn,
    unsigned char *eom
    );

void
fp_query(
    char *msg,
    FILE *file
    );

void
p_query(
    char *msg
    );

extern
void
putshort(
    u_short s,
    u_char *msgp
    );

void
putlong(
    u_long l,
    u_char *msgp
    );

void
_res_close(
    void
    );

int
sendv(
    SOCKET         s,           /* socket descriptor */
    struct iovec  *iov,         /* array of vectors */
    int            iovcnt       /* size of array */
    );

int
strcasecmp(
    char *s1,
    char *s2
    );

int
strncasecmp(
    char *s1,
    char *s2,
    int   n
    );

//
// async etc. functions
//

BOOLEAN
SockCheckAndInitAsyncThread (
    VOID
    );

VOID
SockAcquireGlobalLockExclusive (
    VOID
    );

VOID
SockReleaseGlobalLock (
    VOID
    );

BOOL
SockWaitForSingleObject (
    IN HANDLE Handle,
    IN SOCKET SocketHandle,
    IN DWORD BlockingHookUsage,
    IN DWORD TimeoutUsage
    );

VOID
SockProcessAsyncGetHost (
    IN DWORD TaskHandle,
    IN DWORD OpCode,
    IN HWND hWnd,
    IN unsigned int wMsg,
    IN char FAR *Filter,
    IN int Length,
    IN int Type,
    IN char FAR *Buffer,
    IN int BufferLength
    );
