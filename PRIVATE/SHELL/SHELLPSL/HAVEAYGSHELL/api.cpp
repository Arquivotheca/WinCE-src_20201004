//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
extern "C" BOOL WINAPI NotSystemParametersInfo( UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni );

#undef SHCreateExplorerInstance
BOOL WINAPI SHCreateExplorerInstance(LPCTSTR pszPath, UINT uFlags);

void WINAPI SHAddToRecentDocsI(UINT uFlags, LPCVOID pv);
BOOL WINAPI Shell_NotifyIconI(DWORD dwMsg, PNOTIFYICONDATA pNID);
BOOL WINAPI SHCloseAppsI( DWORD dwMemSought );

extern "C" BOOL SHFileNotifyRemoveII( HWND hwnd );
extern "C" void SHFileNotifyFreeII( FILECHANGENOTIFY *pfcn, HPROCESS proc );
extern "C" BOOL SHChangeNotifyRegisterII( HWND hwnd, LPCTSTR pszWatch, SHCHANGENOTIFYENTRY *pshcne, DWORD procID, LPCTSTR pszEventName, BOOL *pReplace );
BOOL InitializeChangeWatcher();
BOOL ReleaseChangeWatcher();
extern "C" BOOL SendChangeNotificationToWindowI( HPROCESS proc );

LRESULT WINAPI SHNotificationAddII( SHNOTIFICATIONDATA *pndAdd, LPTSTR pszTitle, LPTSTR pszHTML );
LRESULT WINAPI SHNotificationUpdateII(DWORD grnumUpdateMask, SHNOTIFICATIONDATA *pndNew, LPTSTR pszTitle, LPTSTR pszHTML);
LRESULT WINAPI SHNotificationRemoveII(const CLSID *pclsid, DWORD dwID);
LRESULT WINAPI SHNotificationGetDataII(const CLSID *pclsid, DWORD dwID, SHNOTIFICATIONDATA *pndBuffer, LPTSTR pszTitle, LPTSTR pszHTML, DWORD *pdwHTMLLength);

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
	RETAILMSG(1, (TEXT("UNIMPLEMENTED SHELL API called, Please report bug to ARULM!!\r\n")));
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
	(PFNVOID) SHAddToRecentDocsI,
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
    (PFNVOID) SHFileNotifyRemoveII, //34
    (PFNVOID) SHFileNotifyFreeII, //35
    (PFNVOID) PlaceHolder, //36
	(PFNVOID) SHCloseAppsI,
	(PFNVOID) SHSipPreference,
    (PFNVOID) PlaceHolder, //39
    (PFNVOID) PlaceHolder, //40
	(PFNVOID) SHSetNavBarText,
	(PFNVOID) SHDoneButton,
    (PFNVOID) PlaceHolder, //43
    (PFNVOID) PlaceHolder, //44
    (PFNVOID) PlaceHolder, //45
    (PFNVOID) PlaceHolder, //46
    (PFNVOID) PlaceHolder, //47
    (PFNVOID) PlaceHolder, //48
    (PFNVOID) PlaceHolder, //49
    (PFNVOID) SHChangeNotifyRegisterII, //50
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
    (PFNVOID) SendChangeNotificationToWindowI, // 73 - new and internal for mckendrick
//xref ApiSetEnd
};


const DWORD SigTable[NUM_APIS] = {
    FNSIG3(DW,DW,DW), // ShellNotifyCallback
    FNSIG0(),  //ShellApiReservedForNK
	FNSIG1(PTR), // PlaceHolder (was: ShellExecuteEx)
	FNSIG1(DW)/*FNSIG1(PTR)*/, //GetOpenFileName
	FNSIG1(DW)/*FNSIG1(PTR)*/, //GetSaveFileName
	FNSIG1(DW), // xxx_SHGetFileInfo
	FNSIG2(DW,PTR), // Shell_NotifyIcon
	FNSIG2(DW,PTR), // SHAddToRecentDocs
	FNSIG1(DW)/*FNSIG2(PTR,PTR)*/, // SHCreateShortcut
	FNSIG2(PTR,DW), // SHCreateExplorerInstance
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
	FNSIG4(DW,DW,PTR,DW), //NotSystemParametersInfo
	FNSIG1(PTR), // SHGetAppKeyAssoc
	FNSIG2(DW,DW), // SHSetAppKeyWndAssoc
	FNSIG1(DW), // PlaceHolder  30
	FNSIG1(DW), // PlaceHolder  31
	FNSIG1(DW), // PlaceHolder  32
    FNSIG1(DW), // PlaceHolder  33
	FNSIG1(DW), // SHFileNotifyRemove
	FNSIG2(PTR,DW), // SHFileNotifyFree
	FNSIG1(DW), // PlaceHolder  36
	FNSIG1(DW), // SHCloseApps
	FNSIG2(DW,DW), // SHSipPreference
	FNSIG1(DW), // PlaceHolder  39
	FNSIG1(DW), // PlaceHolder  40
	FNSIG2(DW,PTR), // SHSetNavBarText
	FNSIG2(DW,DW), // SHDoneButton
	FNSIG1(DW), // PlaceHolder  43
	FNSIG1(DW), // PlaceHolder  44
	FNSIG1(DW), // PlaceHolder  45
	FNSIG1(DW), // PlaceHolder  46
	FNSIG1(DW), // PlaceHolder  47
	FNSIG1(DW), // PlaceHolder  48
	FNSIG1(DW), // PlaceHolder  49
    FNSIG6(DW,PTR,PTR,DW,PTR,PTR), //SHChangeNotifyRegister
	FNSIG1(DW), // PlaceHolder  51
	FNSIG1(DW), // PlaceHolder  52
	FNSIG1(DW), // PlaceHolder  53
	FNSIG1(DW), // PlaceHolder  54
    FNSIG3(PTR,PTR,PTR), // SHNotificationAdd
	FNSIG4(DW,PTR,PTR,PTR), //SHNotificationUpdate
	FNSIG2(PTR,DW), //SHNotificationRemove
	FNSIG6(PTR,DW,PTR,PTR,PTR,PTR), //SHNotificationGetData
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
    FNSIG1(DW), // SendChangeNotificationToWindowI
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
    InitializeChangeWatcher();

} /* RegisterShellAPIs()
   */

extern "C" VOID UnRegisterShellAPIs(VOID)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
	CloseHandle(hAPISet);
    ReleaseChangeWatcher();
//	ShellNotifyCallback(DLL_PROCESS_DETACH, GetCurrentProcessId(), 0);
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
