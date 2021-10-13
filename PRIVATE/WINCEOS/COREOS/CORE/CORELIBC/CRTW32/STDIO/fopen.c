//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
BOOL WINAPI xxx_CloseHandle(HANDLE hObject);

int __cdecl _close(FILEX* str)
{
  if(xxx_CloseHandle(_osfhndstr(str))) {
    _osfhndstr(str) = INVALID_HANDLE_VALUE; // clear osfhnd
    _osfilestr(str) = 0;    // clear file flags
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

int __cdecl fclose (FILEX *stream)
{
    int result;
    
    if(!CheckStdioInit())
        return EOF;

    _ASSERTE(stream != NULL);

    /* Stream is a real file. */
    if (! _lock_validate_str(stream))
        return EOF;
    result = _fclose_lk(stream);

    _unlock_str(stream);
    return result;
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

int __cdecl _fclose_lk (FILEX *stream)
{
    int result;
    
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
FILE* __cdecl _wfopen_int (
    const WCHAR *filename,
    const WCHAR *mode,
    HANDLE hFile,
    FILEX* stream
    )
{
    int streamflag = 0;           // FILEX->fileflags 
    BYTE fileflags = FTEXT; // FILEX->osfile flags. Start with FTEXT by default
    DWORD fileaccess = 0;   // OS file access (requested)
    DWORD filecreate = 0;   // OS method of opening/creating
    BOOL  fAppend = 0;
    DWORD fileshare = (FILE_SHARE_READ | FILE_SHARE_WRITE); // always allow sharing
    DWORD fileattrib = FILE_ATTRIBUTE_NORMAL;               // always normal attributes

    if((hFile==INVALID_HANDLE_VALUE) && (!filename || !filename[0]))
        return NULL;

    if(!mode || !mode[0])
        return NULL;
    
    if(!CheckStdioInit())
        return NULL;
    
    // If we didnt get passed stream (by freopen) then get a free stream 
    // [NOTE: _getstream() returns a locked stream & freopen passes us a locked stream]

    if(NULL==stream)
    {
        if ((stream = _getstream()) == NULL)
            return(NULL);
    }
    /* Parse the user's specification string as set flags in
           (1) fileaccess, fileshare, filecreate, fileattrib - system call params
           (2) fileflags - osfile flags word
           (2) streamflag - stream handle flags word. */
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

    /* There can be up to three more optional mode characters:
       (1) One of 't' and 'b' and
       (2) One of 'c' and 'n' and
       (3) One of 'S' or 'R' and optionall 'D, 'T'
       (4) and an optional +
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
    // if we got filename, try to open it. Else we got an HFile -- just use that
    if(filename)
    {
        // try to open/create the file
        hFile = CreateFileW(filename, fileaccess, fileshare, NULL, filecreate, fileattrib, NULL);
        if(hFile == INVALID_HANDLE_VALUE)
            goto fail2;
    }
    // the file is open. now, set the info in FILEX struct
    _osfhndstr(stream) = hFile;

    
#ifdef UNDER_CE
    // check if it is a device
    if(filename && lstrlen(filename)==5 && filename[4]==':')
        fileflags |= FDEV;
#else //!UNDER_CE 
    if (GetFileType(hFile) == FILE_TYPE_CHAR)
        fileflags |= FDEV;
#endif //!UNDER_CE

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

        // NOTE: In WIN32 this code was relying on a GLE of ERROR_NEGATIVE_SEEK
        // error from SetFilePointer API if the file length was 0, but on CE
        // we don't get that error code. So check the file size first.

        if(GetFileSize(_osfhndstr(stream), NULL))
        {
            WCHAR ch = 0;
            
            if (_lseek(stream, -1, SEEK_END) == -1)
                goto fail;
            
            /* Seek was OK, read the last char in file. The last
               char is a CTRL-Z if and only if _read returns 0
               and ch ends up with a CTRL-Z. */
            if (_read(stream, &ch, 1) == 0 && ch == 26) 
            {
                // Last char was a ^Z. Nuke it.
                    if((_lseek(stream, -1, SEEK_END) == -1) || !SetEndOfFile(_osfhndstr(stream)))
                    goto fail;
            }
             
            /* now rewind the file to the beginning */
            if (_lseek(stream, 0, SEEK_SET) == -1) 
                goto fail;
        }
    }
    // Set FAPPEND flag if appropriate. Don't do this for devices or pipes.
    if(fAppend  && !(fileflags & (FDEV|FPIPE)))
        _osfilestr(stream) |= FAPPEND;

    /* Set up the stream data base. */
    /* Init pointers */
    stream->_flag = streamflag;
    stream->_cnt = 0;
    stream->_base = stream->_ptr = NULL;

    // unlock stream and return
    _unlock_str(stream);
    return (FILE*)stream;

fail:
    _close(stream);
    // fall through
fail2:
    // unlock stream and return NULL
    _unlock_str(stream);
    return NULL;
}

FILE* __cdecl _wfopen(
    const WCHAR *filename,
    const WCHAR *mode
    )
{
    return _wfopen_int(filename, mode, INVALID_HANDLE_VALUE, NULL);
}

FILE* __cdecl fopen (
    const char *filename,
    const char *mode
    )
{
    WCHAR *wszName, *wszMode;
    int iNameLen, iModeLen;
    int iNameLen2, iModeLen2;
    FILE *fp = NULL;

    // Fix VBug 17032
    if ( !filename || !mode )
        return NULL;

    if ( *filename == '\0' || *mode == '\0' )
        return NULL;

    wszName = malloc( (iNameLen2 = sizeof(WCHAR)*((iNameLen=strlen(filename))+1)) );
    wszMode = malloc( (iModeLen2 = sizeof(WCHAR)*((iModeLen=strlen(mode))+1)) );

    if( MultiByteToWideChar(CP_ACP, 0, filename, -1, wszName, iNameLen2) &&
        MultiByteToWideChar(CP_ACP, 0, mode, -1, wszMode, iModeLen2) )
        fp = _wfopen_int(wszName, wszMode, INVALID_HANDLE_VALUE, NULL);

    free (wszName);
    free (wszMode);

    return fp;
}

void* __cdecl _fileno(FILE* str)
{
    int iConsoleToOpen = -1;
    void *osfhnd = (void *)INVALID_HANDLE_VALUE;

    if (! _lock_validate_str ((FILEX*)str))
        return (void *)INVALID_HANDLE_VALUE;

    if(_osfhndstr((FILEX*)str)==INVALID_HANDLE_VALUE && (((FILEX*)str)->fd < 3))
        iConsoleToOpen = ((FILEX*)str)->fd;
    else
        osfhnd = ((FILEX*)str)->osfhnd;

    _unlock_str((FILEX*)str);

    if (iConsoleToOpen != -1) {
        OpenStdConsole(iConsoleToOpen);
        if (_lock_validate_str ((FILEX*)str)) {
            osfhnd = ((FILEX*)str)->osfhnd;
            _unlock_str((FILEX*)str);
        }
    }

    return osfhnd;

}

FILE* __cdecl _wfdopen(HANDLE hFile, const WCHAR* wszMode)
{
    return _wfopen_int(NULL, wszMode, hFile, NULL);
}

FILE* __cdecl _wfreopen( const wchar_t *path, const wchar_t *mode, FILEX *stream )
{
    FILEX* fp;
    
    // otherwise lock the stream again.
    if (! _lock_validate_str(stream))
        return NULL;

        /* If the stream is in use, try to close it. Ignore possible
         * error (ANSI 4.9.5.4). */
        if ( inuse(stream) )
                _fclose_lk(stream);

    // re-Initialize the stream (copied from _getstream())
    stream->_flag = stream->_cnt = 0;
    stream->_ptr = stream->_base = NULL;
    stream->osfhnd = INVALID_HANDLE_VALUE;
    stream->peekch = '\n';
    stream->osfile = 0;

    // stream will be unlocked by _wfopen_int, on exit, both success & failure
    fp = _wfopen_int(path, mode, INVALID_HANDLE_VALUE, stream);
    // if we just redirected stdin/out/err, need to update SetStdioPath to match
    if(fp && fp->fd<3)
        SetStdioPathW(fp->fd, path);
    return fp;
}


/***
#undef putwc

wint_t __cdecl putwc (
    wint_t ch,
    FILEX *str
    )
{
    return fputwc(ch, str);
}
***/

wint_t __cdecl putwchar(wint_t ch)
{
    if(!CheckStdioInit())
        return WEOF;
    return(fputwc(ch, stdout));
}

/***
#undef getwc

wint_t __cdecl getwc (
    FILEX *stream
    )
{
    return fgetwc(stream);
}
***/

wint_t __cdecl getwchar(void)
{
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

    /* lock the file */
    if (! _lock_validate_str(str))
        return -1;

    retval = ((_osfilestr(str) & FTEXT) ? _O_TEXT : _O_BINARY);

    if (mode == _O_BINARY)
        _osfilestr(str) &= ~FTEXT;
    else if (mode == _O_TEXT)
        _osfilestr(str) |= FTEXT;
    else
        retval = -1;

    /* unlock the file */
    _unlock_str(str);

    return retval;
}

