//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: HTTPCOMN.H
Abstract: Registry settings and default values used by httpd.dll, the http
		  admin object, and ASP.
--*/

//  Default settings for httpd that are used by admin object and httpd.
#define HTTPD_DEFAULT_PAGES   L"default.htm;index.htm"
#define HTTPD_ALLOW_DIR_BROWSE 0	// 1 --> allow directory browsing, 0 --> don't
#define HTTP_DEFAULTP_PERMISSIONS  HSE_URL_FLAGS_EXECUTE | HSE_URL_FLAGS_READ | HSE_URL_FLAGS_SCRIPT



//------------- Registry keys -----------------------
//  These are used by httpd and by the httpd administration object.


// Absolute registry keys.
#ifdef UNDER_CE
#define RK_HTTPD		        L"COMM\\HTTPD"
#define RK_SCRIPT_MAP           L"COMM\\HTTPD\\SCRIPTMAP"
#define RK_SCRIPT_NOUNLOAD      L"COMM\\HTTPD\\SCRIPTPERSIST"
#define RK_WEBSITES             L"COMM\\HTTPD\\WEBSITES"
#else
#define RK_HTTPD		    L"SOFTWARE\\MICROSOFT\\HTTPD"
#define RK_SCRIPT_MAP       L"SOFTWARE\\MICROSOFT\\HTTPD\\SCRIPTMAP"
#define RK_SCRIPT_NOUNLOAD  L"SOFTWARE\\MICROSOFT\\HTTPD\\SCRIPTPERSIST"
#define RK_WEBSITES         L"SOFTWARE\\MICROSOFT\\HTTPD\\WEBSITES"
#endif
// Following registry keys are relative to the website they're under
#define RK_SSL              L"SSL"
#define RK_HTTPDVROOTS	    L"VROOTS"
#define RK_ASP			    L"ASP"
#define RK_IPMAP            L"IPMapping"

// Basic config settings
#define RV_PORT			  L"Port"
#define RV_DEFAULTPAGE    L"DefaultPage"
#define RV_MAXLOGSIZE     L"MaxLogSize"
#define RV_FILTER         L"Filter DLLs"
#define RV_LOGDIR         L"LogFileDirectory"
#define RV_ADMINUSERS     L"AdminUsers"
#define RV_ADMINGROUPS    L"AdminGroups"
#define RV_ISENABLED      L"IsEnabled"
#define RV_POSTREADSIZE   L"PostReadSize"
#define RV_SCRIPTTIMEOUT  L"ScriptUnloadDelay"
#define RV_MAXHEADERSIZE  L"MaxHeaderSize"
#define RV_MAXCONNECTIONS L"MaxConnections"
#define RV_HOSTEDSITES    L"HostedSites"
#define RV_WEBDAV         L"WebDAV"
#define RV_CHANGENUMBER   L"SystemChangeNumber"
#define RV_DEFAULT_DAV_LOCK L"DefaultDavLock"
#define RV_SERVER_ID      L"ServerId"

// VRoot config
#define RV_PERM			  L"p"	// permissions
#define RV_AUTH			  L"a"	// authentication reqd
#define RV_BASIC		  L"Basic"
#define RV_NTLM			  L"NTLM"
#define RV_BASICTONTLM    L"BasicToNtlm"
#define RV_DIRBROWSE	  L"DirBrowse"
#define RV_USERLIST       L"UserList"   // Per vroot Access Control List
#define RV_REDIRECT       L"Redirect"
#define REDIRECT_VALUE    L"$REDIRECT"  // When this value is set as a VRoot's physical path, we'll redirect request.


// ASP
#define RV_ASP_LANGUAGE      L"LANGUAGE"
#define RV_ASP_CODEPAGE      L"CODEPAGE"
#define RV_ASP_LCID          L"LCID"
#define RV_ASP_VERBOSE_ERR   L"VerboseErrorMessages"

// SSL basic configuration (under key SSL)
#define RV_SSL_PORT          L"Port"
#define RV_SSL_ENABLE        L"IsEnabled"
#define RV_SSL_CERT_SUBJECT  L"CertificateSubject"
// SSL Client Cert fields
#define RV_SSL_CERT_TRUST_OVERRIDE     L"CertTrustOverride"

// SSL User mapping table (under key SSL\Users)
#define RK_USERS             L"Users"
#define RV_SSL_ISSUER_CN     L"IssuerCN"
#define RV_SSL_SERIAL_NUMBER L"SerialNumber"


// Multiple websites / IP mapping
#define RV_ALLOW_DEFAULTSITE      L"AllowDefaultSite"

typedef enum {
	VBSCRIPT,
	JSCRIPT
} SCRIPT_LANG;



