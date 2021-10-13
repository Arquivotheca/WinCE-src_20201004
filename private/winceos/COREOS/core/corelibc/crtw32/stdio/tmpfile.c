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
*tmpfile.c - create unique file name or file
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines tmpnam() and tmpfile().
*
*Revision History:
*       ??-??-??  TC    initial version
*       04-17-86  JMB   tmpnam - brought semantics in line with System V
*                       definition as follows: 1) if tmpnam paramter is NULL,
*                       store name in static buffer (do NOT use malloc); (2)
*                       use P_tmpdir as the directory prefix to the temp file
*                       name (do NOT use current working directory)
*       05-26-87  JCR   fixed bug where tmpnam was modifying errno
*       08-10-87  JCR   Added code to support P_tmpdir with or without trailing
*                       '\'.
*       11-09-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       01-22-88  JCR   Added per thread static namebuf area (mthread update)
*       05-27-88  PHG   Merged DLL/normal versions
*       11-14-88  GJF   _openfile() now takes a file sharing flag, also some
*                       cleanup (now specific to the 386)
*       06-06-89  JCR   386 mthread support
*       11-28-89  JCR   Added check to _tmpnam so it can't loop forever
*       02-16-90  GJF   Fixed copyright and indents
*       03-19-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       03-26-90  GJF   Added #include <io.h>.
*       10-03-90  GJF   New-style function declarators.
*       01-21-91  GJF   ANSI naming.
*       07-22-91  GJF   Multi-thread support for Win32 [_WIN32_].
*       03-17-92  GJF   Completely rewrote Win32 version.
*       03-27-92  DJM   POSIX support.
*       05-02-92  SRW   Use _O_TEMPORARY flag for tmpfile routine.
*       05-04-92  GJF   Force cinittmp.obj in for Win32.
*       08-26-92  GJF   Fixed POSIX build.
*       08-28-92  GJF   Oops, forgot about getpid...
*       11-06-92  GJF   Use '/' for POSIX, '\\' otherwise, as the path
*                       separator. Also, backed out JHavens' update of 6-14,
*                       which was itself a bug (albeit a less serious one).
*       02-26-93  GJF   Put in per-thread buffers, purged Cruiser support.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-07-93  SKS   Replace access() with ANSI-conforming _access()
*       04-22-93  GJF   Fixed bug in multi-thread - multiple threads calling
*                       tmpnam would get the same names. Also, went to static
*                       namebufX buffers since failing due to a failed malloc
*                       would violate ANSI.
*       04-29-93  GJF   Multi-thread bug in tmpnam() - forgot to copy the
*                       generated name to the per-thread buffer.
*       12-07-93  CFW   Wide char enable.
*       04-01-94  GJF   #ifdef-ed out __inc_tmpoff for msvcrt*.dll, it's
*                       unnecessary.
*       04-22-94  GJF   Made definitions of namebuf0 and namebuf1 conditional
*                       on DLL_FOR_WIN32S.
*       01-10-95  CFW   Debug CRT allocs.
*       01-18-95  GJF   Must replace _tcsdup with _malloc_crt/_tcscpy for
*                       _DEBUG build.
*       02-21-95  GJF   Appended Mac version of source file (somewhat cleaned
*                       up), with appropriate #ifdef-s. Also replaced WPRFLAG
*                       with _UNICODE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       08-08-97  GJF   Removed initialized-but-unused local variable from 
*                       tmpfile(). Also, detab-ed.
*       03-03-98  GJF   Exception-safe locking.
*       05-13-99  PML   Remove Win32s
*       05-17-99  PML   Remove all Macintosh support.
*       10-06-99  PML   Set errno EMFILE when out of streams.
*       07-03-01  BWT   Fix genfname to use the correct buffer size to encode a dword (7 bytes + NULL).
*       10-19-01  BWT   If malloc_crt fails when allocating the return buffer in tmpnam, 
*                       return NULL and set errno to enomem. We've already set the 
*                       precedence that null can be returned if we're unable to get 
*                       the thread lock.
*       12-11-01  BWT   Replace _getptd with _getptd_noexit - failure to get a
*                       unique tmpname buffer isn't fatal - return NULL/ENOMEM
*                       and let the caller deal with it.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-20-03  SJ    errno shouldn't be set to 0 on success & VSW#179008
*       03-08-04  MSL   Fixed errno preservation in success case (taccess usage)
*                       VSW#217086
*       08-16-04  SJ    VSW#340379 - Errno was getting overwritten in the 
*                       failure case.
*       09-28-04  AC    Modified call to _sopen_s.
*       04-01-05  MSL   Integer overflow protection
*       07-18-07  ATC   Fixing OACR warnings
*
*******************************************************************************/

#include <cruntime.h>
#ifdef  _POSIX_
#include <unistd.h>
#endif
#include <errno.h>
#include <process.h>
#include <fcntl.h>
#include <io.h>
#include <mtdll.h>
#include <share.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <file2.h>
#include <internal.h>
#include <tchar.h>
#include <dbgint.h>

/*
 * Buffers used by tmpnam() and tmpfile() to build filenames.
 * (Taken from stdio.h)
 * L_tmpnam(_s) = length of string _P_tmpdir
 *            + 1 if _P_tmpdir does not end in "/" or "\", else 0
 *            + 12 (for the filename string)
 *            + 1 (for the null terminator)
 * L_tmpnam_s = length of string _P_tmpdir
 *            + 1 if _P_tmpdir does not end in "/" or "\", else 0
 *            + 16 (for the filename string)
 *            + 1 (for the null terminator)
 *
 *
 *  #define L_tmpnam   (sizeof(_P_tmpdir) + 12)
 *  #define L_tmpnam_s (sizeof(_P_tmpdir) + 16)
 *
 *
 *  The 12/16 is calculated as follows
 *  The tmpname(_s) strings look like "Prefix1stPart.2ndPart"
 *  Prefix is "s" - 1 character long.
 *  1st Part is generated by ProcessID converted to string by _ultot 
 *      Even for max process id == UINT_MAX, the resultant string is "3vvvvvv"
 *      i.e. 7 characters long
 *  1 character for the "."
 *  This gives a subtotal of 1 + 7 + 1 = 9 for the "Prefix1stPart."
 *  
 *  The 2ndPart is generated by passing a number to _ultot.
 *  In tmpnam, the max number passed is SHRT_MAX, generating the string "vvv".
 *  i.e. 3 characters long.
 *  In tmpnam_s, the max number passed is INT_MAX, generating "1vvvvvv"
 *  i.e. 7 characters long.
 *  
 *  L_tmpnam   = sizeof(_P_tmpdir + 9 + 3)
 *  L_tmpnam_s = sizeof(_P_tmpdir + 9 + 7)
 *  
 */
 
static _TSCHAR tmpnam_buf[L_tmpnam] = { 0 };      /* used by tmpnam()  */
static _TSCHAR tmpfile_buf[MAX_PATH + 12] = { 0 };      /* used by tmpfile() */
static _TSCHAR tmpnam_s_buf[L_tmpnam_s] = { 0 };      /* used by tmpnam_s() */

#define _TMPNAM_BUFFER 0
#define _TMPFILE_BUFFER 1
#define _TMPNAM_S_BUFFER 2

/*
 * Initializing function for tmpnam_buf and tmpfile_buf.
 */
#ifdef _UNICODE
static int __cdecl winit_namebuf(int);
#else
static int __cdecl init_namebuf(int);
#endif

/*
 * Generator function that produces temporary filenames
 */
#ifdef _UNICODE
static int __cdecl wgenfname(wchar_t *, size_t, unsigned long);
#else
static int __cdecl genfname(char *, size_t, unsigned long);
#endif


errno_t _ttmpnam_helper (
        _TSCHAR *s, size_t sz, int buffer_no, unsigned long tmp_max, _TSCHAR **ret
        )

{
        _TSCHAR *pfnam = NULL;
        size_t pfnameSize = 0;
        errno_t retval = 0;
        errno_t saved_errno=errno;
                
        _ptiddata ptd;

        if ( !_mtinitlocknum( _TMPNAM_LOCK ))
        {
                *ret = NULL;
                return errno;
        }

        _mlock(_TMPNAM_LOCK);

        __try {

        /* buffer_no is either _TMPNAM_BUFFER or _TMPNAM_S_BUFFER
        It's never _TMPFILE_BUFFER */
        
        if (buffer_no == _TMPNAM_BUFFER)
        {
            pfnam = tmpnam_buf;
            pfnameSize = _countof(tmpnam_buf);
        }
        else
        {
            pfnam = tmpnam_s_buf;
            pfnameSize = _countof(tmpnam_s_buf);
        }

        /*
         * Initialize tmpnam_buf, if needed. Otherwise, call genfname() to
         * generate the next filename.
         */
        if ( *pfnam == 0 ) {
#ifdef _UNICODE
                winit_namebuf(buffer_no);
#else
                init_namebuf(buffer_no);
#endif
        }
#ifdef _UNICODE
        else if ( wgenfname(pfnam, pfnameSize, tmp_max) )
#else
        else if ( genfname(pfnam, pfnameSize, tmp_max) )
#endif
                goto tmpnam_err;

        /*
         * Generate a filename that doesn't already exist.
         */
        while ( _taccess_s(pfnam, 0) == 0 )
#ifdef _UNICODE
                if ( wgenfname(pfnam, pfnameSize, tmp_max) )
#else
                if ( genfname(pfnam, pfnameSize, tmp_max) )
#endif
                        goto tmpnam_err;

        /*
         * Filename has successfully been generated.
         */
        if ( s == NULL )
        {        

                /* Will never come here for tmpnam_s */
                _ASSERTE(pfnam == tmpnam_buf);
                /*
                 * Use a per-thread buffer to hold the generated file name.
                 */
                ptd = _getptd_noexit();
                if (!ptd) {
                    retval = ENOMEM;
                    goto tmpnam_err;
                }
#ifdef _UNICODE
                if ( (ptd->_wnamebuf0 != NULL) || ((ptd->_wnamebuf0 =
                      _calloc_crt(L_tmpnam, sizeof(wchar_t))) != NULL) )
                {
                        s = ptd->_wnamebuf0;
                        _ERRCHECK(wcscpy_s(s, L_tmpnam, pfnam));
                }
#else
                if ( (ptd->_namebuf0 != NULL) || ((ptd->_namebuf0 =
                      _malloc_crt(L_tmpnam)) != NULL) )
                {
                        s = ptd->_namebuf0;
                        _ERRCHECK(strcpy_s(s, L_tmpnam, pfnam));
                }
#endif
                else 
                {
                        retval = ENOMEM;
                        goto tmpnam_err;
                }
        }
        else
        {
            if((buffer_no != _TMPNAM_BUFFER) && (_tcslen(pfnam) >= sz))
            {
                retval = ERANGE;
                
                if(sz != 0)
                    s[0] = 0;
                    
                goto tmpnam_err;
            }                
               
            _ERRCHECK(_tcscpy_s(s, sz, pfnam));
        }                


        /*
         * All errors come here.
         */
tmpnam_err:;
        }
        __finally {
                _munlock(_TMPNAM_LOCK);
        }
        *ret = s;
        if (retval != 0)
        {
            errno = retval;
        }
        else
        {
            errno = saved_errno;
        }
        return retval ;
}


errno_t __cdecl _ttmpnam_s(_TSCHAR * s, size_t sz)
{
    _TSCHAR * ret; /* Not used by tmpnam_s */

    _VALIDATE_RETURN_ERRCODE( (s != NULL), EINVAL);
    
    return _ttmpnam_helper(s, sz, _TMPNAM_S_BUFFER, _TMP_MAX_S, &ret);
}

/***
*_TSCHAR *tmpnam(_TSCHAR *s) - generate temp file name
*
*Purpose:
*       Creates a file name that is unique in the directory specified by
*       _P_tmpdir in stdio.h.  Places file name in string passed by user or
*       in static mem if pass NULL.
*
*Entry:
*       _TSCHAR *s - ptr to place to put temp name
*
*Exit:
*       returns pointer to constructed file name (s or address of static mem)
*       returns NULL if fails
*
*Exceptions:
*
*******************************************************************************/

_TSCHAR * __cdecl _ttmpnam (
        _TSCHAR *s
        )
{      
    _TSCHAR * ret = NULL;
    _ttmpnam_helper(s, (size_t)-1, _TMPNAM_BUFFER, TMP_MAX, &ret) ;
    return ret;
}


#ifndef _UNICODE

errno_t __cdecl _tmpfile_helper (FILE ** pFile, int shflag)
{
        FILE *stream;
        int fh;
        errno_t retval = 0;
        errno_t save_errno;

        int stream_lock_held = 0;

        _VALIDATE_RETURN_ERRCODE( (pFile != NULL), EINVAL);
        *pFile = NULL;

        if ( !_mtinitlocknum( _TMPNAM_LOCK ))
        {
                return errno;
        }

        _mlock(_TMPNAM_LOCK);

        __try {

        /*
         * Initialize tmpfile_buf, if needed. Otherwise, call genfname() to
         * generate the next filename.
         */
        if ( *tmpfile_buf == 0 ) {
            if (init_namebuf(_TMPFILE_BUFFER) != 0)
                goto tmpfile_err;
        }
        else if ( genfname(tmpfile_buf, _countof(tmpfile_buf), _TMP_MAX_S) )
                goto tmpfile_err;

        /*
         * Get a free stream.
         *
         * Note: In multi-thread models, the stream obtained below is locked!
         */
        if ( (stream = _getstream()) == NULL ) {
                retval = EMFILE;
                goto tmpfile_err;
        }

        stream_lock_held = 1;

		/*
         * Create a temporary file.
         *
         * Note: The loop below will only create a new file. It will NOT
         * open and truncate an existing file. Either behavior is probably
         * legal under ANSI (4.9.4.3 says tmpfile "creates" the file, but
         * also says it is opened with mode "wb+"). However, the behavior
         * implemented below is compatible with prior versions of MS-C and
         * makes error checking easier.
         */
        save_errno = errno;
        errno = 0;
#ifdef  _POSIX_
        while ( ((fh = open(tmpfile_buf,
                             O_CREAT | O_EXCL | O_RDWR,
                             S_IRUSR | S_IWUSR
                             ))
            == -1) && (errno == EEXIST) )
#else
        while ( (_sopen_s(&fh, tmpfile_buf,
                              _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY |
                                _O_TEMPORARY,
                              shflag,
                              _S_IREAD | _S_IWRITE
                             ) == EEXIST) )
#endif
        {
                if ( genfname(tmpfile_buf, _countof(tmpfile_buf), _TMP_MAX_S) )
                        break;
        }

        if(errno == 0)
        {
            errno = save_errno;
        }

        /*
         * Check that the loop above did indeed create a temporary
         * file.
         */
        if ( fh == -1 )
                goto tmpfile_err;

        /*
         * Initialize stream
         */
#ifdef  _DEBUG
        if ( (stream->_tmpfname = _calloc_crt( (_tcslen( tmpfile_buf ) + 1), sizeof(_TSCHAR) )) == NULL )
#else   /* ndef _DEBUG */
        if ( (stream->_tmpfname = _tcsdup( tmpfile_buf )) == NULL )
#endif  /* _DEBUG */
        {
                /* close the file, then branch to error handling */
#ifdef  _POSIX_
                close(fh);
#else
                _close(fh);
#endif
                goto tmpfile_err;
        }
#ifdef  _DEBUG
        _ERRCHECK(_tcscpy_s( stream->_tmpfname, _tcslen( tmpfile_buf ) + 1, tmpfile_buf ));
#endif  /* _DEBUG */
        stream->_cnt = 0;
        stream->_base = stream->_ptr = NULL;
        stream->_flag = _commode | _IORW;
        stream->_file = fh;

        *pFile = stream;

        /*
         * All errors branch to the label below.
         */
tmpfile_err:;
		}
        __finally {
                if ( stream_lock_held )
                        _unlock_str(stream);
                _munlock(_TMPNAM_LOCK);
        }

        if (retval != 0)
        {
            errno = retval;
        }
        return retval ;
}

/***
*FILE *tmpfile() - create a temporary file
*
*Purpose:
*       Creates a temporary file with the file mode "w+b".  The file
*       will be automatically deleted when closed or the program terminates
*       normally.
*
*Entry:
*       None.
*
*Exit:
*       Returns stream pointer to opened file.
*       Returns NULL if fails
*
*Exceptions:
*
*******************************************************************************/

FILE * __cdecl tmpfile (void)
{
    FILE * fp = NULL;
    _tmpfile_helper(&fp, _SH_DENYNO);
    return fp;
}

/***
*errno_t *tmpfile_s - create a temporary file
*
*Purpose:
*       Creates a temporary file with the file mode "w+b".  The file
*       will be automatically deleted when closed or the program terminates
*       normally. Similiar to tmpfile, except that it opens the tmpfile in
*       _SH_DENYRW share mode.
*
*Entry:
*       FILE ** pFile - in param to fill the FILE * to.
*
*Exit:
*       returns 0 on success & sets pfile
*       returns errno_t on failure.
*       On success, fills in the FILE pointer into the in param.
*
*Exceptions:
*
*******************************************************************************/

errno_t __cdecl tmpfile_s (FILE ** pFile)
{
    return _tmpfile_helper(pFile, _SH_DENYRW);
}

#endif /* _UNICODE */

/***
*static void init_namebuf(flag) - initializes the namebuf arrays
*
*Purpose:
*       Called once each for tmpnam_buf and tmpfile_buf, to initialize
*       them.
*
*Entry:
*       int flag            - flag set to 0 if tmpnam_buf is to be initialized,
*                             set to 1 if tmpfile_buf is to be initialized.
*                             set to 2 if tmpnam_s_buf is to be initialized.
*
*Returns:                   0 if no error; non-0 otherwise.
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

#ifdef _UNICODE
static int __cdecl winit_namebuf(
#else
static int __cdecl init_namebuf(
#endif
        int flag
        )
{
        _TSCHAR *p, *q;
        _TSCHAR tempPathBuffer[MAX_PATH];
        DWORD retVal = 0;
        size_t size = 0;

        switch(flag)
        {
            case 0 : /* _TMPNAM_BUFFER */
                p = tmpnam_buf;
                size = _countof(tmpnam_buf);
                break;
                
            case 1 : /* _TMPFILE_BUFFER */
                p = tmpfile_buf;
                size = _countof(tmpfile_buf);
                break;

            case 2 : /* _TMPNAM_S_BUFFER */
                p = tmpnam_s_buf;
                size = _countof(tmpnam_s_buf);
                break;

        }

        /*
         * Put in the path prefix. Make sure it ends with a slash or
         * backslash character.
         */
        if (flag == _TMPFILE_BUFFER)
        {
            retVal = GetTempPath(MAX_PATH, tempPathBuffer);
            if ( retVal == 0 || retVal > MAX_PATH)
                return -1;
            
#ifdef _UNICODE
            _ERRCHECK(wcscpy_s(p, retVal+1, tempPathBuffer));
#else
            _ERRCHECK(strcpy_s(p, retVal+1, tempPathBuffer));
#endif
            q = p + retVal - 1;      /* same as p + _tcslen(tempPathBuffer) */
        }
        else
        {
#ifdef _UNICODE
            _ERRCHECK(wcscpy_s(p, size, _wP_tmpdir));
#else
            _ERRCHECK(strcpy_s(p, size, _P_tmpdir));
#endif
            q = p + sizeof(_P_tmpdir) - 1;      /* same as p + _tcslen(p) */
        }        

#ifdef _POSIX_
        if  ( *(q - 1) != _T('/') )
                *(q++) = _T('/');
#else
        if  ( (*(q - 1) != _T('\\')) && (*(q - 1) != _T('/')) )
                *(q++) = _T('\\');
#endif

        /*
         * Append the leading character of the filename.
         */
        if ( flag == _TMPFILE_BUFFER )
                /* for tmpfile() */
                *(q++) = _T('t');
        else
                /* for tmpnam() & _tmpnam_s */
                *(q++) = _T('s');

        /*
         * Append the process id, encoded in base 32. Note this makes
         * p back into a string again (i.e., terminated by a '\0').
         */
#ifdef  _POSIX_
        _ERRCHECK(_ultot_s((unsigned long)getpid(), q, size - (q - p), 32));
#else
        _ERRCHECK(_ultot_s((unsigned long)_getpid(), q, size - (q - p), 32));
#endif
        _ERRCHECK(_tcscat_s(p, size, _T(".")));

        return 0;
}


/***
*static int genfname(_TSCHAR *fname) -
*
*Purpose:
*
*Entry:
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

#ifdef _UNICODE
static int __cdecl wgenfname (
#else
static int __cdecl genfname (
#endif
        _TSCHAR *fname, size_t fnameSize, unsigned long tmp_max
        )
{
        _TSCHAR *p;
        _TSCHAR pext[8];        // 7 positions for base 32 ulong + null terminator
        unsigned long extnum;

        p = _tcsrchr(fname, _T('.'));

        p++;

        _VALIDATE_RETURN_NOERRNO(p >= fname && fnameSize > (size_t)(p-fname), -1);


        if ( (extnum = _tcstoul(p, NULL, 32) + 1) >= tmp_max )
                return -1;

        _ERRCHECK(_ultot_s(extnum, pext, _countof(pext), 32));
        _ERRCHECK(_tcscpy_s(p, fnameSize - (p - fname), pext));

        return 0;
}

#if     !defined(_UNICODE) && !defined(CRTDLL)

/***
*void __inc_tmpoff(void) - force external reference for _tmpoff
*
*Purpose:
*       Forces an external reference to be generate for _tmpoff, which is
*       is defined in cinittmp.obj. This has the forces cinittmp.obj to be
*       pulled in, making a call to rmtmp part of the termination.
*
*Entry:
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/


extern int _tmpoff;

void __inc_tmpoff(
        void
        )
{
        _tmpoff++;
}

#endif  /* _UNICODE */
