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
*stat64.c - get file status
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _stat64() - get file status
*
*Revision History:
*       06-02-98  GJF   Created.
*       11-10-99  GB    Made changes so as to take care of DST.
*       10-28-02  PK    Fixed minor bug in IsRootUNCName (whidbey bug 15444)
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       03-17-05  PAL   Don't require valid last-write-time in file info
*                       returned by syscall. VSW 438008
*       03-27-06  AC    Fix usage of errno
*                       VSW#585562
*
*******************************************************************************/

#include <cruntime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <msdos.h>
#include <oscalls.h>
#include <string.h>
#include <internal.h>
#include <stdlib.h>
#include <direct.h>
#include <mbstring.h>
#include <tchar.h>
#include <io.h>
#include <share.h>
#include <fcntl.h>
#include <dbgint.h>

#ifndef _USE_INT64
#define _USE_INT64 1
#endif

#define ISSLASH(a)  ((a) == _T('\\') || (a) == _T('/'))

#ifdef _UNICODE 

/*
 * Local routine which returns true if the argument is a UNC name
 * specifying the root name of a share, such as '\\server\share\'.
 */
static int IsRootUNCName(const wchar_t *path);
static wchar_t * _wfullpath_helper(wchar_t * ,const wchar_t *,size_t , wchar_t **);

extern unsigned short __cdecl __wdtoxmode(int, const wchar_t *);

#endif /* _UNICODE */

/***
*int _stat64(name, buf) - get file status info
*
*Purpose:
*       _stat64 obtains information about the file and stores it in the
*       structure pointed to by buf.
*
*       Note: Unlike _stat, _stat64 uses the UTC time values returned in
*       WIN32_FIND_DATA struct. This means the time values will always be
*       correct on NTFS, but may be wrong on FAT file systems for file times
*       whose DST state is different from the current DST state (this an NT
*       bug).
*
*Entry:
*       _TSCHAR *name -    pathname of given file
*       struct _stat *buffer - pointer to buffer to store info in
*
*Exit:
*       fills in structure pointed to by buffer
*       returns 0 if successful
*       returns -1 and sets errno if unsuccessful
*
*Exceptions:
*
*******************************************************************************/

#if  _USE_INT64
 #define _STAT_FUNC _stat64
 #define _W_STAT_FUNC _wstat64
 #define _STAT_STRUCT _stat64
 #define _FSTAT_FUNC _fstat64
#else
 #define _STAT_FUNC _stat64i32
 #define _W_STAT_FUNC _wstat64i32
 #define _STAT_STRUCT _stat64i32
 #define _FSTAT_FUNC _fstat64i32
#endif

#ifndef _UNICODE

int __cdecl _STAT_FUNC (
    const char *name,
    struct _STAT_STRUCT *buf
    )
{
    wchar_t* namew = NULL;
    int retval;

    if (name)
    {
        if (!__copy_path_to_wide_string(name, &namew))
            return -1;
    }

    /* call the wide-char variant */
    retval = _W_STAT_FUNC(namew, buf);

    _free_crt(namew); /* _free_crt leaves errno alone if everything completes as expected */

    return retval;
}

#else /* _UNICODE */

int __cdecl _W_STAT_FUNC (
    const wchar_t *name,
    struct _STAT_STRUCT *buf
    )
{
    wchar_t *  path;
    wchar_t    pathbuf[ _MAX_PATH ];
    int drive;          /* A: = 1, B: = 2, etc. */
    HANDLE findhandle;
    WIN32_FIND_DATA findbuf;
    int retval = 0;

    _VALIDATE_CLEAR_OSSERR_RETURN( (name != NULL), EINVAL, -1);
    _VALIDATE_CLEAR_OSSERR_RETURN( (buf != NULL), EINVAL, -1);

    /* Don't allow wildcards to be interpreted by system */

    if (wcspbrk(name, L"?*"))
    {
        errno = ENOENT;
        _doserrno = E_nofile;
        return(-1);
    }

    /* Try to get disk from name.  If none, get current disk.  */

    if (name[1] == _T(':'))
    {
        if ( *name && !name[2] )
        {
            errno = ENOENT;             /* return an error if name is   */
            _doserrno = E_nofile;       /* just drive letter then colon */
            return( -1 );
        }
        drive = towlower(*name) - _T('a') + 1;
    }
    else
    {
        drive = _getdrive();
    }

    /* Call Find Match File */
    findhandle = FindFirstFileExW(name, FindExInfoStandard, &findbuf, FindExSearchNameMatch, NULL, 0);
    if ( findhandle == INVALID_HANDLE_VALUE )
    {
        wchar_t * pBuf = NULL;
        
        if ( !( wcspbrk(name, L"./\\") &&
             (path = _wfullpath_helper( pathbuf, name, _MAX_PATH, &pBuf )) &&
             /* root dir. ('C:\') or UNC root dir. ('\\server\share\') */
#ifdef _WIN32_WCE
             ((wcslen( path ) == 3) || IsRootUNCName(path)) ) )
#else
             ((wcslen( path ) == 3) || IsRootUNCName(path)) &&
             (GetDriveTypeW( path ) > 1) ) ) 
#endif // _WIN32_WCE 
        {
            if(pBuf)
            {
                free(pBuf);
            }

            errno = ENOENT;
            _doserrno = E_nofile;
            return( -1 );
        }

        if(pBuf)
        {
            free(pBuf);
        }

        /*
         * Root directories (such as C:\ or \\server\share\ are fabricated.
         */

        findbuf.dwFileAttributes = A_D;
        findbuf.nFileSizeHigh = 0;
        findbuf.nFileSizeLow = 0;
        findbuf.cFileName[0] = _T('\0');

        buf->st_mtime = __loctotime64_t(1980,1,1,0,0,0, -1);
        buf->st_atime = buf->st_mtime;
        buf->st_ctime = buf->st_mtime;
    }
#ifdef _WIN32_WCE
    else if ( (findbuf.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) )
#else
    else if ( (findbuf.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
              (findbuf.dwReserved0 == IO_REPARSE_TAG_SYMLINK) )
#endif // _WIN32_WCE 
    {
        /* if the file is a symbolic link, then use fstat to fill the info in the _stat struct */
        int fd = -1;
        errno_t e;
        int oflag = _O_RDONLY;

        if (findbuf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            oflag |= _O_OBTAIN_DIR;
        }

        e = _wsopen_s(&fd, name, oflag, _SH_DENYNO, 0 /* ignored */);
        if (e != 0 || fd == -1)
        {
            errno = ENOENT;
            _doserrno = E_nofile;
            return -1;
        }

        retval = _FSTAT_FUNC(fd, buf);
        _close(fd);
        FindClose( findhandle );

        return retval;
    }
    else
    {
        SYSTEMTIME SystemTime;
        FILETIME LocalFTime;

        if ( findbuf.ftLastWriteTime.dwLowDateTime ||
             findbuf.ftLastWriteTime.dwHighDateTime )
        {
            if ( !FileTimeToLocalFileTime( &findbuf.ftLastWriteTime, 
                                           &LocalFTime )            ||
                 !FileTimeToSystemTime( &LocalFTime, &SystemTime ) )
            {
                _dosmaperr( GetLastError() );
                FindClose( findhandle );
                return( -1 );
            }

            buf->st_mtime = __loctotime64_t( SystemTime.wYear,
                                             SystemTime.wMonth,
                                             SystemTime.wDay,
                                             SystemTime.wHour,
                                             SystemTime.wMinute,
                                             SystemTime.wSecond,
                                             -1 );
        }
        else
        {
            buf->st_mtime = 0;
        }

        if ( findbuf.ftLastAccessTime.dwLowDateTime ||
             findbuf.ftLastAccessTime.dwHighDateTime )
        {
            if ( !FileTimeToLocalFileTime( &findbuf.ftLastAccessTime,
                                           &LocalFTime )                ||
                 !FileTimeToSystemTime( &LocalFTime, &SystemTime ) )
            {
                _dosmaperr( GetLastError() );
                FindClose( findhandle );
                return( -1 );
            }

            buf->st_atime = __loctotime64_t( SystemTime.wYear,
                                           SystemTime.wMonth,
                                           SystemTime.wDay,
                                           SystemTime.wHour,
                                           SystemTime.wMinute,
                                           SystemTime.wSecond,
                                           -1 );
        }
        else
        {
            buf->st_atime = buf->st_mtime;
        }

        if ( findbuf.ftCreationTime.dwLowDateTime ||
             findbuf.ftCreationTime.dwHighDateTime )
        {
            if ( !FileTimeToLocalFileTime( &findbuf.ftCreationTime,
                                           &LocalFTime )                ||
                 !FileTimeToSystemTime( &LocalFTime, &SystemTime ) )
            {
                _dosmaperr( GetLastError() );
                FindClose( findhandle );
                return( -1 );
            }

            buf->st_ctime = __loctotime64_t( SystemTime.wYear,
                                           SystemTime.wMonth,
                                           SystemTime.wDay,
                                           SystemTime.wHour,
                                           SystemTime.wMinute,
                                           SystemTime.wSecond,
                                           -1 );
        }
        else
        {
            buf->st_ctime = buf->st_mtime;
        }

        FindClose(findhandle);
    }

    /* Fill in buf */

    buf->st_mode = __wdtoxmode(findbuf.dwFileAttributes, name);
    buf->st_nlink = 1;
#if  _USE_INT64
    buf->st_size = ((__int64)(findbuf.nFileSizeHigh)) * (0x100000000i64) +
                    (__int64)(findbuf.nFileSizeLow);
#else   /* _USE_INT64 */
    buf->st_size = findbuf.nFileSizeLow;

    /* If the file size > 4GB, we get an overflow, so reset the file size 
       and set the appropriate error */
    if (findbuf.nFileSizeHigh != 0)
    {
        errno = EOVERFLOW;
        buf->st_size = 0;
        retval = -1;
    }
#endif  /* _USE_INT64 */

    /* now set the common fields */

    buf->st_uid = buf->st_gid = buf->st_ino = 0;

    buf->st_rdev = buf->st_dev = (_dev_t)(drive - 1); /* A=0, B=1, etc. */

    return retval;
}

/*
 * IsRootUNCName - returns TRUE if the argument is a UNC name specifying
 *      a root share.  That is, if it is of the form \\server\share\.
 *      This routine will also return true if the argument is of the
 *      form \\server\share (no trailing slash) but Win32 currently
 *      does not like that form.
 *
 *      Forward slashes ('/') may be used instead of backslashes ('\').
 */

static int IsRootUNCName(const wchar_t *path)
{
    /*
     * If a root UNC name, path will start with 2 (but not 3) slashes
     */

    if ( ( wcslen ( path ) >= 5 ) /* minimum string is "//x/y" */
         && ISSLASH(path[0]) && ISSLASH(path[1])
         && !ISSLASH(path[2]))
    {
        const wchar_t * p = path + 2 ;

        /*
         * find the slash between the server name and share name
         */
        while ( * ++ p )
        {
            if ( ISSLASH(*p) )
            {
                break;
            }
        }

        if ( *p && p[1] )
        {
            /*
             * is there a further slash?
             */
            while ( * ++ p )
            {
                if ( ISSLASH(*p) )
                {
                    break;
                }
            }

            /*
             * just final slash (or no final slash)
             */
            if ( !*p || !p[1])
            {
                return 1;
            }
        }
    }

    return 0 ;
}

static wchar_t * __cdecl _wfullpath_helper(wchar_t * buf,const wchar_t *path,size_t sz, wchar_t ** pBuf)
{
    wchar_t * ret;
    errno_t save_errno = errno;

    errno = 0;
    ret = _wfullpath(buf, path, sz);
    if (ret)
    {
        errno = save_errno;
        return ret;
    }
    
    if (errno != ERANGE)
    {
        return NULL;
    }
    errno = save_errno;
    
    *pBuf = _wfullpath(NULL, path, 0);
    
    return *pBuf;
}

#endif /* _UNICODE */
