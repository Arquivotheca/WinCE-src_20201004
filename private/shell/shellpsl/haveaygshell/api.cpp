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
/*---------------------------------------------------------------------------*\
 *  module: api.cpp (old explorer.cpp)
\*---------------------------------------------------------------------------*/
#include "api.h"
#include <aygshell.h>

#define NUM_APIS    74
HANDLE hAPISet;

void ShellApiReservedForNK (void);
extern "C" void ShellNotifyCallback(DWORD cause, DWORD proc, DWORD thread);
extern "C" BOOL WINAPI NotSystemParametersInfo( UINT uiAction, UINT uiParam, PVOID pvParam, UINT cbpvParam, UINT fWinIni );

#undef SHCreateExplorerInstance
BOOL WINAPI SHCreateExplorerInstance(LPCTSTR pszPath, UINT uFlags);

void WINAPI SHAddToRecentDocsI(UINT uFlags, LPCVOID pv, DWORD cb);
BOOL WINAPI Shell_NotifyIconI(DWORD dwMsg, PNOTIFYICONDATA pNID, DWORD cbNID);
BOOL WINAPI SHCloseAppsI( DWORD dwMemSought );

LRESULT WINAPI SHNotificationAddII( SHNOTIFICATIONDATA *pndAdd, DWORD cbData, LPTSTR pszTitle, LPTSTR pszHTML );
LRESULT WINAPI SHNotificationUpdateII(DWORD grnumUpdateMask, SHNOTIFICATIONDATA *pndNew, DWORD cbData, LPTSTR pszTitle, LPTSTR pszHTML);
LRESULT WINAPI SHNotificationRemoveII(const CLSID *pclsid, DWORD cbCLSID, DWORD dwID);
LRESULT WINAPI SHNotificationGetDataII(const CLSID *pclsid, DWORD cbCLSID,DWORD dwID, SHNOTIFICATIONDATA *pndBuffer, DWORD cbBuffer,LPTSTR pszTitle, DWORD cbTitle, LPTSTR pszHTML, DWORD cbHTML,DWORD *pdwHTMLLength);

#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("Explorer"), {
    TEXT("Init"),TEXT(""),TEXT(""),TEXT(""),
    TEXT(""),TEXT(""),TEXT(""),TEXT(""),
    TEXT(""),TEXT("DST"),TEXT("Interface"),TEXT("Misc"),
    TEXT("Alloc"),TEXT("Function"),TEXT("Warning"),TEXT("Error") },
    0
};
#endif  // DEBUG

BOOL PlaceHolder(DWORD dw)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    RETAILMSG(1, (TEXT("UNIMPLEMENTED SHELL API called.\r\n")));
    return FALSE;
} /* PlaceHolder()
   */


const PFNVOID VTable[NUM_APIS] = {
//xref ApiSetStart
    (PFNVOID) ShellNotifyCallback,
    (PFNVOID) ShellApiReservedForNK, /* Reserved for NK */
    (PFNVOID) PlaceHolder,
    (PFNVOID) PlaceHolder/*XGetOpenFileName*/,
    (PFNVOID) PlaceHolder/*XGetSaveFileName*/,
    (PFNVOID) PlaceHolder/*XSHGetFileInfo*/,
    (PFNVOID) Shell_NotifyIconI,
    (PFNVOID) PlaceHolder, /* SHAddToRecentDocs */
    (PFNVOID) PlaceHolder/*SHCreateShortcut*/,
    (PFNVOID) SHCreateExplorerInstance,
    (PFNVOID) PlaceHolder, //internalSHRemoveFontResource,
    (PFNVOID) PlaceHolder,
    (PFNVOID) PlaceHolder,
    (PFNVOID) PlaceHolder,
    (PFNVOID) PlaceHolder,
    (PFNVOID) PlaceHolder,
    (PFNVOID) PlaceHolder,
    (PFNVOID) PlaceHolder,
    (PFNVOID) PlaceHolder/*SHGetShortcutTarget*/,
    (PFNVOID) PlaceHolder,
    (PFNVOID) PlaceHolder,
    (PFNVOID) PlaceHolder/*xxx_SHLoadDIBitmap*/,
    (PFNVOID) PlaceHolder/*SHSetDesktopPosition*/,
    (PFNVOID) PlaceHolder,
    (PFNVOID) PlaceHolder, //24
    (PFNVOID) PlaceHolder, //25
    (PFNVOID) PlaceHolder, //26
    (PFNVOID) NotSystemParametersInfo, 
    (PFNVOID) SHGetAppKeyAssoc,
    (PFNVOID) SHSetAppKeyWndAssoc,
    (PFNVOID) PlaceHolder, // 30
    (PFNVOID) PlaceHolder, //31
    (PFNVOID) PlaceHolder, //32
    (PFNVOID) PlaceHolder, //33
    (PFNVOID) PlaceHolder, //34
    (PFNVOID) PlaceHolder, //35
    (PFNVOID) PlaceHolder, //36
    (PFNVOID) SHCloseAppsI,
    (PFNVOID) PlaceHolder,
    (PFNVOID) PlaceHolder, //39
    (PFNVOID) PlaceHolder, //40
    (PFNVOID) SHSetNavBarText,
    (PFNVOID) PlaceHolder, // used to be SHDoneButton
    (PFNVOID) PlaceHolder, //43
    (PFNVOID) PlaceHolder, //44
    (PFNVOID) PlaceHolder, //45
    (PFNVOID) PlaceHolder, //46
    (PFNVOID) PlaceHolder, //47
    (PFNVOID) PlaceHolder, //48
    (PFNVOID) PlaceHolder, //49
    (PFNVOID) PlaceHolder, //50
    (PFNVOID) PlaceHolder, //51
    (PFNVOID) PlaceHolder, //52
    (PFNVOID) PlaceHolder, //53
    (PFNVOID) PlaceHolder, //54
    (PFNVOID) SHNotificationAddII,
    (PFNVOID) SHNotificationUpdateII,
    (PFNVOID) SHNotificationRemoveII,
    (PFNVOID) SHNotificationGetDataII, //58
    (PFNVOID) PlaceHolder, //59
    (PFNVOID) PlaceHolder, //60
    (PFNVOID) PlaceHolder, //61
    (PFNVOID) PlaceHolder, //62
    (PFNVOID) PlaceHolder, //63
    (PFNVOID) PlaceHolder, //64
    (PFNVOID) PlaceHolder, //65
    (PFNVOID) PlaceHolder, //66
    (PFNVOID) PlaceHolder, //67
    (PFNVOID) PlaceHolder, //68
    (PFNVOID) PlaceHolder, //69
    (PFNVOID) PlaceHolder, //70
    (PFNVOID) PlaceHolder, //71
    (PFNVOID) PlaceHolder, //72
    (PFNVOID) PlaceHolder, // 73 - new and internal for mckendrick
//xref ApiSetEnd
};


const ULONGLONG SigTable[NUM_APIS] = {
    FNSIG3(DW,DW,DW), // ShellNotifyCallback
    FNSIG0(),  //ShellApiReservedForNK
    FNSIG1(DW), // PlaceHolder (was: ShellExecuteEx)
    FNSIG1(DW)/*FNSIG1(PTR)*/, //GetOpenFileName
    FNSIG1(DW)/*FNSIG1(PTR)*/, //GetSaveFileName
    FNSIG1(DW), // xxx_SHGetFileInfo
    FNSIG3(DW,I_PTR, DW), // Shell_NotifyIcon
    FNSIG1(DW), // SHAddToRecentDocs
    FNSIG1(DW)/*FNSIG2(PTR,PTR)*/, // SHCreateShortcut
    FNSIG2(I_WSTR,DW), // SHCreateExplorerInstance
    FNSIG1(DW), // SHRemoveFontResource
    FNSIG1(DW), // PlaceHolder
    FNSIG1(DW), // PlaceHolder
    FNSIG1(DW), // PlaceHolder
    FNSIG1(DW), // PlaceHolder
    FNSIG1(DW), // PlaceHolder
    FNSIG1(DW), // PlaceHolder
    FNSIG1(DW), // PlaceHolder
    FNSIG1(DW)/*FNSIG3(PTR, PTR, DW)*/, // SHGetShortcutTarget
    FNSIG1(DW), // PlaceHolder
    FNSIG1(DW), // PlaceHolder
    FNSIG1(DW)/*FNSIG1(PTR)*/, // SHLoadDIBitmap
    FNSIG1(DW)/*FNSIG3(DW, DW, DW)*/, // SHSetDesktopPosition
    FNSIG1(DW), // SHFileChange
    FNSIG1(DW), // PlaceHolder
    FNSIG1(DW), // PlaceHolder
    FNSIG1(DW), // PlaceHolder
    FNSIG5(DW,DW,IO_PTR,DW,DW), //NotSystemParametersInfo
    FNSIG1(I_WSTR), // SHGetAppKeyAssoc
    FNSIG2(DW,DW), // SHSetAppKeyWndAssoc
    FNSIG1(DW), // PlaceHolder  30
    FNSIG1(DW), // PlaceHolder  31
    FNSIG1(DW), // PlaceHolder  32
    FNSIG1(DW), // PlaceHolder  33
    FNSIG1(DW), // PlaceHolder, was SHFileNotifyRemove
    FNSIG1(DW), // PlaceHolder, was SHFileNotifyFree
    FNSIG1(DW), // PlaceHolder  36
    FNSIG1(DW), // SHCloseApps
    FNSIG1(DW), // PlaceHolder  38
    FNSIG1(DW), // PlaceHolder  39
    FNSIG1(DW), // PlaceHolder  40
    FNSIG2(DW,I_WSTR), // SHSetNavBarText
    FNSIG1(DW), // PlaceHodler, used to be SHDoneButton
    FNSIG1(DW), // PlaceHolder  43
    FNSIG1(DW), // PlaceHolder  44
    FNSIG1(DW), // PlaceHolder  45
    FNSIG1(DW), // PlaceHolder  46
    FNSIG1(DW), // PlaceHolder  47
    FNSIG1(DW), // PlaceHolder  48
    FNSIG1(DW), // PlaceHolder  49
    FNSIG1(DW), // PlaceHolder, was SHChangeNotifyRegister
    FNSIG1(DW), // PlaceHolder  51
    FNSIG1(DW), // PlaceHolder  52
    FNSIG1(DW), // PlaceHolder  53
    FNSIG1(DW), // PlaceHolder  54
    FNSIG4(I_PTR,DW,I_WSTR,I_WSTR), // SHNotificationAdd
    FNSIG5(DW,I_PTR,DW,I_WSTR,I_WSTR), //SHNotificationUpdate
    FNSIG3(I_PTR,DW,DW), //SHNotificationRemove
    FNSIG10(I_PTR,DW,DW,O_PTR,DW,O_PTR,DW,O_PTR,DW,O_PDW), //SHNotificationGetData
    FNSIG1(DW), // PlaceHolder  59
    FNSIG1(DW), // PlaceHolder  60
    FNSIG1(DW), // PlaceHolder  61
    FNSIG1(DW), // PlaceHolder  62
    FNSIG1(DW), // PlaceHolder  63
    FNSIG1(DW), // PlaceHolder  64
    FNSIG1(DW), // PlaceHolder  65
    FNSIG1(DW), // PlaceHolder  66
    FNSIG1(DW), // PlaceHolder  67
    FNSIG1(DW), // PlaceHolder  68
    FNSIG1(DW), // PlaceHolder  69
    FNSIG1(DW), // PlaceHolder  70
    FNSIG1(DW), // PlaceHolder  71
    FNSIG1(DW), // PlaceHolder  72
    FNSIG1(DW), // PlaceHolder, was SendChangeNotificationToWindowI
};

void ShellApiReservedForNK(void)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    ASSERT( 0 ); // This should never be called.
} /* ShellApiReservedForNK()
   */


extern "C" VOID RegisterShellAPIs(VOID)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    BOOL bResult;

    hAPISet = CreateAPISet((char*)"SHEL", NUM_APIS, VTable, SigTable);
    bResult = RegisterAPISet(hAPISet, SH_SHELL);

} /* RegisterShellAPIs()
   */

extern "C" VOID UnRegisterShellAPIs(VOID)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    CloseHandle(hAPISet);
//  ShellNotifyCallback(DLL_PROCESS_DETACH, GetCurrentProcessId(), 0);
}

extern "C" void ShellNotifyCallback(DWORD cause, DWORD proc, DWORD thread)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    if (DLL_MEMORY_LOW == cause) {
        CompactAllHeaps ();
        return;
    }
    if (cause == DLL_PROCESS_DETACH)
    {
        if (proc == GetCurrentProcessId()) {
            RETAILMSG(TRUE, (TEXT("Explorer(V2.0) attempting restart.\r\n")));      

            CloseHandle(hAPISet);
            RegisterTaskBar(NULL);
            //EDList_Uninitialize();            
            CreateProcess(TEXT("explorer.exe"), NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, NULL);
        }
    }

} /* ShellNotifyCallback()
   */
