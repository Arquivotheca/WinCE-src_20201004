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

    scutil.hxx

Abstract:

    Small client utility functions header


--*/
#if ! defined (__scutil_HXX__)
#define __scutil_HXX__	1

struct ScQueueParms;

LPCWSTR scutil_ParseGuidString(LPCWSTR p, GUID* pg);

WCHAR	*scutil_MakeFormatName (WCHAR *a_lpszPathName,   ScQueueParms *a_pqp);
WCHAR	*scutil_MakeFormatName (WCHAR *a_lpszHostName,   WCHAR *a_lpszQueueName, ScQueueParms *a_pqp);
WCHAR	*scutil_MakePathName   (WCHAR *a_lpszHostName,   WCHAR *a_lpszQueueName, ScQueueParms *a_pqp);
WCHAR	*scutil_MakeFileName   (WCHAR *a_lpszHostName,   WCHAR *a_lpszQueueName,  ScQueueParms *pqp);
int		scutil_ParsePathName   (WCHAR *a_lpszPathName,   WCHAR *&a_lpszHostName, WCHAR *&a_lpszQueueName, ScQueueParms *a_pqp);
int		scutil_ParseNonLocalDirectFormatName (WCHAR *a_lpszFormatName, WCHAR *&a_lpszHostName, WCHAR *&a_lpszFileName, ScQueueParms *a_pqp);

inline WCHAR *scutil_OutOfProcDup (WCHAR *lpsz) {
    return gMem->remoteAlloc.DuplicateStringAndTranslate(lpsz);
}
void scutil_uuidgen (GUID *pGuid);

struct QUEUE_FORMAT;

int scutil_StringToQF (QUEUE_FORMAT *pqf, WCHAR *lpszName);
WCHAR *scutil_QFtoString (QUEUE_FORMAT *pqft);

unsigned int scutil_now (void);
int scutil_GetLanNum (void);
int scutil_IsLocalTCP (WCHAR *szHostName); // NULL == reset the table...


#define SCUTIL_DIRECT_PREFIX		L"DIRECT="
#define SCUTIL_DIRECT_PREFIX_SZ		SVSUTIL_CONSTSTRLEN (SCUTIL_DIRECT_PREFIX)

enum DIRECT_TOKEN_TYPE {
    DT_OS,
    DT_TCP,
    DT_SPX,
    DT_HTTP,
    DT_HTTPS
};
LPCWSTR ParseDirectTokenString(LPCWSTR p, DIRECT_TOKEN_TYPE& dtt);

void SrmpInit(void);
WCHAR* scutil_StrStrWI(const WCHAR * str1, const WCHAR * str2);
void scutil_ReplaceBackSlashesWithSlashes(WCHAR *a_lpszName);
int IsURLLocalMachine(const WCHAR *szURL, int fGeneratedLocally=FALSE);

class ScPacketImage;
WCHAR * GetNextHopOnFwdList(ScPacketImage *pImage);

void CloseOverlappedHandle (SVSHandle h);

#endif	/* __scutil_HXX__ */
