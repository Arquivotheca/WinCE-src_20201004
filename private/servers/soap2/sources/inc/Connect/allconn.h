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
//+----------------------------------------------------------------------------
//
//
// File:
//      AllConn.h
//
// Contents:
//
//      Declaration of global variables shared by all connector objects
//
//-----------------------------------------------------------------------------

#ifndef __ALLCONN_H_INCLUDED__
#define __ALLCONN_H_INCLUDED__

extern WCHAR            s_EndPointURL[];
extern WCHAR            s_Timeout[];
extern WCHAR            s_AuthUser[];
extern WCHAR            s_AuthPassword[];
extern WCHAR            s_ProxyUser[];
extern WCHAR            s_ProxyPassword[];
extern WCHAR            s_ProxyServer[];
extern WCHAR            s_ProxyPort[];
extern WCHAR            s_UseProxy[];
extern WCHAR            s_UseSSL[];
extern WCHAR            s_SSLClientCertificateName[];
extern WCHAR            s_SoapAction[];
extern WCHAR            s_HTTPCharset[];
extern const CHAR       s_TextXmlA[];
extern const WCHAR      s_TextXmlW[];

#endif //__ALLCONN_H_INCLUDED__
