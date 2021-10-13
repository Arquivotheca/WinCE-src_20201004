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
*nlsdata.c - globals for international library - locale handles and code page
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This module defines the locale handles and code page.  The handles are
*       required by almost all locale dependent functions.
*       Also contains the global __mb_cur_max
*
*Revision History:
*       12-01-91  ETC   Created.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       04-12-94  GJF   Made definitions of __lc_handle and __lc_codepage
*                       conditional on ndef DLL_FOR_WIN32S
*       01-12-98  GJF   Added __lc_collate_cp.
*       26-01-00  GB    Added __lc_clike.
*       11-12-01  GB    Implemented new locale with true per thread locale
*                       capablity.
*       01-30-02  GB    added global data variable for single threaded
*                       library __setloc_struct
*       04-01-04  SJ    Design Changes in perthread locale - VSW#141214. Moved
*                       __globallocalestatus to it's own file.
*
*******************************************************************************/

#include <locale.h>
#include <setlocal.h>
#include <mbctype.h>

/*
 *  Value of MB_CUR_MAX macro.
 */
int __mb_cur_max = 1;

__declspec(selectany) wchar_t __wclocalestr[] = L"C";

__declspec(selectany) struct __lc_time_data __lc_time_c = {
    {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"},

    {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
     "Friday", "Saturday"},

    {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
     "Sep", "Oct", "Nov", "Dec"},

    {"January", "February", "March", "April", "May", "June",
     "July", "August", "September", "October",
     "November", "December"},

    {"AM", "PM"},

    "MM/dd/yy",
    "dddd, MMMM dd, yyyy",
    "HH:mm:ss",

    1,
    0,

    { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" },

    { L"Sunday", L"Monday", L"Tuesday", L"Wednesday",
      L"Thursday", L"Friday", L"Saturday" },

    { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul",
      L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" },

    { L"January", L"February", L"March", L"April", L"May",
      L"June", L"July", L"August", L"September", L"October",
      L"November", L"December" },

    { L"AM", L"PM" },

    L"MM/dd/yy",
    L"dddd, MMMM dd, yyyy",
    L"HH:mm:ss",
    L"en-US"
};


/*
 * initial locale information struct, set to the C locale. Used only until the
 * first call to setlocale()
 */
__declspec(selectany) threadlocinfo __initiallocinfo = {
    1,                                        /* refcount                 */
    _CLOCALECP,                               /* lc_codepage              */
    _CLOCALECP,                               /* lc_collate_cp            */
    _CLOCALECP,                               /* lc_time_cp               */
    {   {NULL, NULL, NULL, NULL},             /* lc_category[LC_ALL]      */
        {NULL, __wclocalestr, NULL, NULL},    /* lc_category[LC_COLLATE]  */
        {NULL, __wclocalestr, NULL, NULL},    /* lc_category[LC_CTYPE]    */
        {NULL, __wclocalestr, NULL, NULL},    /* lc_category[LC_MONETARY] */
        {NULL, __wclocalestr, NULL, NULL},    /* lc_category[LC_NUMERIC]  */
        {NULL, __wclocalestr, NULL, NULL}     /* lc_category[LC_TIME]     */
    },
    1,                                        /* lc_clike                 */
    1,                                        /* mb_cur_max               */
    NULL,                                     /* lconv_intl_refcount      */
    NULL,                                     /* lconv_num_refcount       */
    NULL,                                     /* lconv_mon_refcount       */
    &__lconv_c,                               /* lconv                    */
    NULL,                                     /* ctype1_refcount          */
    NULL,                                     /* ctype1                   */
    __newctype + 128,                         /* pctype                   */
    __newclmap + 128,                         /* pclmap                   */
    __newcumap + 128,                         /* pcumap                   */
    &__lc_time_c,                             /* lc_time_curr             */
    {   NULL,             /* locale_name[LC_ALL]      */
        NULL,             /* locale_name[LC_COLLATE]  */
        NULL,             /* locale_name[LC_CTYPE]    */
        NULL,             /* locale_name[LC_MONETARY] */
        NULL,             /* locale_name[LC_NUMERIC]  */
        NULL              /* locale_name[LC_TIME]     */
    }
};

/* 
 * global pointer to the current per-thread locale information structure.
 */
__declspec(selectany) pthreadlocinfo __ptlocinfo = &__initiallocinfo;

__declspec(selectany) _locale_tstruct __initiallocalestructinfo =
{
    &__initiallocinfo,
    &__initialmbcinfo
};
