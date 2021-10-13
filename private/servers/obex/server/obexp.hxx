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
#if ! defined (__obexp_HXX__)
#define __obexp_HXX__   1

#include <windows.h>
#include <winsock2.h>
#undef AF_IRDA
#include <af_irda.h>

#include <svsutil.hxx>
#include <obexparser.h>
#include <obexserver.h>

#include <bthapi.h>
#include <bt_sdp.h>

#define OBEX_BIGBUFFER            1024
#define OBEX_SMALLBUFFER        128

#define OBEX_MAXPORTS            6
#define OBEX_THREAD_TIMEOUT        5000

#define OBEX_SERVICEID_LEN        256

#define OBEX_MAINT_PERIOD_MIN        (60 * 1000)
#define OBEX_SERVER_TIMEOUT_MIN        (30 * 1000)
#define OBEX_CONNECTION_TIMEOUT_MIN    (60 * 1000)

#if defined (_DEBUG) || defined (DEBUG)
#define OBEX_MAINT_PERIOD        (60 * 1000)
#define OBEX_SERVER_TIMEOUT        (60 * 1000)

#define ZONE_INIT       DEBUGZONE(0)
#define ZONE_PROTOCOL   DEBUGZONE(1)
#define ZONE_MAINTAIN   DEBUGZONE(2)
#define ZONE_PACKET     DEBUGZONE(3)
#define ZONE_WARN       DEBUGZONE(14)
#define ZONE_ERROR      DEBUGZONE(15)

#else
#define OBEX_MAINT_PERIOD        (15 * 60 * 1000)
#define OBEX_SERVER_TIMEOUT        (10 * 60 * 1000)
#endif // DEBUG

#define OBEX_CONNECTION_TIMEOUT    (10 * 60 * 1000)

#define OBEX_MEM_SCALE            20

#define OBEX_INVALID_CID        0xffffffff

int obutil_IsLocal        (WCHAR *szFileName);
int obutil_GetGUID        (WCHAR *lpsz, GUID *pguid);
int obutil_PollSocket    (SOCKET s);



#define MAX_NUM_STP_RECS   10

// ppSdpRecord: this IN/OUT allows creation of a new ISdpRecord object. If not passed in, an instance will be created
//              and returned - and caller is responsible for releasing the object.
int    obutil_RegisterPort(unsigned char nPort, ULONG *pBTHHandleArray, UINT *uiNumInArray, ISdpRecord** ppSdpRecord);
int obutil_SdpDelRecord(ULONG hRecord);






static void *obex_Alloc    (int cSize);
static void obex_Free        (void *p);

static int  obex_Execute   (unsigned int uiOp, unsigned int uiTransactionId, struct _obex_command *pCommand);
static HRESULT  obex_Auth   (unsigned int uiTransactionId, UINT *uiRetCode);
static HRESULT  obex_EncryptionRequest  (unsigned int uiTransactionId, UINT *uiRetCode);


#if defined (UNDER_CE)
#define GPAARG(c)    L##c
#else
#define GPAARG(c)    c
#endif

#endif // __obexp_HXX__

