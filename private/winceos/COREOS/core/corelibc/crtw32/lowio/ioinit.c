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
*ioinit.c - Initialization for lowio functions
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Contains initialization and termination routines for lowio.
*       Currently, this includes:
*           1. Initial allocation of array(s) of ioinfo structs.
*           2. Processing of inherited file info from parent process.
*           3. Special case initialization of the first three ioinfo structs,
*              the ones that correspond to handles 0, 1 and 2.
*
*Revision History:
*       02-14-92  GJF   Module created.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       03-28-94  GJF   Made definitions of:
*                           _osfhnd[]
*                           _osfile[]
*                           _pipech[]
*                       conditional on ndef DLL_FOR_WIN32S. Also replaced
*                       MTHREAD with _MT.
*       02-02-95  BWT   Check cbReserved2 before copying inherited handles.
*                       Current NT doesn't always set lpReserved2 to NULL.
*       06-01-95  GJF   Always mark handles 0 - 2 as open. Further, if an
*                       underlying HANDLE-s is invalid or of unknown type,
*                       mark the corresponding handle as a device (handles
*                       0 - 2 only).
*       06-08-95  GJF   Completely revised to use arrays of ioinfo structs.
*       07-07-95  GJF   Fixed the loop in _ioterm so it didn't iterate off
*                       the end of the universe.
*       07-11-95  GJF   Use UNALIGNED int and long pointers to avoid choking
*                       RISC platforms.
*       07-28-95  GJF   Added __badioinfo.
*       04-12-96  SKS   __badioinfo and __pioinfo must be exported for the
*                       Old Iostreams DLLs (msvcirt.dll and msvcirtd.dll).
*       05-16-96  GJF   Don't call GetFileType for inherited pipe handles. 
*                       This avoids a major problem in NT - GetFileType will
*                       stop responding if there is a "blocking read" pending on the
*                       pipe in the parent.
*       07-08-96  GJF   Deleted DLL_FOR_WIN32S. Also, detab-ed.
*       07-09-96  GJF   Replaced __pioinfo[i] == NULL; with __pioinfo[i] =
*                       NULL in _ioterm().
*       02-10-98  GJF   Changes for Win64: use intptr_t for anything holding
*                       HANDLE values.
*       10-19-00  PML   Force text mode in __badioinfo to avoid alignment
*                       fault (vs7#176001).
*       02-20-01  PML   vs7#172586 Avoid _RT_LOCK by preallocating all locks
*                       that will be required, and returning failure back on
*                       inability to allocate a lock.
*       03-27-01  PML   Return -1 on fatal error instead of calling
*                       _amsg_exit (vs7#231220)
*       11-21-01  BWT   Wrap GetStartupInfo with try/except (it can raise 
*                        exceptions on failure)
*       09-12-02  SJ    Front-end has fixed a bug in Warning C4305.Casting 
*                       to avoid the warning (superflous in our case)
*       11-13-03  SJ    File Handle checks made consistent - VSW#199372
*       12-02-03  SJ    Reroute Unicode I/O
*       04-01-05  MSL   Integer overflow protection
*       04-20-05  MSL   Better validation after GetStdHandle
*       08-10-06  ATC   DevDivBugs#12336: Added initialization for dbcsBuffer
*
*******************************************************************************/

#include <cruntime.h>
#include <windows.h>
#include <internal.h>
#include <malloc.h>
#include <msdos.h>
#include <rterr.h>
#include <stddef.h>
#include <stdlib.h>
#include <dbgint.h>
#ifdef _WIN32_WCE
#include <sys/stat.h>
#include <cvt.h>
#include <coredll.h>
#include <string.h>
#include <console.h>

extern CRITICAL_SECTION csfStdioInitLock;
extern int fStdioInited;

// CE doesn't throw an exception, so just init the critical section
__inline BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION pCS,
                                                    DWORD dwSpinCount)
{
    InitializeCriticalSection(pCS);
    return TRUE;
}
#endif

/*
 * Special static ioinfo structure. This is referred to only by the
 * _pioinfo_safe() macro, and its derivatives, in internal.h. These, in turn
 * are used in certain stdio-level functions to more gracefully handle a FILE
 * with -1 in the _file field.
 */
_CRTIMP ioinfo __badioinfo = {
        (intptr_t)(-1), /* osfhnd */
        (char)FTEXT,          /* osfile */
        10,             /* pipech */ 
        0               /* lockinitflag */
        };

/*
 * Number of ioinfo structs allocated at any given time. This number ranges
 * from a minimum of IOINFO_ARRAY_ELTS to a maximum of _NHANDLE_ (==
 * IOINFO_ARRAY_ELTS * IOINFO_ARRAYS) in steps of IOINFO_ARRAY_ELTS.
 */
int _nhandle;

/*
 * Array of pointers to arrays of ioinfo structs.
 */
_CRTIMP ioinfo * __pioinfo[IOINFO_ARRAYS];

#ifndef _WIN32_WCE

/*
 * Global variable for one-time IO initialization structure
 */
INIT_ONCE __ioInitOnce = INIT_ONCE_STATIC_INIT;
#endif

/*
 * macro used to map 0, 1 and 2 to right value for call to GetStdHandle
 */
#define stdhndl(fh)  ( (fh == 0) ? STD_INPUT_HANDLE : ((fh == 1) ? \
                       STD_OUTPUT_HANDLE : STD_ERROR_HANDLE) )

/***
*_ioinit0() -
*
*Purpose:
*       Sets frequently access IO values to pre-initialization values,
*       allowing for lazy initialization of IO info structs and handles.
*
*******************************************************************************/
void __cdecl _ioinit0 (
        void
        )
{
    /* Setting __pioinfo array entries to NULL allows lazy initialization */
    memset((void*)__pioinfo, 0, sizeof (__pioinfo));

    /* Make sure _nhandle have sufficient entries to pass STD IO handle validations 
     * even when __pioinfo array is not yet initialized
     */
    _nhandle = STDIO_HANDLES_COUNT;
}

/***
*_ioinitCallback() -
*
*Purpose:
*       Allocates and initializes initial array(s) of ioinfo structs. Then,
*       obtains and processes information on inherited file handles from the
*       parent process (e.g., cmd.exe).
*
*       Obtains the StartupInfo structure from the OS. The inherited file
*       handle information is pointed to by the lpReserved2 field. The format
*       of the information is as follows:
*
*           bytes 0 thru 3          - integer value, say N, which is the
*                                     number of handles information is passed
*                                     about
*
*           bytes 4 thru N+3        - the N values for osfile
*
*           bytes N+4 thru 5*N+3    - N double-words, the N OS HANDLE values
*                                     being passed
*
*       Next, osfhnd and osfile for the first three ioinfo structs,
*       corrsponding to handles 0, 1 and 2, are initialized as follows:
*
*           If the value in osfhnd is INVALID_HANDLE_VALUE, then try to
*           obtain a HANDLE by calling GetStdHandle, and call GetFileType to
*           help set osfile. Otherwise, assume _osfhndl and _osfile are
*           valid, but force it to text mode (standard input/output/error
*           are to always start out in text mode).
*
*       Notes:
*           1. In general, not all of the passed info from the parent process
*              will describe open handles! If, for example, only C handle 1
*              (STDOUT) and C handle 6 are open in the parent, info for C
*              handles 0 thru 6 is passed to the the child.
*
*           2. Care is taken not to 'overflow' the arrays of ioinfo structs.
*
*           3. See exec\dospawn.c for the encoding of the file handle info
*              to be passed to a child process.
*
*Entry:
*       No parameters: reads the STARTUPINFO structure.
*
*Exit:
*       TRUE on success, FALSE if error encountered
*
*Exceptions:
*
*******************************************************************************/

BOOL CALLBACK _ioinitCallback (
#ifndef _WIN32_WCE
        PINIT_ONCE initOnce,
        PVOID param,
        PVOID* context
#endif
        )
{
    int fh;
    ioinfo *pio;
    intptr_t stdfh;
#ifndef _WIN32_WCE
    STARTUPINFOW StartupInfo;
    int cfi_len;
    int i;
    char *posfile;
    UNALIGNED intptr_t *posfhnd;
    DWORD htype;

    (initOnce);
    (param);
    (context);
#endif

    _mlock(_OSFHND_LOCK);   /* lock the __pioinfo[] array */

    __TRY

        if (__pioinfo[0] != NULL)
            return TRUE;  /* The array has been already initialized */
        /*
         * Allocate and initialize the first array of ioinfo structs. This
         * array is pointed to by __pioinfo[0]
         */
        if ( (pio = _calloc_crt( IOINFO_ARRAY_ELTS, sizeof(ioinfo) ))
             == NULL )
        {
            return FALSE;
        }

        __pioinfo[0] = pio;
        _nhandle = IOINFO_ARRAY_ELTS;

        for ( ; pio < __pioinfo[0] + IOINFO_ARRAY_ELTS ; pio++ ) {
            pio->osfile = 0;
            pio->osfhnd = (intptr_t)INVALID_HANDLE_VALUE;
            pio->pipech = 10;                   /* linefeed/newline char */
            pio->lockinitflag = 0;              /* uninitialized lock */
            pio->textmode = 0;
            pio->unicode = 0;
            pio->pipech2[0] = 10;
            pio->pipech2[1] = 10;
            pio->dbcsBufferUsed = FALSE;
            pio->dbcsBuffer = '\0';
        }

#ifndef _WIN32_WCE // CE doesn't support inheriting filehandles from parent
        /*
         * Process inherited file handle information, if any
         */

        GetStartupInfoW( &StartupInfo );
        if ( (StartupInfo.cbReserved2 != 0) &&
             (StartupInfo.lpReserved2 != NULL) )
        {
            /*
             * Get the number of handles inherited.
             */
            cfi_len = *(UNALIGNED int *)(StartupInfo.lpReserved2);

            /*
             * Set pointers to the start of the passed file info and OS
             * HANDLE values.
             */
            posfile = (char *)(StartupInfo.lpReserved2) + sizeof( int );
            posfhnd = (UNALIGNED intptr_t *)(posfile + cfi_len);

            /*
             * Ensure cfi_len does not exceed the number of supported
             * handles!
             */
            cfi_len = __min( cfi_len, _NHANDLE_ );

            /*
             * Allocate sufficient arrays of ioinfo structs to hold inherited
             * file information.
             */
            for ( i = 1 ; _nhandle < cfi_len ; i++ ) {

                /*
                 * Allocate another array of ioinfo structs
                 */
                if ( (pio = _calloc_crt( IOINFO_ARRAY_ELTS, sizeof(ioinfo) ))
                    == NULL )
                {
                    /*
                     * No room for another array of ioinfo structs, reduce
                     * the number of inherited handles we process.
                     */
                    cfi_len = _nhandle;
                    break;
                }

                /*
                 * Update __pioinfo[] and _nhandle
                 */
                __pioinfo[i] = pio;
                _nhandle += IOINFO_ARRAY_ELTS;

                for ( ; pio < __pioinfo[i] + IOINFO_ARRAY_ELTS ; pio++ ) {
                    pio->osfile = 0;
                    pio->osfhnd = (intptr_t)INVALID_HANDLE_VALUE;
                    pio->pipech = 10;
                    pio->lockinitflag = 0;
                    pio->textmode = 0;
                    pio->pipech2[0] = 10;
                    pio->pipech2[1] = 10;
                    pio->dbcsBufferUsed = FALSE;
                    pio->dbcsBuffer = '\0';
                }
            }

            /*
             * Validate and copy the passed file information
             */
            for ( fh = 0 ; fh < cfi_len ; fh++, posfile++, posfhnd++ ) {
                /*
                 * Copy the passed file info iff it appears to describe
                 * an open, valid file or device.
                 *
                 * Note that GetFileType cannot be called for pipe handles 
                 * since it may stop responding if there is blocked read pending on
                 * the pipe in the parent.
                 */
                if ( (*posfhnd != (intptr_t)INVALID_HANDLE_VALUE) &&
                     (*posfhnd != _NO_CONSOLE_FILENO) &&
                     (*posfile & FOPEN) && 
                     ((*posfile & FPIPE) ||
                      (GetFileType( (HANDLE)*posfhnd ) != FILE_TYPE_UNKNOWN)) )
                {
                    pio = _pioinfo( fh );
                    pio->osfhnd = *posfhnd;
                    pio->osfile = *posfile;
                    /* Allocate the lock for this handle. */
                    InitializeCriticalSectionAndSpinCount( &pio->lock, _CRT_SPINCOUNT );
                    pio->lockinitflag++;
                }
            }
        }
#endif // _WIN32_WCE
        /*
         * If valid HANDLE-s for standard input, output and error were not
         * inherited, try to obtain them directly from the OS. Also, set the
         * appropriate bits in the osfile fields.
         */
        for ( fh = 0 ; fh < STDIO_HANDLES_COUNT ; fh++ ) {

            pio = __pioinfo[0] + fh;

            if ( (pio->osfhnd == (intptr_t)INVALID_HANDLE_VALUE) || 
                 (pio->osfhnd == _NO_CONSOLE_FILENO)) {
                /*
                 * mark the handle as open in text mode.
                 */
                pio->osfile = (char)(FOPEN | FTEXT);
#ifdef _WIN32_WCE
                // Initialize stdio with invalid handles. When anyone tries actual
                // IO on these handles we'll catch them & open the console then.
                // However these must be set like it's all A-OK.
                stdfh = (intptr_t)INVALID_HANDLE_VALUE;
                pio->osfile = (char)(FOPEN | FTEXT | FDEV);
#else
                if ( ((stdfh = (intptr_t)GetStdHandle( stdhndl(fh) )) != (intptr_t)INVALID_HANDLE_VALUE) && 
                     (stdfh!=((intptr_t)NULL)) &&
                     ((htype = GetFileType( (HANDLE)stdfh )) != FILE_TYPE_UNKNOWN) )
#endif // _WIN32_WCE
                {
                    /*
                     * obtained a valid HANDLE from GetStdHandle
                     */
                    pio->osfhnd = stdfh;

                    /* Allocate the lock for this handle. */
                    InitializeCriticalSectionAndSpinCount( &pio->lock,_CRT_SPINCOUNT );
                    pio->lockinitflag++;

#ifndef _WIN32_WCE
                    /*
                     * finish setting osfile: determine if it is a character
                     * device or pipe.
                     */
                    if ( (htype & 0xFF) == FILE_TYPE_CHAR )
                        pio->osfile |= FDEV;
                    else if ( (htype & 0xFF) == FILE_TYPE_PIPE )
                        pio->osfile |= FPIPE;
                    
                }
                else {
                    /*
                     * For stdin, stdout & stderr, if there is no valid HANDLE, 
                     * treat the CRT handle as being open in text mode on a 
                     * device with _NO_CONSOLE_FILENO underlying it. We use this
                     * value different from _INVALID_HANDLE_VALUE to distinguish
                     * between a failure in opening a file & a program run 
                     * without a console.
                     */
                    pio->osfile |= FDEV;
                    pio->osfhnd = _NO_CONSOLE_FILENO;
                    
                    /* Also update the corresponding standard IO stream.
                     * Unless stdio was terminated already in __endstdio, 
                     * __piob should be statically initialized in __initstdio.
                     */
                    if (__piob)
                        ((FILE*)__piob[fh])->_file = _NO_CONSOLE_FILENO;
#endif
                }
            }
            else  {
                /*
                 * handle was passed to us by parent process. make
                 * sure it is text mode.
                 */
                pio->osfile |= FTEXT;
            }
        }

    __FINALLY
        _munlock(_OSFHND_LOCK); /* unlock the __pioinfo[] table */
    __END_TRY_FINALLY

    return TRUE;
}

/***
*_ioinit() -
*
*Purpose:
*       Calls _ioInitCallback using one-time initialization to ensure IO initialization 
*       happens only once.
*
*Exit:
*       0 on success, -1 if error encountered
*
*Exceptions:
*
*******************************************************************************/
int _ioinit()
{
	int ret = -1;
#ifndef _WIN32_WCE
  /* IO Execute the initialization callback function */
  if (InitOnceExecuteOnce(&__ioInitOnce, _ioinitCallback, NULL, NULL))
      return 0;
#else
  EnterCriticalSection(&csfStdioInitLock);
  if (!fStdioInited)
	fStdioInited = _ioinitCallback();
  LeaveCriticalSection(&csfStdioInitLock);
  if (fStdioInited)
	  ret = 0;
#endif

  return ret;
}

/***
*_ioterm() -
*
*Purpose:
*       Free the memory holding the ioinfo arrays.
*
*       In the multi-thread case, first walk each array of ioinfo structs and
*       delete any all initialized critical sections (locks).
*
*Entry:
*       No parameters.
*
*Exit:
*       No return value.
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _ioterm (
        void
        )
{
        int i;
        ioinfo *pio;
#ifdef _WIN32_WCE
		EnterCriticalSection(&csfStdioInitLock);
		if(!fStdioInited) goto done;
        for (i=0; i<3; i++)
        {
            /*
             * We should not close the handle if the handle is
             * invalid or if the handle is DEBUG output. CE denotes
             * the DEBUG output as (HANDLE)-2.
             */
            if (_osfhnd(i) != (intptr_t)INVALID_HANDLE_VALUE &&
                _osfhnd(i) != (intptr_t)-2)
                CloseHandle((HANDLE)_osfhnd(i));
        }
#endif

        for ( i = 0 ; i < IOINFO_ARRAYS ; i++ ) {

            if ( __pioinfo[i] != NULL ) {
                /*
                 * Delete any initialized critical sections.
                 */
                for ( pio = __pioinfo[i] ;
                      pio < __pioinfo[i] + IOINFO_ARRAY_ELTS ;
                      pio++ )
                {
                    if ( pio->lockinitflag )
                        DeleteCriticalSection( &(pio->lock) );
                }
                /*
                 * Free the memory which held the ioinfo array.
                 */
                _free_crt( __pioinfo[i] );
                __pioinfo[i] = NULL;
            }
        }
#ifdef _WIN32_WCE
		fStdioInited = FALSE;
done:
		LeaveCriticalSection(&csfStdioInitLock);
#endif
}

