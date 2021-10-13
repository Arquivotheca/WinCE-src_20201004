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
#ifndef _INC_CORECRT
#define _INC_CORECRT

#include <windows.h>
#include <stdlib.h>
#include <sal.h>

#define _CMNINTRIN_DECLARE_ONLY
#include <cmnintrin.h>
#undef _CMNINTRIN_DECLARE_ONLY

/* During CRT building, there is never a time where it is
 *  useful to see the compiler warning, "'foo' not
 *  available as an intrinsic function.
 */
#pragma warning(disable:4163)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _M_IX86
#define ALIGN(x)  ((unsigned long *)(x))
#else
#define ALIGN(x)  ((unsigned long __unaligned *)(x))
#endif

// from cvt.h
#define _CVTBUFSIZE (309+40) /* # of digits in max. dp value + slop */

typedef struct {
    unsigned char ld[10];
} _LDOUBLE;

typedef union {
    struct {
        unsigned char ld12[12];
    };
    __int64 align; // force 8 byte alignment for sh5
    unsigned long ld12_long[4];
} _LDBL12;

#define MAX_MAN_DIGITS 21


/* specifies '%f' format */

#define SO_FFORMAT 1

typedef struct _FloatOutStruct {
    short exp;
    char sign;
    char ManLen;
    char man[MAX_MAN_DIGITS+1];
} FOS;

/* return values for strgtold12 routine */
#define SLD_UNDERFLOW 1
#define SLD_OVERFLOW 2
#define SLD_NODIGITS 4

/* return values for internal conversion routines (12-byte to long double, double, or float) */
typedef enum {
    INTRNCVT_OK,
    INTRNCVT_OVERFLOW,
    INTRNCVT_UNDERFLOW
} INTRNCVT_STATUS;

#define NDIGITS 17

#define ULONG_MAX       0xffffffffUL    /* maximum unsigned long value */
#define USHORT_MAX      ((unsigned short)0xffff)
#define LONG_MIN        (-2147483647L - 1) /* minimum (signed) long value */
#define MSB_ULONG       ((unsigned long)0x80000000)
#define MSB_USHORT      ((unsigned short)0x8000)
#define D_BIAS          0x3ff   /* exponent bias for double */
#define D_BIASM1        0x3fe   /* D_BIAS - 1 */
#define D_MAXEXP        0x7ff   /* maximum biased exponent */
#define LD_BIAS         0x3fff  /* exponent bias for long double */
#define LD_BIASM1       0x3ffe  /* LD_BIAS - 1 */
#define LD_MAXEXP       0x7fff  /* maximum biased exponent */
#define LD_MAX_MAN_LEN  24      /* maximum length of mantissa (decimal)*/
#define TMAX10          5200    /* maximum temporary decimal exponent */
#define TMIN10          -5200   /* minimum temporary decimal exponent */

#define PTR_LD(x)       ((unsigned char *)(&(x)->ld))
#define PTR_12(x)       ((unsigned char *)(&(x)->ld12))

#define U_SHORT4_D(p)   ((unsigned short *)(p) + 3)
#define UL_HI_D(p)      ((unsigned long *)(p) + 1)
#define UL_LO_D(p)      ((unsigned long *)(p))
#define U_EXP_LD(p)     ((unsigned short *)(PTR_LD(p)+8))
#define U_EXP_12(p)     ((unsigned short *)(PTR_12(p)+10))
#define UL_MANHI_LD(p)  ((unsigned long *)(PTR_LD(p)+4))
#define UL_MANLO_LD(p)  ((unsigned long *)(PTR_LD(p)))
#define UL_MANHI_12(p)  (ALIGN((PTR_12(p)+6)))
#define UL_MANLO_12(p)  (ALIGN((PTR_12(p)+2)))
#define UL_HI_12(p)     ((unsigned long *)(PTR_12(p)+8))
#define UL_MED_12(p)    ((unsigned long *)(PTR_12(p)+4))
#define UL_LO_12(p)     ((unsigned long *)PTR_12(p))
#define U_XT_12(p)      ((unsigned short *)PTR_12(p))
#define USHORT_12(p,i)  ((unsigned short *)((unsigned char *)PTR_12(p)+(i)))
#define ULONG_12(p,i)   ((unsigned long *)((unsigned char *)PTR_12(p)+(i)))
#define UCHAR_12(p,i)   ((unsigned char *)PTR_12(p)+(i))

_CRTIMP_DATA extern double _HUGE;
#define HUGE_VAL _HUGE

void __dtold(_LDOUBLE *pld, double *px);
unsigned int __strgtold12(_LDBL12 *pld12, const char **p_end_ptr, const char *str);
INTRNCVT_STATUS _ld12tod(_LDBL12 *pld12, double *d);

/* from x10fout.c */
/* this is defined as void in convert.h
 * After porting the asm files to c, we need a return value for
 * i10_output, that used to reside in reg. ax
 */
int $I10_OUTPUT(_LDOUBLE ld, int ndigits,
                unsigned output_flags, FOS  *fos);

int __addl(unsigned long x, unsigned long y, unsigned long *sum);
void __mtold12(char *manptr, unsigned manlen, _LDBL12 *ld12);
void __multtenpow12(_LDBL12 *pld12, int pow, unsigned mult12);
void __add_12(_LDBL12 *x, _LDBL12 *y);
void __shl_12(_LDBL12 *p);
void __shr_12(_LDBL12 *p);
void __ld12mul(_LDBL12 *px, const _LDBL12 *py);
void _atodbl(double *d, char *str);
void _atoflt(float *f, char *str);

double exp (double x);
double sqrt(double x);
double cos(double x);
double sin(double x);
double log(double x);
double fabs(double x);
double ceil(double x);
double floor(double x);
double ldexp(double x, int exp);

typedef void (__cdecl *_PVFV)(void);

void __cdecl _cinit(void);
void __cdecl _cexit(void);
void __cdecl exit(int);

#if defined(_M_SH) && (_M_SH <= 4)
typedef struct _CRTINT64 {
    union {
        struct {
            unsigned int LowPart;
            int HighPart;
        };
        __int64 val;
    };
} CRTINT64;
typedef struct _CRTUINT64 {
    union {
        struct {
            unsigned int LowPart;
            unsigned int HighPart;
        };
        __int64 val;
    };
} CRTUINT64;
#define LOW_PART(x) ((x).LowPart)
#define HIGH_PART(x) ((x).HighPart)

#define DECLARE_BINARY_BITOP(name,operator)                   \
CRTUINT64 * name (CRTUINT64 *res, CRTUINT64 *x, CRTUINT64 *y) { \
    LOW_PART(*res) = LOW_PART(*x) operator LOW_PART(*y);      \
    HIGH_PART(*res) = HIGH_PART(*x) operator HIGH_PART(*y);   \
    return res;                                               \
}
#define DECLARE_UNARY_BITOP(name,operator)                    \
CRTUINT64 * name (CRTUINT64 *res, CRTUINT64 *x) {                      \
    LOW_PART(*res) = operator LOW_PART(*x);                   \
    HIGH_PART(*res) = operator HIGH_PART(*x);                 \
    return res;                                               \
}
#endif

/*
 * Default value used for the global /GS security cookie
 */
#define DEFAULT_SECURITY_COOKIE (0xA205B064)

void
#if defined(_M_IX86)
__fastcall
#else
__cdecl
#endif
__security_check_cookie(UINT_PTR Cookie);

typedef struct _GS_HANDLER_DATA
{
    union
    {
        struct
        {
            ULONG   HasAlignment    : 1;
        } Bits;
        LONG        CookieOffset;
    } u;
    LONG            AlignedBaseOffset;
    LONG            Alignment;
} GS_HANDLER_DATA, *PGS_HANDLER_DATA;

DWORD* __crt_get_kernel_flags();

/*
 * This is a crude way to capture the current CPU context, but it works.
 * It causes an access violation exception and copies the CPU context
 * captured by the OS into the provided context structure.  Debugger
 * exception processing is disabled.
 */
#define _CRT_CAPTURE_CONTEXT(pContextRecord)                           \
do {                                                                   \
    DWORD TlsKernBackup = *__crt_get_kernel_flags();                   \
    *__crt_get_kernel_flags() |= TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG; \
    __try                                                              \
    {                                                                  \
        *(unsigned char volatile *)0 = 0;                              \
    }                                                                  \
    __except(memcpy(pContextRecord,                                    \
                    (GetExceptionInformation())->ContextRecord,        \
                    sizeof(*pContextRecord)),                          \
             EXCEPTION_EXECUTE_HANDLER)                                \
    {                                                                  \
    }                                                                  \
    *__crt_get_kernel_flags() = TlsKernBackup;                         \
} while (0,0)

#ifdef __cplusplus
}
#endif

#if defined(_ARM_) || defined(_MIPS_) || defined(_SHX_)

// From nk<arch>.h

//
// Define exception handling structures and function prototypes.
//
// Function table entry structure definition.
//

typedef struct _RUNTIME_FUNCTION {
    ULONG BeginAddress;
    ULONG EndAddress;
    PEXCEPTION_ROUTINE ExceptionHandler;
    PVOID HandlerData;
    ULONG PrologEndAddress;
} RUNTIME_FUNCTION, *PRUNTIME_FUNCTION;

//
// Scope table structure definition.
//

typedef struct _SCOPE_TABLE {
    ULONG Count;
    struct
    {
        ULONG BeginAddress;
        ULONG EndAddress;
        ULONG HandlerAddress;
        ULONG JumpTarget;
    } ScopeRecord[1];
} SCOPE_TABLE, *PSCOPE_TABLE;

#if defined(_ARM_) || defined(_SHX_)

typedef
LONG
(*EXCEPTION_FILTER) (
    ULONG EstablisherFrame,
    struct _EXCEPTION_POINTERS *ExceptionPointers
    );

typedef
VOID
(*TERMINATION_HANDLER) (
    ULONG EstablisherFrame,
    BOOLEAN is_abnormal
    );

#else

typedef
LONG
(*EXCEPTION_FILTER) (
    struct _EXCEPTION_POINTERS *ExceptionPointers
    );

typedef
VOID
(*TERMINATION_HANDLER) (
    BOOLEAN is_abnormal
    );


#endif // defined(_ARM_) || defined(_SHX_)

#endif // defined(_ARM_) || defined(_MIPS_) || defined(_SHX_)

#if defined(_ARM_)
#define CONTEXT_TO_PROGRAM_COUNTER(Context) ((Context)->Pc)
#elif defined(_MIPS_) || defined(_SHX_)
#define CONTEXT_TO_PROGRAM_COUNTER(Context) ((Context)->Fir)
#elif defined(_X86_)
#define CONTEXT_TO_PROGRAM_COUNTER(Context) ((Context)->Eip)
#endif

#endif /* _INC_CORECRT */
