//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

typedef struct
{
	char className[64];
	char attribName[64];
	int  attribType;
	union 
	{
		int	attribInt;
		struct
		{
			int length;
			u_char octets[1024];
		} octetSeq;
		struct
		{
			u_char length;
			u_char charSet;
			u_char str[255];
		} usrStr;
	};
} Variable;

typedef struct
{
	int  result;
	int  WSAError;
	int  type;
#define REMOTE_QUERY_REQUEST	1
#define REMOTE_SET_REQUEST		2
#define REMOTE_UNSET_REQUEST	3
#define REMOTE_BIND_REQUEST		4
#define REMOTE_CONNECT_REQUEST	5
#define REMOTE_DISCOVER_REQUEST 6

#define REMOTE_QUERY_REPLY		101
#define REMOTE_SET_REPLY		102
#define REMOTE_UNSET_REPLY		103
	union
	{
		Variable query;
#define MAX_SOCKET_NAME_LENGTH	25
		char	 socketName[MAX_SOCKET_NAME_LENGTH+1];
	};
} RemoteRequest;

extern BOOL
SendQueryRequest(
	Connection    *pConn,
	IAS_SET		  *pSet,
	IAS_QUERY	  *pQuery,
	int			  *pResult,
	int			  *pWSAError);

extern BOOL
SendSetRequest(
	Connection    *pConn,
	IAS_SET		  *pSet,
	int			  *pResult,
	int			  *pWSAError);

extern BOOL
SendUnSetRequest(
	Connection    *pConn,
	int			  *pResult,
	int			  *pWSAError);

extern BOOL
SendConnectRequest(
	Connection    *pConn,
	char		  *pName,
	int			  *pResult,
	int			  *pWSAError);

extern BOOL
SendBindRequest(
	Connection    *pConn,
	char		  *pName,
	int			  *pResult,
	int			  *pWSAError);

extern BOOL
SendDiscoverRequest(
	Connection    *pConn,
	int			  *pResult,
	int			  *pWSAError);

int
ReceiveRequest(
	Connection *pConn);
