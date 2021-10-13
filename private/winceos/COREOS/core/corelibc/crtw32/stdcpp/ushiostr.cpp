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
// ushiostream -- _UShinit members, dummy for Microsoft
// unsigned short version for wiostram

 #ifdef _NATIVE_WCHAR_T_DEFINED
  #include <fstream>

  #define wistream    ushistream
  #define wostream    ushostream
  #define wfilebuf    ushfilebuf
  #define _Init_wcerr _Init_ushcerr
  #define _Init_wcout _Init_ushcout
  #define _Init_wclog _Init_ushclog
  #define _Init_wcin  _Init_ushcin
  #define _Winit      _UShinit

  #include <iostream>

#if defined(_M_CEE_PURE)
_STD_BEGIN
__PURE_APPDOMAIN_GLOBAL extern wistream *_Ptr_wcin = 0;
__PURE_APPDOMAIN_GLOBAL extern wostream *_Ptr_wcout = 0;
__PURE_APPDOMAIN_GLOBAL extern wostream *_Ptr_wcerr = 0;
__PURE_APPDOMAIN_GLOBAL extern wostream *_Ptr_wclog = 0;
_STD_END
#else
_STD_BEGIN
__PURE_APPDOMAIN_GLOBAL extern _CRTDATA2 wistream *_Ptr_wcin = 0;
__PURE_APPDOMAIN_GLOBAL extern _CRTDATA2 wostream *_Ptr_wcout = 0;
__PURE_APPDOMAIN_GLOBAL extern _CRTDATA2 wostream *_Ptr_wcerr = 0;
__PURE_APPDOMAIN_GLOBAL extern _CRTDATA2 wostream *_Ptr_wclog = 0;
_STD_END
  #include "wiostrea.cpp"
#endif
 #endif

/*
 * Copyright (c) 1992-2007 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V5.03:0009 */
