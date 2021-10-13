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
    SCardf.h

Abstract:  
    Smart Card Function Support. Wrapper classes for each of our test smart cards
    Some cards are no longer available, but the code remains because some folks
    might still want to verify using an old set of WHQL test cards.    
    
Notes: 

    Info for obtaining test smart cards is available at: http://www.pcscworkgroup.com/
    

--*/
#ifndef __SCARDF_H_
#define __SCARDF_H_
typedef enum { SMART_CARD_UNKNOWN=0, SMART_CARD_AMMI,SMART_CARD_BULL,
    SMART_CARD_GIESECKE,SMART_CARD_IBM,SMART_CARD_SCHLUMBGR,SMART_CARD_SNI,
    SMART_CARD_GIESECKEV2
} SupportedSCardType;


class SCardFunction {

public:
    SCardFunction(LPCBYTE pbAtr,LPCBYTE pbAtrMask,DWORD cbAtrLen);
    virtual ~SCardFunction();
    virtual LONG SCardIntroduceCardType(SCARDCONTEXT hContext);
    virtual LPCTSTR GetSCardProvider () =0;
    virtual LPGUID GetGuidPrimary() { return NULL; };
    virtual LPGUID GetGuidInterface(){ return NULL; };

    virtual LPBYTE GetpbAtr() { return m_pbAtr; };
    virtual LPBYTE GetpbAtrMask() { return m_pbAtrMask; };
    virtual DWORD GetAtrLength() { return m_cbAtrLen; };
    LONG SCardForgetCardType(SCARDCONTEXT hContext); 
    virtual LONG SCardCheckGetChallenge(SCARDHANDLE,LPCSCARD_IO_REQUEST,BOOL * bCheckResult);
    virtual LONG SCardCheckTransmit(SCARDHANDLE,LPCSCARD_IO_REQUEST,BOOL * bCheckResult) {
        return  ERROR_NOT_SUPPORTED ;
    };
    virtual SupportedSCardType GetCardType() { return SMART_CARD_UNKNOWN; };
    virtual DWORD GetSupportedProtocols () { return 0; };
private:
    LPBYTE m_pbAtr;
    LPBYTE m_pbAtrMask;
    DWORD m_cbAtrLen;
};
#define SCARD_INITIAL_SIZE 0x100
class SCardContainer {
public:
    SCardContainer(DWORD dwSize=SCARD_INITIAL_SIZE,BOOL bAutoDelete=TRUE);
    ~SCardContainer();
    BOOL AddedSCard(SCardFunction * aScardPtr);
    SCardFunction * GetSCardByIndex(DWORD dwIndex) {
        return ((dwIndex<m_CurLength)? *(m_Array+dwIndex):NULL);
    };
    SCardFunction * FoundCardByAtr(LPBYTE lppbAtr);
    SCardFunction * FoundCardByProvider(LPCTSTR lpProvider);
    SCardFunction * FoundCardByType(SupportedSCardType cardType);
    DWORD GetSCardNumber() { return m_CurLength; };

private:
    SCardFunction ** m_Array;
    DWORD m_CurLength;
    DWORD m_ArraySize;
    BOOL m_bAutoDelete;
};

class AmmiSCard : public SCardFunction {
public:
    AmmiSCard();
    virtual LPCTSTR GetSCardProvider();
    virtual LPGUID GetGuidPrimary();
    virtual LPGUID GetGuidInterface();
    virtual LONG SCardCheckTransmit(SCARDHANDLE,LPCSCARD_IO_REQUEST,BOOL * bCheckResult);
    virtual SupportedSCardType GetCardType() { return SMART_CARD_AMMI; };
    virtual DWORD GetSupportedProtocols () { return SCARD_PROTOCOL_T0; };
};

class BullSCard: public SCardFunction {
public:
    BullSCard();
    virtual LPCTSTR GetSCardProvider();
    virtual LPGUID GetGuidPrimary();
    virtual LPGUID GetGuidInterface();
    virtual LONG SCardCheckGetChallenge(SCARDHANDLE,LPCSCARD_IO_REQUEST,BOOL * bCheckResult);
    virtual LONG SCardCheckTransmit(SCARDHANDLE,LPCSCARD_IO_REQUEST,BOOL * bCheckResult);
    virtual SupportedSCardType GetCardType()  { return SMART_CARD_BULL; };
    virtual DWORD GetSupportedProtocols () { return SCARD_PROTOCOL_T0; };
};
class GieseckeSCard: public SCardFunction {
public:
    GieseckeSCard();
    virtual LPCTSTR GetSCardProvider();
    virtual LPGUID GetGuidPrimary();
    virtual LPGUID GetGuidInterface();
    virtual LONG SCardCheckTransmit(SCARDHANDLE,LPCSCARD_IO_REQUEST,BOOL * bCheckResult);
    virtual SupportedSCardType GetCardType() { return SMART_CARD_GIESECKE; };
    virtual DWORD GetSupportedProtocols () { return SCARD_PROTOCOL_T0|SCARD_PROTOCOL_T1; };
};
class IbmSCard: public SCardFunction {
public:
    IbmSCard();
    virtual LPCTSTR GetSCardProvider();
    virtual LPGUID GetGuidPrimary();
    virtual LPGUID GetGuidInterface();
    virtual LONG SCardCheckTransmit(SCARDHANDLE,LPCSCARD_IO_REQUEST,BOOL * bCheckResult);
    virtual SupportedSCardType GetCardType() { return SMART_CARD_IBM; };
    virtual DWORD GetSupportedProtocols () { return SCARD_PROTOCOL_T1; };
};

class SchlumbgrSCard: public SCardFunction {
public:
    SchlumbgrSCard();
    virtual LPCTSTR GetSCardProvider();
    virtual LPGUID GetGuidPrimary();
    virtual LPGUID GetGuidInterface();
    virtual LONG SCardCheckTransmit(SCARDHANDLE,LPCSCARD_IO_REQUEST,BOOL * bCheckResult);
    virtual SupportedSCardType GetCardType() { return SMART_CARD_SCHLUMBGR; };
    virtual DWORD GetSupportedProtocols () { return SCARD_PROTOCOL_T0; };
};

class SniSCard: public SCardFunction {
public:
    SniSCard();
    virtual LPCTSTR GetSCardProvider();
    virtual LPGUID GetGuidPrimary();
    virtual LPGUID GetGuidInterface();
    virtual LONG SCardCheckTransmit(SCARDHANDLE,LPCSCARD_IO_REQUEST,BOOL * bCheckResult);
    virtual SupportedSCardType GetCardType() { return SMART_CARD_SNI; };
    virtual DWORD GetSupportedProtocols () { return SCARD_PROTOCOL_T1; };
};

class GieseckeSCardV2: public SCardFunction {
public:
    GieseckeSCardV2();
    virtual LPCTSTR GetSCardProvider();
    virtual LPGUID GetGuidPrimary();
    virtual LPGUID GetGuidInterface();
    virtual LONG SCardCheckTransmit(SCARDHANDLE,LPCSCARD_IO_REQUEST,BOOL * bCheckResult);
    virtual SupportedSCardType GetCardType() { return SMART_CARD_GIESECKEV2; };
    virtual DWORD GetSupportedProtocols () { return SCARD_PROTOCOL_T0|SCARD_PROTOCOL_T1; };
};


extern BOOL FindReaderAndIntroduce(LPTSTR lpPrefix,LPTSTR lpLastReaderName=NULL);
extern BOOL ForgetReader(LPTSTR lpPrefix);
extern LPTSTR sprintATR( LPTSTR lpBuffer, DWORD dwBufferLen, PBYTE pATR, DWORD dwATRLength);
#endif
