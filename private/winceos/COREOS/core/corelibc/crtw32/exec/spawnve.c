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
*spawnve.c - Low level routine eventually called by all _spawnXX routines
*       also contains all code for _execve, called by _execXX routines
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*
*       This is the low level routine which is eventually invoked by all the
*       _spawnXX routines.
*
*       This is also the low-level routine which is eventually invoked by
*       all of the _execXX routines.
*
*Revision History:
*       03-??-84  RLB   created
*       05-??-84  DCW   modified to fix bug in initialization of envblock
*                       pointer (used int 0 which would fail in long model) and
*                       changed (char *)0 to NULL.
*       03-31-86  SKS   modified for OS/2; no OVERLAY mode,
*                       new format for DOS Exec function
*                       also check for Xenix or DOS style slash characters
*       10-13-86  SKS   pass program name to _cenvarg()
*       11-19-86  SKS   handle both kinds of slashes, with support for Kanji
*       01-29-87  BCM   don't try ".com" extension in protected mode (OS/2)
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05/31/88  SJM   Re-written to allow spawn of .CMD files, increase
*                       speed. Added comexecmd routine.
*       06/01/88  SJM   added #ifdef statements for execve.obj compilation
*       10-30-88  GJF   Call _dospawn() for EXECVE, not _doexec().
*       07-21-89  GJF   Progagated fixes of 11-23-88 and 05-27-89 from CRT tree.
*       11-16-89  GJF   Added code to execve/spawnve to ensure a relative or
*                       or absolute pathname is always used for the executable,
*                       not just a filename (otherwise DOSEXECPGM will search
*                       the PATH!). Also, cleaned up some of the erratic
*                       formatting. Same as 9-15-89 change to CRT version
*       11-20-89  GJF   Added const attribute to types of appropriate args.
*       02-08-90  GJF   Fixed bug in comexecmd (must free(comname) if and only
*                       if comname points to a malloc-ed block). Propagated
*                       from 02-07-90 change in crt6 version.
*       03-08-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       04-02-90  GJF   Made comexecmd() _CALLTYPE4. Added #include <io.h>
*                       and a prototype for comexecmd() to fix compiler
*                       warnings (-W3).
*       05-21-90  GJF   Fixed stack checking pragma syntax.
*       07-24-90  SBM   Removed redundant include, minor optimization
*       09-27-90  GJF   New-style function declarators.
*       12-28-90  SRW   Added _CRUISER_ conditional around check_stack pragma
*       01-17-91  GJF   ANSI naming.
*       08-21-91  JCR   Call access() in before comexecmd (update)
*       01-24-92  JCR   upgraded for Win32
*       10-24-92  SKS   Remove special treatment for batch files -
*                       Windows NT will spawn %COMSPEC% automatically
*       11-30-92  KRS   Ported _MBCS support from 16-bit tree.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       12-07-93  CFW   Rip out Cruiser.
*       12-07-93  CFW   Wide char enable.
*       01-10-95  CFW   Debug CRT allocs.
*       02-06-98  GJF   Changes for Win64: changed return type to intptr_t.
*       11-02-03  AC    Added validation.
*       03-24-04  MSL   Added missing validation
*                       VSW#209339
*       09-13-04 DJT    Cache errno on extension lookup for success case
*                       VSWhidbey:312731.
*       04-01-05  MSL   Integer overflow fix during allocation 
*
*******************************************************************************/

#include <cruntime.h>
#include <io.h>
#include <process.h>
#include <errno.h>
#include <msdos.h>
#include <string.h>
#include <stdlib.h>
#include <internal.h>
#include <mbstring.h>
#include <tchar.h>
#include <dbgint.h>

#define SLASHCHAR   _T('\\')
#define XSLASHCHAR  _T('/')

#ifndef EXECVE
#ifdef WPRFLAG
static intptr_t __cdecl wcomexecmd(int mode, const wchar_t * name,
        const wchar_t * const * argv, const wchar_t * const * envp);
#else 
static intptr_t __cdecl comexecmd(int mode, const char * name,
        const char * const * argv, const char * const * envp);
#endif
#else /* EXECVE */
#ifdef WPRFLAG
static intptr_t __cdecl wcomexecmd(const wchar_t * name,
        const wchar_t * const * argv, const wchar_t * const * envp);
#else
static intptr_t __cdecl comexecmd(const char * name,
        const char * const * argv, const char * const * envp);
#endif
#endif /* EXECVE */

/***
*static int comexecmd(mode, name, argv, envp) - do the exec
*       or spawn after name fixup
*
*Purpose:
*       Spawns a child process with given parameters and environment.  Either
*       overlays current process or loads in free memory while parent process
*       waits.  If the named file is a .cmd file, modifies the calling sequence
*       and prepends the /c and filename arguments into the command string
*
*       Exec doesn't take a mode; instead, the parent process goes away as
*       the child process is brought in.
*
*Entry:
*       int mode - mode to spawn (WAIT, NOWAIT, or OVERLAY)
*                   only WAIT and OVERLAY currently supported
*
*           ****  mode is only used in the spawnve() version  ****
*
*       _TSCHAR *name - pathname of file to spawn.  Includes the extension
*       _TSCHAR **argv - vector of parameter strings
*       _TSCHAR **envp - vector of environment variables
*
*Exit:
*       returns exit code of child process
*       if fails, returns -1
*
*Exceptions:
*       Returns a value of (-1) to indicate an error in exec'ing the child
*       process.  errno may be set to:
*
*       E2BIG   = failed in argument/environment processing (_cenvarg)
*                 argument list or environment too big;
*       EACCESS = locking or sharing violation on file;
*       EMFILE  = too many files open;
*       ENOENT  = failed to find program name - no such file or directory;
*       ENOEXEC = failed in exec - bad executable format;
*       ENOMEM  = failed in memory allocation (during malloc, or in
*                 setting up memory for executing child process).
*
*******************************************************************************/

#ifdef WPRFLAG
static intptr_t __cdecl wcomexecmd (
#else
static intptr_t __cdecl comexecmd (
#endif

#ifndef EXECVE
        int mode,
#endif /* EXECVE */

        const _TSCHAR *name,
        const _TSCHAR * const *argv,
        const _TSCHAR * const *envp
        )
{
        _TSCHAR *argblk;
        _TSCHAR *envblk;
        intptr_t rc;

        /* validation section */
        _VALIDATE_RETURN(name != NULL, EINVAL, -1);
        _VALIDATE_RETURN(argv != NULL, EINVAL, -1);

#ifdef WPRFLAG
        if (_wcenvarg(argv, envp, &argblk, &envblk, name) == -1)
#else
        if (_cenvarg(argv, envp, &argblk, &envblk, name) == -1)
#endif
                return -1;

#ifndef EXECVE
#ifdef WPRFLAG
        rc = _wdospawn(mode, name, argblk, envblk);
#else
        rc = _dospawn(mode, name, argblk, envblk);
#endif
#else /* EXECVE */
#ifdef WPRFLAG
        rc = _wdospawn(_P_OVERLAY, name, argblk, envblk);
#else
        rc = _dospawn(_P_OVERLAY, name, argblk, envblk);
#endif
#endif /* EXECVE */
        /* free memory */

        _free_crt(argblk);
        _free_crt(envblk);

        return rc;
}

/***
*int _spawnve(mode, name, argv, envp) - low level _spawnXX library function
*int _execve(name, argv, envp) - low level _execXX library function
*
*Purpose:
*       spawns or execs a child process; takes a single pointer to an argument
*       list as well as a pointer to the environment; unlike _spawnvpe,
*       _spawnve does not search the PATH= list in processing the name
*       parameter; mode specifies the parent's execution mode.
*
*Entry:
*       int mode    - parent process's execution mode:
*                     must be one of _P_OVERLAY, _P_WAIT, _P_NOWAIT;
*                     not used for _execve
*       _TSCHAR *name  - path name of program to spawn;
*       _TSCHAR **argv - pointer to array of pointers to child's arguments;
*       _TSCHAR **envp - pointer to array of pointers to child's environment
*                     settings.
*
*Exit:
*       Returns : (int) a status value whose meaning is as follows:
*               0        = normal termination of child process;
*               positive = exit code of child upon error termination
*                          (abort or exit(nonzero));
*               -1       = child process was not spawned;
*                          errno indicates what kind of error:
*                          (E2BIG, EINVAL, ENOENT, ENOEXEC, ENOMEM).
*
*Exceptions:
*       Returns a value of (-1) to indicate an error in spawning the child
*       process.  errno may be set to:
*
*       E2BIG   = failed in argument/environment processing (_cenvarg) -
*                 argument list or environment too big;
*       EINVAL  = invalid mode argument;
*       ENOENT  = failed to find program name - no such file or directory;
*       ENOEXEC = failed in spawn - bad executable format;
*       ENOMEM  = failed in memory allocation (during malloc, or in
*                 setting up memory for spawning child process).
*
*******************************************************************************/

/* Extension array - ordered in search order from right to left.

   ext_strings  = array of extensions
*/

static _TSCHAR *ext_strings[] = { _T(".cmd"), _T(".bat"), _T(".exe"), _T(".com") };
enum {CMD, BAT, EXE, COM, EXTFIRST=CMD, EXTLAST=COM};

intptr_t __cdecl

#ifndef EXECVE

_tspawnve (
        int mode,

#else /* EXECVE */

_texecve (

#endif /* EXECVE */

        const _TSCHAR *name,
        const _TSCHAR * const *argv,
        const _TSCHAR * const *envp
        )
{
        _TSCHAR *ext;   /* where the extension goes if we have to add one */
        _TSCHAR *p;
        _TSCHAR *q;
        _TSCHAR *pathname = (_TSCHAR *)name;
        intptr_t rc;
        int i;
        errno_t save_errno = 0;

        /* validation section */
        _VALIDATE_RETURN(name != NULL, EINVAL, -1);
        _VALIDATE_RETURN(*name != _T('\0'), EINVAL, -1);
        _VALIDATE_RETURN(argv != NULL, EINVAL, -1);
        _VALIDATE_RETURN(*argv != NULL, EINVAL, -1);
        _VALIDATE_RETURN(**argv != _T('\0'), EINVAL, -1);

        p = _tcsrchr(pathname, SLASHCHAR);
        q = _tcsrchr(pathname, XSLASHCHAR);

        /* ensure that pathname is an absolute or relative pathname. also,
         * position p to point at the filename portion of pathname (i.e., just
         * after the last occurence of a colon, slash or backslash character */

        if (!q) {
                if (!p)
                        if (!(p = _tcschr(pathname, _T(':')))) {

                                /* pathname is a filename only, force it to be
                                 * a relative pathname. note that an extra byte
                                 * is malloc-ed just in case pathname is NULL,
                                 * to keep the heap from being trashed by
                                 * strcpy */
                                size_t pathname_size = _tcslen(pathname) + 3;
                                if (!(pathname = _calloc_crt(pathname_size, sizeof(_TSCHAR))))
                                        return(-1);

                                _ERRCHECK(_tcscpy_s(pathname, pathname_size, _T(".\\")));
                                _ERRCHECK(_tcscat_s(pathname, pathname_size, name));

                                /* set p to point to the start of the filename
                                 * (i.e., past the ".\\" prefix) */
                                p = pathname + 2;
                        }
                        /* else pathname has drive specifier prefix and p is
                         * is pointing to the ':' */
        }
        else if (!p || q > p)   /* p == NULL or q > p */
                p = q;


        rc = -1;        /* init to error value */

        if (ext = _tcsrchr(p, _T('.')))  {

                /* extension given; only do filename */

                if (_taccess_s(pathname, 0) == 0) {

#ifndef EXECVE

#ifdef WPRFLAG
                        rc = wcomexecmd(mode, pathname, argv, envp);
#else
                        rc = comexecmd(mode, pathname, argv, envp);
#endif

#else /* EXECVE */

#ifdef WPRFLAG
                        rc = wcomexecmd(pathname, argv, envp);
#else
                        rc = comexecmd(pathname, argv, envp);
#endif

#endif /* EXECVE */
                }

        }
        else    {

                /* no extension; try .cmd/.bat, then .com and .exe */

                size_t size = _tcslen(pathname) + 5;
                if (!(p = _calloc_crt(size, sizeof(_TSCHAR))))
                        return(-1);

                _ERRCHECK(_tcscpy_s(p, size, pathname));
                ext = p + _tcslen(pathname);

                save_errno = errno;
                for (i = EXTLAST; i >= EXTFIRST; --i) {
                        _ERRCHECK(_tcscpy_s(ext, size - (ext - p), ext_strings[i]));

                        if (_taccess_s(p, 0) == 0) {
                                errno = save_errno;

#ifndef EXECVE
#ifdef WPRFLAG
                                rc = wcomexecmd(mode, p, argv, envp);
#else
                                rc = comexecmd(mode, p, argv, envp);
#endif
#else /* EXECVE */
#ifdef WPRFLAG
                                rc = wcomexecmd(p, argv, envp);
#else
                                rc = comexecmd(p, argv, envp);
#endif
#endif /* EXECVE */
                                break;
                        }
                }
                _free_crt(p);
        }

        if (pathname != name)
                _free_crt(pathname);

        return rc;
}
