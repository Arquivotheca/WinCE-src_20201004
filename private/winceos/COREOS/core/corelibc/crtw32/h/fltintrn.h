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
#ifndef _INC_FLTINTRN
#define _INC_FLTINTRN

#ifdef __cplusplus
extern "C" {
#endif

#include <cruntime.h>


/*
 * structs used to fool the compiler into not generating floating point
 * instructions when copying and pushing [long] double values
 */

typedef struct {
    double x;
} _CRT_DOUBLE;

typedef struct {
    float x;
} _CRT_FLOAT;

typedef struct {
    long double x;
} _LONGDOUBLE;

typedef struct _strflt
{
    int sign;         /* zero if positive otherwise negative */
    int decpt;        /* exponent of floating point number */
    int flag;         /* zero if okay otherwise IEEE overflow */
    char *mantissa;       /* pointer to mantissa in string form */
} *STRFLT;

/*
 * typedef for _fltin
 */

typedef struct _flt
{
    int flags;
    int nbytes;      /* number of characters read */
    long lval;
    double dval;         /* the returned floating point number */
} *FLT;


/* floating point conversion routines, keep in sync with mrt32\include\convert.h */

STRFLT  __cdecl _fltout2( __in _CRT_DOUBLE _Dbl, __out STRFLT _Flt, __out_bcount_z(_SizeInBytes) char * _ResultStr, __in size_t _SizeInBytes);
errno_t __cdecl _fptostr(__out_bcount_z(_SizeInBytes) char * _Buf, __in size_t _SizeInBytes, __in int _Digits, __inout STRFLT _PtFlt);
errno_t _cftoe(__in double * _Value, __out_bcount_z(3 + _Dec + 5 + 1) char * _Buf, __in size_t _SizeInBytes, __in int _Dec, __in int _Caps);
errno_t _cftof(__in double * _Value, __out_bcount_z(_SizeInBytes) char * _Buf, __in size_t _SizeInBytes, __in int _Dec);

errno_t __cdecl _cfltcvt( double * arg, char * buffer, size_t sizeInBytes, int format, int precision, int caps);
void __cdecl _cropzeros(char * buf);
void __cdecl _fassign(int flag, char * argument, char * number);
void __cdecl _forcdecpt(char *buffer);

#ifdef __cplusplus
}
#endif

#endif  /* _INC_FLTINTRN */
