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
*fopen.c - open a file
*
*
*Purpose:
*   defines fopen() and _fsopen() - open a file as a stream and open a file
*   with a specified sharing mode as a stream
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <share.h>
#include <dbgint.h>
#include <internal.h>
#include <mtdll.h>
#include <file2.h>
#include <msdos.h>

/***
*int _close(str) - close a file handle
*
*Purpose:
*   Closes the file associated with the file handle fh.
*
*******************************************************************************/

int __cdecl _close (
    FILEX* str
    )
{
    HANDLE h = _osfhndstr(str);
    
    if((h == _CRT_DBGOUT_HANDLE) || CloseHandle(h)) {
        _osfhndstr(str) = INVALID_HANDLE_VALUE; // clear osfhnd
        _osfilestr(str) = 0;                    // clear file flags
        return 0;
    } else {
        return -1;
    }
}

/***
*int fclose(stream) - close a stream
*
*Purpose:
*   Flushes and closes a stream and frees any buffer associated
*   with that stream, unless it was set with setbuf.
*
*Entry:
*   FILEX *stream - stream to close
*
*Exit:
*   returns 0 if OK, EOF if fails (can't _flush, not a FILEX, not open, etc.)
*   closes the file -- affecting FILEX structure
*
*Exceptions:
*
*******************************************************************************/

int __cdecl fclose (
    FILEX *stream
    )
{
    int result;

    _VALIDATE_RETURN((stream != NULL), EINVAL, EOF);

    /* Stream is a real file. */
    if (!_lock_validate_str(stream))
        return EOF;

    __STREAM_TRY
    {
        result = _fclose_lk(stream);
    }
    __STREAM_FINALLY
    {
        _unlock_str(stream);
    }

    return result;
}

int __cdecl _fclose_lk (FILEX *stream)
{
    int result = EOF;

    /* If stream is a string, simply return EOF */
    if (stream->_flag & _IOSTRG)
        result =  EOF;
    else if (inuse(stream))
    {
        /* Stream is in use:
               (1) flush stream
               (2) free the buffer
               (3) close the file
               (4) delete the file if temporary
        */

        result = _flush(stream);
        _freebuf(stream);
        _close(stream);
        stream->_flag = 0;  // clear FILEX flags
    }
    return result;
}


/***
*FILEX *fopen(file, mode) - open a file
*
*Purpose:
*   Opens the file specified as a stream.  mode determines file mode:
*   "r": read   "w": write  "a": append
*   "r+": read/write        "w+": open empty for read/write
*   "a+": read/append
*   Append "t" or "b" for text and binary mode
*
*   Mode can also be:
*        looking for exactly one of {rwa}, at most one '+',
*   at most one of {tb}, at most one of {cn}, at most one of {SR}, at most
*   one 'T', and at most one 'D'.
*
*Entry:
*   char *file - file name to open
*   char *mode - mode of file access
*
*Exit:
*   returns pointer to stream
*   returns NULL if fails
*
*Exceptions:
*
*******************************************************************************/
static errno_t _wfopen_int (
    FILE** ppfile,
    const WCHAR *filename,
    const WCHAR *mode,
    HANDLE hFile,
    FILEX* stream,
    int shflag
    )
{
    errno_t errret = EINVAL;
    int streamflag = 0;     // FILEX->fileflags
    BYTE fileflags = FTEXT; // FILEX->osfile flags. Start with FTEXT by default
    DWORD fileaccess = 0;   // OS file access (requested)
    DWORD filecreate = 0;   // OS method of opening/creating
    BOOL  fAppend = 0;
    DWORD fileshare = 0;
    DWORD fileattrib = FILE_ATTRIBUTE_NORMAL; // always normal attributes

    _VALIDATE_RETURN_ERRCODE(((hFile != INVALID_HANDLE_VALUE) || (filename != NULL)), EINVAL);
    _VALIDATE_RETURN_ERRCODE((mode != NULL), EINVAL);
    _VALIDATE_RETURN_ERRCODE((*mode != '\0'), EINVAL);

    _ASSERTE(ppfile != NULL);
    *ppfile = NULL;

    if((hFile == INVALID_HANDLE_VALUE) && (!filename || filename[0] == L'\0'))
    {
        return EINVAL;
    }

#if 0 // Reenable this if the above validates are removed
    if(!mode || mode[0] == L'\0')
    {
        return EINVAL;
    }
#endif

    if(!CheckStdioInit())
    {
        return ENOMEM;
    }

    // If we didnt get passed stream (by freopen) then get a free stream
    // [NOTE: _getstream() returns a locked stream & freopen passes us a locked stream]

    if(NULL == stream)
    {
        if ((stream = _getstream()) == NULL)
        {
            return EMFILE;
        }
    }

    // Need to get rid of the gotos before we can turn this into a guarded
    // region
    // __STREAM_TRY

    /*
     * Parse the user's specification string and set flags in
     *  (1) fileaccess, fileshare, filecreate, fileattrib - system call params
     *  (2) fileflags - osfile flags word
     *  (3) streamflag - stream handle flags word.
     */
    // First mode character must be 'r', 'w', or 'a'.
    switch (*mode)
    {
        case TEXT('r'):
            fileaccess = GENERIC_READ;
            filecreate = OPEN_EXISTING;
            streamflag |= _IOREAD;
            break;

        case TEXT('w'):
            fileaccess = GENERIC_WRITE;
            filecreate = CREATE_ALWAYS;
            streamflag |= _IOWRT;
            break;

        case TEXT('a'):
            fileaccess = GENERIC_WRITE;
            filecreate = OPEN_ALWAYS;
            fileflags |= FAPPEND;
            streamflag |= _IOWRT;
            fAppend = TRUE;
            break;

        default:
            goto fail2;
    }

    /*
     * There can be up to three more optional mode characters:
     *  (1) One of 't' and 'b' and
     *  (2) One of 'c' and 'n' and
     *  (3) One of 'S' or 'R' and optionall 'D, 'T'
     *  (4) and an optional +
     */
    for(mode++; *mode; mode++)
    {
        switch(*mode)
        {
            case TEXT('+'):
                fileaccess = GENERIC_READ | GENERIC_WRITE;
                streamflag &= ~(_IOREAD | _IOWRT);
                streamflag |= _IORW;
                break;

            case TEXT('b'):
                fileflags &= (~FTEXT);
                break;

            case TEXT('t'):
                fileflags |= FTEXT;
                break;

            case TEXT('c'):
                streamflag |= _IOCOMMIT;
                break;

            case TEXT('n'):
                streamflag &= (~_IOCOMMIT);
                break;

            case TEXT('S'):
                fileattrib |= FILE_FLAG_SEQUENTIAL_SCAN;
                break;

            case TEXT('R'):
                fileattrib |= FILE_FLAG_RANDOM_ACCESS;
                break;

            case TEXT('T'):
                fileattrib |= FILE_ATTRIBUTE_TEMPORARY;
                break;

            case TEXT('D'):
                fileattrib |= FILE_FLAG_DELETE_ON_CLOSE;
                fileaccess |= DELETE;
                break;
        }
    }

    /*
     * decode sharing flags
     */
    switch (shflag)
    {
        case _SH_DENYRW:        /* exclusive access */
            fileshare = 0L;
            break;

        case _SH_DENYWR:        /* share read access */
            fileshare = FILE_SHARE_READ;
            break;

        case _SH_DENYRD:        /* share write access */
            fileshare = FILE_SHARE_WRITE;
            break;

        case _SH_DENYNO:        /* share read and write access */
            fileshare = FILE_SHARE_READ | FILE_SHARE_WRITE;
            break;

        case _SH_SECURE:       /* share read access only if read-only */
            if (fileaccess == GENERIC_READ)
            {
                fileshare = FILE_SHARE_READ;
            }
            else
            {
                fileshare = 0L;
            }
            break;

        default:                /* error, bad shflag */
            _ASSERTE(FALSE);
    }

    // if we got filename, try to open it. Else we got an HFile -- just use that
    if(filename)
    {
        // try to open/create the file
        hFile = CreateFileW(filename, fileaccess, fileshare, NULL, filecreate, fileattrib, NULL);
        if(hFile == INVALID_HANDLE_VALUE)
        {
            errret = _get_errno_from_oserr(GetLastError());
            goto fail2;
        }
    }

    // the file is open. now, set the info in FILEX struct
    _osfhndstr(stream) = hFile;

    // check if it is a device
    if(filename && lstrlen(filename) == 5 && filename[4] == ':')
    {
        fileflags |= FDEV;
    }

     // mark the handle as open. store flags gathered so far in _osfile
    fileflags |= FOPEN;
    _osfilestr(stream) = fileflags;

    // Munge ^Z if neccesary (must be regular Text file in read&write mode)
    if (!(fileflags & (FDEV|FPIPE)) && (fileflags & FTEXT) && (streamflag & _IORW))
    {
        /* We have a text mode file.  If it ends in CTRL-Z, we wish to
           remove the CTRL-Z character, so that appending will work.
           We do this by seeking to the end of file, reading the last
           byte, and shortening the file if it is a CTRL-Z. */

        /*
         * NOTE: In WIN32 this code was relying on a GLE of ERROR_NEGATIVE_SEEK
         * error from SetFilePointer API if the file length was 0, but on CE
         * we don't get that error code. So check the file size first.
         */

        if(GetFileSize(_osfhndstr(stream), NULL))
        {
            WCHAR ch = 0;

            if (_lseek(stream, -1, SEEK_END) == -1)
            {
                // errret set at fail label
                goto fail;
            }

            /* Seek was OK, read the last char in file. The last
               char is a CTRL-Z if and only if _read returns 0
               and ch ends up with a CTRL-Z. */
            if (_read(stream, &ch, 1) == 0 && ch == 26)
            {
                // Last char was a ^Z. Clear it.
                if((_lseek(stream, -1, SEEK_END) == -1) || !SetEndOfFile(_osfhndstr(stream)))
                {
                    // errret set at fail label
                    goto fail;
                }
            }

            /* now rewind the file to the beginning */
            if (_lseek(stream, 0, SEEK_SET) == -1)
            {
                // errret set at fail label
                goto fail;
            }
        }
    }
    // Set FAPPEND flag if appropriate. Don't do this for devices or pipes.
    if(fAppend  && !(fileflags & (FDEV|FPIPE)))
    {
        _osfilestr(stream) |= FAPPEND;
    }

    /* Set up the stream data base. */
    /* Init pointers */
    stream->_flag = streamflag;
    stream->_cnt = 0;
    stream->_base = stream->_ptr = NULL;

    // End of guarded region
    // STREAM_FINALLY

    // unlock stream and return
    _unlock_str(stream);

    // no error
    *ppfile = (FILE*)stream;
    return 0;

fail:
    errret = _get_errno_from_oserr(GetLastError());
    _close(stream);
    // fall through
fail2:
    // unlock stream and return NULL
    _unlock_str(stream);
    return errret;
}

FILE* __cdecl _wfopen(
    const WCHAR *filename,
    const WCHAR *mode
    )
{
    FILE* pfret = NULL;
    _wfopen_int(&pfret, filename, mode, INVALID_HANDLE_VALUE, NULL, _SH_DENYNO);
    return pfret;
}

static errno_t _fopen_int (
    FILE** ppfile,
    const char *filename,
    const char *mode,
    int shflag
    )
{
    errno_t errret = EINVAL;
    WCHAR *wszName, *wszMode;
    int cchName, cchMode;

    _ASSERTE(ppfile);
    *ppfile = NULL;

    _VALIDATE_RETURN_ERRCODE((filename != NULL), EINVAL);
    _VALIDATE_RETURN_ERRCODE((mode != NULL), EINVAL);

    cchName = strlen(filename) + 1;
    cchMode = strlen(mode) + 1;

    wszName = malloc(sizeof(WCHAR) * cchName);
    wszMode = malloc(sizeof(WCHAR) * cchMode);

    __STREAM_TRY
    {
        if (wszName && wszMode)
        {
            if(MultiByteToWideChar(CP_ACP, 0, filename, -1, wszName, cchName) &&
               MultiByteToWideChar(CP_ACP, 0, mode, -1, wszMode, cchMode))
            {
                errret = _wfopen_int(ppfile, wszName, wszMode, INVALID_HANDLE_VALUE, NULL, shflag);
            }
        }
    }
    __STREAM_FINALLY
    {
        if (wszName) free(wszName);
        if (wszName) free(wszMode);
    }

    return errret;
}

FILE* __cdecl fopen (
    const char *filename,
    const char *mode
    )
{
    FILE* pfret;
    _fopen_int(&pfret, filename, mode, _SH_DENYNO);
    return pfret;
}

/***
*errno_t _tfopen_s(pfile, file, mode) - open a file
*
*Purpose:
*       Opens the file specified as a stream.  mode determines file mode:
*       "r": read       "w": write      "a": append
*       "r+": read/write                "w+": open empty for read/write
*       "a+": read/append
*       Append "t" or "b" for text and binary mode
*       This is the secure version fopen - it opens the file in _SH_DENYRW
*       share mode.
*
*Entry:
*       FILE **pfile - Pointer to return the FILE handle into.
*       char *file - file name to open
*       char *mode - mode of file access
*
*Exit:
*       returns 0 on success & sets pfile
*       returns errno_t on failure.
*
*Exceptions:
*
*******************************************************************************/

errno_t __cdecl fopen_s (
    FILE ** pfile,
    const char *filename,
    const char *mode
    )
{
    _VALIDATE_RETURN_ERRCODE((pfile != NULL), EINVAL);

    return _fopen_int(pfile, filename, mode, _SH_SECURE);
}

errno_t __cdecl _wfopen_s (
    FILE ** pfile,
    const wchar_t *filename,
    const wchar_t *mode
    )
{
    _VALIDATE_RETURN_ERRCODE((pfile != NULL), EINVAL);

    return _wfopen_int(pfile, filename, mode, INVALID_HANDLE_VALUE, NULL, _SH_SECURE);
}

void* __cdecl _fileno(FILE* str)
{
    int iConsoleToOpen = -1;
    void *osfhnd = (void *)INVALID_HANDLE_VALUE;

    if (!_lock_validate_str ((FILEX*)str))
        return (void *)INVALID_HANDLE_VALUE;

    __STREAM_TRY
    {
        if (_osfhndstr((FILEX*)str) == INVALID_HANDLE_VALUE &&
            (((FILEX*)str)->fd < 3))
            iConsoleToOpen = ((FILEX*)str)->fd;
        else
            osfhnd = ((FILEX*)str)->osfhnd;
    }
    __STREAM_FINALLY
    {
        _unlock_str((FILEX*)str);
    }

    if (iConsoleToOpen != -1) {
        OpenStdConsole(iConsoleToOpen);
        if (_lock_validate_str ((FILEX*)str))
        {
            __STREAM_TRY
            {
                osfhnd = ((FILEX*)str)->osfhnd;
            }
            __STREAM_FINALLY
            {
                _unlock_str((FILEX*)str);
            }
        }
    }

    return osfhnd;
}

FILE* __cdecl _wfdopen(HANDLE hFile, const WCHAR* wszMode)
{
    FILE* pfret = NULL;
    _wfopen_int(&pfret, NULL, wszMode, hFile, NULL, _SH_DENYNO);
    return pfret;
}

errno_t __cdecl _wfreopen_helper (
    FILE** ppfile,
    const wchar_t *filename,
    const wchar_t *mode,
    FILEX *str,
    int shflag)
{
    errno_t errret;
    FILEX* pfile;

    _ASSERTE(ppfile != NULL);
    *ppfile = NULL;

    _VALIDATE_RETURN_ERRCODE((filename != NULL), EINVAL);
    _VALIDATE_RETURN_ERRCODE((mode != NULL), EINVAL);
    _VALIDATE_RETURN_ERRCODE((*mode != '\0'), EINVAL);
    _VALIDATE_RETURN_ERRCODE((str != NULL), EINVAL);

    // otherwise lock the stream again.
    if (!_lock_validate_str(str))
    {
        return EINVAL;
    }

    /* If the stream is in use, try to close it. Ignore possible
     * error (ANSI 4.9.5.4). */
    if (inuse(str))
    {
        _fclose_lk(str);
    }

    // re-Initialize the stream (copied from _getstream())
    str->_flag = str->_cnt = 0;
    str->_ptr = str->_base = NULL;
    str->osfhnd = INVALID_HANDLE_VALUE;
    str->peekch = '\n';
    str->osfile = 0;

    // stream will be unlocked by _wfopen_int, on exit, both success & failure
    errret = _wfopen_int(&pfile, filename, mode, INVALID_HANDLE_VALUE, str, shflag);

    // if we just redirected stdin/out/err, need to update SetStdioPath to match
    if(pfile && pfile->fd < 3)
    {
        SetStdioPathW(pfile->fd, filename);
    }

    *ppfile = pfile;
    return errret;
}

FILE* __cdecl _wfreopen(
    const wchar_t *filename,
    const wchar_t *mode,
    FILE *str
    )
{
    FILE *fp = NULL;

    _wfreopen_helper(&fp, filename, mode, (FILEX *)str, _SH_DENYNO);

    return fp;
}

errno_t __cdecl _wfreopen_s (
        FILE ** pfile,
        const wchar_t *filename,
        const wchar_t *mode,
        FILE *str
        )
{
    _VALIDATE_RETURN_ERRCODE((pfile != NULL), EINVAL);

    return _wfreopen_helper(pfile, filename, mode, (FILEX *)str, _SH_SECURE);
}


wint_t __cdecl putwchar(wint_t ch)
{
    /* Initialize before using *internal* stdout */
    if(!CheckStdioInit())
        return WEOF;

    return(fputwc(ch, stdout));
}


wint_t __cdecl getwchar(void)
{
    /* Initialize before using *internal* stdin */
    if(!CheckStdioInit())
        return WEOF;

    return(fgetwc(stdin));
}


/***
*int _setmode(fh, mode) - set file translation mode
*
*Purpose:
*   changes file mode to text/binary, depending on mode arg. this affects
*   whether read's and write's on the file translate between CRLF and LF
*   or is untranslated
*
*Entry:
*   int fh - file handle to change mode on
*   int mode - file translation mode (one of O_TEXT and O_BINARY)
*
*Exit:
*   returns old file translation mode
*   returns -1 and sets errno if fails
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _setmode(FILEX* str, int mode)
{
    int retval;

    _VALIDATE_RETURN((str != NULL), EINVAL, -1);

    /* lock the file */
    if (!_lock_validate_str(str))
        return -1;

    __STREAM_TRY
    {
        retval = ((_osfilestr(str) & FTEXT) ? _O_TEXT : _O_BINARY);

        if (mode == _O_BINARY)
            _osfilestr(str) &= ~FTEXT;
        else if (mode == _O_TEXT)
            _osfilestr(str) |= FTEXT;
        else
            retval = -1;
    }
    __STREAM_FINALLY
    {
        /* unlock the file */
        _unlock_str(str);
    }

    return retval;
}

