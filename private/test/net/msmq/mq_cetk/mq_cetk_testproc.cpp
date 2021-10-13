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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------


#include <WINDOWS.H>
#include <TCHAR.H>
#include <TUX.H>
#include <KATOEX.H>
#include "mq_cetk_testproc.h"

extern CKato *g_pKato;

#define BASE_MARK        1
#define MAX_ECHO_RETRIES    3

//
// used to print debugging info
//
#define DEBUGGING 0

#include <mq.h>        // Message Queuing header file
#include <mqmgmt.h>

#define N_PROPERTIES_CREATE_DELETE 10

TESTPROCAPI
Test_CreateDeleteQueue
/* ++

create/delete queues

32 bit number dwId controls behavior

FF08FF88
   ||  |
   |+-++
   |  |
   |  +-- 16 bits, queue property
   |      1111 1111 1111 1111
   |      .... .... .... ...* PROPID_Q_JOURNAL
   |      .... .... .... ..*. PROPID_Q_JOURNAL_QUOTA
   |      .... .... .... .*.. PROPID_Q_LABEL
   |      .... .... .... *... PROPID_Q_QUOTA
   |      .... .... ...* .... PROPID_Q_AUTHENTICATE
   |      .... .... ..*. .... PROPID_Q_PRIV_LEVEL
   |      .... .... .*.. .... PROPID_Q_TRANSACTION
   |      other bits are reserved for future addition
   |
   |
   +----- 3 bits, number of queues to create
          value range 0 .. 15

return TPR_PASS or TPR_FAIL

-- */
(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    g_pKato->Log(LOG_COMMENT, _T("Test_CreateDeleteQueue"));
    DWORD dwId = lpFTE->dwUserData;  // property parameter


    WCHAR szId[] = L"FF00FF00  ";

    wsprintf(szId, L"%04X%04X", (dwId>>16) & 0xFFFF, dwId & 0xFFFF);
    g_pKato->Log(LOG_COMMENT, L"create/delete private$\\mq-cdq-%s", szId);

    HRESULT hr = MQ_OK;
    int iN = ((dwId >> 16) & 0x0F);// MAX iN=16
    if(iN==0)   iN = 1;
    BOOL bQueueCreated[16];
    for(int i=0; i<iN; i++)
    {
        MQPROPVARIANT aQueuePropVar[N_PROPERTIES_CREATE_DELETE];
        QUEUEPROPID aQueuePropId[N_PROPERTIES_CREATE_DELETE];
        DWORD cPropId = 0;

        WCHAR szQueuePathName[MQ_MAX_Q_NAME_LEN];
        wsprintf(szQueuePathName, L".\\private$\\%s-%d", szId, i);
        aQueuePropId[cPropId] = PROPID_Q_PATHNAME;
        aQueuePropVar[cPropId].vt = VT_LPWSTR;
        aQueuePropVar[cPropId].pwszVal = szQueuePathName;
        cPropId++;

        //  lower 16 bit: creation parameter
        //  combination of the following attributes

        if(dwId & 0x00000001)   // PROPID_Q_JOURNAL
        {
            aQueuePropId[cPropId] = PROPID_Q_JOURNAL;
            aQueuePropVar[cPropId].vt = VT_UI1;
            aQueuePropVar[cPropId].bVal = MQ_JOURNAL;
            cPropId++;
            g_pKato->Log(LOG_COMMENT, L"+PROPID_Q_JOURNAL");
        }

        if(dwId & 0x00000002)   // PROPID_Q_JOURNAL_QUOTA
        {
            aQueuePropId[cPropId] = PROPID_Q_JOURNAL_QUOTA;
            aQueuePropVar[cPropId].vt = VT_UI4;
            aQueuePropVar[cPropId].ulVal = 1000;
            cPropId++;
            g_pKato->Log(LOG_COMMENT, L"+PROPID_Q_JOURNAL_QUOTA");
        }

        WCHAR szQueueLabel[MQ_MAX_Q_LABEL_LEN] = L"created by mq_cetk"; // Queue label
        if(dwId & 0x00000004)   //  PROPID_Q_LABEL
        {
            aQueuePropId[cPropId] = PROPID_Q_LABEL;
            aQueuePropVar[cPropId].vt = VT_LPWSTR;
            aQueuePropVar[cPropId].pwszVal = szQueueLabel;
            cPropId++;
            g_pKato->Log(LOG_COMMENT, L"+PROPID_Q_LABEL");
        }

        if(dwId & 0x00000008)   //  PROPID_Q_QUOTA
        {
            aQueuePropId[cPropId] = PROPID_Q_QUOTA;
            aQueuePropVar[cPropId].vt = VT_UI4;
            aQueuePropVar[cPropId].ulVal = 1000;
            cPropId++;
            g_pKato->Log(LOG_COMMENT, L"+PROPID_Q_QUOTA");
        }

        if(dwId & 0x00000010)   //  PROPID_Q_AUTHENTICATE
        {
            aQueuePropId[cPropId] = PROPID_Q_AUTHENTICATE;
            aQueuePropVar[cPropId].vt = VT_UI1;
            aQueuePropVar[cPropId].bVal = MQ_AUTHENTICATE_NONE;
            cPropId++;
            g_pKato->Log(LOG_COMMENT, L"+PROPID_Q_AUTHENTICATE");
        }

        if(dwId & 0x00000020)   //  PROPID_Q_PRIV_LEVEL
        {
            aQueuePropId[cPropId] = PROPID_Q_PRIV_LEVEL;
            aQueuePropVar[cPropId].vt = VT_UI4;
            aQueuePropVar[cPropId].ulVal = MQ_PRIV_LEVEL_NONE;
            cPropId++;
            g_pKato->Log(LOG_COMMENT, L"+PROPID_Q_PRIV_LEVEL");
        }

        if(dwId & 0x00000040)   //  PROPID_Q_TRANSACTION
        {
            aQueuePropId[cPropId] = PROPID_Q_TRANSACTION;
            aQueuePropVar[cPropId].vt = VT_UI1;
            aQueuePropVar[cPropId].bVal = MQ_TRANSACTIONAL;
            cPropId++;
            g_pKato->Log(LOG_COMMENT, L"+PROPID_Q_TRANSACTION");
        }

        if(cPropId > N_PROPERTIES_CREATE_DELETE)
        {    // it never should come here.
            bQueueCreated[i] = FALSE;
            g_pKato->Log(LOG_FAIL, L"error (cPropId >= N_PROPERTIES_CREATE_DELETE)");
        }
        else
        {
            MQQUEUEPROPS QueueProps;
            HRESULT aQueueStatus[N_PROPERTIES_CREATE_DELETE];
            QueueProps.cProp = cPropId;
            QueueProps.aPropID = aQueuePropId;
            QueueProps.aPropVar = aQueuePropVar;
            QueueProps.aStatus = aQueueStatus;

            WCHAR szFormatNameBuffer[MQ_MAX_Q_NAME_LEN] = L"";        // Format name buffer
            DWORD dwFormatNameBufferLength = MQ_MAX_Q_NAME_LEN;    // Length of format name buffer
            hr = MQCreateQueue(NULL,    // Security descriptor
                                &QueueProps,            // Queue properties
                                szFormatNameBuffer,    // OUT: Format name of queue
                                &dwFormatNameBufferLength); // OUT: Format name length
            if(FAILED(hr) && (hr != (HRESULT)MQ_ERROR_QUEUE_EXISTS))
            {
                bQueueCreated[i] = FALSE;
                g_pKato->Log(LOG_FAIL, L"error MQCreateQueue(%s) failed", szQueuePathName);
                ASSERT(0);
            }
            else
            {
                bQueueCreated[i] = TRUE;
                g_pKato->Log(LOG_COMMENT, L"MQCreateQueue(%s) ok", szQueuePathName);
            }
        }
    }

    // now delete

    for(i=0; i<iN; i++)
    {
        if(bQueueCreated[i])
        {
            WCHAR szQueuePathName[MQ_MAX_Q_NAME_LEN];
            wsprintf(szQueuePathName, L".\\private$\\%s-%d", szId, i);
            WCHAR szFormatNameBuffer[MQ_MAX_Q_NAME_LEN] = L"";        // Format name buffer
            DWORD dwFormatNameBufferLength = MQ_MAX_Q_NAME_LEN;    // Length of format name buffer
            MQPathNameToFormatName(szQueuePathName, szFormatNameBuffer, &dwFormatNameBufferLength);
            hr = MQDeleteQueue(szFormatNameBuffer);
            if(FAILED(hr))
                g_pKato->Log(LOG_FAIL, L"error MQDeleteQueue(%s) failed", szFormatNameBuffer);
            else
                g_pKato->Log(LOG_COMMENT, L"MQDeleteQueue(%s) ok", szQueuePathName);
        }
    }

    if(g_pKato->GetVerbosityCount(LOG_FAIL))
        return TPR_FAIL;
    return TPR_PASS;
}









BOOL
ReadRegistry
//
// reads WCHAR string value from the registry
//
(
    HKEY hRootKey,          // root key, HKEY_LOCAL_MACHINE
    LPCWSTR szSubKey,       // subkey name
    LPCWSTR szValueName,    // value name
    LPWSTR szStringValue,   // string value to read
    DWORD iLen              // length of the WCHAR string aray
)
{
    if(!szStringValue)
        return FALSE;
    *szStringValue = L'\0';
    HKEY hKey;
    if(RegOpenKeyEx(hRootKey, szSubKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)   // should not fail
        return FALSE;

    BOOL bR = TRUE;
    DWORD bType;
    DWORD cbData = iLen*sizeof(WCHAR);
    if(
        (RegQueryValueEx(hKey, szValueName, NULL, &bType, (BYTE *)szStringValue, &cbData) != ERROR_SUCCESS) ||
        (bType != REG_SZ))
    {
        *szStringValue = L'\0';
        bR = FALSE;
    }
    RegCloseKey(hKey);

    return bR;
}







#define N_PROPERTIES_CREATE 3


BOOL
CreateLocalQueue
//
//  creating a local queue.
//  return TRUE if queue created or already exists.
//
(
    const WCHAR *szQueueName  // name of the queue, L"mqstress"
)
{
    WCHAR szQueuePathName[MQ_MAX_Q_NAME_LEN];
    wsprintf(szQueuePathName, L".\\private$\\%s", szQueueName);

    // 1. Define the required structures.
    MQPROPVARIANT aQueuePropVar[N_PROPERTIES_CREATE];
    QUEUEPROPID aQueuePropId[N_PROPERTIES_CREATE];
    DWORD cPropId = 0;

    aQueuePropId[cPropId] = PROPID_Q_PATHNAME;
    aQueuePropVar[cPropId].vt = VT_LPWSTR;
    aQueuePropVar[cPropId].pwszVal = szQueuePathName;
    cPropId++;

    aQueuePropId[cPropId] = PROPID_Q_LABEL;
    aQueuePropVar[cPropId].vt = VT_LPWSTR;
    aQueuePropVar[cPropId].pwszVal = L"created for mqstress";
    cPropId++;

    // 2. Initialize the MQQUEUEPROPS structure.
    MQQUEUEPROPS QueueProps;
    HRESULT aQueueStatus[N_PROPERTIES_CREATE];
    QueueProps.cProp = cPropId;
    QueueProps.aPropID = aQueuePropId;
    QueueProps.aPropVar = aQueuePropVar;
    QueueProps.aStatus = aQueueStatus;

    // 3. Create Queue.
    WCHAR szFormatNameBuffer[MQ_MAX_Q_NAME_LEN];
    DWORD dwFormatNameBufferLength = MQ_MAX_Q_NAME_LEN;
    HRESULT hr = MQCreateQueue(
                    NULL,
                    &QueueProps,
                    szFormatNameBuffer,
                    &dwFormatNameBufferLength);

    if(FAILED(hr) && (hr != MQ_ERROR_QUEUE_EXISTS))
        return FALSE;
    return TRUE;
}







#define N_PROPERTIES_SEND_RECEIVE 30


UINT
SendReceiveMessage
/* ++

send/receive messages through '.\private$\mq_cetk'


32 bit number dwId controls behavior

FF00FF00
     |||
     ||+-- 4 bits
     ||      1111
     ||      ...* 0:binary protocol, 1:srmp
     ||      ..*. 0:express delivery, 1:recoverable
     ||      .*.. 0:no journal, 1:MQMSG_DEADLETTER
     ||
     ||
     |+--- 4 bits = number of messages to send and receive (1-16)
     |
     +---- 4 bits = size of message
             0000 --> 0x0100 (256) bytes
             ...
             0F00 --> 0x1000 (4096) bytes

return TPR_PASS or TPR_FAIL

-- */
(
    DWORD dwId,     // property parameter
    WCHAR* szDeviceName,
    BYTE* pucMessageBody    // memory block allocated for message body
)
{
    g_pKato->Log(LOG_COMMENT, _T("Test_SendReceiveMessage"));

    // dwID
    // 00000F00 message size
    // min value = 0x0100
    // max value = 0x0F00 + 0x0100 = 0x1000

    int iMessageBodySize = (dwId & 0x00000F00) + 0x0100;
    int iNoOfMessages = ((dwId >> 4) & 0x0F) + 1;   // 1..16

    MSGPROPID aMsgPropId[N_PROPERTIES_SEND_RECEIVE];
    MQPROPVARIANT aMsgPropVar[N_PROPERTIES_SEND_RECEIVE];

    DWORD cPropId = 0;             // Property counter
    aMsgPropId[cPropId] = PROPID_M_BODY;
    aMsgPropVar[cPropId].vt = VT_VECTOR | VT_UI1;
    aMsgPropVar[cPropId].caub.pElems = pucMessageBody;
    aMsgPropVar[cPropId].caub.cElems = iMessageBodySize;
    cPropId++;

    // express or recoverable?
    aMsgPropId[cPropId] = PROPID_M_DELIVERY;
    aMsgPropVar[cPropId].vt = VT_UI1;
    aMsgPropVar[cPropId].bVal = (dwId & 0x00000002) ? MQMSG_DELIVERY_RECOVERABLE  : MQMSG_DELIVERY_EXPRESS;
    cPropId++;
    g_pKato->Log(LOG_COMMENT, (dwId & 0x00000002) ? L"+MQMSG_DELIVERY_RECOVERABLE" : L"+MQMSG_DELIVERY_EXPRESS");

    // deadletter journal or not?
    if(dwId & 0x00000004)
    {
        aMsgPropId[cPropId] = PROPID_M_JOURNAL;
        aMsgPropVar[cPropId].vt = VT_UI1;
        aMsgPropVar[cPropId].bVal = MQMSG_DEADLETTER;
        cPropId++;
        g_pKato->Log(LOG_COMMENT, L"+MQMSG_DEADLETTER");
    }

    if(cPropId > N_PROPERTIES_SEND_RECEIVE)
    {   // it should never come here
        g_pKato->Log(LOG_FAIL, L"error (cPropId >= N_PROPERTIES_SEND_RECEIVE)");
        return TPR_FAIL;
    }

    HRESULT aMsgStatus[N_PROPERTIES_SEND_RECEIVE];
    MQMSGPROPS msgprops;
    msgprops.cProp = cPropId;
    msgprops.aPropID = aMsgPropId;
    msgprops.aPropVar = aMsgPropVar;
    msgprops.aStatus  = aMsgStatus;

    // 00000001 bit sending protocol

    WCHAR szQueueFormatName[MQ_MAX_Q_NAME_LEN];
    if((dwId&0x00000001) == 0)  // binary
    {
        wsprintf(szQueueFormatName, L"DIRECT=OS:%s\\private$\\mq_cetk", szDeviceName);
        g_pKato->Log(LOG_COMMENT, L"+BINARY MSMQ");
    }
    else        // srmp queue
    {
        wsprintf(szQueueFormatName, L"DIRECT=HTTP://%s/msmq\\private$\\mq_cetk", szDeviceName);
        g_pKato->Log(LOG_COMMENT, L"+SRMP (HTTP MSMQ)");
    }

    g_pKato->Log(LOG_COMMENT, L"send/receive message (%d * sz:%d : %s) via %s",
            iNoOfMessages,
            iMessageBodySize,
            (dwId & 0x00000002) ? L"RECOVERABLE" : L"EXPRESS",
            szQueueFormatName);


    HRESULT hr = MQ_OK;
    HANDLE hQueue;
    hr = MQOpenQueue(szQueueFormatName,
                        MQ_SEND_ACCESS,          // Access mode
                        MQ_DENY_NONE,            // Share mode
                        &hQueue       // OUT: Handle to queue
                        );
    if(FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL, L"SendReceiveMessage : MQOpenQueue(%s) fail, error = 0x%X", szQueueFormatName, hr);
        return TPR_FAIL;
    }

    for(int i=0; i<iNoOfMessages; i++)
    {
        for(int j=0; j<5; j++)
        {
            hr = MQSendMessage(hQueue, &msgprops, NULL);
            if(FAILED(hr))
            {
                g_pKato->Log(LOG_FAIL, L"SendReceiveMessage : MQSendMessage(%s) fail, error = 0x%X", szQueueFormatName, hr);
                Sleep(500); // ms, then try again
            }
            else
                break;
        }

    }
    int iNSent = i;
    if(iNSent < iNoOfMessages)
        g_pKato->Log(LOG_FAIL, L"SendReceiveMessage could not send given number of messages");

    hr = MQCloseQueue(hQueue);
    if(FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL, L"SendReceiveMessage : MQCloseQueue(%s) fail, error = 0x%X", szQueueFormatName, hr);
        return TPR_FAIL;
    }



    // receive

    iMessageBodySize = 0x1000;  // 4 k mem, alloc possible max size, in case other thread is trying to send different size message
    cPropId = 0;                     // Property counter

    // request all possible properties
    // 0. PROPID_M_ACKNOWLEDGE (VT_UI1)
    aMsgPropId[cPropId] = PROPID_M_ACKNOWLEDGE;
    aMsgPropVar[cPropId].vt = VT_UI1;
    cPropId++;

    // 1. PROPID_M_ADMIN_QUEUE (VT_LPWSTR)
    WCHAR szAdminQueue[MQ_MAX_Q_NAME_LEN];
    aMsgPropId[cPropId] = PROPID_M_ADMIN_QUEUE;
    aMsgPropVar[cPropId].vt = VT_LPWSTR;
    aMsgPropVar[cPropId].pwszVal = szAdminQueue;
    cPropId++;

    // 2. PROPID_M_ADMIN_QUEUE_LEN (VT_UI4)
    aMsgPropId[cPropId] = PROPID_M_ADMIN_QUEUE_LEN;
    aMsgPropVar[cPropId].vt = VT_UI4;
    aMsgPropVar[cPropId].ulVal = MQ_MAX_Q_NAME_LEN;
    cPropId++;

    // 3. PROPID_M_APPSPECIFIC (VT_UI4)
    aMsgPropId[cPropId] = PROPID_M_APPSPECIFIC;
    aMsgPropVar[cPropId].vt = VT_UI4;
    cPropId++;

    // 4. PROPID_M_ARRIVEDTIME (VT_UI4)
    aMsgPropId[cPropId] = PROPID_M_ARRIVEDTIME;
    aMsgPropVar[cPropId].vt = VT_UI4;
    cPropId++;

    // 5. PROPID_M_AUTH_LEVEL (VT_UI4)
    aMsgPropId[cPropId] = PROPID_M_AUTH_LEVEL;
    aMsgPropVar[cPropId].vt = VT_UI4;
    cPropId++;

    // 6. PROPID_M_BODY (VT_UI1|VT_VECTOR)
    aMsgPropId[cPropId] = PROPID_M_BODY;
    aMsgPropVar[cPropId].vt = VT_VECTOR|VT_UI1;
    aMsgPropVar[cPropId].caub.pElems = pucMessageBody;
    aMsgPropVar[cPropId].caub.cElems = iMessageBodySize;
    cPropId++;

    // 7. PROPID_M_BODY_SIZE (VT_UI4)
    aMsgPropId[cPropId] = PROPID_M_BODY_SIZE;
    aMsgPropVar[cPropId].vt = VT_UI4;
    cPropId++;

    // 8. PROPID_M_BODY_TYPE (VT_UI4)
    aMsgPropId[cPropId] = PROPID_M_BODY_TYPE;
    aMsgPropVar[cPropId].vt = VT_UI4;
    cPropId++;

    // 9. PROPID_M_CLASS (VT_UI2)
    aMsgPropId[cPropId] = PROPID_M_CLASS;
    aMsgPropVar[cPropId].vt = VT_UI2;
    cPropId++;

    // 10. PROPID_M_CORRELATIONID (VT_UI1|VT_VECTOR)
    BYTE btCorrelationIdByte[PROPID_M_CORRELATIONID_SIZE];
    aMsgPropId[cPropId] = PROPID_M_CORRELATIONID;
    aMsgPropVar[cPropId].vt = VT_UI1|VT_VECTOR;
    aMsgPropVar[cPropId].caub.pElems = btCorrelationIdByte;
    aMsgPropVar[cPropId].caub.cElems = PROPID_M_CORRELATIONID_SIZE;
    cPropId++;

    // 11. PROPID_M_DELIVERY (VT_UI1)
    aMsgPropId[cPropId] = PROPID_M_DELIVERY;
    aMsgPropVar[cPropId].vt = VT_UI1;
    cPropId++;

    WCHAR szDestinationQueueName[MQ_MAX_Q_NAME_LEN];
    // 12. PROPID_M_DEST_QUEUE (VT_LPWSTR)
    aMsgPropId[cPropId] = PROPID_M_DEST_QUEUE;
    aMsgPropVar[cPropId].vt = VT_LPWSTR;
    aMsgPropVar[cPropId].pwszVal = szDestinationQueueName;
    cPropId++;

    // 13. PROPID_M_DEST_QUEUE_LEN (VT_UI4)
    aMsgPropId[cPropId] = PROPID_M_DEST_QUEUE_LEN;
    aMsgPropVar[cPropId].vt = VT_UI4;
    aMsgPropVar[cPropId].ulVal = MQ_MAX_Q_NAME_LEN;
    cPropId++;

    // 14. PROPID_M_EXTENSION (VT_UI1|VT_VECTOR)
    BYTE btExtensionInfoByte[20];    // size is not defined.
    aMsgPropId[cPropId] = PROPID_M_EXTENSION;
    aMsgPropVar[cPropId].vt = VT_UI1|VT_VECTOR;
    aMsgPropVar[cPropId].caub.pElems = btExtensionInfoByte;
    aMsgPropVar[cPropId].caub.cElems = sizeof(btExtensionInfoByte);
    cPropId++;

    // 15. PROPID_M_EXTENSION_LEN (VT_UI4)
    aMsgPropId[cPropId] = PROPID_M_EXTENSION_LEN;
    aMsgPropVar[cPropId].vt = VT_UI4;
    aMsgPropVar[cPropId].ulVal = sizeof(btExtensionInfoByte);
    cPropId++;

    // 16. PROPID_M_JOURNAL (VT_UI1)
    aMsgPropId[cPropId] = PROPID_M_JOURNAL;
    aMsgPropVar[cPropId].vt = VT_UI1;
    cPropId++;

    // 17. PROPID_M_LABEL (VT_LPWSTR)
    WCHAR szLabel[MQ_MAX_MSG_LABEL_LEN];
    aMsgPropId[cPropId] = PROPID_M_LABEL;
    aMsgPropVar[cPropId].vt = VT_LPWSTR;
    aMsgPropVar[cPropId].pwszVal = szLabel;
    cPropId++;

    // 18. PROPID_M_LABEL_LEN (VT_UI4)
    aMsgPropId[cPropId] = PROPID_M_LABEL_LEN;
    aMsgPropVar[cPropId].vt = VT_UI4;
    aMsgPropVar[cPropId].ulVal = MQ_MAX_MSG_LABEL_LEN;
    cPropId++;

    // 19. PROPID_M_MSGID (VT_UI1|VT_VECTOR)
    BYTE btMessageId[PROPID_M_MSGID_SIZE];
    aMsgPropId[cPropId] = PROPID_M_MSGID;
    aMsgPropVar[cPropId].vt = VT_UI1|VT_VECTOR;
    aMsgPropVar[cPropId].caub.pElems = btMessageId;
    aMsgPropVar[cPropId].caub.cElems = PROPID_M_MSGID_SIZE;
    cPropId++;

    // 20. PROPID_M_PRIORITY (VT_UI1)
    aMsgPropId[cPropId] = PROPID_M_PRIORITY;
    aMsgPropVar[cPropId].vt = VT_UI1;
    cPropId++;

    // 21. PROPID_M_RESP_QUEUE (VT_LPWSTR)
    WCHAR szResponseQueue[MQ_MAX_Q_NAME_LEN];
    aMsgPropId[cPropId] = PROPID_M_RESP_QUEUE;
    aMsgPropVar[cPropId].vt = VT_LPWSTR;
    aMsgPropVar[cPropId].pwszVal = szResponseQueue;
    cPropId++;


    // 22. PROPID_M_RESP_QUEUE_LEN (VT_UI4)
    aMsgPropId[cPropId] = PROPID_M_RESP_QUEUE_LEN;
    aMsgPropVar[cPropId].vt = VT_UI4;
    aMsgPropVar[cPropId].ulVal = MQ_MAX_Q_NAME_LEN;
    cPropId++;

    // 23. PROPID_M_SENTTIME (VT_UI4)
    aMsgPropId[cPropId] = PROPID_M_SENTTIME;
    aMsgPropVar[cPropId].vt = VT_UI4;
    cPropId++;

    // 24. PROPID_M_SRC_MACHINE_ID (VT_CLSID)
    GUID guidSource;
    aMsgPropId[cPropId] = PROPID_M_SRC_MACHINE_ID;
    aMsgPropVar[cPropId].vt = VT_CLSID;
    aMsgPropVar[cPropId].puuid = &guidSource;
    cPropId++;

    // 25. PROPID_M_TIME_TO_BE_RECEIVED (VT_UI4)
    aMsgPropId[cPropId] = PROPID_M_TIME_TO_BE_RECEIVED;
    aMsgPropVar[cPropId].vt = VT_UI4;
    cPropId++;

    // 26. PROPID_M_TIME_TO_REACH_QUEUE (VT_UI4)
    aMsgPropId[cPropId] = PROPID_M_TIME_TO_REACH_QUEUE;
    aMsgPropVar[cPropId].vt = VT_UI4;
    cPropId++;

    // 27. PROPID_M_TRACE (VT_UI1)
    aMsgPropId[cPropId] = PROPID_M_TRACE;
    aMsgPropVar[cPropId].vt = VT_UI1;
    cPropId++;

    // 28. PROPID_M_VERSION (VT_UI4)
    aMsgPropId[cPropId] = PROPID_M_VERSION;
    aMsgPropVar[cPropId].vt = VT_UI4;
    cPropId++;

    if(cPropId > N_PROPERTIES_SEND_RECEIVE)
    {
        g_pKato->Log(LOG_FAIL, L"error (cPropId >= N_PROPERTIES_SEND_RECEIVE)");
        return TPR_FAIL;
    }

    msgprops.cProp = cPropId;          // Number of message properties
    msgprops.aPropID = aMsgPropId;     // Ids of message properties
    msgprops.aPropVar = aMsgPropVar;   // Values of message properties
    msgprops.aStatus  = aMsgStatus;    // Error reports


    // open queue
    wsprintf(szQueueFormatName, L"DIRECT=OS:%s\\private$\\mq_cetk", szDeviceName);   // always use binary queue name to receive
    hr = MQOpenQueue(szQueueFormatName,
                        MQ_RECEIVE_ACCESS,          // Access mode
                        MQ_DENY_NONE,            // Share mode
                        &hQueue       // OUT: Handle to queue
                        );
    if(FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL, L"SendReceiveMessage : MQOpenQueue(%s) fail, error = 0x%X", szQueueFormatName, hr);
        return TPR_FAIL;
    }

    for(i=0; i<iNSent; i++)
    {
        // receive the message.
        hr = MQReceiveMessage(hQueue, // Queue handle
                10000,                // Max time (msec)
                MQ_ACTION_RECEIVE,  // Receive action
                &msgprops,          // Msg property structure
                NULL,               // No OVERLAPPED structure
                NULL,               // No callback function
                NULL,               // No cursor
                NULL                // No transaction
                );
        if(FAILED(hr))
        {
            g_pKato->Log(LOG_FAIL, L"MQReceiveMessage(%s) fail, error = 0x%X", szQueueFormatName, hr);
            break;
        }
    }

    // close queue
    hr = MQCloseQueue(hQueue);
    if(FAILED(hr))
        g_pKato->Log(LOG_FAIL, L"SendReceiveMessage : MQCloseQueue(%s) fail, error = 0x%X", szQueueFormatName, hr);

    if(g_pKato->GetVerbosityCount(LOG_FAIL))
        return TPR_FAIL;
    return TPR_PASS;
}





TESTPROCAPI
Test_SendReceiveMessage
(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!CreateLocalQueue(L"mq_cetk"))
    {
        g_pKato->Log(LOG_FAIL, L"CreateLocalQueue(L\"mq_cetk\") failed");
        return TPR_FAIL;
    }

    WCHAR szDeviceName[MAX_PATH];
    if(ReadRegistry(HKEY_LOCAL_MACHINE, L"Ident", L"Name", szDeviceName, MAX_PATH) == FALSE)
    {
        g_pKato->Log(LOG_FAIL, L"registry reading error");
        return TPR_FAIL;
    }

    BYTE* pucMessageBody = (BYTE*)LocalAlloc(LPTR, 0x1000);
    UINT result = SendReceiveMessage(
                        lpFTE->dwUserData,  // property parameter
                        szDeviceName,
                        pucMessageBody    // memory block allocated for message body
                        );
    LocalFree(pucMessageBody);

    return result;
}

