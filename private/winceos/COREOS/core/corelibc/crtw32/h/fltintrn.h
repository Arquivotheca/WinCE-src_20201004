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
*fltintrn.h - contains declarations of internal floating point types,
*             routines and variables
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Declares floating point types, routines and variables used
*       internally by the C run-time.
*
*       [Internal]
*
*Revision History:
*       10-20-88  JCR   Changed 'DOUBLE' to 'double' for 386
*       08-15-89  GJF   Fixed copyright, indents
*       10-30-89  GJF   Fixed copyright (again)
*       03-02-90  GJF   Added #ifndef _INC_STRUCT stuff. Also, cleaned up
*                       the formatting a bit.
*       03-05-90  GJF   Fixed up the arg types in protoypes. Also, added
*                       #include <cruntime.h>
*       03-22-90  GJF   Made _fltin(), _fltin2(), _fltout() and _fltout2()
*                       _CALLTYPE2 (for now) and added a prototype for
*                       _fptostr().
*       08-01-90  SBM   Moved _cftoe() and _cftof() here from internal.h
*                       and _cfltcvt_tab from input.c and output.c,
*                       added typedefs for _cfltcvt_tab entries,
*                       renamed module from <struct.h> to <fltintrn.h> and
*                       adjusted #ifndef stuff to #ifndef _INC_FLTINTRN
*       08-29-90  SBM   Changed type of _cfltcvt_tab[] to agree with
*                       definition in cmiscdat.c
*       04-26-91  SRW   Removed level 3 warnings
*       08-26-91  JCR   Changed MIPS to _MIPS_, ANSI naming
*       08-06-92  GJF   Function calling type and variable type macros. Revised
*                       use of target processor macros.
*       11-09-92  GJF   Fixed preprocessing conditionals for MIPS.
*       01-09-93  SRW   Remove usage of MIPS and ALPHA to conform to ANSI
*                       Use _MIPS_ and _ALPHA_ instead.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*       09-01-93  GJF   Merged Cuda and NT SDK versions.
*       10-13-93  GJF   Dropped _MIPS_. Replaced  _ALPHA_ with _M_ALPHA.
*       10-29-93  GJF   Disabled the ever-annoying 4069 warning.
*       10-02-94  BWT   Add PPC support.
*       12-15-94  XY    merged with mac header
*       02-14-95  CFW   Clean up Mac merge.
*       03-29-95  CFW   Add error message to internal headers.
*       12-14-95  JWM   Add "#pragma once".
*       02-05-97  GJF   Deleted obsolete support for _CRTAPI* and _NTSDK.
*                       Replaced (defined(_M_MPPC) || defined(_M_M68K)) with
*                       defined(_MAC) where appropriate. Replaced _CALLTYPE2
*                       with __cdecl. Also, detab-ed.
*       05-17-99  PML   Remove all Macintosh support.
*       09-05-00  GB    Changed the definition of fltout functions. Use DOUBLE 
*                       instead of double
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       11-19-01  GB    Replaced DOUBLE by _CRT_DOUBLE
*       08-08-03  AC    Added support for %a in printf
*       08-18-03  AC    Added safe functions (_cftoe, _cftof, _cftoa), added _wfltin functions
*       04-07-04  MSL   Changes to support locale sensitive strgtold12
*                       VSW #247190
*       07-29-04  AC    Added _SAFECRT_IMPL implementation
*       10-08-04  ESC   Protect against -Zp with pragma pack
*       10-31-04  MSL   Fix locale-sensitive input/output
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-14-05  MSL   Intellisense locale name cleanup
*       05-25-05  PML   _cfltcvt_tab no longer needed by CRT DLL (vsw#191114)
*
****/

#pragma once

#ifndef _INC_FLTINTRN
#define _INC_FLTINTRN

#include <crtdefs.h>

#pragma pack(push,_CRT_PACKING)

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * For MS C for the x86 family, disable the annoying "long double is the
 * same precision as double" warning
 */

#ifdef  _M_IX86
#pragma warning(disable:4069)
#endif

/*
 * structs used to fool the compiler into not generating floating point
 * instructions when copying and pushing [long] double values
 */


#ifndef _CRT_DOUBLE_DEC

#ifndef _LDSUPPORT

#pragma pack(4)
typedef struct {
    unsigned char ld[10];
} _LDOUBLE;
#pragma pack()

#define _PTR_LD(x) ((unsigned char  *)(&(x)->ld))

#else

typedef long double _LDOUBLE;

#define _PTR_LD(x) ((unsigned char  *)(x))

#endif

typedef struct {
        double x;
} _CRT_DOUBLE;

typedef struct {
    float f;
} _CRT_FLOAT;

typedef struct {
        /*
         * Assume there is a long double type
         */
        long double x;
} _LONGDOUBLE;

#define _CRT_DOUBLE_COMPONENTS_MANTISSA_BITS 52
#define _CRT_DOUBLE_COMPONENTS_EXPONENT_BITS 11
#define _CRT_DOUBLE_COMPONENTS_SIGN_BITS 1

typedef union {
    double d;
    struct {
        unsigned __int64 mantissa: _CRT_DOUBLE_COMPONENTS_MANTISSA_BITS;    /* 52 */
        unsigned __int64 exponent: _CRT_DOUBLE_COMPONENTS_EXPONENT_BITS;    /* 11 */
        unsigned __int64 sign: _CRT_DOUBLE_COMPONENTS_SIGN_BITS;            /*  1 */
    };
} _CRT_DOUBLE_COMPONENTS;    

#pragma pack(4)
typedef struct {
    unsigned char ld12[12];
} _LDBL12;
#pragma pack()

#define _CRT_DOUBLE_DEC
#endif

/*
 * return values for conversion routines
 * (12-byte to long double, double, or float)
 */

typedef enum {
    INTRNCVT_OK,
    INTRNCVT_OVERFLOW,
    INTRNCVT_UNDERFLOW
} INTRNCVT_STATUS;

/*
 * return values for strgtold12 routine
 */

#define SLD_UNDERFLOW 1
#define SLD_OVERFLOW 2
#define SLD_NODIGITS 4

/* 
 * debias values
 */
 
#define BIAS_64 1023

/*
 * typedef for _fltout
 */

typedef struct _strflt
{
        int sign;             /* zero if positive otherwise negative */
        int decpt;            /* exponent of floating point number */
        int flag;             /* zero if okay otherwise IEEE overflow */
        char *mantissa;       /* pointer to mantissa in string form */
}
        *STRFLT;


/*
 * typedef for _fltin
 */

typedef struct _flt
{
        int flags;
        int nbytes;          /* number of characters read */
        long lval;
        double dval;         /* the returned floating point number */
}
        *FLT;


/* floating point conversion routines, keep in sync with mrt32\include\convert.h */

errno_t _cftoe(_In_ double * _Value, _Out_writes_z_(3 + _Dec + 5 + 1) char * _Buf, _In_ size_t _SizeInBytes, _In_ int _Dec, _In_ int _Caps);
errno_t _cftoa(_In_ double * _Value, _Out_writes_z_(1 + 4 + _Dec + 6) char * _Buf, _In_ size_t _SizeInBytes, _In_ int _Dec, _In_ int _Caps);
errno_t _cftof(_In_ double * _Value, _Out_writes_z_(_SizeInBytes) char * _Buf, _In_ size_t _SizeInBytes, _In_ int _Dec);
errno_t __cdecl _fptostr(_Out_writes_z_(_SizeInBytes) char * _Buf, _In_ size_t _SizeInBytes, _In_ int _Digits, _Inout_ STRFLT _PtFlt);
_Check_return_ _CRTIMP int __cdecl _atodbl(_Out_ _CRT_DOUBLE * _Result, _In_z_ char * _Str);
_Check_return_ _CRTIMP int __cdecl _atoldbl(_Out_ _LDOUBLE * _Result, _In_z_ char * _Str);
_Check_return_ _CRTIMP int __cdecl _atoflt(_Out_ _CRT_FLOAT * _Result, _In_z_ char * _Str);
_Check_return_ _CRTIMP int __cdecl _atodbl_l(_Out_ _CRT_DOUBLE * _Result, _In_z_ char * _Str, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _atoldbl_l(_Out_ _LDOUBLE * _Result, _In_z_ char * _Str, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _atoflt_l(_Out_ _CRT_FLOAT * _Result, _In_z_ char * _Str,_In_opt_ _locale_t _Locale);

STRFLT  __cdecl _fltout2( _In_ _CRT_DOUBLE _Dbl, _Out_ STRFLT _Flt, _Out_writes_z_(_SizeInBytes) char * _ResultStr, _In_ size_t _SizeInBytes);
FLT     __cdecl _fltin2( _Out_ FLT _Flt, _In_z_ const char * _Str, _In_ _locale_t _Locale );
FLT     __cdecl _wfltin2( _Out_ FLT _Fit, _In_z_ const wchar_t * _Str, _In_ _locale_t _Locale );

INTRNCVT_STATUS _ld12tod(_In_ _LDBL12 *_Ifp, _Out_ _CRT_DOUBLE *_D);
INTRNCVT_STATUS _ld12tof(_In_ _LDBL12 *_Ifp, _Out_ _CRT_FLOAT *_F);
INTRNCVT_STATUS _ld12told(_In_ _LDBL12 *_Ifp, _Out_ _LDOUBLE *_Ld);

/*
 * floating point helper routines
 *
 * In the non-CRTDLL builds, these prototypes are immediately replaced by
 * macros which replace them with calls through elements of the _cfltcvt_tab
 * array, so we only pull them into a static-link build if floating point
 * code is needed.
 */

errno_t __cdecl _cfltcvt(double *arg, char *buffer, size_t sizeInBytes,
                         int format, int precision,
                         int caps);
errno_t __cdecl _cfltcvt_l(double *arg, char *buffer, size_t sizeInBytes,
                         int format, int precision,
                         int caps, _locale_t plocinfo);

void __cdecl _cropzeros(char *_Buf);
void __cdecl _cropzeros_l(char *_Buf, _locale_t _Locale);
void __cdecl _fassign(int flag, char  *argument, char *number);
void __cdecl _fassign_l(int flag, char  *argument, char *number, _locale_t);
void __cdecl _forcdecpt(char *_Buf);
void __cdecl _forcdecpt_l(char *_Buf, _locale_t _Locale);
int __cdecl _positive(double *arg);

/*
 * table of pointers to floating point helper routines
 *
 * We can't specify the prototypes for the entries of the table accurately,
 * since different functions in the table have different arglists.
 * So we declare the functions to take and return void (which is the
 * correct prototype for _fptrap(), which is what the entries are all
 * initialized to if no floating point is loaded) and cast appropriately
 * on every usage.
 */

#ifndef CRTDLL

typedef void (* PFV)(void);
extern PFV _cfltcvt_tab[10];
#define _CFLTCVT_TAB(i)       (DecodePointer(_cfltcvt_tab[i]))

typedef void (* PF0)(double*, char*, size_t, int, int, int);
#define _cfltcvt(a,b,c,d,e,f) (*((PF0)_CFLTCVT_TAB(0)))(a,b,c,d,e,f)

typedef void (* PF1)(char*);
#define _cropzeros(a)         (*((PF1)_CFLTCVT_TAB(1)))(a)

typedef void (* PF2)(int, char*, char*);
#define _fassign(a,b,c)       (*((PF2)_CFLTCVT_TAB(2)))(a,b,c)

typedef void (* PF3)(char*);
#define _forcdecpt(a)         (*((PF3)_CFLTCVT_TAB(3)))(a)

typedef int (* PF4)(double*);
#define _positive(a)          (*((PF4)_CFLTCVT_TAB(4)))(a)

typedef void (* PF5)(_LONGDOUBLE*, char*, size_t, int, int, int);
#define _cldcvt(a,b,c,d,e,f)  (*((PF5)_CFLTCVT_TAB(5)))(a,b,c,d,e,f)

typedef void (* PF6)(double*, char*, size_t, int, int, int, _locale_t);
#define _cfltcvt_l(a,b,c,d,e,f,g) (*((PF6)_CFLTCVT_TAB(6)))(a,b,c,d,e,f,g)

typedef void (* PF7)(int, char*, char*, _locale_t);
#define _fassign_l(a,b,c,d)       (*((PF7)_CFLTCVT_TAB(7)))(a,b,c,d)

typedef void (* PF8)(char*, _locale_t);
#define _cropzeros_l(a,b)         (*((PF8)_CFLTCVT_TAB(8)))(a,b)

typedef void (* PF9)(char*, _locale_t);
#define _forcdecpt_l(a,b)         (*((PF9)_CFLTCVT_TAB(9)))(a,b)

#endif /* ndef CRTDLL */

unsigned int __strgtold12_l
(
    _Out_ _LDBL12 *pld12,
    _Pre_notnull_ _Post_z_ const char * *p_end_ptr,
    _In_z_ const char * str,
    _In_ int mult12,
    _In_ int scale,
    _In_ int decpt,
    _In_ int implicit_E,
#if defined(_SAFECRT_IMPL) || defined(_WCE_BOOTCRT)
    _In_ char dec_point
#else
    _In_ _locale_t _Locale
#endif
);

unsigned int __strgtold12
(
    _Out_ _LDBL12 *pld12,
    _Pre_notnull_ _Post_z_ const char * *p_end_ptr,
    _In_z_ const char * str,
    _In_ int mult12,
    _In_ int scale,
    _In_ int decpt,
    _In_ int implicit_E
);

unsigned int __wstrgtold12_l
(
    _Out_ _LDBL12 *pld12,
    _Pre_notnull_ _Post_z_ const wchar_t * *p_end_ptr,
    _In_z_ const wchar_t * str,
    _In_ int mult12,
    _In_ int scale,
    _In_ int decpt,
    _In_ int implicit_E,
    _In_ _locale_t _Locale
);

unsigned int __wstrgtold12
(
    _Out_ _LDBL12 *pld12,
    _Pre_notnull_ _Post_z_ const wchar_t * *p_end_ptr,
    _In_z_ const wchar_t * str,
    _In_ int mult12,
    _In_ int scale,
    _In_ int decpt,
    _In_ int implicit_E
);

unsigned __STRINGTOLD
(
	_Out_ _LDOUBLE *pld,
	_Pre_notnull_ _Post_z_ const char  * *p_end_ptr,
	_In_z_ const char  *str,
	_In_ int mult12
);

unsigned __STRINGTOLD_L
(
	_Out_ _LDOUBLE *pld,
	_Pre_notnull_ _Post_z_ const char  * *p_end_ptr,
	_In_z_ const char  *str,
	_In_ int mult12,
    _In_ _locale_t _Locale
);

unsigned __WSTRINGTOLD
(
	_Out_ _LDOUBLE *pld,
    _Pre_notnull_ _Post_z_ const wchar_t  * *p_end_ptr,
    _In_z_ const wchar_t  *str,
    _In_ int mult12
);

unsigned __WSTRINGTOLD_L
(
	_Out_ _LDOUBLE *pld,
    _Pre_notnull_ _Post_z_ const wchar_t  * *p_end_ptr,
    _In_z_ const wchar_t  *str,
    _In_ int mult12,
    _In_ _locale_t _Locale
);

#ifdef  _M_IX86
#pragma warning(default:4069)
#endif

#ifdef  __cplusplus
}
#endif

#pragma pack(pop)

#endif  /* _INC_FLTINTRN */
