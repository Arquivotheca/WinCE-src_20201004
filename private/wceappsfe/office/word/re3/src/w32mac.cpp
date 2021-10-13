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
//
//	Unicode --> MulitByte conversion
//

#define ARRAY_SIZE(x)   (sizeof(x) / sizeof(x[0]))

class CConvertStr
{
public:
    operator char *();

protected:
    CConvertStr();
    ~CConvertStr();
    void Free();

    LPSTR   _pstr;
    char    _ach[MAX_PATH * 2];
};

inline CConvertStr::operator char *()
{
    return _pstr;
}

inline CConvertStr::CConvertStr()
{
    _pstr = NULL;
}

inline CConvertStr::~CConvertStr()
{
    Free();
}

class CStrIn : public CConvertStr
{
public:
    CStrIn(LPCWSTR pwstr);
    CStrIn(LPCWSTR pwstr, int cwch);
    int strlen();

protected:
    CStrIn();
    void Init(LPCWSTR pwstr, int cwch);

    int _cchLen;
};

inline CStrIn::CStrIn()
{
}

inline int CStrIn::strlen()
{
    return _cchLen;
}

class CStrOut : public CConvertStr
{
public:
    CStrOut(LPWSTR pwstr, int cwchBuf);
    ~CStrOut();

    int     BufSize();
    int     Convert();

private:
    LPWSTR  _pwstr;
    int     _cwchBuf;
};

inline int CStrOut::BufSize()
{
    return _cwchBuf * 2;
}

//
//	Multi-Byte ---> Unicode conversion
//

class CConvertStrW
{
public:
    operator WCHAR *();

protected:
    CConvertStrW();
    ~CConvertStrW();
    void Free();

    LPWSTR   _pwstr;
    WCHAR    _awch[MAX_PATH * 2];
};

inline CConvertStrW::CConvertStrW()
{
    _pwstr = NULL;
}

inline CConvertStrW::~CConvertStrW()
{
    Free();
}

inline CConvertStrW::operator WCHAR *()
{
    return _pwstr;
}

class CStrInW : public CConvertStrW
{
public:
    CStrInW(LPCSTR pstr);
    CStrInW(LPCSTR pstr, UINT uiCodePage);
    CStrInW(LPCSTR pstr, int cch, UINT uiCodePage);
    int strlen();

protected:
    CStrInW();
    void Init(LPCSTR pstr, int cch, UINT uiCodePage);

    int _cwchLen;
	UINT _uiCodePage;
};

inline CStrInW::CStrInW()
{
}

inline int CStrInW::strlen()
{
    return _cwchLen;
}

class CStrOutW : public CConvertStrW
{
public:
    CStrOutW(LPSTR pstr, int cchBuf, UINT uiCodePage);
    ~CStrOutW();

    int     BufSize();
    int     Convert();

private:

    LPSTR  	_pstr;
    int     _cchBuf;
	UINT	_uiCodePage;
};

inline int CStrOutW::BufSize()
{
    return _cchBuf;
}

LRESULT W32ImpBase::WndProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	CTxtWinHost *ped = (CTxtWinHost *) GetWindowLong(hwnd, ibPed);

	#ifdef DEBUG
	Tracef(TRCSEVINFO, "hwnd %lx, msg %lx, wparam %lx, lparam %lx", hwnd, msg, wparam, lparam);
	#endif	// DEBUG
	
	switch(msg)
	{
	case WM_NCCREATE:
		return CTxtWinHost::OnNCCreate(hwnd, (CREATESTRUCT *) lparam);
		break;

	case WM_NCDESTROY:
		if( ped )
		{
			CTxtWinHost::OnNCDestroy(ped);
		}
		return 0;
	}
	
	return ped ? ped->TxWindowProc(hwnd, msg, wparam, lparam)
			   : DefWindowProc(hwnd, msg, wparam, lparam);
}

LONG ValidateTextRange(TEXTRANGE *pstrg);

LRESULT W32ImpBase::ANSIWndProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{

	TRACEBEGIN(TRCSUBSYSHOST, TRCSCOPEINTERN, "RichEditANSIWndProc");

	#ifdef DEBUG
	Tracef(TRCSEVINFO, "hwnd %lx, msg %lx, wparam %lx, lparam %lx", hwnd, msg, wparam, lparam);
	#endif	// DEBUG

	CTxtWinHost	*	ped = (CTxtWinHost *) GetWindowLong(hwnd, ibPed);
	LPARAM			lparamNew = 0;
	WPARAM			wparamNew = 0;
	CCharFormat		cf;
	DWORD			cpSelMin, cpSelMost;

	LRESULT			lres;

	switch( msg )
	{
	case WM_SETTEXT:
		{
			CStrInW strinw((char *)lparam, CP_ACP);

			return RichEditWndProc(hwnd, msg, wparam, (LPARAM)(WCHAR *)strinw);
		}
	case WM_CHAR:
		if( W32->UnicodeFromMbcs((LPWSTR)&wparamNew,
			                     1,
								 (char *)&wparam,
								 1,
								 W32->GetKeyboardCodePage()) == 1 )
		{
			wparam = wparamNew;
			goto def;
		}
		break;

	case EM_SETCHARFORMAT:
		if( cf.SetA((CHARFORMATA *)lparam) )
		{
			lparam = (LPARAM)&cf;
			goto def;
		}
		break;

	case EM_GETCHARFORMAT:
		RichEditWndProc(hwnd, msg, wparam, (LPARAM)&cf);
		// Convert CCharFormat to CHARFORMAT(2)A
		if (cf.GetA((CHARFORMATA *)lparam))
			return ((CHARFORMATA *)lparam)->dwMask;
		return 0;

	case EM_FINDTEXT:
	case EM_FINDTEXTEX:
		{
			// we cheat a little here because FINDTEXT and FINDTEXTEX overlap
			// with the exception of the extra out param chrgText in FINDTEXTEX

			FINDTEXTEXW ftexw;
			FINDTEXTA *pfta = (FINDTEXTA *)lparam;
			CStrInW strinw(pfta->lpstrText, W32->GetKeyboardCodePage());

			ftexw.chrg = pfta->chrg;
			ftexw.lpstrText = (WCHAR *)strinw;

			lres = WndProc(hwnd, msg, wparam, (LPARAM)&ftexw);
			
			if( msg == EM_FINDTEXTEX )
			{
				// in the FINDTEXTEX case, the extra field in the
				// FINDTEXTEX data structure is an out parameter indicating
				// the range where the text was found.  Update the 'real'
				// [in, out] parameter accordingly.	
				((FINDTEXTEXA *)lparam)->chrgText = ftexw.chrgText;
			}
			
			return lres;
		}
		break;				

	case EM_GETSELTEXT:
		{
			// we aren't told how big the incoming buffer is; only that it's
			// "big enough".  Since we know we are grabbing the selection,
			// we'll assume that the buffer is the size of the selection's
			// unicode data in bytes.
			WndProc(hwnd, EM_GETSEL, (WPARAM)&cpSelMin, 
				(LPARAM)&cpSelMost);

			CStrOutW stroutw((LPSTR)lparam, 
						(cpSelMost - cpSelMin)* sizeof(WCHAR), W32->GetKeyboardCodePage());
			return WndProc(hwnd, msg, wparam, 
						(LPARAM)(WCHAR *)stroutw);
		}
		break;

	case WM_GETTEXT:
		{
			// comvert WM_GETTEXT to ANSI using EM_GTETEXTEX
			GETTEXTEX gt;

			gt.cb = wparam;
			gt.flags = GT_USECRLF;
			gt.codepage = CP_ACP;
			gt.lpDefaultChar = NULL;
			gt.lpUsedDefChar = NULL;

			return WndProc(hwnd, EM_GETTEXTEX, (WPARAM)&gt, lparam);
		}
		break;

	case WM_GETTEXTLENGTH:
		{
			// convert WM_GETTEXTLENGTH to ANSI using EM_GETTEXTLENGTHEX
			GETTEXTLENGTHEX gtl;

			gtl.flags = GTL_NUMBYTES | GTL_PRECISE | GTL_USECRLF;
			gtl.codepage = CP_ACP;

			return WndProc(hwnd, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
		}
		break;

	case EM_GETTEXTRANGE:
		{
			TEXTRANGEA *ptrg = (TEXTRANGEA *)lparam;

            LONG clInBuffer = ValidateTextRange((TEXTRANGEW *) ptrg);

            // If size is -1, this means that the size required is the total
            // size of the the text.
            if (-1 == clInBuffer)
            {
                // We can get this length either by digging the data out of the
                // various structures below us or we can take advantage of the
                // WM_GETTEXTLENGTH message. The first might be slightly 
                // faster but the second definitely save code size. So we
                // will go with the second.
                clInBuffer = SendMessage(hwnd, WM_GETTEXTLENGTH, 0, 0);
            }

            if (0 == clInBuffer)
            {
                // The buffer was invalid for some reason or there was not data
                // to copy. In any case, we are done.
                return 0;
            }

            // Verify that the output buffer is big enough.
            if (IsBadWritePtr(ptrg->lpstrText, clInBuffer + 1))
            {
                // Not enough space so don't copy any
                return 0;
            }

			// For EM_GETTEXTRANGE case, we again don't know how big the incoming buffer is, only that
			// it should be *at least* as great as cpMax - cpMin in the
			// text range structure.  We also know that anything *bigger*
			// than (cpMax - cpMin)*2 bytes is uncessary.  So we'll just assume
			// that's it's "big enough" and let WideCharToMultiByte scribble
			// as much as it needs.  Memory shortages are the caller's 
			// responsibility (courtesy of the RichEdit1.0 design).
			CStrOutW stroutw( ptrg->lpstrText, (clInBuffer + 1) * sizeof(WCHAR), 
					CP_ACP );
			TEXTRANGEW trgw;
			trgw.chrg = ptrg->chrg;
			trgw.lpstrText = (WCHAR *)stroutw;

			if (WndProc(hwnd, EM_GETTEXTRANGE, wparam, (LPARAM)&trgw))
			{
				// need to return the number of BYTE converted.
				return stroutw.Convert();
			}
		}

	case EM_REPLACESEL:
		{
			CStrInW strinw((LPSTR)lparam, CP_ACP);
			return WndProc(hwnd, msg, wparam, (LPARAM)(WCHAR *)strinw);
		}

	case EM_GETLINE:
		{
			// the size is indicated by the first word of the memory pointed
			// to by lparam
			WORD size = *(WORD *)lparam;
			CStrOutW stroutw((char *)lparam, (DWORD)size, CP_ACP);
			WCHAR *pwsz = (WCHAR *)stroutw;
			*(WORD *)pwsz = size;

			return WndProc(hwnd, msg, wparam, (LPARAM)pwsz);
		}

	case WM_CREATE:
		{
			// the only thing we need to convert are the strings,
			// so just do a structure copy and replace the
			// strings. 
			CREATESTRUCTW csw = *(CREATESTRUCTW *)lparam;
			CREATESTRUCTA *pcsa = (CREATESTRUCTA *)lparam;
			CStrInW strinwName(pcsa->lpszName, W32->GetKeyboardCodePage());
			CStrInW strinwClass(pcsa->lpszClass, CP_ACP);

			csw.lpszName = (WCHAR *)strinwName;
			csw.lpszClass = (WCHAR *)strinwClass;

			return WndProc(hwnd, msg, wparam, (LPARAM)&csw);
		}

	default:
def:	return WndProc(hwnd, msg, wparam, lparam);
	}
	return 0;		// Something went wrong.
}

HGLOBAL WINAPI W32ImpBase::GlobalAlloc( UINT uFlags, DWORD dwBytes )
{
	#pragma message ("Review : Incomplete")
	return NULL;
}

HGLOBAL WINAPI W32ImpBase::GlobalFree( HGLOBAL hMem )
{
	#pragma message ("Review : Incomplete")
	return NULL;
}

UINT WINAPI W32ImpBase::GlobalFlags( HGLOBAL hMem )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HGLOBAL WINAPI W32ImpBase::GlobalReAlloc( HGLOBAL hMem, DWORD dwBytes, UINT uFlags )
{
	#pragma message ("Review : Incomplete")
	return NULL;
}

DWORD WINAPI W32ImpBase::GlobalSize( HGLOBAL hMem )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

LPVOID WINAPI W32ImpBase::GlobalLock( HGLOBAL hMem )
{
	#pragma message ("Review : Incomplete")
	return NULL;
}

HGLOBAL WINAPI W32ImpBase::GlobalHandle( LPCVOID pMem )
{
	#pragma message ("Review : Incomplete")
	return NULL;
}

BOOL WINAPI W32ImpBase::GlobalUnlock( HGLOBAL hMem )
{
	#pragma message ("Review : Incomplete")
	return FALSE;
}

void NLSImpBase::CheckChangeKeyboardLayout ( CTxtSelection *psel, BOOL fChangedFont )
{
	#pragma message ("Review : Incomplete")
	return;
}

void NLSImpBase::CheckChangeFont (
	CTxtSelection *psel,
	CTxtEdit * const ped,
	BOOL fEnableReassign,	// @parm Do we enable CTRL key?
	const WORD lcID,		// @parm LCID from WM_ message
	UINT cpg  				// @parm code page to use (could be ANSI for far east with IME off)
)
{
	#pragma message ("Review : Incomplete")
	return;
}

BOOL NLSImpBase::FormatMatchesKeyboard( const CCharFormat *pFormat )
{
	#pragma message ("Review : Incomplete")
	return FALSE;
}

HKL NLSImpBase::GetKeyboardLayout ( DWORD )
{
	#pragma message ("Review : Incomplete")
	return NULL;
}

int NLSImpBase::GetKeyboardLayoutList ( int, HKL FAR * )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::LoadRegTypeLib ( REFGUID, WORD, WORD, LCID, ITypeLib ** )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::LoadTypeLib ( const OLECHAR *, ITypeLib ** )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

BSTR OLEImpBase::SysAllocString ( const OLECHAR * )
{
	#pragma message ("Review : Incomplete")
	return NULL;
}

BSTR OLEImpBase::SysAllocStringLen ( const OLECHAR *, UINT )
{
	#pragma message ("Review : Incomplete")
	return NULL;
}

void OLEImpBase::SysFreeString ( BSTR )
{
	#pragma message ("Review : Incomplete")
	return;
}

UINT OLEImpBase::SysStringLen ( BSTR )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

void OLEImpBase::VariantInit ( VARIANTARG * )
{
	#pragma message ("Review : Incomplete")
	return;
}

HRESULT OLEImpBase::OleCreateFromData ( LPDATAOBJECT, REFIID, DWORD, LPFORMATETC, LPOLECLIENTSITE, LPSTORAGE, void ** )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

void OLEImpBase::CoTaskMemFree ( LPVOID )
{
	#pragma message ("Review : Incomplete")
	return;
}

HRESULT OLEImpBase::CreateBindCtx ( DWORD, LPBC * )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HANDLE OLEImpBase::OleDuplicateData ( HANDLE, CLIPFORMAT, UINT )
{
	#pragma message ("Review : Incomplete")
	return NULL;
}

HRESULT OLEImpBase::CoTreatAsClass ( REFCLSID, REFCLSID )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::ProgIDFromCLSID ( REFCLSID, LPOLESTR * )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::OleConvertIStorageToOLESTREAM ( LPSTORAGE, LPOLESTREAM )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::OleConvertIStorageToOLESTREAMEx ( LPSTORAGE, CLIPFORMAT, LONG, LONG, DWORD, LPSTGMEDIUM, LPOLESTREAM )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::OleSave ( LPPERSISTSTORAGE, LPSTORAGE, BOOL )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::StgCreateDocfileOnILockBytes ( ILockBytes *, DWORD, DWORD, IStorage ** )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::CreateILockBytesOnHGlobal ( HGLOBAL, BOOL, ILockBytes ** )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::OleCreateLinkToFile ( LPCOLESTR, REFIID, DWORD, LPFORMATETC, LPOLECLIENTSITE, LPSTORAGE, void ** )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

LPVOID OLEImpBase::CoTaskMemAlloc ( ULONG )
{
	#pragma message ("Review : Incomplete")
	return NULL;
}

LPVOID OLEImpBase::CoTaskMemRealloc ( LPVOID, ULONG )
{
	#pragma message ("Review : Incomplete")
	return NULL;
}

HRESULT OLEImpBase::OleInitialize ( LPVOID )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

void OLEImpBase::OleUninitialize ( )
{
	#pragma message ("Review : Incomplete")
	return;
}

HRESULT OLEImpBase::OleSetClipboard ( IDataObject * )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::OleFlushClipboard ( )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::OleIsCurrentClipboard ( IDataObject * )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::DoDragDrop ( IDataObject *, IDropSource *, DWORD, DWORD * )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::OleGetClipboard ( IDataObject ** )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::RegisterDragDrop ( HWND, IDropTarget * )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::OleCreateLinkFromData ( IDataObject *, REFIID, DWORD, LPFORMATETC, IOleClientSite *, IStorage *, void ** )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::OleCreateStaticFromData ( IDataObject *, REFIID, DWORD, LPFORMATETC, IOleClientSite *, IStorage *, void ** )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::OleDraw ( IUnknown *, DWORD, HDC, LPCRECT )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::OleSetContainedObject ( IUnknown *, BOOL )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::CoDisconnectObject ( IUnknown *, DWORD )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::WriteFmtUserTypeStg ( IStorage *, CLIPFORMAT, LPOLESTR )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::WriteClassStg ( IStorage *, REFCLSID )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::SetConvertStg ( IStorage *, BOOL )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::ReadFmtUserTypeStg ( IStorage *, CLIPFORMAT *, LPOLESTR * )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::ReadClassStg ( IStorage *pstg, CLSID * )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::OleRun ( IUnknown * )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::RevokeDragDrop ( HWND )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::CreateStreamOnHGlobal ( HGLOBAL, BOOL, IStream ** )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::GetHGlobalFromStream ( IStream *pstm, HGLOBAL * )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::OleCreateDefaultHandler ( REFCLSID, IUnknown *, REFIID, void ** )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::CLSIDFromProgID ( LPCOLESTR, LPCLSID )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::OleConvertOLESTREAMToIStorage ( LPOLESTREAM, IStorage *, const DVTARGETDEVICE * )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::OleLoad ( IStorage *, REFIID, IOleClientSite *, void ** )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HRESULT OLEImpBase::ReleaseStgMedium ( LPSTGMEDIUM )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

BOOL IMEImpBase::ImmInitialize( void )
{
	#pragma message ("Review : Incomplete")
	return FALSE;
}

void IMEImpBase::ImmTerminate( void )
{
	#pragma message ("Review : Incomplete")
	return;
}

LONG IMEImpBase::ImmGetCompositionStringA ( HIMC, DWORD, LPVOID, DWORD )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HIMC IMEImpBase::ImmGetContext ( HWND )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

BOOL IMEImpBase::ImmSetCompositionFontA ( HIMC, LPLOGFONTA )
{
	#pragma message ("Review : Incomplete")
	return FALSE;
}

BOOL IMEImpBase::ImmSetCompositionWindow ( HIMC, LPCOMPOSITIONFORM )
{
	#pragma message ("Review : Incomplete")
	return FALSE;
}

BOOL IMEImpBase::ImmReleaseContext ( HWND, HIMC )
{
	#pragma message ("Review : Incomplete")
	return FALSE;
}

DWORD IMEImpBase::ImmGetProperty ( HKL, DWORD )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

BOOL IMEImpBase::ImmGetCandidateWindow ( HIMC, DWORD, LPCANDIDATEFORM )
{
	#pragma message ("Review : Incomplete")
	return FALSE;
}

BOOL IMEImpBase::ImmSetCandidateWindow ( HIMC, LPCANDIDATEFORM )
{
	#pragma message ("Review : Incomplete")
	return FALSE;
}

BOOL IMEImpBase::ImmNotifyIME ( HIMC, DWORD, DWORD, DWORD )
{
	#pragma message ("Review : Incomplete")
	return FALSE;
}

HIMC IMEImpBase::ImmAssociateContext ( HWND, HIMC )
{
	#pragma message ("Review : Incomplete")
	return NULL;
}

UINT IMEImpBase::ImmGetVirtualKey ( HWND )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

HIMC IMEImpBase::ImmEscape ( HKL, HIMC, UINT, LPVOID )
{
	#pragma message ("Review : Incomplete")
	return NULL;
}

LONG IMEImpBase::ImmGetOpenStatus ( HIMC )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

BOOL IMEImpBase::ImmGetConversionStatus ( HIMC, LPDWORD, LPDWORD )
{
	#pragma message ("Review : Incomplete")
	return FALSE;
}

BOOL IMEImpBase::FSupportSty ( UINT, UINT )
{
	#pragma message ("Review : Incomplete")
	return FALSE;
}

const IMESTYLE * IMEImpBase::PIMEStyleFromAttr ( const UINT )
{
	#pragma message ("Review : Incomplete")
	return NULL;
}

const IMECOLORSTY * IMEImpBase::PColorStyleTextFromIMEStyle ( const IMESTYLE * )
{
	#pragma message ("Review : Incomplete")
	return NULL;
}

const IMECOLORSTY * IMEImpBase::PColorStyleBackFromIMEStyle ( const IMESTYLE * )
{
	#pragma message ("Review : Incomplete")
	return NULL;
}

BOOL IMEImpBase::FBoldIMEStyle ( const IMESTYLE * )
{
	#pragma message ("Review : Incomplete")
	return FALSE;
}

BOOL IMEImpBase::FItalicIMEStyle ( const IMESTYLE * )
{
	#pragma message ("Review : Incomplete")
	return FALSE;
}

BOOL IMEImpBase::FUlIMEStyle ( const IMESTYLE * )
{
	#pragma message ("Review : Incomplete")
	return FALSE;
}

UINT IMEImpBase::IdUlIMEStyle ( const IMESTYLE * )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

COLORREF IMEImpBase::RGBFromIMEColorStyle ( const IMECOLORSTY * )
{
	#pragma message ("Review : Incomplete")
	return 0;
}

//+---------------------------------------------------------------------------
//
//  Member:     CStrIn::CStrIn
//
//  Synopsis:   Inits the class.
//
//  NOTE:       Don't inline these functions or you'll increase code size
//              by pushing -1 on the stack for each call.
//
//----------------------------------------------------------------------------

CStrIn::CStrIn(LPCWSTR pwstr)
{
	TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CStrIn::CStrIn");

    Init(pwstr, -1);
}

CStrIn::CStrIn(LPCWSTR pwstr, int cwch)
{
	TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CStrIn::CStrIn");

    Init(pwstr, cwch);
}


//+---------------------------------------------------------------------------
//
//  Member:     CStrIn::Init
//
//  Synopsis:   Converts a LPWSTR function argument to a LPSTR.
//
//  Arguments:  [pwstr] -- The function argument.  May be NULL or an atom
//                              (HIWORD(pwstr) == 0).
//
//              [cwch]  -- The number of characters in the string to
//                          convert.  If -1, the string is assumed to be
//                          NULL terminated and its length is calculated.
//
//  Modifies:   [this]
//
//----------------------------------------------------------------------------

void
CStrIn::Init(LPCWSTR pwstr, int cwch)
{
	TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CStrIn::Init");

    int cchBufReq;

    _cchLen = 0;

    // Check if string is NULL or an atom.
    if (HIWORD(pwstr) == 0)
    {
        _pstr = (LPSTR) pwstr;
        return;
    }

    Assert(cwch == -1 || cwch > 0);

    //
    // Convert string to preallocated buffer, and return if successful.
    //

    _cchLen = W32->MbcsFromUnicode(_ach, ARRAY_SIZE(_ach), pwstr, cwch);

    if (_cchLen > 0)
    {
        if(_ach[_cchLen-1] == 0)
            _cchLen--;                // account for terminator
        _pstr = _ach;
        return;
    }

    //
    // Alloc space on heap for buffer.
    //

    TRACEINFOSZ("CStrIn: Allocating buffer for wrapped function argument.");

    cchBufReq = WideCharToMultiByte(
            CP_ACP, 0, pwstr, cwch, NULL, 0,  NULL, NULL);

    Assert(cchBufReq > 0);
    _pstr = new char[cchBufReq];
    if (!_pstr)
    {
        // On failure, the argument will point to the empty string.
        TRACEINFOSZ("CStrIn: No heap space for wrapped function argument.");
        _ach[0] = 0;
        _pstr = _ach;
        return;
    }

    Assert(HIWORD(_pstr));
    _cchLen = -1 + W32->MbcsFromUnicode(_pstr, cchBufReq, pwstr, cwch);

    Assert(_cchLen >= 0);
}



//+---------------------------------------------------------------------------
//
//  Class:      CStrInMulti (CStrIM)
//
//  Purpose:    Converts multiple strings which are terminated by two NULLs,
//              e.g. "Foo\0Bar\0\0"
//
//----------------------------------------------------------------------------

class CStrInMulti : public CStrIn
{
public:
    CStrInMulti(LPCWSTR pwstr);
};



//+---------------------------------------------------------------------------
//
//  Member:     CStrInMulti::CStrInMulti
//
//  Synopsis:   Converts mulitple LPWSTRs to a multiple LPSTRs.
//
//  Arguments:  [pwstr] -- The strings to convert.
//
//  Modifies:   [this]
//
//----------------------------------------------------------------------------

CStrInMulti::CStrInMulti(LPCWSTR pwstr)
{
	TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CStrInMulti::CStrInMulti");

    LPCWSTR pwstrT;

    // We don't handle atoms because we don't need to.
    Assert(HIWORD(pwstr));

    //
    // Count number of characters to convert.
    //

    pwstrT = pwstr;
    if (pwstr)
    {
        do {
            while (*pwstrT++)
                ;

        } while (*pwstrT++);
    }

    Init(pwstr, pwstrT - pwstr);
}

//+---------------------------------------------------------------------------
//
//  Member:     CStrOut::CStrOut
//
//  Synopsis:   Allocates enough space for an out buffer.
//
//  Arguments:  [pwstr]   -- The Unicode buffer to convert to when destroyed.
//                              May be NULL.
//
//              [cwchBuf] -- The size of the buffer in characters.
//
//  Modifies:   [this].
//
//----------------------------------------------------------------------------

CStrOut::CStrOut(LPWSTR pwstr, int cwchBuf)
{
	TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CStrOut::CStrOut");

    Assert(cwchBuf >= 0);

    _pwstr = pwstr;
    _cwchBuf = cwchBuf;

    if (!pwstr)
    {
        Assert(cwchBuf == 0);
        _pstr = NULL;
        return;
    }

    Assert(HIWORD(pwstr));

    // Initialize buffer in case Windows API returns an error.
    _ach[0] = 0;

    // Use preallocated buffer if big enough.
    if (cwchBuf * 2 <= ARRAY_SIZE(_ach))
    {
        _pstr = _ach;
        return;
    }

    // Allocate buffer.
    TRACEINFOSZ("CStrOut: Allocating buffer for wrapped function argument.");
    _pstr = new char[cwchBuf * 2];
    if (!_pstr)
    {
        //
        // On failure, the argument will point to a zero-sized buffer initialized
        // to the empty string.  This should cause the Windows API to fail.
        //

        TRACEINFOSZ("CStrOut: No heap space for wrapped function argument.");
        Assert(cwchBuf > 0);
        _pwstr[0] = 0;
        _cwchBuf = 0;
        _pstr = _ach;
        return;
    }

    Assert(HIWORD(_pstr));
    _pstr[0] = 0;
}



//+---------------------------------------------------------------------------
//
//  Member:     CStrOut::Convert
//
//  Synopsis:   Converts the buffer from MBCS to Unicode.
//
//----------------------------------------------------------------------------

int
CStrOut::Convert()
{
	TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CStrOut::Convert");

    int cch;

    if (!_pstr)
        return 0;

    cch = MultiByteToWideChar(CP_ACP, 0, _pstr, -1, _pwstr, _cwchBuf);
    Assert(cch > 0 || _cwchBuf == 0);

    Free();
    return cch - 1;
}



//+---------------------------------------------------------------------------
//
//  Member:     CStrOut::~CStrOut
//
//  Synopsis:   Converts the buffer from MBCS to Unicode.
//
//  Note:       Don't inline this function, or you'll increase code size as
//              both Convert() and CConvertStr::~CConvertStr will be called
//              inline.
//
//----------------------------------------------------------------------------

CStrOut::~CStrOut()
{
	TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CStrOut::~CStrOut");

    Convert();
}


//
//	MultiByte --> UNICODE routins
//

//+---------------------------------------------------------------------------
//
//  Member:     CConvertStr::Free
//
//  Synopsis:   Frees string if alloc'd and initializes to NULL.
//
//----------------------------------------------------------------------------

void
CConvertStr::Free()
{
	TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CConvertStr::Free");

    if (_pstr != _ach && HIWORD(_pstr) != 0)
    {
        delete [] _pstr;
    }

    _pstr = NULL;
}

//+---------------------------------------------------------------------------
//
//  Member:     CConvertStrW::Free
//
//  Synopsis:   Frees string if alloc'd and initializes to NULL.
//
//----------------------------------------------------------------------------

void
CConvertStrW::Free()
{
	TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CConvertStrW::Free");

    if (_pwstr != _awch && HIWORD(_pwstr) != 0 )
    {
        delete [] _pwstr;
    }

    _pwstr = NULL;
}



//+---------------------------------------------------------------------------
//
//  Member:     CStrInW::CStrInW
//
//  Synopsis:   Inits the class.
//
//  NOTE:       Don't inline these functions or you'll increase code size
//              by pushing -1 on the stack for each call.
//
//----------------------------------------------------------------------------

CStrInW::CStrInW(LPCSTR pstr)
{
	TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CStrInW::CStrInW");

    Init(pstr, -1, CP_ACP);
}

CStrInW::CStrInW(LPCSTR pstr, UINT uiCodePage)
{
	TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CStrInW::CStrInW");

    Init(pstr, -1, uiCodePage);
}

CStrInW::CStrInW(LPCSTR pstr, int cch, UINT uiCodePage)
{
	TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CStrInW::CStrInW");

    Init(pstr, cch, uiCodePage);
}


//+---------------------------------------------------------------------------
//
//  Member:     CStrInW::Init
//
//  Synopsis:   Converts a LPSTR function argument to a LPWSTR.
//
//  Arguments:  [pstr] -- The function argument.  May be NULL or an atom
//                              (HIWORD(pwstr) == 0).
//
//              [cch]  -- The number of characters in the string to
//                          convert.  If -1, the string is assumed to be
//                          NULL terminated and its length is calculated.
//
//  Modifies:   [this]
//
//----------------------------------------------------------------------------

void
CStrInW::Init(LPCSTR pstr, int cch, UINT uiCodePage)
{
	TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CStrInW::Init");

    int cchBufReq;

    _cwchLen = 0;

    // Check if string is NULL or an atom.
    if (HIWORD(pstr) == 0)
    {
        _pwstr = (LPWSTR) pstr;
        return;
    }

    Assert(cch == -1 || cch > 0);

    //
    // Convert string to preallocated buffer, and return if successful.
    //

    _cwchLen = MultiByteToWideChar(
            uiCodePage, 0, pstr, cch, _awch, ARRAY_SIZE(_awch));

    if (_cwchLen > 0)
    {
        if(_awch[_cwchLen-1] == 0)
            _cwchLen--;                // account for terminator
        _pwstr = _awch;
        return;
    }

    //
    // Alloc space on heap for buffer.
    //

    TRACEINFOSZ("CStrInW: Allocating buffer for wrapped function argument.");

    cchBufReq = MultiByteToWideChar(
            CP_ACP, 0, pstr, cch, NULL, 0);

    Assert(cchBufReq > 0);
    _pwstr = new WCHAR[cchBufReq];
    if (!_pwstr)
    {
        // On failure, the argument will point to the empty string.
        TRACEINFOSZ("CStrInW: No heap space for wrapped function argument.");
        _awch[0] = 0;
        _pwstr = _awch;
        return;
    }

    Assert(HIWORD(_pwstr));
    _cwchLen = -1 + MultiByteToWideChar(
            uiCodePage, 0, pstr, cch, _pwstr, cchBufReq);
    Assert(_cwchLen >= 0);
}


//+---------------------------------------------------------------------------
//
//  Member:     CStrOutW::CStrOutW
//
//  Synopsis:   Allocates enough space for an out buffer.
//
//  Arguments:  [pstr]   -- The ansi buffer to convert to when destroyed.
//                              May be NULL.
//
//              [cchBuf] -- The size of the buffer in characters.
//
//  Modifies:   [this].
//
//----------------------------------------------------------------------------

CStrOutW::CStrOutW(LPSTR pstr, int cchBuf, UINT uiCodePage)
{
	TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CStrOutW::CStrOutW");

    Assert(cchBuf >= 0);

    _pstr = pstr;
    _cchBuf = cchBuf;
	_uiCodePage = uiCodePage;

    if (!pstr)
    {
        Assert(cchBuf == 0);
        _pwstr = NULL;
        return;
    }

    Assert(HIWORD(pstr));

    // Initialize buffer in case Windows API returns an error.
    _awch[0] = 0;

    // Use preallocated buffer if big enough.
    if (cchBuf <= ARRAY_SIZE(_awch))
    {
        _pwstr = _awch;
        return;
    }

    // Allocate buffer.
    TRACEINFOSZ("CStrOutW: Allocating buffer for wrapped function argument.");
    _pwstr = new WCHAR[cchBuf * 2];
    if (!_pwstr)
    {
        //
        // On failure, the argument will point to a zero-sized buffer initialized
        // to the empty string.  This should cause the Windows API to fail.
        //

        TRACEINFOSZ("CStrOutW: No heap space for wrapped function argument.");
        Assert(cchBuf > 0);
        _pstr[0] = 0;
        _cchBuf = 0;
        _pwstr = _awch;
        return;
    }

    Assert(HIWORD(_pwstr));
    _pwstr[0] = 0;
}



//+---------------------------------------------------------------------------
//
//  Member:     CStrOutW::Convert
//
//  Synopsis:   Converts the buffer from Unicode to MBCS
//
//----------------------------------------------------------------------------

int
CStrOutW::Convert()
{
	TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CStrOutW::Convert");

    int cch;

    if (!_pwstr)
        return 0;

	WCHAR *pwstr = _pwstr;
	int cchBuf = _cchBuf;

	cch = W32->MbcsFromUnicode(_pstr, cchBuf, _pwstr, -1, _uiCodePage);

    Free();
    return cch - 1;
}



//+---------------------------------------------------------------------------
//
//  Member:     CStrOutW::~CStrOutW
//
//  Synopsis:   Converts the buffer from Unicode to MBCS.
//
//  Note:       Don't inline this function, or you'll increase code size as
//              both Convert() and CConvertStr::~CConvertStr will be called
//              inline.
//
//----------------------------------------------------------------------------

CStrOutW::~CStrOutW()
{
	TRACEBEGIN(TRCSUBSYSWRAP, TRCSCOPEINTERN, "CStrOutW::~CStrOutW");

    Convert();
}

 #if 0

	// flags
#pragma message ("JMO Review : Should be same as has nlsprocs, initialize in constructor")
	unsigned _fIntlKeyboard : 1;

// From nlsprocs.h

// TranslateCharsetInfo
// typedef WINGDIAPI BOOL (WINAPI*TCI_CAST)( DWORD FAR *, LPCHARSETINFO, DWORD);
// #define	pTranslateCharsetInfo(a,b,c) (( TCI_CAST) nlsProcTable[iTranslateCharsetInfo])(a,b,c)

//#define	pTranslateCharsetInfo() (*()nlsProcTable[iTranslateCharsetInfo])()


/*
 *	W32Imp::CheckChangeKeyboardLayout ( BOOL fChangedFont )
 *
 *	@mfunc
 *		Change keyboard for new font, or font at new character position.
 *	@comm
 *		Using only the currently loaded KBs, locate one that will support
 *		the insertion points font. This is called anytime a character format
 *		change occurs, or the insert font (caret position) changes.
 *	@devnote
 *		The current KB is preferred. If a previous association
 *		was made, see if the KB is still loaded in the system and if so use
 *		it. Otherwise, locate a suitable KB, preferring KB's that have
 *		the same charset ID as their default, preferred charset. If no match
 *		can be made then nothing changes.
 *
 *		This routine is only useful on Windows 95.
 */
#ifndef MACPORT


#define MAX_HKLS 256								// It will be awhile
													//  before we have more KBs
	CTxtEdit * const ped = GetPed();				// Document context.

	INT			i, totalLayouts,					// For matching KBs.
				iBestLayout = -1;

	WORD		preferredKB;						// LCID of preferred KB.

	HKL			hklList[MAX_HKLS];					// Currently loaded KBs.

	const CCharFormat *pcf;							// Current font.
	CHARSETINFO	csi;								// Font's CodePage bits.

	AssertSz(ped, "no ped?");						// hey now!

	if (!ped || !ped->IsRich() || !ped->_fFocus || 					// EXIT if no ped or focus or
		!ped->IsAutoKeyboard())						// auto keyboard is turn off
		return;

	pcf = ped->GetCharFormat(_iFormat);				// Get insert font's data

	hklList[0]		= pGetKeyboardLayout(0);		// Current hkl preferred?
	preferredKB		= fc().GetPreferredKB( pcf->bCharSet );
	if ( preferredKB != LOWORD(hklList[0]) )		// Not correct KB?
	{
													// Get loaded HKLs.
		totalLayouts	= 1 + pGetKeyboardLayoutList(MAX_HKLS, &hklList[1]);
													// Know which KB?
		if ( preferredKB )							//  then locate it.
		{											// Sequential match because
			for ( i = 1; i < totalLayouts; i++ )	//  HKL may be unloaded.
			{										// Match LCIDs.
				if ( preferredKB == LOWORD( hklList[i]) )
				{
					iBestLayout = i;
					break;							// Matched it.
				}
			}
			if ( i >= totalLayouts )				// Old KB is missing.
			{										// Reset to locate new KB.
				fc().SetPreferredKB ( pcf->bCharSet, 0 );
			}
		}
		if ( iBestLayout < 0 )							// Attempt to find new KB.
		{
			for ( i = 0; i < totalLayouts; i++ )
			{										
				pTranslateCharsetInfo(				// Get KB's charset.
						(DWORD *) ConvertLanguageIDtoCodePage(LOWORD(hklList[iBestLayout])),
						&csi, TCI_SRCCODEPAGE);
													
				if( csi.ciCharset == pcf->bCharSet)	// If charset IDs match?
				{
					iBestLayout = i;
					break;							//  then this KB is best.
				}
			}
			if ( iBestLayout >= 0)					// Bind new KB.
			{
				fChangedFont = TRUE;
				fc().SetPreferredKB(pcf->bCharSet, LOWORD(hklList[iBestLayout]));
			}
		}
		if ( fChangedFont && iBestLayout >= 0)			// Bind font.
		{
			ICharFormatCache *	pCF;

			if(SUCCEEDED(GetCharFormatCache(&pCF)))
			{
				pCF->AddRefFormat(_iFormat);
				fc().SetPreferredFont(
						LOWORD(hklList[iBestLayout]), _iFormat );
			}
		}
		if( iBestLayout > 0 )							// If == 0 then
		{												//  it's already active.
														// Activate KB.
			ActivateKeyboardLayout( hklList[iBestLayout], 0);
		}
	}
#endif // MACPORT -- the mac needs its own code.

/*
 *	CTxtSelection::CheckChangeFont ( CTxtEdit * const ped, const WORD lcID )
 *
 *	@mfunc
 *		Change font for new keyboard layout.
 *	@comm
 *		If no previous preferred font has been associated with this KB, then
 *		locate a font in the document suitable for this KB. 
 *	@devnote
 *		This routine is called via WM_INPUTLANGCHANGEREQUEST message
 *		(a keyboard layout switch). This routine can also be called
 *		from WM_INPUTLANGCHANGE, but we are called more, and so this
 *		is less efficient.
 *
 *		Exact match is done via charset ID bitmask. If a match was previously
 *		made, use it. A user can force the insertion font to be associated
 *		to a keyboard if the control key is held through the KB changing
 *		process. The association is broken when another KB associates to
 *		the font. If no match can be made then nothing changes.
 *
 *		This routine is only useful on Windows 95.
 *
 */
#ifndef MACPORT

	LOCALESIGNATURE	ls, curr_ls;					// KB's code page bits.

	CCharFormat		cf,								// For creating new font.
					currCF;							// For searching
	const CCharFormat	*pPreferredCF;
	CHARSETINFO		csi;							//  with code page bits.

	LONG			iFormat, iBestFormat = -1;		// Loop support.
	INT				i;

	BOOL			fLastTime;						// Loop support.
	BOOL			fSetPreferred = FALSE;

	HKL				currHKL;						// current KB;

	BOOL			fLookUpFaceName = FALSE;		// when picking a new font.

	ICharFormatCache *	pCF;
	
#ifdef UNDER_CE // mikegins
	LPCTSTR			lpszFaceName = NULL;
#else
	LPTSTR			lpszFaceName = NULL;
#endif
	BYTE			bCharSet;
	BYTE			bPitchAndFamily;
	BOOL			fFaceNameIsDBCS;

	AssertSz (ped, "No ped?");

	if (!ped->IsRich() || !ped->IsAutoFont())		// EXIT if not running W95.	
		return;										// EXIT if auto font is turn off
	
	if(FAILED(GetCharFormatCache(&pCF)))			// Get CharFormat Cache.
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

	if ( fReassign )								// Force font/KB assoc.
	{												// If font supports KB
													//  in any way,
													// Note: test Unicode bits.
		GetLocaleInfoA( fc().GetPreferredKB (currCF.bCharSet),
				LOCALE_FONTSIGNATURE, (char *) &curr_ls, sizeof(ls));
		if ( CountMatchingBits(curr_ls.lsUsb, ls.lsUsb, 4) )
		{											// Break old font/KB assoc.
			fc().SetPreferredFont( fc().GetPreferredKB (currCF.bCharSet), -1 );
													// Bind KB and font.
			fc().SetPreferredKB( currCF.bCharSet, lcID );

			pCF->AddRefFormat(_iFormat);
			fc().SetPreferredFont( lcID, _iFormat );
		}
	}
	else											// Lookup preferred font.
	{
													// Attempt to Find new
		{											//  preferred font.
			CFormatRunPtr rp(_rpCF);				// Nondegenerate range

			fLastTime = TRUE;
			if ( _rpCF.IsValid() )					// If doc has cf runs.
			{
				fLastTime = FALSE;
				rp.AdjustBackward();
			}
			pTranslateCharsetInfo(					//  charset.
						(DWORD *)cpg, &csi, TCI_SRCCODEPAGE);
			
			iFormat = _iFormat;						// Search _iFormat,
													//  then backwards.
			i = MAX_RUNTOSEARCH;					// Don't be searching for
			while ( 1 )								//  years...
			{										// Get code page bits.
				pPreferredCF = ped->GetCharFormat(iFormat);
													
				if (csi.ciCharset == pPreferredCF->bCharSet)	// Equal charset ids?
				{
					fSetPreferred = TRUE;
					break;
				}
				if ( fLastTime )					// Done searching?
					break;
				iFormat = rp.GetFormat();			// Keep searching backward.
				fLastTime = !rp.PrevRun() && i--;
			}
			if ( !fSetPreferred && _rpCF.IsValid())	// Try searching forward.
			{
				rp = _rpCF;
				rp.AdjustBackward();
				i = MAX_RUNTOSEARCH;				// Don't be searching for
				while (i-- && rp.NextRun() )		//  years...
				{
					iFormat = rp.GetFormat();		// Get code page bits.
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
		//	unless explicitly set below.
		fFaceNameIsDBCS = FALSE;
		
		if ( fSetPreferred )
		{
			// pick face name from the previous preferred format
			bPitchAndFamily = pPreferredCF->bPitchAndFamily;
			bCharSet = pPreferredCF->bCharSet;
			lpszFaceName = (LPTSTR)pPreferredCF->szFaceName;
			fFaceNameIsDBCS = pPreferredCF->bInternalEffects & CFMI_FACENAMEISDBCS;
		}
		else											// Still don't have a font?
		{												//  For FE, use hard coded defaults.
														//  else get charset right.
			WORD CurrentCodePage = cpg;

			switch (CurrentCodePage)
			{											// FE hard codes from Word.
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

			default:									// Use translate to get
				pTranslateCharsetInfo(					//  charset.
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
				fLookUpFaceName = TRUE;					// Get Font's real name.

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
		cf.lcid	= lcID;

		// If we relied on GDI to match a font, get the font's real name...
		if ( fLookUpFaceName )
		{
			const CDevDesc		*pdd = _pdp->GetDdRender();
			HDC					hdc;
			CCcs				*pccs;
			HFONT				hfontOld;
			OUTLINETEXTMETRICA	*potm;
			CTempBuf			mem;
			UINT				otmSize;

 			hdc = pdd->GetDC();
												// Select logfont into DC,
			if( hdc)							//  for OutlineTextMetrics.
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
#endif // MACPORT -- the mac needs its own code.

				if (W32->FormatMatchesKeyboard(pcfForward))
				LOCALESIGNATURE ls;								// Per HKL, CodePage bits.
				CHARSETINFO	csi;								// Font's CodePage bits.

				// Font's code page bits.
				pTranslateCharsetInfo((DWORD *) pcfForward->bCharSet, &csi, TCI_SRCCHARSET);
				// Current KB's code page bits.
				GetLocaleInfoA( LOWORD(pGetKeyboardLayout(0)), LOCALE_FONTSIGNATURE, (CHAR *) &ls, sizeof(ls));
				if ( (csi.fs.fsCsb[0] & ls.lsCsbDefault[0]) ||
							(csi.fs.fsCsb[1] & ls.lsCsbDefault[1]) )
#endif


