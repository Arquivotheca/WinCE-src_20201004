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
*initctyp.c - contains __init_ctype
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Contains the locale-category initialization function: __init_ctype().
*
*       Each initialization function sets up locale-specific information
*       for their category, for use by functions which are affected by
*       their locale category.
*
*       *** For internal use by setlocale() only ***
*
*Revision History:
*       12-08-91  ETC   Created.
*       12-20-91  ETC   Updated to use new NLSAPI GetLocaleInfo.
*       12-18-92  CFW   Ported to Cuda tree, changed _CALLTYPE4 to _CRTAPI3.
*       01-19-03  CFW   Move to _NEWCTYPETABLE, remove switch.
*       02-08-93  CFW   Bug fixes under _INTL switch.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-20-93  CFW   Check return val.
*       05-20-93  GJF   Include windows.h, not individual win*.h files
*       05-24-93  CFW   Clean up file (brief is evil).
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       09-15-93  CFW   Fix size parameters.
*       09-17-93  CFW   Use unsigned chars.
*       09-22-93  CFW   Use __crtxxx internal NLS API wrapper.
*       09-22-93  CFW   NT merge.
*       11-09-93  CFW   Add code page for __crtxxx().
*       03-31-94  CFW   Include awint.h.
*       04-15-94  GJF   Made definitions of ctype1 and wctype1 conditional
*                       on DLL_FOR_WIN32S.
*       04-18-94  CFW   Pass lcid to _crtGetStringType.
*       09-06-94  CFW   Remove _INTL switch.
*       01-10-95  CFW   Debug CRT allocs.
*       02-02-95  BWT   Update POSIX support
*       03-16-97  RDK   Added error flag to __crtGetStringTypeA.
*       11-25-97  GJF   When necessary, use LOCALE_IDEFAULTANSICODEPAGE,
*                       not LOCALE_IDEFAULTCODEPAGE.
*       06-29-98  GJF   Changed to support multithread scheme - old ctype
*                       tables must be kept around until all affected threads
*                       have updated or terminated.
*       03-05-99  GJF   Added __ctype1_refcount for use in cleaning up 
*                       per-thread ctype info.
*       09-06-00  GB    Made pwctype independent of locale.
*       01-29-01  GB    Added _func function version of data variable used in msvcprt.lib
*                       to work with STATIC_CPPLIB
*       07-07-01  BWT   Cleanup malloc/free abuse - Only free __ctype1/refcount/
*                       newctype1/cbuffer if they're no zero.  Init refcount to zero
*       11-12-01  GB    Implemented new locale with true per thread locale
*                       capablity.
*       04-25-02  GB    Increased the size of _ctype perthread variable and pointed
*                       it at begin+_COFFSET position so that isxxx macro would work
*                       same for signed char, usigned char and EOF.
*       05-28-02  GB    Removed the _ctype and replaced it with __newctype.
*       06-04-04  SJ    VSW#314764 - Passing a _locale_t param to __getlocaleinfo
*                       so that the newly set locale is used.
*       04-01-05  MSL   Integer overflow protection
*       05-06-05  PML   add ___mb_cur_max_l_func
*
*******************************************************************************/

#include <stdlib.h>
#include <windows.h>
#include <locale.h>
#include <setlocal.h>
#include <ctype.h>
#include <malloc.h>
#include <limits.h>
#include <awint.h>
#include <dbgint.h>
#include <mtdll.h>

#define _CTABSIZE   257     /* size of ctype tables */

/***
*int __init_ctype() - initialization for LC_CTYPE locale category.
*
*Purpose:
*       In non-C locales, preread ctype tables for chars and wide-chars.
*       Old tables are freed when new tables are fully established, else
*       the old tables remain intact (as if original state unaltered).
*       The leadbyte table is implemented as the high bit in ctype1.
*
*       In the C locale, ctype tables are freed, and pointers point to
*       the static ctype table.
*
*       Tables contain 257 entries: -1 to 256.
*       Table pointers point to entry 0 (to allow index -1).
*
*Entry:
*       None.
*
*Exit:
*       0 success
*       1 fail
*
*Exceptions:
*
*******************************************************************************/

int __cdecl __init_ctype (
        pthreadlocinfo ploci
        )
{
#if     defined(_POSIX_)
    return(0);
#else   /* _POSIX_ */
    int *refcount = NULL;
    /* non-C locale table for char's */
    unsigned short *newctype1 = NULL;          /* temp new table */
    unsigned char *newclmap = NULL;            /* temp new map table */
    unsigned char *newcumap = NULL;            /* temp new map table */

    /* non-C locale table for wchar_t's */

    unsigned char *cbuffer = NULL;      /* char working buffer */

    int i;                              /* general purpose counter */
    unsigned char *cp;                  /* char pointer */
    CPINFO cpInfo;                      /* struct for use with GetCPInfo */
    int mb_cur_max;
    _locale_tstruct locinfo;

    locinfo.locinfo = ploci;
    locinfo.mbcinfo = 0;
    
    /* allocate and set up buffers before destroying old ones */
    /* codepage will be restored by setlocale if error */

    if (ploci->locale_name[LC_CTYPE] != NULL)
    {
        if (ploci->lc_codepage == 0)
        { /* code page was not specified */
            if ( __getlocaleinfo( &locinfo, LC_INT_TYPE,
                                  ploci->locale_name[LC_CTYPE],
                                  LOCALE_IDEFAULTANSICODEPAGE,
                                  (char **)&ploci->lc_codepage ) )
                goto error_cleanup;
        }

        /* allocate a new (thread) reference counter */
        refcount = (int *)_malloc_crt(sizeof(int));

            /* allocate new buffers for tables */
            newctype1 = (unsigned short *)
                _calloc_crt((_COFFSET+_CTABSIZE), sizeof(unsigned short));
            newclmap = (char *)
                _calloc_crt((_COFFSET+_CTABSIZE), sizeof(char));
            newcumap = (char *)
                _calloc_crt((_COFFSET+_CTABSIZE), sizeof(char));
            cbuffer = (unsigned char *)
                _calloc_crt (_CTABSIZE, sizeof(char));

        if (!refcount || !newctype1 || !cbuffer || !newclmap || !newcumap)
            goto error_cleanup;

        *refcount = 0;

        /* construct string composed of first 256 chars in sequence */
        for (cp=cbuffer, i=0; i<_CTABSIZE-1; i++)
            *cp++ = (unsigned char)i;

        if (GetCPInfo( ploci->lc_codepage, &cpInfo) == FALSE)
            goto error_cleanup;

        if (cpInfo.MaxCharSize > MB_LEN_MAX)
            goto error_cleanup;

        mb_cur_max = (unsigned short) cpInfo.MaxCharSize;


        /*
         * LCMapString will map past NULL. Must find NULL if in string
         * before cchSrc characters.
         */
        if ( __crtLCMapStringA(NULL,
                    ploci->locale_name[LC_CTYPE], 
                    LCMAP_LOWERCASE,
                    cbuffer+1,
                    _CTABSIZE-2,
                    newclmap+2+_COFFSET,
                    _CTABSIZE-2,
                    ploci->lc_codepage,
                    FALSE ) == FALSE)
            goto error_cleanup;

        if ( __crtLCMapStringA(NULL,
                    ploci->locale_name[LC_CTYPE], 
                    LCMAP_UPPERCASE,
                    cbuffer+1,
                    _CTABSIZE-2,
                    newcumap+2+_COFFSET,
                    _CTABSIZE-2,
                    ploci->lc_codepage,
                    FALSE ) == FALSE)
            goto error_cleanup;

        /* zero out leadbytes so GetStringType doesn't interpret as multi-byte chars */
        if (mb_cur_max > 1)
        {
            for (cp = (unsigned char *)cpInfo.LeadByte; cp[0] && cp[1]; cp += 2)
            {
                for (i = cp[0]; i <= cp[1]; i++)
                    cbuffer[i] = ' ';
            }
        }

        /* convert to newctype1 table - ignore invalid char errors */
        if ( __crtGetStringTypeA(NULL,  CT_CTYPE1,
                                  cbuffer,
                                  _CTABSIZE-1,
                                  newctype1+1+_COFFSET,
                                  ploci->lc_codepage,
                                  FALSE ) == FALSE )
            goto error_cleanup;

        newctype1[_COFFSET] = 0; /* entry for EOF */
        newclmap[_COFFSET] = 0;
        newcumap[_COFFSET] = 0;
        newclmap[_COFFSET+1] = 0; /* entry for null */
        newcumap[_COFFSET+1] = 0; /* entry for null */

        /* ignore DefaultChar */

        /* mark lead-byte entries in newctype1 table */
        if (mb_cur_max > 1)
        {
            for (cp = (unsigned char *)cpInfo.LeadByte; cp[0] && cp[1]; cp += 2)
            {
                for (i = cp[0]; i <= cp[1]; i++)
                    newctype1[_COFFSET+i+1] = _LEADBYTE;
            }
        }
        /* copy last-1 _COFFSET unsigned short to front
         * note -1, we don't really want to copy 0xff
         */
        memcpy(newctype1,newctype1+_CTABSIZE-1,_COFFSET*sizeof(unsigned short));
        memcpy(newclmap,newclmap+_CTABSIZE-1,_COFFSET*sizeof(char));
        memcpy(newcumap,newcumap+_CTABSIZE-1,_COFFSET*sizeof(char));

        /* free old tables */
        if ((ploci->ctype1_refcount) &&
            (InterlockedDecrement(ploci->ctype1_refcount) == 0))
        {
            _ASSERT(0);
            _free_crt(ploci->ctype1 - _COFFSET);
            _free_crt((char *)(ploci->pclmap - _COFFSET - 1));
            _free_crt((char *)(ploci->pcumap - _COFFSET - 1));
            _free_crt(ploci->ctype1_refcount);
        }
        (*refcount) = 1;
        ploci->ctype1_refcount = refcount;
        /* set pointers to point to entry 0 of tables */
        ploci->pctype = newctype1 + 1 + _COFFSET;
        ploci->ctype1 = newctype1 + _COFFSET;
        ploci->pclmap = newclmap + 1 + _COFFSET;
        ploci->pcumap = newcumap + 1 + _COFFSET;
        ploci->mb_cur_max = mb_cur_max;

        /* cleanup and return success */
        _free_crt (cbuffer);
        return 0;

error_cleanup:
        _free_crt (refcount);
        _free_crt (newctype1);
        _free_crt (newclmap);
        _free_crt (newcumap);
        _free_crt (cbuffer);
        return 1;

    } else {

        if ( (ploci->ctype1_refcount != NULL)&&
             (InterlockedDecrement(ploci->ctype1_refcount) == 0))
        {
            _ASSERTE(ploci->ctype1_refcount > 0);
        }
        ploci->ctype1_refcount = NULL;
        ploci->ctype1 = NULL;
        ploci->pctype = __newctype + 1 + _COFFSET;
        ploci->pclmap = __newclmap + 1 + _COFFSET;
        ploci->pcumap = __newcumap + 1 + _COFFSET;
        ploci->mb_cur_max = 1;

        return 0;
    }
#endif   /* _POSIX_ */
}

/* Define a number of functions which exist so, under _STATIC_CPPLIB, the
 * static multithread C++ Library libcpmt.lib can access data found in the
 * main CRT DLL without using __declspec(dllimport).
 */

_CRTIMP int __cdecl ___mb_cur_max_func(void)
{
#if _PTD_LOCALE_SUPPORT
    /*
     * Note that we don't need _LocaleUpdate in this function.
     * The main reason being, that this is a leaf function in
     * locale usage terms.
     */
    _ptiddata ptd = _getptd();
    pthreadlocinfo ptloci = ptd->ptlocinfo;

    __UPDATE_LOCALE(ptd, ptloci);

    return ptloci->mb_cur_max;
#else
    int retval;
    _mlock(_SETLOCALE_LOCK);
    __try 
    {
        retval = __ptlocinfo->mb_cur_max;
    }
    __finally
    {
        _munlock(_SETLOCALE_LOCK);
    }
    return retval;
#endif  /* _PTD_LOCALE_SUPPORT */
}

_CRTIMP int __cdecl ___mb_cur_max_l_func(_locale_t loc)
{
    return (loc == NULL) ? ___mb_cur_max_func() : loc->locinfo->mb_cur_max;
}

_CRTIMP UINT __cdecl ___lc_codepage_func(void)
{
#if _PTD_LOCALE_SUPPORT
    /*
     * Note that we don't need _LocaleUpdate in this function.
     * The main reason being, that this is a leaf function in
     * locale usage terms.
     */
    _ptiddata ptd = _getptd();
    pthreadlocinfo ptloci = ptd->ptlocinfo;

    __UPDATE_LOCALE(ptd, ptloci);

    return ptloci->lc_codepage;
#else
    UINT retval;
    _mlock(_SETLOCALE_LOCK);
    __try
    {
        retval = __ptlocinfo->lc_codepage;
    }
    __finally
    {
        _munlock(_SETLOCALE_LOCK);
    }
    return retval;
#endif  /* _PTD_LOCALE_SUPPORT */
}

_CRTIMP UINT __cdecl ___lc_collate_cp_func(void)
{
#if _PTD_LOCALE_SUPPORT
    /*
     * Note that we don't need _LocaleUpdate in this function.
     * The main reason being, that this is a leaf function in
     * locale usage terms.
     */
    _ptiddata ptd = _getptd();
    pthreadlocinfo ptloci = ptd->ptlocinfo;

    __UPDATE_LOCALE(ptd, ptloci);

    return ptloci->lc_collate_cp;
#else
    UINT retval;
    _mlock(_SETLOCALE_LOCK);
    __try
    {
        retval = __ptlocinfo->lc_collate_cp;
    }
    __finally
    {
        _munlock(_SETLOCALE_LOCK);
    }
    return retval;
#endif  /* _PTD_LOCALE_SUPPORT */
}

_CRTIMP wchar_t** __cdecl ___lc_locale_name_func(void)
{
#if _PTD_LOCALE_SUPPORT
    /*
     * Note that we don't need _LocaleUpdate in this function.
     * The main reason being, that this is a leaf function in
     * locale usage terms.
     */
    _ptiddata ptd = _getptd();
    pthreadlocinfo ptloci = ptd->ptlocinfo;

    __UPDATE_LOCALE(ptd, ptloci);

    return (wchar_t**)ptloci->locale_name;
#else
    wchar_t** retval;
    _mlock(_SETLOCALE_LOCK);
    __try
    {
        retval = (wchar_t**) __ptlocinfo->locale_name;
    }
    __finally
    {
        _munlock(_SETLOCALE_LOCK);
    }
    return retval;
#endif  /* _PTD_LOCALE_SUPPORT */
}
