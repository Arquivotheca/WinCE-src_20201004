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

    expapis.hxx

Abstract:

    Exported APIs header

--*/
#if ! defined (__expapis_HXX__)
#define __expapis_HXX__	1

HRESULT	scapi_MQCreateQueue
(
int				iNull,
SCPROPVAR		*pQueueProps,
WCHAR			*lpszFormatName,
DWORD			*dwNameLen
);

#define MQAPI_CODE_MQCreateQueue			2

HRESULT	scapi_MQDeleteQueue
(
WCHAR			*lpszFormatName
);

#define MQAPI_CODE_MQDeleteQueue			3

HRESULT	scapi_MQGetQueueProperties
(
WCHAR			*lpszFormatName,
SCPROPVAR		*pQueueProps
);

#define MQAPI_CODE_MQGetQueueProperties		4

HRESULT scapi_MQSetQueueProperties
(
WCHAR			*lpszFormatName,
SCPROPVAR		*pQueueProps
);

#define MQAPI_CODE_MQSetQueueProperties		5

HRESULT	scapi_MQOpenQueue
(
WCHAR			*lpszFormatName,
DWORD			dwAccess,
DWORD			dwShareMode,
SCHANDLE		*phQueue
);

#define MQAPI_CODE_MQOpenQueue				6

HRESULT scapi_MQCloseQueue
(
SCHANDLE		hQueue
);

#define MQAPI_CODE_MQCloseQueue				7

HRESULT scapi_MQCreateCursor
(
SCHANDLE		hQueue,
SCHANDLE		*phCursor
);

#define MQAPI_CODE_MQCreateCursor			8

HRESULT scapi_MQCloseCursor
(
SCHANDLE		hCursor
);

#define MQAPI_CODE_MQCloseCursor			9

HRESULT scapi_MQHandleToFormatName
(
SCHANDLE hQueue,
WCHAR *lpszFormatName,
DWORD *dwNameLen
);

#define MQAPI_CODE_MQHandleToFormatName		10

HRESULT scapi_MQPathNameToFormatName
(
WCHAR *lpszPathName,
WCHAR *lpszFormatName,
DWORD *pdwCount
);

#define MQAPI_CODE_MQPathNameToFormatName	11

HRESULT scapi_MQSendMessage
(
SCHANDLE	hQueue,
SCPROPVAR	*pMsgProps,
int			iNull
);

#define MQAPI_CODE_MQSendMessage			12

HRESULT scapi_MQReceiveMessage
(
SCHANDLE		hQueue,
DWORD			dwTimeout,
DWORD			dwAction,
SCPROPVAR		*pMsgProps,
OVERLAPPED		*lpOverlapped,
PMQRECEIVECALLBACK pfnReceiveCallback,
SCHANDLE		hCursor,
int				iNull3
);

#define MQAPI_CODE_MQReceiveMessage			13
typedef struct _data_MQReceiveMessage {
	SCHANDLE			hQueue;
	DWORD				dwTimeout;
	DWORD				dwAction;
	SCPROPVAR			*pMsgProps;
	OVERLAPPED			*lpOverlapped;
	PMQRECEIVECALLBACK	pfnReceiveCallback;
	SCHANDLE			hCursor;
	int					iNull3;
} DataMQReceiveMessage;

HRESULT scapi_MQGetMachineProperties
(
WCHAR			*lpszMachineName,
GUID			*pguidMachineID,
SCPROPVAR		*pQMProps
);

#define MQAPI_CODE_MQGetMachineProperties	14

HRESULT scmgmt_MQMgmtGetInfo2
(
LPCWSTR			pMachineName,
LPCWSTR			pObjectName,
DWORD			cp,
PROPID			aPropID[],
PROPVARIANT		aPropVar[]
);

#define MQAPI_CODE_MQMgmtGetInfo2			24

HRESULT scmgmt_MQMgmtAction
(
LPCWSTR			pMachineName,
LPCWSTR			pObjectName,
LPCWSTR			pAction
);

#define MQAPI_CODE_MQMgmtAction				25

void scapi_ProcExit (void *pvProcId);

// SrmpISAPI sends this code to indicate it has received an HTTP request.
#define MQAPI_Code_SRMPControl              26

//
//	Below are unimplemented functions
//
HRESULT scapi_MQBeginTransaction (ITransaction **ppTransaction);

#define MQAPI_CODE_MQBeginTransaction		15

HRESULT scapi_MQGetQueueSecurity (WCHAR *lpszFormatName, SECURITY_INFORMATION SecurityInformation, SECURITY_DESCRIPTOR *pSecurityDescriptor, DWORD nLength, DWORD *dwlengthNeeded);

#define MQAPI_CODE_MQGetQueueSecurity		16

HRESULT scapi_MQSetQueueSecurity (WCHAR *lpszFormatName, SECURITY_INFORMATION SecurityInformation, SECURITY_DESCRIPTOR *pSecurityDescriptor);

#define MQAPI_CODE_MQSetQueueSecurity		17

HRESULT scapi_MQGetSecurityContext (VOID *lpCertBuffer, DWORD dwCertBufferLength,HANDLE *hSecurityContext);

#define MQAPI_CODE_MQGetSecurityContext		18

void scapi_MQFreeSecurityContext (HANDLE hUnused);

#define MQAPI_CODE_MQFreeSecurityContext	19

HRESULT scapi_MQInstanceToFormatName (GUID  *pGUID, WCHAR *lpszFormatName, DWORD *lpdwCount );

#define MQAPI_CODE_MQInstanceToFormatName	20

HRESULT scapi_MQLocateBegin (WCHAR *lpszContext, MQRESTRICTION *pRestriction, MQCOLUMNSET *pColumns, MQSORTSET *pSort, HANDLE *hEnum);

#define MQAPI_CODE_MQLocateBegin			21

HRESULT scapi_MQLocateNext (HANDLE hEnum, DWORD *pcProps, PROPVARIANT aPropVar[]);

#define MQAPI_CODE_MQLocateNext				22

HRESULT scapi_MQLocateEnd(HANDLE hEnum);

#define MQAPI_CODE_MQLocateEnd				23


#endif
