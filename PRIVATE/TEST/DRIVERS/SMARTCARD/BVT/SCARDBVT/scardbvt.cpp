//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++

Module Name:

    SCardBVT

Description:

    This file contains the implementation of build verification
    tests of the Calais Smart Card API suite.
    
Notes:

    None.
    

--*/


/////////////////////////////////////////////////////////////////////////////
//
// Includes
//

#include <windows.h>
#ifndef UNDER_CE
#include <stdio.h>
#endif
#include <tchar.h>
#include <string.h>

#include <winscard.h>
#ifdef UNDER_CE
#include <winsmcrd.h>
#endif
#include "tuxbvt.h"
#include "debug.h"
#include "SCardF.h"

/////////////////////////////////////////////////////////////////////////////
//
// Defines
//

// size of string buffers
#define BUFFER_SIZE 2048
// output line length for nice formatting
#define OUTPUT_LINE 65
// spacer characters for reporting
// same overall length as OUTPUT_LINE above
#define SPACER      TEXT(".................................................................")
#define BAR         TEXT("=================================================================")
// error code length for nice formatting
#define ERROR_LEN   12
// result length for nice formatting
#define RESULT_LEN   7
// pass/fail strings for reporting
#define SUCCESS     TEXT("SUCCESS")
#define FAILURE     TEXT("FAILURE")
// value for string comparisons
#define MATCH       0
#define MAX_READERS 4
/////////////////////////////////////////////////////////////////////////////
//
// Globals
//

// scope of application context regards Resource Manager
DWORD           g_dwScope              = SCARD_SCOPE_SYSTEM;
// mode in which application shares card reader
DWORD           g_dwShareMode           = SCARD_SHARE_EXCLUSIVE;//SCARD_SHARE_SHARED;
// handle to context
SCARDCONTEXT    g_hContext              = NULL;
// handle to card reader
SCARDHANDLE     g_hCard                 = NULL;
// protocol negotiated with card in reader
DWORD           g_dwActiveProtocol;
// protocol to attempt negotiation with reader
DWORD           dwPreferredProtocols    = SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1; 
// on disconnect, leave the card in current state
DWORD           dwDisposition           = SCARD_LEAVE_CARD;
// buffer for reader names
LPTSTR          mszReaders              = new TCHAR[BUFFER_SIZE];
// size of reader names buffer
DWORD           cmszReaders             = BUFFER_SIZE;
// buffer for one reader name
LPTSTR          szDeviceGnD            = new TCHAR[BUFFER_SIZE];
// buffer for one reader name
LPTSTR          szDeviceSicrypt        = new TCHAR[BUFFER_SIZE];
// buffer for card names
LPTSTR          mszCards                = new TCHAR[BUFFER_SIZE];
// size of card names buffer
DWORD           cmszCards               = BUFFER_SIZE;
// friendly name for card
LPTSTR          szCardName              = TEXT("Calais BVT Card");
// length of ATR 
DWORD           dwATRLen                = 24;
// flag for api call succeeded
BOOLEAN         g_fFailed               = FALSE;
// flag for verification
BOOLEAN         fVerified               = FALSE;
// return value of api call
LONG            lCallReturn             = -1;
// buffer to handle output
UCHAR           rgchBuffer[BUFFER_SIZE];
// size of response received
DWORD           cbRecvLength            = BUFFER_SIZE;
// file handle for output file
FILE            *outstream;

// value for length of ATRs
int     iATRLength              = 24;
// flag for GnD card found in a reader
BOOLEAN fMatch            = TRUE;

// identifying ATR of GnD card
BYTE    bGnDATR[]               = {0x3b, 0xbf, 0x18, 0x00, 0x80, 0x31, 0x70, 0x35,
                                   0x53, 0x54, 0x41, 0x52, 0x43, 0x4f, 0x53, 0x20,
                                   0x53, 0x32, 0x31, 0x20, 0x43, 0x90, 0x00, 0x9b};

// identifying ATR of Sicrypt card
BYTE    bSicryptATR[]           = {0x3b, 0xef, 0x00, 0x00, 0x81, 0x31, 0x20, 0x49,
                                   0x00, 0x5c, 0x50, 0x43, 0x54, 0x10, 0x27, 0xf8,
                                   0xd2, 0x76, 0x00, 0x00, 0x38, 0x33, 0x00, 0x4d};

// GnD command select file EFptsDataCheck
BYTE    bGnDSelectFileEFptsDataCheck[] = {0x00, 0xa4, 0x00, 0x00, 0x02, 0x00, 0x01};
// GnD command select file length
DWORD   cbSendLengthGnDSelectFile    = 7;
// buffer to handle output
UCHAR           l_rgchOutBuffer[BUFFER_SIZE];
// buffer to handle input
UCHAR           l_rgchInBuffer[BUFFER_SIZE];
// number of bytes to read/write
ULONG           l_uNumBytes = 256;
// number of bytes per block
ULONG           l_uBytesPerBlock = 64;
// counters
ULONG           l_uBlock;
ULONG           l_uIndex;



// Sicrypt command select file 05
BYTE    bSicryptSelectFile5[]   = {0x00, 0xa4, 0x08, 0x04, 0x04, 0x3e, 0x00, 0x00, 0x05};
// Sicrytp command select file 05 length
DWORD   cbSendLengthSicryptSelectFile5 = 9;
// Sicrypt command forced timeout (Sicrypt WHQL test card only!)
BYTE    bSicryptForceTimeout[]  = {0x00, 0xB0, 0x00, 0x00, 0xFE};
// Sicrypt command forced timeout length
DWORD   cbSendLengthSicryptForceTimeout = 5;


/////////////////////////////////////////////////////////////////////////////
//
// Helper functions
//

/*++

PrintMessage:

    Simple function to dump text to standard output.
    
    Arguments:

    lpszFormat - String to dump to standard output
    
--*/
#define PrintMessage TRACE
/*++

ReportResult:

    Outputs result of a test

    Arguments:

    szTest   - test name
    szResult - test result (usually success or fail )

--*/
void ReportResult( TCHAR *szTest, TCHAR *szResult )
{
    // Output: <item>,<string of spacer dots to fill line>,<value>
    g_pKato->Log( LOG_DETAIL,TEXT("%s:%s\n"), szTest,szResult);
}


/*++

ReportResult:

    Outputs result of a test

    Arguments:

    szTest   - test name
    szResult - test result (usually success or fail )
    lError   - error code

--*/
void ReportResult( TCHAR *szTest, TCHAR *szResult, LONG lError)
{

    // if failure, mark general failure flag
    if ( MATCH == _tcscmp(szResult, FAILURE) ) {
        g_fFailed = TRUE;
        g_pKato->Log( LOG_FAIL,TEXT("%s%:%lx..%s\n"), szTest, lError, szResult);
    }
    else
        g_pKato->Log( LOG_DETAIL,TEXT("%s%:%lx..%s\n"), szTest, lError, szResult);
}
// make an msz string given a null terminated string
// assumption that mszString passed in is a properly formed
// msz string, satisfies one of:
// (a) <null><null>               first character is null
// (b) abc<null>def<null><null>   trailing two char's are null
void MakeMSZ( LPTSTR mszString, LPTSTR szString )
{
    // make local copy of caller's mszString
    LPTSTR mszLocalNameArray = mszString;

    // check for first character null, if so, denotes
    // an empty msz string.  if not, run to end of string
    // and append given sz string to msz string
    while( *mszLocalNameArray != 0 )
    {
        while( *mszLocalNameArray != 0 )
            mszLocalNameArray++;
        //advance past null terminator for this part of msz string
        mszLocalNameArray++;
    }

    // copy all characters up to the null terminator
    while( *szString != 0 )
    {
        *mszLocalNameArray = *szString;
        mszLocalNameArray++;
        szString++;
    }
    // copy the null terminator
    *mszLocalNameArray = *szString;
    // complete the msz string with a second null terminator
    mszLocalNameArray++;
    *mszLocalNameArray = *szString;
}


/*++

GetLocalReader:

    Queries the Calais database for friendly (device) name of installed reader
    
    Arguments:

    none

--*/
BOOLEAN 
GetLocalReaders( )
{
    // locals
    LPTSTR  szTempDeviceName   = new TCHAR[BUFFER_SIZE];
    LPTSTR  szTempNames        = new TCHAR[BUFFER_SIZE];
    DWORD   cszTempNames    = BUFFER_SIZE;
    DWORD   dwNewState      = 0;
    BOOL fCompareG=TRUE;
    BOOL fCompareS=TRUE;

    // initialize buffers
    memset(szTempDeviceName, 0, BUFFER_SIZE*sizeof(TCHAR));
    memset(mszReaders, 0, BUFFER_SIZE*sizeof(TCHAR));
    cmszReaders = BUFFER_SIZE;
    memset(szDeviceGnD, 0, BUFFER_SIZE*sizeof(TCHAR));
    memset(szDeviceSicrypt, 0, BUFFER_SIZE*sizeof(TCHAR));

    // initial Introduce card.

    lCallReturn = SCardListReaders(NULL,        // no particular context
        NULL,                                   // all readers
        mszReaders,                             // buffer to receive reader names
        &cmszReaders);                          // size of buffer, size of returned multistring

    LPTSTR    pMszReaders=mszReaders;

    if ( SCARD_S_SUCCESS == lCallReturn ) {
        // readers may have been found
        if ( 0 == _tcslen(mszReaders) ) {
            // no readers listed, report and exit
            g_pKato->Log(LOG_DETAIL, TEXT("No Smart Card Readers Installed"));
        }
        else {
            ReportResult(TEXT("SCardEstablishContext"), SUCCESS);
            ReportResult(TEXT("SCardListReaders"), SUCCESS);

            SCardContainer cardArray;
            cardArray.AddedSCard(new AmmiSCard());
            cardArray.AddedSCard(new BullSCard());
            cardArray.AddedSCard(new GieseckeSCard()); 
            cardArray.AddedSCard(new IbmSCard()); 
            cardArray.AddedSCard(new SchlumbgrSCard()); 
            cardArray.AddedSCard(new SniSCard()); 
            cardArray.AddedSCard(new GieseckeSCardV2()); 
            // among readers listed, find the one with the card inserted
            while (*pMszReaders != 0) {
                _tcscpy(szTempDeviceName, pMszReaders); // first reader.
                g_pKato->Log(LOG_DETAIL,TEXT(" Found Reader %s\n"), szTempDeviceName);
                // SCardConnect
                // get a handle to the card in the reader
                g_hCard=NULL;
                DWORD   bcNewAtrLen     = SCARD_ATR_LENGTH;

                lCallReturn = SCardConnect( g_hContext, 
                                            szTempDeviceName, 
                                            g_dwShareMode,
                                            dwPreferredProtocols, 
                                            &g_hCard, 
                                            &g_dwActiveProtocol);
                
                if ( SCARD_S_SUCCESS == lCallReturn ) {
                    // verify success
                    if ( g_hCard != NULL ) {
                        // a valid card handle was obtained
                        // test for which card is in reader
                         lCallReturn = SCardStatus(g_hCard,              // handle for card, reader
                                                  szTempNames,          // buffer for reader name(s)
                                                  &cszTempNames,        // size of buffer
                                                  &dwNewState,          // state of reader
                                                  &g_dwActiveProtocol,  // current protocol
                                                  (LPBYTE)&rgchBuffer,  // buffer for card ATR
                                                  &bcNewAtrLen);        // length of ATR in buffer

                        switch ( lCallReturn )
                        {
                        case SCARD_S_SUCCESS: {
                            SCardFunction *oneCard=cardArray.FoundCardByAtr(rgchBuffer);
                            if (oneCard==NULL) { // no test card found
                                g_pKato->Log( LOG_FAIL,TEXT("SCardStatus(ReaderName=%s,NewState=0x%lx,AtrReturnLen=0x%lx,ActiveProtocol=0x%lx)\n"),
                                    szTempNames,dwNewState,bcNewAtrLen,g_dwActiveProtocol);
                                TCHAR ATRBuffer[0x50];
                                if (sprintATR(ATRBuffer,0x50,rgchBuffer,bcNewAtrLen)) {
                                    g_pKato->Log( LOG_FAIL,TEXT("SCardStatus(ReaderName=%s,Atr=%s)\n"),
                                        szTempNames,ATRBuffer);
                                }
                                ReportResult(TEXT("Test card not found"), FAILURE);
                                g_fFailed=TRUE;
                            }
                                              };
                            break;
                        default:
                            ReportResult(TEXT(" SCardStatus "), FAILURE,lCallReturn);
                            break;
                        } // end switch
                        SCardDisconnect(g_hCard, SCARD_LEAVE_CARD);

                    } // end if g_hCard == NULL
                }  // end if SCardConnect SUCCESS
                else {
                    g_pKato->Log(LOG_DETAIL,TEXT(" Can not connect %s\n"), szTempDeviceName);
                }

                // reset buffers
                memset(rgchBuffer, 0, BUFFER_SIZE);
                memset(szTempNames, 0, BUFFER_SIZE*sizeof(TCHAR));
                cszTempNames = BUFFER_SIZE;

                // reset atr compare flags
                fCompareG = TRUE;
                fCompareS = TRUE;

                // advance to next name
                while (*pMszReaders != 0) {
                    pMszReaders++;
                }
                pMszReaders++;

            }  // end while
            
        } // end mszReaders not zero-length
    } // end list readers
    else {
        ReportResult(TEXT("SCardEstablishContext"), SUCCESS);
        ReportResult(TEXT("SCardListReaders"), FAILURE, lCallReturn);
    }

    delete[] szTempNames;
    delete[] szTempDeviceName;
    

    return !g_fFailed;
}    


/*++

VerifyMSZ:
  
    Checks existence of input string in multistring
    
    Arguments:
      
    szInputString   - string to be checked for
    mszString       - multistring to check
        
--*/
BOOLEAN 
VerifyMSZ( LPTSTR szInput, LPTSTR mszVerify )
{
    // set default to failure, change when match found
    BOOLEAN fMatched = FALSE;
    
    // continue until second null terminator is reached
    while (*mszVerify != 0)
    {
        if ( MATCH == _tcsncmp(szInput, mszVerify, _tcslen(szInput)) )
            // match found
            fMatched = TRUE;

        while (*mszVerify != 0)
        {
            mszVerify++;
        }
        mszVerify++;
    }
    
    return fMatched;
}

/*++

  VerifyGUID:
  
    Compares two GUIDs
    
    Arguments:
      
    guidInput       - guid against which to check
    guidVerify      - guid to be checked
        
--*/
BOOLEAN
VerifyGUID( GUID guidInput, GUID guidVerify )
{
    // set default to failure, change when match found
    BOOLEAN fMatched = FALSE;
    
    // compare Data1 fields
    if (( guidInput.Data1 == guidVerify.Data1 ) &&
        ( guidInput.Data2 == guidVerify.Data2 ) &&
        ( guidInput.Data3 == guidVerify.Data3 ) &&
        ( guidInput.Data4[0] == guidVerify.Data4[0] ) &&
        ( guidInput.Data4[1] == guidVerify.Data4[1] ) &&
        ( guidInput.Data4[2] == guidVerify.Data4[2] ) &&
        ( guidInput.Data4[3] == guidVerify.Data4[3] ) &&
        ( guidInput.Data4[4] == guidVerify.Data4[4] ) &&
        ( guidInput.Data4[5] == guidVerify.Data4[5] ) &&
        ( guidInput.Data4[6] == guidVerify.Data4[6] ) &&
        ( guidInput.Data4[7] == guidVerify.Data4[7] ))
    {
        fMatched = TRUE;
    }
    
    return fMatched;
}

/*++

CleanUp:

    Closes Transaction, Connection, Context

Arguments:

    none

--*/

void 
CleanUp()
{
    
    // Disconnect from reader
    SCardDisconnect(g_hCard, dwDisposition);
       
    // Release Smartcard Context
    SCardReleaseContext(g_hContext);

    // print test outcome
    g_pKato->Log(LOG_DETAIL, TEXT("%.*s\n"), OUTPUT_LINE, BAR);
    ReportResult(TEXT("Smart Card BVT"), g_fFailed == TRUE ? FAILURE : SUCCESS);
    g_pKato->Log(LOG_DETAIL, TEXT("%.*s\n\n"), OUTPUT_LINE, BAR);
}

BOOL SCardTransmitCheck(SCardFunction * oneCard, LPCTSTR readerName)
{
        // GnD card tests
    lCallReturn = SCardConnect( g_hContext, readerName, 
                    g_dwShareMode,
                    SCARD_PROTOCOL_T0|SCARD_PROTOCOL_T1, 
                    &g_hCard, 
                    &g_dwActiveProtocol);
            
    if (lCallReturn==SCARD_E_READER_UNAVAILABLE ||
        lCallReturn==SCARD_E_SHARING_VIOLATION) {
        g_pKato->Log( LOG_SKIP,TEXT("SCardConnect return Unavailable for reader %s! Test Skipped"),readerName);
        return TRUE;        
    };
    if ( SCARD_S_SUCCESS == lCallReturn ) {
        g_pKato->Log( LOG_DETAIL,TEXT("%s SCardConnect Protocol(%s) %s"),
            oneCard->GetSCardProvider(),
            (SCARD_PROTOCOL_T0 ==g_dwActiveProtocol?TEXT("T0"):
            (SCARD_PROTOCOL_T1 ==g_dwActiveProtocol?TEXT("T1"):TEXT("Raw"))),
            SUCCESS);

        BOOL bCheck=FALSE;
                // select file EFptsDataCheck
        lCallReturn=oneCard->SCardCheckTransmit(g_hCard,
                (SCARD_PROTOCOL_T0 == g_dwActiveProtocol?SCARD_PCI_T0:(SCARD_PROTOCOL_T1 == g_dwActiveProtocol?SCARD_PCI_T1:SCARD_PCI_RAW)),
                &bCheck);
        if ( SCARD_S_SUCCESS == lCallReturn ) {
                    // check return buffer for driver reports success
            if (!bCheck){
                ReportResult(TEXT("Card Transmit Check"), FAILURE);
                g_fFailed=TRUE;
            }
            else
                ReportResult(TEXT("Card Transmit Check"), SUCCESS);
        }
        else
             // report failure to complete scardtransmit
            ReportResult(TEXT("SCardTransmitCheck"), FAILURE, lCallReturn);
                    // disconnect
        lCallReturn = SCardDisconnect(g_hCard, 
                SCARD_LEAVE_CARD);
            
        if ( SCARD_S_SUCCESS == lCallReturn ) {
            ReportResult(TEXT("Sicrypt SCardDisconnect"), SUCCESS);
        }
        else {
            ReportResult(TEXT("Sicrypt SCardDisconnect"), FAILURE, lCallReturn);
        }

    }
    else {
        g_pKato->Log( LOG_FAIL,TEXT(" SCardConnect return 0x%lx atLocate Reader %s"),lCallReturn,readerName);
        g_fFailed=TRUE;
    }
    return !g_fFailed;
};

TESTPROCAPI SCardBVT(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) 
{

    // tux locals
    // Check our message value to see why we have been called.
    if (uMsg == TPM_QUERY_THREAD_COUNT) {
      ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = lpFTE->dwUserData;
      return SPR_HANDLED;
    } else 
    if (uMsg != TPM_EXECUTE) {
      return TPR_NOT_HANDLED;
    }

    // locals
    // success/error return

    //  Check number and type of arguments, and return usage prompt if bad
    //
    // BEGIN TESTS
    //
    // start test header
    g_pKato->Log(LOG_DETAIL, TEXT("\n\n%.*s\n"), OUTPUT_LINE, BAR);
    g_pKato->Log(LOG_DETAIL, TEXT("Smart Card BVT Test\n"));
    g_pKato->Log(LOG_DETAIL, TEXT("%.*s\n"), OUTPUT_LINE, BAR);

    // establish context with Resource Manager
    // Initialize context variable
    g_hContext = NULL;
    
    lCallReturn = SCardEstablishContext(g_dwScope, 
                                        NULL,
                                        NULL,
                                        &g_hContext);

    if ( SCARD_S_SUCCESS != lCallReturn ) {
        ReportResult(TEXT("SCardEstablishContext"), FAILURE, lCallReturn);
        CleanUp();
        return (g_fFailed?TPR_FAIL:TPR_PASS);
    } 
    else {
        // SCardListReaders
        // get name of installed reader
        GetLocalReaders();
    }

    // General API tests
    g_pKato->Log(LOG_DETAIL, TEXT("%.*s\n"), OUTPUT_LINE, BAR);
    
    // SCardIntroduceCardType
    // introduce a card to the Calais database, given the ATR just reported
    // locals for this call
    // {C1FDF2EF-A199-11d1-86BD-00A0C903AA9B}
    GUID        guidPrimaryProvider = 
    { 0xc1fdf2ef, 0xa199, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    
    // {06E8EC7E-A19A-11d1-86BD-00A0C903AA9B}
    GUID        guidInterface1 = 
    { 0x6e8ec7e, 0xa19a, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    
    // {0C36DB3B-A1AB-11d1-86BD-00A0C903AA9B}
    GUID        guidInterface2 = 
    { 0xc36db3b, 0xa1ab, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    
    GUID        rgguidInterfaces[2];
    rgguidInterfaces[0] = guidInterface1;
    rgguidInterfaces[1] = guidInterface2;
    
    DWORD   dwInterfaceCount = 2;
    
    // make API call
    lCallReturn = SCardIntroduceCardType(g_hContext, 
                                         szCardName, 
                                         &guidPrimaryProvider, 
                                         rgguidInterfaces, 
                                         dwInterfaceCount,
                                         (LPBYTE) &bGnDATR, NULL, 
                                         dwATRLen); //(LPBYTE)&bAtrMask,
    
    if ( SCARD_S_SUCCESS == lCallReturn ) {
        // verify success
        // reset variables
        memset(mszCards, 0, BUFFER_SIZE*sizeof(TCHAR));
        cmszCards = BUFFER_SIZE;
        
        lCallReturn = SCardListCards(NULL,  // no particular context
            NULL,                           // no particular ATR
            NULL,                           // no particular interfaces GUID
            0,                              // no interfaces
            mszCards,                       // return of card names as multistring
            &cmszCards);                    // length of return
        
        if ( SCARD_S_SUCCESS == lCallReturn )
        {
            // cards found
            fVerified = VerifyMSZ(szCardName, mszCards);
            if ( fVerified ) {
                // success
                ReportResult(TEXT("SCardIntroduceCardType"), SUCCESS);
                ReportResult(TEXT("SCardListCards"), SUCCESS);
            }
            else {
                // card was not in list
                ReportResult(TEXT("SCardIntroduceCardType"), FAILURE);
                g_pKato->Log(LOG_DETAIL, TEXT("  %s card was not added to the database\n"), szCardName);
                ReportResult(TEXT("SCardListCards"), SUCCESS);
            }
        }
        else {
            // could not list cards
            ReportResult(TEXT("SCardIntroduceCardType"), FAILURE);
            ReportResult(TEXT("  SCardListCardsW"), FAILURE, lCallReturn);
        }
    }
    else {
        // api reports failure
        ReportResult(TEXT("SCardIntroduceCardType"), FAILURE, lCallReturn);
        ReportResult(TEXT("SCardListCards"), FAILURE);
    }
    
    //
    // LIST PROVIDER ID
    //
    
    // locals for this call
    GUID        guidProviderId;
    
    // make API call
    lCallReturn = SCardGetProviderId(g_hContext,            // Resource Manager context
                                     szCardName,            // card name
                                     &guidProviderId);      // returned Provider ID

    if ( SCARD_S_SUCCESS == lCallReturn ) {
        // verify success
        fVerified = VerifyGUID(guidPrimaryProvider, guidProviderId);
        if ( fVerified ) {
            // success
            ReportResult(TEXT("SCardGetProviderId"), SUCCESS);
        }
        else {
            // correct provider id not found
            ReportResult(TEXT("SCardGetProviderId"), FAILURE);
        }
    }
    else {
        // api reports failure
        ReportResult(TEXT("SCardGetProviderId"), FAILURE, lCallReturn);
    }
    
    //
    // LIST INTERFACES
    //
    
    // locals for this call
    GUID    rgguidCardInterfaces[3];
    DWORD   dwCardInterfaceCount = 3;
    
    // make API call
    lCallReturn = SCardListInterfaces(g_hContext, 
                                      szCardName, 
                                      rgguidCardInterfaces, 
                                      &dwCardInterfaceCount);
    
    if ( SCARD_S_SUCCESS == lCallReturn ) {
        // verify success
        fVerified = VerifyGUID(guidInterface1, rgguidCardInterfaces[0]);
        if ( fVerified ) {
            // first of two interfaces verified
            fVerified = VerifyGUID(guidInterface2, rgguidCardInterfaces[1]);
            if ( fVerified )
                // second of two interfaces verified
                ReportResult(TEXT("SCardListInterfaces"), SUCCESS);
            else {
                // second interface was not correct
                ReportResult(TEXT("SCardListInterfaces"), FAILURE);
                g_pKato->Log(LOG_DETAIL, TEXT("  second interface GUID was not correct\n"));
            }
        }
        else {
            // first interface was not correct
            ReportResult(TEXT("SCardListInterfaces"), FAILURE);
            g_pKato->Log(LOG_DETAIL, TEXT("  first interface GUID was not correct\n"));
        }
    }
    else {
        

        if ( !(SCARD_E_INSUFFICIENT_BUFFER == lCallReturn) )
            ReportResult(TEXT("SCardListInterfaces"), FAILURE, lCallReturn);
    }
    
    //
    // FORGET CARD TYPE
    //
    
    // make API call
    lCallReturn = SCardForgetCardType(g_hContext, szCardName);
    if ( SCARD_S_SUCCESS == lCallReturn) {
        // verify results
        // reset variables
        memset(mszCards, 0, BUFFER_SIZE*sizeof(TCHAR));
        cmszCards = BUFFER_SIZE;
        
        // get listed cards
        lCallReturn = SCardListCards(NULL,  // no particular context
            NULL,                           // no particular ATR
            NULL,                           // no particular interfaces GUID
            0,                              // no interfaces
            mszCards,
            &cmszCards);
        
        if ( SCARD_S_SUCCESS == lCallReturn )
        {
            // see if card is removed from list
            fVerified = VerifyMSZ(szCardName, mszCards);
            if ( !fVerified )
                // success
                ReportResult(TEXT("SCardForgetCardType"), SUCCESS);
            else {
                // card still in list
                ReportResult(TEXT("SCardForgetCardType"), FAILURE);
                g_pKato->Log(LOG_DETAIL, TEXT("  %s card still in database\n"), szCardName);
            }
        }
        else {
            // could not list cards
            ReportResult(TEXT("SCardForgetCardType"), FAILURE);
            ReportResult(TEXT("  SCardListCards"), FAILURE, lCallReturn);
        }
    }
    else {
        // api reports failure
        ReportResult(TEXT("SCardForgetCardTypeW"), FAILURE, lCallReturn);
    }
  
    // transmit test
    mszCards[0]=0;
    SCardContainer cardArray;
    SCardFunction * oneCard;
    cardArray.AddedSCard(oneCard =new AmmiSCard()); oneCard->SCardIntroduceCardType(g_hContext);MakeMSZ( mszCards, (LPTSTR)oneCard->GetSCardProvider() );
    cardArray.AddedSCard(oneCard =new BullSCard()); oneCard->SCardIntroduceCardType(g_hContext);MakeMSZ( mszCards, (LPTSTR)oneCard->GetSCardProvider() );
    cardArray.AddedSCard(oneCard =new GieseckeSCard()); oneCard->SCardIntroduceCardType(g_hContext);MakeMSZ( mszCards, (LPTSTR)oneCard->GetSCardProvider() );
    cardArray.AddedSCard(oneCard =new IbmSCard()); oneCard->SCardIntroduceCardType(g_hContext);MakeMSZ( mszCards, (LPTSTR)oneCard->GetSCardProvider() );
    cardArray.AddedSCard(oneCard =new SchlumbgrSCard()); oneCard->SCardIntroduceCardType(g_hContext);MakeMSZ( mszCards, (LPTSTR)oneCard->GetSCardProvider() );
    cardArray.AddedSCard(oneCard =new SniSCard()); oneCard->SCardIntroduceCardType(g_hContext);MakeMSZ( mszCards, (LPTSTR)oneCard->GetSCardProvider() );
    cardArray.AddedSCard(oneCard =new GieseckeSCardV2()); oneCard->SCardIntroduceCardType(g_hContext);MakeMSZ( mszCards, (LPTSTR)oneCard->GetSCardProvider() );

    cmszReaders = BUFFER_SIZE;
    SCardListReaders(NULL,                      // no particular context
        NULL,                                   // all readers
        mszReaders,                             // buffer to receive reader names
        &cmszReaders);                          // size of buffer, size of returned multistring

    SCARD_READERSTATE    gReaderStates[MAX_READERS];
    LPTSTR pMszReaders=mszReaders;

    for (DWORD cReaders = 0;cReaders<MAX_READERS;cReaders++)
        if (*pMszReaders!=0 ) {
            g_pKato->Log( LOG_DETAIL,TEXT(" Locate Reader %s"),pMszReaders);
            gReaderStates[cReaders].szReader = pMszReaders;
            gReaderStates[cReaders].dwCurrentState = 0; // don't know current state
            while (*pMszReaders!=0)
                pMszReaders++;
            pMszReaders++;
        }
        else
            break;

    if ((lCallReturn=SCardLocateCards(g_hContext, mszCards, gReaderStates, cReaders))==SCARD_S_SUCCESS){ // may have passed
        for (DWORD readerIndex=0;readerIndex<cReaders;readerIndex++) {
            // check readerstate to see if atr matched card requested
            if ((SCARD_STATE_UNKNOWN|SCARD_STATE_UNAVAILABLE|SCARD_STATE_EXCLUSIVE|SCARD_STATE_INUSE) & gReaderStates[readerIndex].dwEventState) {
                g_pKato->Log( LOG_DETAIL,TEXT(" Reader %s is can not be used currently:status 0x%x"),
                        gReaderStates[readerIndex].szReader,gReaderStates[readerIndex].dwEventState);
                        g_fFailed = TRUE;

            }
            else
            if ( gReaderStates[readerIndex].dwEventState & SCARD_STATE_ATRMATCH) {// PASS
                oneCard=cardArray.FoundCardByAtr(gReaderStates[readerIndex].rgbAtr);
                if (oneCard) {
                    g_pKato->Log( LOG_DETAIL,TEXT(" Locate Card %s at Reader %s"),
                        oneCard->GetSCardProvider(),gReaderStates[readerIndex].szReader);
                    if(!SCardTransmitCheck(oneCard,gReaderStates[readerIndex].szReader))
                        break;
                }
                else {
                    g_pKato->Log( LOG_FAIL,TEXT(" Unknown card at Reader %s"),
                        gReaderStates[readerIndex].szReader);
                    g_fFailed=TRUE;
                };
            }
            else {
                g_pKato->Log( LOG_FAIL,TEXT(" Unknown card at Reader %s, dwEventState:%lx"),
                        gReaderStates[readerIndex].szReader,gReaderStates[readerIndex].dwEventState);
                    g_fFailed=TRUE;
            };
        };
    }
    else
    {
        ReportResult(TEXT("SCardLocateCards"), FAILURE, lCallReturn);
    }

    //
    // END OF TESTS
    //
    
    // cleanup context, connection
    CleanUp(); 

    return (g_fFailed?TPR_FAIL:TPR_PASS);
}

/////////////////////////////////////////////////////////////////////////////


