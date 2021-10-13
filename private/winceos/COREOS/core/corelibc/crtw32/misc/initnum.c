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
*initnum.c - contains __init_numeric
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Contains the locale-category initialization function: __init_numeric().
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
*       12-29-92  CFW   Updated to use new _getlocaleinfo wrapper function.
*       01-25-93  KRS   Change interface to _getlocaleinfo again.
*       02-08-93  CFW   Added _lconv_static_*.
*       02-17-93  CFW   Removed debugging print statement.
*       03-17-93  CFW   C locale thousands sep is "", not ",".
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-08-93  SKS   Replace strdup() with ANSI-conforming _strdup()
*       04-20-93  CFW   Check return val.
*       05-20-93  GJF   Include windows.h, not individual win*.h files
*       05-24-93  CFW   Clean up file (brief is evil).
*       06-11-93  CFW   Now inithelp takes void *.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       09-23-93  GJF   Merged NT SDK and Cuda versions.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       04-06-94  GJF   Removed declaration of __lconv (it is declared in
*                       setlocal.h). Renamed static vars, decimal_point
*                       thousands_sep and grouping to dec_pnt, thous_sep
*                       and grping (resp.). Made the definitions of these
*                       conditional on DLL_FOR_WIN32S.
*       08-02-94  CFW   Change "3;0" to "\3" for grouping as per ANSI.
*       09-06-94  CFW   Remove _INTL switch.
*       01-10-95  CFW   Debug CRT allocs.
*       01-18-95  GJF   Fixed bug introduced with the change above - resetting
*                       to the C locale didn't reset the thousand_sep and
*                       grouping fields correctly.
*       02-06-95  CFW   assert -> _ASSERTE.
*       07-06-98  GJF   Changed to support new multithread scheme - old lconv
*                       structs must be kept around until all affected threads
*                       have updated or terminated.
*       12-08-98  GJF   Fixed logic in __free_lconv_num.
*       01-25-99  GJF   No, I didn't!  Try again...
*       03-15-99  GJF   Added __lconv_num_refcount
*       04-24-99  PML   Added __lconv_intl_refcount
*       09-08-00  GB    Fixed leak of __lconv_intl in init_numeric for single
*                       thread case.
*       10-12-00  PML   Don't call fix_grouping if error detected (vs7#169596)
*       11-05-00  PML   Fixed double-free of __lconv_intl (vs7#181380)
*       11-12-01  GB    Implemented new locale with true per thread locale
*                       capablity.
*       06-04-04  SJ    VSW#314764 - Passing a _locale_t param to __getlocaleinfo
*                       so that the newly set locale is used.
*
*******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <locale.h>
#include <setlocal.h>
#include <malloc.h>
#include <nlsint.h>
#include <dbgint.h>
#include <mtdll.h>

void __cdecl __free_lconv_num(struct lconv *);

static void fix_grouping(
        char *grouping
        )
{
    /*
     * ANSI specifies that the fields should contain "\3" [\3\0] to indicate
     * thousands groupings (100,000,000.00 for example).
     * NT uses "3;0"; ASCII 3 instead of value 3 and the ';' is extra.
     * So here we convert the NT version to the ANSI version.
     */

    while (*grouping)
    {
        /* convert '3' to '\3' */
        if (*grouping >= '0' && *grouping <= '9')
        {    
            *grouping = *grouping - '0';
            grouping++;
        }

        /* remove ';' */
        else if (*grouping == ';')
        {
            char *tmp = grouping;

            do
                *tmp = *(tmp+1);
            while (*++tmp);
        }

        /* unknown (illegal) character, ignore */
        else
            grouping++;
    }
}

/***
*int __init_numeric() - initialization for LC_NUMERIC locale category.
*
*Purpose:
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

int __cdecl __init_numeric (
        pthreadlocinfo ploci
        )
{
    struct lconv *lc;
    int ret = 0;
    wchar_t* ctrylocalename;
    int *lc_refcount;
    int *lconv_num_refcount = NULL;
    _locale_tstruct locinfo;

    locinfo.locinfo = ploci;
    locinfo.mbcinfo = 0;

    if ( (ploci->locale_name[LC_NUMERIC] != NULL) ||
         (ploci->locale_name[LC_MONETARY] != NULL) )
    {
        /*
         * Allocate structure filled with NULL pointers
         */
        if ( (lc = (struct lconv *)_calloc_crt(1, sizeof(struct lconv)))
             == NULL )
            return 1;

        /*
         * Copy over all fields (esp., the monetary category)
         */
        *lc = *ploci->lconv;

        /*
         * Allocate a new reference counter for the lconv structure
         */
        if ( (lc_refcount = _malloc_crt(sizeof(int))) == NULL )
        {
            _free_crt(lc);
            return 1;
        }
        *lc_refcount = 0;

        if ( ploci->locale_name[LC_NUMERIC] != NULL )
        {
            /*
             * Allocate a new reference counter for the numeric info
             */
            if ( (lconv_num_refcount = _malloc_crt(sizeof(int))) == NULL )
            {
                _free_crt(lc);
                _free_crt(lc_refcount);
                return 1;
            }
            *lconv_num_refcount = 0;

            /* 
             * Numeric data is country--not language--dependent.
             */
            ctrylocalename = ploci->locale_name[LC_NUMERIC];

            ret |= __getlocaleinfo(&locinfo, LC_STR_TYPE, ctrylocalename, LOCALE_SDECIMAL,
                    (void *)&lc->decimal_point);
            ret |= __getlocaleinfo(&locinfo, LC_STR_TYPE, ctrylocalename, LOCALE_STHOUSAND,
                    (void *)&lc->thousands_sep);
            ret |= __getlocaleinfo(&locinfo, LC_STR_TYPE, ctrylocalename, LOCALE_SGROUPING,
                    (void *)&lc->grouping);

            ret |= __getlocaleinfo(&locinfo, LC_WSTR_TYPE, ctrylocalename, LOCALE_SDECIMAL,
                    (void *)&lc->_W_decimal_point);
            ret |= __getlocaleinfo(&locinfo, LC_WSTR_TYPE, ctrylocalename, LOCALE_STHOUSAND,
                    (void *)&lc->_W_thousands_sep);

            if (ret) {
                    /* Clean up before returning failure */
                    __free_lconv_num(lc);
                    _free_crt(lc);
                    _free_crt(lc_refcount);
                    return -1;
            }

            fix_grouping(lc->grouping);
        }
        else {
            /*
             * C locale for just the numeric category.
             */
            /*
             * NULL out the reference count pointer
             */
            lconv_num_refcount = NULL;
            lc->decimal_point = __lconv_c.decimal_point;
            lc->thousands_sep = __lconv_c.thousands_sep;
            lc->grouping = __lconv_c.grouping;
            lc->_W_decimal_point = __lconv_c._W_decimal_point;
            lc->_W_thousands_sep = __lconv_c._W_thousands_sep;
        }
        (*lc_refcount) = 1;
        if (lconv_num_refcount)
            (*lconv_num_refcount) = 1;
    }

    else {
        /*
         * C locale for BOTH numeric and monetary categories.
         */
        lconv_num_refcount = NULL;
        lc_refcount = NULL;
        lc = &__lconv_c;           /* point to new one */
    }
    /*
     * If this is part of LC_ALL, then we need to free the old ploci->lconv
     * set up in init_monetary() before this.
     */
    if ( (ploci->lconv_num_refcount != NULL) &&
         (InterlockedDecrement(ploci->lconv_num_refcount) == 0))
    {
        _ASSERTE(ploci->lconv_num_refcount > 0);
    }
    if ( (ploci->lconv_intl_refcount != NULL) &&
         (InterlockedDecrement(ploci->lconv_intl_refcount) == 0))
    {
        _free_crt(ploci->lconv_intl_refcount);
        _free_crt(ploci->lconv);
    }

    ploci->lconv_num_refcount = lconv_num_refcount;
    ploci->lconv_intl_refcount = lc_refcount;

    ploci->lconv = lc;
    return 0;
}

/*
 *  Free the lconv numeric strings.
 *  Numeric values do not need to be freed.
 */
void __cdecl __free_lconv_num(
        struct lconv *l
        )
{
    if (l == NULL)
        return;

    if ( l->decimal_point != __lconv_c.decimal_point )
        _free_crt(l->decimal_point);

    if ( l->thousands_sep != __lconv_c.thousands_sep )
        _free_crt(l->thousands_sep);

    if ( l->grouping != __lconv_c.grouping )
        _free_crt(l->grouping);

    if ( l->_W_decimal_point != __lconv_c._W_decimal_point )
        _free_crt(l->_W_decimal_point);

    if ( l->_W_thousands_sep != __lconv_c._W_thousands_sep )
        _free_crt(l->_W_thousands_sep);
}
