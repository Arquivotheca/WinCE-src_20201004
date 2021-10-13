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
#include <tchar.h>
#include <stdlib.h>
#include "utility.h"

    
const    wchar_t    * sDefaultValue = L"";
const    wchar_t    * sDefaultTargetKey = L"";
const    wchar_t    * sNewKey = L"NewKey";
const    wchar_t    * sDefault = L"AddedTestKey";
const    wchar_t * sDefaultWatchedKeyName = L".000";


const    wchar_t        * sHKCR = L"HKCR"; 
const    wchar_t        * sHKLM = L"HKLM"; 
const    wchar_t        * sHKCU = L"HKCU"; 
const    wchar_t        * sHKUS = L"HKUS"; 

const    wchar_t * sinterval = L"-interval";
const    wchar_t * sINTERVAL = L"-INTERVAL";
const    wchar_t * siter = L"-iter";
const    wchar_t * sITER = L"-ITER";
const    wchar_t * sevtsexp = L"-evtsexp";
const    wchar_t * sEVTSEXP = L"-EVTSEXP";
const    wchar_t * sthreads = L"-threads";
const    wchar_t * sTHREADS = L"-THREADS";
const    wchar_t * svalname = L"-valname";
const    wchar_t * sVALNAME = L"-VALNAME";
const    wchar_t * skeyname = L"-keyname";
const    wchar_t * sKEYNAME = L"-KEYNAME";

const    wchar_t * saddkey = L"-addkey";
const    wchar_t * sADDKEY = L"-ADDKEY";
const    wchar_t * sdelkey = L"-delkey";
const    wchar_t * sDELKEY = L"-DELKEY";
const    wchar_t * saddval = L"-addval";
const    wchar_t * sSETVAL = L"-SETVAL";
const    wchar_t * ssetval = L"-setval";
const    wchar_t * sADDVAL = L"-ADDVAL";
const    wchar_t * sdelval = L"-delval";
const    wchar_t * sDELVAL = L"-DELVAL";
const    wchar_t * sfilter = L"-filter";
const    wchar_t * sFILTER = L"-FILTER";
const    wchar_t * svolatile = L"-volatile";
const    wchar_t * sVOLATILE = L"-VOLATILE";
const    wchar_t * snewval = L"-newval";
const    wchar_t * sNEWVAL = L"-NEWVAL";
const    wchar_t * spassalways = L"-passalways";
const    wchar_t * sPASSALWAYS = L"-PASSALWAYS";
const    wchar_t * snull            = L"null";
const    wchar_t * sNULL            = L"NULL";
const    wchar_t * sqmark        = L"-?";
const    wchar_t * sfile            = L"-file"; 
const    wchar_t * sinfile        = L"-infile"; 
const    wchar_t * soutfile        = L"-outfile"; 
const    wchar_t * sfailifexist    = L"-failifexist"; 
const    wchar_t * sprogress        = L"-progress"; 
const    wchar_t * scancel        = L"-cancel"; 
const    wchar_t * screate        = L"-create"; 
const    wchar_t * sdelete        = L"-delete"; 
const    wchar_t * sbytes        = L"-bytes"; 
const    wchar_t * sinbytes        = L"-inbytes"; 
const    wchar_t * soutbytes        = L"-outbytes"; 
const    wchar_t * stoolong        = L"toolong"; //note this is not a flag
const    wchar_t * sretval        = L"-retval"; 
const    wchar_t * ssystem        = L"-system"; 
const    wchar_t * shidden        = L"-hidden"; 
const    wchar_t * sreadonly        = L"-readonly"; 
const    wchar_t * snormal        = L"-normal"; 
const    wchar_t * sarchive        = L"-archive"; 
const    wchar_t * serrexp        = L"-errexp";
const    wchar_t * spassexpected        = L"-passexpected";
const    wchar_t * sflags        = L"-flags";
const    wchar_t * sexpected        = L"-expected";

const    wchar_t        *    slockexclusive                = L"-lockexclusive"; 
const    wchar_t        *    sfailimmediately            = L"-failimmediately";
const    wchar_t        *    slockfailimmediately        = L"-lockfailimmediately";
const    wchar_t        *    slockoffset                    = L"-lockoffset";
const    wchar_t        *    slocknumbytes                = L"-locknumbytes";
const    wchar_t        *    slockreturn                    = L"-lockreturn";
const    wchar_t        *    slocklasterror                = L"-locklasterror";
const    wchar_t        *    sunlockoffset                = L"-unlockoffset";
const    wchar_t        *    sunlocknumbytes                = L"-unlocknumbytes";
const    wchar_t        *    sunlockreturn                = L"-unlockreturn";
const    wchar_t        *    sunlocklasterror            = L"-unlocklasterror";
const    wchar_t        *    sopstartbyte                = L"-opstartbyte";
const    wchar_t        *    sopnumbytes                    = L"-opnumbytes";
const    wchar_t        *    swaitreturn                    = L"-waitreturn";
const    wchar_t        *    stouchlocked                = L"-touchlocked";
const    wchar_t        *    stouchunlocked                = L"-touchunlocked";
const    wchar_t        *    snumthreads                    = L"-numthreads";
const    wchar_t        *    sread                        = L"-read";
const    wchar_t        *    swrite                        = L"-write";
const    wchar_t        *    slock                        = L"-lock";
const    wchar_t        *    sunlock                        = L"-unlock";
const    wchar_t        *    sthreadlock                    = L"-threadlock";
const    wchar_t        *    sthreadunlock                = L"-threadunlock";
const    wchar_t        *    sreadaccess                    = L"-readaccess";
const    wchar_t        *    swriteaccess                = L"-writeaccess";
const    wchar_t        *    sshareread                    = L"-shareread";
const    wchar_t        *    ssharewrite                    = L"-sharewrite";
const    wchar_t        *    sexpectreadfail                = L"-expectreadfail";
const    wchar_t        *    sexpectwritefail            = L"-expectwritefail";
const    wchar_t        *    sexpectlockfail                = L"-expectlockfail";
const    wchar_t        *    sexpectunlockfail            = L"-expectunlockfail";

const    wchar_t        *    sopoffset                    = L"-opoffset";
const    wchar_t        *    sexpectfail                    = L"-expectfail";
const    wchar_t        *    sexpectopfail                    = L"-expectopfail";
const    wchar_t        *    slockexclusive0                = L"-lockexclusive0";
const    wchar_t        *    slockexclusive1                = L"-lockexclusive1";

const    wchar_t        *    ssystemguid                    = L"-systemguid";
const    wchar_t        *    sdisk                        = L"-disk";

const    wchar_t        *    sexpectunlocknonlockedfail            = L"-expectunlocknonlockedfail";



const    wchar_t * swhackwhackqmarkwhack        = L"\\\\?\\";



const    wchar_t * srk = L"-rk";
const    wchar_t * sRK = L"-RK";
const    wchar_t * sTK = L"-TK";
const    wchar_t * stk = L"-tk";
const    wchar_t * swk = L"-wk";
const    wchar_t * sWK = L"-WK";
const    wchar_t * swr = L"-wr";
const    wchar_t * sWR = L"-WR";
const    wchar_t * str = L"-tr";
const    wchar_t * sTR = L"-TR";
const    wchar_t * sb = L"-b";
const    wchar_t * sB = L"-B";
const    wchar_t * sT = L"-T";
const    wchar_t * sFoo = L"FOO";

const    wchar_t * szorch = L"-zorch";
const    wchar_t * sslashzorch = L"/zorch";

const    wchar_t * sdrive = L"-drive";


/*
* Use this for pulling cmd line options, e.g. "-volatile" --> volatile keys
* TODO:
*    - only works for [non]existance of VOLATILE (-volatile) option
*    - returns ZERO or REG_OPTION_VOLATILE only to preserve backward compatibility of tests
* (probably not an issue anyways)
* ericfowl
*/
DWORD GetOptionsFromString(const _TCHAR * s)
{
    return FlagIsPresentOnCmdLine(s, svolatile) ? REG_OPTION_VOLATILE : 0; 
}


bool GetULIntFromCmdLine(const WCHAR *s, const WCHAR * sFlag, unsigned * p, unsigned radix)
{
    WCHAR sBuf[32];
    
    if(!GetStringFromCmdLine(s, sBuf, CHARCOUNT(sBuf), sFlag))
        return false; 

    *p = wcstoul(sBuf, NULL, radix); 
    return true; 
}

bool GetULONGLONGFromCmdLine(const WCHAR *s, const WCHAR * sFlag, ULONGLONG * p)
{
    WCHAR sBuf[32];
    
    if(!GetStringFromCmdLine(s, sBuf, CHARCOUNT(sBuf), sFlag))
        return false; 

    *p = _wtoi64(sBuf); 
    return true; 
}


bool GetStringFromCmdLine(const WCHAR *sCmdLine, WCHAR * sTargetKey, unsigned cch, const WCHAR * sFlag)
{
    const unsigned        CMD_LINE_MAX_CHARS        = 0x10000;
    const wchar_t    * s = wcsstr(sCmdLine, sFlag);
    WCHAR * sTarg;

    if(!s)
        return false;
    
    s += wcslen(sFlag); 
    
    
    while(*s && iswspace(*s)) ++s; 

    if(*s == L'-') return false;    //error condition, got another flag before name

    //make prefast happy
    if(cch && cch < CMD_LINE_MAX_CHARS)
        memset(sTargetKey, 0, cch * sizeof(*sTargetKey));

    sTarg = sTargetKey;
    
    //We need to check for string null termination as well
    while(!iswspace(*s) && cch > 0 && *s!='\0')
    {
        *sTarg = *s; 
        s++; sTarg++; cch--;
    }
    *sTarg='\0';

    return true; 
}

bool FlagIsPresentOnCmdLine(const wchar_t * s, const wchar_t * sQry)
{
    //for every flagged item
    for(const wchar_t * p = s; *p; ++p)
    {
        if(*p == L'-' || *p == L'/')
        {
            //if perfect match return true
            const wchar_t * pTail;
            for(pTail = p; *pTail && !iswspace(*pTail); ++pTail)
                ; //increment pTail to end of token
            if(!wcsnicmp(p, sQry, (unsigned)(pTail - p)))
                return true; 
        } 
        else
            continue;
    }
    //if it is perfect match, return true

    return false;
}


HKEY    GetRootKeyFromCmdLine(const    wchar_t *s, const    wchar_t *sQry)
{
    const wchar_t * sFlag = 0; 
    if((sFlag = wcsstr(s, sQry)) == 0)
        return NULL; 

    sFlag  += wcslen(sQry); 
    while(*sFlag && iswspace(*sFlag)) ++sFlag; 

    if(!wcsncmp(sFlag, sHKCR, wcslen(sHKCR)))
        return HKEY_CLASSES_ROOT; 

    if(!wcsncmp(sFlag, sHKLM, wcslen(sHKLM)))
        return HKEY_LOCAL_MACHINE; 

    if(!wcsncmp(sFlag, sHKCU, wcslen(sHKCU)))
        return HKEY_CURRENT_USER;

    if(!wcsncmp(sFlag, sHKUS, wcslen(sHKUS)))
        return HKEY_USERS;


    return NULL; 
}


wchar_t * AllocAndFillLongName(void)
{
    const unsigned    TOO_LONG = CE_MAX_PATH + 100; 
    wchar_t * s = (wchar_t *)calloc(TOO_LONG, sizeof(wchar_t)); 
    if(!s) return s; 

    wcscpy_s(s, TOO_LONG, swhackwhackqmarkwhack);

    //buffer is zeroed out by calloc() so no need for null terminator
    for(unsigned i = wcslen(swhackwhackqmarkwhack); i < TOO_LONG - 1; ++i)
        s[i] = (wchar_t)(L'a' + i%26); 

    return s;
}

bool Zorch(const wchar_t *s)
{
    if(!s) 
        return false; 

    if(!*s)
        return false; 

    if(FlagIsPresentOnCmdLine(s, szorch))
        return true; 

    if(FlagIsPresentOnCmdLine(s, sslashzorch))
        return true; 

    return false; 
}

const wchar_t * sZorchMsg[] = 
    {L"WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING",
    L"1.  BACK UP DATA ON ALL CARDS AND DISKS that are attached to the tested device before running this test.",
    L"2.  In the CETK tree control, right click on this test and choose EDIT COMMAND LINE",
    L"3.  Add the following switch to the command line:",
    L"\t\t-c \"-zorch\" \n",
    L"4.  Save the change to the command line.  NOTE:  It is strongly recommended that this command line change be made temporary to prevent accidental loss of data in the future.",
    L"5.  Run the test."};

void Zorchage(void)
{
    for(unsigned i = 0; i < sizeof(sZorchMsg)/sizeof(sZorchMsg[0]); ++i)
        OutputDebugString(sZorchMsg[i]);
}

void Zorchage(CKato * pKato)
{
    for(unsigned i = 0; i < sizeof(sZorchMsg)/sizeof(sZorchMsg[0]); ++i)
        pKato->Log(LOG_COMMENT, sZorchMsg[i]);
}

