//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include <winsock.h>
#include <oscfg.h>

#undef AF_IRDA
#include <af_irda.h>

#include "irsup.h"
#include "msg.h"
#include "request.h"

//
//  The af_irda.h header contains two virtually identical
//  structures.  Rather than writing the code to convert
//  these structures once, it is done once within this
//  macro which can be expanded into two different functions.
//
#define IAS_TO_VARIABLE \
	int length = 0; \
\
	memcpy(&pVar->className[0], &pSet->irdaClassName[0], 60); \
	pVar->className[60] = '\0'; \
	memcpy(&pVar->attribName[0], &pSet->irdaAttribName[0], 60); \
	pVar->attribName[60] = '\0'; \
	pVar->attribType = htonl(pSet->irdaAttribType); \
	switch(pSet->irdaAttribType) \
	{ \
	case IAS_ATTRIB_NO_ATTRIB: \
		length = offsetof(Variable, attribType) + sizeof(pVar->attribType); \
		break; \
\
	case IAS_ATTRIB_INT: \
		pVar->attribInt = htonl(pSet->irdaAttribute.irdaAttribInt); \
		length = offsetof(Variable, attribInt) + sizeof(pVar->attribInt); \
		break; \
\
	case IAS_ATTRIB_OCTETSEQ: \
		ASSERT(pSet->irdaAttribute.irdaAttribOctetSeq.Len <= sizeof(pVar->octetSeq.octets)); \
		pVar->octetSeq.length = htonl(pSet->irdaAttribute.irdaAttribOctetSeq.Len); \
		memcpy(&pVar->octetSeq.octets[0], \
			   &pSet->irdaAttribute.irdaAttribOctetSeq.OctetSeq[0], \
				pSet->irdaAttribute.irdaAttribOctetSeq.Len); \
		length = offsetof(Variable, octetSeq.octets[0]) + pSet->irdaAttribute.irdaAttribOctetSeq.Len; \
		break; \
\
	case IAS_ATTRIB_STR: \
		ASSERT(pSet->irdaAttribute.irdaAttribUsrStr.Len <= sizeof(pVar->usrStr.str)); \
		pVar->usrStr.length = (u_char)pSet->irdaAttribute.irdaAttribUsrStr.Len; \
		pVar->usrStr.charSet = \
			(u_char)pSet->irdaAttribute.irdaAttribUsrStr.CharSet; \
		memcpy(&pVar->usrStr.str[0], \
			   &pSet->irdaAttribute.irdaAttribUsrStr.UsrStr[0], \
				pSet->irdaAttribute.irdaAttribUsrStr.Len); \
		length = offsetof(Variable, usrStr.str[0]) + pSet->irdaAttribute.irdaAttribUsrStr.Len; \
		break; \
\
	default: \
		ASSERT(FALSE); \
		break; \
	} \
	return length;

//
//	Translate an IAS_SET structure to a Variable structure for
//  transmission through the network link.
//
//  Return the number of bytes in the variable structure that
//  need to be transmitted.
//
int
IasSetToQueryRequest(
	IAS_SET		*pSet,
	Variable	*pVar)
{
	// XXX Need to yank out these hard coded constants
	memcpy(&pVar->className[0], &pSet->irdaClassName[0], 60);
	pVar->className[60] = '\0';
	memcpy(&pVar->attribName[0], &pSet->irdaAttribName[0], 60);
	pVar->attribName[60] = '\0';

	// The attribType shouldn't matter in a QUERY, but we'll set
	// it just in case.
	pVar->attribType = htonl(IAS_ATTRIB_NO_ATTRIB);

	return offsetof(Variable, attribType) + sizeof(pVar->attribType);
}

//
//	Translate an IAS_QUERY structure to a Variable structure for
//  transmission through the network link.
//
//  Return the number of bytes in the variable structure that
//  need to be transmitted.
//
int
IasQueryToQueryReply(
	IAS_QUERY	*pSet,
	Variable	*pVar)
{
	IAS_TO_VARIABLE
}

//
//	Translate an IAS_SET structure to a Variable structure for
//  transmission through the network link.
//
//  Return the number of bytes in the variable structure that
//  need to be transmitted.
//
int
IasSetToSetRequest(
	IAS_SET		*pSet,
	Variable	*pVar)
{
	IAS_TO_VARIABLE
}

//
//	Translate an IAS_SET structure from a Variable structure upon
//  reception from the network link.
//
void
SetRequestToIasSet(
	Variable	*pVar,
	IAS_SET		*pSet,
	int			*pLength)
{
	memcpy(&pSet->irdaClassName[0], &pVar->className[0], 61);
	memcpy(&pSet->irdaAttribName[0], &pVar->attribName[0], 61);
	pSet->irdaAttribType = (unsigned short)ntohl(pVar->attribType);
	switch(pSet->irdaAttribType)
	{
	case IAS_ATTRIB_NO_ATTRIB:
		*pLength = offsetof(Variable, attribType) + sizeof(pVar->attribType);
		break;

	case IAS_ATTRIB_INT:
		pSet->irdaAttribute.irdaAttribInt = ntohl(pVar->attribInt);
		*pLength = offsetof(Variable, attribInt) + sizeof(pVar->attribInt);
		break;

	case IAS_ATTRIB_OCTETSEQ:
		pSet->irdaAttribute.irdaAttribOctetSeq.Len = ntohl(pVar->octetSeq.length);
		ASSERT(pSet->irdaAttribute.irdaAttribOctetSeq.Len <= sizeof(pVar->octetSeq.octets));
		memcpy(&pSet->irdaAttribute.irdaAttribOctetSeq.OctetSeq[0],
			   &pVar->octetSeq.octets[0],
				pSet->irdaAttribute.irdaAttribOctetSeq.Len);
		*pLength = offsetof(Variable, octetSeq.octets[0]) + pSet->irdaAttribute.irdaAttribOctetSeq.Len;
		break;

	case IAS_ATTRIB_STR:
		pSet->irdaAttribute.irdaAttribUsrStr.Len = pVar->usrStr.length;
		ASSERT(pSet->irdaAttribute.irdaAttribUsrStr.Len <= sizeof(pVar->usrStr.str));
		pSet->irdaAttribute.irdaAttribUsrStr.CharSet = pVar->usrStr.charSet;
		memcpy(&pSet->irdaAttribute.irdaAttribUsrStr.UsrStr[0],
			   &pVar->usrStr.str[0],
				pSet->irdaAttribute.irdaAttribUsrStr.Len);
		*pLength = offsetof(Variable, usrStr.str[0]) + pSet->irdaAttribute.irdaAttribUsrStr.Len;
		break;

	default:
		ASSERT(FALSE);
		break; 
	} 
}

//
//	Translate to an IAS_QUERY structure from a Variable structure upon
//  reception from the network link.
//
void
QueryRequestToIasQuery(
	Variable	*pVar,
	IAS_QUERY	*pQuery,
	int			*pLength)
{
	memcpy(&pQuery->irdaClassName[0], &pVar->className[0], 61);
	memcpy(&pQuery->irdaAttribName[0], &pVar->attribName[0], 61);

	// The attribute type should not matter in a query, we set
	// it here just in case.
	pQuery->irdaAttribType = IAS_ATTRIB_NO_ATTRIB;
}

void
QueryReplyToIasQuery(
	Variable	*pVar,
	IAS_QUERY	*pSet,
	int			*pLength)
{
	memcpy(&pSet->irdaClassName[0], &pVar->className[0], 61);
	memcpy(&pSet->irdaAttribName[0], &pVar->attribName[0], 61);

	pSet->irdaAttribType = (unsigned short)ntohl(pVar->attribType);
	switch(pSet->irdaAttribType)
	{
	case IAS_ATTRIB_NO_ATTRIB:
		*pLength = offsetof(Variable, attribType) + sizeof(pVar->attribType);
		break;

	case IAS_ATTRIB_INT:
		pSet->irdaAttribute.irdaAttribInt = htonl(pVar->attribInt);
		*pLength = offsetof(Variable, attribInt) + sizeof(pVar->attribInt);
		break;

	case IAS_ATTRIB_OCTETSEQ:
		pSet->irdaAttribute.irdaAttribOctetSeq.Len = htonl(pVar->octetSeq.length);
		ASSERT(pSet->irdaAttribute.irdaAttribOctetSeq.Len <= sizeof(pVar->octetSeq.octets));
		memcpy(&pSet->irdaAttribute.irdaAttribOctetSeq.OctetSeq[0],
			   &pVar->octetSeq.octets[0],
				pSet->irdaAttribute.irdaAttribOctetSeq.Len);
		*pLength = offsetof(Variable, octetSeq.octets[0]) + pSet->irdaAttribute.irdaAttribOctetSeq.Len;
		break;

	case IAS_ATTRIB_STR:
		pSet->irdaAttribute.irdaAttribUsrStr.Len = pVar->usrStr.length;
		ASSERT(pSet->irdaAttribute.irdaAttribUsrStr.Len <= sizeof(pVar->usrStr.str));
		pSet->irdaAttribute.irdaAttribUsrStr.CharSet = pVar->usrStr.charSet;
		memcpy(&pSet->irdaAttribute.irdaAttribUsrStr.UsrStr[0],
			   &pVar->usrStr.str[0],
				pSet->irdaAttribute.irdaAttribUsrStr.Len);
		*pLength = offsetof(Variable, usrStr.str[0]) + pSet->irdaAttribute.irdaAttribUsrStr.Len;
		break;

	default:
		ASSERT(FALSE);
		break; 
	} 
}

int
SendRequest(
	Connection       *pConn,
	RemoteRequest    *pRequest,
	int				  length)
{
	int result;

	pRequest->type = htonl(pRequest->type);

	// Send the request

	result = MessageSend(pConn, (char *)pRequest, length);
	if (result <= 0)
		return result;

	// Get the reply

	length = MessageReceive(pConn, (char *)pRequest, sizeof(*pRequest));
	if (length <= 0)
		return length;

	pRequest->result = ntohl(pRequest->result);
	pRequest->WSAError = ntohl(pRequest->WSAError);
	pRequest->type = ntohl(pRequest->type);

	return length;
}

//
//	Request that the remote server perform an IAS_QUERY operation
//  and return the result.
//
extern BOOL
SendQueryRequest(
	Connection    *pConn,
	IAS_SET		  *pSet,
	IAS_QUERY	  *pQuery,
	int			  *pResult,
	int			  *pWSAError)
{
	RemoteRequest  request;
	int length;

	request.type = REMOTE_QUERY_REQUEST;

	// Copy the IAS_SET fields to the remote request structure
	length = offsetof(RemoteRequest, query);
	length += IasSetToQueryRequest(pSet, &request.query);

	length = SendRequest(pConn, &request, length);

	if (length > 0)
	{
		int varLen;

		QueryReplyToIasQuery(&request.query, pQuery, &varLen);
		*pResult = request.result;
		*pWSAError = request.WSAError;
	}

	return length > 0;
}

//
//	Request that the remote server perform an IAS_SET operation
//  and return the result.
//
extern BOOL
SendSetRequest(
	Connection    *pConn,
	IAS_SET		  *pSet,
	int			  *pResult,
	int			  *pWSAError)
{
	RemoteRequest	request;
	int				length;

	request.type = REMOTE_SET_REQUEST;

	// Copy the IAS_SET fields to the remote request structure
	length = offsetof(RemoteRequest, query);
	length += IasSetToSetRequest(pSet, &request.query);

	length = SendRequest(pConn, &request, length);

	if (length > 0)
	{
		*pResult = request.result;
		*pWSAError = request.WSAError;
	}

	return length > 0;
}

//
//	Request that the remote server perform an IAS_SET operation
//  and return the result.
//
extern BOOL
SendUnSetRequest(
	Connection    *pConn,
	int			  *pResult,
	int			  *pWSAError)
{
	RemoteRequest  request;
	int length;

	request.type = REMOTE_UNSET_REQUEST;
	length = offsetof(RemoteRequest, query);
	length = SendRequest(pConn, &request, length);

	if (length > 0)
	{
		*pResult = request.result;
		*pWSAError = request.WSAError;
	}

	return length > 0;
}

#define MAX_IRDA_SERVICE_NAME_LENGTH (sizeof(((SOCKADDR_IRDA *)NULL)->irdaServiceName) -1)

//
//	Request that the remote server perform a connect
//  and return the result.
//
extern BOOL
SendConnectRequest(
	Connection    *pConn,
	char		  *pName,
	int			  *pResult,
	int			  *pWSAError)
{
	RemoteRequest  request;
	int length;

	ASSERT((strlen(pName) + 1) <= sizeof(request.socketName));

	request.type = REMOTE_CONNECT_REQUEST;

	// Make sure that the name to be connected to is of legal length
	strncpy(&request.socketName[0], pName, MAX_IRDA_SERVICE_NAME_LENGTH);
	request.socketName[MAX_IRDA_SERVICE_NAME_LENGTH] = '\0';

	length = offsetof(RemoteRequest, socketName[0]) + strlen(pName) + 1;

	length = SendRequest(pConn, &request, length);

	if (length > 0)
	{
		*pResult = request.result;
		*pWSAError = request.WSAError;
	}

	return length > 0;
}

//
//	Request that the remote server perform a connect
//  and return the result.
//
extern BOOL
SendBindRequest(
	Connection    *pConn,
	char		  *pName,
	int			  *pResult,
	int			  *pWSAError)
{
	RemoteRequest  request;
	int length;

	ASSERT((strlen(pName) + 1) <= sizeof(request.socketName));

	request.type = REMOTE_BIND_REQUEST;

	// Make sure that the name to be bound is of legal length
	strncpy(&request.socketName[0], pName, MAX_IRDA_SERVICE_NAME_LENGTH);
	request.socketName[MAX_IRDA_SERVICE_NAME_LENGTH] = '\0';

	length = offsetof(RemoteRequest, socketName[0]) + strlen(pName) + 1;

	length = SendRequest(pConn, &request, length);

	if (length > 0)
	{
		*pResult = request.result;
		*pWSAError = request.WSAError;
	}

	return length > 0;
}

//
//	Request that the remote server perform a connect
//  and return the result.
//
extern BOOL
SendDiscoverRequest(
	Connection    *pConn,
	int			  *pResult,
	int			  *pWSAError)
{
	RemoteRequest  request;
	int length;

	request.type = REMOTE_DISCOVER_REQUEST;

	length = offsetof(RemoteRequest, query);

	length = SendRequest(pConn, &request, length);

	if (length > 0)
	{
		*pResult = request.result;
		*pWSAError = request.WSAError;
	}

	return length > 0;
}

//  ***********************************************************
//  *** Server Stuff
//  ***********************************************************

#define MAX_ATTRIB_SIZE 1024
#define MAX_QUERY_SIZE  (sizeof(IAS_QUERY) + MAX_ATTRIB_SIZE)

//
//  Process a query request from the client.
//
//  Return the size of the reply (stored in the
//  RemoteRequest structure) in bytes.
//
int
ProcessQueryRequest(
	Connection     *pConn,
	RemoteRequest  *pRequest,
	int				length)
{
	BYTE		 queryBuffer[MAX_QUERY_SIZE];
	IAS_QUERY   *pQuery = (IAS_QUERY *)&queryBuffer[0];

	QueryRequestToIasQuery(&pRequest->query, pQuery, &length);

	// Get the peer device ID
	if (IRFindNewestDevice(&pQuery->irdaDeviceID[0], 1) == 0)
	{
		SOCKET sock = socket(AF_IRDA, SOCK_STREAM, 0);

		IRIasQuery(sock, pQuery, &pRequest->result, &pRequest->WSAError);
		length = offsetof(RemoteRequest, query);

		if (pRequest->result == 0)
			length += IasQueryToQueryReply(pQuery, &pRequest->query);

		closesocket(sock);
	}
	else
	{
		length = -1;
	}

	return length;
}

// Set operations are performed on a temporary socket
static SOCKET sockSet = INVALID_SOCKET;

//
//  Process a set request from the client.
//
//  Return the size of the reply (stored in the
//  RemoteRequest structure) in bytes.
//
int
ProcessSetRequest(
	Connection     *pConn,
	RemoteRequest  *pRequest,
	int				length)
{
	BYTE		 setBuffer[MAX_QUERY_SIZE];
	IAS_SET		*pSet = (IAS_SET *)&setBuffer[0];

	SetRequestToIasSet(&pRequest->query, pSet, &length);

	IRPrintIASSet(pSet);

	if (sockSet == INVALID_SOCKET)
	{
		sockSet = socket(AF_IRDA, SOCK_STREAM, 0);
	}
	IRIasSet(sockSet, pSet, &pRequest->result, &pRequest->WSAError);

	OutStr(TEXT("IAS_SET result=%d error=%d\n"), pRequest->result, pRequest->WSAError);

	return offsetof(RemoteRequest, query);
}

int
ProcessUnSetRequest(
	Connection     *pConn,
	RemoteRequest  *pRequest,
	int				length)
{	
	if (sockSet != INVALID_SOCKET)
	{
		OutStr(TEXT("Closing socket %d\n"), sockSet);
		pRequest->result = closesocket(sockSet);
		pRequest->WSAError = WSAGetLastError();
		sockSet = INVALID_SOCKET;
	}
	else
	{
		pRequest->result = 0;
	}
	return offsetof(RemoteRequest, query);
}

int
ProcessBindRequest(
	Connection     *pConn,
	RemoteRequest  *pRequest,
	int				length)
{
	if (sockSet == INVALID_SOCKET)
	{
		sockSet = socket(AF_IRDA, SOCK_STREAM, 0);
	}
	OutStr(TEXT("Bind %hs\n"), &pRequest->socketName[0]);
	IRBind(sockSet, &pRequest->socketName[0], &pRequest->result, &pRequest->WSAError);

	return offsetof(RemoteRequest, query);
}

int
ProcessConnectRequest(
	Connection     *pConn,
	RemoteRequest  *pRequest,
	int				length)
{
	if (sockSet == INVALID_SOCKET)
	{
		sockSet = socket(AF_IRDA, SOCK_STREAM, 0);
	}
	OutStr(TEXT("Connect to %hs\n"), &pRequest->socketName[0]);
	IRConnect(sockSet, &pRequest->socketName[0], &pRequest->result, &pRequest->WSAError);

	return offsetof(RemoteRequest, query);
}

int
ProcessDiscoverRequest(
	Connection     *pConn,
	RemoteRequest  *pRequest,
	int				length)
{
	BYTE deviceID[4];

	OutStr(TEXT("Discover...\n"), &pRequest->socketName[0]);
	IRFindNewestDevice(&deviceID[0], 1);

	return offsetof(RemoteRequest, query);
}

int
ReceiveRequest(
	Connection *pConn)
{
	RemoteRequest  request;
	int length;

	// Wait for request
	length = MessageReceive(pConn, (char *)&request, sizeof(request));

	if (length <= 0)
	{
		if (sockSet != INVALID_SOCKET)
		{
			closesocket(sockSet);
			sockSet = INVALID_SOCKET;
		}
		return length;
	}

	// Translate the request
	request.type = ntohl(request.type);

	// Execute  the request
	switch(request.type)
	{
	case REMOTE_QUERY_REQUEST:
		length = ProcessQueryRequest(pConn, &request, length);
		break;

	case REMOTE_SET_REQUEST:
		length = ProcessSetRequest(pConn, &request, length);
		break;

	case REMOTE_UNSET_REQUEST:
		length = ProcessUnSetRequest(pConn, &request, length);
		break;

	case REMOTE_BIND_REQUEST:
		length = ProcessBindRequest(pConn, &request, length);
		break;

	case REMOTE_CONNECT_REQUEST:
		length = ProcessConnectRequest(pConn, &request, length);
		break;

	case REMOTE_DISCOVER_REQUEST:
		length = ProcessDiscoverRequest(pConn, &request, length);
		break;

	default:
		OutStr(TEXT("unknown type = %d\n"), request.type);
		ASSERT(FALSE);
		break;
	}

	// Send the reply
	request.result = htonl(request.result);
	request.WSAError = htonl(request.WSAError);
	request.type = htonl(request.type);

	length = MessageSend(pConn, (char *)&request, length);

	return length;
}