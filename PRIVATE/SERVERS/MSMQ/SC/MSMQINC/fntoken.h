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
    fntoken.h

Abstract:
    Format Name tokens

Revision History:

--*/

#ifndef __FNTOKEN_H
#define __FNTOKEN_H

//
//  Find constant string legth
//
#define STRLEN(x) (sizeof(x)/sizeof((x)[0])-1)

#define GUID_ELEMENTS(p) \
    p->Data1,                 p->Data2,    p->Data3,\
    p->Data4[0], p->Data4[1], p->Data4[2], p->Data4[3],\
    p->Data4[4], p->Data4[5], p->Data4[6], p->Data4[7]



//
//  Format Name prefix tokens
//
#define FN_PUBLIC_TOKEN         L"PUBLIC"
#define FN_PUBLIC_TOKEN_LEN     STRLEN(FN_PUBLIC_TOKEN)

#define FN_PRIVATE_TOKEN        L"PRIVATE"
#define FN_PRIVATE_TOKEN_LEN    STRLEN(FN_PRIVATE_TOKEN)

#define FN_DIRECT_TOKEN         L"DIRECT"
#define FN_DIRECT_TOKEN_LEN     STRLEN(FN_DIRECT_TOKEN)

#define FN_MACHINE_TOKEN        L"MACHINE"
#define FN_MACHINE_TOKEN_LEN    STRLEN(FN_MACHINE_TOKEN)

#define FN_CONNECTOR_TOKEN      L"CONNECTOR"
#define FN_CONNECTOR_TOKEN_LEN  STRLEN(FN_CONNECTOR_TOKEN)


//
//  Format Name suffix tokens
//
#define FN_NONE_SUFFIX          L""
#define FN_NONE_SUFFIX_LEN      STRLEN(FN_NONE_SUFFIX)

#define FN_JOURNAL_SUFFIX       L";JOURNAL"
#define FN_JOURNAL_SUFFIX_LEN   STRLEN(FN_JOURNAL_SUFFIX)

#define FN_DEADLETTER_SUFFIX    L";DEADLETTER"
#define FN_DEADLETTER_SUFFIX_LEN STRLEN(FN_DEADLETTER_SUFFIX)

#define FN_DEADXACT_SUFFIX      L";DEADXACT"
#define FN_DEADXACT_SUFFIX_LEN  STRLEN(FN_DEADXACT_SUFFIX)

#define FN_XACTONLY_SUFFIX      L";XACTONLY"
#define FN_XACTONLY_SUFFIX_LEN  STRLEN(FN_XACTONLY_SUFFIX)


//
//  Format Name direct infix tokens
//
#define FN_DIRECT_OS_TOKEN      L"OS:"
#define FN_DIRECT_OS_TOKEN_LEN  STRLEN(FN_DIRECT_OS_TOKEN)

#define FN_DIRECT_TCP_TOKEN     L"TCP:"
#define FN_DIRECT_TCP_TOKEN_LEN STRLEN(FN_DIRECT_TCP_TOKEN)

#define FN_DIRECT_SPX_TOKEN     L"SPX:"
#define FN_DIRECT_SPX_TOKEN_LEN STRLEN(FN_DIRECT_SPX_TOKEN)


//
//  GUID_STR_LENGTH is the buffer size required for string guid
//
#define GUID_FORMAT     L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x"
#define GUID_STR_LENGTH (8 + 1 + 4 + 1 + 4 + 1 + 4 + 1 + 12)

#define PRIVATE_ID_FORMAT   L"%08x"
#define SUFFIX_FORMAT       L"%s"


//
//  Format Names tokens
//
#define FN_EQUAL_SIGN   L"="
#define FN_EQUAL_SIGN_C L'='

#define FN_PRIVATE_SEPERATOR    L"\\"
#define FN_PRIVATE_SEPERATOR_C  L'\\'
#define FN_HTTP_SEPERATOR_C     L'/'

#define FN_HTTP_SEPERATORS   L"\\/"
#define FN_SUFFIX_DELIMITER_C   L';'
#define FN_MQF_SEPARATOR_C      L','

#endif //  __FNTOKEN_H
