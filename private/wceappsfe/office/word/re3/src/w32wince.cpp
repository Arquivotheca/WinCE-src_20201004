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
// Windows CE implementations of W32 Interfaces.

// The following function is needed only because a Windows CE dll must have at least one export

__declspec(dllexport) void Useless( void )
{
    return;
}

LRESULT CW32System::WndProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    CTxtWinHost *phost = (CTxtWinHost *) GetWindowLong(hwnd, ibPed);

    #ifdef DEBUG
    Tracef(TRCSEVINFO, "hwnd %lx, msg %lx, wparam %lx, lparam %lx", hwnd, msg, wparam, lparam);
    #endif  // DEBUG

    switch(msg)
    {
    case WM_CREATE:
        // On Win CE we must simulate a WM_NCCREATE
        (void) CTxtWinHost::OnNCCreate(hwnd, (CREATESTRUCT *) lparam);
        phost = (CTxtWinHost *) GetWindowLong(hwnd, ibPed);
        break;

    case WM_DESTROY:
        if( phost )
        {
            CTxtWinHost::OnNCDestroy(phost);
        }
        return 0;

    }

    return phost ? phost->TxWindowProc(hwnd, msg, wparam, lparam)
               : DefWindowProc(hwnd, msg, wparam, lparam);
}

LONG ValidateTextRange(TEXTRANGE *pstrg);

ATOM WINAPI CW32System::RegisterREClass(
    const WNDCLASSW *lpWndClass,
    const char *szAnsiClassName,
    WNDPROC AnsiWndProc
)
{
    // On Windows CE we don't do anything with ANSI window class
    return ::RegisterClass(lpWndClass);
}

LRESULT CW32System::ANSIWndProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    // Should never be used in WinCE
    Assert(0);
	RETAILMSG(1, (__TEXT("CW32System::ANSIWndProc called!!!!!!!!!\r\n")));
    return 0;
}

HGLOBAL WINAPI CW32System::GlobalAlloc( UINT uFlags, DWORD dwBytes )
{
#ifdef TARGET_NT
    return ::GlobalAlloc( uFlags, dwBytes);
#else
    return LocalAlloc( uFlags & GMEM_ZEROINIT, dwBytes );
#endif
}

HGLOBAL WINAPI CW32System::GlobalFree( HGLOBAL hMem )
{
#ifdef TARGET_NT
    return ::GlobalFree( hMem );
#else
    return LocalFree( hMem );
#endif
}

UINT WINAPI CW32System::GlobalFlags( HGLOBAL hMem )
{
#ifdef TARGET_NT
    return ::GlobalFlags( hMem );
#else
    return LocalFlags( hMem );
#endif
}

HGLOBAL WINAPI CW32System::GlobalReAlloc( HGLOBAL hMem, DWORD dwBytes, UINT uFlags )
{
#ifdef TARGET_NT
    return ::GlobalReAlloc(hMem, dwBytes, uFlags);
#else
    return LocalReAlloc( hMem, dwBytes, uFlags );
#endif
}

DWORD WINAPI CW32System::GlobalSize( HGLOBAL hMem )
{
#ifdef TARGET_NT
    return ::GlobalSize( hMem );
#else
    return LocalSize( hMem );
#endif
}

LPVOID WINAPI CW32System::GlobalLock( HGLOBAL hMem )
{
#ifdef TARGET_NT
    return ::GlobalLock( hMem );
#else
    return LocalLock( hMem );
#endif
}

HGLOBAL WINAPI CW32System::GlobalHandle( LPCVOID pMem )
{
#ifdef TARGET_NT
    return ::GlobalHandle( pMem );
#else
    return LocalHandle( pMem );
#endif
}

BOOL WINAPI CW32System::GlobalUnlock( HGLOBAL hMem )
{
#ifdef TARGET_NT
    return ::GlobalUnlock( hMem );
#else
    return LocalUnlock( hMem );
#endif
}

BOOL WINAPI CW32System::REGetCharWidth(
    HDC hdc,
    UINT iChar,
    LPINT pAns,
    UINT        // For Windows CE the code page is not used
                //  as the A version is not called.
)
{
    int i;
    SIZE size;
    TCHAR buff[2];

    buff[0] = iChar;
    buff[1] = 0;

    if (GetTextExtentExPoint(hdc, buff, 1, 32000, &i, (LPINT)pAns, &size))
    {
        return TRUE;
    }

    return FALSE;
}

BOOL WINAPI CW32System::REExtTextOut(
    CONVERTMODE cm,
    UINT uiCodePage,
    HDC hdc,
    int x,
    int y,
    UINT fuOptions,
    CONST RECT *lprc,
    const WCHAR *lpString,
    UINT cbCount,
    CONST INT *lpDx,
    BOOL  FEFontOnNonFEWin95
)
{
    return ExtTextOut(hdc, x, y, fuOptions, lprc, lpString, cbCount, lpDx);
}

CONVERTMODE WINAPI CW32System::DetermineConvertMode( BYTE tmCharSet )
{
    return CM_NONE;
}

// Workaround for SA1100 compiler bug
#if ARM
#pragma optimize("",off)
#endif
       
void WINAPI CW32System::CalcUnderlineInfo( CCcs *pcccs, TEXTMETRIC *ptm )
{
    // Default calculation of size of underline
    // Implements a heuristic in the absence of better font information.
    SHORT dyDescent = pcccs->_yDescent;

    if (0 == dyDescent)
    {
        dyDescent = pcccs->_yHeight >> 3;
    }

    pcccs->_dyULWidth = max(1, dyDescent / 4);
    pcccs->_dyULOffset = (dyDescent - 3 * pcccs->_dyULWidth + 1) / 2;

    if ((0 == pcccs->_dyULOffset) && (dyDescent > 1))
    {
        pcccs->_dyULOffset = 1;
    }

    pcccs->_dySOOffset = -ptm->tmAscent / 3;
    pcccs->_dySOWidth = pcccs->_dyULWidth;

    return;
}

// Workaround for SA1100 compiler bug
#if ARM
#pragma optimize("",on)
#endif


BOOL WINAPI CW32System::ShowScrollBar( HWND hWnd, int wBar, BOOL bShow, LONG nMax )
{
    SCROLLINFO si;
    Assert(wBar == SB_VERT || wBar == SB_HORZ);
    W32->ZeroMemory(&si, sizeof(SCROLLINFO));

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_DISABLENOSCROLL;
    if (bShow)
    {
        si.nMax = nMax;
    }
    ::SetScrollInfo(hWnd, wBar, &si, TRUE);
    return TRUE;
}

BOOL WINAPI CW32System::EnableScrollBar( HWND hWnd, UINT wSBflags, UINT wArrows )
{
    BOOL fEnable = TRUE;
    BOOL fApi;
    SCROLLINFO si;

    Assert (wSBflags == SB_VERT || wSBflags == SB_HORZ);
    if (wArrows == ESB_DISABLE_BOTH)
    {
        fEnable = FALSE;
    }
    // Get the current scroll range
    W32->ZeroMemory(&si, sizeof(SCROLLINFO));
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE;
    fApi = ::GetScrollInfo(hWnd, wSBflags, &si);
    if (fApi && !fEnable)
    {
        si.fMask = SIF_RANGE | SIF_DISABLENOSCROLL;
        si.nMin = 0;
        si.nMax = 0;
    }
    if (fApi) ::SetScrollInfo(hWnd, wSBflags, &si, TRUE);
    return fApi ? TRUE : FALSE;
}

BOOL WINAPI CW32System::IsEnhancedMetafileDC( HDC )
{
    // No enhanced metafile
    return FALSE;
}

UINT WINAPI CW32System::GetTextAlign(HDC hdc)
{
    // Review :: SHould we set last error?
    return GDI_ERROR;
}

UINT WINAPI CW32System::SetTextAlign(HDC hdc, UINT uAlign)
{
    // Review :: SHould we set last error?
    return GDI_ERROR;
}

UINT WINAPI CW32System::InvertRect(HDC hdc, CONST RECT *prc)
{
    HBITMAP hbm, hbmOld;
    HDC     hdcMem;
    int     nHeight, nWidth;
	UINT	RetVal = FALSE;

    nWidth = prc->right-prc->left;
    nHeight = prc->bottom-prc->top;

    hdcMem = CreateCompatibleDC(hdc);
	if(!hdcMem)
		{
		goto leave;
		}
    
	hbm = CreateCompatibleBitmap(hdc, nWidth, nHeight);
	if(!hbm)
		{
		DeleteObject(hdcMem);
		goto leave;
		}

    hbmOld = (HBITMAP) SelectObject(hdcMem, hbm);

    BitBlt(hdcMem, 0, 0, nWidth, nHeight, hdc, prc->left, prc->top,
            SRCCOPY);

    FillRect(hdc, prc, (HBRUSH)GetStockObject(WHITE_BRUSH));

    BitBlt(hdc, prc->left, prc->top, nWidth,
                nHeight, hdcMem, 0, 0, SRCINVERT);

    SelectObject(hdcMem, hbmOld);
    DeleteDC(hdcMem);
    DeleteObject(hbm);
	RetVal = TRUE;

leave:    
	return RetVal;
}

HPALETTE WINAPI CW32System::ManagePalette(
    HDC,
    CONST LOGPALETTE *,
    HPALETTE &,
    HPALETTE &
)
{
    // No op for Windows CE
    return NULL;
}

int WINAPI CW32System::GetMapMode(HDC)
{
    // Only MM Text supported on Win CE
    return MM_TEXT;
}

BOOL WINAPI CW32System::WinLPtoDP(HDC, LPPOINT, int)
{
    // This is not available on Win CE
    return 0;
}

BOOL WINAPI CW32System::WinDPtoLP(HDC, LPPOINT, int)
{
    // This is not available on Win CE
    return 0;
}

long WINAPI CW32System::WvsprintfA( LONG cbBuf, LPSTR pszBuf, LPCSTR pszFmt, va_list arglist )
{
    WCHAR wszBuf[64];
    WCHAR wszFmt[64];
    WCHAR *pwszBuf = wszBuf;
    WCHAR *pwszFmt = wszFmt;
    UINT cch = 0;
    Assert(cbBuf < 64);
    while (*pszFmt && (cch<62))
    {
        *pwszFmt++ = *pszFmt++;
        cch++;
        if (*(pwszFmt - 1) == '%')
        {
            Assert(*pszFmt == 's' || *pszFmt == 'd' || *pszFmt == '0' || *pszFmt == 'c');
            if (*pszFmt == 's')
            {
                *pwszFmt++ = 'h';
                cch++;
            }
        }
    }
    *pwszFmt = 0;
    LONG cw = wvsprintf( wszBuf, wszFmt, arglist );
    while (*pszBuf++ = *pwszBuf++);
    Assert(cw < cbBuf);
    return cw;
}

int WINAPI CW32System::MulDiv(int nNumber, int nNumerator, int nDenominator)
{
    // Special handling for Win CE
    // Must be careful to not cause divide by zero
    // Note that overflow on the multiplication is not handled
    // Hopefully that is not a problem for RichEdit use
    // Added Guy's fix up for rounding.

    // Conservative check to see if multiplication will overflow.
    if (IN_RANGE(_I16_MIN, nNumber, _I16_MAX) &&
        IN_RANGE(_I16_MIN, nNumerator, _I16_MAX))
    {
        return nDenominator ? ((nNumber * nNumerator) + (nDenominator / 2)) / nDenominator  : -1;
    }

    __int64 NNumber = nNumber;
    __int64 NNumerator = nNumerator;
    __int64 NDenominator = nDenominator;

    return NDenominator ? ((NNumber * NNumerator) + (NDenominator / 2)) / NDenominator  : -1;
}


void CW32System::CheckChangeKeyboardLayout ( CTxtSelection *psel, BOOL fChangedFont )
{
    return;
}

void CW32System::CheckChangeFont (
    CTxtSelection *psel,
    CTxtEdit * const ped,
    BOOL fEnableReassign,   // @parm Do we enable CTRL key?
    const WORD lcID,        // @parm LCID from WM_ message
    UINT cpg                // @parm code page to use (could be ANSI for far east with IME off)
)
{
    return;
}

BOOL CW32System::FormatMatchesKeyboard( const CCharFormat *pFormat )
{
    return FALSE;
}
/*
HKL CW32System::GetKeyboardLayout ( DWORD )
{
    return NULL;
}

int CW32System::GetKeyboardLayoutList ( int, HKL FAR * )
{
    return 0;
}
*/
HRESULT CW32System::LoadRegTypeLib ( REFGUID, WORD, WORD, LCID, ITypeLib ** )
{
    return E_NOTIMPL;
}

HRESULT CW32System::LoadTypeLib ( const OLECHAR *, ITypeLib ** )
{
    return E_NOTIMPL;
}

BSTR CW32System::SysAllocString ( const OLECHAR * )
{
    return NULL;
}

BSTR CW32System::SysAllocStringLen ( const OLECHAR *, UINT )
{
    return NULL;
}

void CW32System::SysFreeString ( BSTR )
{
    return;
}

UINT CW32System::SysStringLen ( BSTR )
{
    return 0;
}

void CW32System::VariantInit ( VARIANTARG * )
{
    return;
}

HRESULT CW32System::OleCreateFromData ( LPDATAOBJECT, REFIID, DWORD, LPFORMATETC, LPOLECLIENTSITE, LPSTORAGE, void ** )
{
    return 0;
}

void CW32System::CoTaskMemFree ( LPVOID )
{
    return;
}

HRESULT CW32System::CreateBindCtx ( DWORD, LPBC * )
{
    return 0;
}

HANDLE CW32System::OleDuplicateData ( HANDLE, CLIPFORMAT, UINT )
{
    return NULL;
}

HRESULT CW32System::CoTreatAsClass ( REFCLSID, REFCLSID )
{
    return 0;
}

HRESULT CW32System::ProgIDFromCLSID ( REFCLSID, LPOLESTR * )
{
    return E_NOTIMPL;
}

HRESULT CW32System::OleConvertIStorageToOLESTREAM ( LPSTORAGE, LPOLESTREAM )
{
    return 0;
}

HRESULT CW32System::OleConvertIStorageToOLESTREAMEx ( LPSTORAGE, CLIPFORMAT, LONG, LONG, DWORD, LPSTGMEDIUM, LPOLESTREAM )
{
    return 0;
}

HRESULT CW32System::OleSave ( LPPERSISTSTORAGE, LPSTORAGE, BOOL )
{
    return 0;
}

HRESULT CW32System::StgCreateDocfileOnILockBytes ( ILockBytes *, DWORD, DWORD, IStorage ** )
{
    return 0;
}

HRESULT CW32System::CreateILockBytesOnHGlobal ( HGLOBAL, BOOL, ILockBytes ** )
{
    return 0;
}

HRESULT CW32System::OleCreateLinkToFile ( LPCOLESTR, REFIID, DWORD, LPFORMATETC, LPOLECLIENTSITE, LPSTORAGE, void ** )
{
    return 0;
}

LPVOID CW32System::CoTaskMemAlloc ( ULONG )
{
    return NULL;
}

LPVOID CW32System::CoTaskMemRealloc ( LPVOID, ULONG )
{
    return NULL;
}

HRESULT CW32System::OleInitialize ( LPVOID )
{
    return 0;
}

void CW32System::OleUninitialize ( )
{
    return;
}

HRESULT CW32System::OleSetClipboard ( IDataObject * )
{
    return E_NOTIMPL;
}

HRESULT CW32System::OleFlushClipboard ( )
{
    return 0;
}

HRESULT CW32System::OleIsCurrentClipboard ( IDataObject * )
{
    return 0;
}

HRESULT CW32System::DoDragDrop ( IDataObject *, IDropSource *, DWORD, DWORD * )
{
    return 0;
}

HRESULT CW32System::OleGetClipboard ( IDataObject ** )
{
    return E_NOTIMPL;
}

HRESULT CW32System::RegisterDragDrop ( HWND, IDropTarget * )
{
    return 0;
}

HRESULT CW32System::OleCreateLinkFromData ( IDataObject *, REFIID, DWORD, LPFORMATETC, IOleClientSite *, IStorage *, void ** )
{
    return 0;
}

HRESULT CW32System::OleCreateStaticFromData ( IDataObject *, REFIID, DWORD, LPFORMATETC, IOleClientSite *, IStorage *, void ** )
{
    return 0;
}

HRESULT CW32System::OleDraw ( IUnknown *, DWORD, HDC, LPCRECT )
{
    return 0;
}

HRESULT CW32System::OleSetContainedObject ( IUnknown *, BOOL )
{
    return 0;
}

HRESULT CW32System::CoDisconnectObject ( IUnknown *, DWORD )
{
    return 0;
}

HRESULT CW32System::WriteFmtUserTypeStg ( IStorage *, CLIPFORMAT, LPOLESTR )
{
    return 0;
}

HRESULT CW32System::WriteClassStg ( IStorage *, REFCLSID )
{
    return 0;
}

HRESULT CW32System::SetConvertStg ( IStorage *, BOOL )
{
    return 0;
}

HRESULT CW32System::ReadFmtUserTypeStg ( IStorage *, CLIPFORMAT *, LPOLESTR * )
{
    return 0;
}

HRESULT CW32System::ReadClassStg ( IStorage *pstg, CLSID * )
{
    return 0;
}

HRESULT CW32System::OleRun ( IUnknown * )
{
    return 0;
}

HRESULT CW32System::RevokeDragDrop ( HWND )
{
    return 0;
}

HRESULT CW32System::CreateStreamOnHGlobal ( HGLOBAL, BOOL, IStream ** )
{
    return 0;
}

HRESULT CW32System::GetHGlobalFromStream ( IStream *pstm, HGLOBAL * )
{
    return 0;
}

HRESULT CW32System::OleCreateDefaultHandler ( REFCLSID, IUnknown *, REFIID, void ** )
{
    return 0;
}

HRESULT CW32System::CLSIDFromProgID ( LPCOLESTR, LPCLSID )
{
    return 0;
}

HRESULT CW32System::OleConvertOLESTREAMToIStorage ( LPOLESTREAM, IStorage *, const DVTARGETDEVICE * )
{
    return 0;
}

HRESULT CW32System::OleLoad ( IStorage *, REFIID, IOleClientSite *, void ** )
{
    return 0;
}

HRESULT CW32System::ReleaseStgMedium ( LPSTGMEDIUM pstgmed)
{
    // we don't use anything other than TYMED_HGLOBAL currently.
    if (pstgmed && (pstgmed->tymed == TYMED_HGLOBAL)) {
        ::LocalFree(pstgmed->hGlobal);
    }

    return 0;
}

void CW32System::FreeOle()
{
}

BOOL CW32System::ImmInitialize( void )
{
    return FALSE;
}

void CW32System::ImmTerminate( void )
{
    return;
}

// GuyBark: Use the system calls on Windows CE.
#ifndef PWD_JUPITER

LONG CW32System::ImmGetCompositionStringA ( HIMC, DWORD, LPVOID, DWORD )
{
    return 0;
}

HIMC CW32System::ImmGetContext ( HWND )
{
    return 0;
}

BOOL CW32System::ImmSetCompositionFontA ( HIMC, LPLOGFONTA )
{
    return FALSE;
}

BOOL CW32System::ImmSetCompositionWindow ( HIMC, LPCOMPOSITIONFORM )
{
    return FALSE;
}

BOOL CW32System::ImmReleaseContext ( HWND, HIMC )
{
    return FALSE;
}

DWORD CW32System::ImmGetProperty ( HKL, DWORD )
{
    return 0;
}

BOOL CW32System::ImmGetCandidateWindow ( HIMC, DWORD, LPCANDIDATEFORM )
{
    return FALSE;
}

BOOL CW32System::ImmSetCandidateWindow ( HIMC, LPCANDIDATEFORM )
{
    return FALSE;
}

BOOL CW32System::ImmNotifyIME ( HIMC, DWORD, DWORD, DWORD )
{
    return FALSE;
}

HIMC CW32System::ImmAssociateContext ( HWND, HIMC )
{
    return NULL;
}

UINT CW32System::ImmGetVirtualKey ( HWND )
{
    return 0;
}

HIMC CW32System::ImmEscape ( HKL, HIMC, UINT, LPVOID )
{
    return NULL;
}

LONG CW32System::ImmGetOpenStatus ( HIMC )
{
    return 0;
}

BOOL CW32System::ImmGetConversionStatus ( HIMC, LPDWORD, LPDWORD )
{
    return FALSE;
}

#endif // !PWD_JUPITER

BOOL CW32System::FSupportSty ( UINT, UINT )
{
    return FALSE;
}

const IMESTYLE * CW32System::PIMEStyleFromAttr ( const UINT )
{
    return NULL;
}

const IMECOLORSTY * CW32System::PColorStyleTextFromIMEStyle ( const IMESTYLE * )
{
    return NULL;
}

const IMECOLORSTY * CW32System::PColorStyleBackFromIMEStyle ( const IMESTYLE * )
{
    return NULL;
}

BOOL CW32System::FBoldIMEStyle ( const IMESTYLE * )
{
    return FALSE;
}

BOOL CW32System::FItalicIMEStyle ( const IMESTYLE * )
{
    return FALSE;
}

BOOL CW32System::FUlIMEStyle ( const IMESTYLE * )
{
    return FALSE;
}

UINT CW32System::IdUlIMEStyle ( const IMESTYLE * )
{
    return 0;
}

COLORREF CW32System::RGBFromIMEColorStyle ( const IMECOLORSTY * )
{
    return 0;
}

BOOL CW32System::GetVersion(
    DWORD *pdwPlatformId,
    DWORD *pdwMajorVersion
)
{
    OSVERSIONINFO osv;
    osv.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    *pdwPlatformId = 0;
    *pdwMajorVersion = 0;
    if (GetVersionEx(&osv))
    {
        *pdwPlatformId = osv.dwPlatformId;
        *pdwMajorVersion = osv.dwMajorVersion;
        return TRUE;
    }
    return FALSE;
}

BOOL WINAPI CW32System::GetStringTypeEx(
    LCID lcid,
    DWORD dwInfoType,
    LPCTSTR lpSrcStr,
    int cchSrc,
    LPWORD lpCharType
)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "GetStringTypeEx");
    return ::GetStringTypeExW(lcid, dwInfoType, lpSrcStr, cchSrc, lpCharType);
}

LPWSTR WINAPI CW32System::CharLower(LPWSTR pwstr)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CharLowerWrap");
    return ::CharLowerW(pwstr);
}

DWORD WINAPI CW32System::CharLowerBuff(LPWSTR pwstr, DWORD cchLength)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CharLowerBuffWrap");
    return ::CharLowerBuffW(pwstr, cchLength);
}

DWORD WINAPI CW32System::CharUpperBuff(LPWSTR pwstr, DWORD cchLength)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CharUpperBuffWrap");
    return ::CharUpperBuffW(pwstr, cchLength);
}

HDC WINAPI CW32System::CreateIC(
        LPCWSTR             lpszDriver,
        LPCWSTR             lpszDevice,
        LPCWSTR             lpszOutput,
        CONST DEVMODEW *    lpInitData)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CreateIC");
    return ::CreateDCW( lpszDriver, lpszDevice, lpszOutput, lpInitData);
}

HANDLE WINAPI CW32System::CreateFile(
    LPCWSTR                 lpFileName,
    DWORD                   dwDesiredAccess,
    DWORD                   dwShareMode,
    LPSECURITY_ATTRIBUTES   lpSecurityAttributes,
    DWORD                   dwCreationDisposition,
    DWORD                   dwFlagsAndAttributes,
    HANDLE                  hTemplateFile
)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CreateFile");
    return ::CreateFileW(lpFileName,
                        dwDesiredAccess,
                        dwShareMode,
                        lpSecurityAttributes,
                        dwCreationDisposition,
                        dwFlagsAndAttributes,
                        hTemplateFile);
}

HFONT WINAPI CW32System::CreateFontIndirect(CONST LOGFONTW * plfw)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CreateFontIndirect");
    return ::CreateFontIndirectW(plfw);
}

int WINAPI CW32System::CompareString (
    LCID  Locale,           // locale identifier
    DWORD  dwCmpFlags,      // comparison-style options
    LPCWSTR  lpString1,     // pointer to first string
    int  cchCount1,         // size, in bytes or characters, of first string
    LPCWSTR  lpString2,     // pointer to second string
    int  cchCount2          // size, in bytes or characters, of second string
)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CompareString");
    return ::CompareStringW(Locale, dwCmpFlags, lpString1, cchCount1, lpString2, cchCount2);
}

LRESULT WINAPI CW32System::DefWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "DefWindowProc");
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI CW32System::GetObject(HGDIOBJ hgdiObj, int cbBuffer, LPVOID lpvObj)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "GetObject");
    return ::GetObjectW( hgdiObj, cbBuffer, lpvObj );
}

DWORD APIENTRY CW32System::GetProfileSection(
    LPCWSTR ,
    LPWSTR ,
    DWORD
)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "GetProfileSection");
    // Not available on Win CE
    return 0;
}

int WINAPI CW32System::GetTextFace(
        HDC    hdc,
        int    cch,
        LPWSTR lpFaceName
)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "GetTextFaceWrap");
    return ::GetTextFaceW( hdc, cch, lpFaceName );
}

BOOL WINAPI CW32System::GetTextMetrics(HDC hdc, LPTEXTMETRICW lptm)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "GetTextMetrics");
    return ::GetTextMetricsW( hdc, lptm);
}

LONG WINAPI CW32System::GetWindowLong(HWND hWnd, int nIndex)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "GetWindowLong");
    return ::GetWindowLongW(hWnd, nIndex);
}

HBITMAP WINAPI CW32System::LoadBitmap(HINSTANCE hInstance, LPCWSTR lpBitmapName)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "LoadBitmap");
    Assert(HIWORD(lpBitmapName) == 0);
    return ::LoadBitmapW(hInstance, lpBitmapName);
}

HCURSOR WINAPI CW32System::LoadCursor(HINSTANCE hInstance, LPCWSTR lpCursorName)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "LoadCursor");
    Assert(HIWORD(lpCursorName) == 0);
	
    return ::LoadCursorW(hInstance, lpCursorName);
}

HINSTANCE WINAPI CW32System::LoadLibrary(LPCWSTR lpLibFileName)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "LoadLibrary");
    return ::LoadLibraryW(lpLibFileName);
}

LRESULT WINAPI CW32System::SendMessage(
    HWND    hWnd,
    UINT    Msg,
    WPARAM  wParam,
    LPARAM  lParam
)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "SendMessage");
    return ::SendMessageW(hWnd, Msg, wParam, lParam);
}

LONG WINAPI CW32System::SetWindowLong(HWND hWnd, int nIndex, LONG dwNewLong)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "SetWindowLongWrap");
    return ::SetWindowLongW(hWnd, nIndex, dwNewLong);
}

BOOL WINAPI CW32System::PostMessage(
    HWND    hWnd,
    UINT    Msg,
    WPARAM  wParam,
    LPARAM  lParam
)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "PostMessage");
    return ::PostMessageW(hWnd, Msg, wParam, lParam);
}

BOOL WINAPI CW32System::UnregisterClass(LPCWSTR lpClassName, HINSTANCE hInstance)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "UnregisterClass");
    return ::UnregisterClassW( lpClassName, hInstance);
}

int WINAPI CW32System::lstrcmp(LPCWSTR lpString1, LPCWSTR lpString2)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "lstrcmp");
    return ::lstrcmpW(lpString1, lpString2);
}

int WINAPI CW32System::lstrcmpi(LPCWSTR lpString1, LPCWSTR lpString2)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "lstrcmpi");
    return ::lstrcmpiW(lpString1, lpString2);
}

BOOL WINAPI CW32System::PeekMessage(
    LPMSG   lpMsg,
    HWND    hWnd,
    UINT    wMsgFilterMin,
    UINT    wMsgFilterMax,
    UINT    wRemoveMsg
)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "PeekMessage");
    return ::PeekMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
}

DWORD WINAPI CW32System::GetModuleFileName(
    HMODULE hModule,
    LPWSTR lpFilename,
    DWORD nSize
)
{
    TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "GetModuleFileName");
#ifdef TARGET_NT
    return ::GetModuleFileNameW(hModule, lpFilename, nSize);
#else
    // On Windows CE we will always be known as riched20.dll
    CopyMemory(lpFilename, TEXT("riched20.dll"), sizeof(TEXT("riched20.dll")));
#endif
    return sizeof(TEXT("riched20.dll"))/sizeof(WCHAR) - 1;
}

BOOL WINAPI CW32System::GetCursorPos(
    POINT *ppt
)
{
// BUGBUG: When we can figure out if we have full cursor support 
// change this to call GetCursorPos when real cursor support is available
#if 1
    DWORD   dw;

    dw = GetMessagePos();
    ppt->x = LOWORD(dw);
    ppt->y = HIWORD(dw);
#else
	::GetCursorPos(ppt);
#endif
    return TRUE;
}

#if 0

    // flags
#pragma message ("JMO Review : Should be same as has nlsprocs, initialize in constructor")
    unsigned _fIntlKeyboard : 1;

// From nlsprocs.h

// TranslateCharsetInfo
// typedef WINGDIAPI BOOL (WINAPI*TCI_CAST)( DWORD FAR *, LPCHARSETINFO, DWORD);
// #define  pTranslateCharsetInfo(a,b,c) (( TCI_CAST) nlsProcTable[iTranslateCharsetInfo])(a,b,c)

//#define   pTranslateCharsetInfo() (*()nlsProcTable[iTranslateCharsetInfo])()


/*
 *  W32Imp::CheckChangeKeyboardLayout ( BOOL fChangedFont )
 *
 *  @mfunc
 *      Change keyboard for new font, or font at new character position.
 *  @comm
 *      Using only the currently loaded KBs, locate one that will support
 *      the insertion points font. This is called anytime a character format
 *      change occurs, or the insert font (caret position) changes.
 *  @devnote
 *      The current KB is preferred. If a previous association
 *      was made, see if the KB is still loaded in the system and if so use
 *      it. Otherwise, locate a suitable KB, preferring KB's that have
 *      the same charset ID as their default, preferred charset. If no match
 *      can be made then nothing changes.
 *
 *      This routine is only useful on Windows 95.
 */
#ifndef MACPORT
#ifndef PEGASUS


#define MAX_HKLS 256                                // It will be awhile
                                                    //  before we have more KBs
    CTxtEdit * const ped = GetPed();                // Document context.

    INT         i, totalLayouts,                    // For matching KBs.
                iBestLayout = -1;

    WORD        preferredKB;                        // LCID of preferred KB.

    HKL         hklList[MAX_HKLS];                  // Currently loaded KBs.

    const CCharFormat *pcf;                         // Current font.
    CHARSETINFO csi;                                // Font's CodePage bits.

    AssertSz(ped, "no ped?");                       // hey now!

    if (!ped || !ped->IsRich() || !ped->_fFocus ||  // EXIT if no ped or focus or
        !ped->IsAutoKeyboard())                     // auto keyboard is turn off
        return;

    pcf = ped->GetCharFormat(_iFormat);             // Get insert font's data

    hklList[0]      = pGetKeyboardLayout(0);        // Current hkl preferred?
    preferredKB     = fc().GetPreferredKB( pcf->bCharSet );
    if ( preferredKB != LOWORD(hklList[0]) )        // Not correct KB?
    {
                                                    // Get loaded HKLs.
        totalLayouts    = 1 + pGetKeyboardLayoutList(MAX_HKLS, &hklList[1]);
                                                    // Know which KB?
        if ( preferredKB )                          //  then locate it.
        {                                           // Sequential match because
            for ( i = 1; i < totalLayouts; i++ )    //  HKL may be unloaded.
            {                                       // Match LCIDs.
                if ( preferredKB == LOWORD( hklList[i]) )
                {
                    iBestLayout = i;
                    break;                          // Matched it.
                }
            }
            if ( i >= totalLayouts )                // Old KB is missing.
            {                                       // Reset to locate new KB.
                fc().SetPreferredKB ( pcf->bCharSet, 0 );
            }
        }
        if ( iBestLayout < 0 )                          // Attempt to find new KB.
        {
            for ( i = 0; i < totalLayouts; i++ )
            {
                pTranslateCharsetInfo(              // Get KB's charset.
                        (DWORD *) ConvertLanguageIDtoCodePage(LOWORD(hklList[iBestLayout])),
                        &csi, TCI_SRCCODEPAGE);

                if( csi.ciCharset == pcf->bCharSet) // If charset IDs match?
                {
                    iBestLayout = i;
                    break;                          //  then this KB is best.
                }
            }
            if ( iBestLayout >= 0)                  // Bind new KB.
            {
                fChangedFont = TRUE;
                fc().SetPreferredKB(pcf->bCharSet, LOWORD(hklList[iBestLayout]));
            }
        }
        if ( fChangedFont && iBestLayout >= 0)          // Bind font.
        {
            ICharFormatCache *  pCF;

            if(SUCCEEDED(GetCharFormatCache(&pCF)))
            {
                pCF->AddRefFormat(_iFormat);
                fc().SetPreferredFont(
                        LOWORD(hklList[iBestLayout]), _iFormat );
            }
        }
        if( iBestLayout > 0 )                           // If == 0 then
        {                                               //  it's already active.
                                                        // Activate KB.
            ActivateKeyboardLayout( hklList[iBestLayout], 0);
        }
    }
#endif // PEGASUS needs its own code
#endif // MACPORT -- the mac needs its own code.

/*
 *  CTxtSelection::CheckChangeFont ( CTxtEdit * const ped, const WORD lcID )
 *
 *  @mfunc
 *      Change font for new keyboard layout.
 *  @comm
 *      If no previous preferred font has been associated with this KB, then
 *      locate a font in the document suitable for this KB.
 *  @devnote
 *      This routine is called via WM_INPUTLANGCHANGEREQUEST message
 *      (a keyboard layout switch). This routine can also be called
 *      from WM_INPUTLANGCHANGE, but we are called more, and so this
 *      is less efficient.
 *
 *      Exact match is done via charset ID bitmask. If a match was previously
 *      made, use it. A user can force the insertion font to be associated
 *      to a keyboard if the control key is held through the KB changing
 *      process. The association is broken when another KB associates to
 *      the font. If no match can be made then nothing changes.
 *
 *      This routine is only useful on Windows 95.
 *
 */
#ifndef MACPORT
#ifndef PEGASUS

    LOCALESIGNATURE ls, curr_ls;                    // KB's code page bits.

    CCharFormat     cf,                             // For creating new font.
                    currCF;                         // For searching
    const CCharFormat   *pPreferredCF;
    CHARSETINFO     csi;                            //  with code page bits.

    LONG            iFormat, iBestFormat = -1;      // Loop support.
    INT             i;

    BOOL            fLastTime;                      // Loop support.
    BOOL            fSetPreferred = FALSE;

    HKL             currHKL;                        // current KB;

    BOOL            fLookUpFaceName = FALSE;        // when picking a new font.

    ICharFormatCache *  pCF;

    LPTSTR          lpszFaceName = NULL;
    BYTE            bCharSet;
    BYTE            bPitchAndFamily;
    BOOL            fFaceNameIsDBCS;

    AssertSz (ped, "No ped?");

    if (!ped->IsRich() || !ped->IsAutoFont())       // EXIT if not running W95.
        return;                                     // EXIT if auto font is turn off

    if(FAILED(GetCharFormatCache(&pCF)))            // Get CharFormat Cache.
        return;

    cf.InitDefault(0);

    // An alternate approach would be to get the key state from the corresponding
    // message; FUTURE (alexgo): we can consider doing this, but we need to make sure
    // everything works with windowless controls..
    BOOL fReassign = fEnableReassign
                  && (GetAsyncKeyState(VK_CONTROL)<0);// Is user holding CTRL?

    currHKL = pGetKeyboardLayout(0);

    ped->GetCharFormat(_iFormat)->Get(&currCF);
    GetLocaleInfoA( lcID, LOCALE_FONTSIGNATURE, (char *) &ls, sizeof(ls));

    if ( fReassign )                                // Force font/KB assoc.
    {                                               // If font supports KB
                                                    //  in any way,
                                                    // Note: test Unicode bits.
        GetLocaleInfoA( fc().GetPreferredKB (currCF.bCharSet),
                LOCALE_FONTSIGNATURE, (char *) &curr_ls, sizeof(ls));
        if ( CountMatchingBits(curr_ls.lsUsb, ls.lsUsb, 4) )
        {                                           // Break old font/KB assoc.
            fc().SetPreferredFont( fc().GetPreferredKB (currCF.bCharSet), -1 );
                                                    // Bind KB and font.
            fc().SetPreferredKB( currCF.bCharSet, lcID );

            pCF->AddRefFormat(_iFormat);
            fc().SetPreferredFont( lcID, _iFormat );
        }
    }
    else                                            // Lookup preferred font.
    {
                                                    // Attempt to Find new
        {                                           //  preferred font.
            CFormatRunPtr rp(_rpCF);                // Nondegenerate range

            fLastTime = TRUE;
            if ( _rpCF.IsValid() )                  // If doc has cf runs.
            {
                fLastTime = FALSE;
                rp.AdjustBackward();
            }
            pTranslateCharsetInfo(                  //  charset.
                        (DWORD *)cpg, &csi, TCI_SRCCODEPAGE);

            iFormat = _iFormat;                     // Search _iFormat,
                                                    //  then backwards.
            i = MAX_RUNTOSEARCH;                    // Don't be searching for
            while ( 1 )                             //  years...
            {                                       // Get code page bits.
                pPreferredCF = ped->GetCharFormat(iFormat);

                if (csi.ciCharset == pPreferredCF->bCharSet)    // Equal charset ids?
                {
                    fSetPreferred = TRUE;
                    break;
                }
                if ( fLastTime )                    // Done searching?
                    break;
                iFormat = rp.GetFormat();           // Keep searching backward.
                fLastTime = !rp.PrevRun() && i--;
            }
            if ( !fSetPreferred && _rpCF.IsValid()) // Try searching forward.
            {
                rp = _rpCF;
                rp.AdjustBackward();
                i = MAX_RUNTOSEARCH;                // Don't be searching for
                while (i-- && rp.NextRun() )        //  years...
                {
                    iFormat = rp.GetFormat();       // Get code page bits.
                    pPreferredCF = ped->GetCharFormat(iFormat);
                                                    // Equal charset ids?
                    if (csi.ciCharset == pPreferredCF->bCharSet)
                    {
                        fSetPreferred = TRUE;
                        break;
                    }
                }
            }
        }

        if ( !fSetPreferred )
        {
            iFormat = fc().GetPreferredFont( lcID );

                                                        // Set preferred if usable.
            if (iFormat >= 0 &&
                csi.ciCharset == ped->GetCharFormat(iFormat)->bCharSet)
            {
                fSetPreferred = TRUE;
                pPreferredCF = ped->GetCharFormat(iFormat);
            }
        }

        // setup cf needed for creating a new format run
        cf = currCF;

        // We know that the facename is not tagged IsDBCS in all cases
        //  unless explicitly set below.
        fFaceNameIsDBCS = FALSE;

        if ( fSetPreferred )
        {
            // pick face name from the previous preferred format
            bPitchAndFamily = pPreferredCF->bPitchAndFamily;
            bCharSet = pPreferredCF->bCharSet;
            lpszFaceName = (LPTSTR)pPreferredCF->szFaceName;
            fFaceNameIsDBCS = pPreferredCF->bInternalEffects & CFMI_FACENAMEISDBCS;
        }
        else                                            // Still don't have a font?
        {                                               //  For FE, use hard coded defaults.
                                                        //  else get charset right.
            WORD CurrentCodePage = cpg;

            switch (CurrentCodePage)
            {                                           // FE hard codes from Word.
            case _JAPAN_CP:
                bCharSet = SHIFTJIS_CHARSET;
                lpszFaceName = lpJapanFontName;
                bPitchAndFamily = 17;
                break;

            case _KOREAN_CP:
                bCharSet = HANGEUL_CHARSET;
                lpszFaceName = lpKoreanFontName;
                bPitchAndFamily = 49;
                break;

            case _CHINESE_TRAD_CP:
                bCharSet = CHINESEBIG5_CHARSET;
                lpszFaceName = lpTChineseFontName;
                bPitchAndFamily = 54;
                break;

            case _CHINESE_SIM_CP:
                bCharSet = GB2312_CHARSET;
                lpszFaceName = lpSChineseFontName;
                bPitchAndFamily = 54;
                break;

            default:                                    // Use translate to get
                pTranslateCharsetInfo(                  //  charset.
                            (DWORD *) CurrentCodePage, &csi, TCI_SRCCODEPAGE);
                bCharSet = csi.ciCharset;

                if (IsFECharset(currCF.bCharSet) && !IsFECharset(bCharSet))
                {
                    // fall back to default
                    lpszFaceName = L"Arial";
                    bPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
                }
                else
                {
                    bPitchAndFamily = currCF.bPitchAndFamily;
                    lpszFaceName = currCF.szFaceName;
                    fFaceNameIsDBCS = currCF.bInternalEffects & CFEI_FACENAMEISDBCS;
                }
                fLookUpFaceName = TRUE;                 // Get Font's real name.

                break;
            }
        }

        // setup the rest of cf
        cf.bPitchAndFamily = bPitchAndFamily;
        cf.bCharSet = (BYTE) bCharSet;
        _tcscpy ( cf.szFaceName, lpszFaceName );
        if(fFaceNameIsDBCS)
        {
            cf.bInternalEffects |= CFEI_FACENAMEISDBCS;
        }
        else
        {
            cf.bInternalEffects &= ~CFEI_FACENAMEISDBCS;
        }
        cf.lcid = lcID;

        // If we relied on GDI to match a font, get the font's real name...
        if ( fLookUpFaceName )
        {
            const CDevDesc      *pdd = _pdp->GetDdRender();
            HDC                 hdc;
            CCcs                *pccs;
            HFONT               hfontOld;
            OUTLINETEXTMETRICA  *potm;
            CTempBuf            mem;
            UINT                otmSize;

            hdc = pdd->GetDC();
                                                // Select logfont into DC,
            if( hdc)                            //  for OutlineTextMetrics.
            {
                cf.SetCRC();
                pccs = fc().GetCcs(hdc, &cf, _pdp->GetZoomNumerator(),
                    _pdp->GetZoomDenominator(),
                    GetDeviceCaps(hdc, LOGPIXELSY));

                if( pccs )
                {
                    hfontOld = SelectFont(hdc, pccs->_hfont);

                    if( otmSize = ::GetOutlineTextMetricsA(hdc, 0, NULL) )
                    {
                        potm = (OUTLINETEXTMETRICA *) mem.GetBuf(otmSize);
                        if ( NULL != potm )
                        {
                            ::GetOutlineTextMetricsA(hdc, otmSize, potm);

                            CStrInW  strinw( &((CHAR *)(potm))[ BYTE(potm->otmpFaceName)] );

                            cf.bPitchAndFamily
                                = potm->otmTextMetrics.tmPitchAndFamily;
                            cf.bCharSet
                                = (BYTE) potm->otmTextMetrics.tmCharSet;

                            _tcscpy ( cf.szFaceName, (WCHAR *)strinw );
                            cf.bInternalEffects &= ~CFEI_FACENAMEISDBCS;
                        }
                    }

                    SelectFont( hdc, hfontOld );

                    pccs->Release();
                }

                pdd->ReleaseDC(hdc);
            }
        }

        if ( SUCCEEDED(pCF->Cache(&cf, &iFormat)) )
        {
            // This is redundent if ed.IsAutoKeyboard() == TRUE.
            pCF->AddRefFormat(_iFormat);
            fc().SetPreferredFont ( LOWORD (currHKL), _iFormat );

            fc().SetPreferredKB( cf.bCharSet, lcID );
            pCF->AddRefFormat(iFormat);
            fc().SetPreferredFont ( lcID, iFormat );

            pCF->ReleaseFormat(_iFormat);
            _iFormat = iFormat;
            ped->GetCallMgr()->SetSelectionChanged();
        }
    }
#endif
#endif // MACPORT -- the mac needs its own code.

                if (W32->FormatMatchesKeyboard(pcfForward))
                LOCALESIGNATURE ls;                             // Per HKL, CodePage bits.
                CHARSETINFO csi;                                // Font's CodePage bits.

                // Font's code page bits.
                pTranslateCharsetInfo((DWORD *) pcfForward->bCharSet, &csi, TCI_SRCCHARSET);
                // Current KB's code page bits.
                GetLocaleInfoA( LOWORD(pGetKeyboardLayout(0)), LOCALE_FONTSIGNATURE, (CHAR *) &ls, sizeof(ls));
                if ( (csi.fs.fsCsb[0] & ls.lsCsbDefault[0]) ||
                            (csi.fs.fsCsb[1] & ls.lsCsbDefault[1]) )
#endif
