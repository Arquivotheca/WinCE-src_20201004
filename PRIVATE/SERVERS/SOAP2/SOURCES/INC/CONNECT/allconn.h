//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
