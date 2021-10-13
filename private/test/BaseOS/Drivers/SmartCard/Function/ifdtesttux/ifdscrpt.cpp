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
/*++
Module Name:

     ifdscrpt.cpp

Abstract:
Functions:
Notes:
--*/


#ifndef UNDER_CE
#include <afx.h>
#include <afxtempl.h>
#include <conio.h>
#else
#include "afxutil.h"

#endif

#include <winioctl.h>


#include <winsmcrd.h>
#include "ifdtest.h"

class CCardList;

// This represents a single function of a card
class CCardFunction  
{    
    CString m_CName;
    WCHAR m_chShortCut;
    CByteArray m_CData;
    CCardFunction *m_pCNextFunction;

public:
    CCardFunction(
        const CString &in_CName,
        WCHAR in_chShortCut,
        const CByteArray &in_CData
        );
    friend CCardList;
};

// This is a single card
class CCard 
{    
    CString    m_CName;
    WCHAR m_chShortCut;
    CCardFunction *m_pCFirstFunction;
    CCard *m_pCNextCard;

public:
    CCard(
        const CString &in_CCardName,
        WCHAR in_chShortCut
        );

    friend CCardList;
};

// This implements a list of cards
class CCardList  
{
    CString    m_CScriptFileName;
    CCard *m_pCFirstCard;
    CCard *m_pCCurrentCard;
    ULONG m_uNumCards;

public:
    CCardList(CString &in_CScriptFileName);

    void 
    AddCard(
        const CString &in_CardName,
        WCHAR in_chShortCut
        );

    void
    AddCardFunction(
        const CString &in_CFunctionName, 
        WCHAR in_chShortCut,
        const CByteArray &l_pCData
        );

    void ShowCards(
        int (__cdecl *in_pCallBack)(void *in_pContext, ...),
        void *in_pContext
        );

    BOOLEAN SelectCard(WCHAR in_chShortCut);
    void ReleaseCard(void);
    CString GetCardName(void);
    ULONG GetNumCards(void) const
    {         
        return m_uNumCards;
    }
    BOOLEAN    IsCardSelected(void) const;
    BOOLEAN    ListFunctions(void);
    CByteArray *GetApdu(WCHAR in_chShortCut);
};

CCardFunction::CCardFunction(
    const CString &in_CName,
    WCHAR in_chShortCut,
    const CByteArray &in_CData
    )
/*++
    Adds a function to the current card
--*/
{
    m_CName = in_CName;
    m_chShortCut = in_chShortCut;
    m_CData.Copy(in_CData);
    m_pCNextFunction = NULL;
}

CCard::CCard(
    const CString &in_CCardName,
    WCHAR in_chShortCut
    )
/*++

Routine Description:

    Constructor for a new card

Arguments:

    CardName - Reference to card to add
    in_uPos - index of shortcut key

Return Value:

--*/
{
    m_CName = in_CCardName;
    m_chShortCut = in_chShortCut;
    m_pCNextCard = NULL;
    m_pCFirstFunction = NULL;
}
    
void
CCardList::AddCard(
    const CString &in_CCardName,
    WCHAR in_chShortCut
    )
/*++

Routine Description:
    Adds a new card to CardList

Arguments:
    in_CCardName - Reference to card to add

--*/
{
    CCard *l_pCNewCard = new CCard(in_CCardName, in_chShortCut);

    if (m_pCFirstCard == NULL) 
    {
        m_pCFirstCard = l_pCNewCard;

    } 
    else 
    {
        CCard *l_pCCurrent = m_pCFirstCard;
        while (l_pCCurrent->m_pCNextCard) 
        {
            l_pCCurrent = l_pCCurrent->m_pCNextCard;
        }

        l_pCCurrent->m_pCNextCard = l_pCNewCard;
    }

    m_pCCurrentCard = l_pCNewCard;
    m_uNumCards += 1;
}

void
CCardList::AddCardFunction(
    const CString &in_CFunctionName,
    WCHAR in_chShortCut,
    const CByteArray &in_pCData
    )
/*++

Routine Description:
    Adds a new function to the current card

Arguments:
    in_CCardName - Reference to card to add
    in_chShortCut - Shortcut key

Return Value:

--*/
{
    CCardFunction *l_pCNewFunction = new CCardFunction(
        in_CFunctionName, 
        in_chShortCut, 
        in_pCData
        );

    if (m_pCCurrentCard->m_pCFirstFunction == NULL) 
    {
        m_pCCurrentCard->m_pCFirstFunction = l_pCNewFunction;

    } 
    else 
    {
        CCardFunction *l_pCCurrent = m_pCCurrentCard->m_pCFirstFunction;
        while (l_pCCurrent->m_pCNextFunction) 
        {
            l_pCCurrent = l_pCCurrent->m_pCNextFunction;
        }

        l_pCCurrent->m_pCNextFunction = l_pCNewFunction;
    }
}

CCardList::CCardList(
    CString &in_CScriptFileName
    )
/*++

Routine Description:

    Adds a new function to the current card

Arguments:

    CardName - Reference to card to add
    in_uPos - index of shortcut key

Return Value:

--*/
{
    const DWORD BUFFER_SIZE=255;
    FILE *l_hScriptFile;
    WCHAR l_rgchBuffer[BUFFER_SIZE];
    WCHAR l_chKey = 0;
    ULONG l_uLineNumber = 0;
    BOOLEAN l_bContinue = FALSE;
    CByteArray l_Data;
    CString l_CCommand;
        
    m_pCFirstCard = NULL;
    m_pCCurrentCard = NULL;
    
     if (_wfopen_s(&l_hScriptFile, in_CScriptFileName, L"r"))
    {
        wprintf(
            TestLoadStringResource(IDS_SCRIPT_FILE_CAN_NOT_OPENED).GetBuffer(0), 
            in_CScriptFileName
            );
        return;
    }

    m_CScriptFileName = in_CScriptFileName;
    
    while (fgetws(l_rgchBuffer, BUFFER_SIZE, l_hScriptFile))
    {

        const TCHAR *l_pszError = NULL;
        do
        {
            CString l_CLine(l_rgchBuffer);
            CString l_CCommandApdu;

            l_uLineNumber += 1;

            if (l_CLine.GetLength() != 0 && l_CLine[0] == '#') 
            {        
                // comment line found, skip this line
                break;  // Go and grab next line.
            }

            // Get rid of leading and trailing spaces
            l_CLine.TrimLeft();
            l_CLine.TrimRight();

            int l_ichStart = l_CLine.Find('[');       
            int l_ichKey = l_CLine.Find('&');
            int l_ichEnd = l_CLine.Find(']');

            if(l_ichStart == 0 && l_ichKey > 0 && l_ichEnd > l_ichKey + 1) 
            {
                // Add new card to list

                CString l_CardName;

                // Change card name from [&Card] to [C]ard
                l_CardName = 
                    l_CLine.Mid(l_ichStart + 1, l_ichKey - l_ichStart - 1) + 
                    '[' +
                    l_CLine[l_ichKey + 1] +
                    ']' +
                    l_CLine.Mid(l_ichKey + 2, l_ichEnd - l_ichKey - 2);
               
                AddCard(
                    l_CardName, 
                    l_CardName[l_ichKey]
                    );

            } 
            else if (l_ichStart == -1 && l_ichKey >= 0 && l_ichEnd == -1) 
            {
                // Add new function to current card

                // Get function name
                CString l_CToken = l_CLine.SpanExcluding(L",");

                // Search for shurtcut key
                l_ichKey = l_CToken.Find('&');

                if (l_ichKey == -1) 
                {
                    l_pszError = TestLoadStringResource(IDS_MISSING_AND_IN_FUN).GetBuffer(0);
                    break;
                }

                l_chKey = l_CToken[l_ichKey + 1];

                // Change card function from &Function to [F]unction

                l_CCommand = 
                    l_CToken.Mid(l_ichStart + 1, l_ichKey - l_ichStart - 1) + 
                    TEXT('[') +
                    l_CToken[l_ichKey + 1] +
                    TEXT(']') +
                    l_CToken.Right(l_CToken.GetLength() - l_ichKey - 2);
                
                LONG l_lComma = l_CLine.Find(',');
            
                if (l_lComma == -1) 
                {
                    l_pszError = TestLoadStringResource(IDS_MISSING_CMD_APDU).GetBuffer(0);
                    break;
                } 
                else 
                {                    
                    l_CCommandApdu = l_CLine.Right(l_CLine.GetLength() - l_lComma - 1);
                }

            } 
            else if (l_bContinue) 
            {
                l_CCommandApdu = l_CLine;            

            } 
            else if (l_CLine.GetLength() != 0 && l_CLine[0] != '#') 
            {
                l_pszError = TestLoadStringResource(IDS_LINE_INVALID).GetBuffer(0);
                break;
            }
       
            if (l_CCommandApdu != TEXT(""))
            {        
                do 
                {
                    CHAR l_chData;
                    l_CCommandApdu.TrimLeft();

                    ULONG l_uLength = l_CCommandApdu.GetLength();

                    if (l_uLength >= 3 &&
                        l_CCommandApdu[0] == '\'' &&
                        l_CCommandApdu[2] == '\'') 
                    {
                        // add ascsii character like 'c'
                        l_chData = (CHAR) l_CCommandApdu[1]; // flagged
                        l_Data.Add(l_chData);                
                         
                    } 
                    else if(l_uLength >= 3 &&
                        l_CCommandApdu[0] == '\"' &&
                        l_CCommandApdu.Right(l_uLength - 2).Find('\"') != -1) 
                    {
                        // add string like "string"
                        for (INT l_iIndex = 1; l_CCommandApdu[l_iIndex] != '\"'; l_iIndex++) 
                        {
                            l_Data.Add((BYTE) l_CCommandApdu[l_iIndex]);                                         
                        }

                    } 
                    else if (l_CCommandApdu.SpanIncluding(L"0123456789abcdefABCDEF").GetLength() == 2) 
                    {
                        int iRet = swscanf(l_CCommandApdu, L"%2x", &l_chData);
                        l_Data.Add(l_chData);                

                    }
                    else if (l_CCommandApdu.SpanIncluding(TEXT("0123456789abcdefABCDEF")).GetLength() == 1)
                    {
                        if(1 == sscanf(l_CCommandApdu, "%1x", &l_chData))
                        {
                            l_Data.Add(l_chData);                
                        }

                    } 
                    else 
                    {                         
                        l_CCommandApdu = l_CCommandApdu.SpanExcluding(L",");
                        static CString l_CError;
                        l_CError = TestLoadStringResource(IDS_ILLEGAL_VAL_FOUND).GetBuffer(0) + l_CCommandApdu;
                        l_pszError = (LPCTSTR) l_CError;
                        break;
                    } 
                        
                    l_ichStart = l_CCommandApdu.Find(',');
                    if (l_ichStart != -1) 
                    {                         
                        l_CCommandApdu = l_CLine.Right(l_CCommandApdu.GetLength() - l_ichStart - 1);
                    }

                } while (l_ichStart != -1);

                if (l_pszError)
                    break;

                if (l_CLine.Find('\\') != -1) 
                {                    
                    // we have to read more data from the file
                    l_bContinue = TRUE;
                } 
                else 
                {
                    if (m_pCCurrentCard == NULL) 
                    {
                        l_pszError =  TestLoadStringResource(IDS_CARD_CMD_FOUND_NO_CARD_DEFINED).GetBuffer(0);
                        break;
                    }
                        
                    AddCardFunction(
                        l_CCommand,
                        l_chKey,
                        l_Data
                        );

                    l_CCommand = "";
                    l_Data.RemoveAll();
                    l_bContinue = FALSE;                
                }
            } 
        } while (0);
        if (l_pszError)
        {    
            wprintf(
                L"%s (%d): %s\n",
                in_CScriptFileName.GetBuffer(0), 
                l_uLineNumber,
                l_pszError
                );    

            l_CCommand = "";
            l_Data.RemoveAll();
            l_bContinue = FALSE;                
        }
    }
    m_pCCurrentCard = NULL;
}        

void
CCardList::ShowCards(
    int (__cdecl *in_pCallBack)(void *in_pContext, ...),
    void *in_pContext
    )
{
    CCard *l_pCCurrentCard = m_pCFirstCard;

    if (l_pCCurrentCard == NULL) 
    {
        return;
    }

    while(l_pCCurrentCard) 
    {
        (*in_pCallBack) (in_pContext, (PWCHAR) (LPCTSTR) l_pCCurrentCard->m_CName);

        l_pCCurrentCard = l_pCCurrentCard->m_pCNextCard;
    }
}

BOOLEAN
CCardList::ListFunctions(
    void
    )
/*++
    List all card functions
--*/
{
    if (m_pCCurrentCard == NULL)
    {
        return FALSE;
    }

    CCardFunction *l_pCCurrentFunction = m_pCCurrentCard->m_pCFirstFunction;

    while(l_pCCurrentFunction) 
    {
        wprintf(L"   %s\n", (LPCTSTR) l_pCCurrentFunction->m_CName);
        l_pCCurrentFunction = l_pCCurrentFunction->m_pCNextFunction;
    }
    return TRUE;
}

BOOLEAN
CCardList::SelectCard(
    WCHAR in_chShortCut
    )
/*++

Routine Description:
    Selectd a card by shorcut

Arguments:
    chShortCut - Shortcut key
    
Return Value:
    TRUE - card found and selected
    FALSE - no card with that shortcut found

--*/
{
    m_pCCurrentCard = m_pCFirstCard;

    while(m_pCCurrentCard) 
    {
        if (m_pCCurrentCard->m_chShortCut == in_chShortCut) 
        {
            return TRUE;
        }

        m_pCCurrentCard = m_pCCurrentCard->m_pCNextCard;
    }

    m_pCCurrentCard = NULL;

    return FALSE;
}

void CCardList::ReleaseCard(
    void
    )
{
    m_pCCurrentCard = NULL;
}

BOOLEAN
CCardList::IsCardSelected(
    void
    ) const
{
    return (m_pCCurrentCard != NULL);
}

CString 
CCardList::GetCardName(
    void
    )
{
    CString l_CCardName;
    INT l_iLeft = m_pCCurrentCard->m_CName.Find('[');    
    INT l_iLength = m_pCCurrentCard->m_CName.GetLength();

    l_CCardName = 
        m_pCCurrentCard->m_CName.Left(l_iLeft) + 
        m_pCCurrentCard->m_CName[l_iLeft + 1] +
        m_pCCurrentCard->m_CName.Right(l_iLength - l_iLeft - 3);

    return l_CCardName;
}


CByteArray *
CCardList::GetApdu(
    WCHAR in_chShortCut
    )
{
    CCardFunction *l_pCCurrentFunction = m_pCCurrentCard->m_pCFirstFunction;

    while(l_pCCurrentFunction) 
    {
        if (l_pCCurrentFunction->m_chShortCut == in_chShortCut) 
        {
            return &l_pCCurrentFunction->m_CData;
        }

        l_pCCurrentFunction = l_pCCurrentFunction->m_pCNextFunction;
    }
    return NULL; 
}

void 
ManualTest(
    CReader &in_CReader
    )
{
    CString datFile(TestLoadStringResource(IDS_IFDTEST_DAT_FILE));
    CCardList l_CCardList(datFile);
    ULONG l_uRepeat = 0;
    LONG l_lResult;
    WCHAR l_chSelection;
    PUCHAR l_pbResult = NULL;
    ULONG l_uState, l_uPrevState;
    CString 
        l_CCardStates[] =  {
            TestLoadStringResource(IDS_UNKNOWN_STATE), 
            TestLoadStringResource(IDS_ABSENT_STATE),
            TestLoadStringResource(IDS_PRESENT_STATE),
            TestLoadStringResource(IDS_SWALLOWED_STATE), 
            TestLoadStringResource(IDS_POWERED_STATE),
            TestLoadStringResource(IDS_NEGOTIABLE_STATE),
            TestLoadStringResource(IDS_SPECIFIC_STATE) 
        };

    BOOL l_bWaitForInsertion, l_bWaitForRemoval;

    while (TRUE)  
    {
        ULONG l_uResultLength = 0;

        wprintf(
            L"%s",
            TestLoadStringResource(IDS_MANUAL_READER_TEST).GetBuffer(0)
            );

        wprintf(L"------------------\n");

        if (l_CCardList.IsCardSelected()) 
        {
            wprintf(
                TestLoadStringResource(IDS_CMDS).GetBuffer(0), 
                l_CCardList.GetCardName()
                );

            l_CCardList.ListFunctions();
            wprintf(
                L"%s",
                TestLoadStringResource(IDS_OTHER_CMDS).GetBuffer(0)
                );
        } 
        else 
        {            
            wprintf(
                L"%s",
                TestLoadStringResource(IDS_READER_CMDS).GetBuffer(0)
                );
            
            if (l_CCardList.GetNumCards() != 0) 
            {                 
                wprintf(
                    L"%s",
                    TestLoadStringResource(IDS_CARD_CMDS).GetBuffer(0)
                    );
                l_CCardList.ShowCards((int (__cdecl *)(void *, ...)) wprintf, L"   %s\n");
            }
            wprintf(
                L"%s",
                TestLoadStringResource(IDS_OTHER_EXIT_CMD).GetBuffer(0)
                );
        }

        wprintf(
            TestLoadStringResource(IDS_INFO).GetBuffer(0), 
            in_CReader.GetVendorName(), 
            in_CReader.GetIfdType(), 
            in_CReader.GetDeviceUnit()
            );

        l_chSelection = _gettchar();
        putchar('\n');

        int iRet;

        if (l_CCardList.IsCardSelected()) 
        {
            switch (l_chSelection) 
            {
                case 'x':
                    l_CCardList.ReleaseCard();
                    continue;

                case 'r':
                    wprintf(
                        L"%s",
                        TestLoadStringResource(IDS_REPEAT_CNT).GetBuffer(0)
                        );
                    iRet = wscanf(L"%2d", &l_uRepeat);

                    if (l_uRepeat > 99) 
                    {
                        l_uRepeat = 0;
                    }
                    wprintf(
                        L"%s",
                        TestLoadStringResource(IDS_ENT_CMD).GetBuffer(0)
                        );
                    l_chSelection = _gettchar();                 

                    // no bbreak;
                    __fallthrough;

                default:                 
                    CByteArray *l_pCData;

                    if((l_pCData = l_CCardList.GetApdu(l_chSelection)) != NULL) 
                    {                        
                        l_lResult = in_CReader.Transmit(
                            l_pCData->GetData(),
                            (ULONG) l_pCData->GetSize(),
                            &l_pbResult,
                            &l_uResultLength
                            );

                    } 
                    else 
                    {
                        wprintf(
                            L"%s",
                            TestLoadStringResource(IDS_INVALID_SEL).GetBuffer(0)
                            );
                        continue;
                    }
                    break;
            }
        } 
        else 
        {
            switch(l_chSelection)
            {                    
                case '0':
                    wprintf(
                        L"%s",
                        TestLoadStringResource(IDS_CHANGING_T_0).GetBuffer(0)
                        );
                    l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T0);
                    break;

                case '1':
                    wprintf(
                        L"%s",
                        TestLoadStringResource(IDS_CHANGING_T_1).GetBuffer(0)
                        );
                    l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T1);
                    break;
                
                case 'c':
                    wprintf(
                        L"%s",
                        TestLoadStringResource(IDS_COLDRESET).GetBuffer(0)
                        );
                    l_lResult = in_CReader.ColdResetCard();
                    in_CReader.GetAtr(&l_pbResult, &l_uResultLength);
                    break;

                case 'r':
                    wprintf(
                        L"%s",
                        TestLoadStringResource(IDS_WARM_RESET).GetBuffer(0)
                        );
                    l_lResult = in_CReader.WarmResetCard();
                    in_CReader.GetAtr(&l_pbResult, &l_uResultLength);
                    break;
                
                case 'd':
                    wprintf(
                        L"%s",
                        TestLoadStringResource(IDS_POWERDOWN).GetBuffer(0)
                        );
                    l_lResult = in_CReader.PowerDownCard();
                    break;

                case 'h':
                    wprintf(
                        L"%s",
                        TestLoadStringResource(IDS_HIBERNATING_TEST_1_MIN).GetBuffer(0)
                        );
                    l_uPrevState = SCARD_UNKNOWN;
                    l_bWaitForInsertion = FALSE;
                    l_bWaitForRemoval = FALSE;
                    for (l_uRepeat = 0; l_uRepeat < 120; l_uRepeat++) 
                    {                         
                        l_lResult = in_CReader.GetState(&l_uState);
                        wprintf(L"(%ld/%ld)  ", l_lResult, l_uState);

                        Sleep(CLOCKS_PER_SEC/2);
                    }
                    continue;
                case 's': 
                    wprintf(
                        L"%s",
                        TestLoadStringResource(IDS_GET_STATE).GetBuffer(0)
                        );
                    l_lResult = in_CReader.GetState(&l_uState);
                    wprintf(
                        L"%s",
                        TestLoadStringResource(IDS_GET_STATE_1).GetBuffer(0)
                        );
                    l_pbResult = (PBYTE) &l_uState;
                    l_uResultLength = sizeof(ULONG);
                    break;

                case 'a':
                    wprintf(
                        L"%s",
                        TestLoadStringResource(IDS_WAITING_FOR_REMOVAL).GetBuffer(0)
                        );
                    l_lResult = in_CReader.WaitForCardRemoval();
                    break;

                case 'p':
                    wprintf(
                        L"%s",
                        TestLoadStringResource(IDS_WAITING_FOR_INSERTION).GetBuffer(0)
                        );
                    l_lResult = in_CReader.WaitForCardInsertion();
                    break;

                case 'v':
                    wprintf(
                        L"%s",
                        TestLoadStringResource(IDS_TEST_VENDOR_IOCTL).GetBuffer(0)
                        );
                    l_lResult = in_CReader.VendorIoctl(&l_pbResult, &l_uResultLength);
                       break;
                
                case 'x':
                    exit(0);
                
                default:
                    // Try to select a card
                    if (l_CCardList.SelectCard(l_chSelection) == FALSE) 
                    {
                        wprintf(
                            L"%s",
                            TestLoadStringResource(IDS_INVALID_SEL).GetBuffer(0)
                            );
                    }
                    l_uRepeat = 0;
                    continue;
            }
        }

        wprintf(
            TestLoadStringResource(IDS_RET_VAL).GetBuffer(0),                
            l_lResult, 
            MapWinErrorToNtStatus(l_lResult)
            );

        if (l_lResult == ERROR_SUCCESS && l_uResultLength) 
        {
            ULONG l_uIndex, l_uLine, l_uCol;
        
            // The I/O request has data returned
            wprintf(
                TestLoadStringResource(IDS_DATA_RET).GetBuffer(0), 
                l_uResultLength, 
                0
                );

            for (l_uLine = 0, l_uIndex = 0; 
                l_uLine < ((l_uResultLength - 1) / 8) + 1; 
                l_uLine++) 
            {
                for (l_uCol = 0, l_uIndex = l_uLine * 8; 
                    l_uCol < 8 && l_uIndex < l_uResultLength; l_uCol++, 
                    l_uIndex++) 
                {                    
                    wprintf(
                        l_uIndex < l_uResultLength ? L"%02x " : L"   ",
                        l_pbResult[l_uIndex]
                        );
                }

                  putchar(' ');

                for (l_uCol = 0, l_uIndex = l_uLine * 8; 
                    l_uCol < 8; l_uCol++, 
                    l_uIndex++) 
                {
                    wprintf(
                        l_uIndex < l_uResultLength ? L"%c" : L" ",
                        isprint(l_pbResult[l_uIndex]) ? l_pbResult[l_uIndex] : L'.'
                        );
                }

                putchar('\n');
                if (l_uIndex  < l_uResultLength) 
                {
                    wprintf(L"   %04x: ", l_uIndex + 1);
                }
            }
        }
        putchar('\n');
    }
}
