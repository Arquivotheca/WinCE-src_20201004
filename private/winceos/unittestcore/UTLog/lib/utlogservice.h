//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
// ----------------------------------------------------------------------------

//******************************************************************************
//
// UTLogService.H
//
// Definition module for the UTLogService C and C++ interfaces.
//
//******************************************************************************

#ifndef __UTLogService_H__
#define __UTLogService_H__

#include <UTLog.h>
#include <katoex.h>

//******************************************************************************
// Types
//******************************************************************************

typedef HANDLE HUTLOGSVC;  // Handle to CUTLogService object

//******************************************************************************
// Object-interface
//******************************************************************************

EXTERN_C HUTLOGSVC WINAPI
UTLSvcGetDefault(
    void);

EXTERN_C BOOL WINAPI
UTLSvcSetDefault(
    HUTLOGSVC hNewLogger);

EXTERN_C HKATO WINAPI
UTLSvcGetKato(
    const HUTLOGSVC hLogger);

EXTERN_C BOOL WINAPI
UTLSvcSetKato(
    HUTLOGSVC hLogger, 
    HKATO     hNewKato);

typedef HUTLOGSVC(*PFUTLSvcGetDefault_t)(void);
typedef BOOL     (*PFUTLSvcSetDefault_t)(HUTLOGSVC);
typedef HKATO    (*PFUTLSvcGetKato_t)   (const HUTLOGSVC);
typedef BOOL     (*PFUTLSvcSetKato_t)   (HUTLOGSVC, HKATO);

#define UTLSVC_PROCNAME_GetDefault  CE_PROCNAME("UTLSvcGetDefault")
#define UTLSVC_PROCNAME_SetDefault  CE_PROCNAME("UTLSvcSetDefault")
#define UTLSVC_PROCNAME_GetService  CE_PROCNAME("UTLSvcGetService")
#define UTLSVC_PROCNAME_SetService  CE_PROCNAME("UTLSvcSetService")
#define UTLSVC_PROCNAME_GetKato     CE_PROCNAME("UTLSvcGetKato")
#define UTLSVC_PROCNAME_SetKato     CE_PROCNAME("UTLSvcSetKato")

//******************************************************************************
// Construction and destruction
//******************************************************************************

EXTERN_C HUTLOGSVC WINAPI
UTLSvcCreateW(
    DWORD   LogStreams,
    LPCWSTR pLogName,
    LPCWSTR pKatoName);

EXTERN_C HUTLOGSVC WINAPI
UTLSvcCreateA(
    DWORD   LogStreams,
    LPCSTR  pLogName,
    LPCSTR  pKatoName);

EXTERN_C BOOL WINAPI
UTLSvcDestroy(
    HUTLOGSVC hLogger);

typedef HUTLOGSVC(*PFUTLSvcCreateW_t)(DWORD, LPCWSTR, LPCWSTR);
typedef HUTLOGSVC(*PFUTLSvcCreateA_t)(DWORD, LPCSTR, LPCSTR);
typedef HUTLOGSVC(*PFUTLSvcCopy_t)   (const HUTLOGSVC);
typedef BOOL     (*PFUTLSvcDestroy_t)(HUTLOGSVC);

#define UTLSVC_PROCNAME_CreateW  CE_PROCNAME("UTLSvcCreateW")
#define UTLSVC_PROCNAME_CreateA  CE_PROCNAME("UTLSvcCreateA")
#define UTLSVC_PROCNAME_Copy     CE_PROCNAME("UTLogCopy")
#define UTLSVC_PROCNAME_Destroy  CE_PROCNAME("UTLSvcDestroy")

//******************************************************************************
// Unicode messages
//******************************************************************************

EXTERN_C BOOL WINAPIV
UTLSvcLogW(
    HUTLOGSVC hLogger,
    DWORD     Verbosity, 
    LPCWSTR   pFormat,
    ...);

EXTERN_C BOOL WINAPI
UTLSvcLogVW(
    HUTLOGSVC hLogger,
    DWORD     Verbosity,
    LPCWSTR   pFormat, 
    va_list   pArgs);

typedef BOOL (*PFUTLSvcLogW_t) (HUTLOGSVC, DWORD, LPCWSTR, ...);
typedef BOOL (*PFUTLSvcLogVW_t)(HUTLOGSVC, DWORD, LPCWSTR, va_list);

#define UTLSVC_PROCNAME_LogW   CE_PROCNAME("UTLSvcLogW")
#define UTLSVC_PROCNAME_LogVW  CE_PROCNAME("UTLSvcLogVW")

//******************************************************************************
// ASCII messages
//******************************************************************************

EXTERN_C BOOL WINAPIV
UTLSvcLogA(
    HUTLOGSVC hLogger, 
    DWORD     Verbosity, 
    LPCSTR    pFormat,
    ...);

EXTERN_C BOOL WINAPI
UTLSvcLogVA(
    HUTLOGSVC hLogger, 
    DWORD     Verbosity,
    LPCSTR    pFormat,
    va_list   pArgs);

typedef BOOL (*PFUTLSvcLogA_t) (HUTLOGSVC, DWORD, LPCSTR, ...);
typedef BOOL (*PFUTLSvcLogVA_t)(HUTLOGSVC, DWORD, LPCSTR, va_list);

#define UTLSVC_PROCNAME_LogA   CE_PROCNAME("UTLSvcLogA")
#define UTLSVC_PROCNAME_LogVA  CE_PROCNAME("UTLSvcLogVA")

//******************************************************************************
// Non-string functions
//******************************************************************************

EXTERN_C BOOL WINAPI
UTLSvcFlush(
    HUTLOGSVC hLogger);

EXTERN_C DWORD WINAPI
UTLSvcGetLogStreams(
    const HUTLOGSVC hLogger);

EXTERN_C DWORD WINAPI
UTLSvcGetLogLevel(
    const HUTLOGSVC hLogger);

EXTERN_C BOOL WINAPI
UTLSvcSetLogLevel(
    HUTLOGSVC hLogger,
    DWORD     NewVerbosity);

typedef BOOL  (*PFUTLSvcFlush_t)         (HUTLOGSVC);
typedef DWORD (*PFUTLSvcGetLogStreams_t)(const HUTLOGSVC);
typedef DWORD (*PFUTLSvcGetLogLevel_t)  (const HUTLOGSVC);
typedef BOOL  (*PFUTLSvcSetLogLevel_t)  (HUTLOGSVC, DWORD);

#define UTLSVC_PROCNAME_Flush          CE_PROCNAME("UTLSvcFlush")
#define UTLSVC_PROCNAME_GetLogStreams  CE_PROCNAME("UTLSvcGetLogStreams")
#define UTLSVC_PROCNAME_GetLogLevel    CE_PROCNAME("UTLSvcGetLogLevel")
#define UTLSVC_PROCNAME_SetLogLevel    CE_PROCNAME("UTLSvcSetLogLevel")

//******************************************************************************
// Map function names to the correct APIs based on the UNICODE flag
//******************************************************************************

#ifdef UNICODE
    #define UTLSvcCreate            UTLSvcCreateW
    #define UTLSvcLog               UTLSvcLogW
    #define UTLSvcLogV              UTLSvcLogVW
    #define PFUTLSvcCreate_t        PFUTLSvcCreateW_t
    #define PFUTLSvcLog_t           PFUTLSvcLogW_t
    #define PFUTLSvcLogV_t          PFUTLSvcLogVW_t
    #define UTLSVC_PROCNAME_Create  UTLSVC_PROCNAME_CreateW
    #define UTLSVC_PROCNAME_Log     UTLSVC_PROCNAME_LogW
    #define UTLSVC_PROCNAME_LogV    UTLSVC_PROCNAME_LogVW
#else
    #define UTLSvcCreate            UTLSvcCreateA
    #define UTLSvcLog               UTLSvcLogA
    #define UTLSvcLogV              UTLSvcLogVA
    #define PFUTLSvcCreate_t        PFUTLSvcCreateA_t
    #define PFUTLSvcLog_t           PFUTLSvcLogA_t
    #define PFUTLSvcLogV_t          PFUTLSvcLogVA_t
    #define UTLSVC_PROCNAME_Create  UTLSVC_PROCNAME_CreateA
    #define UTLSVC_PROCNAME_Log     UTLSVC_PROCNAME_LogA
    #define UTLSVC_PROCNAME_LogV    UTLSVC_PROCNAME_LogVA
#endif

#endif // __UTLogService_H__
