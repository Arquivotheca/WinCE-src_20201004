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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

// Abstract: Standard header file for Location Framework

#ifndef __LOC_SVC__
#define __LOC_SVC__

//
//  System includes
//
#include <windows.h>
#include <service.h>
#include <errorrep.h>
#include <lfApi.h>
#include <lfPlugin.h>

// System utilities
#include <svsutil.hxx>
#include <creg.hxx>
#include <string.hxx>
#include <list.hxx>
#include <vector.hxx>
#include <auto_xxx.hxx>
#include <hash_map.hxx>



//
//  Debugging utilities
//

#ifdef DEBUG
#define ZONE_INIT      DEBUGZONE(0)
#define ZONE_SERVICE   DEBUGZONE(1)
#define ZONE_PROVIDER  DEBUGZONE(2)
#define ZONE_RESOLVER  DEBUGZONE(3)
#define ZONE_REPCOL    DEBUGZONE(4)
#define ZONE_PLGMGR    DEBUGZONE(5)
#define ZONE_API       DEBUGZONE(6)
#define ZONE_THREAD    DEBUGZONE(7)

#define ZONE_VERBOSE   DEBUGZONE(11)
#define ZONE_ALLOC     DEBUGZONE(12)
#define ZONE_FUNCTION  DEBUGZONE(13)
#define ZONE_ERROR     DEBUGZONE(14)
#define ZONE_WARN      DEBUGZONE(15)

// Some messages should be output for either resolvers or providers
#define ZONE_PLUGIN    (ZONE_PROVIDER || ZONE_RESOLVER)


#define DEBUGMSG_OOM() DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Memory allocation failed.  GLE=<0x%08x>\r\n",GetLastError()))

#else  // DEBUG
#define DEBUGMSG_OOM()
#endif // DEBUG

//
//  Location Framework Registry Key and Values
//

// Registry key names
extern const WCHAR *g_rkLocBase;
extern const WCHAR *g_rkProviders;
extern const WCHAR *g_rkResolvers;

// Registry values
extern const WCHAR *g_rvDll;
extern const WCHAR *g_rvFriendlyName;
extern const WCHAR *g_rvGuid;
extern const WCHAR *g_rvPreference;
extern const WCHAR *g_rvPluginFlags;
extern const WCHAR *g_rvVersion;
extern const WCHAR *g_rvProviderFlags;
extern const WCHAR *g_rvResolverFlags;
extern const WCHAR *g_rvPollInterval;
extern const WCHAR *g_rvMaximumInitialWait;
extern const WCHAR *g_rvRetryOnFailure;
extern const WCHAR *g_rvGeneratedReports;
extern const WCHAR *g_rvSupportedReports;
extern const WCHAR *g_rvMinimumRequery;

//
//  Location Framework Specific Includes
//
#include "locPlugin.hpp"
#include "locRepCol.hpp"
#include "locPlgMgr.hpp"
#include "locHandles.hpp"
#include "..\inc\locIoctl.hpp"

//
//  Global variables and functions
// 
extern SVSSynch      *g_pLocLock;
extern SVSThreadPool *g_pThreadPool;
extern DWORD          g_serviceState;
extern const DWORD    g_pollOnUnload;
extern const DWORD    g_pollRetriesWarnOnUnload;


BOOL  InitLocService(void);
void  DeInitLocService(void);
DWORD WINAPI StartLocServiceThread(LPVOID lpv);
DWORD WINAPI StopLocServiceThread(LPVOID lpv);
DWORD WINAPI RefreshLocServiceThread(LPVOID lpv);
BOOL  GetServiceStatus(PBYTE pBufOut, DWORD lenOut, DWORD *pActualLenOut);

typedef DWORD (WINAPI *PFN_LOCATION_THREAD)(LPVOID lpv);
BOOL LocationSpinThreadAndWait(PFN_LOCATION_THREAD pFunc, LPVOID lpv);

// By default, only unhandled exceptions are sent to the Watson server.
// Since LF uses __try/__except so much, these exceptions would not be 
// recorded unless we manually do so via ReportFault.
#define REPORT_EXCEPTION_TO_WATSON() (ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER)

//
//  Handeling of user mode APIs calls
//

// MY_PSL_COPYIN_GUID typedef used to marshal GUID type across processes
typedef ce::marshal_arg<ce::copy_in,const GUID *> MY_PSL_COPYIN_GUID;

BOOL LocationOpenIntrnl(serviceHandle_t *pServiceHandle, DWORD version, ce::PSL_HANDLE pReserved, DWORD flags, ce::marshal_arg<ce::copy_out,ce::PSL_HANDLE *> phOutHandle);
BOOL LocationCloseIntrnl(serviceHandle_t *pServiceHandle, ce::PSL_HANDLE hLocation);
BOOL LocationRegisterForReportIntrnl(serviceHandle_t *pServiceHandle, ce::PSL_HANDLE hLocation, ce::PSL_HANDLE hNewLocationReport,
                                 ce::PSL_HANDLE hStateChangeEvent, MY_PSL_COPYIN_GUID pReportType, 
                                 DWORD flags);
BOOL LocationUnRegisterForReportIntrnl(serviceHandle_t *pServiceHandle, ce::PSL_HANDLE hLocation, MY_PSL_COPYIN_GUID pReportType, DWORD flags);
BOOL LocationGetReportIntrnl(serviceHandle_t *pServiceHandle, ce::PSL_HANDLE hLocation, MY_PSL_COPYIN_GUID pReportType, DWORD maximumAge, ce::marshal_arg<ce::copy_out,ce::psl_buffer_wrapper<BYTE*> > pLocationReport,
                             ce::marshal_arg<ce::copy_in_out,DWORD*> pcbLocationReport, DWORD flags);
BOOL LocationGetServiceStateIntrnl(serviceHandle_t *pServiceHandle, ce::PSL_HANDLE hLocation, ce::marshal_arg<ce::copy_out,LOCATION_SERVICE_STATE*> pServiceState);
BOOL LocationGetPluginInfoForReportIntrnl(serviceHandle_t *pServiceHandle, ce::PSL_HANDLE hLocation, MY_PSL_COPYIN_GUID pReportType, ce::marshal_arg<ce::copy_out,PLUGIN_STATE *> pPluginState, ce::marshal_arg<ce::copy_out,GUID *> pPlugin);
BOOL LocationGetProvidersInfoIntrnl(serviceHandle_t *pServiceHandle, ce::PSL_HANDLE hLocation, ce::marshal_arg<ce::copy_out,ce::psl_buffer_wrapper<BYTE*> > pProvidersBuffer, ce::marshal_arg<ce::copy_in_out,DWORD*> pcbBuffer);
BOOL LocationGetResolversInfoIntrnl(serviceHandle_t *pServiceHandle, ce::PSL_HANDLE hLocation, ce::marshal_arg<ce::copy_out,ce::psl_buffer_wrapper<BYTE*> > pResolversBuffer, ce::marshal_arg<ce::copy_in_out,DWORD*> pcbBuffer);
BOOL LocationPluginOpenIntrnl(serviceHandle_t *pServiceHandle, HLOCATION hLocation, const GUID *pPluginGuid, HLOCATIONPLUGIN *phOutHandle);
BOOL LocationPluginOpenPSL(serviceHandle_t *pServiceHandle, ce::PSL_HANDLE hLocation, MY_PSL_COPYIN_GUID pPluginGuid, ce::marshal_arg<ce::copy_out,ce::PSL_HANDLE *> phOutHandle);
BOOL LocationPluginIOCTLIntrnl(serviceHandle_t *pServiceHandle, HLOCATION hLocation, HLOCATIONPLUGIN hPlugin, DWORD dwCode, BYTE* pbIn, DWORD cbIn, BYTE* pbOut, DWORD* pcbOut);
BOOL LocationPluginIOCTLPSL(serviceHandle_t *pServiceHandle, ce::PSL_HANDLE hLocation, ce::PSL_HANDLE hPlugin, DWORD dwCode,
                               ce::marshal_arg<ce::copy_in,ce::psl_buffer_wrapper<BYTE*> > pbIn,
                               ce::marshal_arg<ce::copy_out,ce::psl_buffer_wrapper<BYTE*> > pbOut,
                               ce::marshal_arg<ce::copy_in_out,DWORD*> pcbOut);
BOOL LocationPluginCloseIntrnl(serviceHandle_t *pServiceHandle, HLOCATION hLocation, HLOCATIONPLUGIN hPlugin);
BOOL LocationPluginClosePSL(serviceHandle_t *pServiceHandle, ce::PSL_HANDLE hLocation, ce::PSL_HANDLE hPlugin);

#endif // __LOC_SVC__

