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
*internal.h - contains declarations of internal routines and variables
*
*
*Purpose:
*       Declares routines and variables used internally by the C run-time.
*
*       [Internal]
*
****/

#ifndef _INC_INTERNAL
#define _INC_INTERNAL

#ifdef __cplusplus
#include <new> // for std::bad_alloc
#define _THROW0() throw()
#define _THROW1(x0) throw(x0)
#else
#define _THROW0()
#define _THROW1(x0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <cruntime.h>

// need CRITICAL_SECTION
#include <windows.h>

#define _osfhnd(n)          ( __piob[n]->osfhnd )
#define _osfile(n)          ( __piob[n]->osfile )
#define _osfhndstr(str)     ( (str)->osfhnd )
#define _osfilestr(str)     ( (str)->osfile )
//#define _free_osfhnd(fh)  (_osfhnd(fh) = (long)INVALID_HANDLE_VALUE)

#define _isatty(str)    ((int)(_osfilestr(str) & FDEV))
#define _tell(str)      _lseek(str,0L,1)
#define _telli64(str)   _lseeki64(str,0L,1)
#define _commit(str)    FlushFileBuffers(_osfhndstr(str))



// Flags for the _flag field of the FILEX struct
// moved out of the public STDIO/STDLIB.H
#define _IOREAD         0x0001
#define _IOWRT          0x0002

#define _IOFBF          0x0000
#define _IOLBF          0x0040
#define _IONBF          0x0004

#define _IOMYBUF        0x0008
#define _IOEOF          0x0010
#define _IOERR          0x0020
#define _IOSTRG         0x0040
#define _IORW           0x0080

/*
 * We have now eliminated the FILE struct & the static _iob. Therefore
 * __piob[] is always fully dynamically allocated, always contains only FILEX
 * structures. There are no FILE structures any more. FILE is simply defined
 * to void in the public STDLIB.H
 * _FILEX & FILEX are identical. The distinction is maintained for a little while
 * just so we can tell what used to be FILE (and became FILEX) and what was always
 * _FILEX
 */
#if !defined(_FILEX_DEFINED)
    typedef struct
    {
        char *_ptr;
        int   _cnt;
        char *_base;
        int   _flag;
        int   _charbuf;
        int   _bufsiz;
        CRITICAL_SECTION lock;
        // moved here from the ioinfo struct
        HANDLE osfhnd;  /* underlying OS file HANDLE */
        char   osfile;  /* attributes of file (e.g., open in text mode?) */
        char   peekch;  /* one byte lookahead reqd for CRLF translations */
        int    fd;      // used to figure out if this is one of the special stdin/out/err
    }
    _FILEX;
    typedef _FILEX FILEX;
#   define _FILEX_DEFINED
#endif  /* _FILEX_DEFINED */

int _lock_validate_str (_FILEX *stream);

int __cdecl _get_errno_from_oserr(unsigned long);

// Redefine these if you want to enable guarding stream the stream locks
#define __STREAM_TRY
#define __STREAM_FINALLY

// Current Number of entries supported in the array pointed to by __piob[].
// The array starts at INITIAL_NSTREAM in size & grows by increments of
// NSTREAM_INCREMENT. There is no static/fixed limit. Also it never shrinks.
extern int _nstream;
#define INITIAL_NSTREAM   3
#define NSTREAM_INCREMENT 3

// Special value interpreted by output functions to redirect to OutputDebugStringW
#define _CRT_DBGOUT_HANDLE ((HANDLE)(-2))

// Hack for supporting redirect to COM
extern HANDLE hCOMPort;

typedef BOOL (*tGetCommState) (HANDLE hFile, LPDCB lpDCB);
typedef BOOL (*tSetCommState) (HANDLE hFile, LPDCB lpDCB);
typedef BOOL (*tSetCommTimeouts) (HANDLE hFile, LPCOMMTIMEOUTS lpCommTimeouts);
typedef BOOL (*tGetCommTimeouts) (HANDLE hFile, LPCOMMTIMEOUTS lpCommTimeouts);

extern tGetCommState g_pfnGetCommState;
extern tSetCommState g_pfnSetCommState;
extern tSetCommTimeouts g_pfnSetCommTimeouts;
extern tGetCommTimeouts g_pfnGetCommTimeouts;

typedef HANDLE (*tActivateDeviceEx)(LPCWSTR lpszDevKey, LPCVOID lpRegEnts, DWORD cRegEnts, LPVOID lpvParam);
typedef BOOL (*tDeactivateDevice)(HANDLE hDevice);
typedef BOOL (*tGetDeviceInformationByDeviceHandle)(HANDLE hDevice, PDEVMGR_DEVICE_INFORMATION pdi);

// Control whether CRT APIs use pre-CE6 input validation and terminate
// on invalid parameters
extern BOOL g_fCrtLegacyInputValidation;

/*
 * Pointer to the array of pointers to FILEX/_FILEX structures that are used
 * to manage stdio-level files.
 */
extern FILEX** __piob;

// Don't use external (an inefficient!) stdin/stderr/stdout macros internally
#undef stdin
#undef stdout
#undef stderr
#define stdin  __piob[0]
#define stdout __piob[1]
#define stderr __piob[2]

// For componentization
typedef int (__cdecl *PFNFILBUF)(FILEX *str, int fWide);
typedef int (__cdecl *PFNFLSBUF)(int ch, FILEX *str, int fWide);
extern PFNFILBUF _pfnfilbuf2;
extern PFNFLSBUF _pfnflsbuf2;
#define _filbuf(_str)       (_pfnfilbuf2 ? _pfnfilbuf2(_str, FALSE) : (EOF))
#define _filwbuf(_str)      (_pfnfilbuf2 ? _pfnfilbuf2(_str, TRUE)  : (WEOF))
#define _flsbuf(_ch, _str)  (_pfnflsbuf2 ? _pfnflsbuf2(_ch,_str, FALSE) : (EOF))
// NOTE: _flswbuf should return WEOF on failure, but we can't due to BC.
// See Windows CE .NET 134886
#define _flswbuf(_ch, _str) (_pfnflsbuf2 ? _pfnflsbuf2(_ch,_str, TRUE)  : (EOF))

FILEX * __cdecl _getstream(void);
FILEX * __cdecl _openfile(const char *, const char *, int, FILEX *);
FILEX * __cdecl _wopenfile(const wchar_t *, const wchar_t *, int, FILEX *);
void __cdecl _getbuf(FILEX *);
void __cdecl _freebuf(FILEX *);
int __cdecl _stbuf(FILEX *);
void __cdecl _ftbuf(int, FILEX *);
int __cdecl _output(FILEX *, const char *, va_list, BOOL fFormatValidations);
int __cdecl _woutput(FILEX *, const wchar_t *, va_list, BOOL fFormatValidations);
int __cdecl _input(FILEX *, const unsigned char *, va_list, BOOL fSecureScanf);
int __cdecl _winput(FILEX *, const wchar_t *, va_list, BOOL fSecureScanf);
int __cdecl _flush(FILEX *);
void __cdecl _endstdio(void);

#ifdef CRT_UNICODE
#define _toutput   _woutput
#define _tinput    _winput
#else
#define _toutput   _output
#define _tinput    _input
#endif

int __cdecl _fseeki64(FILEX *, __int64, int);
int __cdecl _fseeki64_lk(FILEX *, __int64, int);
__int64 __cdecl _ftelli64(FILEX *);
__int64 __cdecl _ftelli64_lk(FILEX *);

#define _INTERNAL_BUFSIZ    4096
#define _SMALL_BUFSIZ       512

int __cdecl _fclose_lk(FILEX *);
int __cdecl _fflush_lk(FILEX *);
size_t __cdecl _fread_lk(void *, size_t, size_t, FILEX *);
int __cdecl _fseek_lk(FILEX *, long, int);
long __cdecl _ftell_lk(FILEX *);
size_t __cdecl _fwrite_lk(const void *, size_t, size_t, FILEX *);
int __cdecl _ungetc_lk(int, FILEX *);
_CRTIMP wint_t __cdecl _getwc_lk(FILEX *);
_CRTIMP int __cdecl _putwc_lk(wint_t, FILEX *);
_CRTIMP wint_t __cdecl _ungetwc_lk(wint_t, FILEX *);
char * __cdecl _wtmpnam_lk(char *);

#define _getc_lk(_stream)       (--(_stream)->_cnt >= 0 ? 0xff & *(_stream)->_ptr++ : _filbuf(_stream))
#define _putc_lk(_c,_stream)    (--(_stream)->_cnt >= 0 ? 0xff & (*(_stream)->_ptr++ = (char)(_c)) :  _flsbuf((_c),(_stream)))
#define _getchar_lk()           _getc_lk((FILEX*)stdin)
#define _putchar_lk(_c)         _putc_lk((_c),((FILEX*)stdout))
#define _getwchar_lk()          _getwc_lk((FILEX*)stdin)
#define _putwchar_lk(_c)        _putwc_lk((_c),((FILEX*)stdout))


// externs from revamped LOWIO
int __cdecl _close(FILEX* str);
long    __cdecl _lseek   (FILEX* str, long    pos, int mthd);
__int64 __cdecl _lseeki64(FILEX* str, __int64 pos, int mthd);
int     __cdecl _read    (FILEX* str, void    *buf,unsigned cnt);
int     __cdecl _write   (FILEX* str, const void *buf, unsigned cnt);

extern HANDLE __cdecl OpenStdConsole(int fh);

// Cleanup of startup & componentize
extern int fStdioInited;
int __cdecl InitStdio(void);
#define CheckStdioInit() (fStdioInited ? TRUE : InitStdio())


/* calls the currently installed new handler */
int __cdecl _callnewh(size_t) _THROW1(std::bad_alloc);
void * __cdecl _nh_malloc (size_t size, int nhFlag, int zeroint) _THROW1(std::bad_alloc);
extern int _newmode;    /* malloc new() handler mode */

/* internal helper functions for encoding and decoding pointers */
_CRTIMP void * __cdecl _encode_pointer(void *);
_CRTIMP void * __cdecl _encoded_null();
_CRTIMP void * __cdecl _decode_pointer(void *);

/* Macros to simplify the use of Secure CRT in the CRT itself.
 * We should use [_BEGIN/_END]_SECURE_CRT_DEPRECATION_DISABLE sparingly.
 */
#define _BEGIN_SECURE_CRT_DEPRECATION_DISABLE \
    __pragma(warning(push)) \
    __pragma(warning(disable:4996))

#define _END_SECURE_CRT_DEPRECATION_DISABLE \
    __pragma(warning(pop))

#define _ERRCHECK(e) \
    _INVOKE_WATSON_IF_ERROR(e)

#define _ERRCHECK_EINVAL(e) \
    _INVOKE_WATSON_IF_ONEOF(e, EINVAL, EINVAL)

#define _ERRCHECK_EINVAL_ERANGE(e) \
    _INVOKE_WATSON_IF_ONEOF(e, EINVAL, ERANGE)

/*
 * Type from ntdef.h
 */

typedef LONG NTSTATUS;

/*
 * Exception code used in _invalid_parameter
 */

#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)
#endif

/*
 * Validate functions
 */
#include <crtdbg.h> /* _ASSERTE */
#include <errno.h>

#define __STR2WSTR(str)    L##str

#define _STR2WSTR(str)     __STR2WSTR(str)

#define __FILEW__          _STR2WSTR(__FILE__)
#define __FUNCTIONW__      _STR2WSTR(__FUNCTION__)

/* We completely fill the buffer only in debug (see _SECURECRT__FILL_STRING
 * and _SECURECRT__FILL_BYTE macros).
 */
#if !defined(_SECURECRT_FILL_BUFFER)
#ifdef _DEBUG
#define _SECURECRT_FILL_BUFFER 1
#else
#define _SECURECRT_FILL_BUFFER 0
#endif
#endif

/* _invalid_parameter is already defined in safecrt.h and safecrt.lib */
_CRTIMP void __cdecl
_invalid_parameter(
    __in_z_opt const wchar_t *,
    __in_z_opt const wchar_t *,
    __in_z_opt const wchar_t *,
    unsigned int,
    uintptr_t
    );

_CRTIMP void __cdecl
__crt_unrecoverable_error(
    __in_z_opt const wchar_t *,
    __in_z_opt const wchar_t *,
    __in_z_opt const wchar_t *,
    unsigned int,
    uintptr_t
    );

_CRTIMP void __cdecl
_invalid_parameter_noinfo(
    void
    );

/* Unrecoverable if _ExpressionError is not 0; otherwise simply return _EspressionError */
__forceinline
void _unrecoverable_error_if_error(
    errno_t _ExpressionError,
    const wchar_t *_Expression,
    const wchar_t *_Function,
    const wchar_t *_File,
    unsigned int _Line,
    uintptr_t _Reserved
    )
{
    if (_ExpressionError == 0)
    {
        return;
    }
    __crt_unrecoverable_error(_Expression, _Function, _File, _Line, _Reserved);
}

/* Unrecoverable if _ExpressionError is not 0 and equal to _ErrorValue1 or _ErrorValue2; otherwise simply return _EspressionError */
__forceinline
errno_t _unrecoverable_error_if_oneof(
    errno_t _ExpressionError,
    errno_t _ErrorValue1,
    errno_t _ErrorValue2,
    const wchar_t *_Expression,
    const wchar_t *_Function,
    const wchar_t *_File,
    unsigned int _Line,
    uintptr_t _Reserved
    )
{
    if (_ExpressionError == 0 || (_ExpressionError != _ErrorValue1 && _ExpressionError != _ErrorValue2))
    {
        return _ExpressionError;
    }
    __crt_unrecoverable_error(_Expression, _Function, _File, _Line, _Reserved);
    return _ExpressionError;
}

/*
 * Assert in debug builds.
 * set errno and return
 *
 */
#ifdef _DEBUG
#define _CALL_INVALID_PARAMETER_FUNC(funcname, expr) funcname(expr, __FUNCTIONW__, __FILEW__, __LINE__, 0)
#define _INVOKE_WATSON_IF_ERROR(expr) _unrecoverable_error_if_error((expr), __STR2WSTR(#expr), __FUNCTIONW__, __FILEW__, __LINE__, 0)
#define _INVOKE_WATSON_IF_ONEOF(expr, errvalue1, errvalue2) _unrecoverable_error_if_oneof(expr, (errvalue1), (errvalue2), __STR2WSTR(#expr), __FUNCTIONW__, __FILEW__, __LINE__, 0)
#else
#define _CALL_INVALID_PARAMETER_FUNC(funcname, expr) funcname(NULL, NULL, NULL, 0, 0)
#define _INVOKE_WATSON_IF_ERROR(expr) _unrecoverable_error_if_error(expr, NULL, NULL, NULL, 0, 0)
#define _INVOKE_WATSON_IF_ONEOF(expr, errvalue1, errvalue2) _unrecoverable_error_if_oneof((expr), (errvalue1), (errvalue2), NULL, NULL, NULL, 0, 0)
#endif

#define _SET_ERRNO(_errnoval)  /* not supported on WinCE */

#define _INVALID_PARAMETER(expr) _CALL_INVALID_PARAMETER_FUNC(_invalid_parameter, expr)

#define _VALIDATE_RETURN_VOID( expr, errorcode )                               \
    {                                                                          \
        int _Expr_val=!!(expr);                                                \
        _ASSERT_EXPR( ( _Expr_val ), _CRT_WIDE(#expr) );                       \
        if ( !( _Expr_val ) )                                                  \
        {                                                                      \
            _SET_ERRNO(errorcode);                                             \
            _INVALID_PARAMETER(_CRT_WIDE(#expr));                              \
            return;                                                            \
        }                                                                      \
    }

/*
 * Assert in debug builds.
 * set errno and return value
 */

#ifndef _VALIDATE_RETURN
#define _VALIDATE_RETURN( expr, errorcode, retexpr )                           \
    {                                                                          \
        int _Expr_val=!!(expr);                                                \
        _ASSERT_EXPR( ( _Expr_val ), _CRT_WIDE(#expr) );                       \
        if ( !( _Expr_val ) )                                                  \
        {                                                                      \
            _SET_ERRNO(errorcode);                                             \
            _INVALID_PARAMETER(_CRT_WIDE(#expr) );                             \
            return ( retexpr );                                                \
        }                                                                      \
    }
#endif

#ifndef _VALIDATE_RETURN_NOEXC
#define _VALIDATE_RETURN_NOEXC( expr, errorcode, retexpr )                     \
    {                                                                          \
        if ( !(expr) )                                                         \
        {                                                                      \
            _SET_ERRNO(errorcode);                                             \
            return ( retexpr );                                                \
        }                                                                      \
    }
#endif

/*
 * Assert in debug builds.
 * set errno and set retval for later usage
 */

#define _VALIDATE_SETRET( expr, errorcode, retval, retexpr )                   \
    {                                                                          \
        int _Expr_val=!!(expr);                                                \
        _ASSERT_EXPR( ( _Expr_val ), _CRT_WIDE(#expr) );                       \
        if ( !( _Expr_val ) )                                                  \
        {                                                                      \
            _SET_ERRNO(errorcode);                                             \
            _INVALID_PARAMETER(_CRT_WIDE(#expr));                              \
            retval=( retexpr );                                                \
        }                                                                      \
    }

#define _CHECK_FH_RETURN( handle, errorcode, retexpr )                         \
    {                                                                          \
        if(handle == _NO_CONSOLE_FILENO)                                       \
        {                                                                      \
            _SET_ERRNO(errorcode);                                             \
            return ( retexpr );                                                \
        }                                                                      \
    }

/*
    We use _VALIDATE_STREAM_ANSI_RETURN to ensure that ANSI file operations(
    fprintf etc) aren't called on files opened as UNICODE. We do this check
    only if it's an actual FILE pointer & not a string
*/

#define _VALIDATE_STREAM_ANSI_RETURN( stream, errorcode, retexpr )                   \
    {                                                                                \
        FILE *_Stream=stream;                                                        \
        _VALIDATE_RETURN(( (_Stream->_flag & _IOSTRG) ||                             \
                       (   (_textmode_safe(_fileno(_Stream)) == __IOINFO_TM_ANSI) && \
                            !_tm_unicode_safe(_fileno(_Stream)))),                   \
                        errorcode, retexpr)                                          \
    }

/*
    We use _VALIDATE_STREAM_ANSI_SETRET to ensure that ANSI file operations(
    fprintf etc) aren't called on files opened as UNICODE. We do this check
    only if it's an actual FILE pointer & not a string. It doesn't actually return
    immediately
*/

#define _VALIDATE_STREAM_ANSI_SETRET( stream, errorcode, retval, retexpr)            \
    {                                                                                \
        FILE *_Stream=stream;                                                        \
        _VALIDATE_SETRET(( (_Stream->_flag & _IOSTRG) ||                             \
                       (   (_textmode_safe(_fileno(_Stream)) == __IOINFO_TM_ANSI) && \
                            !_tm_unicode_safe(_fileno(_Stream)))),                   \
                            errorcode, retval, retexpr)                              \
    }

/*
 * Assert in debug builds.
 * Return value (do not set errno)
 */

#define _VALIDATE_RETURN_NOERRNO( expr, retexpr )                              \
    {                                                                          \
        int _Expr_val=!!(expr);                                                \
        _ASSERT_EXPR( ( _Expr_val ), _CRT_WIDE(#expr) );                       \
        if ( !( _Expr_val ) )                                                  \
        {                                                                      \
            _INVALID_PARAMETER(_CRT_WIDE(#expr));                              \
            return ( retexpr );                                                \
        }                                                                      \
    }

/*
 * Assert in debug builds.
 * set errno and return errorcode
 */

#define _VALIDATE_RETURN_ERRCODE( expr, errorcode )                            \
    {                                                                          \
        int _Expr_val=!!(expr);                                                \
        _ASSERT_EXPR( ( _Expr_val ), _CRT_WIDE(#expr) );                       \
        if ( !( _Expr_val ) )                                                  \
        {                                                                      \
            _SET_ERRNO(errorcode);                                             \
            _INVALID_PARAMETER(_CRT_WIDE(#expr));                              \
            return ( errorcode );                                              \
        }                                                                      \
    }

#define _VALIDATE_RETURN_ERRCODE_NOEXC( expr, errorcode )                      \
    {                                                                          \
        if (!(expr))                                                           \
        {                                                                      \
            _SET_ERRNO(errorcode);                                             \
            return ( errorcode );                                              \
        }                                                                      \
    }

#ifdef _DEBUG
extern size_t __crtDebugFillThreshold;
#endif

#if !defined(_SECURECRT_FILL_BUFFER_THRESHOLD)
#ifdef _DEBUG
#define _SECURECRT_FILL_BUFFER_THRESHOLD __crtDebugFillThreshold
#else
#define _SECURECRT_FILL_BUFFER_THRESHOLD ((size_t)0)
#endif
#endif

#if _SECURECRT_FILL_BUFFER
#define _SECURECRT__FILL_STRING(_String, _Size, _Offset)                            \
    if ((_Size) != ((size_t)-1) && (_Size) != INT_MAX &&                            \
        ((size_t)(_Offset)) < (_Size))                                              \
    {                                                                               \
        memset((_String) + (_Offset),                                               \
            _SECURECRT_FILL_BUFFER_PATTERN,                                         \
            (_SECURECRT_FILL_BUFFER_THRESHOLD < ((size_t)((_Size) - (_Offset))) ?   \
                _SECURECRT_FILL_BUFFER_THRESHOLD :                                  \
                ((_Size) - (_Offset))) * sizeof(*(_String)));                       \
    }
#else
#define _SECURECRT__FILL_STRING(_String, _Size, _Offset)
#endif

#if _SECURECRT_FILL_BUFFER
#define _SECURECRT__FILL_BYTE(_Position)                \
    if (_SECURECRT_FILL_BUFFER_THRESHOLD > 0)           \
    {                                                   \
        (_Position) = _SECURECRT_FILL_BUFFER_PATTERN;   \
    }
#else
#define _SECURECRT__FILL_BYTE(_Position)
#endif

#ifdef __cplusplus
}
#endif

#endif  /* _INC_INTERNAL */
