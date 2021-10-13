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
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.
Copyright (c) 1995-2000 Microsoft Corporation

Module Name:   Kitl_priv.h

Abstract:  
    This contains declarations the KITL routines.

Functions:


Notes: 
    This is a copy of the kitl.h file found in the Tools tree at:
        %_WINCEROOT%\Tools\platman\source\kitl\inc

--*/
#ifndef _KITL_H_
#define _KITL_H_

#include <KitlTran_priv.h>
#include <KitlClnt_priv.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef UINT KITLTRANLIB;
typedef void (* PFN_REGCB) (LPCWSTR pszSvcName, BOOL fRegister, LPVOID pUserData);
typedef void (* PFN_NEWSVCCB) (LPCWSTR pszSvcName, LPVOID pUserData);
typedef BOOL (* PFN_ENUMTRAN) (LPCWSTR pszDllName, LPCWSTR pszDesc, LPVOID pUserData);
typedef void (* PFN_OSSTARTCB) (LPVOID pUserData, LPCVOID pData, USHORT cbData);
typedef void (* PFN_NAMESVCCB) (LPCWSTR pszDevName, LPVOID pUserData, LPCVOID pData, USHORT cbData);

typedef enum _KITL_NOTIF_CMD {
    KitlNotifCmdOpen,
    KitlNotifCmdClose,
    KitlNotifCmdRead
} KITL_NOTIF_CMD;

typedef struct _KITL_NOTIF {
    KITL_NOTIF_CMD cmd; // Command
    const WCHAR* pwszFileName; // Fully qualified file name
    HANDLE hFile; // Handle to the file
    DWORD dwFilePos; // File position
} KITL_NOTIF, *PKITL_NOTIF;

// default service callbacks
typedef int (* PFN_GETINPUT) (LPWSTR pszInput, int nMax, LPVOID pUserData);
typedef void (* PFN_CMDOUTPUT) (LPCWSTR pszMsg, LPVOID pUserData);
typedef BOOL (* PFN_OPENFILECB) (LPCWSTR pszFileName, LPWSTR pszFullPath, BOOL fOpenExisting, LPVOID pUserData);
typedef void (* PFN_MSGOUTPUT) (LPCWSTR pszMsg, DWORD dwThrdId, DWORD dwProcId, DWORD dwTimeStamp, LPVOID pUserData);
typedef BOOL (* PFN_NOTIF) (KITL_NOTIF* pNotif, LPVOID pUserData);

#define INVALID_KLIBID  ((KITLTRANLIB) -1)

// default services (PPSH and DBGMSH)
KITLID KITLRegisterPPSHService (LPCWSTR pszDevName, PFN_GETINPUT pfnInput, PFN_CMDOUTPUT pfnOutput, PFN_OPENFILECB pfnOfnCB, LPVOID pUserData);
KITLID KITLRegisterPPSHService2 (LPCWSTR pszDevName, PFN_GETINPUT pfnInput, PFN_CMDOUTPUT pfnOutput, PFN_OPENFILECB pfnOfnCB, PFN_NOTIF pfnNotifCB, LPVOID pUserData);
KITLID KITLRegisterDBGMSGService (LPCWSTR pszDevName, PFN_MSGOUTPUT pfnOutput, LPVOID pUserData);

//////////////////////////////////
// server side functions
//

// load/unload transport library
KITLTRANLIB KITLLoadTransportLib (LPCWSTR pszDllName);
BOOL KITLUnloadTransportLib (KITLTRANLIB klib);
// create/delete KITL server
KITLID KITLCreateServer (KITLTRANLIB klib, LPCWSTR pszDevName);
KITLID KITLCreateServer2 (KITLTRANLIB klib, LPCWSTR pszDevName, DWORD dwBootFlags);
BOOL KITLDeleteServer (KITLID id);
// callback for client registration
BOOL KITLSetRegisterCallback (KITLID id, PFN_REGCB pfnCB, LPVOID pUserData);
// callback when device asked for a service not registered
BOOL KITLSetNewSvcCallback (KITLID id, PFN_NEWSVCCB pfnCB, LPVOID pUserData);

// callback when OS started
BOOL KITLSetOSStartCB (KITLID id, PFN_OSSTARTCB pfnCB, LPVOID pUserData);

// callback to provide name service
BOOL KITLSetNameSvcCB (KITLID id, PFN_NAMESVCCB pfnCB, LPVOID pUserData);

// Provide the access rights and flat release directory to the Command Service
BOOL KITLSetFileSystemAccessRights (KITLID id, BOOL fAccessAllowed, LPCWSTR strFlatReleaseFolder);

// the following two function is a 'one-shot' command (can't expect ACK from 
// the device). The device might not reset/break even if the command succeed.
// reset device
BOOL KITLResetDevice (KITLID id, BOOL fCleanBoot);
// debug break
BOOL KITLDebugBreak (KITLID id);

// bypass functions for the transport
BOOL KITLEnumKnownDevice (KITLTRANLIB klib, PFN_EnumProc pfnCB, LPVOID pUserData);

#define KITL_FL_DBGMSG    0x01  // Debug messages
#define KITL_FL_PPSH      0x02  // Text shell
#define KITL_FL_KDBG      0x04  // Kernel debugger
#define KITL_FL_CLEANBOOT 0x08  // Force a clean boot

#define KITL_FL_DEFAULT (KITL_FL_DBGMSG|KITL_FL_PPSH|KITL_FL_KDBG|KITL_FL_CLEANBOOT)
DWORD KITLGetBootFlags (KITLID id);
void KITLSetBootFlags (KITLID id, DWORD dwBootFlags);
DWORD KITLGetCpuId (KITLID id);

////////////////////////////////////
// helper functions

// get all available transport dll names.
// string is NULL seperated, with the last name terminated by 2 NULLs
BOOL KITLEnumTransports (PFN_ENUMTRAN pfnEnum, LPVOID pUserData);
BOOL KITLGetTranXMLParams (KITLTRANLIB klib, LPCWSTR *ppstrXML);
BOOL KITLSetTranParam (KITLTRANLIB klib, LPCWSTR strName, LPCWSTR strValue, LPWSTR pstrError, long dwErrorStringSize);
BOOL KITLGetTranInfo (KITLTRANLIB klib, PTRANPORTINFOSTRUCT pInfo);

// function to call for setup program to add a new DLL to KITL registry.
// (You can also deal with registry yourself, but I think this is cleaner.)
BOOL KITLAddNewTransportDll (LPCWSTR pszDllName, LPCWSTR pszDesc);

// debug support
typedef void (* PFN_OUTPUTSTRING) (LPCWSTR pszStr, LPVOID pUserData);
void KITLSetDebug (DWORD dwZones, PFN_OUTPUTSTRING pfnDbgMsg, LPVOID pUserData);

// function to set alternate device name
BOOL KITLSetDeviceAlias (KITLID id, LPCWSTR pszDevName);


#ifdef __cplusplus
}
#endif

#endif  // _KITL_H_
