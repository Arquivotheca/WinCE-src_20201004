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
*output.c - printf style output to a FILE
*
*
*Purpose:
*       This file contains the code that does all the work for the
*       printf family of functions.  It should not be called directly, only
*       by the *printf functions.  We don't make any assumtions about the
*       sizes of ints, longs, shorts, or long doubles, but if types do overlap,
*       we also try to be efficient.  We do assume that pointers are the same
*       size as either ints or longs.
*       **** DOESN'T CURRENTLY DO MTHREAD LOCKING ****
*
*******************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

#if _INTEGRAL_MAX_BITS < 64
#undef _INTEGRAL_MAX_BITS
#define _INTEGRAL_MAX_BITS 64
#endif

#include <corecrt.h>
#include <cruntime.h>
#include <crttchar.h>
#include <crtmisc.h>
#include <limits.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <internal.h>
#include <process.h>
#include <fltintrn.h>
#include <stdlib.h>
#include <ctype.h>
#include <dbgint.h>
#include <setlocal.h>

#ifndef _MBTOWC
#define _MBTOWC change_to_widechar
#endif

#ifndef _WCTOMB_S
#define _WCTOMB_S __internal_wctomb_s
#endif

#ifndef _CFLTCVT
#define _CFLTCVT _cfltcvt
#endif

#ifndef _CLDCVT
#define _CLDCVT _cldcvt
#endif

/* inline keyword is non-ANSI C7 extension */
#if     !defined(_MSC_VER) || defined(__STDC__)
#define __inline static
#endif

#ifdef  _MBCS   /* always want either Unicode or SBCS for tchar.h */
#undef  _MBCS
#endif
#include <tchar.h>

/* this macro defines a function which is private and as fast as possible: */
/* for example, in C 6.0, it might be static _fastcall <type> near. */
#define LOCAL(x) static x __cdecl

/* int/long/short/pointer sizes */

/* the following should be set depending on the sizes of various types */
#define LONG_IS_INT      1      /* 1 means long is same size as int */
#define SHORT_IS_INT     0      /* 1 means short is same size as int */
#define LONGDOUBLE_IS_DOUBLE 1  /* 1 means long double is same as double */
#define LONGLONG_IS_INT64 1     /* 1 means long long is same as int64 */
#define PTR_IS_INT       1      /* 1 means ptr is same size as int */
#define PTR_IS_LONG      1      /* 1 means ptr is same size as long */
#define PTR_IS_INT64     0      /* 1 means ptr is same size as int64 */

#if     LONGLONG_IS_INT64
    #define get_long_long_arg(x) (long long)get_int64_arg(x)
#endif

#if     LONG_IS_INT
    #define get_long_arg(x) (long)get_int_arg(x)
#endif

#ifndef CRT_UNICODE
#if SHORT_IS_INT
    #define get_short_arg(x) (short)get_int_arg(x)
#endif
#endif

#if     PTR_IS_INT
    #define get_ptr_arg(x) (void *)(intptr_t)get_int_arg(x)
#elif   PTR_IS_LONG
    #define get_ptr_arg(x) (void *)(intptr_t)get_long_arg(x)
#elif   PTR_IS_INT64
    #define get_ptr_arg(x) (void *)get_int64_arg(x)
#else
    #error Size of pointer must be same as size of int or long
#endif



/* CONSTANTS */

/* size of conversion buffer (ANSI-specified minimum is 509) */

#define BUFFERSIZE    512
#define MAXPRECISION  BUFFERSIZE

#if     BUFFERSIZE < _CVTBUFSIZE + 6
/*
 * Buffer needs to be big enough for default minimum precision
 * when converting floating point needs bigger buffer, and malloc
 * fails
 */
#error Conversion buffer too small for max double.
#endif

/* flag definitions */
#define FL_SIGN       0x00001   /* put plus or minus in front */
#define FL_SIGNSP     0x00002   /* put space or minus in front */
#define FL_LEFT       0x00004   /* left justify */
#define FL_LEADZERO   0x00008   /* pad with leading zeros */
#define FL_LONG       0x00010   /* long value given */
#define FL_SHORT      0x00020   /* short value given */
#define FL_SIGNED     0x00040   /* signed data given */
#define FL_ALTERNATE  0x00080   /* alternate form requested */
#define FL_NEGATIVE   0x00100   /* value is negative */
#define FL_FORCEOCTAL 0x00200   /* force leading '0' for octals */
#define FL_LONGDOUBLE 0x00400   /* long double value given */
#define FL_WIDECHAR   0x00800   /* wide characters */
#define FL_LONGLONG   0x01000   /* long long value given */
#define FL_I64        0x08000   /* __int64 value given */

/* state definitions */
enum STATE {
    ST_NORMAL,          /* normal state; outputting literal chars */
    ST_PERCENT,         /* just read '%' */
    ST_FLAG,            /* just read flag character */
    ST_WIDTH,           /* just read width specifier */
    ST_DOT,             /* just read '.' */
    ST_PRECIS,          /* just read precision specifier */
    ST_SIZE,            /* just read size specifier */
    ST_TYPE,            /* just read type specifier */
    ST_INVALID          /* Invalid format */
};

#define NUMSTATES_S (ST_INVALID + 1)
#define NUMSTATES (ST_TYPE + 1)

/* character type values */
enum CHARTYPE {
    CH_OTHER,           /* character with no special meaning */
    CH_PERCENT,         /* '%' */
    CH_DOT,             /* '.' */
    CH_STAR,            /* '*' */
    CH_ZERO,            /* '0' */
    CH_DIGIT,           /* '1'..'9' */
    CH_FLAG,            /* ' ', '+', '-', '#' */
    CH_SIZE,            /* 'h', 'l', 'L', 'N', 'F', 'w' */
    CH_TYPE             /* type specifying character */
};

/* static data (read only, since we are re-entrant) */
#if     defined(CRT_UNICODE)
extern char *__nullstring;  /* string to print on null ptr */
extern wchar_t *__wnullstring;  /* string to print on null ptr */
#else   /* CRT_UNICODE */
char *__nullstring = "(null)";  /* string to print on null ptr */
wchar_t *__wnullstring = L"(null)";/* string to print on null ptr */
#endif  /* CRT_UNICODE */

/* The state table.  This table is actually two tables combined into one. */
/* The lower nybble of each byte gives the character class of any         */
/* character; while the uper nybble of the byte gives the next state      */
/* to enter.  See the macros below the table for details.                 */
/*                                                                        */
/* The table is generated by maketabc.c -- use this program to make       */
/* changes.                                                               */

#if defined(CRT_UNICODE)
extern const char __lookuptable[];
#else /* defined(CRT_UNICODE) */
extern const char __lookuptable[] = {
 /* ' ' */  0x06,
 /* '!' */  0x00,
 /* '"' */  0x00,
 /* '#' */  0x06,
 /* '$' */  0x00,
 /* '%' */  0x01,
 /* '&' */  0x00,
 /* ''' */  0x00,
 /* '(' */  0x10,
 /* ')' */  0x00,
 /* '*' */  0x03,
 /* '+' */  0x06,
 /* ',' */  0x00,
 /* '-' */  0x06,
 /* '.' */  0x02,
 /* '/' */  0x10,
 /* '0' */  0x04,
 /* '1' */  0x45,
 /* '2' */  0x45,
 /* '3' */  0x45,
 /* '4' */  0x05,
 /* '5' */  0x05,
 /* '6' */  0x05,
 /* '7' */  0x05,
 /* '8' */  0x05,
 /* '9' */  0x35,
 /* ':' */  0x30,
 /* ';' */  0x00,
 /* '<' */  0x50,
 /* '=' */  0x00,
 /* '>' */  0x00,
 /* '?' */  0x00,
 /* '@' */  0x00,
 /* 'A' */  0x20,       // Disable %A format
 /* 'B' */  0x20,
 /* 'C' */  0x38,
 /* 'D' */  0x50,
 /* 'E' */  0x58,
 /* 'F' */  0x07,
 /* 'G' */  0x08,
 /* 'H' */  0x00,
 /* 'I' */  0x37,
 /* 'J' */  0x30,
 /* 'K' */  0x30,
 /* 'L' */  0x57,
 /* 'M' */  0x50,
 /* 'N' */  0x07,
 /* 'O' */  0x00,
 /* 'P' */  0x00,
 /* 'Q' */  0x20,
 /* 'R' */  0x20,
 /* 'S' */  0x08,
 /* 'T' */  0x00,
 /* 'U' */  0x00,
 /* 'V' */  0x00,
 /* 'W' */  0x00,
 /* 'X' */  0x08,
 /* 'Y' */  0x60,
 /* 'Z' */  0x68,
 /* '[' */  0x60,
 /* '\' */  0x60,
 /* ']' */  0x60,
 /* '^' */  0x60,
 /* '_' */  0x00,
 /* '`' */  0x00,
 /* 'a' */  0x70,       // Disable %a format
 /* 'b' */  0x70,
 /* 'c' */  0x78,
 /* 'd' */  0x78,
 /* 'e' */  0x78,
 /* 'f' */  0x78,
 /* 'g' */  0x08,
 /* 'h' */  0x07,
 /* 'i' */  0x08,
 /* 'j' */  0x00,
 /* 'k' */  0x00,
 /* 'l' */  0x07,
 /* 'm' */  0x00,
 /* 'n' */  0x08,
 /* 'o' */  0x08,
 /* 'p' */  0x08,
 /* 'q' */  0x00,
 /* 'r' */  0x00,
 /* 's' */  0x08,
 /* 't' */  0x00,
 /* 'u' */  0x08,
 /* 'v' */  0x00,
 /* 'w' */  0x07,
 /* 'x' */  0x08
};

#endif /* defined(CRT_UNICODE) */

#if defined(CRT_UNICODE)
extern const unsigned char __lookuptable_s[];
#else /* defined(CRT_UNICODE) */
extern const unsigned char __lookuptable_s[] = {
 /* ' ' */  0x06,
 /* '!' */  0x80,
 /* '"' */  0x80,
 /* '#' */  0x86,
 /* '$' */  0x80,
 /* '%' */  0x81,
 /* '&' */  0x80,
 /* ''' */  0x00,
 /* '(' */  0x00,
 /* ')' */  0x10,
 /* '*' */  0x03,
 /* '+' */  0x86,
 /* ',' */  0x80,
 /* '-' */  0x86,
 /* '.' */  0x82,
 /* '/' */  0x80,
 /* '0' */  0x14,
 /* '1' */  0x05,
 /* '2' */  0x05,
 /* '3' */  0x45,
 /* '4' */  0x45,
 /* '5' */  0x45,
 /* '6' */  0x85,
 /* '7' */  0x85,
 /* '8' */  0x85,
 /* '9' */  0x05,
 /* ':' */  0x00,
 /* ';' */  0x00,
 /* '<' */  0x30,
 /* '=' */  0x30,
 /* '>' */  0x80,
 /* '?' */  0x50,
 /* '@' */  0x80,
 /* 'A' */  0x80,       // Disable %A format
 /* 'B' */  0x00,
 /* 'C' */  0x08,
 /* 'D' */  0x00,
 /* 'E' */  0x28,
 /* 'F' */  0x27,
 /* 'G' */  0x38,
 /* 'H' */  0x50,
 /* 'I' */  0x57,
 /* 'J' */  0x80,
 /* 'K' */  0x00,
 /* 'L' */  0x07,
 /* 'M' */  0x00,
 /* 'N' */  0x37,
 /* 'O' */  0x30,
 /* 'P' */  0x30,
 /* 'Q' */  0x50,
 /* 'R' */  0x50,
 /* 'S' */  0x88,
 /* 'T' */  0x00,
 /* 'U' */  0x00,
 /* 'V' */  0x00,
 /* 'W' */  0x20,
 /* 'X' */  0x28,
 /* 'Y' */  0x80,
 /* 'Z' */  0x88,
 /* '[' */  0x80,
 /* '\' */  0x80,
 /* ']' */  0x00,
 /* '^' */  0x00,
 /* '_' */  0x00,
 /* '`' */  0x60,
 /* 'a' */  0x68,
 /* 'b' */  0x60,
 /* 'c' */  0x68,
 /* 'd' */  0x68,
 /* 'e' */  0x68,
 /* 'f' */  0x08,
 /* 'g' */  0x08,
 /* 'h' */  0x07,
 /* 'i' */  0x78,
 /* 'j' */  0x70,
 /* 'k' */  0x70,
 /* 'l' */  0x77,
 /* 'm' */  0x70,
 /* 'n' */  0x70,
 /* 'o' */  0x08,
 /* 'p' */  0x08,
 /* 'q' */  0x00,
 /* 'r' */  0x00,
 /* 's' */  0x08,
 /* 't' */  0x00,
 /* 'u' */  0x08,
 /* 'v' */  0x00,
 /* 'w' */  0x07,
 /* 'x' */  0x08
};
#endif /* defined(CRT_UNICODE) */

#define FIND_CHAR_CLASS(lookuptbl, c)      \
        ((c) < CRT_T(' ') || (c) > CRT_T('x') ? \
            CH_OTHER            \
            :               \
        (enum CHARTYPE)(lookuptbl[(c)-CRT_T(' ')] & 0xF))

#define FIND_NEXT_STATE(lookuptbl, class, state)   \
        (enum STATE)(lookuptbl[(class) * NUMSTATES + (state)] >> 4)

#define FIND_NEXT_STATE_S(lookuptbl, class, state)   \
        (enum STATE)(lookuptbl[(class) * NUMSTATES_S + (state)] >> 4)

/* prototypes */

#define WRITE_CHAR(ch, pnw)         write_char(ch, stream, pnw)
#define WRITE_MULTI_CHAR(ch, num, pnw)  write_multi_char(ch, num, stream, pnw)
#define WRITE_STRING(s, len, pnw)   write_string(s, len, stream, pnw)
#define WRITE_WSTRING(s, len, pnw)  write_wstring(s, len, stream, pnw)

LOCAL(void) write_char(CRT__TCHAR ch, FILEX *f, int *pnumwritten);
LOCAL(void) write_multi_char(CRT__TCHAR ch, int num, FILEX *f, int *pnumwritten);
LOCAL(void) write_string(CRT__TCHAR *string, int len, FILEX *f, int *numwritten);
LOCAL(void) write_wstring(wchar_t *string, int len, FILEX *f, int *numwritten);

__inline int __cdecl get_int_arg(va_list *pargptr);

#ifndef CRT_UNICODE
#if     !SHORT_IS_INT
__inline short __cdecl get_short_arg(va_list *pargptr);
#endif
#endif

#if     !LONG_IS_INT
__inline long __cdecl get_long_arg(va_list *pargptr);
#endif

#if     !LONGLONG_IS_INT64
__inline long long __cdecl get_long_long_arg(va_list *pargptr);
#endif

#if     _INTEGRAL_MAX_BITS >= 64
__inline __int64 __cdecl get_int64_arg(va_list *pargptr);
#endif


/***
*int _output(stream, format, argptr), static int output(format, argptr)
*
*Purpose:
*   Output performs printf style output onto a stream.  It is called by
*   printf/fprintf/sprintf/vprintf/vfprintf/vsprintf to so the dirty
*   work.  In multi-thread situations, _output assumes that the given
*   stream is already locked.
*
*   Algorithm:
*       The format string is parsed by using a finite state automaton
*       based on the current state and the current character read from
*       the format string.  Thus, looping is on a per-character basis,
*       not a per conversion specifier basis.  Once the format specififying
*       character is read, output is performed.
*
*Entry:
*   FILEX *stream   - stream for output
*   char *format   - printf style format string
*   va_list argptr - pointer to list of subsidiary arguments
*
*Exit:
*   Returns the number of characters written, or -1 if an output error
*   occurs.
*ifdef CRT_UNICODE
*   The wide-character flavour returns the number of wide-characters written.
*endif
*
*Exceptions:
*
*******************************************************************************/
int
__cdecl
_toutput (
    FILEX *stream,
    const CRT__TCHAR *format,
    va_list argptr,
    BOOL fFormatValidations
    )
{
    int hexadd=0;           /* offset to add to number to get 'a'..'f' */
    CRT_TCHAR ch;           /* character just read */
    int flags=0;            /* flag word -- see #defines above for flag values */
    enum STATE state;       /* current state */
    enum CHARTYPE chclass;  /* class of current character */
    int radix;              /* current conversion radix */
    int charsout;           /* characters currently written so far, -1 = IO error */
    int fldwidth = 0;       /* selected field width -- 0 means default */
    int precision = 0;      /* selected precision  -- -1 means default */
    CRT_TCHAR prefix[2];    /* numeric prefix -- up to two characters */
    int prefixlen = 0;      /* length of prefix -- 0 means no prefix */
    int capexp = 0;         /* non-zero = 'E' exponent signifient, zero = 'e' */
    int no_output = 0;      /* non-zero = prodcue no output for this specifier */
    union {
        char *sz;           /* pointer text to be printed, not zero terminated */
        wchar_t *wz;
    } text = {0};

    int textlen;            /* length of the text in bytes/wchars to be printed.
                               textlen is in multibyte or wide chars if CRT_UNICODE */
    union {
        char sz[BUFFERSIZE];
#ifdef  CRT_UNICODE
        wchar_t wz[BUFFERSIZE];
#endif
    } buffer;
    wchar_t wchar;          /* temp wchar_t */
    int buffersize;         /* size of text.sz (used only for the call to _cfltcvt) */
    int bufferiswide=0;     /* non-zero = buffer contains wide chars already */

    char *heapbuf = NULL;   /* non-zero = test.sz using heap buffer to be freed */

    _VALIDATE_RETURN( (stream != NULL), EINVAL, -1);
#ifndef CRT_UNICODE
//  Don't support textmode, so we can't validate this
//    _VALIDATE_STREAM_ANSI_RETURN(stream, EINVAL, EOF);
#endif
    _VALIDATE_RETURN( (format != NULL), EINVAL, -1);

    charsout = 0;           /* no characters written yet */
    textlen = 0;            /* no text yet */
    state = ST_NORMAL;      /* starting state */
    heapbuf = NULL;         /* not using heap-allocated buffer */
    buffersize = 0;
    /* main loop -- loop while format character exist and no I/O errors */
    while ((ch = *format++) != CRT_T('\0') && charsout >= 0) {
        if (!fFormatValidations) {
            chclass = FIND_CHAR_CLASS(__lookuptable, ch);  /* find character class */
            state = FIND_NEXT_STATE(__lookuptable, chclass, state); /* find next state */
        } else {
            chclass = FIND_CHAR_CLASS(__lookuptable_s, ch);  /* find character class */
            state = FIND_NEXT_STATE_S(__lookuptable_s, chclass, state); /* find next state */

            _VALIDATE_RETURN((state != ST_INVALID), EINVAL, -1);
        }

        /* execute code for each state */
        switch (state) {

        case ST_NORMAL:

        NORMAL_STATE:

            /* normal state -- just write character */
#ifdef  CRT_UNICODE
            bufferiswide = 1;
#else
            bufferiswide = 0;
            if (isleadbyte((unsigned char)ch)) {
                WRITE_CHAR(ch, &charsout);
                ch = *format++;
                if (g_fCrtLegacyInputValidation) {
                    _ASSERTE(ch != CRT_T('\0'));
                } else {
                    /* don't fall off format string */
                    _VALIDATE_RETURN( (ch != CRT_T('\0')), EINVAL, -1);
                }
            }
#endif  /* !CRT_UNICODE */
            WRITE_CHAR(ch, &charsout);
            break;

        case ST_PERCENT:
            /* set default value of conversion parameters */
            prefixlen = fldwidth = no_output = capexp = 0;
            flags = 0;
            precision = -1;
            bufferiswide = 0;   /* default */
            break;

        case ST_FLAG:
            /* set flag based on which flag character */
            switch (ch) {
            case CRT_T('-'):
                flags |= FL_LEFT;   /* '-' => left justify */
                break;
            case CRT_T('+'):
                flags |= FL_SIGN;   /* '+' => force sign indicator */
                break;
            case CRT_T(' '):
                flags |= FL_SIGNSP; /* ' ' => force sign or space */
                break;
            case CRT_T('#'):
                flags |= FL_ALTERNATE;  /* '#' => alternate form */
                break;
            case CRT_T('0'):
                flags |= FL_LEADZERO;   /* '0' => pad with leading zeros */
                break;
            }
            break;

        case ST_WIDTH:
            /* update width value */
            if (ch == CRT_T('*')) {
                /* get width from arg list */
                fldwidth = get_int_arg(&argptr);
                if (fldwidth < 0) {
                    /* ANSI says neg fld width means '-' flag and pos width */
                    flags |= FL_LEFT;
                    fldwidth = -fldwidth;
                }
            }
            else {
                /* add digit to current field width */
                fldwidth = fldwidth * 10 + (ch - CRT_T('0'));
            }
            break;

        case ST_DOT:
            /* zero the precision, since dot with no number means 0
               not default, according to ANSI */
            precision = 0;
            break;

        case ST_PRECIS:
            /* update precison value */
            if (ch == CRT_T('*')) {
                /* get precision from arg list */
                precision = get_int_arg(&argptr);
                if (precision < 0)
                    precision = -1; /* neg precision means default */
            }
            else {
                /* add digit to current precision */
                precision = precision * 10 + (ch - CRT_T('0'));
            }
            break;

        case ST_SIZE:
            /* just read a size specifier, set the flags based on it */
            switch (ch) {
            case CRT_T('l'):
                /*
                 * In order to handle the ll case, we depart from the
                 * simple deterministic state machine.
                 */
                if (*format == CRT_T('l'))
                {
                    ++format;
                    flags |= FL_LONGLONG;   /* 'll' => long long */
                }
                else
                {
                    flags |= FL_LONG;   /* 'l' => long int or wchar_t */
                }
                break;

            case CRT_T('I'):
                /*
                 * In order to handle the I, I32, and I64 size modifiers, we
                 * depart from the simple deterministic state machine. The
                 * code below scans for characters following the 'I',
                 * and defaults to 64 bit on WIN64 and 32 bit on WIN32
                 */
#if     PTR_IS_INT64
                flags |= FL_I64;    /* 'I' => __int64 on WIN64 systems */
#endif
                if ( (*format == CRT_T('6')) && (*(format + 1) == CRT_T('4')) )
                {
                    format += 2;
                    flags |= FL_I64;    /* I64 => __int64 */
                }
                else if ( (*format == CRT_T('3')) && (*(format + 1) == CRT_T('2')) )
                {
                    format += 2;
                    flags &= ~FL_I64;   /* I32 => __int32 */
                }
                else if ( (*format == CRT_T('d')) ||
                          (*format == CRT_T('i')) ||
                          (*format == CRT_T('o')) ||
                          (*format == CRT_T('u')) ||
                          (*format == CRT_T('x')) ||
                          (*format == CRT_T('X')) )
                {
                   /*
                    * Nothing further needed.  %Id (et al) is
                    * handled just like %d, except that it defaults to 64 bits
                    * on WIN64.  Fall through to the next iteration.
                    */
                }
                else {
                    state = ST_NORMAL;
                    goto NORMAL_STATE;
                }
                break;

            case CRT_T('h'):
                flags |= FL_SHORT;  /* 'h' => short int or char */
                break;

            case CRT_T('w'):
                flags |= FL_WIDECHAR;  /* 'w' => wide character */
                break;

            }
            break;

        case ST_TYPE:
            /* we have finally read the actual type character, so we       */
            /* now format and "print" the output.  We use a big switch     */
            /* statement that sets 'text' to point to the text that should */
            /* be printed, and 'textlen' to the length of this text.       */
            /* Common code later on takes care of justifying it and        */
            /* other miscellaneous chores.  Note that cases share code,    */
            /* in particular, all integer formatting is done in one place. */
            /* Look at those funky goto statements!                        */

            switch (ch) {

            case CRT_T('C'):   /* ISO wide character */
                if (!(flags & (FL_SHORT|FL_LONG|FL_WIDECHAR)))
#ifdef  CRT_UNICODE
                    flags |= FL_SHORT;
#else
                    flags |= FL_WIDECHAR;   /* ISO std. */
#endif
                /* fall into 'c' case */

            case CRT_T('c'): {
                /* print a single character specified by int argument */
#ifdef  CRT_UNICODE
                bufferiswide = 1;
                        wchar = (wchar_t) get_int_arg(&argptr);
                if (flags & FL_SHORT) {
                    /* format multibyte character */
                    /* this is an extension of ANSI */
                    char tempchar[2];
#ifdef  _OUT
                    if (isleadbyte((unsigned char)(wchar >> 8))) {
                        tempchar[0] = (wchar >> 8);
                        tempchar[1] = (wchar & 0x00ff);
                    }
                    else
#endif  /* _OUT */
                    {
                        tempchar[0] = (char)(wchar & 0x00ff);
                        tempchar[1] = '\0';
                    }

                    // BUGBUG: This is BROKEN. Need correct size & correct treatment of lead & trail bytes
                    if (_MBTOWC(buffer.wz,tempchar, MB_CUR_MAX) < 0)
                    {
                        /* ignore if conversion was unsuccessful */
                        no_output = 1;
                    }
                } else {
                    buffer.wz[0] = wchar;
                }
                text.wz = buffer.wz;
                textlen = 1;    /* print just a single character */
#else   /* CRT_UNICODE */
                if (flags & (FL_LONG|FL_WIDECHAR)) {
                    errno_t e = 0;
                    wchar = (wchar_t) get_short_arg(&argptr);
                    /* convert to multibyte character */
                    e = _WCTOMB_S(&textlen, buffer.sz, _countof(buffer.sz), wchar);

                    /* check that conversion was successful */
                    if (e != 0)
                        no_output = 1;
                } else {
                    /* format multibyte character */
                    /* this is an extension of ANSI */
                    unsigned short temp;
                    temp = (unsigned short) get_int_arg(&argptr);
#ifdef  _OUT
                    if (isleadbyte((unsigned char)(temp >> 8))) {
                        buffer.sz[0] = temp >> 8;
                        buffer.sz[1] = temp & 0x00ff;
                        textlen = 2;
                    } else
#endif  /* _OUT */
                    {
                        buffer.sz[0] = (char) temp;
                        textlen = 1;
                    }
                }
                text.sz = buffer.sz;
#endif  /* CRT_UNICODE */
            }
            break;

            case CRT_T('Z'): {
                /* print a Counted String

                int i;
                char *p;       /* temps */
                struct _count_string {
                    short Length;
                    short MaximumLength;
                    char *Buffer;
                } *pstr;

                pstr = (struct _count_string *)get_ptr_arg(&argptr);
                if (pstr == NULL || pstr->Buffer == NULL) {
                    /* null ptr passed, use special string */
                    text.sz = __nullstring;
                    textlen = (int)strlen(text.sz);
                } else {
                    if (flags & FL_WIDECHAR) {
                        text.wz = (wchar_t *)pstr->Buffer;
                        textlen = pstr->Length / (int)sizeof(wchar_t);
                        bufferiswide = 1;
                    } else {
                        bufferiswide = 0;
                        text.sz = pstr->Buffer;
                        textlen = pstr->Length;
                    }
                }
            }
            break;

            case CRT_T('S'):   /* ISO wide character string */
#ifndef CRT_UNICODE
                if (!(flags & (FL_SHORT|FL_LONG|FL_WIDECHAR)))
                    flags |= FL_WIDECHAR;
#else
                if (!(flags & (FL_SHORT|FL_LONG|FL_WIDECHAR)))
                    flags |= FL_SHORT;
#endif

            case CRT_T('s'): {
                /* print a string --                            */
                /* ANSI rules on how much of string to print:   */
                /*   all if precision is default,               */
                /*   min(precision, length) if precision given. */
                /* prints '(null)' if a null string is passed   */

                int i;
                char *p;       /* temps */
                wchar_t *pwch;

                /* At this point it is tempting to use strlen(), but */
                /* if a precision is specified, we're not allowed to */
                /* scan past there, because there might be no null   */
                /* at all.  Thus, we must do our own scan.           */

                i = (precision == -1) ? INT_MAX : precision;
                text.sz = (char *)get_ptr_arg(&argptr);

                /* scan for null upto i characters */
#ifdef  CRT_UNICODE
                if (flags & FL_SHORT) {
                    if (text.sz == NULL) /* NULL passed, use special string */
                        text.sz = __nullstring;
                    p = text.sz;
                    for (textlen=0; textlen<i && *p; textlen++) {
                        if (isleadbyte((unsigned char)(*p)))
                            ++p;
                        ++p;
                    }
                    /* textlen now contains length in multibyte chars */
                } else {
                    if (text.wz == NULL) /* NULL passed, use special string */
                        text.wz = __wnullstring;
                    bufferiswide = 1;
                    pwch = text.wz;
                    while (i-- && *pwch)
                        ++pwch;
                    textlen = (int)(pwch - text.wz);       /* in wchar_ts */
                    /* textlen now contains length in wide chars */
                }
#else   /* CRT_UNICODE */
                if (flags & (FL_LONG|FL_WIDECHAR)) {
                    if (text.wz == NULL) /* NULL passed, use special string */
                        text.wz = __wnullstring;
                    bufferiswide = 1;
                    pwch = text.wz;
                    while ( i-- && *pwch )
                        ++pwch;
                    textlen = (int)(pwch - text.wz);
                    /* textlen now contains length in wide chars */
                } else {
                    if (text.sz == NULL) /* NULL passed, use special string */
                        text.sz = __nullstring;
                    p = text.sz;
                    while (i-- && *p)
                        ++p;
                    textlen = (int)(p - text.sz);    /* length of the string */
                }

#endif  /* CRT_UNICODE */
            }
            break;


            case CRT_T('n'): {
                /* write count of characters seen so far into */
                /* short/int/long thru ptr read from args */

                void *p;        /* temp */

                p = get_ptr_arg(&argptr);

                /* if %n is disabled, we skip an arg and print 'n' */
                if ( !_get_printf_count_output() )
                {
                    _VALIDATE_RETURN(("'n' format specifier disabled", 0), EINVAL, -1);
                    break;
                }

                /* store chars out into short/long/int depending on flags */
#if     !LONG_IS_INT
                if (flags & FL_LONG)
                    *(long *)p = charsout;
                else
#endif

#if     !SHORT_IS_INT
                if (flags & FL_SHORT)
                    *(short *)p = (short) charsout;
                else
#endif
                    *(int *)p = charsout;

                no_output = 1;              /* force no output */
            }
            break;

            case CRT_T('E'):
            case CRT_T('G'):
            case CRT_T('A'):
                capexp = 1;                 /* capitalize exponent */
                ch += CRT_T('a') - CRT_T('A');    /* convert format char to lower */
                /* DROP THROUGH */
            case CRT_T('e'):
            case CRT_T('f'):
            case CRT_T('g'):
            case CRT_T('a'): {
                /* floating point conversion -- we call cfltcvt routines */
                /* to do the work for us.                                */
                flags |= FL_SIGNED;         /* floating point is signed conversion */
                text.sz = buffer.sz;        /* put result in buffer */
                buffersize = BUFFERSIZE;

                /* compute the precision value */
                if (precision < 0)
                    precision = 6;          /* default precision: 6 */
                else if (precision == 0 && ch == CRT_T('g'))
                    precision = 1;          /* ANSI specified */
                else if (precision > MAXPRECISION)
                    precision = MAXPRECISION;

                if (precision > BUFFERSIZE - _CVTBUFSIZE) {
                    /* conversion will potentially overflow local buffer */
                    /* so we need to use a heap-allocated buffer.        */
                    heapbuf = (char *)_malloc_crt(_CVTBUFSIZE + precision);
                    if (heapbuf != NULL)
                    {
                        text.sz = heapbuf;
                        buffersize = _CVTBUFSIZE + precision;
                    }
                    else
                        /* malloc failed, cap precision further */
                        precision = BUFFERSIZE - _CVTBUFSIZE;
                }

#if     !LONGDOUBLE_IS_DOUBLE
                /* do the conversion */
                if (flags & FL_LONGDOUBLE) {
                    _LONGDOUBLE tmp;
                    tmp=va_arg(argptr, _LONGDOUBLE);
                    /* Note: assumes ch is in ASCII range */
                    _CLDCVT(&tmp, text.sz, buffersize, (char)ch, precision, capexp);
                } else
#endif
                {
                    _CRT_DOUBLE tmp;
                    tmp=va_arg(argptr, _CRT_DOUBLE);
                    /* Note: assumes ch is in ASCII range */
                    _CFLTCVT((double*)&tmp, text.sz, buffersize, (char)ch, precision, capexp);
                }

                /* '#' and precision == 0 means force a decimal point */
                if ((flags & FL_ALTERNATE) && precision == 0)
                {
                    _forcdecpt(text.sz);
                }

                /* 'g' format means crop zero unless '#' given */
                if (ch == CRT_T('g') && !(flags & FL_ALTERNATE))
                {
                    _cropzeros(text.sz);
                }

                /* check if result was negative, save '-' for later */
                /* and point to positive part (this is for '0' padding) */
                if (*text.sz == '-') {
                    flags |= FL_NEGATIVE;
                    ++text.sz;
                }

                textlen = (int)strlen(text.sz);     /* compute length of text */
            }
            break;

            case CRT_T('d'):
            case CRT_T('i'):
                /* signed decimal output */
                flags |= FL_SIGNED;
                radix = 10;
                goto COMMON_INT;

            case CRT_T('u'):
                radix = 10;
                goto COMMON_INT;

            case CRT_T('p'):
                /* write a pointer -- this is like an integer or long */
                /* except we force precision to pad with zeros and */
                /* output in big hex. */

                precision = 2 * sizeof(void *);     /* number of hex digits needed */
#if     PTR_IS_INT64
                flags |= FL_I64;                    /* assume we're converting an int64 */
#elif   !PTR_IS_INT
                flags |= FL_LONG;                   /* assume we're converting a long */
#endif
                /* DROP THROUGH to hex formatting */

            case CRT_T('X'):
                /* unsigned upper hex output */
                hexadd = CRT_T('A') - CRT_T('9') - 1;     /* set hexadd for uppercase hex */
                goto COMMON_HEX;

            case CRT_T('x'):
                /* unsigned lower hex output */
                hexadd = CRT_T('a') - CRT_T('9') - 1;     /* set hexadd for lowercase hex */
                /* DROP THROUGH TO COMMON_HEX */

            COMMON_HEX:
                radix = 16;
                if (flags & FL_ALTERNATE) {
                    /* alternate form means '0x' prefix */
                    prefix[0] = CRT_T('0');
                    prefix[1] = (CRT_TCHAR)(CRT_T('x') - CRT_T('a') + CRT_T('9') + 1 + hexadd);  /* 'x' or 'X' */
                    prefixlen = 2;
                }
                goto COMMON_INT;

            case CRT_T('o'):
                /* unsigned octal output */
                radix = 8;
                if (flags & FL_ALTERNATE) {
                    /* alternate form means force a leading 0 */
                    flags |= FL_FORCEOCTAL;
                }
                /* DROP THROUGH to COMMON_INT */

            COMMON_INT: {
                /* This is the general integer formatting routine. */
                /* Basically, we get an argument, make it positive */
                /* if necessary, and convert it according to the */
                /* correct radix, setting text and textlen */
                /* appropriately. */

#if     _INTEGRAL_MAX_BITS >= 64
                unsigned __int64 number;    /* number to convert */
                int digit;              /* ascii value of digit */
                __int64 l;              /* temp long value */
#else
                unsigned long number;   /* number to convert */
                int digit;              /* ascii value of digit */
                long l;                 /* temp long value */
#endif

                /* 1. read argument into l, sign extend as needed */
#if     _INTEGRAL_MAX_BITS >= 64
                if (flags & FL_I64)
                    l = get_int64_arg(&argptr);
                else
#endif

                if (flags & FL_LONGLONG)
                    l = get_long_long_arg(&argptr);

                else

#if     !LONG_IS_INT
                if (flags & FL_LONG)
                    l = get_long_arg(&argptr);
                else
#endif

#if     !SHORT_IS_INT
                if (flags & FL_SHORT) {
                    if (flags & FL_SIGNED)
                        l = (short) get_int_arg(&argptr); /* sign extend */
                    else
                        l = (unsigned short) get_int_arg(&argptr);    /* zero-extend*/

                } else
#endif
                {
                    if (flags & FL_SIGNED)
                        l = get_int_arg(&argptr); /* sign extend */
                    else
                        l = (unsigned int) get_int_arg(&argptr);    /* zero-extend*/

                }

                /* 2. check for negative; copy into number */
                if ( (flags & FL_SIGNED) && l < 0) {
                    number = -l;
                    flags |= FL_NEGATIVE;   /* remember negative sign */
                } else {
                    number = l;
                }

#if     _INTEGRAL_MAX_BITS >= 64
                if ( (flags & FL_I64) == 0 && (flags & FL_LONGLONG) == 0 ) {
                    /*
                     * Unless printing a full 64-bit value, insure values
                     * here are not in cananical longword format to prevent
                     * the sign extended upper 32-bits from being printed.
                     */
                    number &= 0xffffffff;
                }
#endif

                /* 3. check precision value for default; non-default */
                /*    turns off 0 flag, according to ANSI. */
                if (precision < 0)
                    precision = 1;  /* default precision */
                else {
                    flags &= ~FL_LEADZERO;
                    if (precision > MAXPRECISION)
                        precision = MAXPRECISION;
                }

                /* 4. Check if data is 0; if so, turn off hex prefix */
                if (number == 0)
                    prefixlen = 0;

                /* 5. Convert data to ASCII -- note if precision is zero */
                /*    and number is zero, we get no digits at all.       */

                text.sz = &buffer.sz[BUFFERSIZE-1];    /* last digit at end of buffer */

                while (precision-- > 0 || number != 0) {
                    digit = (int)(number % radix) + '0';
                    number /= radix;                /* reduce number */
                    if (digit > '9') {
                        /* a hex digit, make it a letter */
                        digit += hexadd;
                    }
                    *text.sz-- = (char)digit;       /* store the digit */
                }

                textlen = (int)((char *)&buffer.sz[BUFFERSIZE-1] - text.sz); /* compute length of number */
                ++text.sz;          /* text points to first digit now */


                /* 6. Force a leading zero if FORCEOCTAL flag set */
                if ((flags & FL_FORCEOCTAL) && (textlen == 0 || text.sz[0] != '0')) {
                    *--text.sz = '0';
                    ++textlen;      /* add a zero */
                }
            }
            break;
            }



            /* At this point, we have done the specific conversion, and */
            /* 'text' points to text to print; 'textlen' is length.  Now we */
            /* justify it, put on prefixes, leading zeros, and then */
            /* print it. */

            if (!no_output) {
                int padding;    /* amount of padding, negative means zero */

                if (flags & FL_SIGNED) {
                    if (flags & FL_NEGATIVE) {
                        /* prefix is a '-' */
                        prefix[0] = CRT_T('-');
                        prefixlen = 1;
                    }
                    else if (flags & FL_SIGN) {
                        /* prefix is '+' */
                        prefix[0] = CRT_T('+');
                        prefixlen = 1;
                    }
                    else if (flags & FL_SIGNSP) {
                        /* prefix is ' ' */
                        prefix[0] = CRT_T(' ');
                        prefixlen = 1;
                    }
                }

                /* calculate amount of padding -- might be negative, */
                /* but this will just mean zero */
                padding = fldwidth - textlen - prefixlen;

                /* put out the padding, prefix, and text, in the correct order */

                if (!(flags & (FL_LEFT | FL_LEADZERO))) {
                    /* pad on left with blanks */
                    WRITE_MULTI_CHAR(CRT_T(' '), padding, &charsout);
                }

                /* write prefix */
                WRITE_STRING(prefix, prefixlen, &charsout);

                if ((flags & FL_LEADZERO) && !(flags & FL_LEFT)) {
                    /* write leading zeros */
                    WRITE_MULTI_CHAR(CRT_T('0'), padding, &charsout);
                }

                /* write text */
#ifndef CRT_UNICODE
                if (bufferiswide && (textlen > 0)) {
                    wchar_t *p;
                    int retval, count;
                    errno_t e = 0;
                    char L_buffer[MB_LEN_MAX+1];

                    p = text.wz;
                    count = textlen;
                    while (count--) {
                        e = _WCTOMB_S(&retval, L_buffer, _countof(L_buffer), *p++);
                        if (e != 0 || retval == 0) {
                            charsout = -1;
                            break;
                        }
                        WRITE_STRING(L_buffer, retval, &charsout);
                    }
                } else {
                    WRITE_STRING(text.sz, textlen, &charsout);
                }
#else
                if (!bufferiswide && textlen > 0) {
                    char *p;
                    int retval, count;

                    p = text.sz;
                    count = textlen;
                    while (count-- > 0) {
                        retval = _MBTOWC(&wchar, p, MB_CUR_MAX);
                        if (retval <= 0) {
                            charsout = -1;
                            break;
                        }
                        WRITE_CHAR(wchar, &charsout);
                        p += retval;
                    }
                } else {
                    WRITE_STRING(text.wz, textlen, &charsout);
                }
#endif  /* CRT_UNICODE */

                if (charsout >= 0 && (flags & FL_LEFT)) {
                    /* pad on right with blanks */
                    WRITE_MULTI_CHAR(CRT_T(' '), padding, &charsout);
                }

                /* we're done! */
            }
            if (heapbuf) {
                _free_crt(heapbuf);
                heapbuf = NULL;
            }
            break;
        }
    }

    if (fFormatValidations) {
        /* The format string shouldn't be incomplete - i.e. when we are finished
            with the format string, the last thing we should have encountered
            should have been a regular char to be output or a type specifier. Else
            the format string was incomplete */
        _VALIDATE_RETURN(((state == ST_NORMAL) || (state == ST_TYPE)), EINVAL, -1);
    }

    return charsout;        /* return value = number of characters written */
}

/*
 *  Future Optimizations for swprintf:
 *  - Don't free the memory used for converting the buffer to wide chars.
 *    Use realloc if the memory is not sufficient.  Free it at the end.
 */

/***
*void write_char(_TCHAR ch, FILEX *f, int *pnumwritten)
*
*Purpose:
*   Writes a single character to the given file/console.  If no error occurs,
*   then *pnumwritten is incremented; otherwise, *pnumwritten is set
*   to -1.
*
*Entry:
*   CRT__TCHAR ch    - character to write
*   FILEX *f         - file to write to
*   int *pnumwritten - pointer to integer to update with total chars written
*
*Exit:
*   No return value.
*
*Exceptions:
*
*******************************************************************************/

LOCAL(void) write_char (
    CRT__TCHAR ch,
    FILEX *f,
    int *pnumwritten
    )
{
    if ( (f->_flag & _IOSTRG) && f->_base == NULL)
    {
        ++(*pnumwritten);
        return;
    }
#ifdef  CRT_UNICODE
    /*
     * NOTE: EOF is intentional here.  _putwc_lk returns EOF, not WEOF on
     * failure.
     */
    if (_putwc_lk(ch, f) == EOF)
#else
    if (_putc_lk(ch, f) == EOF)
#endif  //CRT_UNICODE
        *pnumwritten = -1;
    else
        ++(*pnumwritten);
}

/***
*void write_multi_char(_TCHAR ch, int num, FILEX *f, int *pnumwritten)
*
*Purpose:
*   Writes num copies of a character to the given file/console.  If no error occurs,
*   then *pnumwritten is incremented by num; otherwise, *pnumwritten is set
*   to -1.  If num is negative, it is treated as zero.
*
*Entry:
*   CRT__TCHAR ch    - character to write
*   int num          - number of times to write the characters
*   FILEX *f         - file to write to
*   int *pnumwritten - pointer to integer to update with total chars written
*
*Exit:
*   No return value.
*
*Exceptions:
*
*******************************************************************************/

LOCAL(void) write_multi_char (
    CRT__TCHAR ch,
    int num,
    FILEX *f,
    int *pnumwritten
    )
{
    while (num-- > 0) {
        write_char(ch, f, pnumwritten);
        if (*pnumwritten == -1)
            break;
    }
}

/***
*ifdef CRT_UNICODE
*void write_string(_TCHAR *string, int len, FILEX *f, int *pnumwritten)
*
*Purpose:
*   Writes a string of the given length to the given file.  If no error occurs,
*   then *pnumwritten is incremented by len; otherwise, *pnumwritten is set
*   to -1.  If len is negative, it is treated as zero.
*
*Entry:
*   CRT__TCHAR *string - string to write (NOT null-terminated)
*   int len            - length of string
*   FILEX *f           - file to write to
*   int *pnumwritten   - pointer to integer to update with total chars written
*
*Exit:
*   No return value.
*
*Exceptions:
*
*******************************************************************************/

LOCAL(void) write_string (
    CRT__TCHAR *string,
    int len,
    FILEX *f,
    int *pnumwritten
    )
{
    if ( (f->_flag & _IOSTRG) && f->_base == NULL)
    {
        (*pnumwritten) += len;
        return;
    }
    while (len-- > 0) {
        write_char(*string++, f, pnumwritten);
        if (*pnumwritten == -1)
        {
#if 0 // no errno support
            if (errno == EILSEQ)
                write_char(CRT_T('?'), f, pnumwritten);
            else
#endif
                break;
        }
    }
}


/***
*int get_int_arg(va_list *pargptr)
*
*Purpose:
*   Gets an int argument off the given argument list and updates *pargptr.
*
*Entry:
*   va_list *pargptr - pointer to argument list; updated by function
*
*Exit:
*   Returns the integer argument read from the argument list.
*
*Exceptions:
*
*******************************************************************************/

__inline int __cdecl get_int_arg (
    va_list *pargptr
    )
{
    return va_arg(*pargptr, int);
}

/***
*long get_long_arg(va_list *pargptr)
*
*Purpose:
*   Gets an long argument off the given argument list and updates *pargptr.
*
*Entry:
*   va_list *pargptr - pointer to argument list; updated by function
*
*Exit:
*   Returns the long argument read from the argument list.
*
*Exceptions:
*
*******************************************************************************/

#if     !LONG_IS_INT
__inline long __cdecl get_long_arg (
    va_list *pargptr
    )
{
    return va_arg(*pargptr, long);
}
#endif

#if     !LONGLONG_IS_INT64
__inline long long __cdecl get_long_long_arg (
    va_list *pargptr
    )
{
    return va_arg(*pargptr, long long);
}
#endif

#if     _INTEGRAL_MAX_BITS >= 64
__inline __int64 __cdecl get_int64_arg (
    va_list *pargptr
    )
{
    return va_arg(*pargptr, __int64);
}
#endif

#ifndef CRT_UNICODE
/***
*short get_short_arg(va_list *pargptr)
*
*Purpose:
*   Gets a short argument off the given argument list and updates *pargptr.
*   *** CURRENTLY ONLY USED TO GET A WCHAR_T, IFDEF _INTL ***
*
*Entry:
*   va_list *pargptr - pointer to argument list; updated by function
*
*Exit:
*   Returns the short argument read from the argument list.
*
*Exceptions:
*
*******************************************************************************/

#if     !SHORT_IS_INT
__inline short __cdecl get_short_arg (
    va_list *pargptr
    )
{
    return va_arg(*pargptr, short);
}
#endif

#endif

#ifdef __cplusplus
}
#endif


#ifdef CRT_UNICODE

static UINT_PTR __enable_percent_n = 0;

/***
 *int _set_printf_count_output(int)
 *
 *Purpose:
 *   Enables or disables %n format specifier for printf family functions
 *
 *Internals:
 *   __enable_percent_n is set to (__security_cookie|1) for security reasons;
 *   if set to a static value, an attacker could first modify __enable_percent_n
 *   and then provide a malicious %n specifier.  The cookie is ORed with 1
 *   because a zero cookie is a possibility.
 ******************************************************************************/
int __cdecl _set_printf_count_output(int value)
{
    int old = (__enable_percent_n == (__security_cookie | 1));
    __enable_percent_n = (value ? (__security_cookie | 1) : 0);
    return old;
}

/***
 *int _get_printf_count_output()
 *
 *Purpose:
 *   Checks whether %n format specifier for printf family functions is enabled
 ******************************************************************************/
int __cdecl _get_printf_count_output()
{
    return g_fCrtLegacyInputValidation || ( __enable_percent_n == (__security_cookie | 1));
}

#endif // CRT_UNICODE

