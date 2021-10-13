//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _INC_CORECRT
#define _INC_CORECRT

#include <windows.h>
#include <stdlib.h>

#define _CMNINTRIN_DECLARE_ONLY
#include <cmnintrin.h>
#undef _CMNINTRIN_DECLARE_ONLY

/* During CRT building, there is never a time where it is
 *  useful to see the compiler warning, "'foo' not
 *  available as an intrinsic function.
 */
#pragma warning(disable:4163)

#ifdef x86
#define ALIGN(x)  ((unsigned long *)(x))
#else
#define ALIGN(x)  ((unsigned long __unaligned *)(x))
#endif

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

typedef struct _strflt {
    int sign;         /* zero if positive otherwise negative */
    int decpt;        /* exponent of floating point number */
    int flag;         /* zero if okay otherwise IEEE overflow */
    char mantissa[MAX_MAN_DIGITS+1];   /* pointer to mantissa in string form */
} *STRFLT;

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
int $I10_OUTPUT(_LDOUBLE ld, FOS *fos);
INTRNCVT_STATUS _ld12tod(_LDBL12 *pld12, double *d);

char *_cftoe(double *, char *, int, int);
char *_cftof(double *, char *, int);
char *_cftog(double * pvalue, char * buf, int ndec, int caps);
void __cdecl _fassign(int flag, char * argument, char * number);

void _fltout2(double x, STRFLT pflt);
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

#if defined(M32R)
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
UINT64 name (CRTUINT64 x, CRTUINT64 y) { \
    CRTUINT64 res; \
    LOW_PART(res) = LOW_PART(x) operator LOW_PART(y);      \
    HIGH_PART(res) = HIGH_PART(x) operator HIGH_PART(y);   \
    return res.val;                                               \
}
#define DECLARE_UNARY_BITOP(name,operator)                    \
UINT64 name (CRTUINT64 x) {                      \
    CRTUINT64 res; \
    LOW_PART(res) = operator LOW_PART(x);                   \
    HIGH_PART(res) = operator HIGH_PART(x);                 \
    return res.val;                                               \
}
#endif

#if defined(PPC) || defined(x86) || defined(_M_ARM) || defined(_M_TRICORE) || defined(M32R)
#define _IS_MAN_IND(signbit, manhi, manlo) ((signbit) && (manhi)==0xc0000000 && (manlo)==0)
#elif defined(SHx) || defined(_M_SH) || defined(_M_AM) || defined(MIPS) 
#define _IS_MAN_IND(signbit, manhi, manlo) (!(signbit) && (manhi)==0xbfffffff && (manlo)==0xfffff800)
#endif

#define NAN_BIT (1<<30)

#define _IS_MAN_INF(signbit, manhi, manlo) ((manhi)==MSB_ULONG && (manlo)==0x0)


#if defined(_M_ARM) || defined(PPC) || defined(_M_TRICORE) || defined(x86)  || defined(M32R)
#define _IS_MAN_SNAN(signbit, manhi, manlo) (!((manhi)&NAN_BIT))
#else
#define _IS_MAN_SNAN(signbit, manhi, manlo) ((manhi)&NAN_BIT)
#endif

/*
 * Default value used for the global /GS security cookie
 */
#define DEFAULT_SECURITY_COOKIE (0xA205B064)

/*
 * /GS buffer overrun error code for handler
 */
#define _SECERR_BUFFER_OVERRUN 1    /* void* arg ignored */

#endif /* _INC_CORECRT */
