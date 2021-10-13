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
/***
*internal_safecrt.h - contains declarations of internal routines and variables for safecrt
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Declares routines and variables used internally by safecrt.lib.
*
*       [Internal]
*
*Revision History:
*       07-19-04  AC   Created
*       07-21-04  AC   Added some defines for mbs functions
*       07-29-04  AC   Added some declaration for floating-point support in scanf
*       08-05-04  AC   Include internal_securecrt.h to ensure we have the same behavior
*                      in safecrt and crt.
*       09-27-04  ESC  Added _ISMBBLEADPREFIX macro
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*
****/

#pragma once

#ifndef _INC_INTERNAL_SAFECRT
#define _INC_INTERNAL_SAFECRT

#define _SAFECRT_IMPL
#include <internal_securecrt.h>
#include <memory.h>
#include <stdio.h>
#include <fltintrn.h>

#undef _USE_LOCALE_ARG
#define _USE_LOCALE_ARG 0

#undef _ISMBBLEAD
#define _ISMBBLEAD(_Ch) \
    _ismbblead(_Ch)

/* Macro reports if bytePtr points to a char that is acting as a lead char,   */
/* prefix or otherwise. Needs to be redefined here since _MSMBBLEAD macro has */
/* been redefined.                                                            */
#undef _ISMBBLEADPREFIX
#define _ISMBBLEADPREFIX(_Result, _StringStart, _BytePtr)               \
    {                                                                   \
        unsigned char *_Tmp_VAR, *_StringStart_VAR, *_BytePtr_VAR;      \
                                                                        \
        _StringStart_VAR = (_StringStart);                              \
        _BytePtr_VAR = (_BytePtr);                                      \
        _Tmp_VAR = _BytePtr_VAR;                                        \
        while ((_Tmp_VAR >= _StringStart_VAR) && _ISMBBLEAD(*_Tmp_VAR)) \
        {                                                               \
            _Tmp_VAR--;                                                 \
        }                                                               \
        (_Result) = ((_BytePtr_VAR - _Tmp_VAR) & 1) != 0;               \
    }

#undef _cfltcvt
#undef _cldcvt
#define _safecrt_cldcvt _safecrt_cfltcvt

errno_t __cdecl _safecrt_cfltcvt(_In_ _CRT_DOUBLE *_Arg, _Out_writes_z_(_SizeInBytes) char *_Buffer, _In_ size_t _SizeInBytes, _In_ int _Type, _In_ int _Precision, _In_ int _Flags);

int __cdecl _soutput_s(_Out_writes_z_(_SizeInBytes) char *_Dst, _In_ size_t _SizeInBytes, _In_z_ _Printf_format_string_ const char *_Format, va_list _ArgList);
int __cdecl _swoutput_s(_Out_writes_z_(_SizeInWords) wchar_t *_Dst, _In_ size_t _SizeInWords, _In_z_ _Printf_format_string_ const wchar_t *_Format, va_list _ArgList);

int __cdecl _stdin_input_s(_In_z_ _Scanf_s_format_string_ const char *_Format, va_list _ArgList);
int __cdecl _stdin_winput_s(_In_z_ _Scanf_s_format_string_ const wchar_t *_Format, va_list _ArgList);

int __cdecl _sinput_s(_In_reads_(_Count) _Pre_z_ const char *_String, _In_ size_t _Count, _In_z_ _Scanf_s_format_string_ const char *_Format, va_list _ArgList);
int __cdecl _swinput_s(_In_reads_(_Count) _Pre_z_ const wchar_t *_String, _In_ size_t _Count, _In_z_ _Scanf_s_format_string_ const wchar_t *_Format, va_list _ArgList);

#if defined(_NTSUBSET_)
int __cdecl _safecrt_mbtowc(_Pre_notnull_ _Post_z_ wchar_t *_PWChar, _In_opt_z_ const char *_MbChar, _In_ size_t _MbCharLen);
#endif
errno_t __cdecl _safecrt_wctomb_s(_Out_opt_ int *_PRetValue, _Out_writes_opt_z_(_SizeInBytes) char *_Dst, _In_ size_t _SizeInBytes, _In_ wchar_t _WChar);

void __cdecl _safecrt_fassign(_In_ int _Flag, _Pre_notnull_ _Post_z_ char *_Argument, _In_z_ char *_Number, _In_ char _DecPoint);

#endif /* _INC_INTERNAL_SAFECRT */
