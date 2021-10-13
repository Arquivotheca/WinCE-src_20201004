//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
