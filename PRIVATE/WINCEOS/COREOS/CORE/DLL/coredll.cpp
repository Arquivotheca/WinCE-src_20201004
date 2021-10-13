//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <coredll.h>
#include <KmodeEntries.hpp>

extern "C" VOID WINAPI WinRegInit(HANDLE  hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
extern "C" BOOL WINAPI _CRTDLL_INIT(HANDLE hinstDll, DWORD dwReason, LPVOID lpreserved);
extern "C" void LMemDeInit (void);
extern "C" void InitLocale(void);


extern "C" LPVOID *Win32Methods;
LPVOID *Win32Methods;

KmodeEntries_t*	g_pKmodeEntries;


HANDLE hInstCoreDll;



#ifdef DEBUG
DBGPARAM dpCurSettings = { TEXT("Coredll"), {
    TEXT("FixHeap"),    TEXT("LocalMem"),  TEXT("Mov"),       TEXT("SmallBlock"),
        TEXT("VirtMem"),    TEXT("Devices"),   TEXT("Undefined"), TEXT("Undefined"),
        TEXT("Stdio"),   TEXT("Stdio HiFreq"), TEXT("Shell APIs"), TEXT("Imm"),
        TEXT("Heap Validate"),  TEXT("RemoteMem"), TEXT("VerboseHeap"), TEXT("Undefined") },
        0x00000000 };
#endif


//TODO:
//mbstowcs and wcstombs will need to be modified in future to
//handle multibyte strings; currently, they will only handle single byte
//strings.

size_t mbstowcs(wchar_t *wcstr, const char *mbstr, size_t count) {
        int     RetVal;
        const char* mbtemp = mbstr;

        // VBUG #4507, #4879 - count of 0 should return 0.  Copied from VC98 CRTs - mbstowcs.c
        if (wcstr && count == 0)
            /* dest string exists, but 0 bytes converted */
            return (size_t) 0;

        if (NULL == wcstr) {
            // Determine how many characters are required
            RetVal = MultiByteToWideChar(CP_ACP, 0, mbstr, -1, NULL, 0);
            if (0 == RetVal) {
                RetVal = -1;
            } else {
                // MultiByteToWideChar includes the terminator.
                RetVal = RetVal--;
            }
            return RetVal;
        }

        //We cannot rely on strlen to tell us the size of mbstr, since it does not have to be null terminated.
        //It's better to search only until we get count characters.

        if(count <= 0)
            return 0;

        while (*mbtemp && (size_t) (mbtemp-mbstr) < count)
            mbtemp++;

        RetVal = MultiByteToWideChar(CP_ACP, 0, mbstr,  ((size_t) (mbtemp-mbstr) < count) ? -1 : count, wcstr, count);

        // Fix up return code.  MultiByteToWideChar returns 0 on error
        // mbstowcs should return -1.
        if ((0 == RetVal) && GetLastError()) {
            RetVal = -1;
        } else if (RetVal && (TEXT('\0') == wcstr[RetVal - 1])) {
            // MultiByteToWideChar returned length includes the null.  mbstowcs does not
            RetVal--;
        }
        return (size_t)RetVal;
}

size_t wcstombs(char *mbstr, const wchar_t *wcstr, size_t count) {
        int     RetVal;
        const wchar_t* wctemp = wcstr;

        // VBUG #4507, #4879 - count of 0 should return 0.  Copied from VC98 CRTs - wcstombs.c
        if (mbstr && count == 0)
            /* dest string exists, but 0 bytes converted */
            return (size_t) 0;

        if (NULL == mbstr) {
                RetVal = WideCharToMultiByte(CP_ACP, 0, wcstr, -1, NULL, 0, NULL, NULL);
                if (0 == RetVal) {
                        RetVal = -1;
                } else {
                        RetVal--;
                }
                return RetVal;
        }

        if(count <= 0)
            return 0;

        while (*wctemp && (size_t) (wctemp-wcstr) < count)
            wctemp++;

        RetVal = WideCharToMultiByte(CP_ACP, 0, wcstr, ((size_t) (wctemp-wcstr) < count) ? -1 : count , mbstr, count, NULL, NULL);

        // Fix up return code.  WideCharToMultiByte returns 0 on error
        // wcstombs should return -1.
        if ((0 == RetVal) && GetLastError()) {
                RetVal = -1;
        } else if (RetVal && ('\0' == mbstr[RetVal - 1])) {
                // WideCharToMultiByte returned length includes the null.  wcstombs does not
                RetVal--;
        }
        return (size_t)RetVal;
}

#ifndef _CRTBLD // A testing CRT build cannot build CoreDllInit()
#include <..\..\gwe\inc\dlgmgr.h>

extern "C"
void
RegisterDlgClass(
	void
	)
{
	BOOL IsAPIReady(DWORD hAPI);

	if ( IsAPIReady(SH_WMGR) )
		{
		WNDCLASS	 wc;
		wc.style         = 0;
		wc.lpfnWndProc   = DefDlgProcW;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = sizeof(DLG);
		wc.hInstance     = (HINSTANCE)hInstCoreDll;
		wc.hIcon         = NULL;
		wc.hCursor       = NULL;
		wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
		wc.lpszMenuName  = NULL;
		wc.lpszClassName = DIALOGCLASSNAME;
		RegisterClassW(&wc);
		}
}

extern "C"
BOOL WINAPI CoreDllInit (HANDLE  hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    BOOL bAllKMode;

    DEBUGCHK ((hinstDLL == hInstCoreDll) && GetCurrentProcessIndex ());

    //
    // NOTE: DLL_THREAD_ATTACH/DLL_THREAD_DETACH will NEVER be called for COREDLL.
    //
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        GetRomFileInfo(3,(LPWIN32_FIND_DATA)&Win32Methods,(DWORD)&bAllKMode);
        GetRomFileInfo(4,(LPWIN32_FIND_DATA)&g_pKmodeEntries, 0);
        DEBUGREGISTER((HINSTANCE)hinstDLL);
        if(!LMemInit())
            DEBUGCHK(0);
        InitLocale();

        break;
    case DLL_PROCESS_DETACH:
        LMemDeInit ();
        break;
    default:
        break;
    }
    _CRTDLL_INIT(hinstDLL,fdwReason,lpvReserved);
    WinRegInit(hinstDLL, fdwReason, lpvReserved);

    return(TRUE);
}

#endif // _CRTBLD






