//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*input.c - C formatted input, used by scanf, etc.
*
*
*Purpose:
*       defines _input() to do formatted input; called from scanf(),
*       etc. functions.  This module defines _cscanf() instead when
*       CPRFLAG is defined.  The file cscanf.c defines that symbol
*       and then includes this file in order to implement _cscanf().
*
*******************************************************************************/

#define ALLOW_RANGE /* allow "%[a-z]"-style scansets */


/* temporary work-around for compiler without 64-bit support */

#ifndef _INTEGRAL_MAX_BITS
#define _INTEGRAL_MAX_BITS  64
#endif


#include <cruntime.h>
#include <crtmisc.h>
#include <stdio.h>
#include <crtctype.h>
#include <stdarg.h>
#include <string.h>
#include <internal.h>
#include <fltintrn.h>
#include <mtdll.h>
#include <stdlib.h>
#include <dbgint.h>

#ifdef _MBCS    /* always want either Unicode or SBCS for tchar.h */
#undef _MBCS
#endif
#include <crttchar.h>

#ifdef CRT_UNICODE
#define _eof_char WEOF
#else
#define _eof_char EOF
#endif

#undef iswspace
__inline static int iswspace(wchar_t i) {
        if (((i)==0x20) || ((i)>=0x09 && (i)<=0x0D))
                return 1;
        return 0;
}

#undef isspace
__inline static int isspace(char i)
{
        if(((i)==0x20) || ((i)>=0x09 && (i)<=0x0D))
                return 1;
        else
                return 0;
}

#define HEXTODEC(chr)   _hextodec(chr)
#define LEFT_BRACKET    ('[' | ('a' - 'A')) /* 'lowercase' version */

#ifdef CRT_UNICODE
static wchar_t __cdecl _hextodec(wchar_t);
#else
static int __cdecl _hextodec(int);
#endif


/*
 * Note: CPRFLAG and CRT_UNICODE cases are currently mutually exclusive.
 */

#ifdef CPRFLAG

#define INC()           (++charcount, _inc())
#define UN_INC(chr)     (--charcount, _ungetch_lk(chr))
#define EAT_WHITE()     _whiteout(&charcount)

static int __cdecl _inc(void);
static int __cdecl _whiteout(int *);

#else

#define INC()           (++charcount, _inc(stream))
#define UN_INC(chr)     (--charcount, _un_inc(chr, stream))
#define EAT_WHITE()     _whiteout(&charcount, stream)

#ifndef CRT_UNICODE
static int __cdecl _inc(FILEX *);
static void __cdecl _un_inc(int, FILEX *);
static int __cdecl _whiteout(int *, FILEX *);
#else
static wchar_t __cdecl _inc(FILEX *);
static void __cdecl _un_inc(wchar_t, FILEX *);
static wchar_t __cdecl _whiteout(int *, FILEX *);
#endif /* CRT_UNICODE */

#endif /* CPRFLAG */


#ifndef CRT_UNICODE
#define _ISDIGIT(chr)   isdigit(chr)
#define _ISXDIGIT(chr)  isxdigit(chr)
#else
#define _ISDIGIT(chr)   ( !(chr & 0xff00) && isdigit( chr & 0x00ff ) )
#define _ISXDIGIT(chr)  ( !(chr & 0xff00) && isxdigit( chr & 0x00ff ) )
#endif


#ifdef CRT_UNICODE
int __cdecl _winput(FILEX *, const wchar_t *, va_list);
#endif

#ifdef CPRFLAG
static int __cdecl input(const unsigned char *, va_list);


/***
*int _cscanf(format, arglist) - read formatted input direct from console
*
*Purpose:
*   Reads formatted data like scanf, but uses console I/O functions.
*
*Entry:
*   char *format - format string to determine data formats
*   arglist - list of POINTERS to where to put data
*
*Exit:
*   returns number of successfully matched data items (from input)
*
*Exceptions:
*
*******************************************************************************/


int __cdecl _cscanf (
    const char *format,
    ...
    )
{
    va_list arglist;

    va_start(arglist, format);

    _ASSERTE(format != NULL);

    return input(format,arglist);   /* get the input */
}

#endif  /* CPRFLAG */


#define ASCII       32           /* # of bytes needed to hold 256 bits */

#define SCAN_SHORT     0         /* also for FLOAT */
#define SCAN_LONG      1         /* also for DOUBLE */
#define SCAN_L_DOUBLE  2         /* only for LONG DOUBLE */

#define SCAN_NEAR    0
#define SCAN_FAR     1

#ifdef ALLOW_RANGE
        static const CRT__TCHAR sbrackset[] = CRT_T(" \t-\r]"); /* use range-style list */ 
#else
        static const CRT__TCHAR sbrackset[] = CRT_T(" \t\n\v\f\r]"); /* chars defined by isspace() */ 
#endif

static const CRT__TCHAR cbrackset[] = CRT_T("]"); 


/***
*int _input(stream, format, arglist), static int input(format, arglist)
*
*Purpose:
*   get input items (data items or literal matches) from the input stream
*   and assign them if appropriate to the items thru the arglist. this
*   function is intended for internal library use only, not for the user
*
*   The _input entry point is for the normal scanf() functions
*   The input entry point is used when compiling for _cscanf() [CPRFLAF
*   defined] and is a static function called only by _cscanf() -- reads from
*   console.
*
*Entry:
*   FILEX *stream - file to read from
*   char *format - format string to determine the data to read
*   arglist - list of pointer to data items
*
*Exit:
*   returns number of items assigned and fills in data items
*   returns EOF if error or EOF found on stream before 1st data item matched
*
*Exceptions:
*
*******************************************************************************/

#ifdef CPRFLAG

static int __cdecl input (
    const unsigned char *format,
    va_list arglist
    )
#elif defined(CRT_UNICODE)

int __cdecl _winput (
    FILEX *stream,
    const wchar_t *format,
    va_list arglist
    )
#else

int __cdecl _input (
    FILEX *stream,
    const unsigned char *format,
    va_list arglist
    )
#endif

{
#ifndef CRT_UNICODE
    char table[ASCII];                  /* which chars allowed for %[], %s   */
    char floatstring[CVTBUFSIZE + 1];   /* ASCII buffer for floats           */
#else
    char table[256*ASCII];
    wchar_t floatstring[CVTBUFSIZE + 1];
#endif

    unsigned long number;               /* temp hold-value                   */
#if _INTEGRAL_MAX_BITS >= 64
    unsigned __int64 num64;             /* temp for 64-bit integers          */
#endif
    void *pointer;                      /* points to user data receptacle    */
    void *start;                        /* indicate non-empty string         */


#ifdef CRT_UNICODE
    wchar_t *scanptr;           /* for building "table" data         */
REG2 wchar_t ch = 0;
#else
    wchar_t wctemp;
    unsigned char *scanptr;             /* for building "table" data         */
REG2 int ch = 0;
#endif
    int charcount;                      /* total number of chars read        */
REG1 int comchr;                        /* holds designator type             */
    int count;                          /* return value.  # of assignments   */

    int started;                        /* indicate good number              */
    int width;                          /* width of field                    */
    int widthset;                       /* user has specified width          */

/* Neither coerceshort nor farone are need for the 386 */


    char done_flag;                     /* general purpose loop monitor      */
    char longone;                       /* 0 = SHORT, 1 = LONG, 2 = L_DOUBLE */
#if _INTEGRAL_MAX_BITS >= 64
    int integer64;                      /* 1 for 64-bit integer, 0 otherwise */
#endif
    signed char widechar;               /* -1 = char, 0 = ????, 1 = wchar_t  */
    char reject;                        /* %[^ABC] instead of %[ABC]         */
    char negative;                      /* flag for '-' detected             */
    char suppress;                      /* don't assign anything             */
    char match;                         /* flag: !0 if any fields matched    */
    va_list arglistsave;                /* save arglist value                */

    char fl_wchar_arg;                  /* flags wide char/string argument   */
#ifdef CRT_UNICODE
#ifdef ALLOW_RANGE
    wchar_t rngch;              /* used while scanning range         */
#endif
    wchar_t last;               /* also for %[a-z]                   */
    wchar_t prevchar;           /* for %[a-z]                        */
    wchar_t *wptr;                      /* pointer traverses wide floatstring*/
#else
#ifdef ALLOW_RANGE
    unsigned char rngch;                /* used while scanning range         */
#endif
    unsigned char last;                 /* also for %[a-z]                   */
    unsigned char prevchar;             /* for %[a-z]                        */
#endif

    _ASSERTE(format != NULL);

#ifndef CPRFLAG
    _ASSERTE(stream != NULL);
#endif

    /*
    count = # fields assigned
    charcount = # chars read
    match = flag indicating if any fields were matched

    [Note that we need both count and match.  For example, a field
    may match a format but have assignments suppressed.  In this case,
    match will get set, but 'count' will still equal 0.  We need to
    distinguish 'match vs no-match' when terminating due to EOF.]
    */

    count = charcount = match = 0;

    while (*format) {

        if (_istspace((CRT__TUCHAR)*format)) {

            UN_INC(EAT_WHITE()); /* put first non-space char back */

            while ((_istspace)(*++format)); /* NULL */
            /* careful: isspace macro may evaluate argument more than once! */

        }

        if (CRT_T('%') == *format) {

            number = 0;
            prevchar = 0;
            width = widthset = started = 0;
            fl_wchar_arg = done_flag = suppress = negative = reject = 0;
            widechar = 0;

            longone = 1;

            integer64 = 0;

            while (!done_flag) {

                comchr = *++format;
                if (_ISDIGIT((CRT__TUCHAR)comchr)) {
                    ++widthset;
                    width = MUL10(width) + (comchr - CRT_T('0'));
                } else
                    switch (comchr) {
                        case CRT_T('F') :
                        case CRT_T('N') :   /* no way to push NEAR in large model */
                            break;  /* NEAR is default in small model */
                        case CRT_T('h') :
                            /* set longone to 0 */
                            --longone;
                            --widechar;         /* set widechar = -1 */
                            break;

#if _INTEGRAL_MAX_BITS >= 64
                        case CRT_T('I'):
                            if ( (*(format + 1) == CRT_T('6')) &&
                                 (*(format + 2) == CRT_T('4')) )
                            {
                                format += 2;
                                ++integer64;
                                num64 = 0;
                                break;
                            }
                            goto DEFAULT_LABEL;
#endif

                        case CRT_T('L') :
                        /*  ++longone;  */
                            ++longone;
                            break;

                        case CRT_T('l') :
                            ++longone;
                                    /* NOBREAK */
                        case CRT_T('w') :
                            ++widechar;         /* set widechar = 1 */
                            break;

                        case CRT_T('*') :
                            ++suppress;
                            break;

                        default:
DEFAULT_LABEL:
                            ++done_flag;
                            break;
                    }
            }

            if (!suppress) {
                arglistsave = arglist;
                pointer = va_arg(arglist,void *);
            }

            done_flag = 0;

            if (!widechar) {    /* use case if not explicitly specified */
                if ((*format == CRT_T('S')) || (*format == CRT_T('C')))
#ifdef CRT_UNICODE
                    --widechar;
                else
                    ++widechar;
#else
                    ++widechar;
                else
                    --widechar;
#endif
            }

            /* switch to lowercase to allow %E,%G, and to
               keep the switch table small */

            comchr = *format | (CRT_T('a') - CRT_T('A'));

            if (CRT_T('n') != comchr)
                if (CRT_T('c') != comchr && LEFT_BRACKET != comchr)
                    ch = EAT_WHITE();
                else
                    ch = INC();

            if (!widthset || width) {

                switch(comchr) {

                    case CRT_T('c'):
                /*  case CRT_T('C'):  */
                        if (!widthset) {
                            ++widthset;
                            ++width;
                        }
                        if (widechar>0)
                            fl_wchar_arg++;
                        scanptr = (CRT_TCHAR*)cbrackset;
                        --reject; /* set reject to 255 */
                        goto scanit2;

                    case CRT_T('s'):
                /*  case CRT_T('S'):  */
                        if (widechar>0)
                            fl_wchar_arg++;
                        scanptr = (CRT_TCHAR*)sbrackset;
                        --reject; /* set reject to 255 */
                        goto scanit2;

                    case LEFT_BRACKET :   /* scanset */
                        if (widechar>0)
                            fl_wchar_arg++;
                        scanptr = (CRT__TCHAR *)(++format);

                        if (CRT_T('^') == *scanptr) {
                            ++scanptr;
                            --reject; /* set reject to 255 */
                        }


scanit2:
#ifdef CRT_UNICODE
                        memset(table, 0, ASCII*256);
#else
                        memset(table, 0, ASCII);
#endif

#ifdef ALLOW_RANGE

                        if (LEFT_BRACKET == comchr)
                            if (CRT_T(']') == *scanptr) {
                                prevchar = CRT_T(']');
                                ++scanptr;

                                table[ CRT_T(']') >> 3] = 1 << (CRT_T(']') & 7);

                            }

                        while (CRT_T(']') != *scanptr) {

                            rngch = *scanptr++;

                            if (CRT_T('-') != rngch ||
                                 !prevchar ||           /* first char */
                                 CRT_T(']') == *scanptr) /* last char */

                                table[(prevchar = rngch) >> 3] |= 1 << (rngch & 7);

                            else {  /* handle a-z type set */

                                rngch = *scanptr++; /* get end of range */

                                if (prevchar < rngch)  /* %[a-z] */
                                    last = rngch;
                                else {              /* %[z-a] */
                                    last = prevchar;
                                    prevchar = rngch;
                                }
                                for (rngch = prevchar; rngch <= last; ++rngch)
                                    table[rngch >> 3] |= 1 << (rngch & 7);

                                prevchar = 0;

                            }
                        }


#else
                        if (LEFT_BRACKET == comchr)
                            if (CRT_T(']') == *scanptr) {
                                ++scanptr;
                                table[(prevchar = CRT_T(']')) >> 3] |= 1 << (CRT_T(']') & 7);
                            }

                        while (CRT_T(']') != *scanptr) {
                            table[scanptr >> 3] |= 1 << (scanptr & 7);
                            ++scanptr;
                        }
                        /* code under !ALLOW_RANGE is probably never compiled */
                        /* and has probably never been tested */
#endif
                        if (!*scanptr)
                            goto error_return;      /* trunc'd format string */

                        /* scanset completed.  Now read string */

                        if (LEFT_BRACKET == comchr)
                            format = scanptr;

                        start = pointer;

                        /*
                         * execute the format directive. that is, scan input
                         * characters until the directive is fulfilled, eof
                         * is reached, or a non-matching character is
                         * encountered.
                         *
                         * it is important not to get the next character
                         * unless that character needs to be tested! other-
                         * wise, reads from line-buffered devices (e.g.,
                         * scanf()) would require an extra, spurious, newline
                         * if the first newline completes the current format
                         * directive.
                         */
                        UN_INC(ch);

                        while ( !widthset || width-- ) {

                            ch = INC();
                            if (
#ifndef CRT_UNICODE
#ifndef CPRFLAG
                              (EOF != ch) &&
#endif
                              ((table[ch >> 3] ^ reject) & (1 << (ch & 7)))
#else
                              (WEOF != ch) &&
                         /*     ((ch>>3 >= ASCII) ? reject : */
                              ((table[ch >> 3] ^ reject) & 
                                        (1 << (ch & 7)))         /* ) */
#endif /* CRT_UNICODE */
                            ) {
                                if (!suppress) {
#ifndef CRT_UNICODE
                                    if (fl_wchar_arg) {
                                        char temp[2];
                                        int mbsize = 1;
                                        temp[0] = (char) ch;
                                        if (isleadbyte((BYTE)ch))
                                        {
                                            temp[1] = (char) INC();
                                            mbsize++;
                                        }
                                        change_to_widechar(&wctemp, temp, mbsize);
                                        *(wchar_t UNALIGNED *)pointer = wctemp;
                                        /* do nothing if mbtowc fails */
                                        pointer = (wchar_t *)pointer + 1;
                                    } else
#else
                                    if (fl_wchar_arg) {
                                        *(wchar_t UNALIGNED *)pointer = ch;
                                        pointer = (wchar_t *)pointer + 1;
                                    } else
#endif
                                    {
#ifndef CRT_UNICODE
                                    *(char *)pointer = (char)ch;
                                    pointer = (char *)pointer + 1;
#else
                                    int temp;
                                    /* convert wide to multibyte */
                    temp = change_to_multibyte((char *)pointer, MB_LEN_MAX, ch);
                                    /* do nothing if wctomb fails */
                                    pointer = (char *)pointer + temp;
#endif
                                    }
                                } /* suppress */
                                else {
                                    /* just indicate a match */
                                    start = (CRT__TCHAR *)start + 1;
                                }
                            }
                            else  {
                                UN_INC(ch);
                                break;
                            }
                        }

                        /* make sure something has been matched and, if
                           assignment is not suppressed, null-terminate
                           output string if comchr != c */

                        if (start != pointer) {
                            if (!suppress) {
                                ++count;
                                if ('c' != comchr) /* null-terminate strings */
                                    if (fl_wchar_arg)
                                        *(wchar_t UNALIGNED *)pointer = L'\0';
                                    else
                                    *(char *)pointer = '\0';
                            } else /*NULL*/;
                        }
                        else
                            goto error_return;

                        break;

                    case CRT_T('i') :   /* could be d, o, or x */

                        comchr = CRT_T('d'); /* use as default */

                    case CRT_T('x'):

                        if (CRT_T('-') == ch) {
                            ++negative;

                            goto x_incwidth;

                        } else if (CRT_T('+') == ch) {
x_incwidth:
                            if (!--width && widthset)
                                ++done_flag;
                            else
                                ch = INC();
                        }

                        if (CRT_T('0') == ch) {

                            if (CRT_T('x') == (CRT__TCHAR)(ch = INC()) || CRT_T('X') == (CRT__TCHAR)ch) {
                                ch = INC();
                                comchr = CRT_T('x');
                            } else {
                                ++started;
                                if (CRT_T('x') != comchr)
                                    comchr = CRT_T('o');
                                else {
                                    /* scanning a hex number that starts */
                                    /* with a 0. push back the character */
                                    /* currently in ch and restore the 0 */
                                    UN_INC(ch);
                                    ch = CRT_T('0');
                                }
                            }
                        }
                        goto getnum;

                        /* NOTREACHED */

                    case CRT_T('p') :
                        /* force %hp to be treated as %p */
                        longone = 1;

                    case CRT_T('o') :
                    case CRT_T('u') :
                    case CRT_T('d') :

                        if (CRT_T('-') == ch) {
                            ++negative;

                            goto d_incwidth;

                        } else if (CRT_T('+') == ch) {
d_incwidth:
                            if (!--width && widthset)
                                ++done_flag;
                            else
                                ch = INC();
                        }

getnum:
#if _INTEGRAL_MAX_BITS >= 64
                        if ( integer64 ) {

                            while (!done_flag) {

                                if (CRT_T('x') == comchr)

                                    if (_ISXDIGIT(ch)) {
                                        num64 <<= 4;
                                        ch = HEXTODEC(ch);
                                    }
                                    else
                                        ++done_flag;

                                else if (_ISDIGIT(ch))

                                    if (CRT_T('o') == comchr)
                                        if (CRT_T('8') > ch)
                                                num64 <<= 3;
                                        else {
                                                ++done_flag;
                                        }
                                    else /* CRT_T('d') == comchr */
                                        num64 = MUL10(num64);

                                else
                                    ++done_flag;

                                if (!done_flag) {
                                    ++started;
                                    num64 += ch - CRT_T('0');

                                    if (widthset && !--width)
                                        ++done_flag;
                                    else
                                        ch = INC();
                                } else
                                    UN_INC(ch);

                            } /* end of WHILE loop */

                            if (negative)
                                num64 = (unsigned __int64 )(-(__int64)num64);
                        }
                        else {
#endif
                            while (!done_flag) {

                                if (CRT_T('x') == comchr || CRT_T('p') == comchr)

                                    if (_ISXDIGIT(ch)) {
                                        number = (number << 4);
                                        ch = HEXTODEC(ch);
                                    }
                                    else
                                        ++done_flag;

                                else if (_ISDIGIT(ch))

                                    if (CRT_T('o') == comchr)
                                        if (CRT_T('8') > ch)
                                            number = (number << 3);
                                        else {
                                            ++done_flag;
                                        }
                                    else /* CRT_T('d') == comchr */
                                        number = MUL10(number);

                                else
                                    ++done_flag;

                                if (!done_flag) {
                                    ++started;
                                    number += ch - CRT_T('0');

                                    if (widthset && !--width)
                                        ++done_flag;
                                    else
                                        ch = INC();
                                } else
                                    UN_INC(ch);

                            } /* end of WHILE loop */

                            if (negative)
                                number = (unsigned long)(-(long)number);
#if _INTEGRAL_MAX_BITS >= 64
                        }
#endif
                        if (CRT_T('F')==comchr) /* expected ':' in long pointer */
                            started = 0;

                        if (started)
                            if (!suppress) {

                                ++count;
assign_num:
#if _INTEGRAL_MAX_BITS >= 64
                                if ( integer64 )
                                    *(__int64 UNALIGNED *)pointer = (unsigned __int64)num64;
                                else
#endif
                                if (longone)
                                    *(long UNALIGNED *)pointer = (unsigned long)number;
                                else
                                    *(short UNALIGNED *)pointer = (unsigned short)number;

                            } else /*NULL*/;
                        else
                            goto error_return;

                        break;

                    case CRT_T('n') :   /* char count, don't inc return value */
                        number = charcount;
                        if(!suppress)
                            goto assign_num; /* found in number code above */
                        break;


                    case CRT_T('e') :
                 /* case CRT_T('E') : */
                    case CRT_T('f') :
                    case CRT_T('g') : /* scan a float */
                 /* case CRT_T('G') : */

#ifndef CRT_UNICODE
                        scanptr = floatstring;

                        if (CRT_T('-') == ch) {
                            *scanptr++ = CRT_T('-');
                            goto f_incwidth;

                        } else if (CRT_T('+') == ch) {
f_incwidth:
                            --width;
                            ch = INC();
                        }

                        if (!widthset || width > CVTBUFSIZE)              /* must watch width */
                            width = CVTBUFSIZE;


                        /* now get integral part */

                        while (_ISDIGIT(ch) && width--) {
                            ++started;
                            *scanptr++ = (char)ch;
                            ch = INC();
                        }

                        /* now check for decimal */

                        if ('.' == (char)ch && width--) {
                            ch = INC();
                            *scanptr++ = '.';

                            while (_ISDIGIT(ch) && width--) {
                                ++started;
                                *scanptr++ = (char)ch;
                                ch = INC();
                            }
                        }

                        /* now check for exponent */

                        if (started && (CRT_T('e') == ch || CRT_T('E') == ch) && width--) {
                            *scanptr++ = 'e';

                            if (CRT_T('-') == (ch = INC())) {

                                *scanptr++ = '-';
                                goto f_incwidth2;

                            } else if (CRT_T('+') == ch) {
f_incwidth2:
                                if (!width--)
                                    ++width;
                                else
                                    ch = INC();
                            }


                            while (_ISDIGIT(ch) && width--) {
                                ++started;
                                *scanptr++ = (char)ch;
                                ch = INC();
                            }

                        }

                        UN_INC(ch);

                        if (started)
                            if (!suppress) {
                                ++count;
                                *scanptr = '\0';
                                _fassign( longone-1, pointer , floatstring);
                            } else /*NULL */;
                        else
                            goto error_return;

#else /* CRT_UNICODE */
                        wptr = floatstring;

                        if (L'-' == ch) {
                            *wptr++ = L'-';
                            goto f_incwidthw;

                        } else if (L'+' == ch) {
f_incwidthw:
                            --width;
                            ch = INC();
                        }

                        if (!widthset || width > CVTBUFSIZE)
                            width = CVTBUFSIZE;


                        /* now get integral part */

                        while (_ISDIGIT(ch) && width--) {
                            ++started;
                            *wptr++ = ch;
                            ch = INC();
                        }

                        /* now check for decimal */

                        if ('.' == ch && width--) {
                            ch = INC();
                            *wptr++ = '.';

                            while (_ISDIGIT(ch) && width--) {
                                ++started;
                                *wptr++ = ch;
                                ch = INC();
                            }
                        }

                        /* now check for exponent */

                        if (started && (L'e' == ch || L'E' == ch) && width--) {
                            *wptr++ = L'e';

                            if (L'-' == (ch = INC())) {

                                *wptr++ = L'-';
                                goto f_incwidth2w;

                            } else if (L'+' == ch) {
f_incwidth2w:
                                if (!width--)
                                    ++width;
                                else
                                    ch = INC();
                            }


                            while (_ISDIGIT(ch) && width--) {
                                ++started;
                                *wptr++ = ch;
                                ch = INC();
                            }

                        }

                        UN_INC(ch);

                        if (started)
                            if (!suppress) {
                                ++count;
                                *wptr = '\0';
                                {
                                /* convert floatstring to char string */
                                /* and do the conversion */
                                size_t cfslength;
                                char *cfloatstring;
                                cfslength =(wptr-floatstring+1)*sizeof(wchar_t);
                                cfloatstring = (char *)_malloc_crt (cfslength);
                                wcstombs (cfloatstring, floatstring, cfslength);
                                _fassign( longone-1, pointer , cfloatstring);
                                _free_crt (cfloatstring);
                                }
                            } else /*NULL */;
                        else
                            goto error_return;

#endif /* CRT_UNICODE */
                        break;


                    default:    /* either found '%' or something else */

                        if ((int)*format != (int)ch) {
                            UN_INC(ch);
                            goto error_return;
                            }
                        else
                            match--; /* % found, compensate for inc below */

                        if (!suppress)
                            arglist = arglistsave;

                } /* SWITCH */

                match++;        /* matched a format field - set flag */

            } /* WHILE (width) */

            else {  /* zero-width field in format string */
                UN_INC(ch);  /* check for input error */
                goto error_return;
            }

            ++format;  /* skip to next char */

        } else  /*  ('%' != *format) */
            {

            if ((int)*format++ != (int)(ch = INC()))
                {
                UN_INC(ch);
                goto error_return;
                }
#ifndef CRT_UNICODE
            if (isleadbyte((BYTE)ch))
                {
                int ch2;
                if ((int)*format++ != (ch2=INC()))
                    {
                    UN_INC(ch2);
                    UN_INC(ch);
                    goto error_return;
                    }

                    --charcount; /* only count as one character read */
                }
#endif
            }

#ifndef CPRFLAG
        if ( (_eof_char == ch) && ((*format != '%') || (*(format + 1) != 'n')) )
            break;
#endif

    }  /* WHILE (*format) */

error_return:

#ifndef CPRFLAG
    if (_eof_char == ch)
        /* If any fields were matched or assigned, return count */

    /* Phil Lucido (MS) email:  "MSDN is wrong on fwscanf, and the 
     * new docs in VS7 have it correctly - the return value for errors 
     * is supposed to be EOF, not WEOF.  (7/11/01)
     */
        return ( (count || match) ? count : EOF);
    else
#endif
        return count;

}

/* _hextodec() returns a value of 0-15 and expects a char 0-9, a-f, A-F */
/* _inc() is the one place where we put the actual getc code. */
/* _whiteout() returns the first non-blank character, as defined by isspace() */

#ifndef CRT_UNICODE
static int __cdecl _hextodec (
    int chr
    )
{
    return _ISDIGIT(chr) ? chr : (chr & ~(CRT_T('a') - CRT_T('A'))) - CRT_T('A') + 10 + CRT_T('0');
}
#else
static CRT__TCHAR __cdecl _hextodec (
    CRT__TCHAR chr
    )
{
    if (_ISDIGIT(chr))
        return chr;
    if (_istlower(chr))
        return (CRT__TCHAR)(chr - CRT_T('a') + 10 + CRT_T('0'));
    else
        return (CRT__TCHAR)(chr - CRT_T('A') + 10 + CRT_T('0'));
}
#endif


#ifdef CPRFLAG

static int __cdecl _inc (
    void
    )
{
    return(_getche_lk());
}

static int __cdecl _whiteout (
    REG1 int *counter
    )
{
    REG2 int ch;

    while((_istspace)(ch = (++*counter, _inc())));
    return ch;
}

#elif defined(CRT_UNICODE)

/*
 * Manipulate wide-chars in a file.
 * A wide-char is hard-coded to be two chars for efficiency.
 */

static wchar_t __cdecl _inc (
    REG1 FILEX *fileptr
    )
{
    return(_getwc_lk(fileptr));
}

static void __cdecl _un_inc (
    wchar_t chr,
    FILEX *fileptr
    )
{
    if (WEOF != chr)
        _ungetwc_lk(chr, fileptr);
}

static wchar_t __cdecl _whiteout (
    REG1 int *counter,
    REG3 FILEX *fileptr
    )
{
    REG2 wchar_t ch;

    while((iswspace)(ch = (++*counter, _inc(fileptr))));
    return ch;
}

#else

static int __cdecl _inc (
    REG1 FILEX *fileptr
    )
{
    return(_getc_lk(fileptr));
}

static void __cdecl _un_inc (
    int chr,
    FILEX *fileptr
    )
{
    if (EOF != chr)
        _ungetc_lk(chr, fileptr);
}

static int __cdecl _whiteout (
    REG1 int *counter,
    REG3 FILEX *fileptr
    )
{
    REG2 int ch;

    while((isspace)((char)(ch = (++*counter, _inc(fileptr)))));
    return ch;
}

#endif

