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
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _input() to do formatted input; called from scanf(),
*       etc. functions.  This module defines _cscanf() instead when
*       CPRFLAG is defined.  The file cscanf.c defines that symbol
*       and then includes this file in order to implement _cscanf().
*
*Note:
*       this file is included in safecrt.lib build directly, plese refer
*       to safecrt_[w]input_s.c
*
*Revision History:
*       09-26-83  RN    author
*       11-01-85  TC    added %F? %N? %?p %n %i
*       11-20-86  SKS   enlarged "table" to 256 bytes, to support chars > 0x7F
*       12-12-86  SKS   changed "s_in()" to pushback whitespace or other delimiter
*       03-24-87  BCM   Evaluation Issues:
*                       SDS - needs #ifdef SS_NE_DS for the "number" buffer
*                           (for S/M models only)
*                       GD/TS : (not evaluated)
*                       other INIT : (not evaluated)
*                           needs _cfltcvt_init to have been called if
*                           floating-point i/o conversions are being done
*                       TERM - nothing
*       06-25-87  PHG   added check_stack pragma
*       08-31-87  JCR   Made %n conform to ANSI standard: (1) %n is supposed to
*                       return the # of chars read so far by the current scanf(),
*                       NOT the total read on the stream since open; (2) %n is NOT
*                       supposed to affect the # of items read that is returned by
*                       scanf().
*       09-24-87  JCR   Made cscanf() use the va_ macros (fixes cl warnings).
*       11-04-87  JCR   Multi-thread support
*       11-16-87  JCR   Cscanf() now gets _CONIO_LOCK
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       02-25-88  JCR   If burn() char hits EOF, only return EOF if count==0.
*       05-31-88  WAJ   Now suports %Fs and %Ns
*       06-01-88  PHG   Merged DLL and normal versions
*       06-08-88  SJM   %D no longer means %ld.  %[]ABC], %[^]ABC] work.
*       06-14-88  SJM   Fixed %p, and %F? and %N? code.
*                 SJM   Complete re-write of input/_input for 6.00
*       09-15-88  JCR   If we match a field but it's not assigned, then are
*                       terminated by EOF, we must return 0 not EOF (ANSI).
*       09-25-88  GJF   Initial adaption for the 386
*       10-04-88  JCR   386: Removed 'far' keyword
*       11-30-88  GJF   Cleanup, now specific to 386
*       06-09-89  GJF   Propagated fixes of 03-06-89 and 04-05-89
*       11-20-89  GJF   Added const attribute to type of format. Also, fixed
*                       copyright.
*       12-21-89  GJF   Allow null character in scanset
*       02-14-90  KRS   Fix suppressed-assignment pattern matching.
*       03-20-90  GJF   Made _cscanf() _CALLTYPE2 and _input() _CALLTYPE1. Added
*                       #include <cruntime.h> and #include <register.h>.
*       03-26-90  GJF   Made static functions _CALLTYPE4. Placed prototype for
*                       _input() in internal.h and #include-d it. Changed type of
*                       arglist from void ** to va_list (to get rid of annoying
*                       warnings). Added #include <string.h>. Elaborated prototypes
*                       of static functions to get rid of compiler warnings.
*       05-21-90  GJF   Fixed stack checking pragma syntax.
*       07-23-90  SBM   Compiles cleanly with -W3, replaced <assertm.h> by
*                       <assert.h>, moved _cfltcvt_tab to new header
*                       <fltintrn.h>, formerly named <struct.h>
*       08-13-90  SBM   Compiles cleanly with -W3 with new build of compiler
*       08-27-90  SBM   Minor cleanup to agree with CRT7 version
*       10-02-90  GJF   New-style function declarators. Also, rewrote expr. to
*                       avoid using casts as lvalues.
*       10-22-90  GJF   Added arglistsave, used to save and restore arglist pointer
*                       without using pointer arithmetic.
*       12-28-90  SRW   Added _CRUISER_ conditional around check_stack pragma
*       01-16-91  GJF   ANSI naming.
*       03-14-91  GJF   Fix to allow processing of %n, even at eof. Fix devised by
*                       DanK of PSS.
*       06-19-91  GJF   Fixed execution of string, character and scan-set format
*                       directives to avoid problem with line-buffered devices
*                       (C700 bug 1441).
*       10-22-91  ETC   Int'l dec point; Under _INTL: wchar_t/mb support; fix bug
*                       under !ALLOW_RANGE (never compiled).
*       11-15-91  ETC   Fixed bug with %f %lf %Lf (bad handling of longone).
*       11-19-91  ETC   Added support for _wsscanf with WPRFLAG; added %tc %ts.
*       06-09-92  KRS   Rip out %tc/%ts; conform to new ISO spec.
*       08-17-92  KRS   Further ISO changes:  Add %lc/%ls/%hc/%hs/%C/%S.
*       12-23-92  SKS   Needed to handle %*n (suppressed storage of byte count)
*       02-16-93  CFW   Added wide character output for [] scanset.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-26-93  CFW   Wide char enable.
*       08-17-93  CFW   Avoid mapping tchar macros incorrectly if _MBCS defined.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       11-08-93  GJF   Merged in NT SDK version (use __unaligned pointer
*                       casts on MIPS and Alpha. Also, fixed #elif WPRFLAG to
*                       be #elif defined(WPRFLAG), and removed old CRUISER
*                       support.
*       12-16-93  CFW   Get rid of spurious compiler warnings.
*       03-15-94  GJF   Added support for I64 size modifier.
*       04-21-94  GJF   Must reinitialize integer64 flag.
*       09-05-94  SKS   Remove include of obsolete 16-bit file <sizeptr.h>
*       12-14-94  GJF   Changed test for (hex) digits so that when WPRFLAG is
*                       defined, only zero-extended (hex) digits are
*                       recognized. This way, the familar arithmetic to convert
*                       from character rep. to binary integer value will work.
*       01-10-95  CFW   Debug CRT allocs.
*       02-06-94  CFW   assert -> _ASSERTE.
*       02-22-95  GJF   Appended Mac version of source file (somewhat cleaned
*                       up), with appropriate #ifdef-s. Also, replaced
*                       WPRFLAG with _UNICODE.
*       08-01-96  RDK   For PMac, added __int64 support for _input.
*       02-27-98  RKP   Added 64 bit support.
*       07-07-98  RKP   Corrected %P formatting for 64 bit.
*       09-21-98  GJF   Added support for %I and %I32 modifiers.
*       05-17-99  PML   Remove all Macintosh support.
*       10-28-99  PML   vs7#10705 Win64 %p was totally busted
*       04-25-00  GB    Adding support for _cwprintf.
*       05-31-00  GB    Changed scanf to match with standards. problem was
*                       reading octal or hexa while %d was specified.
*       10-20-00  GB    Changed input not to use %[] case for %c and %s.
*       02-19-01  GB    Added check for return value of malloc.
*       03-13-01  PML   Fix heap leak on multiple %[] specs (vs7#224990)
*       07-07-01  BWT   Fix prefix bug - init pointer to a known value
*                       when handling * formatting.
*       02-20-02  BWT   prefast fixes - don't use alloca
*       06-19-02  MSL   Switched to use _CVTBUFSIZE instead of CVTBUFSIZE; 
*                       public symbol
*                       VS7 551701
*       09-09-02  GB    Added MACRO _CHECK_FLOAT_STRING to be able to convert big
*                       Floats comming through input stream to a float or double.
*       10-03-02  SJ    Fix for EOF at the beginning of read when the stream is
*                       a device. vs-whidbey#2442
*       10-25-02  SJ    Fix for fwscanf is supposed to return EOF if no fields 
*                       have been successfully matched vs-whidbey#2440
*       10-28-02  PK    Fix for __tinput so that it does not force useage of 
*                       function forms of _istspace (vs-whidbey#26539)
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-23-03  SJ    Secure Version for scanf family which takes an extra 
*                       size parameter from the var arg list.
*       12-12-03  AJS   Swap order of size parameter and buffer (buffer first,
*                       size second). VSWhidbey#207501
*       03-13-04  MSL   Avoid returning from __try for prefast
*       04-02-04  SJ    VSW#215722 - Used wctomb_s instead of wctomb to 
*                       correctly validate the size available for conversion.
*       07-19-04  AC    Added _SAFECRT_IMPL and _stdin_[w]input_s for safecrt.lib
*       07-29-04  AC    Added support for floating-point in safecrt.lib
*                       Use the safecrt macros to fill up the destination strings.
*       10-09-04  MSL   Prefix initialisation fix
*       10-04-04  AC    Do not increment pointer if wctomb fails
*                       VSW#334221
*       10-06-04  AGH   Added check that stream is ANSI for non-unicode versions.
*       11-23-04  AC    Fix use of mbtowc in error case.
*                       VSW#334221
*       03-15-05  PAL   _whiteout: Check for _TEOF before coercing char to UCHAR.
*                       VSW#452520
*       03-23-05  MSL   Review comment cleanup
*       04-01-05  MSL   Integer overflow protection
*       04-25-05  AC    Added debug filling
*                       VSW#2459
*       05-06-05  PML   MB_CUR_MAX is not _locale_t-dependent (noticed while
*                       fixing VSW#2495).
*       11-10-06  PMB   Removed most _INTEGRAL_MAX_BITS #ifdefs
*                       DDB#11174
*
*******************************************************************************/


#define ALLOW_RANGE /* allow "%[a-z]"-style scansets */

#include <cruntime.h>
#include <stdio.h>
#include <ctype.h>
#include <cvt.h>
#include <conio.h>
#include <stdarg.h>
#include <string.h>
#include <internal.h>
#include <fltintrn.h>
#include <malloc.h>
#include <locale.h>
#include <mtdll.h>
#include <stdlib.h>
#include <setlocal.h>
#include <dbgint.h>

#ifndef _INC_INTERNAL_SAFECRT
#include <internal_securecrt.h>
#endif

#ifdef _MBCS    /* always want either Unicode or SBCS for tchar.h */
#undef _MBCS
#endif
#include <tchar.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef _SAFECRT_IMPL

#undef _malloc_crt
#undef _realloc_crt
#define _malloc_crt malloc
#define _realloc_crt realloc

/* Helpers for scanf_s. */
#ifdef _UNICODE
int __cdecl _stdin_winput_s(const wchar_t *_Format, va_list _ArgList)
#else
int __cdecl _stdin_input_s(const char *_Format, va_list _ArgList)
#endif
{
    int retval = -1;

    _lock(_STREAM_LOCKS + 0);
    __try {
        retval = __tinput_s(stdin, _Format, _ArgList);
    }
    __finally {
        _unlock(_STREAM_LOCKS + 0);
    }

    return retval;
}

#ifdef _UNICODE
int __cdecl _swinput_s(const wchar_t *_String, size_t _Count, const wchar_t *_Format, va_list _ArgList)
#else
int __cdecl _sinput_s(const char *_String, size_t _Count, const char *_Format, va_list _ArgList)
#endif
{
    FILE stream = { 0 };
    FILE *infile = &stream;
    int retval = -1;

    /* validation section */
    _VALIDATE_RETURN( (_String != NULL), EINVAL, -1);
    _VALIDATE_RETURN( (_Format != NULL), EINVAL, -1);
    _VALIDATE_RETURN(_Count <= (INT_MAX / sizeof(_TCHAR)), EINVAL, -1);

    infile->_flag = _IOREAD | _IOSTRG | _IOMYBUF;
    infile->_ptr = infile->_base = (char *) _String;
    infile->_cnt = (int)_Count * sizeof(_TCHAR);

    retval = __tinput_s(infile, _Format, _ArgList);

    return retval;
}

#endif /* _SAFECRT_IMPL */

#ifdef _SAFECRT_IMPL
#define _FASSIGN(flag, argument, number, dec_point, locale) _safecrt_fassign((flag), (argument), (number), (dec_point))
#else
#define _FASSIGN(flag, argument, number, dec_point, locale) _fassign_l((flag), (argument), (number), (locale))
#endif

#if  defined (UNICODE)
#define ALLOC_TABLE 1
#else 
#define ALLOC_TABLE 0
#endif

#define HEXTODEC(chr)   _hextodec(chr)

#define LEFT_BRACKET    ('[' | ('a' - 'A')) /* 'lowercase' version */

static _TINT __cdecl _hextodec(_TCHAR);
#ifdef CPRFLAG

#define INC()           (++charcount, _inc())
#define UN_INC(chr)     (--charcount, _un_inc(chr))
#define EAT_WHITE()     _whiteout(&charcount)

static _TINT __cdecl _inc(void);
static void __cdecl _un_inc(_TINT);
static _TINT __cdecl _whiteout(int *);

#else /* CPRFLAG */

#define INC()           (++charcount, _inc(stream))
#define UN_INC(chr)     (--charcount, _un_inc(chr, stream))
#define EAT_WHITE()     _whiteout(&charcount, stream)

static _TINT __cdecl _inc(FILE *);
static void __cdecl _un_inc(_TINT, FILE *);
static _TINT __cdecl _whiteout(int *, FILE *);

#endif /* CPRFLAG */

#ifndef _UNICODE
#define _ISDIGIT(chr)   isdigit((unsigned char)chr)
#define _ISXDIGIT(chr)  isxdigit((unsigned char)chr)
#else
#define _ISDIGIT(chr)   ( !(chr & 0xff00) && isdigit( ((chr) & 0x00ff) ) )
#define _ISXDIGIT(chr)  ( !(chr & 0xff00) && isxdigit( ((chr) & 0x00ff) ) )
#endif


#define LONGLONG_IS_INT64 1     /* 1 means long long is same as int64
                                   0 means long long is same as long */

/***
*  int __check_float_string(size_t,size_t *, _TCHAR**, _TCHAR*, int*)
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
                                _TCHAR **pFloatStr,
                                _TCHAR *floatstring,
                                int *pmalloc_FloatStrFlag)
{
    void *tmpPointer;
    CRT_WARNING_DISABLE_PUSH(22011, "Silence prefast about overflow/underflow");
    _ASSERTE(nFloatStrUsed<=(*pnFloatStrSz));
    if (nFloatStrUsed==(*pnFloatStrSz))
      {
        if ((*pFloatStr)==floatstring)
        {
            if (((*pFloatStr)=(_TCHAR *)_calloc_crt((*pnFloatStrSz),2*sizeof(_TCHAR)))==NULL)
            {
              return FALSE;
            }
            (*pmalloc_FloatStrFlag)=1;
            memcpy((*pFloatStr),floatstring,(*pnFloatStrSz)*sizeof(_TCHAR));
            (*pnFloatStrSz)*=2;
        }
        else
        {
            if ((tmpPointer=(_TCHAR *)_recalloc_crt((*pFloatStr), (*pnFloatStrSz),2*sizeof(_TCHAR)))==NULL)
            {
                return FALSE;
            }
            (*pFloatStr)=(_TCHAR *)(tmpPointer);
            (*pnFloatStrSz)*=2;
        }
    }
    CRT_WARNING_POP;
    return TRUE;
}


#ifdef CPRFLAG

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
#ifdef _SAFECRT_IMPL

#ifndef _SECURE_SCANF
static int __cdecl input(const _TUCHAR *, va_list);
#else
static int __cdecl input_s(const _TUCHAR *, va_list);
#endif

#ifndef _SECURE_SCANF
int __cdecl _tcscanf (const _TCHAR *format,...)
#else
int __cdecl _tcscanf_s (const _TCHAR *format,...)
#endif
{
    va_list arglist;

    va_start(arglist, format);

#ifndef _SECURE_SCANF
    return input(reinterpret_cast<const _TUCHAR*>(format), arglist);   /* get the input */
#else
    return input_s(reinterpret_cast<const _TUCHAR*>(format), arglist);   /* get the input */
#endif    
}

#else /* _SAFECRT_IMPL */

#ifndef _SECURE_SCANF
    #define _CPRSCANF   _tcscanf
    #define _CPRSCANF_L _tcscanf_l
    #define _CPRINPUT_L _cprinput_l
#else
    #define _CPRSCANF   _tcscanf_s
    #define _CPRSCANF_L _tcscanf_s_l
    #define _CPRINPUT_L _cprinput_s_l
#endif

static int __cdecl _CPRINPUT_L(const _TUCHAR *, _locale_t , va_list);

int __cdecl _CPRSCANF(const _TCHAR *format,...)
{
    va_list arglist;

    va_start(arglist, format);

    return _CPRINPUT_L(reinterpret_cast<const _TUCHAR*>(format), NULL, arglist);   /* get the input */
}

int __cdecl _CPRSCANF_L(const _TCHAR *format, _locale_t plocinfo, ...)
{
    va_list arglist;

    va_start(arglist, plocinfo);

    return _CPRINPUT_L(reinterpret_cast<const _TUCHAR*>(format), plocinfo, arglist);   /* get the input */
}

#undef _CPRSCANF
#undef _CPRSCANF_L
#undef _CPRINPUT_L

#endif /* _SAFECRT_IMPL */

#endif  /* CPRFLAG */


#define ASCII       32           /* # of bytes needed to hold 256 bits */

#define SCAN_SHORT     0         /* also for FLOAT */
#define SCAN_LONG      1         /* also for DOUBLE */
#define SCAN_L_DOUBLE  2         /* only for LONG DOUBLE */

#define SCAN_NEAR    0
#define SCAN_FAR     1

#ifndef _UNICODE
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
*   FILE *stream - file to read from
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

#ifdef _SAFECRT_IMPL
    #define _INTRN_LOCALE_CONV( x ) localeconv()
#else
    inline const lconv* _INTRN_LOCALE_CONV( _LocaleUpdate& l )
    {
        return l.GetLocaleT()->locinfo->lconv;
    }
#endif

#ifdef _SAFECRT_IMPL
#ifdef CPRFLAG
	#ifndef _SECURE_SCANF
	static int __cdecl input(const _TUCHAR* format, va_list arglist)
	#else
	static int __cdecl input_s(const _TUCHAR* format, va_list arglist)
	#endif
#else
	#ifndef _SECURE_SCANF
	int __cdecl __tinput (FILE* stream, const _TUCHAR* format, va_list arglist)
	#else
	int __cdecl __tinput_s (FILE* stream, const _TUCHAR* format, va_list arglist)
	#endif
#endif /* CPRFLAG */
#else
#ifdef CPRFLAG
	#ifndef _SECURE_SCANF
	static int __cdecl _cprinput_l(const _TUCHAR* format, _locale_t plocinfo, va_list arglist)
	#else
	static int __cdecl _cprinput_s_l(const _TUCHAR* format, _locale_t plocinfo, va_list arglist)
	#endif
#else
	#ifndef _SECURE_SCANF
	int __cdecl _tinput_l(FILE* stream, const _TUCHAR* format, _locale_t plocinfo, va_list arglist)
	#else
	int __cdecl _tinput_s_l(FILE* stream, const _TUCHAR* format, _locale_t plocinfo, va_list arglist)
	#endif
#endif /* CPRFLAG */
#endif
{
    _TCHAR floatstring[_CVTBUFSIZE + 1];
    _TCHAR *pFloatStr=floatstring;
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

    unsigned __int64 num64;             /* temp for 64-bit integers          */
    void *pointer=NULL;                 /* points to user data receptacle    */
    void *start;                        /* indicate non-empty string         */


#ifndef _UNICODE
    wchar_t wctemp=L'\0';
#endif
    _TUCHAR *scanptr;                   /* for building "table" data         */
    _TINT ch = 0;
    int charcount;                      /* total number of chars read        */
    int comchr;                         /* holds designator type             */
    int count;                          /* return value.  # of assignments   */

    int started;                        /* indicate good number              */
    int width;                          /* width of field                    */
    int widthset;                       /* user has specified width          */
#ifdef _SECURE_SCANF    
    size_t array_width = 0;
    size_t original_array_width = 0;
    int enomem = 0;
    int format_error = FALSE;
#endif

/* Neither coerceshort nor farone are need for the 386 */


    char done_flag;                     /* general purpose loop monitor      */
    char longone;                       /* 0 = SHORT, 1 = LONG, 2 = L_DOUBLE */
    int integer64;                      /* 1 for 64-bit integer, 0 otherwise */
    signed char widechar;               /* -1 = char, 0 = ????, 1 = wchar_t  */
    char reject;                        /* %[^ABC] instead of %[ABC]         */
    char negative;                      /* flag for '-' detected             */
    char suppress;                      /* don't assign anything             */
    char match;                         /* flag: !0 if any fields matched    */
    va_list arglistsave;                /* save arglist value                */

    char fl_wchar_arg;                  /* flags wide char/string argument   */

    _TCHAR decimal;


#ifdef ALLOW_RANGE
    _TUCHAR rngch;
#endif
    _TUCHAR last;
    _TUCHAR prevchar;
    _TCHAR tch;

    _VALIDATE_RETURN( (format != NULL), EINVAL, EOF);
    _CHECK_IO_INIT(-1);

#ifndef CPRFLAG
    _VALIDATE_RETURN( (stream != NULL), EINVAL, EOF);
#ifndef _UNICODE
    _VALIDATE_STREAM_ANSI_RETURN(stream, EINVAL, EOF);
#endif
#endif

#ifndef _SAFECRT_IMPL
    _LocaleUpdate _loc_update(plocinfo);
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

        if (_istspace((_TUCHAR)*format)) {

            UN_INC(EAT_WHITE()); /* put first non-space char back */

            do {
                tch = *++format;
            } while (_istspace((_TUCHAR)tch));  

            continue;

        }

        if (_T('%') == *format && _T('%') != *(format + 1)) {

            number = 0;
            prevchar = 0;
            width = widthset = started = 0;
#ifdef _SECURE_SCANF
            original_array_width = array_width = 0;
            enomem = 0;
#endif
            fl_wchar_arg = done_flag = suppress = negative = reject = 0;
            widechar = 0;

            longone = 1;
            integer64 = 0;

            while (!done_flag) {

                comchr = *++format;
                if (_ISDIGIT((_TUCHAR)comchr)) {
                    ++widthset;
                    width = MUL10(width) + (comchr - _T('0'));
                } else
                    switch (comchr) {
                        case _T('F') :
                        case _T('N') :   /* no way to push NEAR in large model */
                            break;  /* NEAR is default in small model */
                        case _T('h') :
                            /* set longone to 0 */
                            --longone;
                            --widechar;         /* set widechar = -1 */
                            break;

                        case _T('I'):
                            if ( (*(format + 1) == _T('6')) &&
                                 (*(format + 2) == _T('4')) )
                            {
                                format += 2;
                                ++integer64;
                                num64 = 0;
                                break;
                            }
                            else if ( (*(format + 1) == _T('3')) &&
                                      (*(format + 2) == _T('2')) )
                            {
                                format += 2;
                                break;
                            }
                            else if ( (*(format + 1) == _T('d')) ||
                                      (*(format + 1) == _T('i')) ||
                                      (*(format + 1) == _T('o')) ||
                                      (*(format + 1) == _T('x')) ||
                                      (*(format + 1) == _T('X')) )
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

                        case _T('L') :
                        /*  ++longone;  */
                            ++longone;
                            break;

                        case _T('l') :
                            if (*(format + 1) == _T('l'))
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
                        case _T('w') :
                            ++widechar;         /* set widechar = 1 */
                            break;

                        case _T('*') :
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
                if ((*format == _T('S')) || (*format == _T('C')))
#ifdef _UNICODE
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

            comchr = *format | (_T('a') - _T('A'));

            if (_T('n') != comchr)
                if (_T('c') != comchr && LEFT_BRACKET != comchr)
                    ch = EAT_WHITE();
                else
                    ch = INC();

            if (_T('n') != comchr)
            {
                if (_TEOF == ch)
                    goto error_return;
            }

            if (!widthset || width) {

#ifdef _SECURE_SCANF
                if(!suppress && (comchr == _T('c') || comchr == _T('s') || comchr == LEFT_BRACKET)) {
                    
                    arglist = arglistsave;

                    /* Reinitialize pointer to point to the array to which we write the input */
                    pointer = va_arg(arglist, void*);

                    arglistsave = arglist;

                    /* Get the next argument - size of the array in characters */
#ifdef _WIN64
                    original_array_width = array_width = (size_t)(va_arg(arglist, unsigned int));
#else
                    original_array_width = array_width = va_arg(arglist, size_t);
#endif
                    
                    if(array_width < 1) {
                        if (widechar > 0)
                            *(wchar_t UNALIGNED *)pointer = L'\0';
                        else
                            *(char *)pointer = '\0';
                        
                        errno = ENOMEM;
                        
                        goto error_return;
                    }
                }
#endif
                switch(comchr) {

                    case _T('c'):
                /*  case _T('C'):  */
                        if (!widthset) {
                            ++widthset;
                            ++width;
                        }
                        if (widechar > 0)
                            fl_wchar_arg++;
                        goto scanit;


                    case _T('s'):
                /*  case _T('S'):  */
                        if(widechar > 0)
                            fl_wchar_arg++;
                        goto scanit;


                    case LEFT_BRACKET :   /* scanset */
                        if (widechar>0)
                            fl_wchar_arg++;
                        scanptr = (_TUCHAR *)(++format);

                        if (_T('^') == *scanptr) {
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
                            if (_T(']') == *scanptr) {
                                prevchar = _T(']');
                                ++scanptr;

                                table[ _T(']') >> 3] = 1 << (_T(']') & 7);

                            }

                        while (_T(']') != *scanptr) {

                            rngch = *scanptr++;

                            if (_T('-') != rngch ||
                                 !prevchar ||           /* first char */
                                 _T(']') == *scanptr) /* last char */

                                table[(prevchar = rngch) >> 3] |= 1 << (rngch & 7);

                            else {  /* handle a-z type set */

                                rngch = *scanptr++; /* get end of range */

                                if (prevchar < rngch)  /* %[a-z] */
                                    last = rngch;
                                else {              /* %[z-a] */
                                    last = prevchar;
                                    prevchar = rngch;
                                }
                                /* last could be 0xFF, so we handle it at the end of the for loop */
                                for (rngch = prevchar; rngch < last; ++rngch)
                                {
                                    table[rngch >> 3] |= 1 << (rngch & 7);
                                }
                                table[last >> 3] |= 1 << (last & 7);

                                prevchar = 0;

                            }
                        }


#else
                        if (LEFT_BRACKET == comchr)
                            if (_T(']') == *scanptr) {
                                ++scanptr;
                                table[(prevchar = _T(']')) >> 3] |= 1 << (_T(']') & 7);
                            }

                        while (_T(']') != *scanptr) {
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

#ifdef _SECURE_SCANF
                        /* One element is needed for '\0' for %s & %[ */
                        if(comchr != _T('c')) {
                            --array_width;
                        }
#endif
                        while ( !widthset || width-- ) {

                            ch = INC();
                            if (
#ifndef CPRFLAG
                                 (_TEOF != ch) &&
#endif
                                   // char conditions
                                 ( ( comchr == _T('c')) ||
                                   // string conditions !isspace()
                                   ( ( comchr == _T('s') &&
                                       (!(ch >= _T('\t') && ch <= _T('\r')) && 
                                       ch != _T(' ')))) ||
                                   // BRACKET conditions
                                   ( (comchr == LEFT_BRACKET) &&
                                     ((table[ch >> 3] ^ reject) & (1 << (ch & 7)))
                                     )
                                   )
                                )
                            {
                                if (!suppress) {
#ifdef _SECURE_SCANF
                                    if(!array_width) {
                                        /* We have exhausted the user's buffer */
                                        
                                        enomem = 1;
                                        break;
                                    }
#endif
#ifndef _UNICODE
                                    if (fl_wchar_arg) {
                                        char temp[2];
                                        temp[0] = (char) ch;
                                        if (isleadbyte((unsigned char)ch))
                                        {
                                            temp[1] = (char) INC();
                                        }
                                        wctemp = L'?';
#ifdef _SAFECRT_IMPL
                                        mbtowc(&wctemp, temp, MB_CUR_MAX);
#else
                                        _mbtowc_l(&wctemp,
                                                  temp,
                                                  _loc_update.GetLocaleT()->locinfo->mb_cur_max,
                                                  _loc_update.GetLocaleT());
#endif
                                        *(wchar_t UNALIGNED *)pointer = wctemp;
                                        /* just copy L'?' if mbtowc fails, errno is set by mbtowc */
                                        pointer = (wchar_t *)pointer + 1;
#ifdef _SECURE_SCANF
                                        --array_width;
#endif
                                    } else
#else
                                    if (fl_wchar_arg) {
                                        *(wchar_t UNALIGNED *)pointer = ch;
                                        pointer = (wchar_t *)pointer + 1;
#ifdef _SECURE_SCANF
                                        --array_width;
#endif
                                    } else
#endif
                                    {
#ifndef _UNICODE
                                    *(char *)pointer = (char)ch;
                                    pointer = (char *)pointer + 1;
#ifdef _SECURE_SCANF
                                    --array_width;
#endif
#else
                                    int temp = 0;
#ifndef _SECURE_SCANF
                                    /* convert wide to multibyte */
                                    if (_ERRCHECK_EINVAL_ERANGE(wctomb_s(&temp, (char *)pointer, MB_LEN_MAX, ch)) == 0)
                                    {
                                        /* do nothing if wctomb fails, errno will be set to EILSEQ */
                                        pointer = (char *)pointer + temp;
                                    }
#else
                                    /* convert wide to multibyte */
#ifdef _SAFECRT_IMPL
                                    if (array_width >= ((size_t)MB_CUR_MAX))
                                    {
_BEGIN_SECURE_CRT_DEPRECATION_DISABLE
                                        temp = wctomb((char *)pointer, ch);
_END_SECURE_CRT_DEPRECATION_DISABLE
                                    }
                                    else
                                    {
                                        char tmpbuf[MB_LEN_MAX];
_BEGIN_SECURE_CRT_DEPRECATION_DISABLE
                                        temp = wctomb(tmpbuf, ch);
_END_SECURE_CRT_DEPRECATION_DISABLE
                                        if (temp > 0 && ((size_t)temp) > array_width)
                                        {
                                            /* We have exhausted the user's buffer */
                                            enomem = 1;
                                            break;
                                        }
                                        memcpy(pointer, tmpbuf, temp);
                                    }
#else
                                    if(wctomb_s(&temp,(char *)pointer, array_width, ch) == ERANGE) {
                                        /* We have exhausted the user's buffer */
                                        enomem = 1;
                                        break;
                                    }
#endif
                                    if (temp > 0)
                                    {
                                        /* do nothing if wctomb fails, errno will be set to EILSEQ */
                                        pointer = (char *)pointer + temp;
                                        array_width -= temp;
                                    }
#endif
#endif
                                    }
                                } /* suppress */
                                else {
                                    /* just indicate a match */
                                    start = (_TCHAR *)start + 1;
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
                        
#ifdef _SECURE_SCANF
                        if(enomem) {
                            errno = ENOMEM;
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
#endif

                        if (start != pointer) {
                            if (!suppress) {
                                ++count;
                                if ('c' != comchr) /* null-terminate strings */
                                    if (fl_wchar_arg)
                                    {
                                        *(wchar_t UNALIGNED *)pointer = L'\0';
#ifdef _SECURE_SCANF
                                        _FILL_STRING(((wchar_t UNALIGNED *)start), original_array_width, 
                                            ((wchar_t UNALIGNED *)pointer - (wchar_t UNALIGNED *)start + 1))
#endif
                                    }
                                    else
                                    {
                                        *(char *)pointer = '\0';
#ifdef _SECURE_SCANF
                                        _FILL_STRING(((char *)start), original_array_width, 
                                            ((char *)pointer - (char *)start + 1))
#endif
                                    }
                            } else /*NULL*/;
                        }
                        else
                            goto error_return;

                        break;

                    case _T('i') :      /* could be d, o, or x */

                        comchr = _T('d'); /* use as default */

                    case _T('x'):

                        if (_T('-') == ch) {
                            ++negative;

                            goto x_incwidth;

                        } else if (_T('+') == ch) {
x_incwidth:
                            if (!--width && widthset)
                                ++done_flag;
                            else
                                ch = INC();
                        }

                        if (_T('0') == ch) {

                            if (_T('x') == (_TCHAR)(ch = INC()) || _T('X') == (_TCHAR)ch) {
                                ch = INC();
                                if (widthset) {
                                    width -= 2;
                                    if (width < 1)
                                        ++done_flag;
                                }
                                comchr = _T('x');
                            } else {
                                ++started;
                                if (_T('x') != comchr) {
                                    if (widthset && !--width)
                                        ++done_flag;
                                    comchr = _T('o');
                                }
                                else {
                                    /* scanning a hex number that starts */
                                    /* with a 0. push back the character */
                                    /* currently in ch and restore the 0 */
                                    UN_INC(ch);
                                    ch = _T('0');
                                }
                            }
                        }
                        goto getnum;

                        /* NOTREACHED */

                    case _T('p') :
                        /* force %hp to be treated as %p */
                        longone = 1;
#ifdef  _WIN64
                        /* force %p to be 64 bit in WIN64 */
                        ++integer64;
                        num64 = 0;
#endif
                    case _T('o') :
                    case _T('u') :
                    case _T('d') :

                        if (_T('-') == ch) {
                            ++negative;

                            goto d_incwidth;

                        } else if (_T('+') == ch) {
d_incwidth:
                            if (!--width && widthset)
                                ++done_flag;
                            else
                                ch = INC();
                        }

getnum:
                        if ( integer64 ) {

                            while (!done_flag) {

                                if (_T('x') == comchr || _T('p') == comchr)

                                    if (_ISXDIGIT(ch)) {
                                        num64 <<= 4;
                                        ch = _hextodec(ch);
                                    }
                                    else
                                        ++done_flag;

                                else if (_ISDIGIT(ch))

                                    if (_T('o') == comchr)
                                        if (_T('8') > ch)
                                                num64 <<= 3;
                                        else {
                                                ++done_flag;
                                        }
                                    else /* _T('d') == comchr */
                                        num64 = MUL10(num64);

                                else
                                    ++done_flag;

                                if (!done_flag) {
                                    ++started;
                                    num64 += ch - _T('0');

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
                            while (!done_flag) {

                                if (_T('x') == comchr || _T('p') == comchr)

                                    if (_ISXDIGIT(ch)) {
                                        number = (number << 4);
                                        ch = _hextodec(ch);
                                    }
                                    else
                                        ++done_flag;

                                else if (_ISDIGIT(ch))

                                    if (_T('o') == comchr)
                                        if (_T('8') > ch)
                                            number = (number << 3);
                                        else {
                                            ++done_flag;
                                        }
                                    else /* _T('d') == comchr */
                                        number = MUL10(number);

                                else
                                    ++done_flag;

                                if (!done_flag) {
                                    ++started;
                                    number += ch - _T('0');

                                    if (widthset && !--width)
                                        ++done_flag;
                                    else
                                        ch = INC();
                                } else
                                    UN_INC(ch);

                            } /* end of WHILE loop */

                            if (negative)
                                number = (unsigned long)(-(long)number);
                        }
                        if (_T('F')==comchr) /* expected ':' in long pointer */
                            started = 0;

                        if (started)
                            if (!suppress) {

                                ++count;
assign_num:
                                if ( integer64 )
                                    *(__int64 UNALIGNED *)pointer = (unsigned __int64)num64;
                                else
                                if (longone)
                                    *(long UNALIGNED *)pointer = (unsigned long)number;
                                else
                                    *(short UNALIGNED *)pointer = (unsigned short)number;

                            } else /*NULL*/;
                        else
                            goto error_return;

                        break;

                    case _T('n') :      /* char count, don't inc return value */
                        number = charcount;
                        if(!suppress)
                            goto assign_num; /* found in number code above */
                        break;


                    case _T('e') :
                 /* case _T('E') : */
                    case _T('f') :
                    case _T('g') : /* scan a float */
                 /* case _T('G') : */
                        nFloatStrUsed=0;

                        if (_T('-') == ch) {
                            pFloatStr[nFloatStrUsed++] = _T('-');
                            goto f_incwidth;

                        } else if (_T('+') == ch) {
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

#ifdef _UNICODE
                        decimal=*_INTRN_LOCALE_CONV(_loc_update)->_W_decimal_point;
#else
                        decimal=*_INTRN_LOCALE_CONV(_loc_update)->decimal_point;
#endif

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
                                pFloatStr[nFloatStrUsed++] = (_TCHAR)ch;
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

                        if (started && (_T('e') == ch || _T('E') == ch) && width--) {
                            pFloatStr[nFloatStrUsed++] = _T('e');
                            if (__check_float_string(nFloatStrUsed,
                                                     &nFloatStrSz,
                                                     &pFloatStr,
                                                     floatstring,
                                                     &malloc_FloatStrFlag
                                                     )==FALSE) {
                                goto error_return;
                            }

                            if (_T('-') == (ch = INC())) {

                                pFloatStr[nFloatStrUsed++] = _T('-');
                                if (__check_float_string(nFloatStrUsed,
                                                         &nFloatStrSz,
                                                         &pFloatStr,
                                                         floatstring,
                                                         &malloc_FloatStrFlag
                                                         )==FALSE) {
                                    goto error_return;
                                }
                                goto f_incwidth2;

                            } else if (_T('+') == ch) {
f_incwidth2:
                                if (!width--)
                                    ++width;
                                else
                                    ch = INC();
                            }


                            while (_ISDIGIT(ch) && width--) {
                                ++started;
                                pFloatStr[nFloatStrUsed++] = (_TCHAR)ch;
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
                                pFloatStr[nFloatStrUsed]= _T('\0');
#ifdef _UNICODE
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
                                    _FASSIGN( longone-1, (char*)pointer , cfloatstring, (char)decimal, _loc_update.GetLocaleT());
                                    _free_crt (cfloatstring);
                                }
#else
                                _FASSIGN( longone-1, (char*)pointer , pFloatStr, (char)decimal, _loc_update.GetLocaleT());
#endif
                            } else /*NULL */;
                        else
                            goto error_return;

                        break;


                    default:    /* either found '%' or something else */

                        if ((int)*format != (int)ch) {
                            UN_INC(ch);
#ifdef _SECURE_SCANF
                            /* error_return ASSERT's if format_error is true */
                                format_error = TRUE;
#endif
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
            if (_T('%') == *format && _T('%') == *(format + 1))
                {
                format++;
                }
            if ((int)*format++ != (int)(ch = INC()))
                {
                UN_INC(ch);
                goto error_return;
                }
#ifndef _UNICODE
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

#ifndef CPRFLAG
        if ( (_TEOF == ch) && ((*format != _T('%')) || (*(format + 1) != _T('n'))) )
            break;
#endif

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

#ifndef CPRFLAG
    if (_TEOF == ch)
        /* If any fields were matched or assigned, return count */
        return ( (count || match) ? count : EOF);
    else
#endif
#ifdef _SECURE_SCANF
        if(format_error == TRUE) {
            _VALIDATE_RETURN( ("Invalid Input Format",0), EINVAL, count);
        }
#endif
        return count;

}

/* _hextodec() returns a value of 0-15 and expects a char 0-9, a-f, A-F */
/* _inc() is the one place where we put the actual getc code. */
/* _whiteout() returns the first non-blank character, as defined by isspace() */

static _TINT __cdecl _hextodec ( _TCHAR chr)
{
    return _ISDIGIT(chr) ? chr : (chr & ~(_T('a') - _T('A'))) - _T('A') + 10 + _T('0');
}

#ifdef CPRFLAG

static _TINT __cdecl _inc(void)
{
    return (_gettche_nolock());
}

static void __cdecl _un_inc(_TINT chr)
{
    if (_TEOF != chr) {
        _ungettch_nolock(chr);
    }
}

static _TINT __cdecl _whiteout(int* counter)
{
    _TINT ch;

    do
    {
        ++*counter;
        ch = _inc();

        if (ch == _TEOF)
        {
            break;
        }
    }
    while(_istspace((_TUCHAR)ch));
    return ch;
}

#else /* CPRFLAG */

static _TINT __cdecl _inc(FILE* fileptr)
{
    return (_gettc_nolock(fileptr));
}

static void __cdecl _un_inc(_TINT chr, FILE* fileptr)
{
    if (_TEOF != chr) {
        _ungettc_nolock(chr,fileptr);
    }
}

static _TINT __cdecl _whiteout(int* counter, FILE* fileptr)
{
    _TINT ch;

    do
    {
        ++*counter;
        ch = _inc(fileptr);

        if (ch == _TEOF)
        {
            break;
        }
    }
    while(_istspace((_TUCHAR)ch));
    return ch;
}

#endif /* CPRFLAG */

#ifdef __cplusplus
} /* extern "C" */
#endif
