//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

// Current Number of entries supported in the array pointed to by __piob[].
// The array starts at INITIAL_NSTREAM in size & grows by increments of
// NSTREAM_INCREMENT. There is no static/fixed limit. Also it never shrinks.
extern int _nstream;
#define INITIAL_NSTREAM   3
#define NSTREAM_INCREMENT 3

// Hack for supporting redirect to COM
extern HANDLE hCOMPort;

typedef BOOL (*tGetCommState) (HANDLE hFile, LPDCB lpDCB);
typedef BOOL (*tSetCommState) (HANDLE hFile, LPDCB lpDCB);
typedef BOOL (*tSetCommTimeouts) (HANDLE hFile, LPCOMMTIMEOUTS lpCommTimeouts);
typedef BOOL (*tGetCommTimeouts) (HANDLE hFile, LPCOMMTIMEOUTS lpCommTimeouts);

extern tGetCommState fGetCommState;
extern tSetCommState fSetCommState;
extern tSetCommTimeouts fSetCommTimeouts;
extern tGetCommTimeouts fGetCommTimeouts;

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
#define _flswbuf(_ch, _str) (_pfnflsbuf2 ? _pfnflsbuf2(_ch,_str, TRUE)  : (EOF))

FILEX * __cdecl _getstream(void);
FILEX * __cdecl _openfile(const char *, const char *, int, FILEX *);
FILEX * __cdecl _wopenfile(const wchar_t *, const wchar_t *, int, FILEX *);
void __cdecl _getbuf(FILEX *);
void __cdecl _freebuf(FILEX *);
int __cdecl _stbuf(FILEX *);
void __cdecl _ftbuf(int, FILEX *);
int __cdecl _output(FILEX *, const char *, va_list);
int __cdecl _woutput(FILEX *, const wchar_t *, va_list);
int __cdecl _input(FILEX *, const unsigned char *, va_list);
int __cdecl _winput(FILEX *, const wchar_t *, va_list);
int __cdecl _flush(FILEX *);
void __cdecl _endstdio(void);

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
int __cdecl _callnewh(size_t);
void * __cdecl _nh_malloc (size_t size, int nhFlag, int zeroint);
extern int _newmode;    /* malloc new() handler mode */


#ifdef __cplusplus
}
#endif

#endif  /* _INC_INTERNAL */
