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

    expapis.hxx

Abstract:

    Exported APIs header

--*/
#if ! defined (__expapis_HXX__)
#define __expapis_HXX__	1

#define IOCTL_MSMQ_INVOKE                  4000


#define MQAPI_CODE_MQCreateQueue			2
#define MQAPI_CODE_MQDeleteQueue			3
#define MQAPI_CODE_MQGetQueueProperties		4
#define MQAPI_CODE_MQSetQueueProperties		5
#define MQAPI_CODE_MQOpenQueue				6
#define MQAPI_CODE_MQCloseQueue				7
#define MQAPI_CODE_MQCreateCursor			8
#define MQAPI_CODE_MQCloseCursor			9
#define MQAPI_CODE_MQHandleToFormatName		10
#define MQAPI_CODE_MQPathNameToFormatName	11
#define MQAPI_CODE_MQSendMessage			12
#define MQAPI_CODE_MQReceiveMessage			13
#define MQAPI_CODE_MQGetMachineProperties	14
#define MQAPI_CODE_MQMgmtGetInfo2			24
#define MQAPI_CODE_MQMgmtAction				25
// SrmpISAPI sends this code to indicate it has received an HTTP request.
#define MQAPI_Code_SRMPControl              26
#define MQAPI_CODE_MQFreeMemory             27

#endif

