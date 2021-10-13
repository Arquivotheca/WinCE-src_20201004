//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// debug.Cpp
//=--------------------------------------------------------------------------=
//
//  debug stuff
//
#include "ipserver.h"
#include <stdlib.h>

// for ASSERT and FAIL
//
SZTHISFILE

UINT g_cAlloc = 0;
UINT g_cAllocBstr = 0;
struct MemNode *g_pmemnodeFirst = NULL;
struct BstrNode *g_pbstrnodeFirst = NULL;

// debug memory tracking
struct MemNode
{
    MemNode *m_pmemnodeNext;
    UINT m_cAlloc;
    size_t m_nSize;
    LPCSTR m_lpszFileName;
    int m_nLine;
    VOID *m_pv;

    MemNode() { 
      m_pmemnodeNext = NULL;
      m_cAlloc = 0;
      m_pv = NULL;
      m_nSize = 0;
      m_lpszFileName = NULL;
      m_nLine = 0;
    }
};

void AddMemNode(void *pv, size_t nSize, LPCSTR lpszFileName, int nLine)
{
    MemNode *pmemnode;
#ifdef DEBUG    
    HRESULT hresult = NOERROR;
#endif

    pmemnode = (MemNode *)::operator new(sizeof(MemNode));
    if (pmemnode == NULL) {
      ASSERT(hresult == NOERROR, L"OOM");
    }
    else {
      // cons
      pmemnode->m_pv = pv;
      pmemnode->m_cAlloc = g_cAlloc;
      pmemnode->m_nSize = nSize;
      pmemnode->m_lpszFileName = lpszFileName;
      pmemnode->m_nLine = nLine;
      pmemnode->m_pmemnodeNext = g_pmemnodeFirst;
      g_pmemnodeFirst = pmemnode;
    }
    return;
}

VOID RemMemNode(void *pv)
{
    MemNode *pmemnodeCur = g_pmemnodeFirst;
    MemNode *pmemnodePrev = NULL;

    while (pmemnodeCur) {
      if (pmemnodeCur->m_pv == pv) {

        // remove
        if (pmemnodePrev) {
          pmemnodePrev->m_pmemnodeNext = pmemnodeCur->m_pmemnodeNext;
        }
        else {
          g_pmemnodeFirst = pmemnodeCur->m_pmemnodeNext;
        }
        ::operator delete(pmemnodeCur);
        break;
      }
      pmemnodePrev = pmemnodeCur;
      pmemnodeCur = pmemnodeCur->m_pmemnodeNext;
    } // while
    return;
}


void* __cdecl operator new(
    size_t nSize, 
    LPCSTR lpszFileName, 
    int nLine)
{
    void *pv = malloc(nSize);
    
#if DEBUG
    g_cAlloc++;
    if (pv) {
      AddMemNode(pv, nSize, lpszFileName, nLine);
    }
#endif // DEBUG
    return pv;
}

void __cdecl operator delete(void* pv)
{
#if DEBUG
    RemMemNode(pv);    
#endif // DEBUG    
    free(pv);
}


void DumpMemLeaks()                
{
    MemNode *pmemnodeCur = g_pmemnodeFirst;
    WCHAR wszMessage[_MAX_PATH];

    ASSERT(pmemnodeCur == NULL, L"operator new leaked: View | Output");
    while (pmemnodeCur != NULL) {
      // assume the debugger or auxiliary port
      // WIN95: can use ANSI versions on NT as well...
      //
      wsprintf(wszMessage, 
                L"operator new leak: pv %x cAlloc %u File %hs, Line %d\n",
                 pmemnodeCur->m_pv,
                 pmemnodeCur->m_cAlloc,
	         pmemnodeCur->m_lpszFileName, 
                 pmemnodeCur->m_nLine);
      OutputDebugString(wszMessage);
      pmemnodeCur = pmemnodeCur->m_pmemnodeNext;
    }    
}


// BSTR debugging...
struct BstrNode
{
    BstrNode *m_pbstrnodeNext;
    UINT m_cAlloc;
    size_t m_nSize;
    VOID *m_pv;

    BstrNode() { 
      m_pbstrnodeNext = NULL;
      m_cAlloc = 0;
      m_pv = NULL;
      m_nSize = 0;
    }
};

void AddBstrNode(void *pv, size_t nSize)
{
    BstrNode *pbstrnode;
#ifdef DEBUG
    HRESULT hresult = NOERROR;
#endif

    pbstrnode = (BstrNode *)::operator new(sizeof(BstrNode));
    if (pbstrnode == NULL) {
      ASSERT(hresult == NOERROR, L"OOM");
    }
    else {
      // cons
      pbstrnode->m_pv = pv;
      pbstrnode->m_cAlloc = g_cAllocBstr;
      pbstrnode->m_nSize = nSize;
      pbstrnode->m_pbstrnodeNext = g_pbstrnodeFirst;
      g_pbstrnodeFirst = pbstrnode;
    }
    return;
}

VOID RemBstrNode(void *pv)
{
    BstrNode *pbstrnodeCur = g_pbstrnodeFirst;
    BstrNode *pbstrnodePrev = NULL;

    if (pv == NULL) {
      return;
    }
    while (pbstrnodeCur) {
      if (pbstrnodeCur->m_pv == pv) {

        // remove
        if (pbstrnodePrev) {
          pbstrnodePrev->m_pbstrnodeNext = pbstrnodeCur->m_pbstrnodeNext;
        }
        else {
          g_pbstrnodeFirst = pbstrnodeCur->m_pbstrnodeNext;
        }
        ::operator delete(pbstrnodeCur);
        break;
      }
      pbstrnodePrev = pbstrnodeCur;
      pbstrnodeCur = pbstrnodeCur->m_pbstrnodeNext;
    } // while
    return;
}

void DebSysFreeString(BSTR bstr)
{
    if (bstr) {
      RemBstrNode(bstr);
    }
    SysFreeString(bstr);  
}

BSTR DebSysAllocString(const OLECHAR FAR* sz)
{
    BSTR bstr = SysAllocString(sz);
    if (bstr) {
      g_cAllocBstr++;
      AddBstrNode(bstr, SysStringByteLen(bstr));
    }
    return bstr;
}

BSTR DebSysAllocStringLen(const OLECHAR *sz, unsigned int cch)
{
    BSTR bstr = SysAllocStringLen(sz, cch);
    if (bstr) {
      g_cAllocBstr++;
      AddBstrNode(bstr, SysStringByteLen(bstr));
    }
    return bstr;
}

BSTR DebSysAllocStringByteLen(const CHAR *sz, unsigned int cb)
{
    BSTR bstr = SysAllocStringByteLen(sz, cb);
    if (bstr) {
      g_cAllocBstr++;
      AddBstrNode(bstr, SysStringByteLen(bstr));
    }
    return bstr;
}

BOOL DebSysReAllocString(BSTR *pbstr, const OLECHAR *sz)
{
    BSTR bstr = DebSysAllocString(sz);
    if (bstr == NULL) {
      return FALSE;
    }
    if (*pbstr) {
      DebSysFreeString(*pbstr);
    }
    *pbstr = bstr;
    return TRUE;
}

BOOL DebSysReAllocStringLen(
    BSTR *pbstr, 
    const OLECHAR *sz, 
    unsigned int cch)
{
    BSTR bstr = DebSysAllocStringLen(sz, cch);
    if (bstr == NULL) {
      return FALSE;
    }
    if (*pbstr) {
      DebSysFreeString(*pbstr);
    }
    *pbstr = bstr;
    return TRUE;
}

void DumpBstrLeaks()
{
    BstrNode *pbstrnodeCur = g_pbstrnodeFirst;
    WCHAR wszMessage[_MAX_PATH];

    ASSERT(pbstrnodeCur == NULL, L"BSTRs leaked: View | Output");
    while (pbstrnodeCur != NULL) {
      // assume the debugger or auxiliary port
      // WIN95: can use ANSI versions on NT as well...
      //
      wsprintf(wszMessage, 
                L"bstr leak: pv %x cAlloc %u size %d\n",
                 pbstrnodeCur->m_pv,
                 pbstrnodeCur->m_cAlloc,
                 pbstrnodeCur->m_nSize);
      OutputDebugString(wszMessage);
      pbstrnodeCur = pbstrnodeCur->m_pbstrnodeNext;
    }    
}

#ifdef DEBUG

static const WCHAR szFormat[]  = L"%s\nFile %s, Line %d";
static const WCHAR szFormat2[] = L"%s\n%s\nFile %s, Line %d";

#define _SERVERNAME_ L"ActiveX Framework"

static const WCHAR szTitle[]  = _SERVERNAME_ L" Assertion  (Abort = UAE, Retry = INT 3, Ignore = Continue)";


//=--------------------------------------------------------------------------=
// Local functions
//=--------------------------------------------------------------------------=
int NEAR _IdMsgBox(LPTSTR pszText, LPCTSTR pszTitle, UINT mbFlags);
//=--------------------------------------------------------------------------=
// DisplayAssert
//=--------------------------------------------------------------------------=
// Display an assert message box with the given pszMsg, pszAssert, source
// file name, and line number. The resulting message box has Abort, Retry,
// Ignore buttons with Abort as the default.  Abort does a FatalAppExit;
// Retry does an int 3 then returns; Ignore just returns.
//
VOID DisplayAssert
(
    LPTSTR	 pszMsg,
    LPTSTR	 pszAssert,
    LPTSTR	 pszFile,
    UINT	 line
)
{
    WCHAR	szMsg[250];
    LPTSTR	lpszText;

    lpszText = pszMsg;		// Assume no file & line # info

    // If C file assert, where you've got a file name and a line #
    //
    if (pszFile) {

        // Then format the assert nicely
        //
        wsprintf(szMsg, szFormat, (pszMsg&&*pszMsg) ? pszMsg : pszAssert, pszFile, line);
        lpszText = szMsg;
    }

    // Put up a dialog box
    //
    switch (_IdMsgBox(lpszText, szTitle, MB_ICONHAND|MB_ABORTRETRYIGNORE)) {
        case IDABORT:
        //    FatalAppExit(0, lpszText);
            return;

        case IDRETRY:
            // call the win32 api to break us.
            //
            DebugBreak();
            return;
    }

    return;
}

//=---------------------------------------------------------------------------=
// Beefed-up version of WinMessageBox.
//=---------------------------------------------------------------------------=
//
int NEAR _IdMsgBox
(
    LPTSTR	pszText,
    LPCTSTR	pszTitle,
    UINT	mbFlags
)
{
    HWND hwndActive;
    int  id;

    hwndActive = GetActiveWindow();

    id = MessageBox(hwndActive, pszText, pszTitle, mbFlags);

    return id;
}


#endif // DEBUG
