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
// ******************************************************************
// **             
// ******************************************************************
//
//    dial.c        The dialing state machine for Unimodem
//
//    @doc    EX_TSPI
//
//    @topic    TSPI |
//
//        Tspi Stuff
//
//

#include "windows.h"
#include "types.h"
#include "memory.h"
#include "mcx.h"
#include "tspi.h"
#include "linklist.h"
#include "tspip.h"
#include "tapicomn.h"
#include "dial.h"
#include "termctrl.h"
#include "resource.h"
#include "windev.h"

#include <ntintsafe.h>

// Functions from modem.c
extern LONG SignalCommMask(PTLINEDEV pLineDev);
extern void SetDCBfromDevMiniCfg(DCB * pDCB,PDEVMINICFG lpDevMiniCfg);
extern void WriteModemLog(PTLINEDEV pLineDev, UCHAR * szCommand, DWORD dwOp);
extern void SetDialerTimeouts(PTLINEDEV pLineDev);
extern void GetModemCommand(PTLINEDEV pLineDev, LPWSTR CommandName, LPSTR CommandString, LPSTR DefaultCommandString);

// Unimodem thread priority
extern DWORD g_dwUnimodemThreadPriority;

// Some keys we read to get modem related info from registry
const WCHAR szSettings[] =   TEXT("Settings");
const WCHAR szDialSuffix[] = TEXT("DialSuffix");
static const WCHAR szInit[] =   TEXT("Init");
static const WCHAR szSkipInit[] =   TEXT("SkipInit");
#define MAXINITKEYS 5
static const PWCHAR szInitNum[MAXINITKEYS] = {
    TEXT("1"),
    TEXT("2"),
    TEXT("3"),
    TEXT("4"),
    TEXT("5") 
};

//
// Reset system idle timer so device doesn't go to sleep in the middle of dialing.
//
void MdmResetIdleTimer(void)
{
    HMODULE hCoreDLL;
    FARPROC pfnSystemIdleTimerReset;

    if (WAIT_OBJECT_0 == WaitForAPIReady(SH_WMGR, 0)) {
        hCoreDLL = (HMODULE)LoadLibrary(TEXT("COREDLL.DLL"));
        if (hCoreDLL != NULL) {
            if (pfnSystemIdleTimerReset = GetProcAddress(hCoreDLL, TEXT("SystemIdleTimerReset"))) {
                pfnSystemIdleTimerReset();
            }
            FreeLibrary(hCoreDLL);
        }
    }
}

int strncmpi(char *dst, const char *src, long count)
{
    while (count) {
        if (toupper(*dst) != toupper(*src))
            return 1;
        if (*src == 0)
            return 0;
        dst++;
        src++;
        count--;
    }

    return 0;
}


/******************************************************************************

 @doc INTERNAL

 @api BOOL | ExpandMacros | Takes the string pszLine, and copies it to 
 lpszVal after expanding macros
 
 @parm char * | pszRegResponse | ptr to response string from registry.
 
 @parm char * | pszExpanded | ptr to buffer to copy string to w/ macros expanded
 
 @parm DWORD * | pdwValLen | length of pszVal w/ expanded macros.
 
 @rdesc Returns FALSE if a needed macro translation could not be found in the
 pMacroXlations table, TRUE otherwise.
 
*****************************************************************************/
BOOL
ExpandMacros(
    PWCHAR  pszRegResponse,
    PWCHAR  pszExpanded,
    DWORD   cchExpanded,
    MODEMMACRO * pMdmMacro
    )
{
    DWORD cchName;
    DWORD cchVal;
    DWORD i;
        
    DEBUGMSG(ZONE_DIAL, (TEXT("UNIMODEM:+ExpandMacros : Original \"%s\"\r\n"), pszRegResponse ));

    for (i = 0; ((i < cchExpanded) && (*pszRegResponse)); ) 
    {
        // check for a macro
        if ( *pszRegResponse == LMSCH ) 
        {
            // <cr>
            if (!wcsnicmp(pszRegResponse,CR_MACRO,CR_MACRO_LENGTH)) 
            {
                pszExpanded[i] = CR;
                i++;
                pszRegResponse += CR_MACRO_LENGTH;
                continue;
            }
            // <lf>
            if (!wcsnicmp(pszRegResponse,LF_MACRO,LF_MACRO_LENGTH)) 
            {
                pszExpanded[i] = LF;
                i++;
                pszRegResponse += LF_MACRO_LENGTH;
                continue;
            }
            // <hxx>
            if ((pszRegResponse[1] == 'h' || pszRegResponse[1] == 'H') &&
                     isxdigit(pszRegResponse[2]) &&
                     isxdigit(pszRegResponse[3]) &&
                     pszRegResponse[4] == RMSCH ) 
            {
                pszExpanded[i] = (unsigned char) ((ctox(pszRegResponse[2]) << 4) + ctox(pszRegResponse[3]));
                i++;
                pszRegResponse += 5;
                continue;
            }
            // <macro>
            if (pMdmMacro) 
            {
                // Check for a matching macro.
                cchName = wcslen(pMdmMacro->MacroName);
                if (!wcsnicmp(pszRegResponse, pMdmMacro->MacroName, cchName))
                {
                    cchVal = wcslen(pMdmMacro->MacroValue);
                    if (cchVal < (cchExpanded - i)) {
                        memcpy(&pszExpanded[i], pMdmMacro->MacroValue, cchVal * sizeof(WCHAR));
                        i += cchVal;
                        pszRegResponse += cchName;
                        continue;
                    } else {
                        DEBUGMSG(1, (L"UNIMODEM:ExpandMacros Macro expansion failed! Buffer too small.\n"));
                        break;
                    }
                }
            }  // <macro>
        } // LMSCH
        
        // No matches, copy the character verbatim.
        pszExpanded[i] = *pszRegResponse++;
        i++;
    } // for

    if (i < cchExpanded) {
        pszExpanded[i] = 0;
    }

    DEBUGMSG( ZONE_DIAL, (TEXT("UNIMODEM:-ExpandMacros : Expanded \"%s\"\r\n"), pszExpanded ));

    return TRUE;
}

// Create the dial strings in memory ala:
//              "<prefix> <blind_on/off> <dial prefix> <phonenumber> <dial suffix> <terminator>"
//              ...  more dial strings for long phone numbers...
//              "" <- final null of a doubly null terminated list
//
//  if no dial prefix, then return NULL
//  if no dial suffix, then don't do any commands after the first dial command
//
//  Set fOriginate to TRUE if these dial strings will cause a connection origination.
//                    FALSE otherwise.
//
//  break lines longer than pLineDev->dwMaxCmd
//
//  BUGBUG - this function is reall cheesy and hacked.  The main reason for this
//  is that it attempts to be memory (read: stack) optimized.
//
//  szPhoneNumber is a null terminated string of digits (0-9, $, @, W), with a possible
//  ';' at the end.  The semicolon can only be at the end.
//
//  Examples:
//
//  ""         -> originate          -> ATX_DT<cr>         fOriginate = TRUE
//  ";"        -> dialtone detection -> ATX_DT;<cr>        fOriginate = FALSE
//  "5551212"  -> dial and originate -> ATX_DT5551212<cr>  fOriginate = TRUE
//  "5551212;" -> dial               -> ATX_DT5551212;<cr> fOriginate = FALSE
//  "123456789012345678901234567890123456789012345678901234567890"
//             -> dial and originate -> ATX_DT12345678901234567890123456789012;<cr>
//                                      ATX_DT3456789012345678901234567890<cr>
//                                                         fOriginate = TRUE
//  "123456789012345678901234567890123456789012345678901234567890;"
//             -> dial               -> ATX_DT12345678901234567890123456789012;<cr>
//                                      ATX_DT3456789012345678901234567890;<cr>
//                                                         fOriginate = FALSE
//
PWCHAR
CreateDialCommands(
    PTLINEDEV pLineDev,
    BOOL *fOriginate
    )
{
    DWORD  dwSize;
    DWORD  dwTmp;
    PWCHAR pszTemp;
    PWCHAR pszDialPrefix;    // ex. "ATX4DT" or "ATX3DT"
    PWCHAR pszDialSuffix;    // ex. ";<cr>"
    PWCHAR pszOrigSuffix;    // ex. "<cr>"
    PWCHAR pszzDialCommands = NULL;
    PWCHAR ptchSrc, ptchDest;
    WCHAR  pszShortTemp[2];
    static const WCHAR szPrefix[] =     TEXT("Prefix");
    static const WCHAR szTerminator[] = TEXT("Terminator");
    static const WCHAR szDialPrefix[] = TEXT("DialPrefix");
    static const WCHAR szPulse[] =      TEXT("Pulse");
    static const WCHAR szTone[] =       TEXT("Tone");
    static const WCHAR *szDialType;  // szTone or szPulse
    static const WCHAR szBlindOff[] =   TEXT("Blind_Off");
    static const WCHAR szBlindOn[] =    TEXT("Blind_On");
    static const WCHAR *szBlindType;  // szBlindOn or szBlindOff

    if (NULL == pLineDev->szAddress) {
        DEBUGMSG( 1, (TEXT("UNIMODEM:CreateDialCommands - pLineDev->szAddress == NULL!!!\r\n")));
        return NULL;
    }

    DEBUGMSG( ZONE_DIAL, (TEXT("UNIMODEM:+CreateDialCommands - number %s\r\n"),
               pLineDev->szAddress ? pLineDev->szAddress : TEXT("") ));

     // Figure out fOriginate
    ptchSrc = pLineDev->szAddress;
    
    *fOriginate = TRUE;
    while (*ptchSrc)
    {
        if (pLineDev->chContinuation == *ptchSrc)   // usually a semicolon
        {
            *fOriginate = FALSE;
#ifdef DEBUG
            // make sure the string is correctly formed.
            if (ptchSrc[1])
            {
                DEBUGMSG(ZONE_DIAL, (TEXT("UNIMODEM: CreateDialCommands szPhoneNumber had a line continuation character not at the end.\r\n")));
            }
#endif // DEBUG
        }
        ptchSrc++;
    }

    // Trim the command continuation character off the end, now that we know this is not an origination string.
    if (!(*fOriginate))
    {
        DEBUGMSG(ZONE_DIAL, (TEXT("UNIMODEM:CreateDialCommands Non-originate string, trim trailing \"%c\"\r\n"),pLineDev->chContinuation));
#ifdef DEBUG
        if (ptchSrc[-1] != pLineDev->chContinuation)    // usually a semicolon
        {
            DEBUGMSG( ZONE_DIAL, (TEXT("UNIMODEM:CreateDialCommands made a bad assumption.\r\n")));
        }
#endif // DEBUG
        ptchSrc[-1] = 0;
    }

    // At this point, szPhoneNumber is just a string of digits to be dialed, with no semicolon at
    // the end.  Plus we know whether to originate or not.

    // make some temp space for pszTemp, pszDialPrefix, pszDialSuffix and pszOrigSuffix
    //
    // dwSize = ((pLineDev->dwMaxCmd + 1 +    // pszTemp
    //            pLineDev->dwMaxCmd + 1 +    // pszDialPrefix
    //            pLineDev->dwMaxCmd + 1 +    // pszDialSuffix
    //            pLineDev->dwMaxCmd + 1)     // pszOrigSuffix
    //           * SZWCHAR);
    #define NUM_DIAL_STRINGS 4

    if ((STATUS_SUCCESS == RtlUIntMult(pLineDev->dwMaxCmd, NUM_DIAL_STRINGS*SZWCHAR, &dwTmp)) &&
        (STATUS_SUCCESS == RtlUIntAdd(dwTmp, NUM_DIAL_STRINGS*SZWCHAR, &dwSize))) { // room for null terminators.
        pszTemp = (PWCHAR)TSPIAlloc( dwSize );
    } else {
        pszTemp = NULL;
    }
    
    if (NULL == pszTemp)
    {
        DEBUGMSG( ZONE_DIAL, (TEXT("UNIMODEM:-CreateDialCommands : out of memory.\r\n")));
        return NULL;
    }
    DEBUGMSG(ZONE_DIAL, (TEXT("UNIMODEM:CreateDialCommands Allocated %d bytes at x%X for tmp dial strings\r\n"),
               dwSize, pszTemp));

    pszDialPrefix = pszTemp       + pLineDev->dwMaxCmd + 1;
    pszDialSuffix = pszDialPrefix + pLineDev->dwMaxCmd + 1;
    pszOrigSuffix = pszDialSuffix + pLineDev->dwMaxCmd + 1;

        // read in prefix
        dwSize = pLineDev->dwMaxCmd * SZWCHAR;
        if (ERROR_SUCCESS !=
            MdmRegGetValue( pLineDev,
                            szSettings,
                            szPrefix,
                            REG_SZ,
                            (PUCHAR)pszTemp,
                            &dwSize) )
        {
            goto Failure;
        }
        ExpandMacros(pszTemp, pszDialPrefix, pLineDev->dwMaxCmd, NULL);

        //
        // BUGBUG do we need to set the blind dialing state?  This one is strange
        // since the user can request BLIND_DIAL in the config dialog, but
        // we also request it in a few places by setting it as part of
        // dwDialOptions.  I guess we will look for it in either place.
        //
        if ( (MDM_BLIND_DIAL & pLineDev->DevMiniCfg.dwModemOptions) ||
             (MDM_BLIND_DIAL & pLineDev->dwDialOptions) )
        {   // Turn on blind dialing
            szBlindType = szBlindOn;
            DEBUGMSG( ZONE_DIAL,
                      (TEXT("UNIMODEM:CreateDialCommands BLIND DIALING - ModemOptions 0x%X, DialOptions 0x%X\r\n"),
                       pLineDev->DevMiniCfg.dwModemOptions, pLineDev->dwDialOptions ));
        }
        else
        {   // Turn off blind dialing
            szBlindType = szBlindOff;        
        }
    
        // BUGBUG. Some strange japanese modems can't handle the blind dial option in
        // conjunction with the actual dial command. So I split it out into its own
        // AT command later on.  We used to do it here as part of the pszDialPrefix.
#ifdef BLIND_OPTION_IN_PREFIX
        // read in appropriate blind options
        dwSize = pLineDev->dwMaxCmd * SZWCHAR;
        if (ERROR_SUCCESS !=
            MdmRegGetValue( pLineDev,
                            szSettings,
                            szBlindType,
                            REG_SZ,
                            (PUCHAR)pszTemp,
                            &dwSize) )
        {
            goto Failure;
        }
        ExpandMacros(pszTemp, pszDialPrefix + wcslen(pszDialPrefix), pLineDev->dwMaxCmd, NULL);
#endif
    

    // read in dial prefix
    dwSize = pLineDev->dwMaxCmd * SZWCHAR;
    if (ERROR_SUCCESS != MdmRegGetValue( pLineDev,
                                         szSettings,
                                         szDialPrefix,
                                         REG_SZ,
                                         (PUCHAR)pszTemp,
                                         &dwSize) )
    {
        goto Failure;
    }
    
    ExpandMacros(pszTemp, pszDialPrefix + wcslen(pszDialPrefix), pLineDev->dwMaxCmd, NULL);
    
    // BUGBUG read in dial mode (tone or pulse).  Note that we get this from dwDialOptions
    // which got set when we translated the address.  This is a tad wierd since it
    // gets to us via the phone number rather than in fwOptions.
    dwSize = pLineDev->dwMaxCmd * SZWCHAR;
    if (MDM_TONE_DIAL & pLineDev->dwDialOptions)
    {   // We want to do tone dialing
        szDialType = szTone;
    }
    else
    {   // We want to do pulse dialing
        szDialType = szPulse;
    }
            
    if (ERROR_SUCCESS != MdmRegGetValue( pLineDev,
                                         szSettings,
                                         szDialType,
                                         REG_SZ,
                                         (PUCHAR)pszTemp,
                                         &dwSize) )
    {
        goto Failure;
    }
    
    ExpandMacros(pszTemp, pszDialPrefix + wcslen(pszDialPrefix), pLineDev->dwMaxCmd, NULL);
    
    // read in dial suffix
    dwSize = pLineDev->dwMaxCmd * SZWCHAR;
    if (ERROR_SUCCESS != MdmRegGetValue( pLineDev,
                                         szSettings,
                                         szDialSuffix,
                                         REG_SZ,
                                         (PUCHAR)pszTemp,
                                         &dwSize) )
    {
        wcscpy(pszDialSuffix, TEXT(""));
#ifdef TODO
        hPort->mi_fHaveDialSuffix = FALSE;
#endif
    }
    else
    {
#ifdef TODO
        hPort->mi_fHaveDialSuffix = TRUE;
#endif
        ExpandMacros(pszTemp, pszDialSuffix, pLineDev->dwMaxCmd, NULL);
    }

    // read in prefix terminator
    dwSize = pLineDev->dwMaxCmd * SZWCHAR;
    if (ERROR_SUCCESS == MdmRegGetValue( pLineDev,
                                         szSettings,
                                         szTerminator,
                                         REG_SZ,
                                         (PUCHAR)pszTemp,
                                         &dwSize) )
    {
        ExpandMacros(pszTemp, pszOrigSuffix, pLineDev->dwMaxCmd, NULL);
        wcscat(pszDialSuffix, pszOrigSuffix);
#ifdef DEBUG
        if (wcslen(pszOrigSuffix) > wcslen(pszDialSuffix))
        {
            DEBUGMSG( ZONE_DIAL,
                      (TEXT("UNIMODEM:CreateDialCommands OrigSuffix longer than DialSuffix!!! (contact developer)\r\n")));
        }
#endif // DEBUG
    }

#ifdef DEBUG
    if ((wcslen(pszDialPrefix) + wcslen(pszDialSuffix)) > pLineDev->dwMaxCmd)
    {
        DEBUGMSG( ZONE_DIAL,
                  (TEXT("UNIMODEM:CreateDialCommands Prefix, Terminator, BlindOn/Off, Dial Prefix, and Dial Suffix are too long!\r\n")));
    }
#endif // DEBUG

    // allocate space for the phone number lines
    {
        DWORD dwCharsAlreadyTaken = wcslen(pszDialPrefix) + wcslen(pszDialSuffix);
        DWORD dwAvailCharsPerLine = (pLineDev->dwMaxCmd - dwCharsAlreadyTaken);
        DWORD dwPhoneNumLen       = wcslen(pLineDev->szAddress);
        DWORD dwNumLines          = dwPhoneNumLen ? (dwPhoneNumLen / dwAvailCharsPerLine +
                                                     (dwPhoneNumLen % dwAvailCharsPerLine ? 1 : 0))
                                                  : 1;  // handle null string
        dwSize                    = (dwPhoneNumLen + dwNumLines * (dwCharsAlreadyTaken + 1) + 1) * SZWCHAR;

        DEBUGMSG( ZONE_DIAL,
                  (TEXT("UNIMODEM:CreateDialCommands Allocation sizes - prefix/suffix %d, Phone num %d, numLines %d\r\n"),
                   dwCharsAlreadyTaken, dwPhoneNumLen, dwNumLines));
    }

    DEBUGMSG( ZONE_DIAL,
              (TEXT("UNIMODEM:CreateDialCommands Allocate %d bytes for Dial Commands.\r\n"), dwSize));

     // BUGBUG - Lets see if bigger buffer will save us
    dwSize *= 4;
    
    if (!(pszzDialCommands = (PWCHAR)TSPIAlloc(dwSize)))
    {
        DEBUGMSG( ZONE_DIAL,
                  (TEXT("UNIMODEM:CreateDialCommands ran out of memory and failed a TSPIAlloc!\r\n")));
        goto Failure;
    }
    DEBUGMSG( ZONE_DIAL,
              (TEXT("UNIMODEM:CreateDialCommands Allocated %d bytes at x%X for dial string\r\n"),
               dwSize, pszzDialCommands));
    
    ptchDest = pszzDialCommands;  // point to the beginning of the commands
        
    // build dial line(s):

#ifndef BLIND_OPTION_IN_PREFIX
    // See above : we now split the blind dial out from the dial prefix.

    // read in prefix
    dwSize = pLineDev->dwMaxCmd * SZWCHAR;
    if (ERROR_SUCCESS !=
        MdmRegGetValue( pLineDev,
                        szSettings,
                        szPrefix,
                        REG_SZ,
                        (PUCHAR)pszTemp,
                        &dwSize) )
    {
        goto Failure;
    }
    ExpandMacros(pszTemp, ptchDest, pLineDev->dwMaxCmd, NULL);

    // read in appropriate blind options
    dwSize = pLineDev->dwMaxCmd * SZWCHAR;
    if (ERROR_SUCCESS !=
        MdmRegGetValue( pLineDev,
                        szSettings,
                        szBlindType,
                        REG_SZ,
                        (PUCHAR)pszTemp,
                        &dwSize) )
    {
        goto Failure;
    }
    dwSize = wcslen(ptchDest);
    ExpandMacros(pszTemp, ptchDest + dwSize, pLineDev->dwMaxCmd - dwSize, NULL);
    DEBUGMSG( ZONE_DIAL,
              (TEXT("UNIMODEM:CreateDialCommands prepended Blind dial sequence = %s, len %d\r\n"),
               ptchDest, wcslen(ptchDest)));    

    //OK, be sure to add a CR to this
    dwSize = wcslen(ptchDest);
    ExpandMacros(CR_MACRO, ptchDest + dwSize, pLineDev->dwMaxCmd - dwSize, NULL);
    DEBUGMSG( ZONE_DIAL,
              (TEXT("UNIMODEM:CreateDialCommands prepended Blind dial sequence = %s, len %d\r\n"),
               ptchDest, wcslen(ptchDest)));    

    // begin a new dial string
    ptchDest += wcslen(ptchDest) + 1;
#endif

    // prime the pump
    DEBUGMSG( ZONE_DIAL,
              (TEXT("UNIMODEM:CreateDialCommands Copying Dial prefix %s to x%X\r\n"),
               pszDialPrefix, ptchDest));
    wcscpy(ptchDest, pszDialPrefix);

    
    // do we have a dial suffix
    if ( /*!hPort->mi_fHaveDialSuffix*/ FALSE )
    {
        // BUGBUG we can't do much except just use the whole string and pray...
#ifdef DEBUG
        // but, can we fit the dial string?
        if (wcslen(pszDialPrefix) + wcslen(pLineDev->szAddress) +
            wcslen(pszDialSuffix) + 1 > pLineDev->dwMaxCmd)
        {
            DEBUGMSG( ZONE_DIAL,
                      (TEXT("UNIMODEM:CreateDialCommands dial string is too long and we don't have dial suffix capability\r\n")));
        }
        // did we not want to originate?
        if (!(*fOriginate))
        {
            DEBUGMSG( ZONE_DIAL,
                      (TEXT("UNIMODEM:CreateDialCommands was told not to originate, but it has to because it doesn't have a dial suffix! (tsp error!)\r\n")));
        }
#endif // DEBUG

        // build it
        DEBUGMSG( ZONE_DIAL,
                  (TEXT("UNIMODEM:CreateDialCommands Appending phone number %s\r\n"), pLineDev->szAddress));
        wcscat(ptchDest, pLineDev->szAddress);        
        DEBUGMSG( ZONE_DIAL,
                  (TEXT("UNIMODEM:CreateDialCommands Appending Dial Suffix %s\r\n"), pszDialSuffix));
        wcscat(ptchDest, pszDialSuffix);
        DEBUGMSG( ZONE_DIAL,
                  (TEXT("UNIMODEM:CreateDialCommands Final Dial String %s\n"), ptchDest));

    }
    else
    {
        // we have a dial suffix.

        // populate new pszzDialCommands with semi-colons as necessary.

        // go through and add suffixi, making sure lines don't exceed pLineDev->dwMaxCmd
        ptchSrc = pLineDev->szAddress;     // moves a character at a time.
        pszShortTemp[1] = 0;    // null terminate the 1 character temp string

         // step through the source
        while (*ptchSrc)
        {
            if (wcslen(ptchDest) + wcslen(pszDialSuffix) + 1 > pLineDev->dwMaxCmd)
            {
                // finish up this string
                DEBUGMSG( ZONE_DIAL,
                          (TEXT("UNIMODEM:CreateDialCommands Appending Dial suffix %s\r\n"), pszDialSuffix));
                wcscat(ptchDest, pszDialSuffix);

                // begin a new string
                ptchDest += wcslen(ptchDest) + 1;
                DEBUGMSG( ZONE_DIAL,
                          (TEXT("UNIMODEM:CreateDialCommands Copying Dial prefix %s for a new line at 0x%X\r\n"),
                           pszDialPrefix, ptchDest));
                wcscpy(ptchDest, pszDialPrefix);
            }
            else
            {
                // copy char
                DEBUGMSG( ZONE_DIAL,
                          (TEXT("UNIMODEM:CreateDialCommands Appending character %c to dial string %s\r\n"),
                           *ptchSrc, ptchDest));
                pszShortTemp[0] = *ptchSrc;
                wcscat(ptchDest, pszShortTemp);
                ptchSrc++;
            }
        }
        
         // conclude with the approprate Suffix.
        DEBUGMSG( ZONE_DIAL,
                  (TEXT("UNIMODEM:CreateDialCommands Appending Final Dial Suffix %s\r\n"), (*fOriginate ? pszOrigSuffix : pszDialSuffix)));
        wcscat(ptchDest, (*fOriginate ? pszOrigSuffix : pszDialSuffix));
    }

    // close keys  
Exit:
    DEBUGMSG( ZONE_DIAL, (TEXT("UNIMODEM:CreateDialCommands Trying to free pszTemp x%X\r\n"), pszTemp));
    TSPIFree(pszTemp);
    DEBUGMSG( ZONE_DIAL, (TEXT("UNIMODEM:-CreateDialCommands\r\n")));

    return pszzDialCommands;

Failure:
    if (pszzDialCommands)
    {
        DEBUGMSG( ZONE_DIAL|ZONE_ERROR, (TEXT("UNIMODEM:CreateDialCommands - Error, freeing dial string\r\n")));
        TSPIFree(pszzDialCommands);
        pszzDialCommands = NULL;
    }
    goto Exit;
}


BOOL
IsDigit(
    UCHAR ch
    )
{
    switch (ch) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return TRUE;
    }

    return FALSE;
}

//
// Extract the connection speed from an extended CONNECT or CARRIER modem response
// and store it in pLineDev->dwCurrentBaudRate
//
void
ParseConnectSpeed(
    PTLINEDEV pLineDev,
    PUCHAR pszResponse
    )
{
    DWORD dwSpeed = 0;
    PUCHAR psz;

    DEBUGMSG(ZONE_FUNC|ZONE_CALLS, (TEXT("UNIMODEM:+ParseConnectSpeed %a\n"), pszResponse));
    //
    // CONNECT and CARRIER typically follow the form:
    // CONNECT <speed></protocol>< protocol> <compressed>
    // CARRIER <speed></protocol> < protocol> <compressed>
    // Sometimes there is a colon and/or a space before the speed.
    // Sometimes a K follows the speed indicating * 1000
    // Sometimes there are commas in the speed
    //

    //
    // Skip to the first digit
    //
    psz = pszResponse;
    while (*psz) {
        if (IsDigit(*psz)) {
            break;
        }
        psz++;
    }

    while (*psz) {
        if (IsDigit(*psz)) {
            dwSpeed = dwSpeed * 10 + (DWORD)(*psz - '0');
        } else if (*psz == 'K') {
            dwSpeed *= 1000;
        } else if (*psz != ',') {
            break;
        }
        psz++;
    }

    if (dwSpeed) {
        pLineDev->dwCurrentBaudRate = dwSpeed;
        if (pLineDev->htCall) {
            CallLineEventProc(
                pLineDev,
                pLineDev->htCall,
                LINE_CALLINFO,
                LINECALLINFOSTATE_RATE,
                0,
                0
                );
        }
        
    }

    DEBUGMSG(ZONE_FUNC|ZONE_CALLS, (TEXT("UNIMODEM:-ParseConnectSpeed %d\n"), dwSpeed));
}


const
MODEM_RESPONSE ResponseTable[] =
{
    { "OK",             MODEM_SUCCESS },
    { "BUSY",           MODEM_BUSY},
    { "CARRIER",        MODEM_CARRIER },
    { "CONNECT",        MODEM_CONNECT },
    { "NO ANSWER",      MODEM_NOANSWER },
    { "NO CARRIER",     MODEM_HANGUP },
    { "NO DIALTONE",    MODEM_NODIALTONE },
    { "NO DIAL TONE",   MODEM_NODIALTONE },
    { "PROTOCOL",       MODEM_PROTOCOL },
    { "RINGING",        MODEM_RING },
    { "RING",           MODEM_RING },
    { "BLACKLIST",      MODEM_NOANSWER },
    { "DATA",           MODEM_FAILURE },
    { "DATE",           MODEM_PROGRESS },
    { "DELAYED",        MODEM_FAILURE },
    { "DIALING",        MODEM_PROGRESS },
    { "ERROR",          MODEM_FAILURE },
    { "FAX",            MODEM_FAILURE },
    { "FCERROR",        MODEM_FAILURE },
    { "FCON",           MODEM_FAILURE },
    { "NOT CORRECT",    MODEM_NODIALTONE },
    { "NOT READY",      MODEM_NODIALTONE },
    { "OUT OF SERVICE", MODEM_NODIALTONE },
    { "PS NO RESPONSE", MODEM_NODIALTONE },
    { "RESTRICT",       MODEM_NODIALTONE },
    { "RRING",          MODEM_RING},
    { "VCON",           MODEM_SUCCESS },
    { "VOICE",          MODEM_FAILURE },
    { "+FCERROR",       MODEM_FAILURE },
    { "+FCON",          MODEM_FAILURE },
    { "+CRING",         MODEM_RING},
    { "+CME ERROR",     MODEM_FAILURE },
    { "+ILRR",          MODEM_CARRIER },
    { "+MRR",           MODEM_CARRIER },
};

#ifdef DEBUG
LPWSTR
ResponseName(
    MODEMRESPCODES MdmRsp
    )
{
    switch (MdmRsp) {
    case MODEM_SUCCESS:     return TEXT("MODEM_SUCCESS");
    case MODEM_PENDING:     return TEXT("MODEM_PENDING");
    case MODEM_CONNECT:     return TEXT("MODEM_CONNECT");
    case MODEM_FAILURE:     return TEXT("MODEM_FAILURE");
    case MODEM_HANGUP:      return TEXT("MODEM_HANGUP");
    case MODEM_NODIALTONE:  return TEXT("MODEM_NODIALTONE");
    case MODEM_BUSY:        return TEXT("MODEM_BUSY");
    case MODEM_NOANSWER:    return TEXT("MODEM_NOANSWER");
    case MODEM_RING:        return TEXT("MODEM_RING");
    case MODEM_CARRIER:     return TEXT("MODEM_CARRIER");
    case MODEM_PROTOCOL:    return TEXT("MODEM_PROTOCOL");
    case MODEM_PROGRESS:    return TEXT("MODEM_PROGRESS");
    case MODEM_UNKNOWN:     return TEXT("MODEM_UNKNOWN");
    case MODEM_IGNORE:      return TEXT("MODEM_IGNORE");
    case MODEM_EXIT:        return TEXT("MODEM_EXIT");
    default:                return TEXT("UNKNOWN Modem Response Code!!!");
    }
}

LPWSTR
PendingOpName(
    DWORD dwPendingOp
    )
{
    switch (dwPendingOp){
    case INVALID_PENDINGOP:        return TEXT("INVALID_PENDINGOP");
    case PENDING_LINEMAKECALL:     return TEXT("PENDING_LINEMAKECALL");
    case PENDING_LINEANSWER:       return TEXT("PENDING_LINEANSWER");
    case PENDING_LINEDROP:         return TEXT("PENDING_LINEDROP");
    case PENDING_LINEDIAL:         return TEXT("PENDING_LINEDIAL");
    case PENDING_LINEACCEPT:       return TEXT("PENDING_LINEACCEPT");
    case PENDING_LINEDEVSPECIFIC:  return TEXT("PENDING_LINEDEVSPECIFIC");
    case PENDING_LISTEN:           return TEXT("PENDING_LISTEN");
    case PENDING_EXIT:             return TEXT("PENDING_EXIT"); 
    default:                       return TEXT("");
    } // endswitch
}
#endif // DEBUG

const UCHAR pchDCCResp[] = "CLIENTSERVER";  // Registry configurable with Settings\DCCResponse
const UCHAR pchDCCCmd[] = "CLIENT";         // Registry configurable with Settings\DCCRequest
        
#define SZ_RESPONSE_TABLE (sizeof(ResponseTable)/sizeof(MODEM_RESPONSE))

MODEMRESPCODES
ModemResponseHandler(
    PTLINEDEV pLineDev,
    PUCHAR pszResponse,
    PUCHAR pszOrigCommand
    )
{
    UINT16 wRespCode = MODEM_UNKNOWN;
    int i;
    
    DEBUGMSG(ZONE_FUNC,
             (TEXT("UNIMODEM:+ModemResponseHandler x%X, x%X, x%X\r\n"),
              pLineDev, pszResponse, pszOrigCommand));

    // Lets remove any leading CR/LF from response.
    while( (CR == *pszResponse) || (LF == *pszResponse) )
        pszResponse++;
    
    if( 0 == *pszResponse )
    {
        DEBUGMSG(ZONE_FUNC,
                 (TEXT("UNIMODEM:-ModemResponseHandler, Empty response\r\n")));
        return MODEM_IGNORE;
    }

    DEBUGMSG(ZONE_CALLS,
             (TEXT("UNIMODEM:ModemResponseHandler Modem Response '%a'\r\n"), pszResponse));

    // Lets remove any leading CR/LF from orig command.
    while( (CR == *pszOrigCommand) || (LF == *pszOrigCommand) )
        pszOrigCommand++;
    
    // First, check to see if this is just an echo of the command
    if( NULL != pszOrigCommand )
    {
        DEBUGMSG(ZONE_FUNC,
                 (TEXT("UNIMODEM:ModemResponseHandler Checking for echo of CMD %a\r\n"), pszOrigCommand));
        
        if( ! strncmpi( pszResponse, pszOrigCommand, strlen(pszResponse) ) )
        {
            // Ignore this, it was just a duplicate
            DEBUGMSG(ZONE_FUNC,
                     (TEXT("UNIMODEM:-ModemResponseHandler, Command Echo\r\n")));
            return MODEM_IGNORE; 
        }
    }
    
    DEBUGMSG(ZONE_FUNC,
             (TEXT("UNIMODEM:ModemResponseHandler response was not an echo\r\n")));
        
    
    // Regular Modems have a lookup table for responses
        
    // Lets convert the ASCII response to a numeric code
    for( i=0; i<SZ_RESPONSE_TABLE; i++ )
    {
        if( !strncmpi( pszResponse, ResponseTable[i].pszResponse, strlen(ResponseTable[i].pszResponse) ) )
        {
            // Found a match.
            wRespCode = ResponseTable[i].ModemRespCode;
            break;
        }
    }
    DEBUGMSG(ZONE_CALLS|ZONE_FUNC,
             (TEXT("UNIMODEM:-ModemResponseHandler, %s\n"), ResponseName(wRespCode) ));

    return wRespCode;
}


//
// Expand macros and convert command to chars
//
LPSTR
MdmConvertCommand(
    WCHAR const *pszWCommand,
    LPSTR pchCmd,
    LPDWORD lpdwSize
    )
{
    LPSTR lpstr;
    WCHAR szExpand[256];
    DWORD dwCmdLen;

    if (pchCmd) {
        lpstr = pchCmd;
    } else {
        lpstr = NULL;
    }

    if (!ExpandMacros((PWCHAR)pszWCommand, (PWCHAR)szExpand, 256, NULL)) {
        return NULL;
    }

    dwCmdLen = wcslen(szExpand) + 1;

    if (lpstr == NULL) {
        lpstr = TSPIAlloc(dwCmdLen);
        if (lpstr == NULL) {
            return NULL;
        }
    }
    dwCmdLen = wcstombs( lpstr, szExpand, dwCmdLen );

    if (lpdwSize) {
        *lpdwSize = dwCmdLen;
    }

    return lpstr;
}
    

BOOL
MdmSendCommand(
    PTLINEDEV pLineDev,
    UCHAR const *pszCommand
    )
{
    DWORD   dwLen;
    BOOL bRet = FALSE;
    
    //
    // This routine sends a Modem command, and then waits for
    // a valid response from the modem (or timeout).
    //

    try
    {
        if(  (HANDLE)INVALID_DEVICE != pLineDev->hDevice )
        {    
            DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:MdmSendCommand '%a'\r\n"), pszCommand));
            //
            // Some modems just can't take commands so fast.
            //
            if(pLineDev->dwCmdSendDelay) {
                Sleep(pLineDev->dwCmdSendDelay);
            }

            // Purge any old responses sitting in serial buffer
            if (PurgeComm( pLineDev->hDevice, PURGE_RXCLEAR|PURGE_TXCLEAR )) {
                // Now, lets send this nice new string to the modem
                bRet = WriteFile(pLineDev->hDevice, (LPVOID)pszCommand, strlen((LPVOID)pszCommand), &dwLen, 0 );
                if ((FALSE == bRet) || (dwLen != strlen((LPVOID)pszCommand))) {
                    bRet = FALSE;
                    RETAILMSG (1,
                               (TEXT("UNIMODEM:!!!MdmSendCommand wrote %d of %d byte dial string\r\n"),
                                dwLen, strlen((LPVOID)pszCommand)) );
                                       
                    DEBUGMSG(ZONE_MISC,
                             (TEXT("UNIMODEM:MdmSendCommand Wrote %d bytes of %d byte dial string\r\n"),
                              dwLen, strlen((LPVOID)pszCommand)) );
                }
            }
        }
    }
    except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
            EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        // Just return of handle is bad
        bRet = FALSE;
    }

    WriteModemLog(pLineDev, (PUCHAR)pszCommand, bRet ? MDMLOG_COMMAND_OK : MDMLOG_COMMAND_FAIL);
    return bRet;
}


#ifdef DEBUG
void
DisplayModemStatus(
    DWORD ModemStat
    )
{
    WCHAR szModemStat[256];

    szModemStat[0] = 0;

    if( ModemStat & EV_BREAK)
        wcscat(szModemStat, TEXT(" EV_BREAK"));
    if( ModemStat & EV_CTS)
        wcscat(szModemStat, TEXT(" EV_CTS"));
    if( ModemStat & EV_DSR)
        wcscat(szModemStat, TEXT(" EV_DSR"));
    if( ModemStat & EV_ERR)
        wcscat(szModemStat, TEXT(" EV_ERR"));
    if( ModemStat & EV_RING)
        wcscat(szModemStat, TEXT(" EV_RING"));
    if( ModemStat & EV_RLSD)
        wcscat(szModemStat, TEXT(" EV_RLSD"));
    if( ModemStat & EV_RXCHAR)
        wcscat(szModemStat, TEXT(" EV_RXCHAR"));
    if( ModemStat & EV_RXFLAG)
        wcscat(szModemStat, TEXT(" EV_RXFLAG"));
    if( ModemStat & EV_TXEMPTY)
        wcscat(szModemStat, TEXT(" EV_TXEMPTY"));

    DEBUGMSG (ZONE_MISC, (TEXT("UNIMODEM:DisplayModemStatus ModemStat =%s\n"), szModemStat));
}   // DisplayModemStatus

void
DisplayCommError(
    DWORD dwCommError
    )
{
    WCHAR szCommError[256];

    szCommError[0] = 0;

    if (dwCommError & CE_BREAK) {
        wcscat(szCommError, TEXT(" CE_BREAK"));
    }
    if (dwCommError & CE_FRAME) {
        wcscat(szCommError, TEXT(" CE_FRAME"));
    }
    if (dwCommError & CE_IOE) {
        wcscat(szCommError, TEXT(" CE_IOE"));
    }
    if (dwCommError & CE_MODE) {
        wcscat(szCommError, TEXT(" CE_MODE"));
    }
    if (dwCommError & CE_OOP) {
        wcscat(szCommError, TEXT(" CE_OOP"));
    }
    if (dwCommError & CE_OVERRUN) {
        wcscat(szCommError, TEXT(" CE_OVERRUN"));
    }
    if (dwCommError & CE_RXOVER) {
        wcscat(szCommError, TEXT(" CE_RXOVER"));
    }
    if (dwCommError & CE_RXPARITY) {
        wcscat(szCommError, TEXT(" CE_RXPARITY"));
    }
    if (dwCommError & CE_TXFULL) {
        wcscat(szCommError, TEXT(" CE_TXFULL"));
    }
    if (dwCommError & CE_DNS) {
        wcscat(szCommError, TEXT(" CE_DNS"));
    }
    if (dwCommError & CE_PTO) {
        wcscat(szCommError, TEXT(" CE_PTO"));
    }

    DEBUGMSG (ZONE_MISC, (TEXT("UNIMODEM:DisplayCommError CommError=%s\n"), szCommError));
}   // DisplayCommError
#endif


BOOL
LineNotDropped(
    PTLINEDEV pLineDev,
    DEVSTATES OrigDevState
    )
{
    if (NULL == pLineDev->htLine) {
        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:LineNotDropped line closed\n")));
        return FALSE;
    }

    if (pLineDev->DevState == OrigDevState) {
        return TRUE;
    }
    return FALSE;
}

#define LOW_COMPLETE_READ_THRESHOLD 15  // sizeof CONNECT 115200

MODEMRESPCODES
MdmGetResponse(
    PTLINEDEV pLineDev,
    PUCHAR    pszCommand,
    BOOL      bLeaveCS,
    BOOL      bOpened
    )
{
    MODEMRESPCODES ModemResp = MODEM_IGNORE;
    DWORD   ModemStat;
    UCHAR   InBuf[MAXSTRINGLENGTH];
    UCHAR   szCmd[MAX_CMD_LENGTH];
    int     i;
    UCHAR   ModemResponse[MAXSTRINGLENGTH];
    UINT16  ModemResponseIndex = 0;
    DWORD   dwLen;
    DEVSTATES OrigDevState;
    PUCHAR  pchDCC; // either pchDCCCmd or pchDCCResp
    MODEMRESPCODES DCCResp;
    DWORD dwDCCLen;
    BOOL bInCS;
    DWORD OrigPendingOp;
    BOOL bStatus;
    
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+MdmGetResponse for %a\r\n"), pszCommand));

    OrigDevState = pLineDev->DevState;
    OrigPendingOp = pLineDev->dwPendingType;
    bInCS = bLeaveCS;

    EnterCriticalSection(&pLineDev->OpenCS);
    if (pLineDev->dwWaitMask) {
        SetCommMask(pLineDev->hDevice, EV_DEFAULT);
    }
    LeaveCriticalSection(&pLineDev->OpenCS);

    // Wait until we get some type of meaningful response from the modem
    while (MODEM_IGNORE == ModemResp)
    {
        if (bOpened) {
            if (!LineNotDropped(pLineDev, OrigDevState)) {
                break;
            }
        }

        try
        {
            ModemStat = 0;

            if (bInCS) {
                //
                // To help avoid missing signals from SignalControlThread, stay in CS as long
                // as possible before calling WaitCommEvent.
                //
                LeaveCriticalSection(&pLineDev->OpenCS);
                bInCS = FALSE;
            }
            bStatus = WaitCommEvent(pLineDev->hDevice, &ModemStat, NULL);
            //
            // Look for signals from SignalControlThread
            //
            if (pLineDev->dwPendingType != OrigPendingOp) {
                DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:MdmGetResponse %s interrupted %s\n"),PendingOpName(pLineDev->dwPendingType) , PendingOpName(OrigPendingOp)));
                switch (pLineDev->dwPendingType){
                case PENDING_LINEDROP:
                    ModemResp = MODEM_ABORT;
                   break;
                default:
                    ModemResp = MODEM_FAILURE;
                    break;
                } // endswitch
                goto mgr_exit;
            }

            if (pLineDev->bWatchdogTimedOut) {
                pLineDev->bWatchdogTimedOut = FALSE;
                DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:MdmGetResponse - Watchdog timed out\n")));
                ModemResp = MODEM_EXIT;
                goto mgr_exit;
            }

            if (FALSE == bStatus) {
                dwLen = GetLastError();
                DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:MdmGetResponse - WaitCommEvent failed %d\n"), dwLen));
                ModemResp = MODEM_EXIT;
                goto mgr_exit;
            }

            if (ModemStat) {
#ifdef DEBUG
                DisplayModemStatus(ModemStat);
#endif
                if (ModemStat & EV_ERR) {
                    if (ClearCommError(pLineDev->hDevice, &dwLen, NULL)) {
#ifdef DEBUG
                        if (dwLen) {
                            DisplayCommError(dwLen);
                        }
                    } else {
                        dwLen = GetLastError();
                        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:MdmGetResponse ClearCommError failed %d\n"), dwLen));
#endif
                    }
                }

                if( ModemStat & EV_RING) {
                    ModemResp = MODEM_RING;
                }

                if (ModemStat & EV_RXCHAR) {
                    if (bOpened) {
                        if (!LineNotDropped(pLineDev, OrigDevState)) {
                            break;
                        }
                    }

                    if (!ReadFile (pLineDev->hDevice, (LPVOID)InBuf, sizeof(InBuf)-1, &dwLen, 0)) {
                        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:MdmGetResponse - ReadFile failed %d\n"), GetLastError()));
                        ModemResp = MODEM_EXIT;
                        goto mgr_exit;
                    }

                    // If we didn't read much data, retry to make sure we have all of it.
                    if ( dwLen < LOW_COMPLETE_READ_THRESHOLD) {
                        DWORD dwNewLen;

                        // Sleep here to give the other threads ( IRCOMM ... ) time to procoess
                        Sleep(50);

                        // Null terminate for the debug output
                        InBuf[dwLen] = 0;
                        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:First MdmGetResponse Response = %a\n"), InBuf));

                        // Read from the port again
                        if (ReadFile (pLineDev->hDevice, (LPVOID)(InBuf+dwLen), sizeof(InBuf)-1-dwLen, &dwNewLen, 0)) {
                            DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:MdmGetResponse - Second ReadFile succeeded. %i characters recovered\n"),dwNewLen));
                            dwLen += dwNewLen;
                        }
                    }

                    InBuf[dwLen] = 0;
                    WriteModemLog(pLineDev, (PUCHAR)InBuf, MDMLOG_RESPONSE);
#ifdef DEBUG_RESPONSES
                    if (dwLen) {
                        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:MdmGetResponse Response = %a\n"), InBuf));
                    }
#endif
                    // Now lets process each response as they arrive
                    for( i=0; i<(int)dwLen; i++ )
                    {
                        ModemResponse[ModemResponseIndex] = InBuf[i];

                        if(  IS_NULL_MODEM(pLineDev) )
                        {
                            //
                            // Only allow one response per call to MdmGetResponse for a DCC connection
                            //
                            ModemResp = MODEM_FAILURE;

                            //
                            // Are we client or server?
                            //
                            if (pLineDev->dwPendingType == PENDING_LINEMAKECALL) {
                                GetModemCommand(pLineDev, L"DCCResponse", szCmd, (LPSTR)pchDCCResp);
                                pchDCC = (PUCHAR)szCmd;
                                DCCResp = MODEM_CONNECT;
                            } else {
                                GetModemCommand(pLineDev, L"DCCRequest", szCmd, (LPSTR)pchDCCCmd);
                                pchDCC = (PUCHAR)szCmd;
                                DCCResp = MODEM_RING;
                                if ((strlen(pchDCC)) == 0)
                                {
                                    ModemResp = DCCResp;
                                    DEBUGMSG (ZONE_CALLS, (TEXT("UNIMODEM:MdmGetResponse - ....\r\n")));
                                    goto mgr_exit;
                                }
                            }
                            dwDCCLen = strlen(pchDCC);

                            if( ModemResponse[ModemResponseIndex] == pchDCC[ModemResponseIndex] )
                            {
                                ModemResponseIndex++;

                                // characters match so far, see if we have a complete response.
                                if( ModemResponseIndex >= dwDCCLen )
                                {
                                    // OK, we got a full response.  Zero terminate it and exit
                                    ModemResponse[ModemResponseIndex] = 0;
                                    // No need to call response handler, we already did all the work.
                                    ModemResp = DCCResp;
                                    DEBUGMSG (ZONE_CALLS, (TEXT("UNIMODEM:MdmGetResponse Null Response detected\r\n")));
                                    break;
                                }
                                
                            }
                            else // character mismatch - restart scan sequence.
                            {
                                // If character is 'C', it might be first character of the actual
                                // response.  Any other case means that all data up to here is useless
                                // so we should toss it.  This is only true because 'C' appears only as
                                // the first character of a response, and never after that.
                                ModemResponseIndex = 0;
                                if (pchDCC[0] == InBuf[i])
                                {
                                    ModemResponse[ModemResponseIndex] = InBuf[i];
                                    ModemResponseIndex++;
                                }
                            }
                        }
                        else
                        {
                            if( (CR == InBuf[i]) ||
                                (LF == InBuf[i]) ||
                                ((DWORD)i == dwLen-1)   ||
                                ((pLineDev->dwMaxCmd - 1) <= ModemResponseIndex) ) 
                            {
                                // have a complete response (or max length).  Now we parse it.
                                ModemResponse[ModemResponseIndex] = 0;  // Lets Zero Terminate the string
                                ModemResp = ModemResponseHandler( pLineDev, ModemResponse, pszCommand );
                                
                                // BUGBUG Hmm, what do we want to do if we have more data to get, but the
                                // response code looks interesting/good.  For now, I guess what I will
                                // do is return the interesting code and lose the remaining stuff.
                                switch (ModemResp) {
                                case MODEM_CARRIER:
                                    ParseConnectSpeed(pLineDev, ModemResponse);
                                    // fall through!!!

                                case MODEM_PROTOCOL:
                                case MODEM_PROGRESS:
                                    NewCallState(pLineDev, LINECALLSTATE_PROCEEDING, 0L);
                                    if (i < (int)(dwLen - 1)) {
                                        ModemResponseIndex = 0;  // Start a new response
                                        ModemResp = MODEM_IGNORE;
                                    }
                                    break;

                                case MODEM_UNKNOWN:
                                case MODEM_IGNORE:
                                    ModemResponseIndex = 0;  // Start a new response
                                    ModemResp = MODEM_IGNORE;
                                    break;

                                case MODEM_CONNECT:
                                    ParseConnectSpeed(pLineDev, ModemResponse);
                                    // fall through

                                default:
                                    goto mgr_exit;
                                }

                                ModemResponseIndex = 0;  // Start a new response
                            }
                            else
                            {
                                ModemResponseIndex++;  //Add another character to the string
                            }
                        }
                    }
                
                }
            }
            else
            {
                // A zero ModemStat is an error, and indicates that the
                // waitcommevent was stopped by our watchdog or linedrop.
                ModemResp = MODEM_FAILURE;
                DEBUGMSG (ZONE_CALLS, (TEXT("UNIMODEM:MdmGetResponse - Timeout\r\n")));
                goto mgr_exit;
            }
        
            DEBUGMSG(ZONE_FUNC|ZONE_CALLS, (TEXT("UNIMODEM:MdmGetResponse %s\n"), ResponseName(ModemResp)));
        }
        except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
                EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
            // Must have been a bad handle.  Give up
            ModemResp = MODEM_FAILURE;
            DEBUGMSG (ZONE_FUNC|ZONE_CALLS,
                      (TEXT("UNIMODEM:MdmGetResponse - Exception.  Handle \r\n"),
                        pLineDev->hDevice ));
        }
    }
    
mgr_exit:    
    EnterCriticalSection(&pLineDev->OpenCS);
    pLineDev->dwWaitMask = EV_DEFAULT;
    LeaveCriticalSection(&pLineDev->OpenCS);

    if (bInCS) {
        LeaveCriticalSection(&pLineDev->OpenCS);
    }

    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-MdmGetResponse %s\r\n"), ResponseName(ModemResp)));
    return ModemResp;
}


//
// There were cases where the ModemWatchdog thread would not make it to the WaitForSingleObject
// before the next SetWatchdog happened. This would leave two or more watchdogs runnning at the
// same time which leads to all kinds of problems. Now SetWatchdog passes a UNIMODEM_WATCHDOG_INFO
// structure to the thread and the watchdog checks that it is the current one before aborting
// any pending operations.
//
typedef struct _UNIMODEM_WATCHDOG_INFO {
    PTLINEDEV pLineDev;
    DWORD dwTimeout;
    DWORD dwThisWatchdog;
} UNIMODEM_WATCHDOG_INFO, * PUNIMODEM_WATCHDOG_INFO;


// **********************************************************************
//
// This is the dialer/answer watchdog thread.  It is created by the dialer thread.
// Its purpose is to wait for a specified interval, or until signalled by
// the dialer.  If the wait times out, then the watchdog calls into the
// COM port driver, causing the waitcommevent in the dialer thread to
// return.
// 
// **********************************************************************
void
ModemWatchdog(
    PUNIMODEM_WATCHDOG_INFO pWDInfo
    )
{
    DWORD RetCode;
    BOOL bAbort;
    PTLINEDEV pLineDev;
    DWORD dwTimeout;
    DWORD dwThisWatchdog;

    pLineDev        = pWDInfo->pLineDev;
    dwTimeout       = pWDInfo->dwTimeout;
    dwThisWatchdog  = pWDInfo->dwThisWatchdog;
    TSPIFree(pWDInfo);

    DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:+ModemWatchdog(%d) - timeout set to %d seconds (ends %d)\r\n"),
        dwThisWatchdog, dwTimeout/1000, GetTickCount() + dwTimeout));
    
    
    // Wait for the specified interval.
    RetCode = WaitForSingleObject( pLineDev->hTimeoutEvent, dwTimeout );

    if ((dwThisWatchdog == pLineDev->dwCurWatchdog) && ( WAIT_TIMEOUT == RetCode )) {
        pLineDev->bWatchdogTimedOut = TRUE;

        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:ModemWatchdog(%d)  *** %s TIMEOUT at %d\n"),
            dwThisWatchdog, GetPendingName(pLineDev->dwPendingType), GetTickCount()));

        // We need to stop the waitcommevent.  Note that we set the
        // mask to 0, which will cause any subsequent waitcommevent
        // calls to also return immediately.
        SignalCommMask(pLineDev);

        switch (pLineDev->dwPendingType) {
        case PENDING_LINEMAKECALL:
            //
            // Allow retries for DCC connections and ModemInit()
            //
            if (IS_NULL_MODEM(pLineDev)) {
                bAbort = FALSE;
            } else {
                bAbort = (pLineDev->MdmState != MDMST_INITIALIZING);
            }
            break;

        case PENDING_LINEANSWER:
            NewCallState(pLineDev, LINECALLSTATE_DISCONNECTED, LINEDISCONNECTMODE_UNKNOWN);
            NewCallState(pLineDev, LINECALLSTATE_IDLE, 0L);
            bAbort = TRUE;
            break;

        case PENDING_LINEDIAL:
            bAbort = TRUE;
            break;

        case PENDING_LINEDROP:
            // Don't fail lineDrop due to timeouts
            SetEvent(pLineDev->hCallComplete);
            // FALLTHROUGH
        default:
            bAbort = FALSE;
            break;
        }

        // We set an event when we are done so that DevLineDrop can
        // continue with his cleanup.
        if (bAbort) {
            SetEvent(pLineDev->hCallComplete);
            SetAsyncStatus(pLineDev, LINEERR_OPERATIONFAILED);
        }
    }
#ifdef DEBUG
    else {
        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:ModemWatchdog(%d) cancelled at %d\r\n"), dwThisWatchdog, GetTickCount()));
    }
#endif

    // This is a one shot timeout.  The thread should go away as
    // soon as the timer expires or is signalled/cancelled.
}

// **********************************************************************
// Wrapper function to start the watchdog.
// Note that a timeout of 0 cancels the current watchdog.
// **********************************************************************
DWORD
SetWatchdog(
    PTLINEDEV pLineDev,
    DWORD dwTimeout
    )
{
    HANDLE hThread;
    PUNIMODEM_WATCHDOG_INFO pWDInfo;

    pLineDev->dwCurWatchdog++;
    pLineDev->bWatchdogTimedOut = FALSE;

    // First, lets cancel any current outstanding watchdogs for this line
    PulseEvent(pLineDev->hTimeoutEvent);  // Stop the watchdog

    // And now, start a new watchdog if they want us to 
    if( dwTimeout ) {
        pWDInfo = TSPIAlloc(sizeof(UNIMODEM_WATCHDOG_INFO));
        if (NULL == pWDInfo) {
            NKDbgPrintfW( TEXT("UNIMODEM:SetWatchDog Unable to alloc Watchdog info\n") );
            return (DWORD)-1;
        }

        pWDInfo->pLineDev       = pLineDev;
        pWDInfo->dwTimeout      = dwTimeout;
        pWDInfo->dwThisWatchdog = pLineDev->dwCurWatchdog;

        hThread = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)ModemWatchdog,
                                pWDInfo, 0, NULL );
        if ( ! hThread ) {
            TSPIFree(pWDInfo);
            NKDbgPrintfW( TEXT("UNIMODEM:SetWatchDog Unable to Create Watchdog Thread\n") );
            return (DWORD)-1;
        }
        CeSetThreadPriority(hThread, g_dwUnimodemThreadPriority - 1);
        CloseHandle( hThread );
    }
    return STATUS_SUCCESS;
}

#define INIT_WATCHDOG_TIMEOUT 4000

BOOL
ModemInit(
    PTLINEDEV pLineDev
    )
{
    MODEMRESPCODES MdmResp = MODEM_FAILURE;
    DWORD dwSize, dwCmdLen;
    int   RetryCount = 0;
    PWCHAR pszTemp = NULL;
    PWCHAR pszExpand = NULL;
    PCHAR pchCmd = NULL;
    MODEMMACRO ModemMacro;
    DWORD dwCallSetupFailTimer;
    DEVSTATES OrigDevState;
    BOOL bFailed;
    BOOL bEscapeSent;   // only send "+++" once.
    MDMSTATE NewMdmState;
    DWORD rc;
    UINT MaxCmd_Plus_1 = 0; 
    UINT TotalStringAllocSize = 0;
    UCHAR EscapeCmd[MAX_CMD_LENGTH];
    UCHAR ResetCmd[MAX_CMD_LENGTH];

    static const WCHAR szFlowHard[] =  TEXT("FlowHard");
    static const WCHAR szFlowSoft[] =  TEXT("FlowSoft");
    static const WCHAR szFlowNone[] =  TEXT("FlowOff");
    static const WCHAR *szFlowType;  // Hard, Soft, None
    static const WCHAR szCallFail[] =   TEXT("CallSetupFailTimeout");

    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+ModemInit\r\n")));

    //
    // Skip modem initialization if requested
    //
    dwSize = sizeof(DWORD);
    rc = MdmRegGetValue( pLineDev,
                         szInit,
                         szSkipInit,
                         REG_DWORD,
                         (PUCHAR)&dwCmdLen,
                         &dwSize);
    if (ERROR_SUCCESS == rc) {
        return TRUE;
    }


    EnterCriticalSection(&pLineDev->OpenCS);
    if (bFailed = (pLineDev->MdmState == MDMST_INITIALIZING)) {
        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:ModemInit Already MDMST_INITIALIZING!\r\n")));
    } else {
        pLineDev->MdmState = MDMST_INITIALIZING;
        OrigDevState = pLineDev->DevState;
    }
    LeaveCriticalSection(&pLineDev->OpenCS);

    if (bFailed) {
        return FALSE;
    }

    bFailed = TRUE;
    bEscapeSent = FALSE;
    NewMdmState = MDMST_UNKNOWN;

    try
    {

        // Allocate temp buffers for registry results
        if(STATUS_SUCCESS != RtlUIntAdd(pLineDev->dwMaxCmd, 1, &MaxCmd_Plus_1) ||
           STATUS_SUCCESS != RtlUIntMult(MaxCmd_Plus_1, SZWCHAR, &TotalStringAllocSize))
        {
            goto init_error;
        }
        
        pszTemp = (PWCHAR)TSPIAlloc( TotalStringAllocSize );
        pszExpand = (PWCHAR)TSPIAlloc( TotalStringAllocSize );
        pchCmd =  (PCHAR)TSPIAlloc( MaxCmd_Plus_1 );
        if (!(pszTemp && pszExpand && pchCmd))
        {
            goto init_error;
        }
    
        //
        // First, read the reset and escape commands from the registry
        //
        GetHangupCommands(pLineDev, EscapeCmd, NULL, ResetCmd);
        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:ModemInit Reset string is \"%s\"\r\n"), ResetCmd));
    
        // ***** reset modem by clearing DTR    
        if(LineNotDropped(pLineDev, OrigDevState))
        {
            DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:ModemInit EscapeCommFunction - DTR off\r\n")));
            EscapeCommFunction ( pLineDev->hDevice, CLRDTR);
            Sleep( 400 );
        }
    
    
        if(LineNotDropped(pLineDev, OrigDevState))
        {
            DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:ModemInit EscapeCommFunction - DTR on\r\n")));
            EscapeCommFunction ( pLineDev->hDevice, SETDTR);
            Sleep( 200 );
        }
    
        while( (LineNotDropped(pLineDev, OrigDevState)) &&
                 (RetryCount < MAX_COMMAND_TRIES) )
        {
            // Set short timeout for init strings
            if( SetWatchdog( pLineDev, INIT_WATCHDOG_TIMEOUT ) )
            {
                goto init_error;
            }
        
            if( LineNotDropped(pLineDev, OrigDevState) )
            {
                DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:ModemInit Send Init\r\n")));
                MdmResp = MODEM_FAILURE;
                if (MdmSendCommand( pLineDev, ResetCmd )) {
                    MdmResp = MdmGetResponse( pLineDev, ResetCmd, NOCS, OPENED);
                }
            }
        
            // See if we managed to get initialized
            if( MODEM_SUCCESS == MdmResp )
            {
                NewMdmState = MDMST_DISCONNECTED;  // It worked
                break;
            }
            else
            {
                if (bEscapeSent) {
                    //
                    // Avoid trying "+++" 3 times. If there is no modem on the port
                    // we'd take over 15 seconds to fail. If there is a modem, one "+++"
                    // ought to be enough.
                    //
                    goto init_error;
                } else {
                    bEscapeSent = TRUE;
                    if( LineNotDropped(pLineDev, OrigDevState) )
                    {
                        // the modem didn't reset
                        NewMdmState = MDMST_UNKNOWN;

                        // If Reset didn't work, we likely need a +++
                        Sleep(pLineDev->EscapeDelay);
                        MdmSendCommand( pLineDev, EscapeCmd );
                        Sleep(pLineDev->EscapeWait);
                    }
                }
            }
            RetryCount++;
        }

        if( (MDMST_DISCONNECTED == NewMdmState) &&
            (LineNotDropped(pLineDev, OrigDevState)) )
        {
            DWORD mdmRegRslt = ERROR_SUCCESS;
            WORD wInitKey;

            // read a sequence of additional init strings from the
            // registry and send them down now. Can't enumerate the 
            // keys since that wouldn't allow us to have modem specific
            // values mixed with default values.
            wInitKey = 0;
            while( (wInitKey < MAXINITKEYS) &&
                     (LineNotDropped(pLineDev, OrigDevState)) &&
                     (ERROR_SUCCESS == mdmRegRslt) )
            {
                dwSize = pLineDev->dwMaxCmd * SZWCHAR;
                mdmRegRslt = MdmRegGetValue( pLineDev,
                                             szInit,
                                             szInitNum[wInitKey],
                                             REG_SZ,
                                             (PUCHAR)pszTemp,
                                             &dwSize);

                if (ERROR_SUCCESS == mdmRegRslt)
                {
                    MdmConvertCommand(pszTemp, pchCmd, &dwCmdLen);
                    DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:ModemInit Init string %d - %s\r\n"),
                                          wInitKey, pszTemp));

                    // Set short timeout for init strings
                    if( SetWatchdog( pLineDev, INIT_WATCHDOG_TIMEOUT ) )
                    {
                        goto init_error;
                    }
                
                    if (LineNotDropped(pLineDev, OrigDevState)) {
                        // We got a value, write it out to modem
                        MdmResp = MODEM_FAILURE;
                        if (MdmSendCommand( pLineDev, pchCmd )) {
                            if (LineNotDropped(pLineDev, OrigDevState)) {
                                MdmResp = MdmGetResponse( pLineDev, pchCmd, NOCS, OPENED);
                            }
                        }
                    }
                    if( MODEM_SUCCESS != MdmResp )
                    {
                        // bad init string.  We can't recover from this so exit
                        goto init_error;
                    }
                }
                else
                {
                    DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:ModemInit Error x%X reading Init string %d\r\n"),
                                          mdmRegRslt, wInitKey));
                }
            
                wInitKey++;
            }

            // And now that we have the basic init in place, we still need to
            // make sure the flow control settings match what we are doing in
            // the UART.
            if( MDM_FLOWCONTROL_HARD & pLineDev->DevMiniCfg.dwModemOptions )
            {  // Set Modem for hardware flow control
                szFlowType = szFlowHard;
            }
            else  if( MDM_FLOWCONTROL_SOFT & pLineDev->DevMiniCfg.dwModemOptions )
            {  // Set Modem for software flow control
                szFlowType = szFlowSoft;            
            }
            else
            {  // Set modem for no flow control
                szFlowType = szFlowNone;            
            }
            dwSize = pLineDev->dwMaxCmd * SZWCHAR;
            if( ERROR_SUCCESS == MdmRegGetValue( pLineDev,
                                                 szSettings,
                                                 szFlowType,
                                                 REG_SZ,
                                                 (PUCHAR)pszTemp,
                                                 &dwSize) )
            {
                // OK, we got the flow control command, send it.
                MdmConvertCommand(pszTemp, pchCmd, &dwCmdLen);
                DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:ModemInit Flow Control string %s\r\n"),
                                      pszTemp));
            
                if (LineNotDropped(pLineDev, OrigDevState))
                {
                    // We got a value, write it out to modem
                    MdmResp = MODEM_FAILURE;
                    if (MdmSendCommand( pLineDev, pchCmd )) {
                        if (LineNotDropped(pLineDev, OrigDevState)) {
                            MdmResp = MdmGetResponse( pLineDev, pchCmd, NOCS, OPENED);
                        }
                    }
                }
                if( MODEM_SUCCESS != MdmResp )
                {
                    // bad flow control string.  We can't recover from this.
                    goto init_error;
                }
            }

            // Now we set up the call fail timer if it exists.
            dwSize = pLineDev->dwMaxCmd * SZWCHAR;
            if( ERROR_SUCCESS == MdmRegGetValue( pLineDev,
                                                 szSettings,
                                                 szCallFail,
                                                 REG_SZ,
                                                 (PUCHAR)pszTemp,
                                                 &dwSize) )
            {
                // OK, we got the Call Fail setup command, send it.

                // Most modems can't handle a value > 255 for call fail time
                // And a time of 0 usually really means wait the maximum
                dwCallSetupFailTimer = pLineDev->DevMiniCfg.dwCallSetupFailTimer;
                if( (dwCallSetupFailTimer > 255) ||
                    (dwCallSetupFailTimer == 0) )
                    dwCallSetupFailTimer = 255;
                
                // Expand macros, including the fail time <#>
                wcscpy(ModemMacro.MacroName, INTEGER_MACRO);
                StringCchPrintfW(ModemMacro.MacroValue, sizeof(ModemMacro.MacroValue)/sizeof(WCHAR), L"%d", dwCallSetupFailTimer);
                ExpandMacros(pszTemp, pszExpand, pLineDev->dwMaxCmd, &ModemMacro);
            
                // We need to convert the UniCode to ASCII
                dwCmdLen = wcslen(pszExpand) + 1;
                dwCmdLen = wcstombs( pchCmd, pszExpand, dwCmdLen );
                DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:ModemInit Call fail timer string %s\r\n"),
                                      pszTemp));
            
                if (LineNotDropped(pLineDev, OrigDevState))
                {
                    // We got a value, write it out to modem
                    MdmResp = MODEM_FAILURE;
                    if (MdmSendCommand( pLineDev, pchCmd )) {
                        if (LineNotDropped(pLineDev, OrigDevState)) {
                            MdmResp = MdmGetResponse( pLineDev, pchCmd, NOCS, OPENED);
                        }
                    }
                }
                if( MODEM_SUCCESS != MdmResp )
                {
                    // Hmm, we could probably ignore this failure, but the call might
                    // never time out.  Lets be safe and abort now.
                    goto init_error;
                }
            }

            // And finally, see if user requested any special modifier string
            if( dwCmdLen = wcslen(pLineDev->DevMiniCfg.szDialModifier) )
            {
                DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:ModemInit Dial Modifier string %s\r\n"),
                                      pLineDev->DevMiniCfg.szDialModifier));

                // Convert the UniCode to ASCII, and insert 'AT' in command
                pchCmd[0] = 'A';
                pchCmd[1] = 'T';
                
                dwCmdLen += 1;
                dwCmdLen = wcstombs( pchCmd+2, pLineDev->DevMiniCfg.szDialModifier, dwCmdLen );

                // OK, now don't forget the CR at end.
                strcat( pchCmd, "\r" );
                DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:ModemInit Generated Modifier string %a\r\n"),
                                      pchCmd));

                if (LineNotDropped(pLineDev, OrigDevState))
                {
                    // And write this out to modem
                    MdmResp = MODEM_FAILURE;
                    if (MdmSendCommand( pLineDev, pchCmd )) {
                        if (LineNotDropped(pLineDev, OrigDevState)) {
                            MdmResp = MdmGetResponse( pLineDev, pchCmd, NOCS, OPENED);
                        }
                    }
                }
                if( MODEM_SUCCESS != MdmResp )
                {
                    // bad modifier string.  We can't recover from this.
                    DEBUGMSG(ZONE_CALLS | ZONE_ERROR,
                             (TEXT("UNIMODEM:ModemInit Bad user supplied dial modifier %s\r\n"),
                              pLineDev->DevMiniCfg.szDialModifier));
                    goto init_error;
                }
            }
            
        }
        bFailed = FALSE;
    } except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
              EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        // Most likely a bad handle.  Just give up
        bFailed = TRUE;
    }
    
init_error:
    EnterCriticalSection(&pLineDev->OpenCS);
    if (bFailed) {
        pLineDev->MdmState = MDMST_UNKNOWN;
    } else {
        pLineDev->MdmState = NewMdmState;
    }
    LeaveCriticalSection(&pLineDev->OpenCS);

    // Cancel watchdog if he's still around.
    SetWatchdog( pLineDev, 0 );
    if( pszTemp )
        TSPIFree( pszTemp );        
    if( pchCmd )
        TSPIFree( pchCmd );
    if( pszExpand )
        TSPIFree( pszExpand );

    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM: -ModemInit %s\n"),
        (pLineDev->MdmState == MDMST_UNKNOWN) ? TEXT("FAILED"): TEXT("OK")));
    return (pLineDev->MdmState == MDMST_UNKNOWN) ? FALSE : TRUE;
}   // ModemInit


#define NUM_BAUD_RATES 9

DWORD
NextBaudRate(
    DWORD dwBaudRate
    )
{
    switch (dwBaudRate) {
    case CBR_19200:  return CBR_57600;
    case CBR_57600:  return CBR_115200;
    case CBR_115200: return CBR_38400;
    case CBR_38400:  return CBR_56000;
    case CBR_56000:  return CBR_128000;
    case CBR_128000: return CBR_256000;
    case CBR_256000: return CBR_9600;
    case CBR_9600:   return CBR_14400;
    case CBR_14400:  return CBR_19200;
    }
    return CBR_19200;
}

//
// Called to set the baud rate for a NULL MODEM device.
//
BOOL
AutoBaudRate(
    PTLINEDEV pLineDev,
    DWORD dwBaudRate
    )
{
    DCB commDCB;
    DWORD status;
    DWORD Retry;
    DWORD dwCurBaud;

    if (!GetCommState(pLineDev->hDevice, &commDCB)) {
        status = GetLastError();
        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:AutoBaudRate GetCommState failed %d\n"), status));
        return FALSE;
    }

    Retry = 0;

    if (dwBaudRate) {
        dwCurBaud = dwBaudRate;
    } else {
abr_next:
        dwCurBaud = NextBaudRate(pLineDev->dwCurrentBaudRate);
    }

    pLineDev->dwCurrentBaudRate = commDCB.BaudRate = dwCurBaud;

    if (!PurgeComm(pLineDev->hDevice, PURGE_TXCLEAR|PURGE_RXCLEAR)) {
        status = GetLastError();
        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:AutoBaudRate PurgeComm failed %d\r\n"), status));
    }
    if (!SetCommState(pLineDev->hDevice, &commDCB)) {
        status = GetLastError();
        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:AutoBaudRate SetCommState(baud = %d) failed %d\n"),
            commDCB.BaudRate, status));
        //
        // If SetCommState fails with inval parm, assume the error was an unsupported baud rate
        // and try the next one.
        //
        if (status == ERROR_INVALID_PARAMETER) {
            Retry++;
            if (Retry < NUM_BAUD_RATES) {
                goto abr_next;
            }
        }
        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:AutoBaudRate failed.\n")));
        return FALSE;
    }

    SetDialerTimeouts(pLineDev);

    DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:AutoBaudRate trying %d\n"), commDCB.BaudRate));
    return TRUE;
}   // AutoBaudRate


BOOL
CallNotDropped(
    PTLINEDEV pLineDev
    )
{
    DWORD ModemStatus;

    if (NULL == pLineDev->htLine) {
        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:CallNotDropped line closed\n")));
        return FALSE;
    }

    if (NULL == pLineDev->htCall) {
        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:CallNotDropped call closed\n")));
        return FALSE;
    }

    if (IS_NULL_MODEM(pLineDev)) {
        if (GetCommModemStatus(pLineDev->hDevice_r0, &ModemStatus)) {
            if (!(ModemStatus & MS_RLSD_ON)) {
                DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:CallNotDropped DCC disconnected\n")));
                return FALSE;
            }
        } else {
            DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:CallNotDropped DCC GetCommModemStatus failed\n")));
            return FALSE;
        }
    }

    switch (pLineDev->dwCallState) {
    case LINECALLSTATE_CONNECTED:
    case LINECALLSTATE_DIALING:
    case LINECALLSTATE_DIALTONE:
    case LINECALLSTATE_PROCEEDING:
    case LINECALLSTATE_UNKNOWN:
            return TRUE;
    }

    DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:CallNotDropped call state = %x\n"), pLineDev->dwCallState));
    return FALSE;
}   // CallNotDropped


#define DEFAULT_NULL_MODEM_RETRY    3
    
//
// Attempt DCC connection (send "CLIENT", expect "CLIENTSERVER" response)
// Called by Dialer()
//
// Return:  TRUE    => Connected
//          FALSE   => Not connected
//
BOOL
ConnectDCC(
    PTLINEDEV pLineDev,
    DWORD dwCallFailTimer
    )
{
    MODEMRESPCODES MdmResp;
    DWORD Retry;
    DWORD BaudRates;
    DWORD NumBaudRates;        
    DWORD MaxRetries = DEFAULT_NULL_MODEM_RETRY;
    DWORD dwSize;
    DWORD dwWatchDogTime;
    UCHAR szCmd[MAX_CMD_LENGTH];
    BOOL  bImmediateConnect;

    DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:ConnectDCC - Attempting DCC connection\r\n")));

    //
    //    Allow the default number of retries to be overridden by the
    //    line specific registry setting:
    //        HKLM\Drivers\Builtin\IrComm\Unimodem\Settings\DCCRetries (DWORD)
    //  or if a line specific setting is not present, then from the global default:
    //        HKLM\Drivers\Unimodem\Settings\DCCRetries (DWORD)
    //
    dwSize = sizeof( MaxRetries );
    (void)MdmRegGetValue(pLineDev, TEXT("Settings"), TEXT("DCCRetries"), REG_DWORD, (LPBYTE)&MaxRetries, &dwSize);

    //
    // This is a DCC connection.  Dial strings are simple.
    // Send 'CLIENT', expect back 'CLIENTSERVER'. Try several
    // times, unless we are dropping the line
    //
    GetModemCommand(pLineDev, L"DCCRequest", szCmd, (LPSTR)pchDCCCmd);
    bImmediateConnect = (strlen(szCmd) == 0) ? TRUE : FALSE;

    if (pLineDev->DevMiniCfg.dwModemOptions & MDM_SPEED_ADJUST) {
        NumBaudRates = NUM_BAUD_RATES;
    } else {
        //
        // Automatic baud rate detection is off, just use the supplied baud rate
        //
        NumBaudRates = 1;
        pLineDev->dwLastBaudRate = 0;
    }

    if (!AutoBaudRate(
            pLineDev, 
            (pLineDev->dwLastBaudRate) ?
                    pLineDev->dwLastBaudRate : pLineDev->DevMiniCfg.dwBaudRate)) {
        goto cd_done;
    }

    for (BaudRates=0; BaudRates < NumBaudRates; BaudRates++) {
        for( Retry=0; Retry <= MaxRetries; Retry++) {
            if (bImmediateConnect) {
                MdmResp = MODEM_CONNECT;    // No command strings
            } else {
                //
                // Start the watchdog thread before we block in a read.
                // Wait only one second on the first try because NT RAS
                // server doesn't answer on the first "ring"
                //
                dwWatchDogTime = dwCallFailTimer * 1000;
                if ((0 == Retry) && (DT_IRCOMM_MODEM != pLineDev->wDeviceType)) {
                    dwWatchDogTime = 1000;
                }

                if (SetWatchdog( pLineDev, dwWatchDogTime)) {
                    goto cd_done;
                }

                NewCallState(pLineDev, LINECALLSTATE_DIALING, 0L);

                // Send out CLIENT
                if (MdmSendCommand( pLineDev, szCmd )) {
                    // Now look for CLIENTSERVER
                    do
                    {
                        MdmResp = MdmGetResponse( pLineDev, (PUCHAR)szCmd, NOCS, OPENED);
                    } while ( (MODEM_FAILURE != MdmResp) &&
                              (MODEM_EXIT != MdmResp) &&
                              (MODEM_CONNECT != MdmResp) &&
                              CallNotDropped(pLineDev) );

                } else {
                    MdmResp = MODEM_FAILURE;
                }
            }

            if( MODEM_CONNECT == MdmResp )
            {
                // Cool, we are connected
                pLineDev->dwLastBaudRate = pLineDev->dwCurrentBaudRate;
                pLineDev->dwCallFlags |= CALL_ACTIVE;
                return TRUE;
            }

            // We timed out on a connect.  Reset the watchdog and try again
            SetWatchdog( pLineDev, 0 );

            //
            // Check if the operation got cancelled.
            //
            if (pLineDev->dwPendingType != PENDING_LINEMAKECALL) {
                goto cd_done;
            }

            //
            // Check if the cable is connected.
            //
            if (FALSE == CallNotDropped(pLineDev)) {
                goto cd_done;
            }
        }

        if (BaudRates == 0) {
            //
            // Make sure the baud rate is within autobaud's range
            //
            pLineDev->dwCurrentBaudRate = NextBaudRate(0);
        }

        if (!AutoBaudRate(pLineDev, 0)) {
            goto cd_done;
        }
    }

cd_done:
    pLineDev->dwLastBaudRate = 0;
    return FALSE;

}   // ConnectDCC


// **********************************************************************
//
// This is the dialer thread.  It is created whenever we decide to
// dial a modem, and exists until the call completes or is terminated.
// 
// **********************************************************************
void
Dialer(
    PTLINEDEV pLineDev
    )
{
    MODEMRESPCODES MdmResp;
    BOOL fOriginate;
    BOOL bFirstResp;
    PWCHAR pszzDialCommands = NULL;  // strings of dial strings returned from CreateDialCommand
    PWCHAR ptchDialCmd;
    PCHAR  pchDialCmd = NULL;
    DWORD  dwDialLen;
    DWORD  LineCallState;
    DWORD  LineCallStateParm2;
    DWORD  LineError;   // Indicates asynchronous return code for LineMakeCall completion
    DWORD  dwCallFailTimer;
    BOOL bKeepLooping;
    BOOL bClose;
    UINT MaxCmd_Plus_4 = 0;
    
    DEBUGMSG(ZONE_FUNC|ZONE_CALLS,
             (TEXT("UNIMODEM:Dialer - dial options x%X (x%X), modem options x%X\r\n"),
              pLineDev->dwDialOptions, pLineDev->DevMiniCfg.fwOptions, pLineDev->DevMiniCfg.dwModemOptions));

    dwCallFailTimer = pLineDev->DevMiniCfg.dwCallSetupFailTimer;
    LineCallStateParm2 = LINEDISCONNECTMODE_NORMAL;
    LineError = LINEERR_OPERATIONFAILED;
    bFirstResp = TRUE;
    bClose = FALSE;

    // NOTENOTE : A fail time of 0 means don't time out.  But I know better than that.
    // Most modems can't wait longer than 255 seconds anyway, so there's no point
    // in having us wait much longer than that either.
    if( (dwCallFailTimer == 0) ||
        (dwCallFailTimer > 512) )
        dwCallFailTimer = 512;
    
    // Clear any old CallComplete events before starting a new call
    WaitForSingleObject(pLineDev->hCallComplete, 0);

    MdmResetIdleTimer();

    try
    {
        // Our job is to dial the modem, and then sit here waiting for
        // a response.  All the helper routines in this file are responsible
        // for driving the state machine.

        if (pLineDev->dwPendingType == PENDING_LINEDIAL) {
            //
            // If we are here because of a lineDial call, then skip the NULL modem
            // and manual dial stuff
            //
            goto dial_continue;
        }

        if( IS_NULL_MODEM(pLineDev) )
        {
            if (ConnectDCC(pLineDev, dwCallFailTimer)) {
                LineError = 0;
                goto dial_connected;
            }
            LineCallStateParm2 = LINEDISCONNECTMODE_UNKNOWN;
            goto dial_done;
        }
#ifdef USE_TERMINAL_WINDOW  // removing dial terminal window feature
        else if (pLineDev->DevMiniCfg.fwOptions & MANUAL_DIAL)
        {
            if (DisplayTerminalWindow(pLineDev, IDS_MANUAL_DIAL_TERMINAL)) {
                // The user pressed OK, so he must be connected.
                LineError = 0;
                pLineDev->dwCallFlags |= CALL_ACTIVE;
                goto dial_connected;
            }
            goto dial_done;
        }
#endif
        else  
        {
            // Its not a NULL modem, so send actual dial strings
            DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:Dialer - Attempting Modem connection\r\n")));
            
            // If modeminit fails, we sure won't be able to dial
            if ((!ModemInit(pLineDev)) || (MDMST_UNKNOWN == pLineDev->MdmState))
            {
                DEBUGMSG(ZONE_CALLS,
                         (TEXT("UNIMODEM:Dialer - Modem Reset failed, aborting call\r\n")));
                LineCallStateParm2 = LINEDISCONNECTMODE_UNAVAIL;
                goto dial_done;
            }
            
#ifdef USE_TERMINAL_WINDOW  // removing dial terminal window feature
            // Are we supposed to do a pre-dial terminal???
            if ( (pLineDev->DevMiniCfg.fwOptions & TERMINAL_PRE) &&
                 CallNotDropped(pLineDev) )
            {
                DEBUGMSG(ZONE_CALLS,
                         (TEXT("UNIMODEM:Dialer - PreDial - Modem State x%X\r\n"),
                          pLineDev->MdmState ));
                if (!DisplayTerminalWindow(pLineDev, IDS_PRE_DIAL_TERMINAL))
                {
                    // Either the user cancelled or something went wrong.
                    goto dial_done;
                }    
            }
#endif

dial_continue:       
            // Convert the dial string to a modem format.
            DEBUGMSG(ZONE_MISC, (TEXT("UNIMODEM:Dialer - Ready to create Dial Strings\r\n")));
            pszzDialCommands = CreateDialCommands( pLineDev, &fOriginate );
            if (!pszzDialCommands) {
                goto dial_done;
            }

            ptchDialCmd = pszzDialCommands;

            // Dial strings get truncated to pLineDev->dwMaxCmd so
            // allocate a buffer large enough to store that.

            if(STATUS_SUCCESS != RtlUIntAdd(pLineDev->dwMaxCmd, 4, &MaxCmd_Plus_4)){
                goto dial_done;
            }

            pchDialCmd = TSPIAlloc( MaxCmd_Plus_4 );
            if (!pchDialCmd) {
                goto dial_done;
            }
    
            // Start the watchdog thread before we block in a read
            if( SetWatchdog( pLineDev, dwCallFailTimer * 1000 ) )
            {
                LineCallStateParm2 = LINEDISCONNECTMODE_UNKNOWN;
                goto dial_done;
            }

            // Remember, this is a string of dial strings.  We need to iterate
            // through each of them and handle the responses.
            while (CallNotDropped(pLineDev) && *ptchDialCmd)
            {
                // We need to convert the UniCode to ASCII
                dwDialLen = wcslen(ptchDialCmd) + 1;
                DEBUGMSG(ZONE_MISC,
                         (TEXT("UNIMODEM:Dialer - Sending Dial string of len %d - %s\r\n"),
                          dwDialLen, ptchDialCmd));
                
                dwDialLen = wcstombs( pchDialCmd, ptchDialCmd, dwDialLen );

                DEBUGMSG(ZONE_CALLS,
                         (TEXT("UNIMODEM:Dialer - Advancing ptchDialCmd to x%X\r\n"),
                          ptchDialCmd));
                ptchDialCmd += wcslen(ptchDialCmd) + 1;
                        
                NewCallState(pLineDev, LINECALLSTATE_DIALING, 0L);

                // Send the command and wait for a response
                MdmSendCommand( pLineDev, pchDialCmd );

                // Loop looking for a failure or success response
                bKeepLooping = TRUE;
                while ((CallNotDropped(pLineDev)) && (bKeepLooping)) {
                    MdmResp = MdmGetResponse( pLineDev, pchDialCmd, NOCS, OPENED);
                    bKeepLooping = FALSE;

                    if (pLineDev->dwPendingType != PENDING_LINEDIAL) {
                        if (bFirstResp) {
                            bFirstResp = FALSE;
                            switch (MdmResp) {
                            case MODEM_CARRIER:
                            case MODEM_CONNECT:
                            case MODEM_PROGRESS:
                            case MODEM_PROTOCOL:
                            case MODEM_SUCCESS:
                                //
                                // BUGBUG : This seems hokey, but Win98 and NT do this - they send a LINECALLSTATE_DIALTONE
                                // immediately after completing the lineMakeCall request without looking at any
                                // dialtone status on the modem.
                                //
                                NewCallState(pLineDev, LINECALLSTATE_DIALTONE, LINEDIALTONEMODE_UNAVAIL);
    
                                break;
                            }
                        }
                    }

                    LineError = LINEERR_OPERATIONFAILED;

                    switch (MdmResp) {
                    case MODEM_CONNECT:
                        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:Dialer - Dial succeeded\n")));
                        pLineDev->dwCallFlags |= CALL_ACTIVE;
                        LineError = 0;
                        break;

                    case MODEM_BUSY:
                        // If it was busy, let application know about it
                        NewCallState(pLineDev, LINECALLSTATE_BUSY, LINEDISCONNECTMODE_BUSY);
                        LineCallStateParm2 = LINEDISCONNECTMODE_BUSY;
                        DEBUGMSG(ZONE_CALLS,(TEXT("UNIMODEM:Dialer - BUSY\r\n")));
                        goto dial_done;

                    case MODEM_NODIALTONE:
                        LineCallStateParm2 = LINEDISCONNECTMODE_NODIALTONE;
                        DEBUGMSG(ZONE_CALLS,(TEXT("UNIMODEM:Dialer - No Dial Tone\r\n")));
                        goto dial_done;

                    case MODEM_NOANSWER:
                        LineCallStateParm2 = LINEDISCONNECTMODE_NOANSWER;
                        DEBUGMSG(ZONE_CALLS,(TEXT("UNIMODEM:Dialer - No Answer\r\n")));
                        goto dial_done;
                    
                    case MODEM_HANGUP:
                        LineCallStateParm2 = LINEDISCONNECTMODE_NORMAL;
                        DEBUGMSG(ZONE_CALLS,(TEXT("UNIMODEM:Dialer - HANGUP\r\n")));
                        goto dial_done;
                    
                    case MODEM_FAILURE:
                    case MODEM_EXIT:
                        LineCallStateParm2 = LINEDISCONNECTMODE_UNKNOWN;
                        DEBUGMSG(ZONE_CALLS,(TEXT("UNIMODEM:Dialer - UNKNOWN Disconnect mode\r\n")));
                        goto dial_done;
                    
                    case MODEM_SUCCESS:
                        //
                        // Some modems respond "OK" to "ATDT123456" and then respond "CONNECT"
                        //
                        if ((*ptchDialCmd == 0) && (0 == strncmp("ATDT", pchDialCmd, 4))) {
                            bKeepLooping = TRUE;
                            DEBUGMSG(ZONE_CALLS,(TEXT("UNIMODEM:Dialer - ATDT OK, wait for CONNECT\r\n")));
                        } else {
                            DEBUGMSG(ZONE_CALLS,(TEXT("UNIMODEM:Dialer - OK Continuing Dial Sequence\r\n")));
                        }
                        LineError = 1;
                        break;

                    case MODEM_PROGRESS:
                    case MODEM_CARRIER:
                        DEBUGMSG(ZONE_CALLS,(TEXT("UNIMODEM:Dialer - Continuing Dial Sequence\r\n")));
                        LineError = 1;
                        break;

                    case MODEM_ABORT:
                        LineCallStateParm2 = LINEDISCONNECTMODE_UNKNOWN;
                        DEBUGMSG(ZONE_CALLS,(TEXT("UNIMODEM:Dialer - ABORT\r\n")));
                        LineError = 2;
                        goto dial_done;

                    default:
                        DEBUGMSG(ZONE_CALLS,
                                 (TEXT("UNIMODEM:Dialer - Unknown response x%X\r\n"),
                                  MdmResp));
                        // Don't fail on unknown responses. Just look for a known good, a fail, or a timeout
                        bKeepLooping = TRUE;
                        break;
                    }
                }
            }   // while more dial strings
        }
    
dial_connected:
        SetWatchdog( pLineDev, 0 );  // Stop the watchdog

#ifdef USE_TERMINAL_WINDOW  // removing dial terminal window feature
        // Are we supposed to do a post-dial terminal???
        // LAM - Why isn't this inside of the NOT MANUAL DIAL section
        if ( (pLineDev->DevMiniCfg.fwOptions & TERMINAL_POST) &&
             CallNotDropped(pLineDev) )

        {
            DEBUGMSG(ZONE_CALLS,
                     (TEXT("UNIMODEM:Dialer - PostDial - Modem State x%X\r\n"),
                      pLineDev->MdmState ));
            if (!DisplayTerminalWindow(pLineDev, IDS_POST_DIAL_TERMINAL)) {
                // Either the user cancelled or something went wrong.
                goto dial_done;
            }
        }
#endif
    }
    except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
            EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        // Somebody must have hit a bad handle.  Fail this dial sequence
        goto dial_done;        
    }
    
        
dial_done:
    switch (LineError) {
    case 0:
        pLineDev->DevState = DEVST_CONNECTED;
        LineCallState = LINECALLSTATE_CONNECTED;
        LineCallStateParm2 = 0;
        break;

    case 1:
        pLineDev->DevState = DEVST_PORTCONNECTWAITFORLINEDIAL;
        LineCallState = LINECALLSTATE_DIALING;
        LineCallStateParm2 = 0;
        break;

    case 2:
        DEBUGMSG(1, (TEXT("UNIMODEM:Dialer *******ABORTING*****\n")));
        pLineDev->dwCallState = LINECALLSTATE_UNKNOWN;
        bClose = TRUE;
        //    FALL THROUGH

    default:
        pLineDev->DevState = DEVST_DISCONNECTED;
        LineCallState = LINECALLSTATE_DISCONNECTED;
        break;
    }

    SetWatchdog( pLineDev, 0 );  // Stop the watchdog
    
    if ((pLineDev->dwPendingType == PENDING_LINEMAKECALL) ||
        (pLineDev->dwPendingType == PENDING_LINEDIAL)) {
        DEBUGMSG(ZONE_CALLS,
                 (TEXT("UNIMODEM:Dialer - tCall Completion for ReqID x%X, Status x%X\r\n"),
                  pLineDev->dwPendingID, (LineError == 1) ? 0 : LineError));
        SetAsyncStatus(pLineDev, (LineError == 1) ? 0 : LineError);
    }

    NewCallState(pLineDev, LineCallState, LineCallStateParm2);

    if (!CallNotDropped(pLineDev)) {
        DEBUGMSG(ZONE_CALLS, (TEXT("UNIMODEM:Dialer **** Call State x%X, closing port\r\n"), LineCallState));
        bClose = TRUE;
    }

    // Free up anything we may have allocated in here.
    if( pchDialCmd )
        TSPIFree( pchDialCmd );
    if( pszzDialCommands )
        TSPIFree( pszzDialCommands );
    
    // We set an event when we are done so that DevLineDrop can
    // continue with his cleanup.
    SetEvent(pLineDev->hCallComplete);
    
    if (bClose) {
        DevlineClose(pLineDev, TRUE);
    }
    DEBUGMSG(ZONE_FUNC|ZONE_CALLS, (TEXT("UNIMODEM:-Dialer\r\n")));
}

