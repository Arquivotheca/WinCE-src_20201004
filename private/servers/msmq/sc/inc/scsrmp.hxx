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

    scsrmp.hxx

Abstract:

    SRMP Global Header File

--*/



#if ! defined (__scrmp_HXX__)
#define __scrmp_HXX__	1

//
// Registry information
//
// Base Key
#define MSMQ_SC_SRMP_REGISTRY_KEY			L"SOFTWARE\\Microsoft\\MSMQ\\SimpleClient\\SRMP"

// Maximum size of POST data to read by ISAPI extension
#define MSMQ_SC_SRMP_MAX_POST               L"MaxPost"
#define MSMQ_SC_SRMP_MAX_POST_DEFAULT       131072


//
// Data structures, enums, and globals
//
extern const BOOL g_fHaveSRMP;

typedef enum {
	CONTENT_TYPE_XML,        // text/xml
	CONTENT_TYPE_MIME,       // multipart/related
	CONTENT_TYPE_UNKNOWN     // ??
} CONTENT_TYPE;

typedef struct _SrmpIOCTLPacket {
	LPSTR             pszHeaders;
	DWORD             cbHeaders;
	LPSTR             pszPost;
	DWORD             cbPost;
	CONTENT_TYPE      contentType;
	BOOL              fSSL;
	DWORD             dwIP4Addr;
} SrmpIOCTLPacket, *PSrmpIOCTLPacket;

void SrmpAcceptHttpRequest(PSrmpIOCTLPacket pSrmpPacket, DWORD *pdwHttpStatus);
#endif
