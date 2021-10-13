//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// Debug.H
//=--------------------------------------------------------------------------=
//
// contains the various macros and the like which are only useful in DEBUG
// builds
//
#ifndef _DEBUG_H_

//=---------------------------------------------------------------------------=
// all the things required to handle our ASSERT mechanism
//=---------------------------------------------------------------------------=
//
// UNDONE: workaround for Falcon
#undef ASSERT
#if DEBUG

// Function Prototypes
//
VOID DisplayAssert(LPTSTR pszMsg, LPTSTR pszAssert, LPTSTR pszFile, UINT line);

// Macros
//
// *** Include this macro at the top of any source file using *ASSERT*() macros ***
//
#define SZTHISFILE	static WCHAR _szThisFile[] = TEXT(__FILE__);


// our versions of the ASSERT and FAIL macros.
//
#define ASSERT(fTest, szMsg)                                \
    if (!(fTest))  {                                        \
        static WCHAR szMsgCode[] = szMsg;                    \
        static WCHAR szAssert[] = TEXT(#fTest);                    \
        DisplayAssert(szMsgCode, szAssert, _szThisFile, __LINE__); \
    }

#define FAIL(szMsg)                                         \
        { static WCHAR szMsgCode[] = szMsg;                    \
        DisplayAssert(szMsgCode, L"FAIL", _szThisFile, __LINE__); }



// macro that checks a pointer for validity on input
//
#define CHECK_POINTER(val) if (!(val) || IsBadWritePtr((void *)(val), sizeof(void *))) return E_POINTER

#else  // DEBUG

#define SZTHISFILE
#define ASSERT(fTest, err)
#define FAIL(err)

#define CHECK_POINTER(val)
#endif	// DEBUG




#define _DEBUG_H_
#endif // _DEBUG_H_
