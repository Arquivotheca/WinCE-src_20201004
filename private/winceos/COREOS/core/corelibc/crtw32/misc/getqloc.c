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
*getqloc.c - get qualified locale
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines __get_qualified_locale - get complete locale information
*
*Revision History:
*       12-11-92  CFW   initial version
*       01-08-93  CFW   cleaned up file
*       02-02-93  CFW   Added test for NULL input string fields
*       02-08-93  CFW   Casts to remove warnings.
*       02-18-93  CFW   Removed debugging support routines, changed copyright.
*       02-18-93  CFW   Removed debugging support routines, changed copyright.
*       03-01-93  CFW   Test code page validity, use ANSI comments.
*       03-02-93  CFW   Add ISO 3166 3-letter country codes, verify country table.
*       03-04-93  CFW   Call IsValidCodePage to test code page vailidity.
*       03-10-93  CFW   Protect table testing code.
*       03-17-93  CFW   Add __ to lang & ctry info tables, move defs to setlocal.h.
*       03-23-93  CFW   Make internal functions static, add _ to GetQualifiedLocale.
*       03-24-93  CFW   Change to _get_qualified_locale, support ".codepage".
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-08-93  SKS   Replace stricmp() with ANSI-conforming _stricmp()
*       04-20-93  CFW   Enable all strange countries.
*       05-20-93  GJF   Include windows.h, not individual win*.h files
*       05-24-93  CFW   Clean up file (brief is evil).
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       09-22-93  CFW   Use __crtxxx internal NLS API wrapper.
*       11-09-93  CFW   Add code page for __crtxxx().
*       11-11-93  CFW   Verify ALL code pages.
*       02-04-94  CFW   Remove unused param, clean up, new languages,
*                       default is ANSI, allow .ACP/.OCP for code page.
*       02-07-94  CFW   Back to OEM, NT 3.1 doesn't handle ANSI properly.
*       02-24-94  CFW   Back to ANSI, we'll use our own table.
*       04-04-94  CFW   Update NT-supported countries/languages.
*       04-25-94  CFW   Update countries to new ISO 3166 (1993) standard.
*       02-02-95  BWT   Update _POSIX_ support
*       04-07-95  CFW   Remove NT 3.1 hacks, reduce string space.
*       02-14-97  RDK   Complete rewrite to dynamically use the installed
*                       system locales to determine the best match for the
*                       language and/or country specified.
*       02-19-97  RDK   Do not use iPrimaryLen if zero.
*       02-24-97  RDK   For Win95, simulate nonfunctional GetLocaleInfoA
*                       calls with hard-coded values.
*       07-07-97  GJF   Made arrays of data global and selectany so linker 
*                       can eliminate them when possible.
*       10-02-98  GJF   Replaced IsThisWindowsNT with test of _osplatform.
*       11-10-99  PML   Try untranslated language string first (vs7#61130).
*       05-17-00  GB    Translating LCID 0814 to Norwegian-Nynorsk as special
*                       case
*       09-06-00  PML   Use proper geopolitical terminology (vs7#81673).  Also
*                       move data tables to .rdata.
*       01-30-02  GB    Replaced all the static global variables with per thread
*                       variables inside setloc_struct
*       02-20-02  BWT   prefast fixes - Check lpInStr before indirecting (for iCodePage)
*       12-12-03  AJS   Fix locale matching (uk to English_UnitedKingdom not Ukrainian) (VSWhidbey#188934)
*       04-02-04  GB    Since we don't support UTF8 codepage, we return false
*                       from __get_qualified_locale when ever we see the
*                       codepage is 65001
*       10-10-04  ESC   Ban codepage 65000 (UTF-7) because it is also unsupported
*       05-13-05  MSL   Remove dead Win95 locale helpers
*       06-09-21  WL    Use GetACP to return the default ANSI code page of the system for
*                       locales (e.g. Hindi) that have no associated ANSI code page. (DEVDIV#12792)
*       11-08-06  PMB   Remove support for Win9x and _osplatform and relatives.
*                       DDB#11325
*
*******************************************************************************/

#include <cruntime.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <setlocal.h>
#include <awint.h>
#include <mtdll.h>
#include <internal.h>
#include <dbgint.h>

#if defined(_POSIX_)

BOOL __cdecl __get_qualified_locale(const LPLC_STRINGS lpInStr, UINT* lpCodePage, LPLC_STRINGS lpOutStr)
{
    return FALSE;
}

#else   //if defined(_POSIX_)

//  local defines
#define __LOC_DEFAULT  0x1     //  default language locale for country
#define __LOC_PRIMARY  0x2     //  primary language locale for country
#define __LOC_FULL     0x4     //  fully matched language locale for country
#define __LOC_LANGUAGE 0x100   //  language default seen
#define __LOC_EXISTS   0x200   //  language is installed

//  local structure definitions
typedef struct tagLOCALETAB
{
    wchar_t *  szName;
    wchar_t    chAbbrev[4];
} LOCALETAB;

//  function prototypes
BOOL __cdecl __get_qualified_locale(const LPLC_STRINGS, UINT*, LPLC_STRINGS);
static BOOL TranslateName(const LOCALETAB *, int, const wchar_t **);

static void GetLocaleNameFromLangCountry (_psetloc_struct _psetloc_data);
static BOOL CALLBACK LangCountryEnumProcEx(LPWSTR, DWORD, LPARAM);

static void GetLocaleNameFromLanguage (_psetloc_struct _psetloc_data);
static BOOL CALLBACK LanguageEnumProcEx(LPWSTR, DWORD, LPARAM);

static void GetLocaleNameFromDefault (_psetloc_struct _psetloc_data);

static int ProcessCodePage (LPWSTR lpCodePageStr, _psetloc_struct _psetloc_data);
static BOOL TestDefaultCountry(LPCWSTR localeName);
static BOOL TestDefaultLanguage (LPCWSTR localeName, BOOL bTestPrimary, _psetloc_struct _psetloc_data);

static int GetPrimaryLen(LPCWSTR);

//  non-NLS language string table
__declspec(selectany) const LOCALETAB __rg_language[] =
{ 
    {L"american",                    L"ENU"},
    {L"american english",            L"ENU"},
    {L"american-english",            L"ENU"},
    {L"australian",                  L"ENA"},
    {L"belgian",                     L"NLB"},
    {L"canadian",                    L"ENC"},
    {L"chh",                         L"ZHH"},
    {L"chi",                         L"ZHI"},
    {L"chinese",                     L"CHS"},
    {L"chinese-hongkong",            L"ZHH"},
    {L"chinese-simplified",          L"CHS"},
    {L"chinese-singapore",           L"ZHI"},
    {L"chinese-traditional",         L"CHT"},
    {L"dutch-belgian",               L"NLB"},
    {L"english-american",            L"ENU"},
    {L"english-aus",                 L"ENA"},
    {L"english-belize",              L"ENL"},
    {L"english-can",                 L"ENC"},
    {L"english-caribbean",           L"ENB"},
    {L"english-ire",                 L"ENI"},
    {L"english-jamaica",             L"ENJ"},
    {L"english-nz",                  L"ENZ"},
    {L"english-south africa",        L"ENS"},
    {L"english-trinidad y tobago",   L"ENT"},
    {L"english-uk",                  L"ENG"},
    {L"english-us",                  L"ENU"},
    {L"english-usa",                 L"ENU"},
    {L"french-belgian",              L"FRB"},
    {L"french-canadian",             L"FRC"},
    {L"french-luxembourg",           L"FRL"},
    {L"french-swiss",                L"FRS"},
    {L"german-austrian",             L"DEA"},
    {L"german-lichtenstein",         L"DEC"},
    {L"german-luxembourg",           L"DEL"},
    {L"german-swiss",                L"DES"},
    {L"irish-english",               L"ENI"},
    {L"italian-swiss",               L"ITS"},
    {L"norwegian",                   L"NOR"},
    {L"norwegian-bokmal",            L"NOR"},
    {L"norwegian-nynorsk",           L"NON"},
    {L"portuguese-brazilian",        L"PTB"},
    {L"spanish-argentina",           L"ESS"},
    {L"spanish-bolivia",             L"ESB"},
    {L"spanish-chile",               L"ESL"},
    {L"spanish-colombia",            L"ESO"},
    {L"spanish-costa rica",          L"ESC"},
    {L"spanish-dominican republic",  L"ESD"},   
    {L"spanish-ecuador",             L"ESF"},
    {L"spanish-el salvador",         L"ESE"},
    {L"spanish-guatemala",           L"ESG"},
    {L"spanish-honduras",            L"ESH"},
    {L"spanish-mexican",             L"ESM"},
    {L"spanish-modern",              L"ESN"},
    {L"spanish-nicaragua",           L"ESI"},
    {L"spanish-panama",              L"ESA"},
    {L"spanish-paraguay",            L"ESZ"},
    {L"spanish-peru",                L"ESR"},
    {L"spanish-puerto rico",         L"ESU"},
    {L"spanish-uruguay",             L"ESY"},
    {L"spanish-venezuela",           L"ESV"},
    {L"swedish-finland",             L"SVF"},
    {L"swiss",                       L"DES"},
    {L"uk",                          L"ENG"},
    {L"us",                          L"ENU"},
    {L"usa",                         L"ENU"}
};

//  non-NLS country/region string table
__declspec( selectany ) const LOCALETAB __rg_country[] =
{
    {L"america",                     L"USA"},
    {L"britain",                     L"GBR"},
    {L"china",                       L"CHN"},
    {L"czech",                       L"CZE"},
    {L"england",                     L"GBR"},
    {L"great britain",               L"GBR"},
    {L"holland",                     L"NLD"},
    {L"hong-kong",                   L"HKG"},
    {L"new-zealand",                 L"NZL"},
    {L"nz",                          L"NZL"},
    {L"pr china",                    L"CHN"},
    {L"pr-china",                    L"CHN"},
    {L"puerto-rico",                 L"PRI"},
    {L"slovak",                      L"SVK"},
    {L"south africa",                L"ZAF"},
    {L"south korea",                 L"KOR"},
    {L"south-africa",                L"ZAF"},
    {L"south-korea",                 L"KOR"},
    {L"trinidad & tobago",           L"TTO"},
    {L"uk",                          L"GBR"},
    {L"united-kingdom",              L"GBR"},
    {L"united-states",               L"USA"},
    {L"us",                          L"USA"},
};

/***
*BOOL __get_qualified_locale - return fully qualified locale
*
*Purpose:
*       get default locale, qualify partially complete locales
*
*Entry:
*       lpInStr - input strings to be qualified
*       lpOutStr - pointer to string locale names and codepage output
*
*Exit:
*       TRUE if success, qualified locale is valid
*       FALSE if failure
*
*Exceptions:
*
*******************************************************************************/
BOOL __cdecl __get_qualified_locale(const LPLC_STRINGS lpInStr, UINT* lpOutCodePage, LPLC_STRINGS lpOutStr)
{
    int iCodePage;
    _psetloc_struct _psetloc_data = &_getptd()->_setloc_data;
    _psetloc_data->_cacheLocaleName[0] = '\x0'; // Initialize to invariant localename

    //  initialize pointer to call locale info routine based on operating system

    _psetloc_data->iLocState = 0;
    _psetloc_data->pchLanguage = lpInStr->szLanguage;
    _psetloc_data->pchCountry = lpInStr->szCountry;

    //  if country defined
    //  convert non-NLS country strings to three-letter abbreviations
    if (*_psetloc_data->pchCountry)
        TranslateName(__rg_country, _countof(__rg_country) - 1,
                        &_psetloc_data->pchCountry);


    //  if language defined ...
    if (*_psetloc_data->pchLanguage)
    {
        //  and country defined
        if (*_psetloc_data->pchCountry)
        {
            //  both language and country strings defined
            //  get locale info using language and country
            GetLocaleNameFromLangCountry(_psetloc_data);
        }
        else
        {
            //  language string defined, but country string undefined
            //  get locale info using language only
            GetLocaleNameFromLanguage(_psetloc_data);
        }

        // still not done?
        if (_psetloc_data->iLocState == 0) {
            //  first attempt failed, try substituting the language name
            //  convert non-NLS language strings to three-letter abbrevs
            if (TranslateName(__rg_language, _countof(__rg_language) - 1,
                                &_psetloc_data->pchLanguage))
            {
                if (*_psetloc_data->pchCountry)
                {
                    //  get locale info using language and country
                    GetLocaleNameFromLangCountry(_psetloc_data);
                }
                else
                {
                    //  get locale info using language only
                    GetLocaleNameFromLanguage(_psetloc_data);
                }
            }
        }
    }
    else
    {
        //  language is an empty string, use the User Default locale name
        GetLocaleNameFromDefault(_psetloc_data);
    }

    //  test for error in locale processing
    if (_psetloc_data->iLocState == 0)
        return FALSE;

    //  process codepage value
    iCodePage = ProcessCodePage(lpInStr ? lpInStr->szCodePage: NULL, _psetloc_data);

    //  verify codepage validity
    if (!iCodePage || iCodePage == CP_UTF7 || iCodePage == CP_UTF8 ||
        !IsValidCodePage((WORD)iCodePage))
        return FALSE;

    //  set codepage
    if (lpOutCodePage)
    {
        *lpOutCodePage = (UINT)iCodePage;
    }

    //  set locale name and codepage results
    if (lpOutStr)
    {
        lpOutStr->szLocaleName[0] = L'\x0'; // Init the locale name to emptry string

        _ERRCHECK(wcsncpy_s(lpOutStr->szLocaleName, _countof(lpOutStr->szLocaleName), _psetloc_data->_cacheLocaleName, wcslen(_psetloc_data->_cacheLocaleName) + 1));

        // Get and store the Enlgish language lang name, to be returned to user
        if (__crtGetLocaleInfoEx(lpOutStr->szLocaleName, LOCALE_SENGLISHLANGUAGENAME,
                                    lpOutStr->szLanguage, MAX_LANG_LEN) == 0)
            return FALSE;

        // Get and store the Enlgish language country name, to be returned to user
        if (__crtGetLocaleInfoEx(lpOutStr->szLocaleName, LOCALE_SENGLISHCOUNTRYNAME,
                                    lpOutStr->szCountry, MAX_CTRY_LEN) == 0)
            return FALSE;

        // Special case: Both '.' and '_' are separators in string passed to setlocale, 
        // so if found in Country we use abbreviated name instead.                
        if (wcschr(lpOutStr->szCountry, L'_') || wcschr(lpOutStr->szCountry, L'.'))
            if (__crtGetLocaleInfoEx(lpOutStr->szLocaleName, LOCALE_SABBREVCTRYNAME,
                                    lpOutStr->szCountry, MAX_CTRY_LEN) == 0)
                return FALSE;

        _itow_s((int)iCodePage, (wchar_t *)lpOutStr->szCodePage, MAX_CP_LEN, 10);        
    }

    return TRUE;
}

/***
*BOOL TranslateName - convert known non-NLS string to NLS equivalent
*
*Purpose:
*   Provide compatibility with existing code for non-NLS strings
*
*Entry:
*   lpTable  - pointer to LOCALETAB used for translation
*   high     - maximum index of table (size - 1)
*   ppchName - pointer to pointer of string to translate
*
*Exit:
*   ppchName - pointer to pointer of string possibly translated
*   TRUE if string translated, FALSE if unchanged
*
*Exceptions:
*
*******************************************************************************/
static BOOL TranslateName (
    const LOCALETAB * lpTable,
    int               high,
    const wchar_t**   ppchName)
{
    int     i;
    int     cmp = 1;
    int     low = 0;

    //  typical binary search - do until no more to search or match
    while (low <= high && cmp != 0)
    {
        i = (low + high) / 2;
        cmp = _wcsicmp(*ppchName, (const wchar_t *)(*(lpTable + i)).szName);

        if (cmp == 0)
            *ppchName = (*(lpTable + i)).chAbbrev;
        else if (cmp < 0)
            high = i - 1;
        else
            low = i + 1;
    }

    return !cmp;
}

/***
*void GetLocaleNameFromLangCountry - get locale names from language and country strings
*
*Purpose:
*   Match the best locale names to the language and country string given.
*   After global variables are initialized, the LangCountryEnumProcEx
*   routine is registered as an EnumSystemLocalesEx callback to actually
*   perform the matching as the locale names are enumerated.
*
*Entry:
*   pchLanguage     - language string
*   bAbbrevLanguage - language string is a three-letter abbreviation
*   pchCountry      - country string
*   bAbbrevCountry  - country string ia a three-letter abbreviation
*   iPrimaryLen     - length of language string with primary name
*
*Exit:
*   localeName - locale name of given language and country
*
*Exceptions:
*
*******************************************************************************/
static void GetLocaleNameFromLangCountry (_psetloc_struct _psetloc_data)
{
    //  initialize static variables for callback use
    _psetloc_data->bAbbrevLanguage = wcslen(_psetloc_data->pchLanguage) == 3;
    _psetloc_data->bAbbrevCountry = wcslen(_psetloc_data->pchCountry) == 3;

    _psetloc_data->iPrimaryLen = _psetloc_data->bAbbrevLanguage ?
                             2 : GetPrimaryLen(_psetloc_data->pchLanguage);
    
    // Enumerate all locales that come with the operating system, 
    // including replacement locales, but excluding alternate sorts.
    __crtEnumSystemLocalesEx(LangCountryEnumProcEx, LOCALE_WINDOWS | LOCALE_SUPPLEMENTAL, (LPARAM) NULL);

    //  locale value is invalid if the language was not installed or the language
    //  was not available for the country specified
    if (!(_psetloc_data->iLocState & __LOC_LANGUAGE) ||
        !(_psetloc_data->iLocState & __LOC_EXISTS) ||
        !(_psetloc_data->iLocState & (__LOC_FULL |
                                    __LOC_PRIMARY |
                                    __LOC_DEFAULT)))
        _psetloc_data->iLocState = 0;
}

/***
*BOOL CALLBACK LangCountryEnumProcEx - callback routine for GetLocaleNameFromLangCountry
*
*Purpose:
*   Determine if locale name given matches the language in pchLanguage
*   and country in pchCountry.
*
*Entry:
*   lpLocaleString   - pointer to locale name string string
*   pchCountry     - pointer to country name
*   bAbbrevCountry - set if country is three-letter abbreviation
*
*Exit:
*   iLocState   - status of match
*       __LOC_FULL - both language and country match (best match)
*       __LOC_PRIMARY - primary language and country match (better)
*       __LOC_DEFAULT - default language and country match (good)
*       __LOC_LANGUAGE - default primary language exists
*       __LOC_EXISTS - full match of language string exists
*       (Overall match occurs for the best of FULL/PRIMARY/DEFAULT
*        and LANGUAGE/EXISTS both set.)
*   localeName - lpLocaleString matched
*   FALSE if match occurred to terminate enumeration, else TRUE.
*
*Exceptions:
*
*******************************************************************************/
static BOOL CALLBACK LangCountryEnumProcEx (LPWSTR lpLocaleString, DWORD dwFlags, LPARAM lParam)
{
    _psetloc_struct _psetloc_data = &_getptd()->_setloc_data;

    wchar_t  rgcInfo[MAX_LANG_LEN]; // MAX_LANG_LEN == MAX_CTRY_LEN == 64

    //  test locale country against input value
    if (__crtGetLocaleInfoEx(lpLocaleString,
                        _psetloc_data->bAbbrevCountry ? LOCALE_SABBREVCTRYNAME : LOCALE_SENGLISHCOUNTRYNAME,
                        rgcInfo, _countof(rgcInfo)) == 0)
    {
        //  set error condition and exit
        _psetloc_data->iLocState = 0;
        return TRUE;
    }

    //  if country names matched
    if (_wcsicmp(_psetloc_data->pchCountry, rgcInfo) == 0)
    {
        //  test for language match
        if (GetLocaleInfoEx(lpLocaleString,
                                 _psetloc_data->bAbbrevLanguage ?
                                 LOCALE_SABBREVLANGNAME : LOCALE_SENGLISHLANGUAGENAME,
                                 rgcInfo, _countof(rgcInfo)) == 0)
        {
            //  set error condition and exit
            _psetloc_data->iLocState = 0;
            return TRUE;
        }

        if (_wcsicmp(_psetloc_data->pchLanguage, rgcInfo) == 0)
        {
            //  language matched also - set state and value
            //  this is the best match
            _psetloc_data->iLocState |= (__LOC_FULL |
                                       __LOC_LANGUAGE |
                                       __LOC_EXISTS);

            _ERRCHECK(wcsncpy_s(_psetloc_data->_cacheLocaleName, _countof(_psetloc_data->_cacheLocaleName), lpLocaleString, wcslen(lpLocaleString) + 1));
        }
        //  test if match already for primary langauage
        else if (!(_psetloc_data->iLocState & __LOC_PRIMARY))
        {
            //  if not, use _psetloc_data->iPrimaryLen to partial match language string
            if (_psetloc_data->iPrimaryLen && !_wcsnicmp(_psetloc_data->pchLanguage, rgcInfo, _psetloc_data->iPrimaryLen))
            {
                //  primary language matched - set locale name
                _psetloc_data->iLocState |= __LOC_PRIMARY;
                _ERRCHECK(wcsncpy_s(_psetloc_data->_cacheLocaleName, _countof(_psetloc_data->_cacheLocaleName), lpLocaleString, wcslen(lpLocaleString) + 1));
            }

            //  test if default language already defined
            else if (!(_psetloc_data->iLocState & __LOC_DEFAULT))
            {
                //  if not, test if locale language is default for country
                if (TestDefaultCountry(lpLocaleString))
                {
                    //  default language for country - set state, value
                    _psetloc_data->iLocState |= __LOC_DEFAULT;
                    _ERRCHECK(wcsncpy_s(_psetloc_data->_cacheLocaleName, _countof(_psetloc_data->_cacheLocaleName), lpLocaleString, wcslen(lpLocaleString) + 1));
                }
            }
        }
    }

    //  test if input language both exists and default primary language defined
    if ((_psetloc_data->iLocState & (__LOC_LANGUAGE | __LOC_EXISTS)) !=
                      (__LOC_LANGUAGE | __LOC_EXISTS))
    {
        //  test language match to determine whether it is installed
        if (__crtGetLocaleInfoEx(lpLocaleString, _psetloc_data->bAbbrevLanguage ? LOCALE_SABBREVLANGNAME
                                                                           : LOCALE_SENGLISHLANGUAGENAME,
                           rgcInfo, sizeof(rgcInfo)) == 0)
        {
            //  set error condition and exit
            _psetloc_data->iLocState = 0;
            return TRUE;
        }

        // the input language matches
        if (_wcsicmp(_psetloc_data->pchLanguage, rgcInfo) == 0)
        {
            //  language matched - set bit for existance
            _psetloc_data->iLocState |= __LOC_EXISTS;

            if (_psetloc_data->bAbbrevLanguage)
            {
                //  abbreviation - set state
                //  also set language locale name if not set already
                _psetloc_data->iLocState |= __LOC_LANGUAGE;
                if (!_psetloc_data->_cacheLocaleName[0])
                    _ERRCHECK(wcsncpy_s(_psetloc_data->_cacheLocaleName, _countof(_psetloc_data->_cacheLocaleName), lpLocaleString, wcslen(lpLocaleString) + 1));
            }

            //  test if language is primary only (no sublanguage)
            else if (_psetloc_data->iPrimaryLen && ((int)wcslen(_psetloc_data->pchLanguage) == _psetloc_data->iPrimaryLen))
            {
                //  primary language only - test if default locale name
                if (TestDefaultLanguage(lpLocaleString, TRUE, _psetloc_data))
                {
                    //  default primary language - set state
                    //  also set locale name if not set already
                    _psetloc_data->iLocState |= __LOC_LANGUAGE;
                    if (!_psetloc_data->_cacheLocaleName[0])
                        _ERRCHECK(wcsncpy_s(_psetloc_data->_cacheLocaleName, _countof(_psetloc_data->_cacheLocaleName), lpLocaleString, wcslen(lpLocaleString) + 1));
                }
            }
            else
            {
                //  language with sublanguage - set state
                //  also set locale name if not set already
                _psetloc_data->iLocState |= __LOC_LANGUAGE;
                if (!_psetloc_data->_cacheLocaleName[0])
                    _ERRCHECK(wcsncpy_s(_psetloc_data->_cacheLocaleName, _countof(_psetloc_data->_cacheLocaleName), lpLocaleString, wcslen(lpLocaleString) + 1));
            }
        }
    }

    //  if LOCALE_FULL set, return FALSE to stop enumeration,
    //  else return TRUE to continue
    return (_psetloc_data->iLocState & __LOC_FULL) == 0;
}

/***
*void GetLocaleNameFromLanguage - get locale name from language string
*
*Purpose:
*   Match the best locale name to the language string given.  After global
*   variables are initialized, the LanguageEnumProcEx routine is
*   registered as an EnumSystemLocalesEx callback to actually perform
*   the matching as the locale names are enumerated.
*
*Entry:
*   pchLanguage     - language string
*   bAbbrevLanguage - language string is a three-letter abbreviation
*   iPrimaryLen     - length of language string with primary name
*
*Exit:
*   localeName - locale name of language with default country
*
*Exceptions:
*
*******************************************************************************/
static void GetLocaleNameFromLanguage (_psetloc_struct _psetloc_data)
{
    //  initialize static variables for callback use
    _psetloc_data->bAbbrevLanguage = wcslen(_psetloc_data->pchLanguage) == 3;
    _psetloc_data->iPrimaryLen = _psetloc_data->bAbbrevLanguage ? 2 : GetPrimaryLen(_psetloc_data->pchLanguage);

    // Enumerate all locales that come with the operating system, including replacement locales, 
    // but excluding alternate sorts.
    __crtEnumSystemLocalesEx(LanguageEnumProcEx, LOCALE_WINDOWS | LOCALE_SUPPLEMENTAL, (LPARAM) NULL);

    //  locale value is invalid if the language was not installed
    //  or the language was not available for the country specified
    if (!(_psetloc_data->iLocState & __LOC_FULL))
        _psetloc_data->iLocState = 0;
}

/***
*BOOL CALLBACK LanguageEnumProcEx - callback routine for GetLocaleNameFromLanguage
*
*Purpose:
*   Determine if locale name given matches the default country for the
*   language in pchLanguage.
*
*Entry:
*   lpLocaleString    - pointer to string with locale name
*   dwFlags     - not used
*   lParam      - not used
*
*Exit:
*   localeName - locale name matched
*   FALSE if match occurred to terminate enumeration, else TRUE.
*
*Exceptions:
*
*******************************************************************************/
static BOOL CALLBACK LanguageEnumProcEx (LPWSTR lpLocaleString, DWORD dwFlags, LPARAM lParam)
{
    _psetloc_struct    _psetloc_data = &_getptd()->_setloc_data;
    wchar_t    rgcInfo[120];

    //  test locale for language specified
    if (__crtGetLocaleInfoEx(lpLocaleString, _psetloc_data->bAbbrevLanguage ? LOCALE_SABBREVLANGNAME
                                                                       : LOCALE_SENGLISHLANGUAGENAME,
                       rgcInfo, _countof(rgcInfo)) == 0)
    {
        //  set error condition and exit
        _psetloc_data->iLocState = 0;
        return TRUE;
    }

    if (_wcsicmp(_psetloc_data->pchLanguage, rgcInfo) == 0)
    {
        //  language matches
        _ERRCHECK(wcsncpy_s(_psetloc_data->_cacheLocaleName, _countof(_psetloc_data->_cacheLocaleName), lpLocaleString, wcslen(lpLocaleString) + 1));

        _psetloc_data->iLocState |= __LOC_FULL;
    }

    return (_psetloc_data->iLocState & __LOC_FULL) == 0;
}


/***
*void GetLocaleNameFromDefault - get default locale names
*
*Purpose:
*   Set both language and country locale names to the user default.
*
*Entry:
*   None.
*
*Exceptions:
*
*******************************************************************************/
static void GetLocaleNameFromDefault (_psetloc_struct _psetloc_data)
{
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
    _psetloc_data->iLocState |= (__LOC_FULL | __LOC_LANGUAGE);
     
    // Store the default user locale name. The returned buffer size includes the
    // terminating null character, so only store if the size retrned is > 1
    if (__crtGetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) > 1)
    {
        _ERRCHECK(wcsncpy_s(_psetloc_data->_cacheLocaleName, _countof(_psetloc_data->_cacheLocaleName), localeName, wcslen(localeName) + 1));
    }
}

/***
*int ProcessCodePage - convert codepage string to numeric value
*
*Purpose:
*   Process codepage string consisting of a decimal string, or the
*   special case strings "ACP" and "OCP", for ANSI and OEM codepages,
*   respectively.  Null pointer or string returns the ANSI codepage.
*
*Entry:
*   lpCodePageStr - pointer to codepage string
*
*Exit:
*   Returns numeric value of codepage.
*
*Exceptions:
*
*******************************************************************************/
static int ProcessCodePage (LPWSTR lpCodePageStr, _psetloc_struct _psetloc_data)
{
    int iCodePage;

    if (!lpCodePageStr || !*lpCodePageStr || wcscmp(lpCodePageStr, L"ACP") == 0)
    {
        //  get ANSI codepage for the country locale name
        if (__crtGetLocaleInfoEx(_psetloc_data->_cacheLocaleName, LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER,
                                 (LPWSTR) &iCodePage, sizeof(iCodePage) / sizeof(wchar_t)) == 0)
            return 0;
        
        if (iCodePage == 0) // for locales have no assoicated ANSI codepage, e.g. Hindi locale
            return GetACP();
    }
    else if (wcscmp(lpCodePageStr, L"OCP") == 0)
    {
        //  get OEM codepage for the country locale name
        if (__crtGetLocaleInfoEx(_psetloc_data->_cacheLocaleName, LOCALE_IDEFAULTCODEPAGE | LOCALE_RETURN_NUMBER,
                                 (LPWSTR) &iCodePage, sizeof(iCodePage) / sizeof(wchar_t)) == 0)
            return 0;
    }
    else
    {
         // convert decimal string to numeric value
         iCodePage = (int)_wtol(lpCodePageStr);
    }
    
    return iCodePage;
}

/***
*BOOL TestDefaultCountry - determine if default locale for country
*
*Purpose:
*   Determine if the locale of the given locale name has the default sublanguage.
*   This is determined by checking if the given language is neutral.
*
*Entry:
*   localeName - name of locale to test
*
*Exit:
*   Returns TRUE if default sublanguage, else FALSE.
*
*Exceptions:
*
*******************************************************************************/
static BOOL TestDefaultCountry (LPCWSTR localeName)
{
    wchar_t sIso639LangName[9]; // The maximum length for LOCALE_SISO3166CTRYNAME 
                                // is 9 including NULL

    //  Get 2-letter ISO Standard 639 or 3-letter ISO 639-2 value
    if (__crtGetLocaleInfoEx(localeName, LOCALE_SISO639LANGNAME,
                                sIso639LangName, _countof(sIso639LangName)) == 0)
        return FALSE;

    // Determine if this is a neutral language
    if (wcsncmp(sIso639LangName, localeName, _countof(sIso639LangName)) == 0)
        return TRUE;

    return FALSE;
}

/***
*BOOL TestDefaultLanguage - determine if default locale for language
*
*Purpose:
*   Determines if the given locale name has the default sublanguage.
*   If bTestPrimary is set, also allow TRUE when string contains an
*   implicit sublanguage.
*
*Entry:
*   localeName         - locale name of locale to test
*   bTestPrimary - set if testing if language is primary
*
*Exit:
*   Returns TRUE if sublanguage is default for locale tested.
*   If bTestPrimary set, TRUE is language has implied sublanguge.
*
*Exceptions:
*
*******************************************************************************/
static BOOL TestDefaultLanguage (LPCWSTR localeName, BOOL bTestPrimary, _psetloc_struct _psetloc_data)
{
    if (!TestDefaultCountry (localeName))
    {
        //  test if string contains an implicit sublanguage by
        //  having a character other than upper/lowercase letters.
        if (bTestPrimary && GetPrimaryLen(_psetloc_data->pchLanguage) == (int)wcslen(_psetloc_data->pchLanguage))
            return FALSE;
    }

    return TRUE;
}

/***
*int GetPrimaryLen - get length of primary language name
*
*Purpose:
*   Determine primary language string length by scanning until
*   first non-alphabetic character.
*
*Entry:
*   pchLanguage - string to scan
*
*Exit:
*   Returns length of primary language string.
*
*Exceptions:
*
*******************************************************************************/
static int GetPrimaryLen (LPCWSTR pchLanguage)
{
    int     len = 0;
    wchar_t    ch;

    if (!pchLanguage)
        return 0;

    ch = *pchLanguage++;
    while ((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z'))
    {
        len++;
        ch = *pchLanguage++;
    }

    return len;
}

#endif  //if defined(_POSIX_)
