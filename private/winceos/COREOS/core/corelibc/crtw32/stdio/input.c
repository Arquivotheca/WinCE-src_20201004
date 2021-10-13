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
*input.c - C formatted input, used by scanf, etc.
*
*Purpose:
*       defines _input() to do formatted input; called from scanf(),
*       etc. functions.
*
*******************************************************************************/


#define ALLOW_RANGE /* allow "%[a-z]"-style scansets */


/* temporary work-around for compiler without 64-bit support */

#ifndef _INTEGRAL_MAX_BITS
#define _INTEGRAL_MAX_BITS  64
#endif

#include <corecrt.h>
#include <cruntime.h>
#include <crttchar.h>
#include <crtmisc.h>
#include <stdio.h>
#include <crtctype.h>
#include <stdarg.h>
#include <string.h>
#include <internal.h>
#include <fltintrn.h>
#include <malloc.h>
#include <stdlib.h>
#include <dbgint.h>
#include <internal_securecrt.h>

#ifdef _MBCS    /* always want either Unicode or SBCS for tchar.h */
#undef _MBCS
#endif
#include <tchar.h>

#pragma warning(disable: 4127) // conditional expression is constant

#ifdef __cplusplus
extern "C"
{
#endif

#define _FASSIGN(flag, argument, number, dec_point) _fassign((flag), (argument), (number))

#if  defined (CRT_UNICODE)
#define ALLOC_TABLE 1
#else
#define ALLOC_TABLE 0
#endif

#define HEXTODEC(chr)   _hextodec(chr)

#define LEFT_BRACKET    ('[' | ('a' - 'A')) /* 'lowercase' version */

static CRT__TINT __cdecl _hextodec(CRT__TCHAR);

#define INC()           (++charcount, _inc(stream))
#define UN_INC(chr)     (--charcount, _un_inc(chr, stream))
#define EAT_WHITE()     _whiteout(&charcount, stream)

static CRT__TINT __cdecl _inc(FILEX *);
static void __cdecl _un_inc(CRT__TINT, FILEX *);
static CRT__TINT __cdecl _whiteout(int *, FILEX *);

#ifndef CRT_UNICODE
#define _ISDIGIT(chr)   isdigit((unsigned char)chr)
#define _ISXDIGIT(chr)  isxdigit((unsigned char)chr)
#else
#define _ISDIGIT(chr)   ( !(chr & 0xff00) && isdigit( ((chr) & 0x00ff) ) )
#define _ISXDIGIT(chr)  ( !(chr & 0xff00) && isxdigit( ((chr) & 0x00ff) ) )
#endif


#define LONGLONG_IS_INT64 1     /* 1 means long long is same as int64
                                   0 means long long is same as long */

/***
*  int __check_float_string(size_t,size_t *, CRT__TCHAR**, CRT__TCHAR*, int*)
*
*  Purpose:
*       Check if there is enough space insert onemore character in the given
*       block, if not then allocate more memory.
*
*  Return:
*       FALSE if more memory needed and the reallocation failed.
*
*******************************************************************************/

static int __check_float_string(size_t nFloatStrUsed,
                                size_t *pnFloatStrSz,
                                CRT__TCHAR **pFloatStr,
                                CRT__TCHAR *floatstring,
                                int *pmalloc_FloatStrFlag)
{
    void *tmpPointer;
    _ASSERTE(nFloatStrUsed<=(*pnFloatStrSz));
    if (nFloatStrUsed==(*pnFloatStrSz))
    {
        if (*pnFloatStrSz > INT_MAX)
        {
            return FALSE;
        }

        if ((*pFloatStr)==floatstring)
        {
            if (((*pFloatStr)=(CRT__TCHAR *)_calloc_crt((*pnFloatStrSz),2*sizeof(CRT__TCHAR)))==NULL)
            {
              return FALSE;
            }
            (*pmalloc_FloatStrFlag)=1;
            memcpy((*pFloatStr),floatstring,(*pnFloatStrSz)*sizeof(CRT__TCHAR));
            (*pnFloatStrSz)*=2;
        }
        else
        {
            if ((tmpPointer=(CRT__TCHAR *)_recalloc_crt((*pFloatStr), (*pnFloatStrSz),2*sizeof(CRT__TCHAR)))==NULL)
            {
                return FALSE;
            }
            (*pFloatStr)=(CRT__TCHAR *)(tmpPointer);
            (*pnFloatStrSz)*=2;
        }
    }
    return TRUE;
}


#define ASCII       32           /* # of bytes needed to hold 256 bits */

#define SCAN_SHORT     0         /* also for FLOAT */
#define SCAN_LONG      1         /* also for DOUBLE */
#define SCAN_L_DOUBLE  2         /* only for LONG DOUBLE */

#define SCAN_NEAR    0
#define SCAN_FAR     1

#ifndef CRT_UNICODE
#define TABLESIZE    ASCII
#else
#define TABLESIZE    (ASCII * 256)
#endif


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
*   This code also defines _input_s, which works differently for %c, %s & %[.
*   For these, _input_s first picks up the next argument from the variable
*   argument list & uses it as the maximum size of the character array pointed
*   to by the next argument in the list.
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

int
__cdecl
_tinput (
    FILEX* stream,
    const CRT__TUCHAR* format,
    va_list arglist,
    BOOL fSecureScanf
    )
{
    CRT__TCHAR floatstring[_CVTBUFSIZE + 1];
    CRT__TCHAR *pFloatStr=floatstring;
    size_t nFloatStrUsed=0;
    size_t nFloatStrSz=sizeof(floatstring)/sizeof(floatstring[0]);
    int malloc_FloatStrFlag=0;

    unsigned long number;               /* temp hold-value                   */
#if ALLOC_TABLE
    char *table = NULL;                 /* which chars allowed for %[]       */
    int malloc_flag = 0;                /* is "table" allocated on the heap? */
#else
    char AsciiTable[TABLESIZE];
    char *table = AsciiTable;
#endif

#if _INTEGRAL_MAX_BITS >= 64
    unsigned __int64 num64 = 0;         /* temp for 64-bit integers          */
#endif
    void *pointer=NULL;                 /* points to user data receptacle    */
    void *start;                        /* indicate non-empty string         */


#ifndef CRT_UNICODE
    wchar_t wctemp=L'\0';
#endif
    CRT__TUCHAR *scanptr;                   /* for building "table" data         */
REG2 CRT__TINT ch = 0;
    int charcount;                      /* total number of chars read        */
REG1 int comchr;                        /* holds designator type             */
    int count;                          /* return value.  # of assignments   */

    int started;                        /* indicate good number              */
    int width;                          /* width of field                    */
    int widthset;                       /* user has specified width          */

    size_t array_width = 0;
    size_t original_array_width = 0;
    int enomem = 0;
    int format_error = FALSE;

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
    va_list arglistsave = NULL;         /* save arglist value                */

    char fl_wchar_arg;                  /* flags wide char/string argument   */

    CRT__TCHAR decimal;


#ifdef ALLOW_RANGE
    CRT__TUCHAR rngch;
#endif
    CRT__TUCHAR last;
    CRT__TUCHAR prevchar;
    CRT__TCHAR tch;

    _VALIDATE_RETURN( (format != NULL), EINVAL, EOF);

    _VALIDATE_RETURN( (stream != NULL), EINVAL, EOF);
#ifndef CRT_UNICODE
//  Don't support textmode, so we can't validate this
//    _VALIDATE_STREAM_ANSI_RETURN(stream, EINVAL, EOF);
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

            do {
                tch = *++format;
            } while (_istspace((CRT__TUCHAR)tch));

            continue;

        }

        if (CRT_T('%') == *format) {

            number = 0;
            prevchar = 0;
            width = widthset = started = 0;
            if (fSecureScanf) {
                original_array_width = array_width = 0;
                enomem = 0;
            }
            fl_wchar_arg = done_flag = suppress = negative = reject = 0;
            widechar = 0;

            longone = 1;

#if _INTEGRAL_MAX_BITS >= 64
            integer64 = 0;
#endif

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
                            else if ( (*(format + 1) == CRT_T('3')) &&
                                      (*(format + 2) == CRT_T('2')) )
                            {
                                format += 2;
                                break;
                            }
                            else if ( (*(format + 1) == CRT_T('d')) ||
                                      (*(format + 1) == CRT_T('i')) ||
                                      (*(format + 1) == CRT_T('o')) ||
                                      (*(format + 1) == CRT_T('x')) ||
                                      (*(format + 1) == CRT_T('X')) )
                            {
                                if (sizeof(void*) == sizeof(__int64))
                                {
                                    ++integer64;
                                    num64 = 0;
                                }
                                break;
                            }
                            if (sizeof(void*) == sizeof(__int64))
                            {
                                    ++integer64;
                                    num64 = 0;
                            }
                            goto DEFAULT_LABEL;
#endif

                        case CRT_T('L') :
                        /*  ++longone;  */
                            ++longone;
                            break;

                        case CRT_T('l') :
                            if (*(format + 1) == CRT_T('l'))
                            {
                                ++format;
#ifdef LONGLONG_IS_INT64
                                ++integer64;
                                num64 = 0;
                                break;
#else
                                ++longone;
                                    /* NOBREAK */
#endif
                            }
                            else
                            {
                                ++longone;
                                    /* NOBREAK */
                            }
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
            } else {
                pointer = NULL;         // doesn't matter what value we use here - we're only using it as a flag
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

            if (CRT_T('n') != comchr)
            {
                if (CRT_TEOF == ch)
                    goto error_return;
            }

            if (!widthset || width) {

                if (fSecureScanf) {
                    if(!suppress && (comchr == CRT_T('c') || comchr == CRT_T('s') || comchr == LEFT_BRACKET)) {

                        arglist = arglistsave;

                        /* Reinitialize pointer to point to the array to which we write the input */
                        pointer = va_arg(arglist, void*);

                        arglistsave = arglist;

                        /* Get the next argument - size of the array in characters */
                        original_array_width = array_width = va_arg(arglist, size_t);

                        if(array_width < 1) {
                            if (widechar > 0)
                                *(wchar_t UNALIGNED *)pointer = L'\0';
                            else
                                *(char *)pointer = '\0';

                            _SET_ERRNO(ENOMEM);

                            goto error_return;
                        }
                    }
                }
                switch(comchr) {

                    case CRT_T('c'):
                /*  case CRT_T('C'):  */
                        if (!widthset) {
                            ++widthset;
                            ++width;
                        }
                        if (widechar > 0)
                            fl_wchar_arg++;
                        goto scanit;


                    case CRT_T('s'):
                /*  case CRT_T('S'):  */
                        if(widechar > 0)
                            fl_wchar_arg++;
                        goto scanit;


                    case LEFT_BRACKET :   /* scanset */
                        if (widechar>0)
                            fl_wchar_arg++;
                        scanptr = (CRT__TUCHAR *)(++format);

                        if (CRT_T('^') == *scanptr) {
                            ++scanptr;
                            --reject; /* set reject to 255 */
                        }

                        /* Allocate "table" on first %[] spec */
#if ALLOC_TABLE
                        if (table == NULL) {
                            table = (char*)_malloc_crt(TABLESIZE);
                            if ( table == NULL)
                                goto error_return;
                            malloc_flag = 1;
                        }
#endif
                        memset(table, 0, TABLESIZE);

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

scanit:
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

                        if (fSecureScanf) {
                            /* One element is needed for '\0' for %s & %[ */
                            if(comchr != CRT_T('c')) {
                                --array_width;
                            }
                        }
                        while ( !widthset || width-- ) {

                            ch = INC();
                            if (
                                 (CRT_TEOF != ch) &&
                                   // char conditions
                                 ( ( comchr == CRT_T('c')) ||
                                   // string conditions !isspace()
                                   ( ( comchr == CRT_T('s') &&
                                       (!(ch >= CRT_T('\t') && ch <= CRT_T('\r')) &&
                                       ch != CRT_T(' ')))) ||
                                   // BRACKET conditions
                                   ( (comchr == LEFT_BRACKET) &&
                                     ((table[ch >> 3] ^ reject) & (1 << (ch & 7)))
                                     )
                                   )
                                )
                            {
                                if (!suppress) {
                                    if (fSecureScanf) {
                                        if(!array_width) {
                                            /* We have exhausted the user's buffer */

                                            enomem = 1;
                                            break;
                                        }
                                    }
#ifndef CRT_UNICODE
                                    if (fl_wchar_arg) {
                                        char temp[2];
                                        temp[0] = (char) ch;
                                        if (isleadbyte((unsigned char)ch))
                                        {
                                            temp[1] = (char) INC();
                                        }
                                        wctemp = L'?';
                                        change_to_widechar(&wctemp, temp, MB_CUR_MAX);
                                        *(wchar_t UNALIGNED *)pointer = wctemp;
                                        /* just copy L'?' if mbtowc fails, errno is set by mbtowc */
                                        pointer = (wchar_t *)pointer + 1;
                                        if (fSecureScanf) {
                                            --array_width;
                                        }
                                    } else
#else
                                    if (fl_wchar_arg) {
                                        *(wchar_t UNALIGNED *)pointer = ch;
                                        pointer = (wchar_t *)pointer + 1;
                                        if (fSecureScanf) {
                                            --array_width;
                                        }
                                    } else
#endif
                                    {
#ifndef CRT_UNICODE
                                        *(char *)pointer = (char)ch;
                                        pointer = (char *)pointer + 1;
                                        if (fSecureScanf) {
                                            --array_width;
                                        }
#else
                                        int temp = 0;
                                        if (!fSecureScanf) {
                                            /* convert wide to multibyte */
                                            if (_ERRCHECK_EINVAL_ERANGE(__internal_wctomb_s(&temp, (char *)pointer, MB_LEN_MAX, ch)) == 0)
                                            {
                                                /* do nothing if wctomb fails, errno will be set to EILSEQ */
                                                pointer = (char *)pointer + temp;
                                            }
                                        } else {
                                            /* convert wide to multibyte */
                                            if(__internal_wctomb_s(&temp,(char *)pointer, array_width, ch) == ERANGE) {
                                                /* We have exhausted the user's buffer */
                                                enomem = 1;
                                                break;
                                            }

                                            if (temp > 0)
                                            {
                                                /* do nothing if wctomb fails, errno will be set to EILSEQ */
                                                pointer = (char *)pointer + temp;
                                                array_width -= temp;
                                            }
                                        }
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

                        if (fSecureScanf) {
                            if(enomem) {
                                _SET_ERRNO(ENOMEM);
                                /* In case of error, blank out the input buffer */
                                if (fl_wchar_arg)
                                {
                                    _RESET_STRING(((wchar_t UNALIGNED *)start), original_array_width);
                                }
                                else
                                {
                                    _RESET_STRING(((char *)start), original_array_width);
                                }

                                goto error_return;
                            }
                        }

                        if (start != pointer) {
                            if (!suppress) {
                                ++count;
                                if ('c' != comchr) /* null-terminate strings */
                                    if (fl_wchar_arg)
                                    {
                                        *(wchar_t UNALIGNED *)pointer = L'\0';
                                        if (fSecureScanf) {
                                            _FILL_STRING(((wchar_t UNALIGNED *)start), original_array_width,
                                                ((wchar_t UNALIGNED *)pointer - (wchar_t UNALIGNED *)start + 1))
                                        }
                                    }
                                    else
                                    {
                                        *(char *)pointer = '\0';
                                        if (fSecureScanf) {
                                            _FILL_STRING(((char *)start), original_array_width,
                                                ((char *)pointer - (char *)start + 1))
                                        }
                                    }
                            } else /*NULL*/;
                        }
                        else
                            goto error_return;

                        break;

                    case CRT_T('i') :      /* could be d, o, or x */

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
                                if (widthset) {
                                    width -= 2;
                                    if (width < 1)
                                        ++done_flag;
                                }
                                comchr = CRT_T('x');
                            } else {
                                ++started;
                                if (CRT_T('x') != comchr) {
                                    if (widthset && !--width)
                                        ++done_flag;
                                    comchr = CRT_T('o');
                                }
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
#ifdef  _WIN64
                        /* force %p to be 64 bit in WIN64 */
                        ++integer64;
                        num64 = 0;
#endif
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

                                if (CRT_T('x') == comchr || CRT_T('p') == comchr)

                                    if (_ISXDIGIT(ch)) {
                                        num64 <<= 4;
                                        ch = _hextodec((CRT__TCHAR)ch);
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
                                        ch = _hextodec((CRT__TCHAR)ch);
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

                    case CRT_T('n') :      /* char count, don't inc return value */
                        number = charcount;
                        if(!suppress)
                            goto assign_num; /* found in number code above */
                        break;


                    case CRT_T('e') :
                 /* case CRT_T('E') : */
                    case CRT_T('f') :
                    case CRT_T('g') : /* scan a float */
                 /* case CRT_T('G') : */
                        nFloatStrUsed=0;

                        if (CRT_T('-') == ch) {
                            pFloatStr[nFloatStrUsed++] = CRT_T('-');
                            goto f_incwidth;

                        } else if (CRT_T('+') == ch) {
f_incwidth:
                            --width;
                            ch = INC();
                        }

                        if (!widthset)              /* must watch width */
                            width = -1;


                        /* now get integral part */

                        while (_ISDIGIT(ch) && width--) {
                            ++started;
                            pFloatStr[nFloatStrUsed++] = (char)ch;
                            if (__check_float_string(nFloatStrUsed,
                                                     &nFloatStrSz,
                                                     &pFloatStr,
                                                     floatstring,
                                                     &malloc_FloatStrFlag
                                                     )==FALSE) {
                                goto error_return;
                            }
                            ch = INC();
                        }

                        decimal = '.';

                        /* now check for decimal */
                        if (decimal == (char)ch && width--) {
                            ch = INC();
                            pFloatStr[nFloatStrUsed++] = decimal;
                            if (__check_float_string(nFloatStrUsed,
                                                     &nFloatStrSz,
                                                     &pFloatStr,
                                                     floatstring,
                                                     &malloc_FloatStrFlag
                                                     )==FALSE) {
                                goto error_return;
                            }

                            while (_ISDIGIT(ch) && width--) {
                                ++started;
                                pFloatStr[nFloatStrUsed++] = (CRT__TCHAR)ch;
                                if (__check_float_string(nFloatStrUsed,
                                                         &nFloatStrSz,
                                                         &pFloatStr,
                                                         floatstring,
                                                         &malloc_FloatStrFlag
                                                         )==FALSE) {
                                    goto error_return;
                                }
                                ch = INC();
                            }
                        }

                        /* now check for exponent */

                        if (started && (CRT_T('e') == ch || CRT_T('E') == ch) && width--) {
                            pFloatStr[nFloatStrUsed++] = CRT_T('e');
                            if (__check_float_string(nFloatStrUsed,
                                                     &nFloatStrSz,
                                                     &pFloatStr,
                                                     floatstring,
                                                     &malloc_FloatStrFlag
                                                     )==FALSE) {
                                goto error_return;
                            }

                            if (CRT_T('-') == (ch = INC())) {

                                pFloatStr[nFloatStrUsed++] = CRT_T('-');
                                if (__check_float_string(nFloatStrUsed,
                                                         &nFloatStrSz,
                                                         &pFloatStr,
                                                         floatstring,
                                                         &malloc_FloatStrFlag
                                                         )==FALSE) {
                                    goto error_return;
                                }
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
                                pFloatStr[nFloatStrUsed++] = (CRT__TCHAR)ch;
                                if (__check_float_string(nFloatStrUsed,
                                                         &nFloatStrSz,
                                                         &pFloatStr,
                                                         floatstring,
                                                         &malloc_FloatStrFlag
                                                         )==FALSE) {
                                    goto error_return;
                                }
                                ch = INC();
                            }

                        }

                        UN_INC(ch);

                        if (started)
                            if (!suppress) {
                                ++count;
                                pFloatStr[nFloatStrUsed]= CRT_T('\0');
#ifdef CRT_UNICODE
                                {
                                    /* convert floatstring to char string */
                                    /* and do the conversion */
                                    size_t cfslength;
                                    char *cfloatstring;

                                    /*
                                     * Basically the code below assumes that the MULTI BYTE
                                     * Characters are at max 2 bytes. This is true for CRT
                                     * because currently we don't support UTF8.
                                     */
                                    cfslength =(size_t)(nFloatStrSz+1)*sizeof(wchar_t);

                                    if ((cfloatstring = (char *)_malloc_crt (cfslength)) == NULL)
                                        goto error_return;
                                    _ERRCHECK_EINVAL_ERANGE(wcstombs_s (NULL, cfloatstring, cfslength, pFloatStr, cfslength - 1));
                                    _FASSIGN( longone-1, (char*)pointer , cfloatstring, (char)decimal);
                                    _free_crt (cfloatstring);
                                }
#else
                                _FASSIGN( longone-1, (char*)pointer , pFloatStr, (char)decimal);
#endif
                            } else /*NULL */;
                        else
                            goto error_return;

                        break;


                    default:    /* either found '%' or something else */

                        if ((int)*format != (int)ch) {
                            UN_INC(ch);
                            if (fSecureScanf) {
                                /* error_return ASSERT's if format_error is true */
                                format_error = TRUE;
                            }
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
            if (isleadbyte((unsigned char)ch))
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

        if ( (CRT_TEOF == ch) && ((*format != CRT_T('%')) || (*(format + 1) != CRT_T('n'))) )
            break;

    }  /* WHILE (*format) */

error_return:
#if ALLOC_TABLE
    if (malloc_flag == 1)
    {
        _free_crt(table);
    }
#endif
    if (malloc_FloatStrFlag == 1)
    {
        _free_crt(pFloatStr);
    }

    if (CRT_TEOF == ch)
        /* If any fields were matched or assigned, return count */
        return ( (count || match) ? count : EOF);
    else
        if (fSecureScanf) {
            if(format_error == TRUE) {
                _VALIDATE_RETURN( ("Invalid Input Format",0), EINVAL, count);
            }
        }
        return count;

}

/* _hextodec() returns a value of 0-15 and expects a char 0-9, a-f, A-F */
/* _inc() is the one place where we put the actual getc code. */
/* _whiteout() returns the first non-blank character, as defined by isspace() */

static CRT__TINT __cdecl _hextodec (CRT__TCHAR chr)
{
    return _ISDIGIT(chr) ? chr : (chr & ~(CRT_T('a') - CRT_T('A'))) - CRT_T('A') + 10 + CRT_T('0');
}

static CRT__TINT __cdecl _inc(FILEX* fileptr)
{
#ifndef CRT_UNICODE
    return _getc_lk(fileptr);
#else
    return _getwc_lk(fileptr);
#endif
}

static void __cdecl _un_inc(CRT__TINT chr, FILEX* fileptr)
{
    if (CRT_TEOF != chr) {
#ifndef CRT_UNICODE
        _ungetc_lk(chr, fileptr);
#else
        _ungetwc_lk(chr, fileptr);
#endif
    }
}

static CRT__TINT __cdecl _whiteout(int* counter, FILEX* fileptr)
{
    CRT__TINT ch;

    do
    {
        ++*counter;
        ch = _inc(fileptr);

        if (ch == CRT_TEOF)
        {
            break;
        }
    }
    while(_istspace((CRT__TUCHAR)ch));
    return ch;
}

#ifdef __cplusplus
} /* extern "C" */
#endif
