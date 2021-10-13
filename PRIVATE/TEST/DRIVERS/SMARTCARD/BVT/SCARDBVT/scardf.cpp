//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "stdafx.h"
#ifndef UNDER_CE
#include <tchar.h>
#endif
#include <winscard.h>
#include "scardf.h"
#include "tuxbvt.h"

BYTE AtrMask[0x20]=
{ 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff };

extern CKato *g_pKato;

SCardFunction::SCardFunction(LPCBYTE pbAtr,LPCBYTE pbAtrMask,DWORD cbAtrLen)
{
    if (pbAtr && cbAtrLen) {
        m_pbAtr= new BYTE [cbAtrLen];
        memcpy(m_pbAtr,pbAtr,cbAtrLen);
    }
    else
        m_pbAtr=NULL;
    if (pbAtrMask && cbAtrLen) {
        m_pbAtrMask= new BYTE [cbAtrLen];
        memcpy(m_pbAtrMask,pbAtrMask,cbAtrLen);
    }
    else
        m_pbAtrMask=NULL;

    m_cbAtrLen=cbAtrLen;
}
SCardFunction::~SCardFunction()
{
    if (m_pbAtr)
        delete m_pbAtr;
    if (m_pbAtrMask)
        delete m_pbAtrMask;
};
LONG SCardFunction::SCardIntroduceCardType(SCARDCONTEXT hContext)
{
    LONG lReturn=::SCardIntroduceCardType(
            hContext,
            GetSCardProvider(),
            GetGuidPrimary(),
            GetGuidInterface(),
            1,
            m_pbAtr,m_pbAtrMask,m_cbAtrLen);
        g_pKato->Log(LOG_DETAIL, TEXT("SCardIntroduceCardType - return(%ld) for card %s "),
                   lReturn,GetSCardProvider());
    return lReturn;

};

LONG SCardFunction::SCardForgetCardType(SCARDCONTEXT hContext)
{
    LONG lReturn=::SCardForgetCardType(hContext,GetSCardProvider());
        g_pKato->Log(LOG_DETAIL, TEXT("SCardForgetCardType - return(%ld) for card %s "),
                   lReturn,GetSCardProvider());
    return lReturn;
};

LONG SCardFunction::SCardCheckGetChallenge(SCARDHANDLE hCard,LPCSCARD_IO_REQUEST lpSCard_IoRequest,BOOL * pbCheckResult)
{
    BYTE    bRecvBuffer[SCARD_ATR_LENGTH];
    DWORD   cbRecvLength = SCARD_ATR_LENGTH;
    SCARD_IO_REQUEST outRequest=*lpSCard_IoRequest;

    LONG lcallReturn = 
        SCardTransmit(hCard,lpSCard_IoRequest, 
            (PUCHAR) "\xC0\x84\x00\x00\0x08",5, &outRequest, (LPBYTE)bRecvBuffer, &cbRecvLength);
        g_pKato->Log(LOG_DETAIL, TEXT("SCardTransmit - return(%ld) length(%ld),buffer8(%lx),buffer9(%lx) "),
                        lcallReturn,cbRecvLength,(DWORD)bRecvBuffer[0],(DWORD)bRecvBuffer[1]);

    if (lcallReturn== SCARD_S_SUCCESS) {
        *pbCheckResult=TRUE;
    };
    return lcallReturn;
}


SCardContainer::SCardContainer(DWORD dwSize,BOOL bAutoDelete)
{
    m_bAutoDelete=bAutoDelete;
    m_ArraySize=dwSize;
    m_Array=(SCardFunction **)malloc(m_ArraySize*sizeof(SCardFunction *));
    m_CurLength=0;
}
SCardContainer::~SCardContainer()
{
    if (m_bAutoDelete) {
        for (DWORD i=0;i<m_CurLength;i++) {
            delete *(m_Array+i);
        };
    }
    free(m_Array);
}
BOOL SCardContainer::AddedSCard(SCardFunction * aScardPtr)
{
    if (m_CurLength<m_ArraySize) {
        *(m_Array+m_CurLength)=aScardPtr;
        m_CurLength++;
        return TRUE;
    }
    else
        return FALSE;
}
SCardFunction * SCardContainer::FoundCardByAtr(LPBYTE lppbAtr)
{
    for (DWORD index=0;index<m_CurLength;index++) 
        if (memcmp(lppbAtr,(*(m_Array+index))->GetpbAtr(),(*(m_Array+index))->GetAtrLength())==0)
            return *(m_Array+index);
    return NULL;
}
SCardFunction * SCardContainer::FoundCardByProvider(LPCTSTR lpProvider)
{
    for (DWORD index=0;index<m_CurLength;index++) 
        if (_tcscmp(lpProvider,(*(m_Array+index))->GetSCardProvider())==0)
            return *(m_Array+index);
    return NULL;
}
SCardFunction * SCardContainer::FoundCardByType(SupportedSCardType cardType)
{
    for (DWORD index=0;index<m_CurLength;index++) 
        if ((*(m_Array+index))->GetCardType()==cardType)
            return *(m_Array+index);
    return NULL;
}

AmmiSCard::AmmiSCard() :
    SCardFunction(
    (PBYTE) "\x3b\x7e\x13\x00\x00\x80\x53\xff\xff\xff\x62\x00\xff\x71\xbf\x83\x03\x90\x00", 
        AtrMask,19)
{;}
LPCTSTR AmmiSCard::GetSCardProvider()
{
    return TEXT("AMMI");
}
LPGUID AmmiSCard::GetGuidPrimary()
{
    static GUID     guidPrimaryProvider = 
    { 0xc1fdf200, 0xa199, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    return &guidPrimaryProvider;
}        
LPGUID AmmiSCard::GetGuidInterface()
{
    static GUID     guidInterface = 
    { 0x6e8ec00, 0xa19a, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    return &guidInterface;
}

LONG AmmiSCard::SCardCheckTransmit(SCARDHANDLE hCard,LPCSCARD_IO_REQUEST lpSCard_IoRequest,BOOL * pbCheckResult)
{
    //TestStart("SELECT FILE EFptsDataCheck");
    SCARD_IO_REQUEST recvPci;
    BYTE RecvBuffer[0x10];
    DWORD BufLength=0x10;
    memcpy(&recvPci, lpSCard_IoRequest, lpSCard_IoRequest->cbPciLength);
    LONG lReturn=SCardTransmit(hCard,lpSCard_IoRequest,
        (PBYTE) "\x00\xa4\x00\x00\x02\x00\x10",
        7,
                &recvPci,
                RecvBuffer,&BufLength);
        g_pKato->Log(LOG_DETAIL, TEXT("SCardTransmit - return(%ld) length(%ld),buffer0(%lx),buffer1(%lx) "),
                        lReturn,BufLength,(DWORD)RecvBuffer[0],(DWORD)RecvBuffer[1]);

    if (lReturn== SCARD_S_SUCCESS )
        if (BufLength==2 && RecvBuffer[0]==0x61 && RecvBuffer[1]==0x15) 
            *pbCheckResult=TRUE;
        else
            *pbCheckResult=FALSE;
    return lReturn;
};

BullSCard::BullSCard() :
    SCardFunction((PBYTE)"\x3f\x67\x25\x00\x21\x20\x00\x0F\x68\x90\x00",
        AtrMask, 11)
{;};
LPCTSTR BullSCard::GetSCardProvider()
{
    return TEXT("Bull");
}
LPGUID BullSCard::GetGuidPrimary()
{
    static GUID     guidPrimaryProvider = 
    { 0xc1fdf201, 0xa199, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    return &guidPrimaryProvider;
}        
LPGUID BullSCard::GetGuidInterface()
{
    static GUID     guidInterface = 
    { 0x6e8ec01, 0xa19a, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    return &guidInterface;
}

LONG BullSCard::SCardCheckGetChallenge(SCARDHANDLE hCard,LPCSCARD_IO_REQUEST lpSCard_IoRequest,BOOL * pbCheckResult)
{
    BYTE    bRecvBuffer[SCARD_ATR_LENGTH];
    DWORD   cbRecvLength = SCARD_ATR_LENGTH;
    SCARD_IO_REQUEST outRequest=*lpSCard_IoRequest;

    LONG lcallReturn = 
        SCardTransmit(hCard,lpSCard_IoRequest, 
            (PUCHAR) "\xBC\xC4\x00\x00\x08",5, &outRequest, (LPBYTE)bRecvBuffer, &cbRecvLength);
        g_pKato->Log(LOG_DETAIL, TEXT("SCardTransmit - return(%ld) length(%ld),buffer8(%lx),buffer9(%lx) "),
                        lcallReturn,cbRecvLength,(DWORD)bRecvBuffer[0],(DWORD)bRecvBuffer[1]);

    if (lcallReturn== SCARD_S_SUCCESS) {
        if (cbRecvLength==10 && bRecvBuffer[8]==0x90 && bRecvBuffer[9]== 0x00)
            *pbCheckResult=TRUE;
        else
            *pbCheckResult=FALSE;
    };
    return lcallReturn;
}
LONG BullSCard::SCardCheckTransmit(SCARDHANDLE hCard,LPCSCARD_IO_REQUEST lpSCard_IoRequest,BOOL * pbCheckResult)
{
    return SCardCheckGetChallenge(hCard,lpSCard_IoRequest,pbCheckResult);
}

GieseckeSCard::GieseckeSCard():
    SCardFunction((PBYTE) "\x3b\xbf\x18\x00\x80\x31\x70\x35\x53\x54\x41\x52\x43\x4f\x53\x20\x53\x32\x31\x20\x43\x90\x00\x9b",
        AtrMask,24)
{
;
}
LPCTSTR GieseckeSCard::GetSCardProvider()
{
    return TEXT("Giesecke & Devrient America Inc.");
};
LPGUID GieseckeSCard::GetGuidPrimary()
{
    static GUID     guidPrimaryProvider = 
    { 0xc1fdf202, 0xa199, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    return &guidPrimaryProvider;
}        
LPGUID GieseckeSCard::GetGuidInterface()
{
    static GUID     guidInterface = 
    { 0x6e8ec02, 0xa19a, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    return &guidInterface;
}
LONG GieseckeSCard::SCardCheckTransmit(SCARDHANDLE hCard,LPCSCARD_IO_REQUEST lpSCard_IoRequest,BOOL * pbCheckResult)
{
    BYTE RecvBuffer[0x10];
    DWORD BufLength=0x10;
    SCARD_T0_REQUEST replyIO;
    ASSERT(lpSCard_IoRequest->cbPciLength<=sizeof(SCARD_T0_REQUEST));
    memcpy(&replyIO,lpSCard_IoRequest,lpSCard_IoRequest->cbPciLength);

    LONG lReturn=SCardTransmit(hCard,lpSCard_IoRequest,
        (PUCHAR) "\x00\xa4\x00\x00\x02\x00\x01",7,
                (LPSCARD_IO_REQUEST)&replyIO,
                RecvBuffer,&BufLength);
        g_pKato->Log(LOG_DETAIL, TEXT("SCardTransmit - return(%ld) length(%ld),buffer0(%lx),buffer1(%lx) "),
                        lReturn,BufLength,(DWORD)RecvBuffer[0],(DWORD)RecvBuffer[1]);

    if (lReturn== SCARD_S_SUCCESS )
        if (lpSCard_IoRequest == SCARD_PCI_T0)
            if (BufLength==2 && RecvBuffer[0]==0x61 && RecvBuffer[1]==0x09) 
                *pbCheckResult=TRUE;
            else
                *pbCheckResult=FALSE;
        else // T1
        if (lpSCard_IoRequest == SCARD_PCI_T1)
            if (BufLength==11 && RecvBuffer[9]==0x90 && RecvBuffer[10]==0x0) 
                *pbCheckResult=TRUE;
            else
                *pbCheckResult=FALSE;
        else
            *pbCheckResult=FALSE;
    return lReturn;
};

IbmSCard::IbmSCard():
    SCardFunction((PBYTE) "\x3b\xef\x00\xff\x81\x31\x86\x45\x49\x42\x4d\x20\x4d\x46\x43\x34\x30\x30\x30\x30\x38\x33\x31\x43",
        AtrMask,24)
{
;
}
LPCTSTR IbmSCard::GetSCardProvider()
{
    return TEXT("IBM");
};
LPGUID IbmSCard::GetGuidPrimary()
{
    static GUID     guidPrimaryProvider = 
    { 0xc1fdf203, 0xa199, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    return &guidPrimaryProvider;
}        
LPGUID IbmSCard::GetGuidInterface()
{
    static GUID     guidInterface = 
    { 0x6e8ec03, 0xa19a, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    return &guidInterface;
}
LONG IbmSCard::SCardCheckTransmit(SCARDHANDLE hCard,LPCSCARD_IO_REQUEST lpSCard_IoRequest,BOOL * pbCheckResult)
{
    BYTE RecvBuffer[0x20];
    DWORD BufLength=0x20;
    ASSERT(lpSCard_IoRequest==SCARD_PCI_T1);
    SCARD_T1_REQUEST replyIO;
    replyIO=*(LPSCARD_T1_REQUEST)lpSCard_IoRequest;
    LONG lReturn=SCardTransmit(hCard,lpSCard_IoRequest,
        (PUCHAR) "\xa4\xa4\x00\x00\x02\x00\x07",7,
                (LPSCARD_IO_REQUEST)&replyIO,
                RecvBuffer,&BufLength);
        g_pKato->Log(LOG_DETAIL, TEXT("SCardTransmit - return(%ld) length(%ld),buffer14(%lx),buffer15(%lx) "),
                        lReturn,BufLength,(DWORD)RecvBuffer[14],(DWORD)RecvBuffer[15]);

    if (lReturn== SCARD_S_SUCCESS )
        if (BufLength==16 && RecvBuffer[14]==0x90 && RecvBuffer[15]==0x00) 
            *pbCheckResult=TRUE;
        else
            *pbCheckResult=FALSE;
    return lReturn;
};

SchlumbgrSCard::SchlumbgrSCard():
    SCardFunction((PBYTE) "\x3b\xe2\x00\x00\x40\x20\x99\x01",AtrMask, 8)
{;
}

LPCTSTR SchlumbgrSCard::GetSCardProvider()
{
    return TEXT("Schlumberger");
};
LPGUID SchlumbgrSCard::GetGuidPrimary()
{
    static GUID     guidPrimaryProvider = 
    { 0xc1fdf204, 0xa199, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    return &guidPrimaryProvider;
}        
LPGUID SchlumbgrSCard::GetGuidInterface()
{
    static GUID     guidInterface = 
    { 0x6e8ec04, 0xa19a, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    return &guidInterface;
}

LONG SchlumbgrSCard::SCardCheckTransmit(SCARDHANDLE hCard,LPCSCARD_IO_REQUEST lpSCard_IoRequest,BOOL * pbCheckResult)
{
    BYTE RecvBuffer[0x20];
    DWORD BufLength=0x20;
    Sleep(1000); // Sleep a second befor transfer.
    LONG lReturn=SCardTransmit(hCard,lpSCard_IoRequest,
        (PUCHAR) "\x00\xa4\x00\x00\x02\xa0\x00",7,
                NULL,
                RecvBuffer,&BufLength);
        g_pKato->Log(LOG_DETAIL, TEXT("SCardTransmit - return(%ld) length(%ld),buffer14(%lx),buffer15(%lx) "),
                        lReturn,BufLength,(DWORD)RecvBuffer[0],(DWORD)RecvBuffer[1]);

    if (lReturn== SCARD_S_SUCCESS )
        if (BufLength==2 && RecvBuffer[0]==0x90 && RecvBuffer[1]==0x00) 
            *pbCheckResult=TRUE;
        else
            *pbCheckResult=FALSE;
    return lReturn;
};

SniSCard::SniSCard():
    SCardFunction((PBYTE) "\x3b\xef\x00\x00\x81\x31\x20\x49\x00\x5c\x50\x43\x54\x10\x27\xf8\xd2\x76\x00\x00\x38\x33\x00\x4d", 
        AtrMask,24)
{;}

LPCTSTR SniSCard::GetSCardProvider()
{
    return TEXT("SCENIC");
};
LPGUID SniSCard::GetGuidPrimary()
{
    static GUID     guidPrimaryProvider = 
    { 0xc1fdf205, 0xa199, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    return &guidPrimaryProvider;
}        
LPGUID SniSCard::GetGuidInterface()
{
    static GUID     guidInterface = 
    { 0x6e8ec05, 0xa19a, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    return &guidInterface;
}

LONG SniSCard::SCardCheckTransmit(SCARDHANDLE hCard,LPCSCARD_IO_REQUEST lpSCard_IoRequest,BOOL * pbCheckResult)
{
    BYTE RecvBuffer[0x100];
    DWORD BufLength=0x100;
    ASSERT(lpSCard_IoRequest==SCARD_PCI_T1);
    SCARD_T1_REQUEST replyIO;
    replyIO=*(LPSCARD_T1_REQUEST)lpSCard_IoRequest;
    LONG lReturn=SCardTransmit(hCard,lpSCard_IoRequest,
        (PUCHAR) "\x00\xa4\x08\x04\x04\x3e\x00\xA0\x00",9,
                (LPSCARD_IO_REQUEST)&replyIO,
                RecvBuffer,&BufLength);
        g_pKato->Log(LOG_DETAIL, TEXT("SCardTransmit - return(%ld) length(%ld),buffer0(%lx),buffer1(%lx) "),
                        lReturn,BufLength,(DWORD)RecvBuffer[0],(DWORD)RecvBuffer[1]);

    if (lReturn== SCARD_S_SUCCESS ) {
        if (BufLength==2 && RecvBuffer[0]==0x90 && RecvBuffer[1]==0x0) 
            *pbCheckResult=TRUE;
        else
            *pbCheckResult=FALSE;
    };
    return lReturn;
};

GieseckeSCardV2::GieseckeSCardV2():
    SCardFunction((PBYTE) "\x3b\xbf\x18\x00\xc0\x20\x31\x70\x52\x53\x54\x41\x52\x43\x4f\x53\x20\x53\x32\x31\x20\x43\x90\x00\x9c",
        AtrMask,25)
{
;
}
LPCTSTR GieseckeSCardV2::GetSCardProvider()
{
    return TEXT("Giesecke & Devrient America Inc. Starcos S2.1c");
};
LPGUID GieseckeSCardV2::GetGuidPrimary()
{
    static GUID     guidPrimaryProvider = 
    { 0xc1fdf202, 0xa199, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    return &guidPrimaryProvider;
}        
LPGUID GieseckeSCardV2::GetGuidInterface()
{
    static GUID     guidInterface = 
    { 0x6e8ec02, 0xa19a, 0x11d1, { 0x86, 0xbd, 0x0, 0xa0, 0xc9, 0x3, 0xaa, 0x9b } };
    return &guidInterface;
}
LONG GieseckeSCardV2::SCardCheckTransmit(SCARDHANDLE hCard,LPCSCARD_IO_REQUEST lpSCard_IoRequest,BOOL * pbCheckResult)
{
    BYTE RecvBuffer[0x10];
    DWORD BufLength=0x10;
    SCARD_T0_REQUEST replyIO;
    ASSERT(lpSCard_IoRequest->cbPciLength<=sizeof(SCARD_T0_REQUEST));
    memcpy(&replyIO,lpSCard_IoRequest,lpSCard_IoRequest->cbPciLength);

    LONG lReturn=SCardTransmit(hCard,lpSCard_IoRequest,
        (PUCHAR) "\x00\xa4\x00\x00\x02\x00\x01",7,
                (LPSCARD_IO_REQUEST)&replyIO,
                RecvBuffer,&BufLength);
        g_pKato->Log(LOG_DETAIL, TEXT("SCardTransmit - return(%ld) length(%ld),buffer0(%lx),buffer1(%lx) "),
                        lReturn,BufLength,(DWORD)RecvBuffer[0],(DWORD)RecvBuffer[1]);

    if (lReturn== SCARD_S_SUCCESS )
        if (lpSCard_IoRequest == SCARD_PCI_T0)
            if (BufLength==2 && RecvBuffer[0]==0x61 && RecvBuffer[1]==0x09) 
                *pbCheckResult=TRUE;
            else
                *pbCheckResult=FALSE;
        else // T1
        if (lpSCard_IoRequest == SCARD_PCI_T1)
            if (BufLength==11 && RecvBuffer[9]==0x90 && RecvBuffer[10]==0x0) 
                *pbCheckResult=TRUE;
            else
                *pbCheckResult=FALSE;
        else
            *pbCheckResult=FALSE;
    return lReturn;
};


typedef enum {SCARD_BEGIN,
SCARD_AMMI,SCARD_BULL,SCARD_GIESECKE,SCARD_IBM,SCARD_SCHLUMBGR,SCARD_SNI,
SCARD_END, SCARD_GIESECKE_V2} SupportedSCard;


SCardFunction * CreateExistSCardByATR(
            SCARDCONTEXT hContext,
            LPCTSTR szCardName,LPGUID pguidPrimaryProvider,LPGUID rgguidInterfaces,
            DWORD dwInterfaceCount)

{
    SCardFunction * sCard;
    for (int aCard=SCARD_BEGIN+1;aCard<SCARD_END;aCard++) {
        sCard=NULL;
        switch(aCard) {
          case SCARD_AMMI:
            sCard =new AmmiSCard();
            break;
          case SCARD_BULL:
            sCard =new BullSCard();
            break;
          case SCARD_GIESECKE:
            sCard =new GieseckeSCard();
            break;
          case SCARD_IBM:
            sCard = new IbmSCard();
            break;
          case SCARD_SCHLUMBGR:
            sCard=new SchlumbgrSCard();
            break;
          case SCARD_SNI:
            sCard = new SniSCard();
            break;
          case SCARD_GIESECKE_V2:
            sCard = new GieseckeSCardV2();
          default:
            sCard=NULL;
        };
        if (sCard && sCard->SCardIntroduceCardType(hContext)
                ==SCARD_S_SUCCESS)
            return sCard;
        if (sCard)
            delete sCard;
    };
    return NULL;
};
BOOL FindReaderAndIntroduce(LPTSTR lpPrefix,LPTSTR lpLastReaderName)
{
    BOOL bReturn=FALSE;
    // Establish Smartcard Context
    SCARDCONTEXT hContext;
    if ( SCARD_S_SUCCESS != SCardEstablishContext(SCARD_SCOPE_SYSTEM , NULL,NULL,&hContext) ) {
        ASSERT(FALSE);
        return FALSE;
    }
    for (UINT l_uIndex = 0; l_uIndex < 10; l_uIndex++) {

    TCHAR l_rgchDeviceName[128];

        if (l_uIndex!=0)
            _stprintf(l_rgchDeviceName, TEXT("SCR%d:"), l_uIndex);
        else
            _stprintf(l_rgchDeviceName, TEXT("SCR:"));

        HANDLE m_hReader = CreateFile(l_rgchDeviceName,GENERIC_READ | GENERIC_WRITE,
                0,NULL,OPEN_EXISTING,0,NULL );
        if (m_hReader != INVALID_HANDLE_VALUE ) {
            CloseHandle(m_hReader);
            TCHAR l_rgchFriendlyName[128];
            _stprintf(l_rgchFriendlyName,TEXT("%s%d"),lpPrefix,l_uIndex);

            if (SCardIntroduceReader( hContext,l_rgchFriendlyName,l_rgchDeviceName)!=SCARD_S_SUCCESS) {
                ASSERT(FALSE);
                bReturn=FALSE;
                break;
            }
            else {
                if (lpLastReaderName)
                    _tcscpy(lpLastReaderName,l_rgchFriendlyName);
                bReturn=TRUE;
            };
        };

    }
    SCardReleaseContext(hContext );
    return bReturn;
}
BOOL ForgetReader(LPTSTR lpPrefix)
{
    BOOL bReturn=TRUE;
   // Establish Smartcard Context
    SCARDCONTEXT hContext;
    if ( SCARD_S_SUCCESS != SCardEstablishContext(SCARD_SCOPE_SYSTEM , NULL,NULL,&hContext) ) {
        ASSERT(FALSE);
        return FALSE;
    }
    for (UINT l_uIndex = 0; l_uIndex < 10; l_uIndex++) {
        TCHAR l_rgchFriendlyName[128];
        _stprintf(l_rgchFriendlyName,TEXT("%s%d"),lpPrefix,l_uIndex);
        SCardForgetReader(hContext,l_rgchFriendlyName);
    }

    SCardReleaseContext(hContext );
    return bReturn;
}

LPTSTR sprintATR( LPTSTR lpBuffer, DWORD dwBufferLen, PBYTE pATR, DWORD dwATRLength)
{
    if (lpBuffer==NULL || dwBufferLen==0)
        return NULL;
    LPTSTR lpReturn=lpBuffer;
    while (3<dwBufferLen-1 && dwATRLength!=0) {
        _stprintf(lpBuffer,TEXT("%02X "),*pATR);
        lpBuffer+=3;
        dwBufferLen-=3;
        pATR+=1;
        dwATRLength-=1;
    }
    *lpBuffer=0;
    return lpReturn;
}
