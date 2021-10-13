//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _INC_CRTTCHAR
#define _INC_CRTTCHAR

#ifdef  CRT_UNICODE

// #include <wchar.h>
typedef wchar_t     CRT__TCHAR;
typedef wchar_t     CRT__TSCHAR;
typedef wchar_t     CRT__TUCHAR;
typedef wchar_t     CRT__TXCHAR;
typedef wint_t      CRT__TINT;
typedef wchar_t     CRT_TCHAR;
#define CRT_TEOF    WEOF
#define CRT_T(x)    L ## x

#define _istspace   iswspace
#define _istlower   iswlower

#else // !CRT_UNICODE

typedef char            CRT__TCHAR;
typedef signed char     CRT__TSCHAR;
typedef unsigned char   CRT__TUCHAR;
typedef char            CRT__TXCHAR;
typedef int             CRT__TINT;
typedef char            CRT_TCHAR;
#define CRT_TEOF    	EOF
#define CRT_T(x)      	x

#define _istspace   isspace
#define _istlower   islower

#endif // !CRT_UNICODE

#endif //_INC_CRTTCHAR

