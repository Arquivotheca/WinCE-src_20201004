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
* mbstring.h - MBCS string manipulation macros and functions
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       This file contains macros and function declarations for the MBCS
*       string manipulation functions.
*
*       [Public]
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       02-23-93  SKS   Update copyright to 1993
*       05-24-93  KRS   Added new functions from Ikura.
*       07-09-93  KRS   Put proper switches around _ismbblead/trail.
*       07-14-93  KRS   Add new mbsnbxxx functions: byte-count versions.
*       08-12-93  CFW   Fix ifstrip macro name.
*       10-07-93  GJF   Merged Cuda and NT versions. Added _CRTIMP.
*       10-13-93  GJF   Deleted obsolete COMBOINC check.
*       10-19-93  CFW   Remove _MBCS test and SBCS defines.
*       10-22-93  CFW   Add new ismbc* function prototypes.
*       12-08-93  CFW   Remove type-safe macros.
*       12-17-93  CFW   Remove wide char version mappings.
*       04-11-94  CFW   Add _NLSCMPERROR.
*       05-23-94  CFW   Add _mbs*coll.
*       11-03-94  GJF   Ensure 8 byte alignment.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       12-14-95  JWM   Add "#pragma once".
*       02-20-97  GJF   Cleaned out obsolete support for _CRTAPI* and _NTSDK.
*                       Also, detab-ed.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       04-21-98  GJF   Added support for per-thread mbc information.
*       12-15-98  GJF   Changes for 64-bit size_t.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       10-06-99  PML   Add _W64 modifier to types which are 32 bits in Win32,
*                       64 bits in Win64.
*       11-03-99  PML   Add va_list definition for _M_CEE.
*       06-08-00  PML   Remove threadmbcinfo.{pprev,pnext}.  Rename
*                       THREADMBCINFO to _THREADMBCINFO.
*       09-07-00  PML   Remove va_list definition for _M_CEE (vs7#159777)
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       10-08-02  EAN   removed _CRTIMP on __ismbslead_mt
*       04-25-03  EAN   const-correct overloads for _mbschr, _mbspbrk, 
*                       _mbsrchr and _mbsstr (VSWhidbey#71756)
*       08-01-03  AC    Added support for mode _dbg functions (a la _malloc_dbg)
*       03-08-04  MSL   Added declarations of missing _mbs[lwr|upr]_s[_l]. Not yet implemented.
*                       VSW #247263
*       03-08-04  MSL   const-correct overloads for _mbschr_l, _mbspbrk_l, 
*                       _mbsrchr_l and _mbsstr_l 
*                       VSW #252559
*       03-09-04  AC    Change the _mbslen_s family name to _mbsnlen.
*                       Added _mbslwr_s and _mbsupr_s.
*       03-24-04  MSL   Fixed char type in mbc info to be unsigned, resolves
*                       Lots of subtle bugs
*                       VSW#251689
*       07-29-04  AC    Added more _CRT_ALTERNATIVE_* defines for safecrt.h
*       08-05-04  AC    Added _mbsset_s family.
*                       Removed _CRT_ALTERNATIVE and introduced _CRTIMP_ALTERNATIVE.
*       08-12-04  AC    Added _mbscpy_s, _mbscpy_s_l and _mbscat_s_l declarations.
*       10-05-04  AC    Removed obsolete strlen_s family.
*                       VSW#379264
*       10-10-04  ESC   Use _CRT_PACKING
*       11-02-04  AC    Add cpp overloads for secure functions.
*                       VSW#313364
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       01-21-05  MSL   Add param names to overload macros
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-14-05  MSL   Intellisense locale name cleanup
*                       SAL cleanup for n functions
*       12-07-05  MSL   SAL fixes for prefast
*       01-25-06  AC    Added/modified some macro for secure cpp overloads
*                       VSW#565201
*       07-18-07  ATC   Fixing OACR warnings.
*
****/

#pragma once

#ifndef _INC_MBSTRING
#define _INC_MBSTRING

#include <crtdefs.h>


/*
 * Currently, all MS C compilers for Win32 platforms default to 8 byte
 * alignment.
 */
#pragma pack(push,_CRT_PACKING)

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef _FILE_DEFINED
struct _iobuf {
        char *_ptr;
        int   _cnt;
        char *_base;
        int   _flag;
        int   _file;
        int   _charbuf;
        int   _bufsiz;
        char *_tmpfname;
        };
typedef struct _iobuf FILE;
#define _FILE_DEFINED
#endif

#ifndef _INTERNAL_IFSTRIP_
/*
 * MBCS - Multi-Byte Character Set
 */
#ifndef _THREADMBCINFO
typedef struct threadmbcinfostruct {
        int refcount;
        int mbcodepage;
        int ismbcodepage;
        unsigned short mbulinfo[6];
        unsigned char mbctype[257];
        unsigned char mbcasemap[256];
        const wchar_t* mblocalename;
} threadmbcinfo;
#define _THREADMBCINFO
#endif
extern pthreadmbcinfo __ptmbcinfo;
pthreadmbcinfo __cdecl __updatetmbcinfo(void);
#endif  /* _INTERNAL_IFSTRIP_ */

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP

#ifndef _MBSTRING_DEFINED

/* function prototypes */

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma push_macro("_mbsdup")
#undef _mbsdup
#endif

_Check_return_ _CRTIMP unsigned char * __cdecl _mbsdup(_In_z_ const unsigned char * _Str);

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma pop_macro("_mbsdup")
#endif

_Check_return_ _CRTIMP unsigned int __cdecl _mbbtombc(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP unsigned int __cdecl _mbbtombc_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _mbbtype(_In_ unsigned char _Ch, _In_ int _CType);
_Check_return_ _CRTIMP int __cdecl _mbbtype_l(_In_ unsigned char _Ch, _In_ int _CType, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP unsigned int __cdecl _mbctombb(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP unsigned int __cdecl _mbctombb_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_CRTIMP int __cdecl _mbsbtype(_In_reads_z_(_Pos) const unsigned char * _Str, _In_ size_t _Pos);
_CRTIMP int __cdecl _mbsbtype_l(_In_reads_z_(_Pos) const unsigned char * _Str, _In_ size_t _Pos, _In_opt_ _locale_t _Locale);
_CRTIMP_ALTERNATIVE errno_t __cdecl _mbscat_s(_Inout_updates_z_(_SizeInBytes) unsigned char * _Dst, _In_ size_t _SizeInBytes, _In_z_ const unsigned char * _Src);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _mbscat_s, unsigned char, _Dst, _In_z_ const unsigned char *, _DstSizeInBytes)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_1(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbscat, _Inout_updates_z_(_String_length_(_Dest) + _String_length_(_Source) + 1), unsigned char, _Dest, _In_z_ const unsigned char *, _Source)
_CRTIMP errno_t __cdecl _mbscat_s_l(_Inout_updates_z_(_DstSizeInBytes) unsigned char * _Dst, _In_ size_t _DstSizeInBytes, _In_z_ const unsigned char * _Src, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, _mbscat_s_l, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_opt_ _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_2_EX(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbscat_l, _mbscat_s_l, _Inout_z_ unsigned char, _Inout_z_, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_opt_ _locale_t, _Locale)
_Check_return_ _CRTIMP  _CONST_RETURN unsigned char * __cdecl _mbschr(_In_z_ const unsigned char * _Str, _In_ unsigned int _Ch);
_Check_return_ _CRTIMP  _CONST_RETURN unsigned char * __cdecl _mbschr_l(_In_z_ const unsigned char * _Str, _In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _mbscmp(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2);
_Check_return_ _CRTIMP int __cdecl _mbscmp_l(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _mbscoll(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2);
_Check_return_ _CRTIMP int __cdecl _mbscoll_l(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_opt_ _locale_t _Locale);
_CRTIMP_ALTERNATIVE errno_t __cdecl _mbscpy_s(_Out_writes_z_(_SizeInBytes) unsigned char * _Dst, _In_ size_t _SizeInBytes, _In_z_ const unsigned char * _Src);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _mbscpy_s, unsigned char, _Dest, _In_z_ const unsigned char *, _Source)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_1(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbscpy, _Out_writes_z_(_String_length_(_Source) + 1), unsigned char, _Dest, _In_z_ const unsigned char *, _Source)
_CRTIMP errno_t __cdecl _mbscpy_s_l(_Out_writes_z_(_DstSizeInBytes) unsigned char * _Dst, _In_ size_t _DstSizeInBytes, _In_z_ const unsigned char * _Src, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, _mbscpy_s, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_opt_ _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_2_EX(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbscpy_l, _mbscpy_s_l, _Pre_notnull_ _Post_z_ unsigned char, _Pre_notnull_ _Post_z_, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_opt_ _locale_t, _Locale)
_Check_return_ _CRTIMP size_t __cdecl _mbscspn(_In_z_ const unsigned char * _Str, _In_z_ const unsigned char * _Control);
_Check_return_ _CRTIMP size_t __cdecl _mbscspn_l(_In_z_ const unsigned char * _Str, _In_z_ const unsigned char * _Control, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP unsigned char * __cdecl _mbsdec(_In_reads_z_(_Pos-_Start +1) const unsigned char * _Start, _In_z_ const unsigned char * _Pos);
_Check_return_ _CRTIMP unsigned char * __cdecl _mbsdec_l(_In_reads_z_(_Pos-_Start+1) const unsigned char * _Start, _In_z_ const unsigned char * _Pos, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _mbsicmp(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2);
_Check_return_ _CRTIMP int __cdecl _mbsicmp_l(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _mbsicoll(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2);
_Check_return_ _CRTIMP int __cdecl _mbsicoll_l(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP unsigned char * __cdecl _mbsinc(_In_z_ const unsigned char * _Ptr);
_Check_return_ _CRTIMP unsigned char * __cdecl _mbsinc_l(_In_z_ const unsigned char * _Ptr, _In_opt_ _locale_t _Locale);

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

_Check_return_ _CRTIMP size_t __cdecl _mbslen(_In_z_ const unsigned char * _Str);
_Check_return_ _CRTIMP size_t __cdecl _mbslen_l(_In_z_ const unsigned char * _Str, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP size_t __cdecl _mbsnlen(_In_z_ const unsigned char * _Str, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP size_t __cdecl _mbsnlen_l(_In_z_ const unsigned char * _Str, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP

_CRTIMP errno_t __cdecl _mbslwr_s(_Inout_updates_opt_z_(_SizeInBytes) unsigned char *_Str, _In_ size_t _SizeInBytes);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_0(errno_t, _mbslwr_s, _Inout_updates_z_(_Size) unsigned char, _String)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_0(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbslwr, _Inout_z_, unsigned char, _String)
_CRTIMP errno_t __cdecl _mbslwr_s_l(_Inout_updates_opt_z_(_SizeInBytes) unsigned char *_Str, _In_ size_t _SizeInBytes, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _mbslwr_s_l, unsigned char, _String, _In_opt_ _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_1_EX(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbslwr_l, _mbslwr_s_l, _Inout_updates_z_(_Size) unsigned char, _Inout_z_, unsigned char, _String, _In_opt_ _locale_t, _Locale)
_CRTIMP_ALTERNATIVE errno_t __cdecl _mbsnbcat_s(_Inout_updates_z_(_SizeInBytes) unsigned char * _Dst, _In_ size_t _SizeInBytes, _In_z_ const unsigned char * _Src, _In_ size_t _MaxCount);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, _mbsnbcat_s, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_ size_t, _Count)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_2(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbsnbcat, _Inout_z_, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_ size_t, _Count)
_CRTIMP errno_t __cdecl _mbsnbcat_s_l(_Inout_updates_z_(_DstSizeInBytes) unsigned char * _Dst, _In_ size_t _DstSizeInBytes, _In_z_ const unsigned char * _Src, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_3(errno_t, _mbsnbcat_s_l, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_ size_t, _Count, _In_opt_ _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_3_EX(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbsnbcat_l, _mbsnbcat_s_l, _Inout_updates_z_(_Size) unsigned char, _Inout_z_, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_ size_t, _Count, _In_opt_ _locale_t, _Locale)
_Check_return_ _CRTIMP int __cdecl _mbsnbcmp(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int __cdecl _mbsnbcmp_l(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _mbsnbcoll(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int __cdecl _mbsnbcoll_l(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP size_t __cdecl _mbsnbcnt(_In_reads_or_z_(_MaxCount) const unsigned char * _Str, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP size_t __cdecl _mbsnbcnt_l(_In_reads_or_z_(_MaxCount) const unsigned char * _Str, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_CRTIMP_ALTERNATIVE errno_t __cdecl _mbsnbcpy_s(_Out_writes_z_(_SizeInBytes) unsigned char * _Dst, _In_ size_t _SizeInBytes, _In_z_ const unsigned char * _Src, _In_ size_t _MaxCount);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, _mbsnbcpy_s, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_ size_t, _Count)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_2(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbsnbcpy, _Out_writes_(_Count) _Post_maybez_, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_ size_t, _Count)
_CRTIMP errno_t __cdecl _mbsnbcpy_s_l(_Out_writes_z_(_DstSizeInBytes) unsigned char * _Dst, _In_ size_t _DstSizeInBytes, _In_z_ const unsigned char * _Src, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_3(errno_t, _mbsnbcpy_s_l, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_ size_t, _Count, _In_opt_ _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_3_EX(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbsnbcpy_l, _mbsnbcpy_s_l, _Out_writes_z_(_Size) unsigned char, _Out_writes_(_Count) _Post_maybez_, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_ size_t, _Count, _In_opt_ _locale_t, _Locale)
_Check_return_ _CRTIMP int __cdecl _mbsnbicmp(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int __cdecl _mbsnbicmp_l(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _mbsnbicoll(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int __cdecl _mbsnbicoll_l(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_CRTIMP_ALTERNATIVE errno_t __cdecl _mbsnbset_s(_Inout_updates_z_(_SizeInBytes) unsigned char * _Dst, _In_ size_t _SizeInBytes, _In_ unsigned int _Ch, _In_ size_t _MaxCount);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, _mbsnbset_s, _Prepost_z_ unsigned char, _Dest, _In_ unsigned int, _Val, _In_ size_t, _MaxCount)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_2_EX(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbsnbset, _mbsnbset_s, _Inout_updates_z_(_Size) unsigned char, _Inout_updates_z_(_MaxCount), unsigned char, _String, _In_ unsigned int, _Val, _In_ size_t, _MaxCount)
_CRTIMP errno_t __cdecl _mbsnbset_s_l(_Inout_updates_z_(_DstSizeInBytes) unsigned char * _Dst, _In_ size_t _DstSizeInBytes, _In_ unsigned int _Ch, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_3(errno_t, _mbsnbset_s_l, _Prepost_z_ unsigned char, _Dest, _In_ unsigned int, _Val, _In_ size_t, _MaxCount, _In_opt_ _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_3_EX(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbsnbset_l, _mbsnbset_s_l, _Inout_updates_z_(_Size) unsigned char, _Inout_updates_z_(_MaxCount), unsigned char, _String, _In_ unsigned int, _Val, _In_ size_t, _MaxCount, _In_opt_ _locale_t, _Locale)
_CRTIMP_ALTERNATIVE errno_t __cdecl _mbsncat_s(_Inout_updates_z_(_SizeInBytes) unsigned char * _Dst, _In_ size_t _SizeInBytes, _In_z_ const unsigned char * _Src, _In_ size_t _MaxCount);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, _mbsncat_s, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_ size_t, _Count)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_2(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbsncat, _Inout_z_, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_ size_t, _Count)
_CRTIMP errno_t __cdecl _mbsncat_s_l(_Inout_updates_z_(_DstSizeInBytes) unsigned char * _Dst, _In_ size_t _DstSizeInBytes, _In_z_ const unsigned char * _Src, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_3(errno_t, _mbsncat_s_l, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_ size_t, _Count, _In_opt_ _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_3_EX(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbsncat_l, _mbsncat_s_l, _Inout_updates_z_(_Size) unsigned char, _Inout_z_, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_ size_t, _Count, _In_opt_ _locale_t, _Locale)
_Check_return_ _CRTIMP size_t __cdecl _mbsnccnt(_In_reads_or_z_(_MaxCount) const unsigned char * _Str, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP size_t __cdecl _mbsnccnt_l(_In_reads_or_z_(_MaxCount) const unsigned char * _Str, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _mbsncmp(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int __cdecl _mbsncmp_l(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _mbsncoll(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int __cdecl _mbsncoll_l(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_CRTIMP_ALTERNATIVE errno_t __cdecl _mbsncpy_s(_Out_writes_z_(_SizeInBytes) unsigned char * _Dst, _In_ size_t _SizeInBytes, _In_z_ const unsigned char * _Src, _In_ size_t _MaxCount);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, _mbsncpy_s, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_ size_t, _Count)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_2(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbsncpy, _Pre_notnull_ _Out_writes_(2*_Count) _Post_maybez_, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_ size_t, _Count)
_CRTIMP errno_t __cdecl _mbsncpy_s_l(_Out_writes_z_(_DstSizeInBytes) unsigned char * _Dst, _In_ size_t _DstSizeInBytes, _In_z_ const unsigned char * _Src, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_3(errno_t, _mbsncpy_s_l, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_ size_t, _Count, _In_opt_ _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_3_EX(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbsncpy_l, _mbsncpy_s_l, _Out_writes_z_(_Size) unsigned char, _Out_writes_(_Count)  _Post_maybez_, unsigned char, _Dest, _In_z_ const unsigned char *, _Source, _In_ size_t, _Count, _In_opt_ _locale_t, _Locale)
_Check_return_ _CRTIMP unsigned int __cdecl _mbsnextc (_In_z_ const unsigned char * _Str);
_Check_return_ _CRTIMP unsigned int __cdecl _mbsnextc_l(_In_z_ const unsigned char * _Str, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _mbsnicmp(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int __cdecl _mbsnicmp_l(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _mbsnicoll(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int __cdecl _mbsnicoll_l(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char * _Str2, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP unsigned char * __cdecl _mbsninc(_In_reads_or_z_(_Count) const unsigned char * _Str, _In_ size_t _Count);
_Check_return_ _CRTIMP unsigned char * __cdecl _mbsninc_l(_In_reads_or_z_(_Count) const unsigned char * _Str, _In_ size_t _Count, _In_opt_ _locale_t _Locale);
_CRTIMP_ALTERNATIVE errno_t __cdecl _mbsnset_s(_Inout_updates_z_(_SizeInBytes) unsigned char * _Dst, _In_ size_t _SizeInBytes, _In_ unsigned int _Val, _In_ size_t _MaxCount);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, _mbsnset_s, _Prepost_z_ unsigned char, _Dest, _In_ unsigned int, _Val, _In_ size_t, _MaxCount)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_2_EX(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbsnset, _mbsnset_s, _Inout_updates_z_(_Size) unsigned char, _Inout_updates_z_(_MaxCount), unsigned char, _String, _In_ unsigned int, _Val, _In_ size_t, _MaxCount)
_CRTIMP errno_t __cdecl _mbsnset_s_l(_Inout_updates_z_(_DstSizeInBytes) unsigned char * _Dst, _In_ size_t _DstSizeInBytes, _In_ unsigned int _Val, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_3(errno_t, _mbsnset_s_l, _Prepost_z_ unsigned char, _Dest, _In_ unsigned int, _Val, _In_ size_t, _MaxCount, _In_opt_ _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_3_EX(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbsnset_l, _mbsnset_s_l, _Inout_updates_z_(_Size) unsigned char, _Inout_updates_z_(_MaxCount), unsigned char, _String, _In_ unsigned int, _Val, _In_ size_t, _MaxCount, _In_opt_ _locale_t, _Locale)
_Check_return_ _CRTIMP  _CONST_RETURN unsigned char * __cdecl _mbspbrk(_In_z_ const unsigned char * _Str, _In_z_ const unsigned char * _Control);
_Check_return_ _CRTIMP  _CONST_RETURN unsigned char * __cdecl _mbspbrk_l(_In_z_ const unsigned char * _Str, _In_z_ const unsigned char * _Control, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP  _CONST_RETURN unsigned char * __cdecl _mbsrchr(_In_z_ const unsigned char * _Str, _In_ unsigned int _Ch);
_Check_return_ _CRTIMP  _CONST_RETURN unsigned char * __cdecl _mbsrchr_l(_In_z_ const unsigned char *_Str, _In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_CRTIMP unsigned char * __cdecl _mbsrev(_Inout_z_ unsigned char * _Str);
_CRTIMP unsigned char * __cdecl _mbsrev_l(_Inout_z_ unsigned char *_Str, _In_opt_ _locale_t _Locale);
_CRTIMP_ALTERNATIVE errno_t __cdecl _mbsset_s(_Inout_updates_z_(_SizeInBytes) unsigned char * _Dst, _In_ size_t _SizeInBytes, _In_ unsigned int _Val);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _mbsset_s, _Prepost_z_ unsigned char, _Dest, _In_ unsigned int, _Val)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_1_EX(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbsset, _mbsset_s, _Inout_updates_z_(_Size) unsigned char, _Inout_z_, unsigned char, _String, _In_ unsigned int, _Val)
_CRTIMP errno_t __cdecl _mbsset_s_l(_Inout_updates_z_(_DstSizeInBytes) unsigned char * _Dst, _In_ size_t _DstSizeInBytes, _In_ unsigned int _Val, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, _mbsset_s_l, _Prepost_z_ unsigned char, _Dest, _In_ unsigned int, _Val, _In_opt_ _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_2_EX(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbsset_l, _mbsset_s_l, _Inout_updates_z_(_Size) unsigned char, _Inout_z_, unsigned char, _String, _In_ unsigned int, _Val, _In_opt_ _locale_t, _Locale)
_Check_return_ _CRTIMP size_t __cdecl _mbsspn(_In_z_ const unsigned char *_Str, _In_z_ const unsigned char * _Control);
_Check_return_ _CRTIMP size_t __cdecl _mbsspn_l(_In_z_ const unsigned char * _Str, _In_z_ const unsigned char * _Control, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP unsigned char * __cdecl _mbsspnp(_In_z_ const unsigned char * _Str1, _In_z_ const unsigned char *_Str2);
_Check_return_ _CRTIMP unsigned char * __cdecl _mbsspnp_l(_In_z_ const unsigned char *_Str1, _In_z_ const unsigned char *_Str2, _In_opt_ _locale_t _Locale);
_Check_return_ _Ret_maybenull_ _CRTIMP  _CONST_RETURN unsigned char * __cdecl _mbsstr(_In_z_ const unsigned char * _Str, _In_z_ const unsigned char * _Substr);
_Check_return_ _Ret_maybenull_ _CRTIMP  _CONST_RETURN unsigned char * __cdecl _mbsstr_l(_In_z_ const unsigned char * _Str, _In_z_ const unsigned char * _Substr, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP _CRT_INSECURE_DEPRECATE(_mbstok_s) unsigned char * __cdecl _mbstok(_Inout_opt_z_ unsigned char * _Str, _In_z_ const unsigned char * _Delim);
_Check_return_ _CRTIMP _CRT_INSECURE_DEPRECATE(_mbstok_s_l) unsigned char * __cdecl _mbstok_l(_Inout_opt_z_ unsigned char *_Str, _In_z_ const unsigned char * _Delim, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP_ALTERNATIVE unsigned char * __cdecl _mbstok_s(_Inout_opt_z_ unsigned char *_Str, _In_z_ const unsigned char * _Delim, _Inout_ _Deref_prepost_opt_z_ unsigned char ** _Context);
_Check_return_ _CRTIMP unsigned char * __cdecl _mbstok_s_l(_Inout_opt_z_ unsigned char * _Str, _In_z_ const unsigned char * _Delim, _Inout_ _Deref_prepost_opt_z_ unsigned char ** _Context, _In_opt_ _locale_t _Locale);
_CRTIMP errno_t __cdecl _mbsupr_s(_Inout_updates_z_(_SizeInBytes) unsigned char *_Str, _In_ size_t _SizeInBytes);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_0(errno_t, _mbsupr_s, _Inout_updates_z_(_Size) unsigned char, _String)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_0(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbsupr, _Inout_z_, unsigned char, _String)
_CRTIMP errno_t __cdecl _mbsupr_s_l(_Inout_updates_z_(_SizeInBytes) unsigned char *_Str, _In_ size_t _SizeInBytes, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _mbsupr_s_l, unsigned char, _String, _In_opt_ _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_1_EX(unsigned char *, __RETURN_POLICY_DST, _CRTIMP, _mbsupr_l, _mbsupr_s_l, _Inout_updates_z_(_Size) unsigned char, _Inout_z_, unsigned char, _String, _In_opt_ _locale_t, _Locale)

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

_Check_return_ _CRTIMP size_t __cdecl _mbclen(_In_z_ const unsigned char *_Str);
_Check_return_ _CRTIMP size_t __cdecl _mbclen_l(_In_z_ const unsigned char * _Str, _In_opt_ _locale_t _Locale);

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP

_CRTIMP _CRT_INSECURE_DEPRECATE(_mbccpy_s) void __cdecl _mbccpy(_Out_writes_bytes_(2) unsigned char * _Dst, _In_z_ const unsigned char * _Src);
_CRTIMP _CRT_INSECURE_DEPRECATE(_mbccpy_s_l) void __cdecl _mbccpy_l(_Out_writes_bytes_(2) unsigned char *_Dst, _In_z_ const unsigned char *_Src, _In_opt_ _locale_t _Locale);
_CRTIMP_ALTERNATIVE errno_t __cdecl _mbccpy_s(_Out_writes_z_(_SizeInBytes) unsigned char * _Dst, _In_ size_t _SizeInBytes, _Out_opt_ int * _PCopied, _In_z_ const unsigned char * _Src);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, _mbccpy_s, unsigned char, _Dest, _Out_opt_ int *, _PCopied, _In_z_ const unsigned char *, _Source)
_CRTIMP errno_t __cdecl _mbccpy_s_l(_Out_writes_bytes_(_DstSizeInBytes) unsigned char * _Dst, _In_ size_t _DstSizeInBytes, _Out_opt_ int * _PCopied, _In_z_ const unsigned char * _Src, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_3(errno_t, _mbccpy_s_l, unsigned char, _Dest, _Out_opt_ int *,_PCopied, _In_z_ const unsigned char *,_Source, _In_opt_ _locale_t, _Locale)
#define _mbccmp(_cpc1, _cpc2) _mbsncmp((_cpc1),(_cpc2),1)

#ifdef  __cplusplus
#ifndef _CPP_MBCS_INLINES_DEFINED
#define _CPP_MBCS_INLINES_DEFINED
extern "C++" {
_Check_return_ inline unsigned char * __CRTDECL _mbschr(_In_z_ unsigned char *_String, _In_ unsigned int _Char)
{
    return ((unsigned char *)_mbschr((const unsigned char *)_String, _Char)); 
}

_Check_return_ inline unsigned char * __CRTDECL _mbschr_l(_In_z_ unsigned char *_String, _In_ unsigned int _Char, _In_opt_ _locale_t _Locale)
{
    return ((unsigned char *)_mbschr_l((const unsigned char *)_String, _Char, _Locale)); 
}

_Check_return_ inline unsigned char * __CRTDECL _mbspbrk(_In_z_ unsigned char *_String, _In_z_ const unsigned char *_CharSet)
{
    return ((unsigned char *)_mbspbrk((const unsigned char *)_String, _CharSet)); 
}

_Check_return_ inline unsigned char * __CRTDECL _mbspbrk_l(_In_z_ unsigned char *_String, _In_z_ const unsigned char *_CharSet, _In_opt_ _locale_t _Locale)
{
    return ((unsigned char *)_mbspbrk_l((const unsigned char *)_String, _CharSet, _Locale)); 
}

_Check_return_ inline unsigned char * __CRTDECL _mbsrchr(_In_z_ unsigned char *_String, _In_ unsigned int _Char)
{
    return ((unsigned char *)_mbsrchr((const unsigned char *)_String, _Char)); 
}

_Check_return_ inline unsigned char * __CRTDECL _mbsrchr_l(_In_z_ unsigned char *_String, _In_ unsigned int _Char, _In_opt_ _locale_t _Locale)
{
    return ((unsigned char *)_mbsrchr_l((const unsigned char *)_String, _Char, _Locale)); 
}

_Check_return_ _Ret_maybenull_ inline unsigned char * __CRTDECL _mbsstr(_In_z_ unsigned char *_String, _In_z_ const unsigned char *_Match)
{
    return ((unsigned char *)_mbsstr((const unsigned char *)_String, _Match)); 
}

_Check_return_ _Ret_maybenull_ inline unsigned char * __CRTDECL _mbsstr_l(_In_z_ unsigned char *_String, _In_z_ const unsigned char *_Match, _In_opt_ _locale_t _Locale)
{
    return ((unsigned char *)_mbsstr_l((const unsigned char *)_String, _Match, _Locale)); 
}
}
#endif
#endif  /* __cplusplus */

/* character routines */

_Check_return_ _CRTIMP int __cdecl _ismbcalnum(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbcalnum_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbcalpha(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbcalpha_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbcdigit(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbcdigit_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbcgraph(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbcgraph_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbclegal(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbclegal_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbclower(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbclower_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbcprint(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbcprint_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbcpunct(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbcpunct_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbcspace(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbcspace_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbcupper(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbcupper_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);

_Check_return_ _CRTIMP unsigned int __cdecl _mbctolower(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP unsigned int __cdecl _mbctolower_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP unsigned int __cdecl _mbctoupper(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP unsigned int __cdecl _mbctoupper_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);

#define _MBSTRING_DEFINED
#endif

#ifndef _MBLEADTRAIL_DEFINED
_Check_return_ _CRTIMP int __cdecl _ismbblead(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbblead_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbbtrail(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbbtrail_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbslead(_In_reads_z_(_Pos - _Str+1) const unsigned char * _Str, _In_z_ const unsigned char * _Pos);
_Check_return_ _CRTIMP int __cdecl _ismbslead_l(_In_reads_z_(_Pos - _Str+1) const unsigned char * _Str, _In_z_ const unsigned char * _Pos, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbstrail(_In_reads_z_(_Pos - _Str+1) const unsigned char * _Str, _In_z_ const unsigned char * _Pos);
_Check_return_ _CRTIMP int __cdecl _ismbstrail_l(_In_reads_z_(_Pos - _Str+1) const unsigned char * _Str, _In_z_ const unsigned char * _Pos, _In_opt_ _locale_t _Locale);
#define _MBLEADTRAIL_DEFINED
#endif

/*  Kanji specific prototypes.  */

_Check_return_ _CRTIMP int __cdecl _ismbchira(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbchira_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbckata(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbckata_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbcsymbol(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbcsymbol_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbcl0(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbcl0_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbcl1(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbcl1_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _ismbcl2(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP int __cdecl _ismbcl2_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP unsigned int __cdecl _mbcjistojms(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP unsigned int __cdecl _mbcjistojms_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP unsigned int __cdecl _mbcjmstojis(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP unsigned int __cdecl _mbcjmstojis_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP unsigned int __cdecl _mbctohira(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP unsigned int __cdecl _mbctohira_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP unsigned int __cdecl _mbctokata(_In_ unsigned int _Ch);
_Check_return_ _CRTIMP unsigned int __cdecl _mbctokata_l(_In_ unsigned int _Ch, _In_opt_ _locale_t _Locale);

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#ifdef  __cplusplus
}
#endif

#pragma pack(pop)

#endif  /* _INC_MBSTRING */
