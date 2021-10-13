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
#include <windows.h>
#include <kitl_priv.h>

typedef BOOL (* PFN_KITLInitLibrary) (LPCWSTR pszRegKeyRoot);
typedef BOOL (* PFN_KITLDeInitLibrary) (void);
typedef KITLID (* PFN_KITLRegisterClient) (LPCWSTR pszDevName, LPCWSTR pszSvcName, DWORD dwTimeout, DWORD dwFlags);
typedef BOOL (* PFN_KITLDeRegisterClient) (KITLID id);
typedef BOOL (* PFN_KITLSend) (KITLID id, LPCVOID pData, DWORD cbData, DWORD dwTimeout);
typedef BOOL (* PFN_KITLRecv) (KITLID id, LPVOID pBuffer, DWORD *pcbBuffer, DWORD dwTimeout);
typedef BOOL (* PFN_KITLWaitForSvcConnect) (KITLID id, DWORD dwTimeout);

typedef KITLID (* PFN_KITLRegisterPPSHService) (LPCWSTR pszDevName, PFN_GETINPUT pfnInput, PFN_CMDOUTPUT pfnOutput, PFN_OPENFILECB pfnOfnCB, LPVOID pUserData);
typedef KITLID (* PFN_KITLRegisterDBGMSGService) (LPCWSTR pszDevName, PFN_MSGOUTPUT pfnOutput, LPVOID pUserData);
typedef KITLTRANLIB (* PFN_KITLLoadTransportLib) (LPCWSTR pszDllName);
typedef BOOL (* PFN_KITLUnloadTransportLib) (KITLTRANLIB klib);
typedef KITLID (* PFN_KITLCreateServer) (KITLTRANLIB klib, LPCWSTR pszDevName);
typedef BOOL (*PFN_KITLDeleteServer) (KITLID id);
typedef BOOL (* PFN_KITLSetRegisterCallback) (KITLID id, PFN_REGCB pfnCB, LPVOID pUserData);
typedef BOOL (* PFN_KITLSetNewSvcCallback) (KITLID id, PFN_NEWSVCCB pfnCB, LPVOID pUserData);
typedef BOOL (* PFN_KITLSetOSStartCB) (KITLID id, PFN_OSSTARTCB pfnCB, LPVOID pUserData);
typedef BOOL (* PFN_KITLSetNameSvcCB) (KITLID id, PFN_NAMESVCCB pfnCB, LPVOID pUserData);
typedef BOOL (* PFN_KITLResetDevice) (KITLID id, BOOL fCleanBoot);
typedef BOOL (* PFN_KITLDebugBreak) (KITLID id);
typedef BOOL (* PFN_KITLEnumKnownDevice) (KITLTRANLIB klib, PFN_EnumProc pfnCB, LPVOID pUserData);
typedef DWORD (* PFN_KITLGetBootFlags) (KITLID id);
typedef void (* PFN_KITLSetBootFlags) (KITLID id, DWORD dwBootFlags);
typedef DWORD (* PFN_KITLGetCpuId) (KITLID id);
typedef BOOL (* PFN_KITLEnumTransports) (PFN_ENUMTRAN pfnEnum, LPVOID pUserData);
typedef BOOL (* PFN_KITLGetTranXMLParams) (KITLTRANLIB klib, LPCWSTR *ppstrXML);
typedef BOOL (* PFN_KITLSetTranParam) (KITLTRANLIB klib, LPCWSTR strName, LPCWSTR strValue, LPWSTR pstrError, long dwErrorStringSize);
typedef BOOL (* PFN_KITLGetTranInfo) (KITLTRANLIB klib, PTRANPORTINFOSTRUCT pInfo);
typedef BOOL (* PFN_KITLAddNewTransportDll) (LPCWSTR pszDllName, LPCWSTR pszDesc);

typedef void (* PFN_OUTPUTSTRING) (LPCWSTR pszStr, LPVOID pUserData);
typedef void (* PFN_KITLSetDebug) (DWORD dwZones, PFN_OUTPUTSTRING pfnDbgMsg, LPVOID pUserData);

static HINSTANCE g_hKITLInstance = NULL;

// from kitlclnt.h
static PFN_KITLInitLibrary g_pfnKITLInitLibrary = NULL;
static PFN_KITLDeInitLibrary g_pfnKITLDeInitLibrary = NULL;
static PFN_KITLRegisterClient g_pfnKITLRegisterClient = NULL;
static PFN_KITLDeRegisterClient g_pfnKITLDeRegisterClient = NULL;
static PFN_KITLSend g_pfnKITLSend = NULL;
static PFN_KITLRecv g_pfnKITLRecv = NULL;
static PFN_KITLWaitForSvcConnect g_pfnKITLWaitForSvcConnect = NULL;

// from kitldll.h
static PFN_KITLRegisterPPSHService g_pfnKITLRegisterPPSHService = NULL;
static PFN_KITLRegisterDBGMSGService g_pfnKITLRegisterDBGMSGService = NULL;
static PFN_KITLLoadTransportLib g_pfnKITLLoadTransportLib = NULL;
static PFN_KITLUnloadTransportLib g_pfnKITLUnloadTransportLib = NULL;
static PFN_KITLCreateServer g_pfnKITLCreateServer = NULL;
static PFN_KITLDeleteServer g_pfnKITLDeleteServer = NULL;
static PFN_KITLSetRegisterCallback g_pfnKITLSetRegisterCallback = NULL;
static PFN_KITLSetNewSvcCallback g_pfnKITLSetNewSvcCallback = NULL;
static PFN_KITLSetOSStartCB g_pfnKITLSetOSStartCB = NULL;
static PFN_KITLSetNameSvcCB g_pfnKITLSetNameSvcCB = NULL;
static PFN_KITLResetDevice g_pfnKITLResetDevice = NULL;
static PFN_KITLDebugBreak g_pfnKITLDebugBreak = NULL;
static PFN_KITLEnumKnownDevice g_pfnKITLEnumKnownDevice = NULL;
static PFN_KITLGetBootFlags g_pfnKITLGetBootFlags = NULL;
static PFN_KITLSetBootFlags g_pfnKITLSetBootFlags = NULL;
static PFN_KITLGetCpuId g_pfnKITLGetCpuId = NULL;
static PFN_KITLEnumTransports g_pfnKITLEnumTransports = NULL;
static PFN_KITLGetTranXMLParams g_pfnKITLGetTranXMLParams = NULL;
static PFN_KITLSetTranParam g_pfnKITLSetTranParam = NULL;
static PFN_KITLGetTranInfo g_pfnKITLGetTranInfo = NULL;
static PFN_KITLAddNewTransportDll g_pfnKITLAddNewTransportDll = NULL;
static PFN_KITLSetDebug g_pfnKITLSetDebug = NULL;


//////////////////////////////////
// global initializations
//
BOOL KITLInitLibrary (LPCWSTR pszRegKeyRoot)
{
    BOOL bRet = FALSE;

    WCHAR wszKITL_DLL[] = L"KITLDLL.DLL";
    WCHAR wszPBHive[MAX_PATH] = L"SOFTWARE\\Microsoft\\Platform Builder\\7.00\\Directories";
    WCHAR wszPBInstallDirHiveOld[] = L"Install Dir";  // This is used prior to build 2378
    WCHAR wszPBInstallDirHive[] = L"IDE Install Dir"; // This is used for build 2378+
    WCHAR wszRelativePath[] = L"CoreCon\\BIN";
    WCHAR wzPBInstallPath[MAX_PATH]=L"";
    WCHAR wszKITLDllPath[MAX_PATH] = L"";
    WCHAR wszKITDllFullPath[MAX_PATH]=L"";
    DWORD dwBuffSize = 0;
    DWORD dwType = 0;
    
    HKEY hKey = NULL;
    LONG lRet = 0;


    // Check if KITL is on the default folder, if that is the case, use it
    g_hKITLInstance = LoadLibrary(wszKITL_DLL);
    if (NULL == g_hKITLInstance)
    {
        // Attempt to locate KITL dll
        lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
            wszPBHive, 
            0, 
            KEY_READ, 
            &hKey);
        if (ERROR_SUCCESS != lRet)
        {
            return FALSE;
        }

        dwBuffSize = sizeof(wzPBInstallPath)/sizeof(wzPBInstallPath[0]);

        // Try Query for the new value first assume it is using the newer PB
        lRet = RegQueryValueEx(hKey,
            wszPBInstallDirHive,
            NULL, 
            &dwType,
            reinterpret_cast<LPBYTE> (wzPBInstallPath),
            &dwBuffSize);

        if (ERROR_SUCCESS != lRet) 
        {
            // Try Query for the old value 
            lRet = RegQueryValueEx(hKey,
                wszPBInstallDirHiveOld,
                NULL, 
                &dwType,
                reinterpret_cast<LPBYTE> (wzPBInstallPath),
                &dwBuffSize);
        }

        CloseHandle(hKey);
        if (ERROR_SUCCESS != lRet)
        {
            return false;
        }

        if (REG_SZ != dwType)
        {
            return false;
        }

        wsprintf(wszKITLDllPath, L"%s\\%s",wzPBInstallPath , wszRelativePath);
        wsprintf(wszKITDllFullPath, L"%s\\%s", wszKITLDllPath, wszKITL_DLL);
        

        // is the libaray already loaded?
        if(NULL != g_hKITLInstance) 
            return FALSE;

        bRet = SetDllDirectory(wszKITLDllPath);
        if (!bRet)
        {
            return FALSE;
        }

        // load the KITL library
        g_hKITLInstance = LoadLibrary(wszKITDllFullPath);
        if( NULL == g_hKITLInstance)
            return FALSE;
    }
    
    // get all the KITL proc addresses
    g_pfnKITLInitLibrary = (PFN_KITLInitLibrary)GetProcAddress(g_hKITLInstance, "KITLInitLibrary");
    g_pfnKITLDeInitLibrary = (PFN_KITLDeInitLibrary)GetProcAddress(g_hKITLInstance, "KITLDeInitLibrary");
    g_pfnKITLRegisterClient = (PFN_KITLRegisterClient)GetProcAddress(g_hKITLInstance, "KITLRegisterClient");
    g_pfnKITLDeRegisterClient = (PFN_KITLDeRegisterClient)GetProcAddress(g_hKITLInstance, "KITLDeRegisterClient");
    g_pfnKITLSend = (PFN_KITLSend)GetProcAddress(g_hKITLInstance, "KITLSend");
    g_pfnKITLRecv = (PFN_KITLRecv)GetProcAddress(g_hKITLInstance, "KITLRecv");
    g_pfnKITLWaitForSvcConnect = (PFN_KITLWaitForSvcConnect)GetProcAddress(g_hKITLInstance, "KITLWaitForSvcConnect");

    g_pfnKITLRegisterPPSHService = (PFN_KITLRegisterPPSHService)GetProcAddress(g_hKITLInstance, "KITLRegisterPPSHService");
    g_pfnKITLRegisterDBGMSGService = (PFN_KITLRegisterDBGMSGService)GetProcAddress(g_hKITLInstance, "KITLRegisterDBGMSGService");
    g_pfnKITLLoadTransportLib = (PFN_KITLLoadTransportLib)GetProcAddress(g_hKITLInstance, "KITLLoadTransportLib");
    g_pfnKITLUnloadTransportLib = (PFN_KITLUnloadTransportLib)GetProcAddress(g_hKITLInstance, "KITLUnloadTransportLib");
    g_pfnKITLCreateServer = (PFN_KITLCreateServer)GetProcAddress(g_hKITLInstance, "KITLCreateServer");
    g_pfnKITLDeleteServer = (PFN_KITLDeleteServer)GetProcAddress(g_hKITLInstance, "KITLDeleteServer");
    g_pfnKITLSetRegisterCallback = (PFN_KITLSetRegisterCallback)GetProcAddress(g_hKITLInstance, "KITLSetRegisterCallback");
    g_pfnKITLSetNewSvcCallback = (PFN_KITLSetNewSvcCallback)GetProcAddress(g_hKITLInstance, "KITLSetNewSvcCallback");
    g_pfnKITLSetOSStartCB = (PFN_KITLSetOSStartCB)GetProcAddress(g_hKITLInstance, "KITLSetOSStartCB");
    g_pfnKITLSetNameSvcCB = (PFN_KITLSetNameSvcCB)GetProcAddress(g_hKITLInstance, "KITLSetNameSvcCB");
    g_pfnKITLResetDevice = (PFN_KITLResetDevice)GetProcAddress(g_hKITLInstance, "KITLResetDevice");
    g_pfnKITLDebugBreak = (PFN_KITLDebugBreak)GetProcAddress(g_hKITLInstance, "KITLDebugBreak");
    g_pfnKITLEnumKnownDevice = (PFN_KITLEnumKnownDevice)GetProcAddress(g_hKITLInstance, "KITLEnumKnownDevice");
    g_pfnKITLGetBootFlags = (PFN_KITLGetBootFlags)GetProcAddress(g_hKITLInstance, "KITLGetBootFlags");
    g_pfnKITLSetBootFlags = (PFN_KITLSetBootFlags)GetProcAddress(g_hKITLInstance, "KITLSetBootFlags");
    g_pfnKITLGetCpuId = (PFN_KITLGetCpuId)GetProcAddress(g_hKITLInstance, "KITLGetCPUId");
    g_pfnKITLEnumTransports = (PFN_KITLEnumTransports)GetProcAddress(g_hKITLInstance, "KITLEnumTransports");
    g_pfnKITLGetTranXMLParams = (PFN_KITLGetTranXMLParams)GetProcAddress(g_hKITLInstance, "KITLGetTranXMLParams");
    g_pfnKITLSetTranParam = (PFN_KITLSetTranParam)GetProcAddress(g_hKITLInstance, "KITLSetTranParam");
    g_pfnKITLGetTranInfo = (PFN_KITLGetTranInfo)GetProcAddress(g_hKITLInstance, "KITLGetTranInfo");
    g_pfnKITLAddNewTransportDll = (PFN_KITLAddNewTransportDll)GetProcAddress(g_hKITLInstance, "KITLAddNewTransportDll");
    g_pfnKITLSetDebug = (PFN_KITLSetDebug)GetProcAddress(g_hKITLInstance, "KITLSetDebug");

    // grabbed all the KITL exports, now call to actually init the library
    if(g_pfnKITLInitLibrary)
        return (g_pfnKITLInitLibrary)(pszRegKeyRoot);
    else
        return FALSE;
}

BOOL KITLDeInitLibrary (void)
{
    if(g_pfnKITLDeInitLibrary && (g_pfnKITLDeInitLibrary)() && g_hKITLInstance) {
    
        FreeLibrary(g_hKITLInstance);
        g_hKITLInstance = NULL;
        return TRUE;
    } 
    else
        return FALSE;
}

//////////////////////////////////
// KITL client functions
//

// client registration/de-registration
KITLID KITLRegisterClient (LPCWSTR pszDevName, LPCWSTR pszSvcName, DWORD dwTimeout, DWORD dwFlags)
{
    if(g_pfnKITLRegisterClient)
        return (g_pfnKITLRegisterClient)(pszDevName, pszSvcName, dwTimeout, dwFlags);
    else
        return INVALID_KITLID;
}

BOOL KITLDeRegisterClient (KITLID id)
{
    if(g_pfnKITLDeRegisterClient)
        return (g_pfnKITLDeRegisterClient)(id);
    else
        return FALSE;
}

// send/recv data
BOOL KITLSend (KITLID id, LPCVOID pData, DWORD cbData, DWORD dwTimeout)
{
    if(g_pfnKITLSend)
        return (g_pfnKITLSend)(id, pData, cbData, dwTimeout);
    else
        return FALSE;
}

BOOL KITLRecv (KITLID id, LPVOID pBuffer, DWORD *pcbBuffer, DWORD dwTimeout)
{
    if(g_pfnKITLRecv)
        return (g_pfnKITLRecv)(id, pBuffer, pcbBuffer, dwTimeout);
    else
        return FALSE;
}


// config related
BOOL KITLWaitForSvcConnect (KITLID id, DWORD dwTimeout)
{
    if(g_pfnKITLWaitForSvcConnect)
        return (g_pfnKITLWaitForSvcConnect)(id, dwTimeout);
    else
        return FALSE;
}

KITLID KITLRegisterPPSHService (LPCWSTR pszDevName, PFN_GETINPUT pfnInput, PFN_CMDOUTPUT pfnOutput, PFN_OPENFILECB pfnOfnCB, LPVOID pUserData)
{
    if(g_pfnKITLRegisterPPSHService)
        return (g_pfnKITLRegisterPPSHService)(pszDevName,  pfnInput, pfnOutput, pfnOfnCB, pUserData);
    else
        return INVALID_KITLID;
}

KITLID KITLRegisterDBGMSGService (LPCWSTR pszDevName, PFN_MSGOUTPUT pfnOutput, LPVOID pUserData)
{
    if(g_pfnKITLRegisterDBGMSGService)
        return (g_pfnKITLRegisterDBGMSGService)(pszDevName, pfnOutput, pUserData);
    else
        return INVALID_KITLID;
}

//////////////////////////////////
// server side functions
//

// load/unload transport library
KITLTRANLIB KITLLoadTransportLib (LPCWSTR pszDllName)
{
    if(g_pfnKITLLoadTransportLib) 
        return (g_pfnKITLLoadTransportLib)(pszDllName);
    else
        return NULL;
}


BOOL KITLUnloadTransportLib (KITLTRANLIB klib)
{
    if(g_pfnKITLUnloadTransportLib) 
        return (g_pfnKITLUnloadTransportLib)(klib);
    else 
        return FALSE;
}
// create/delete KITL server
KITLID KITLCreateServer (KITLTRANLIB klib, LPCWSTR pszDevName)
{
    if(g_pfnKITLCreateServer) 
        return (g_pfnKITLCreateServer)(klib, pszDevName);
    else
        return FALSE;
}

BOOL KITLDeleteServer (KITLID id)
{
    if(g_pfnKITLDeleteServer)
        return (g_pfnKITLDeleteServer)(id);
    else
        return FALSE;
}

// callback for client registration
BOOL KITLSetRegisterCallback (KITLID id, PFN_REGCB pfnCB, LPVOID pUserData)
{
    if(g_pfnKITLSetRegisterCallback) 
        return (g_pfnKITLSetRegisterCallback)(id, pfnCB, pUserData);
    else
        return FALSE;
}

// callback when device asked for a service not registered
BOOL KITLSetNewSvcCallback (KITLID id, PFN_NEWSVCCB pfnCB, LPVOID pUserData)
{
    if(g_pfnKITLSetNewSvcCallback)
        return (g_pfnKITLSetNewSvcCallback)(id, pfnCB, pUserData);
    else
        return FALSE;

}

// callback when OS started
BOOL KITLSetOSStartCB (KITLID id, PFN_OSSTARTCB pfnCB, LPVOID pUserData)
{
    if(g_pfnKITLSetOSStartCB)
        return (g_pfnKITLSetOSStartCB)(id, pfnCB, pUserData);
    else
        return FALSE;
}

// callback to provide name service
BOOL KITLSetNameSvcCB (KITLID id, PFN_NAMESVCCB pfnCB, LPVOID pUserData)
{
    if(g_pfnKITLSetNameSvcCB)
        return (g_pfnKITLSetNameSvcCB)(id, pfnCB, pUserData);
    else
        return FALSE;
}

// the following two function is a 'one-shot' command (can't expect ACK from 
// the device). The device might not reset/break even if the command succeed.
// reset device
BOOL KITLResetDevice (KITLID id, BOOL fCleanBoot)
{
    if(g_pfnKITLResetDevice)
        return (g_pfnKITLResetDevice)(id, fCleanBoot);
    else
        return FALSE;
}

// debug break
BOOL KITLDebugBreak (KITLID id)
{
    if(g_pfnKITLDebugBreak)
        return (g_pfnKITLDebugBreak)(id);
    else
        return FALSE;
}

// bypass functions for the transport
BOOL KITLEnumKnownDevice (KITLTRANLIB klib, PFN_EnumProc pfnCB, LPVOID pUserData)
{
    if(g_pfnKITLEnumKnownDevice)
        return (g_pfnKITLEnumKnownDevice)(klib, pfnCB, pUserData);
    else
        return FALSE;
}

DWORD KITLGetBootFlags (KITLID id)
{
    if(g_pfnKITLGetBootFlags)
        return (g_pfnKITLGetBootFlags)(id);
    else
        return 0;
}

void KITLSetBootFlags (KITLID id, DWORD dwBootFlags)
{
    if(g_pfnKITLSetBootFlags)
        (g_pfnKITLSetBootFlags)(id, dwBootFlags);
}

DWORD KITLGetCpuId (KITLID id)
{
    if(g_pfnKITLGetCpuId)
        return (g_pfnKITLGetCpuId)(id);
    else
        return 0;
}

////////////////////////////////////
// helper functions

// get all available transport dll names.
// string is NULL seperated, with the last name terminated by 2 NULLs
BOOL KITLEnumTransports (PFN_ENUMTRAN pfnEnum, LPVOID pUserData)
{
    if(g_pfnKITLEnumTransports)
        return (g_pfnKITLEnumTransports)(pfnEnum, pUserData);
    else
        return FALSE;
}

BOOL KITLGetTranXMLParams (KITLTRANLIB klib, LPCWSTR *ppstrXML)
{
    if(g_pfnKITLGetTranXMLParams)
        return (g_pfnKITLGetTranXMLParams)(klib, ppstrXML);
    else
        return FALSE;
}

BOOL KITLSetTranParam (KITLTRANLIB klib, LPCWSTR strName, LPCWSTR strValue, LPWSTR pstrError, long dwErrorStringSize)
{
    if(g_pfnKITLSetTranParam)
        return (g_pfnKITLSetTranParam)(klib, strName, strValue, pstrError, dwErrorStringSize);
    else
        return FALSE;
}

BOOL KITLGetTranInfo (KITLTRANLIB klib, PTRANPORTINFOSTRUCT pInfo)
{
    if(g_pfnKITLGetTranInfo)
        return (g_pfnKITLGetTranInfo)(klib, pInfo);
    else
        return FALSE;
}

// function to call for setup program to add a new DLL to KITL registry.
// (You can also deal with registry yourself, but I think this is cleaner.)
BOOL KITLAddNewTransportDll (LPCWSTR pszDllName, LPCWSTR pszDesc)
{   
    if(g_pfnKITLAddNewTransportDll) 
        return (g_pfnKITLAddNewTransportDll)(pszDllName, pszDesc);
    else
        return FALSE;
}

// debug support
void KITLSetDebug (DWORD dwZones, PFN_OUTPUTSTRING pfnDbgMsg, LPVOID pUserData)
{
    if(g_pfnKITLSetDebug)
        (g_pfnKITLSetDebug)(dwZones, pfnDbgMsg, pUserData);
}



