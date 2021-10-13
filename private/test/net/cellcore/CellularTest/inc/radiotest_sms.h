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

#ifndef _radiotest_sms_h_
#define _radiotest_sms_h_

#include "sms.h"
#include "radiotest.h"

#define MAX_SMS_CHAR    160             // Use SMSDE_OPTIMAL codec
#define MAX_SMS_LEN     (MAX_SMS_CHAR * 2)
#define MAX_MSG_LENGTH  (1024 * 5)      // arbitrarily large buffer

const DWORD SMS_SEND_INTERVAL = 10*1000;                // 10 seconds
const DWORD SMS_P2P_TEST_USER2_MIN_TIMEOUT = 5*60*1000; // 5 minutes
const DWORD SMS_P2P_TEST_USER1_MIN_TIMEOUT = SMS_P2P_TEST_USER2_MIN_TIMEOUT + 2*60*1000; // 7 minutes
const DWORD SMS_P2P_TEST_TIMEOUT_PER_MSG = 20*1000;     // 20 seconds
const DWORD SMS_TEST_MAX_LONG_MSG_PARTS = 6;
const DWORD MAX_GSM_MULTI_PART_SMS_CHAR_PER_MSG = 153;  // 153 bytes per message
const DWORD SMS_PASS_RATE = 95;

/******************************************************************************/
//
//    END of user1 and user2 interactive.
//
/******************************************************************************/


enum SMS_MSGTYPE
{
    SMS_TEXT,
    SMS_NOTIFICATION,
    SMS_WDP,
    SMS_WCMP,
    SMS_STATUS,
    SMS_BROADCAST,
    SMS_RAW,
};

enum NET_TYPE
{
    NETWORK_TYPE_GSM,
    NETWORK_TYPE_CDMA,
};

typedef struct
{
    SMS_HANDLE          m_hSMS;
    HANDLE              m_hMsgEvent;
    SMS_MSGTYPE         m_MsgType;
    NET_TYPE            m_NetType;
    SMS_ADDRESS         saDestinationAddress;
    SMS_ADDRESS         saSMSCAddress;
    SMS_MESSAGE_ID      smsmidMsgID;
    SMS_DATA_ENCODING   sdeEncoding;

} SMS_DATA_T;


/////////////////////////////////////////////////////////////////////////
// Message structures for sending and receiving
/////////////////////////////////////////////////////////////////////////

// Send message structure
// Some members are pointers instead of objects because we need to test
// NULL pointer scenario.
typedef struct _SEND_MESSAGE_DATA
{
    SMS_MSGTYPE     smsmtMessageType;
    SMS_ADDRESS*    psmsaSMSCAddress;
    SMS_ADDRESS*    psmsaDestinationAddress;
    SYSTEMTIME*     pstValidityPeriod;
    BYTE*           pbData;
    DWORD           dwDataSize;
    BYTE*           pbPSD;
    DWORD           dwPSDSize;
    SMS_DATA_ENCODING smsdeDataEncoding;
    DWORD           dwOptions;
    SMS_MESSAGE_ID  smsmidMessageID;

} SEND_MESSAGE_DATA;


// Receive message structure
typedef struct _RECV_MESSAGE_DATA
{
    SMS_MSGTYPE     smsmtMessageType;
    SMS_ADDRESS     smsaSMSCAddress;
    SMS_ADDRESS     smsaSourceAddress;
    SYSTEMTIME      stReceiveTime;
    BYTE*           pbBuffer;
    DWORD           dwBufferBytesNeeded;
    BYTE*           pbPSD;
    DWORD           dwPSDSize;
    DWORD           dwBufferBytesUsed;

    // Ctor. Initialize pointers
    //    _RECV_MESSAGE_DATA(): pbBuffer( NULL ), pbPSD( NULL ) {}

} RECV_MESSAGE_DATA;


// SMS Interactive test data structure
struct SMSP2PTestData
{
    SMS_HANDLE  hSMS;
    HANDLE      hMsgEvent;
    HANDLE      hReceiveCompleteEvent;
    HANDLE      hExitEvent;
    SMS_ADDRESS saDestinationAddress;
    DWORD       dwTestMsgCount;
    DWORD       dwUser2Result;
    DWORD       dwReceivedSMSCount;
    PBYTE       pbReceivedFlag;
    BOOL        bReceivedResult;
};

extern void SetMessageReceiveFlag(PBYTE pBuf, DWORD dwSize, DWORD dwMessageID);
extern DWORD GetReceivedMessageCount(PBYTE pBuf, DWORD dwSize);
extern BOOL SendSMSTestMessage(
    SMS_HANDLE hSMS, 
    SMS_ADDRESS *pSMSAddr, 
    DWORD dwMessageCharCount,
    DWORD dwSessionID,
    DWORD dwMessageID);
extern void InitPSD(TEXT_PROVIDER_SPECIFIC_DATA* pTextPSD);
extern void InitMessage(SEND_MESSAGE_DATA* pMessageData,
                        SMS_MSGTYPE mtMsgType,
                        SMS_ADDRESS* psmsaDestinationAddress,
                        BYTE* pbData,
                        DWORD dwDataSize,
                        BYTE* pbPSD,
                        DWORD dwPSDSize
                        );
extern BOOL SMS_TestSendMessage(const SMS_HANDLE smshHandle, SEND_MESSAGE_DATA* pMessageData);

// Macro to simplify determining the number of elements in an array (do *not*
// use this macro for pointers)
#define ARRAY_LENGTH(x) (sizeof(x)/sizeof((x)[0]))

// Initialize threads etc
BOOL SMS_Init(VOID);
BOOL SMS_DeInit(VOID);
BOOL SMS_Summary(VOID);

// Tests
BOOL SMS_TestMultipart (DWORD cchMsgLen);

BOOL SMS_SendRecv(DWORD dwUserData);
BOOL SMS_LongMessage(DWORD dwUserData);

BOOL SMS_SendTestMsg ( INT iMessageIndex, 
                                const SMS_HANDLE smshHandle, 
                                SMS_ADDRESS& smsaDestinationAddress);
BOOL SummarizeResult();
BOOL SMS_SendMSG (const SMS_HANDLE smshHandle, const WCHAR *pContent, SMS_ADDRESS &smsaddress);

// Utils
void SetSMSAddress( SMS_ADDRESS &smsaAddress, TCHAR* pszAddress, SMS_ADDRESS_TYPE satAddressType );

#endif
