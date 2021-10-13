//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// Dispids.H
//
// dispids for use in an automation or control object.
//
#ifndef _DISPIDS_H_

// MqMessage dispids
#define DISPID_MQMESSAGE_BODY               0
#define DISPID_MQMESSAGE_DELIVERY           1
#define DISPID_MQMESSAGE_PRIORITY           2
#define DISPID_MQMESSAGE_JOURNAL            3
#define DISPID_MQMESSAGE_QUEUEINFORESPONSE  4
#define DISPID_MQMESSAGE_APPSPECIFIC        5
#define DISPID_MQMESSAGE_GUIDSRCMACHINE     6
#define DISPID_MQMESSAGE_MSGCLASS           7
#define DISPID_MQMESSAGE_QUEUEINFOADMIN     8
#define DISPID_MQMESSAGE_ID                 9
#define DISPID_MQMESSAGE_IDCORRELATION      10
#define DISPID_MQMESSAGE_ACKNOWLEDGE        11
#define DISPID_MQMESSAGE_LABEL              12
#define DISPID_MQMESSAGE_LENBODY            13 
#define DISPID_MQMESSAGE_MAXTIMETOREACHQUEUE    14
#define DISPID_MQMESSAGE_MAXTIMETORECEIVE   15
#define DISPID_MQMESSAGE_ENCRYPTALG         16
#define DISPID_MQMESSAGE_HASHALG            17
#define DISPID_MQMESSAGE_SENTTIME           18
#define DISPID_MQMESSAGE_ARRIVEDTIME        19
#define DISPID_MQMESSAGE_QUEUEINFODEST      20
#define DISPID_MQMESSAGE_SENDERCERT         21
#define DISPID_MQMESSAGE_SENDERID           22
#define DISPID_MQMESSAGE_SENDERIDTYPE       23
#define DISPID_MQMESSAGE_TRACE              24
#define DISPID_MQMESSAGE_PRIVLEVEL          25
#define DISPID_MQMESSAGE_AUTHLEVEL          26
#define DISPID_MQMESSAGE_AUTHENTICATED      27

// MqQueue dispids
#define DISPID_MQQUEUE_HANDLE               0
#define DISPID_MQQUEUE_ACCESS               1
#define DISPID_MQQUEUE_SHAREMODE            2
#define DISPID_MQQUEUE_QUEUEINFO            3
#define DISPID_MQQUEUE_ISOPEN               4

// MqQueueInfo
#define DISPID_MQQUEUEINFO_GUIDQUEUE         0
#define DISPID_MQQUEUEINFO_GUIDSERVICETYPE   1
#define DISPID_MQQUEUEINFO_LABEL             2
#define DISPID_MQQUEUEINFO_PATHNAME          3
#define DISPID_MQQUEUEINFO_MACHINE           4
#define DISPID_MQQUEUEINFO_FORMATNAME        5
#define DISPID_MQQUEUEINFO_ISTRANSACTIONAL   6
#define DISPID_MQQUEUEINFO_PRIVLEVEL         7
#define DISPID_MQQUEUEINFO_JOURNAL           8
#define DISPID_MQQUEUEINFO_BASEPRIORITY      9
#define DISPID_MQQUEUEINFO_CREATETIME        10
#define DISPID_MQQUEUEINFO_MODIFYTIME        11
#define DISPID_MQQUEUEINFO_AUTHENTICATE      12
#define DISPID_MQQUEUEINFO_QUOTA             13
#define DISPID_MQQUEUEINFO_JOURNALQUOTA      14
#define DISPID_MQQUEUEINFO_ISWORLDREADABLE   15

// MqQueueEvents
#define DISPID_MQEVENTEVENTS_ARRIVED        0
#define DISPID_MQEVENTEVENTS_ARRIVEDERROR   1

// MqTransaction
#define DISPID_MQTRANSACTION_TRANSACTION    0

#define _DISPIDS_H_
#endif // _DISPIDS_H_



