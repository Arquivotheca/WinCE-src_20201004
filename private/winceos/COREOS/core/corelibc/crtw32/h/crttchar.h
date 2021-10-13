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
#ifndef _INC_CRTTCHAR
#define _INC_CRTTCHAR

#ifdef  CRT_UNICODE

typedef wchar_t         CRT__TCHAR;
typedef wchar_t         CRT__TSCHAR;
typedef wchar_t         CRT__TUCHAR;
typedef wchar_t         CRT__TXCHAR;
typedef wint_t          CRT__TINT;
typedef wchar_t         CRT_TCHAR;
#define CRT_TEOF        WEOF
#define CRT_T(x)        L ## x

#define _istspace   iswspace
#define _istlower   iswlower

#define _tcslen         wcslen
#define _gettc_lk       _getwc_lk
#define _fgetts         fgetws
#define _getts          _getws
#define _getts_s        _getws_s
#define _ftscanf        fwscanf
#define _ftscanf_s      fwscanf_s
#define _tscanf         wscanf
#define _tscanf_s       wscanf_s
#define _stscanf        swscanf
#define _stscanf_s      swscanf_s
#define _vftprintf      vfwprintf
#define _vftprintf_s    vfwprintf_s
#define _vtprintf       vwprintf
#define _vtprintf_s     vwprintf_s
#define _ftprintf       fwprintf
#define _ftprintf_s     fwprintf_s
#define _tprintf        wprintf
#define _tprintf_s      wprintf_s
#define _vsntprintf     _vsnwprintf
#define _vsntprintf_s   _vsnwprintf_s
#define _vstprintf      vswprintf
#define _vstprintf_s    vswprintf_s
#define _vsctprintf     _vscwprintf
#define _sntprintf      _snwprintf
#define _sntprintf_s    _snwprintf_s
#define _stprintf       swprintf
#define _stprintf_s     swprintf_s
#define _sctprintf      _scwprintf

#else // !CRT_UNICODE

typedef char            CRT__TCHAR;
typedef signed char     CRT__TSCHAR;
typedef unsigned char   CRT__TUCHAR;
typedef char            CRT__TXCHAR;
typedef int             CRT__TINT;
typedef char            CRT_TCHAR;
#define CRT_TEOF        EOF
#define CRT_T(x)        x

#define _istspace   isspace
#define _istlower   islower

#define _tcslen         strlen
#define _gettc_lk       _getc_lk
#define _fgetts         fgets
#define _getts          gets
#define _getts_s        gets_s
#define _ftscanf        fscanf
#define _ftscanf_s      fscanf_s
#define _tscanf         scanf
#define _tscanf_s       scanf_s
#define _stscanf        sscanf
#define _stscanf_s      sscanf_s
#define _vftprintf      vfprintf
#define _vftprintf_s    vfprintf_s
#define _vtprintf       vprintf
#define _vtprintf_s     vprintf_s
#define _ftprintf       fprintf
#define _ftprintf_s     fprintf_s
#define _tprintf        printf
#define _tprintf_s      printf_s
#define _vsntprintf     _vsnprintf
#define _vsntprintf_s   _vsnprintf_s
#define _vstprintf      vsprintf
#define _vstprintf_s    vsprintf_s
#define _vsctprintf     _vscprintf
#define _sntprintf      _snprintf
#define _sntprintf_s    _snprintf_s
#define _stprintf       sprintf
#define _stprintf_s     sprintf_s
#define _sctprintf      _scprintf

#endif // !CRT_UNICODE

#endif //_INC_CRTTCHAR

