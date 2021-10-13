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
*localref.c - Contains the __[add|remove]localeref() functions.
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Contains the __[add|remove]localeref() functions.
*
*Revision History:
*       10-10-08  RMS   Module created.
*
*******************************************************************************/

#include <locale.h>
#include <internal.h>

#include <cruntime.h>
#include <setlocal.h>
#include <mtdll.h>
#include <stdlib.h>
#include <dbgint.h>
#include <rterr.h>

void __cdecl __free_lconv_mon(struct lconv *);
void __cdecl __free_lconv_num(struct lconv *);
void __cdecl __free_lc_time(struct __lc_time_data *);

extern struct __lc_time_data __lc_time_c;
extern wchar_t __wclocalestr[];

/***
* __addlocaleref(pthreadlocinfo ptloci)
*
* Purpose:
*       Increment the refrence count for each element in the localeinfo struct.
*
*******************************************************************************/
void __cdecl __addlocaleref(pthreadlocinfo ptloci)
{
    int category;

    InterlockedIncrement(&(ptloci->refcount));

    if ( ptloci->lconv_intl_refcount != NULL )
        InterlockedIncrement(ptloci->lconv_intl_refcount);
    
    if ( ptloci->lconv_mon_refcount != NULL )
        InterlockedIncrement(ptloci->lconv_mon_refcount);
    
    if ( ptloci->lconv_num_refcount != NULL )
        InterlockedIncrement(ptloci->lconv_num_refcount);
    
    if ( ptloci->ctype1_refcount != NULL )
        InterlockedIncrement(ptloci->ctype1_refcount);
    
    for (category = LC_MIN; category <= LC_MAX; ++category) {
        if (ptloci->lc_category[category].wlocale != __wclocalestr &&
            ptloci->lc_category[category].wrefcount != NULL) 
            InterlockedIncrement(ptloci->lc_category[category].wrefcount);

        if (ptloci->lc_category[category].locale != NULL &&
            ptloci->lc_category[category].refcount != NULL) 
            InterlockedIncrement(ptloci->lc_category[category].refcount);
    }

    InterlockedIncrement(&(ptloci->lc_time_curr->refcount));
}

/***
* __removelocaleref(pthreadlocinfo ptloci)
*
* Purpose:
*       Decrement the refrence count for each elemnt in the localeinfo struct.
*
******************************************************************************/
void * __cdecl __removelocaleref(pthreadlocinfo ptloci)
{
    int category;

    if ( ptloci != NULL )
    {
        InterlockedDecrement(&(ptloci->refcount));

        if ( ptloci->lconv_intl_refcount != NULL )
            InterlockedDecrement(ptloci->lconv_intl_refcount);

        if ( ptloci->lconv_mon_refcount != NULL )
            InterlockedDecrement(ptloci->lconv_mon_refcount);

        if ( ptloci->lconv_num_refcount != NULL )
            InterlockedDecrement(ptloci->lconv_num_refcount);

        if ( ptloci->ctype1_refcount != NULL )
            InterlockedDecrement(ptloci->ctype1_refcount);

        for (category = LC_MIN; category <= LC_MAX; ++category) {
            if (ptloci->lc_category[category].wlocale != __wclocalestr &&
                ptloci->lc_category[category].wrefcount != NULL) 
                InterlockedDecrement(ptloci->lc_category[category].wrefcount);

            if (ptloci->lc_category[category].locale != NULL &&
                ptloci->lc_category[category].refcount != NULL) 
                InterlockedDecrement(ptloci->lc_category[category].refcount);
        }

        InterlockedDecrement(&(ptloci->lc_time_curr->refcount));
    }

    return ptloci;
}


/***
*__freetlocinfo() - free threadlocinfo
*
*Purpose:
*       Free up the per-thread locale info structure specified by the passed
*       pointer.
*
*Entry:
*       pthreadlocinfo ptloci
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

void __cdecl __freetlocinfo (
        pthreadlocinfo ptloci
        )
{
    int category;    
    /*
     * Free up lconv struct
     */
    if ( (ptloci->lconv != NULL) &&
         (ptloci->lconv != &__lconv_c) &&
         ((ptloci->lconv_intl_refcount != NULL) &&
         (*(ptloci->lconv_intl_refcount) == 0)))
    {
        if ( (ptloci->lconv_mon_refcount != NULL) &&
             (*(ptloci->lconv_mon_refcount) == 0))
        {
            _free_crt(ptloci->lconv_mon_refcount);
            __free_lconv_mon(ptloci->lconv);
        }

        if ( (ptloci->lconv_num_refcount != NULL) &&
             (*(ptloci->lconv_num_refcount) == 0))
        {
            _free_crt(ptloci->lconv_num_refcount);
            __free_lconv_num(ptloci->lconv);
        }

        _free_crt(ptloci->lconv_intl_refcount);
        _free_crt(ptloci->lconv);
    }

    /*
     * Free up ctype tables
     */
    if ( (ptloci->ctype1_refcount != NULL) &&
         (*(ptloci->ctype1_refcount) == 0) )
    {
        _free_crt(ptloci->ctype1-_COFFSET);
        _free_crt((char *)(ptloci->pclmap - _COFFSET - 1));
        _free_crt((char *)(ptloci->pcumap - _COFFSET - 1));
        _free_crt(ptloci->ctype1_refcount);
    }

    /*
     * Free up the __lc_time_data struct
     */
    if ( ptloci->lc_time_curr != &__lc_time_c &&
         ((ptloci->lc_time_curr->refcount) == 0) )
    {
        __free_lc_time(ptloci->lc_time_curr);
        _free_crt(ptloci->lc_time_curr);
    }

    for (category = LC_MIN; category <= LC_MAX; ++category) {
        if ((ptloci->lc_category[category].wlocale != __wclocalestr) &&
              (ptloci->lc_category[category].wrefcount != NULL) &&
              (*ptloci->lc_category[category].wrefcount == 0) )
        {
            _free_crt(ptloci->lc_category[category].wrefcount);
            _free_crt(ptloci->locale_name[category]);
        }

        _ASSERTE(((ptloci->lc_category[category].locale != NULL) && (ptloci->lc_category[category].refcount != NULL)) ||
                 ((ptloci->lc_category[category].locale == NULL) && (ptloci->lc_category[category].refcount == NULL)));

        if ((ptloci->lc_category[category].locale != NULL) &&
              (ptloci->lc_category[category].refcount != NULL) &&
              (*ptloci->lc_category[category].refcount == 0) )
        {
            _free_crt(ptloci->lc_category[category].refcount);
        }
    }

    /*
     * Free up the threadlocinfo struct
     */
    _free_crt(ptloci);
}


/***
*
* _updatelocinfoEx_nolock(pthreadlocinfo *pptlocid, pthreadlocinfo ptlocis)
*
* Purpose:
*       Update *pptlocid to ptlocis. This might include freeing contents of *pptlocid.
*
******************************************************************************/
pthreadlocinfo __cdecl _updatetlocinfoEx_nolock(
    pthreadlocinfo *pptlocid,
    pthreadlocinfo ptlocis)
{
    pthreadlocinfo ptloci;

    if (ptlocis == NULL || pptlocid == NULL)
        return NULL;

    ptloci = *pptlocid;
    if ( ptloci != ptlocis)
    {
        /*
         * Update to the current locale info structure and increment the
         * reference counts.
         */
        *pptlocid = ptlocis;
        __addlocaleref(ptlocis);

        /*
         * Decrement the reference counts in the old locale info
         * structure.
         */
        if ( ptloci != NULL )
        {
            __removelocaleref(ptloci);
        }

        /*
         * Free the old locale info structure, if necessary.  Must be done
         * after incrementing reference counts in current locale in case
         * any refcounts are shared with the old locale.
         */
        if ( (ptloci != NULL) &&
             (ptloci->refcount == 0) &&
             (ptloci != &__initiallocinfo) )
            __freetlocinfo(ptloci);

    }

    return ptlocis;
}

#if _PTD_LOCALE_SUPPORT
/***
*__updatetlocinfo() - refresh the thread's locale info
*
*Purpose:
*       If this thread does not have it's ownlocale which means that either
*       ownlocale flag in ptd is not set or ptd->ptloci == NULL, then Update
*       the current thread's reference to the locale information to match the
*       current global locale info. Decrement the reference on the old locale
*       information struct and if this count is now zero (so that no threads
*       are using it), free it.
*
*Entry:
*
*Exit:
*
*       if (!_getptd()->ownlocale || _getptd()->ptlocinfo == NULL)
*           _getptd()->ptlocinfo == __ptlocinfo
*       else
*           _getptd()->ptlocinfo
*
*Exceptions:
*
*******************************************************************************/
pthreadlocinfo __cdecl __updatetlocinfo(void)
{
    pthreadlocinfo ptloci;
    _ptiddata ptd = _getptd();

    if (!(ptd->_ownlocale & __globallocalestatus)|| !ptd->ptlocinfo) {
        _mlock(_SETLOCALE_LOCK);

        __try 
        {
            ptloci = _updatetlocinfoEx_nolock(&ptd->ptlocinfo, __ptlocinfo);
        }
        __finally
        {
            _munlock(_SETLOCALE_LOCK);
        }
    } else {
        ptloci = _getptd()->ptlocinfo;
    }

    if(!ptloci)
    {
        _amsg_exit(_RT_LOCALE);
    }

    return ptloci;
}
#else

/***
* pthreadlocinfo __get_locinfo_addref()
*
* Purpose:
*       Returns the global locinfo (pointed to by __ptlocinfo) and increments 
*       its ref count.
******************************************************************************/
pthreadlocinfo __cdecl __get_locinfo_addref(void)
{
    pthreadlocinfo ptloci;
    _mlock(_SETLOCALE_LOCK);
    __try
    {
        ptloci = __ptlocinfo;
        __addlocaleref(ptloci);
    }
    __finally
    {
        _munlock(_SETLOCALE_LOCK);
    }
    return ptloci;
}


/***
* void __locinfo_release(pthreadlocinfo*)
*
* Purpose:
*       Decrement the ref count of locinfo structure.  Free the memory when the
*       ref count reaches zero.
******************************************************************************/
void __cdecl __locinfo_release(pthreadlocinfo* pptloci)
{
    _ASSERTE(pptloci != NULL);
    if (*pptloci)
    {
        _mlock(_SETLOCALE_LOCK);
        __try {
            __removelocaleref(*pptloci);
            if ( (*pptloci != NULL) &&
                 ((*pptloci)->refcount == 0) &&
                 (*pptloci != &__initiallocinfo) )
                __freetlocinfo(*pptloci);
            *pptloci = NULL;
        }
        __finally {
            _munlock(_SETLOCALE_LOCK);
        }
    }
}

#endif  /* _PTD_LOCALE_SUPPORT */