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
// Copyright (c) 1999-2000 Microsoft Corporation
/* Custom Format Name Body */
#pragma warning(disable : 4200) // Disable "C4200: nonstandard extension used : zero-sized array in struct/union"
typedef struct tNameStore {
    unsigned short cbSize;          // SizeOf this structure
    TCHAR       achName[];          // The name
} NameStore, *PNameStore, FAR * LPNameStore;


#define NAMELEN(x) (((x)->cbSize-sizeof(NameStore))/sizeof(TCHAR))
#define STRING_LEN 128

typedef UINT (WINAPI *CHOOSEHOOKPROC)
(
    HWND                    hwnd,
    UINT                    uMsg,
    WPARAM                  wParam,
    LPARAM                  lParam
);

/*****************************************************************************
 * @doc INTERNAL
 * 
 * @types InstData | This stores global variables for a filter chooser
 * instance.  GetProp/SetProp will be used to assign this to a dialog.
 *
 * @field UINT | uType | Specifies the type of the instance data
 *
 * @field HWND | hwnd | Window handle of the dialog.
 *
 * @field HWND | hFormatTags | Window handle to the FormatTags dropdown listbox
 *     
 * @field HWND | hFormats | Window handle to the Formats dropdown listbox
 *     
 * @field UINT | uiFormatTab | Tabstop for the Formats dropdown listbox
 *
 * @field HWND | hCancel | Window handle to the Cancel button
 *
 * @field PNameStore | pnsTemp | Temporary string storage
 *
 * @field PNameStore | pnsStrOut | Temporary string storage
 *
 * @field LPACMFORMATCHOOSE | pcfmtc | Initialization structure
 * @field LPACMFILTERCHOOSE | pcfltrc | Initialization structure
 *
 * @field PACMGARB | pag | Pointer to the ACMGARB structure associated
 * with this instance of the ACM.
 *
 ****************************************************************************/

typedef struct tInstData {
    UINT            uType;
    MMRESULT        mmrSubFailure;   // Failure in an acm subfunction
    HWND            hwnd;
    HWND            hFormatTags;
    int             iPrevFormatTagsSel; // previous selection
    
    HWND            hFormats;
    UINT            uiFormatTab;
    
    HWND            hOk;
    HWND            hCancel;

    PNameStore      pnsTemp;    // Walk all over this.
    PNameStore      pnsStrOut;  // Another temporary NameStore
    HKEY            hkeyFormats;    // HKEY corresponding to key name.
    LPBYTE          lpbSel;         // return data
    DWORD           dwTag;          // Generic 'Tag'

    LPTSTR          pszName;        // Choice name buffer
    DWORD           cchName;         // Choice buffer length
    BOOL            fTagFilter;     // Filter for an explicit 'Tag'.

    UINT            uHelpMsg;   // Help button to parent
    
    UINT            cdwTags;          // count of tags
    DWORD *         pdwTags;        // pointer to array of tags
    UINT            cbwfxEnum;
    UINT            cbwfltrEnum;
    LPACMFORMATDETAILS  pafdSimple;
    HANDLE          hProc;
    CHOOSEHOOKPROC  pfnHook;        // Hook proc
    BOOL            fEnableHook;    // Hook enabled.

    union {
        PACMFORMATCHOOSE_INT pfmtc;    // Initialization structure
        PACMFILTERCHOOSE_INT pafltrc;  // Initialization structure
    };                              // Chooser Specific

    PACMGARB        pag;
    
} InstData, *PInstData, FAR * LPInstData;

enum { FILTER_CHOOSE, FORMAT_CHOOSE };

#define MAX_HWND_NOTIFY             100
#define MAX_CUSTOM_FORMATS          100
#define MAX_FORMAT_KEY               64

/*
 * Save instance data in a property to give others access to the DWL_USER
 */
#define SetInstData(hwnd, p)   (SetWindowLong(hwnd, GWL_USERDATA, p) ? 1 : 1)
#define GetInstData(hwnd)      (PInstData) (GetWindowLong(hwnd, GWL_USERDATA))
#define RemoveInstData(hwnd)   (0)
// BUGBUGBUGBUG what here?
//#define SetInstData(hwnd, p) SetProp(hwnd,gszInstProp,(HANDLE)(p))
//#define GetInstData(hwnd)    (PInstData)(LPVOID)GetProp(hwnd, gszInstProp)
//#define RemoveInstData(hwnd) RemoveProp(hwnd,gszInstProp)


/*
 * For passing near pointers in lparams
 */
#define PTR2LPARAM(x)       (LPARAM)(VOID *)(x)
#define LPARAM2PTR(x)       (VOID *)(x)    


//
//  This routine deleted a NameStore object allocated by NewNameStore().
//
__inline void
DeleteNameStore ( PNameStore pns )
{
    LocalFree((HLOCAL)pns);
}


