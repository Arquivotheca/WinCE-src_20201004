#pragma once


//
//  FORWARD DECLARATIONS
//
struct SMB_PACKET;
struct SMB_NT_CREATE_CLIENT_REQUEST;
struct SMB_NT_CREATE_SERVER_RESPONSE;


#ifdef DEBUG
//
//  DEBUG
//
void TraceProtocolReceiveSmbRequest( SMB_PACKET* pSMB );
void TraceProtocolSendSmbResponse( SMB_PACKET* pSMB );

#define TRACE_PROTOCOL_RECEIVE_SMB_REQUEST              TraceProtocolReceiveSmbRequest
#define TRACE_PROTOCOL_SEND_SMB_RESPONSE                TraceProtocolSendSmbResponse


void TraceProtocolSmbNtCreateRequest( SMB_NT_CREATE_CLIENT_REQUEST* pRequest );
void TraceProtocolSmbNtCreateRequestFileName( WCHAR* lpszFileName, SMB_NT_CREATE_CLIENT_REQUEST* pRequest );
void TraceProtocolSmbNtCreateResponse( DWORD dwRetCode, SMB_NT_CREATE_SERVER_RESPONSE* pResponse );

#define TRACE_PROTOCOL_SMB_NT_CREATE_REQUEST            TraceProtocolSmbNtCreateRequest
#define TRACE_PROTOCOL_SMB_NT_CREATE_REQUEST_FILENAME   TraceProtocolSmbNtCreateRequestFileName
#define TRACE_PROTOCOL_SMB_NT_CREATE_RESPONSE           TraceProtocolSmbNtCreateResponse

#else
//
//  RETAIL
//
#define TRACE_PROTOCOL_RECEIVE_SMB_REQUEST
#define TRACE_PROTOCOL_SEND_SMB_RESPONSE


#define TRACE_PROTOCOL_SMB_NT_CREATE_REQUEST
#define TRACE_PROTOCOL_SMB_NT_CREATE_REQUEST_FILENAME
#define TRACE_PROTOCOL_SMB_NT_CREATE_RESPONSE

#endif
