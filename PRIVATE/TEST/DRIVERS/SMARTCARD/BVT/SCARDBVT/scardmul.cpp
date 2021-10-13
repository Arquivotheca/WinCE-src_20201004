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

    SCardMul

Description:

    This file contains the implementation of build verification
    tests of the Calais Smart Card API suite.
    
Notes:

    None.
    

--*/
#include <windows.h>
#include <tchar.h>
#include <string.h>
#include <winscard.h>
#include <winsmcrd.h>
#include "ft.h"
#include "tuxbvt.h"
#include "debug.h"
#include "SCardf.h"

#include "mthread.h"
#include "SCardMul.h"

#define MAX_READERS         4   // max number of readers to test at once

extern void MakeMSZ( LPTSTR mszString, LPTSTR szString );

BOOL IsStringInMultiString(LPCTSTR pMultiString, LPCTSTR pString) 
{
    while(*pMultiString!=0) {
        if (_tcscmp(pMultiString,pString)==0) // equal
            return TRUE;
        while (*pMultiString!=0)
            pMultiString++;
        // pMultString point to first NULL. skip it
        pMultiString++;
    };
    return FALSE;// not found
}

class SCard_Query: public SyncTestObj {
public:
    SCard_Query(SCardFunction * pcCard,
            DWORD dwTestCase,
            SCARDCONTEXT hContext,
            CEvent * pcEvent,
            DWORD dwSyncCount): 
        SyncTestObj(pcEvent,dwSyncCount),
        m_hContext(hContext)
    {m_pcCard=pcCard;m_dwTestCase=dwTestCase;};
    virtual DWORD RunTest ();
private:
    SCardFunction *m_pcCard;
    DWORD m_dwTestCase;
    SCARDCONTEXT m_hContext;
};

DWORD SCard_Query::RunTest()
{
    // Test SCardGetProvider
    SCARDCONTEXT hContext=m_hContext;
    LPCTSTR szCard=m_pcCard->GetSCardProvider();
    switch (m_dwTestCase) {
        case 1:
            hContext=NULL;
            break;
    };
    GUID guidProviderId;
    CheckLock();
    DWORD errorCode=SCardGetProviderId(hContext,szCard,&guidProviderId);
    if (errorCode!=SCARD_S_SUCCESS) {
        SetErrorCode(errorCode);
        return errorCode;
    };
    // Check Result.
    if (guidProviderId != *(m_pcCard->GetGuidPrimary())) {
        SetErrorCode((DWORD)-1);
        return (DWORD)-1;
    };

    // Test SCardListCards
    TCHAR mszCards[NAME_BUFFER_SIZE];
    mszCards[0]=0;
    DWORD cchCards=NAME_BUFFER_SIZE;

    hContext=m_hContext;
    LPCBYTE pbAtr=NULL;
    LPCGUID rgguidInterfaces=NULL;
    DWORD   cguidInterfaceCount=0;

    switch (m_dwTestCase) {
    case 1: 
        hContext=NULL;
        break;
    case 2:
        pbAtr=m_pcCard->GetpbAtr();
        break;
    case 3:
        pbAtr=m_pcCard->GetpbAtr();
        break;
    case 4:
        rgguidInterfaces=m_pcCard->GetGuidInterface();
        cguidInterfaceCount=1;
        break;
    case 5:
        rgguidInterfaces=m_pcCard->GetGuidInterface();
        cguidInterfaceCount=1;
        pbAtr=m_pcCard->GetpbAtr();
        break;
    case 6:
        rgguidInterfaces=m_pcCard->GetGuidInterface();
        cguidInterfaceCount=1;
        pbAtr=m_pcCard->GetpbAtr();
        hContext=NULL;
        break;
    };
    CheckLock();
    errorCode=SCardListCards(hContext,pbAtr,rgguidInterfaces,cguidInterfaceCount,
        mszCards,&cchCards);
    if (errorCode!=SCARD_S_SUCCESS) {
        SetErrorCode(errorCode);
        return errorCode;
    };
    if (!IsStringInMultiString(mszCards,m_pcCard->GetSCardProvider())) {
        SetErrorCode((DWORD)-2);
        return (DWORD)-2;
    };

    hContext=m_hContext;
    szCard=m_pcCard->GetSCardProvider();
    GUID guidInterfaces[10];
    DWORD cguidInterfaces=10;
    switch (m_dwTestCase) {
    case 1: 
        hContext=NULL;
        break;
    };
    CheckLock();
    errorCode=SCardListInterfaces(hContext,szCard,guidInterfaces,&cguidInterfaces);
    if (errorCode!=SCARD_S_SUCCESS) {
        SetErrorCode(errorCode);
        return errorCode;
    };
    BOOL bFound=FALSE;
    for (DWORD index=0;index<cguidInterfaces;index++) {
        if (guidInterfaces[index]==*(m_pcCard->GetGuidInterface()) ) {
            bFound=TRUE;
            break;
        };
    };
    if (!bFound) {
        SetErrorCode((DWORD)-3);
        return ((DWORD)-3);
    };

    // Test List Reader.
    return 0;
};




class SCard_Tracking: public SyncTestObj {
public:
    SCard_Tracking(SCardFunction * pcCard,
            LPCTSTR lpReader,
            DWORD dwTestCase,
            SCARDCONTEXT hContext,
            CEvent * pcEvent,
            DWORD dwSyncCount): 
        SyncTestObj(pcEvent,dwSyncCount),
        m_lpReader(lpReader),m_hContext(hContext)
    {m_pcCard=pcCard;m_dwTestCase=dwTestCase;};
    virtual DWORD RunTest ();
private:
    SCardFunction *m_pcCard;
    DWORD m_dwTestCase;
    SCARDCONTEXT m_hContext;
    LPCTSTR m_lpReader;
};

DWORD SCard_Tracking::RunTest()
{

    SCARDCONTEXT hContext=m_hContext;
    TCHAR mszCards[NAME_BUFFER_SIZE];

    mszCards[0]=0;
    _tcscpy(mszCards,m_pcCard->GetSCardProvider());
    mszCards[_tcslen(mszCards) + 1 ] =0;// end of list.
    SCARD_READERSTATE    ReaderStates;
    
    ReaderStates.szReader = m_lpReader;
    ReaderStates.dwCurrentState = 0; // don't know current state

    CheckLock();
    DWORD errorCode=SCardLocateCards(hContext,mszCards,&ReaderStates,1);
    if (errorCode==SCARD_E_UNKNOWN_CARD) { // There is no reader available.should be SCARD_E_UNKNOWN_READER
        g_pKato->Log(LOG_SKIP,TEXT("SCardLocate return Unavailable for reader %s! Test Skipped"),m_lpReader);
        return 0;

    }
    if (errorCode!=SCARD_S_SUCCESS) {
        SetErrorCode(errorCode);
        return errorCode;
    };
    CheckLock();
    errorCode = SCardGetStatusChange(hContext, 1000,&ReaderStates,1);
    if (errorCode!=SCARD_S_SUCCESS && errorCode!=SCARD_E_TIMEOUT) {
        SetErrorCode(errorCode);
        return errorCode;
    };
    CheckLock();
    errorCode=SCardCancel(hContext );
    if (errorCode!=SCARD_S_SUCCESS) {
        SetErrorCode(errorCode);
        return errorCode;
    };
    return 0;

};
 

class SCard_Access: public SyncTestObj {
public:
    SCard_Access(SCardFunction * pcCard,
            LPCTSTR lpReader,
            DWORD dwTestCase,
            SCARDCONTEXT hContext,
            CEvent * pcEvent,
            DWORD dwSyncCount): 
        SyncTestObj(pcEvent,dwSyncCount),
        m_lpReader(lpReader),m_hContext(hContext)
    {m_pcCard=pcCard;m_dwTestCase=dwTestCase;};
    virtual DWORD RunTest ();
private:
    SCardFunction *m_pcCard;
    DWORD m_dwTestCase;
    SCARDCONTEXT m_hContext;
    LPCTSTR m_lpReader;
};

DWORD SCard_Access::RunTest()
{
    SCARDHANDLE hCard;
    DWORD dwActiveProtocol;
    CheckLock();
    CheckLock();
    DWORD errorCode=SCardEstablishContext(SCARD_SCOPE_SYSTEM, 
                              NULL,
                              NULL,
                              &m_hContext);
    if (errorCode!=SCARD_S_SUCCESS) {
        SetErrorCode(errorCode);
        g_pKato->Log( LOG_FAIL,TEXT("SCardEstablishContext return %lx, Failure "),errorCode);
        return errorCode;
    };
    CheckLock();
    errorCode=SCardConnect(m_hContext,m_lpReader,SCARD_SHARE_EXCLUSIVE ,
        SCARD_PROTOCOL_T0|SCARD_PROTOCOL_T1,&hCard,&dwActiveProtocol);
    if (errorCode==SCARD_E_READER_UNAVAILABLE || errorCode==SCARD_E_SHARING_VIOLATION) {
        g_pKato->Log(LOG_SKIP,TEXT("SCardConnect return Unavailable for reader %s! Test Skipped"),m_lpReader);
        SCardReleaseContext(m_hContext);
        return 0;       
    };
    if (errorCode!=SCARD_S_SUCCESS) {
        g_pKato->Log( LOG_FAIL,TEXT("SCardConnect return 0x%lx for reader %s"),errorCode,m_lpReader);
        SetErrorCode(errorCode);
        SCardReleaseContext(m_hContext);
        return errorCode;
    };

    DWORD dwInitialization = SCARD_LEAVE_CARD ;
    switch (m_dwTestCase) {
    case 2: 
        dwInitialization=SCARD_RESET_CARD ;
        break;
    case 3:
        dwInitialization=SCARD_UNPOWER_CARD ;
        break;
    };

    CheckLock();
    errorCode=SCardReconnect(hCard,SCARD_SHARE_EXCLUSIVE,
        SCARD_PROTOCOL_T0|SCARD_PROTOCOL_T1,dwInitialization,&dwActiveProtocol);
    if (errorCode!=SCARD_S_SUCCESS) {
        SetErrorCode(errorCode);
        SCardReleaseContext(m_hContext);
        return errorCode;
    };

    DWORD rCode=0;
    CheckLock();
    if (SCardBeginTransaction(hCard)==SCARD_S_SUCCESS) {

        TCHAR mszReaderNames[NAME_BUFFER_SIZE];
        DWORD cmszReaderNames=NAME_BUFFER_SIZE;
        DWORD dwStates;
        DWORD dwProtocol;
        BYTE bATR[0x20];
        DWORD bcAtrLen=0x20;
        CheckLock();
        errorCode=SCardStatus(hCard,mszReaderNames,&cmszReaderNames,&dwStates,
                &dwProtocol,bATR,&bcAtrLen);
        if (errorCode==SCARD_S_SUCCESS) {
            // compare the ATR
            if (m_pcCard->GetAtrLength()!=bcAtrLen ||
                    memcmp(m_pcCard->GetpbAtr(),bATR,bcAtrLen)!=0) {
                SetErrorCode(rCode=errorCode=(DWORD)-1);
            }
            else 
                if (dwActiveProtocol & (SCARD_PROTOCOL_T0  | SCARD_PROTOCOL_T1 )) {// t1 or t0
                    BOOL bCheckResult;
                    CheckLock();
                    
                    g_pKato->Log( LOG_DETAIL,TEXT("SCardCheckTransmit for card %s"),m_pcCard->GetSCardProvider());
                    errorCode= m_pcCard->SCardCheckTransmit(hCard,
                        (dwActiveProtocol & SCARD_PROTOCOL_T0)?SCARD_PCI_T0:SCARD_PCI_T1,
                        &bCheckResult);
                    if (errorCode!=SCARD_S_SUCCESS) {
                        SetErrorCode(rCode=errorCode) ;
                    }
                    else {
                        if (!bCheckResult) {
                            SetErrorCode(rCode=errorCode=(DWORD)-2);
                        };
                    };
                };
        }
        else {
            SetErrorCode(rCode=errorCode);
        }

        
        DWORD dwDisposition=SCARD_LEAVE_CARD ;
        switch (m_dwTestCase) {
        case 2: 
            dwDisposition=SCARD_RESET_CARD ;
            break;
        case 3:
            dwDisposition=SCARD_UNPOWER_CARD;
            break;
        case 4:
            dwDisposition=SCARD_EJECT_CARD;
            break;
        };
        errorCode=SCardEndTransaction(hCard,dwDisposition);
        if (errorCode!=SCARD_S_SUCCESS) 
            SetErrorCode(rCode=errorCode);
    }
    else 
        SetErrorCode(rCode=errorCode);

    DWORD dwDisposition=SCARD_LEAVE_CARD ;
    switch (m_dwTestCase) {
    case 2: 
        dwDisposition=SCARD_RESET_CARD ;
        break;
    case 3:
        dwDisposition=SCARD_UNPOWER_CARD;
        break;
    case 4:
        dwDisposition=SCARD_EJECT_CARD;
        break;
    };
    errorCode=SCardDisconnect(hCard,dwDisposition );
    SCardReleaseContext(m_hContext);

    if (errorCode!=SCARD_S_SUCCESS) {
        SetErrorCode(errorCode);
        return errorCode;
    };
    return rCode;
}
class SCard_Direct: public SyncTestObj {
public:
    SCard_Direct(SCardFunction * pcCard,
            LPCTSTR lpReader,
            DWORD dwTestCase,
            SCARDCONTEXT hContext,
            CEvent * pcEvent,
            DWORD dwSyncCount): 
        SyncTestObj(pcEvent,dwSyncCount),
        m_lpReader(lpReader),m_hContext(hContext)
    {m_pcCard=pcCard;m_dwTestCase=dwTestCase;};
    virtual DWORD RunTest ();
private:
    SCardFunction *m_pcCard;
    DWORD m_dwTestCase;
    SCARDCONTEXT m_hContext;
    LPCTSTR m_lpReader;
};

DWORD SCard_Direct::RunTest()
{
    SCARDHANDLE hCard;
    DWORD dwActiveProtocol;
    CheckLock();
    CheckLock();
    DWORD errorCode=SCardEstablishContext(SCARD_SCOPE_SYSTEM, 
                              NULL,
                              NULL,
                              &m_hContext);
    if (errorCode!=SCARD_S_SUCCESS) {
        SetErrorCode(errorCode);
        g_pKato->Log( LOG_FAIL,TEXT("SCardEstablishContext return %lx, Failure "),errorCode);
        return errorCode;
    };
    CheckLock();
    errorCode=SCardConnect(m_hContext,m_lpReader,SCARD_SHARE_EXCLUSIVE ,
        SCARD_PROTOCOL_T0|SCARD_PROTOCOL_T1,&hCard,&dwActiveProtocol);

    if (errorCode==SCARD_E_READER_UNAVAILABLE || errorCode==SCARD_E_SHARING_VIOLATION) {
        g_pKato->Log( LOG_SKIP,TEXT("SCardConnect return Unavailable for reader %s! Test Skipped"),m_lpReader);
        SCardReleaseContext(m_hContext);
        return 0;       
    };
    if (errorCode!=SCARD_S_SUCCESS) {
        g_pKato->Log( LOG_SKIP,TEXT("SCardConnect return 0x%lx for reader %s"),errorCode,m_lpReader);
        SetErrorCode(errorCode);
        SCardReleaseContext(m_hContext);
        return errorCode;
    };

    BYTE    bOutBuffer[SCARD_ATR_LENGTH];
    DWORD   cbOutBufferSize = SCARD_ATR_LENGTH;
    DWORD   cbBytesReturned;

    CheckLock();
    errorCode= SCardControl(hCard, IOCTL_SMARTCARD_GET_STATE , NULL, 0,
                    (LPVOID)bOutBuffer, cbOutBufferSize, &cbBytesReturned);

    DWORD rCode=0;

    if (errorCode!=SCARD_S_SUCCESS) {
        g_pKato->Log( LOG_FAIL,TEXT("SCardControl for card %s Failure errorcode=%lx"),m_pcCard->GetSCardProvider(),errorCode);
        SetErrorCode(rCode=errorCode);
    }
    else {
        CheckLock();
        errorCode= SCardGetAttrib(hCard, SCARD_ATTR_CURRENT_PROTOCOL_TYPE,
            (LPBYTE)bOutBuffer, &cbOutBufferSize);
        if (errorCode!=SCARD_S_SUCCESS) {
            g_pKato->Log( LOG_FAIL,TEXT("SCardGetAttrib for card %s Failure errorcode=%lx"),m_pcCard->GetSCardProvider(),errorCode);
            SetErrorCode(rCode=errorCode);
        }
        else {
            if (!cbOutBufferSize)   // something wrong.
                SetErrorCode(rCode=errorCode=(DWORD)-1);
            else {
                CheckLock();
                errorCode= SCardSetAttrib(hCard, SCARD_ATTR_CURRENT_PROTOCOL_TYPE,
                    (LPCBYTE)bOutBuffer, cbOutBufferSize);
                if (errorCode!=SCARD_S_SUCCESS && errorCode!=ERROR_NOT_SUPPORTED) {
                    g_pKato->Log( LOG_FAIL,TEXT("SCardSetAttr for card %s Failure errorcode=%lx"),m_pcCard->GetSCardProvider(),errorCode);
                    SetErrorCode(rCode=errorCode);
                }
            };
        };
    };

    CheckLock();
    errorCode=SCardDisconnect(hCard,SCARD_LEAVE_CARD);
    SCardReleaseContext(m_hContext);
    if (errorCode!=SCARD_S_SUCCESS) {
        g_pKato->Log( LOG_FAIL,TEXT("SCardDisconnect for card %s Failure errorcode=%lx"),m_pcCard->GetSCardProvider(),errorCode);
        SetErrorCode(errorCode);
        return errorCode;
    };
    return rCode;
}

class RunInThread : public MiniThread {
public:
    RunInThread(TestObj * pTestObj) { m_pTestObj=pTestObj; };
private:
    virtual DWORD ThreadRun();
    TestObj *m_pTestObj;
};
DWORD RunInThread::ThreadRun()
{
    ASSERT(m_pTestObj);
    return m_pTestObj->RunTest();
};

TESTPROCAPI SCardMulti_Test(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) 
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


    // Initial
    SCARDCONTEXT hContext;
    DWORD lCallReturn=SCardEstablishContext(SCARD_SCOPE_SYSTEM, 
                              NULL,
                              NULL,
                              &hContext);
    if (lCallReturn!=SCARD_S_SUCCESS) {
        g_pKato->Log( LOG_FAIL,TEXT("SCardEstablishContext return %lx, Failure "),lCallReturn);
        return TPR_FAIL;
    };
    

    TCHAR mszCards[NAME_BUFFER_SIZE];
    mszCards[0]=0;

    SCardContainer cardArray;
    SCardFunction * oneCard;
    cardArray.AddedSCard(oneCard =new AmmiSCard()); oneCard->SCardIntroduceCardType(hContext);MakeMSZ( mszCards, (LPTSTR)oneCard->GetSCardProvider() );
    cardArray.AddedSCard(oneCard =new BullSCard()); oneCard->SCardIntroduceCardType(hContext);MakeMSZ( mszCards, (LPTSTR)oneCard->GetSCardProvider() );
    cardArray.AddedSCard(oneCard =new GieseckeSCard()); oneCard->SCardIntroduceCardType(hContext);MakeMSZ( mszCards, (LPTSTR)oneCard->GetSCardProvider() );
    cardArray.AddedSCard(oneCard =new IbmSCard()); oneCard->SCardIntroduceCardType(hContext);MakeMSZ( mszCards, (LPTSTR)oneCard->GetSCardProvider() );
    cardArray.AddedSCard(oneCard =new SchlumbgrSCard()); oneCard->SCardIntroduceCardType(hContext);MakeMSZ( mszCards, (LPTSTR)oneCard->GetSCardProvider() );
    cardArray.AddedSCard(oneCard =new SniSCard()); oneCard->SCardIntroduceCardType(hContext);MakeMSZ( mszCards, (LPTSTR)oneCard->GetSCardProvider() );
    cardArray.AddedSCard(oneCard =new GieseckeSCardV2()); oneCard->SCardIntroduceCardType(hContext);MakeMSZ( mszCards, (LPTSTR)oneCard->GetSCardProvider() );

    TCHAR mszReaders[NAME_BUFFER_SIZE];
    DWORD cmszReaders = NAME_BUFFER_SIZE;
    mszReaders[0]=0;
    lCallReturn = SCardListReaders(NULL,        // no particular context
            NULL,                           
            mszReaders,                             // buffer to receive reader names
            &cmszReaders);                          // size of buffer, size of returned multistring

    SCardFunction * foundCards[MAX_READERS];
    LPCTSTR lpAtReader[MAX_READERS];
    DWORD dwcFoundCards=0;
    BOOL fFailed=FALSE;

    if(SCARD_S_SUCCESS == lCallReturn)
    {
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

        if ((lCallReturn=SCardLocateCards(hContext, mszCards, gReaderStates, cReaders))==SCARD_S_SUCCESS){ // may have passed
            for (DWORD readerIndex=0;readerIndex<cReaders;readerIndex++) {
                // check readerstate to see if atr matched card requested
                if ( gReaderStates[readerIndex].dwEventState & SCARD_STATE_ATRMATCH) {// PASS
                    oneCard=cardArray.FoundCardByAtr(gReaderStates[readerIndex].rgbAtr);
                    if (oneCard) {
                        g_pKato->Log( LOG_DETAIL,TEXT(" Locate Card %s at Reader %s"),
                            oneCard->GetSCardProvider(),gReaderStates[readerIndex].szReader);
                        if (dwcFoundCards<MAX_READERS) {
                            foundCards[dwcFoundCards]=oneCard;
                            lpAtReader[dwcFoundCards]=gReaderStates[readerIndex].szReader;
                            dwcFoundCards++;
                        };
                    }
                    else {
                        g_pKato->Log( LOG_FAIL,TEXT(" Unknown card at Reader %s"),
                                gReaderStates[readerIndex].szReader);
                        fFailed=TRUE;
                    };
                }
                else
                // check readerstate to see if atr matched card requested
                if ((SCARD_STATE_UNKNOWN|SCARD_STATE_UNAVAILABLE|SCARD_STATE_EXCLUSIVE|SCARD_STATE_INUSE) & gReaderStates[readerIndex].dwEventState) {
                    g_pKato->Log( LOG_DETAIL,TEXT(" Reader %s is can not be used currently. Status:0x%x"),
                                gReaderStates[readerIndex].szReader,gReaderStates[readerIndex].dwEventState);
                                fFailed = TRUE;
                }
                else {
                    g_pKato->Log( LOG_FAIL,TEXT(" Unknown card at Reader %s,dwEventState:%lx"),
                                gReaderStates[readerIndex].szReader,gReaderStates[readerIndex].dwEventState);
                        fFailed=TRUE;
                };
            };
        }
        else {
            g_pKato->Log(LOG_SKIP,TEXT("SCardLocateCards Failure 0x%lx: Reader & Card may be used by other processor"), lCallReturn);
            dwcFoundCards=0;
            fFailed = TRUE;
        };

        // test case 1;
        if (!fFailed && dwcFoundCards) {
            CEvent cEvent(FALSE,TRUE,NULL,NULL);
            TestObj * testObjs[MAX_READERS];
            RunInThread * testThreads[MAX_READERS];
            for (DWORD testCase=1; testCase<10 && !fFailed;testCase++) {
                cEvent.ResetEvent();
                for (DWORD index=0;index<dwcFoundCards && index<MAX_READERS;index++) {
                    switch ( lpFTE->dwUniqueID) {
                    default:
                        testObjs[index]= new SCard_Query(foundCards[index],testCase,
                            hContext,&cEvent,testCase);
                    break;
                    case BASE+3:
                        testObjs[index] = new SCard_Tracking(foundCards[index],
                            lpAtReader[index],
                            testCase,hContext,&cEvent,testCase);
                        break;
                    case BASE+4:
                        testObjs[index] = new SCard_Access(foundCards[index],
                            lpAtReader[index],
                            testCase,hContext,&cEvent,testCase);
                        break;
                    case BASE+5:
                        testObjs[index] = new SCard_Direct(foundCards[index],
                            lpAtReader[index],
                            testCase,hContext,&cEvent,testCase);
                        break;
                    }
                    testThreads[index] = new RunInThread(testObjs[index]);
                };
                Sleep(1000); // let every one ready.
                cEvent.SetEvent();
        
                for (index=0;index<dwcFoundCards && index<MAX_READERS;index++) {
                    testThreads[index]->ThreadTerminated(20000);
                    delete testThreads[index];
                    DWORD errorCode=testObjs[index]->GetErrorCode();
                    delete testObjs[index];
                    if (errorCode) {
                        g_pKato->Log( LOG_FAIL,TEXT(" Error occurred In Test Case %d thread %ld, ErrorCode %lx"),testCase,index,errorCode);
                        fFailed=TRUE;
                    };
                };
            };
        };
      // cleanup
        for (DWORD index=0;index<cardArray.GetSCardNumber();index++) {
            oneCard=cardArray.GetSCardByIndex(index);
            if(oneCard)
                oneCard->SCardForgetCardType(hContext);
        };
    }
    else
    {
        g_pKato->Log(LOG_FAIL, TEXT("SCardListReaders failed; status [%x]"), lCallReturn);
        fFailed = TRUE;
    }

    if (SCARD_S_SUCCESS != (lCallReturn=SCardReleaseContext(hContext) ) )  {
       g_pKato->Log(LOG_FAIL, TEXT("Could not release Smartcard Context: [%x]."), lCallReturn);
       // return context variable to neutral state
    }
    return (fFailed?TPR_FAIL:TPR_PASS);
};



