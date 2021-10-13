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
//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  File:       ifdtest.h
//
//--------------------------------------------------------------------------
#ifndef IFDTEST_H
#define IFDTEST_H

#include <winsmcrd.h>
#include <afxutil.h>
#include <kato.h>
#include "katolog.h"



#undef  WTT_ENABLED


#define min(a, b)  (((a) < (b)) ? (a) : (b)) 

// Status codes
#define IFDSTATUS_SUCCESS       0
#define IFDSTATUS_FAILED        1
#define IFDSTATUS_CARD_UNKNOWN  2
#define IFDSTATUS_TEST_UNKNOWN  3
#define IFDSTATUS_NO_PROVIDER   4
#define IFDSTATUS_NO_FUNCTION   5
#define IFDSTATUS_END           7

// query codes
#define IFDQUERY_CARD_TESTED    0
#define IFDQUERY_CARD_NAME      1
#define IFDQUERY_TEST_RESULT    2

#define READER_TYPE_WDM         L"WDM PnP"
#define READER_TYPE_NT          L"NT 4.00"
#define READER_TYPE_VXD         L"Win9x VxD"
#define READER_TYPE_CE          L"CE"

#define OS_WINNT4               L"Windows NT 4.0"
#define OS_WINNT5               L"Windows NT 5.0"
#define OS_WIN95                L"Windows 95"
#define OS_WIN98                L"Windows 98"
#define OS_WINCE                L"Windows Embedded CE"

const DWORD MAX_THREAD_TIMEOUT = CLOCKS_PER_SEC * 60 * 5; // 5 min


// Exception classes

class CIfdTestException 
{
public:
    
    CIfdTestException(LPCTSTR in_pMsg): m_pMsg(in_pMsg) {}
    CString GetMessage() { return m_pMsg; }

protected:

    CIfdTestException() {}
    void SetMessage(LPCTSTR in_pMsg) { m_pMsg= in_pMsg; }

private:
    
    CString m_pMsg;
};

class CStringResourceNotFoundException: public CIfdTestException {
public:
    CStringResourceNotFoundException(UINT l_uId);
};

class CWttLoggerException: public CIfdTestException {
public:
    CWttLoggerException(HRESULT in_hr);
};

// Prototypes 


CString TestLoadStringResource(UINT id);
void LogMessage(__in DWORD in_dwVerbosity, __in UINT in_uFormat, ...);
void LogOpen(__in LPCTSTR in_pchLogFile);
void LogClose(void);
void TestStart(__in UINT in_uFormat,  ...);
void TestCheck(BOOL in_bResult, __in UINT in_uFormat, ...);
void TestEnd(void);
BOOLEAN TestFailed(void);
BOOLEAN ReaderFailed(void);
void ResetReaderFailedFlag(void);
CString & GetOperatingSystem(void);

// below are the deprecated calls till we port all strings back to resources
void LogMessage(__in DWORD in_dwVerbosity, __in LPCTSTR in_pchFormat, ...);

void
TestCheck(
    ULONG in_lResult,
    __in LPCTSTR in_pchOperator,
    __in_bcount(in_uExpectedLength) const ULONG in_uExpectedResult,
    ULONG in_uResultLength,
    ULONG in_uExpectedLength,
    UCHAR in_chSw1,
    UCHAR in_chSw2,
    UCHAR in_chExpectedSw1,
    UCHAR in_chExpectedSw2,
    const UCHAR* in_pchData,
    const UCHAR* in_pchExpectedData,
    ULONG  in_uDataLength
    );

extern "C" {

    LONG MapWinErrorToNtStatus(ULONG in_uErrorCode);
}

//
// some useful macros
//
#define TEST_END() {TestEnd(); if(TestFailed()) return IFDSTATUS_FAILED;}

#define TEST_CHECK_SUCCESS(Text, Result) \
TestCheck( \
    Result == ERROR_SUCCESS, \
    IDS_IFD_CHECK_TEST_SUCCESS_MSG, \
    Text, \
    Result, \
    MapWinErrorToNtStatus(Result) \
    ); 

#define TEST_CHECK_NOT_SUPPORTED(Text, Result) \
TestCheck( \
    Result == ERROR_NOT_SUPPORTED, \
    IDS_IFD_CHECK_NOT_SUPPORTED_MSG, \
    Text, \
    Result, \
    MapWinErrorToNtStatus(Result), \
    ERROR_NOT_SUPPORTED, \
    MapWinErrorToNtStatus(ERROR_NOT_SUPPORTED) \
    ); 
//
// Class definitions
//
class CAtr {

    UCHAR m_rgbAtr[SCARD_ATR_LENGTH];
    ULONG m_uAtrLength;
     
public:
    CAtr() {
                                           
        m_uAtrLength = 0;
        memset(m_rgbAtr, 0, SCARD_ATR_LENGTH);
    }

    CAtr(
        const BYTE in_rgbAtr[], 
        ULONG in_uAtrLength
        )
    {
        *this = CAtr();
        m_uAtrLength = min(SCARD_ATR_LENGTH, in_uAtrLength);
        memcpy(m_rgbAtr, in_rgbAtr, m_uAtrLength);
    }

    // In usage, the buffer passed to GetAtrString is always 256 bytes
    // This is assumed, as an ATR is unpacked into this buffer with no other
    // knowledge of the length of the buffer.
    PWCHAR GetAtrString(__out_ecount(256) PWCHAR io_pchBuffer);
    
    PBYTE GetAtr(PBYTE *io_pchBuffer, PULONG io_puAtrLength) {
         
        *io_pchBuffer = (PBYTE) m_rgbAtr;
        *io_puAtrLength = m_uAtrLength;
        return (PBYTE) m_rgbAtr;
    }

    ULONG GetLength() const {

        return m_uAtrLength;
    }

    BOOLEAN operator==(const CAtr& a) const {

        return (m_uAtrLength && 
            a.m_uAtrLength == m_uAtrLength && 
            memcmp(m_rgbAtr, a.m_rgbAtr, m_uAtrLength) == 0);
    }

    BOOLEAN operator!=(const CAtr& a) const {

        return !(*this == a);
    }
};

class CReader {

    // device name. E.g. SCReader0
    CString m_CDeviceName;

    // Name of the reader to be tested. E.g. Bull
    CString m_CVendorName;

    // Name of the reader to be tested. E.g. Bull
    CString m_CIfdType;

    // Atr of the current card
    class CAtr m_CAtr;

    // handle to the reader device
    HANDLE m_hReader;

    // io-request struct used for transmissions
    SCARD_IO_REQUEST m_ScardIoRequest;

    // Storage area for smart card i/o
    UCHAR m_rgbReplyBuffer[1024];

    // size of the reply buffer
    ULONG m_uReplyBufferSize;

    // Number of bytes returned by the card
    ULONG m_uReplyLength;

    // function used by WaitForCardInsertion|Removal
    LONG WaitForCard(const ULONG);

    LONG StartWaitForCard(const ULONG);

    LONG PowerCard(ULONG in_uMinorIoControlCode);

    BOOLEAN m_fDump;

public:
    CReader();

    // Close reader
    BOOL Close(void);

    // power functions
    LONG CReader::ColdResetCard(void) {

        return PowerCard(SCARD_COLD_RESET);     
    }  

    LONG CReader::WarmResetCard(void) {

        return PowerCard(SCARD_WARM_RESET);     
    }  

    LONG CReader::PowerDownCard(void) {

        return PowerCard(SCARD_POWER_DOWN);     
    }  

    PBYTE GetAtr(PBYTE *io_pchBuffer, PULONG io_puAtrLength) {
         
        return m_CAtr.GetAtr(io_pchBuffer, io_puAtrLength);
    }

    // in practice, the buffer passed to GetAtrString is always 256 bytes, an
    // assumed length used to unpack an ATR into string format.
    PWCHAR GetAtrString(__in_ecount(256) PWCHAR io_pchBuffer) {
         
        return m_CAtr.GetAtrString(io_pchBuffer);
    }

    HANDLE GetHandle(void) {
         
        return m_hReader;
    }

    CString &GetDeviceName(void) {
         
        return m_CDeviceName;
    }

    LONG VendorIoctl(PUCHAR* o_ppInternalBuffer, ULONG* o_uLen);

    CString &GetVendorName(void);
    CString &GetIfdType(void);
    ULONG GetDeviceUnit(void);
    LONG GetState(PULONG io_puState);

    // Open the reader
    BOOLEAN Open(
        const CString &in_CReaderName
        );

    // (Re)Open reader using the existing name
    BOOLEAN Open(void);

    //
    // Set size of the reply buffer
    // (Only for testing purposes)
    //
    void SetReplyBufferSize(ULONG in_uSize) {
         
        if (in_uSize > sizeof(m_rgbReplyBuffer)) {

            m_uReplyBufferSize = sizeof(m_rgbReplyBuffer);

        } else {
             
            m_uReplyBufferSize = in_uSize;
        }
    }

    // assigns an ATR
    void SetAtr(const BYTE* const in_pchAtr, ULONG in_uAtrLength) {

        m_CAtr = CAtr(in_pchAtr, in_uAtrLength);         
    }

    // returns the ATR of the current card
    class CAtr &GetAtr() {

        return m_CAtr;     
    }

    // set protocol to be used
    LONG SetProtocol(const ULONG in_uProtocol);

    // transmits an APDU to the reader/card
    DWORD Transmit(
        const UCHAR *in_pchRequest,
        ULONG in_uRequestLength,
        PUCHAR *out_pchReply,
        PULONG out_puReplyLength
        );

    // wait to insert card
    LONG WaitForCardInsertion() {
         
        return WaitForCard(IOCTL_SMARTCARD_IS_PRESENT);
    };

    // wait to remove card
    LONG WaitForCardRemoval() {
         
        return WaitForCard(IOCTL_SMARTCARD_IS_ABSENT);
    };

    void SetDump(BOOLEAN in_fOn) {
         
        m_fDump = in_fOn;
    }
};

class CCardProvider {
     
    // Start of list pointer
    static class CCardProvider *s_pFirst;

    // Pointer to next provider
    class CCardProvider *m_pNext;

    // name of the card to be tested
    CString m_CCardName;

    // atr of this card
    CAtr m_CAtr[3];

    // test no to run
    ULONG m_uTestNo;

    // max number of tests
    ULONG m_uTestMax;

    // This flag indicates that the card test was unsuccessful
    BOOLEAN m_bTestFailed;

    // This flag indicates that the card has been tested
    BOOLEAN m_bCardTested;

    // set protocol function
    ULONG ((*m_pSetProtocol)(class CCardProvider&, class CReader&));

    // set protocol function
    ULONG ((*m_pCardTest)(class CCardProvider&, class CReader&));

public:

    // Constructor
    CCardProvider(void);

    // Constructor to be used by plug-in 
    CCardProvider(void (*pEntryFunction)(class CCardProvider&));

    // Method that mangages all card tests
    static void CardTest(class CReader&, ULONG in_uTestNo);

    // return if there are still untested cards
    static BOOLEAN CardsUntested(void);

    // Reset all cards to "untested" state.
    static void SetAllCardsUntested(void);
    
    // List all cards that have not been tested
    static void ListUntestedCards(void);

    // Assigns a friendly name to a card
    void SetCardName(__in const WCHAR in_rgchCardName[]);

    // Set ATR of the card
    void SetAtr(const BYTE* const in_rgbAtr, ULONG in_uAtrLength);

    // Assign callback functions
    void SetProtocol(ULONG ((in_pFunction)(class CCardProvider&, class CReader&))) {
         
        m_pSetProtocol = in_pFunction;
    }

    void SetCardTest(ULONG ((in_pFunction)(class CCardProvider&, class CReader&))) {
         
        m_pCardTest = in_pFunction;
    }

    // returns the test number to perform
    ULONG GetTestNo(void) const {
         
        return m_uTestNo;
    }

    BOOL IsValidAtr(const CAtr& in_CAtr) const {

        for (int i = 0; i < MAX_NUM_ATR; i++) {

            if (m_CAtr[i] == in_CAtr) {

                return TRUE;
            }
        }
        return FALSE;
    }
};

// represents a list of all installed readers
class CReaderList {

    // number of constructor calls to avoid multiple build of reader list
    static ULONG m_uRefCount;

    // number of currently installed readers
    static ULONG m_uNumReaders;

    // pointer to array of reader list
    static class CReaderList **m_pList;

    ULONG m_uCurrentReader;

    CString m_CDeviceName;
    CString m_CPnPType;
    CString m_CVendorName;
    CString m_CIfdType;

public:

    CReaderList();
    CReaderList(
        const CString &in_CDeviceName,
        const CString &in_CPnPType,
        const CString &in_CVendorName,
        const CString &in_CIfdType
        );
    ~CReaderList();
    
    static void AddDevice(
        CString in_pchDeviceName,
        CString in_pchPnPType
        );

    static CString &GetVendorName(ULONG in_uIndex);
    static CString &GetDeviceName(ULONG in_uIndex);
    static CString &GetIfdType(ULONG in_uIndex);
    static CString &GetPnPType(ULONG in_uIndex);

    static ULONG GetNumReaders(void) {
         
        return m_uNumReaders;
    }
};

// This structure represents the T=0 result file of a smart card
typedef struct _T0_RESULT_FILE_HEADER {
     
    // Offset to first test result
    UCHAR Offset;

    // Number of times the card has been reset
    UCHAR CardResetCount;

    // Version number of this card
    UCHAR CardMajorVersion;
    UCHAR CardMinorVersion;

} T0_RESULT_FILE_HEADER, *PT0_RESULT_FILE_HEADER;

typedef struct _T0_RESULT_FILE {

    //
    // The following structures store the results
    // of the tests. Each result comes with the 
    // reset count when the test was performed.
    // This is used to make sure that we read not
    // the result from an old test, maybe even 
    // performed with another reader/driver.
    //
    struct {

        UCHAR Result;
        UCHAR ResetCount;     

    } TransferAllBytes;

    struct {

        UCHAR Result;
        UCHAR ResetCount;     

    } TransferNextByte;

    struct {

        UCHAR Result;
        UCHAR ResetCount;     

    } Read256Bytes;

    struct {

        UCHAR Result;
        UCHAR ResetCount;     

    } Case1Apdu;

    struct {

        UCHAR Result;
        UCHAR ResetCount;     

    } RestartWorkWaitingTime;

    struct {

        UCHAR Result;
        UCHAR ResetCount;     

    } PTS;

    struct {

        UCHAR Result;
        UCHAR ResetCount;     

    } PTSDataCheck;

} T0_RESULT_FILE, *PT0_RESULT_FILE;


// Test Function Prototypes
//***************************************************************************
// Purpose:    Permits user to select a reader if more than one is attached.
// Arguments:
//   void
// Returns:
//   Name of reader driver selected.
//***************************************************************************
CString & SelectReader(void);

//***************************************************************************
// Purpose:    Responsible for running the card provider tests
// Arguments:
//   __in CReader &in_CReader       - Instance of card reader
//   __in BOOL    in_bTestOneCard   - Test only one card before exiting. This
//                                    one-card test is also known as "AutoIFD"
//                                    and is used to sanity check the driver.
//   __in ULONG   in_uTestNo        - Only run specified test on inserted card
//   __in BOOL    in_bReqCardRemoval- Require card removal after each card
//                                    is tested.  Set to TRUE when this test 
//                                    is used to test full deck of 5 cards.
//   __in BOOL    in_bStress        - Test in stress mode:
//                                    - Do not ask user to reinsert card if 
//                                      resetting card fails.
//
// Returns:
//   void - Test result is obtained via ReaderFailed().
//***************************************************************************
void 
CardProviderTest(
    CReader &in_CReader,
    BOOL    in_bTestOneCard,
    ULONG   in_uTestNo,
    BOOL    in_bReqCardRemoval,
    BOOL    in_bStress
    );


//***************************************************************************
// Purpose:    Thread that schedules the system to suspend and then wakeup
//             after 15 seconds.
// Arguments:
//   __in VOID * in_lpvParam - Pointer to DWORD specifying amount of time
//                             (in seconds) to wait before suspending.
//
// Returns:
//   The result of CReader::Open().
//***************************************************************************
DWORD WINAPI
SuspendDeviceAndScheduleWakeupThread(const VOID * const in_lpvParam);

#include "commonrc.h"
#endif IFDTEST_H