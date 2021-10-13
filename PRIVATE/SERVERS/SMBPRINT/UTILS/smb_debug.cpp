#include <windows.h>
#include <smb_debug.h>
#include <smbpackets.h>
#include <cracker.h>
#include <utils.h>


#ifdef DEBUG
//
//  TraceProtocolReceiveSmbRequest:
//
//      - dumps the incoming request packet
//      - prints the packet command type
//      - indicates the incoming transport
//
void TraceProtocolReceiveSmbRequest( SMB_PACKET* pSMB )
{
    ASSERT( NULL != pSMB );
    ASSERT( NULL != pSMB->lpszTransport );
    ASSERT( NULL != pSMB->pInSMB );
    ASSERT( 0x00 != pSMB->pInSMB->Command );

    SMB_HEADER* pRequest = (SMB_HEADER*) pSMB->pInSMB;

    TRACEMSG( ZONE_PROTOCOL, (L"[PROTOCOL] Incoming SMB Request") );
    TRACEMSG( ZONE_PROTOCOL, (L"\tTransport[%s]", pSMB->lpszTransport) ); 
    TRACEMSG( ZONE_PROTOCOL, (L"\tCommand[0x%02x][%s]", pRequest->Command, GetCMDName(pRequest->Command)) ); 
}


//
//  TraceProtocolSendSmbResponse:
//
//      - dumps the outgoing response packet
//      - prints the packet command type
//      - prints the packet status code
//      - indicates the time delta from receipt until response
//      - indicates the outgoing transport
//
void TraceProtocolSendSmbResponse( SMB_PACKET* pSMB )
{
    ASSERT( NULL != pSMB );
    ASSERT( NULL != pSMB->lpszTransport );
    ASSERT( NULL != pSMB->pOutSMB );
    ASSERT( 0x00 != pSMB->pOutSMB->Command );

    SMB_HEADER* pResponse = (SMB_HEADER*) pSMB->pOutSMB;

    TRACEMSG( ZONE_PROTOCOL, (L"[PROTOCOL] Outgoing SMB Response" ));
    TRACEMSG( ZONE_PROTOCOL, (L"\tTransport[%s]", pSMB->lpszTransport) ); 
    TRACEMSG( ZONE_PROTOCOL, (L"\tCommand[0x%02x][%s]", pResponse->Command, GetCMDName(pResponse->Command)) ); 
    TRACEMSG( ZONE_PROTOCOL, (L"\tStatusCode[0x%08x]", pResponse->StatusFields.Fields.dwError) );
    TRACEMSG( ZONE_PROTOCOL, (L"\tProcessingTime[%d]", pSMB->GetProcessingTime()) );
}


//
//  TraceProtocolSmbNtCreateRequest:
//
//      - dumps the incoming request packet parameters
//
void TraceProtocolSmbNtCreateRequest( SMB_NT_CREATE_CLIENT_REQUEST* pRequest )
{
    ASSERT( NULL != pRequest );
   
    TRACEMSG( ZONE_PROTOCOL, (L"\t\t[SMB_NT_CREATE_ANDX] Request") );
    TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tRootDirFID[0x%08x]", pRequest->RootDirectoryFID) );
    TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tFlags[0x%08x]", pRequest->Flags) );
    TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tDesiredAccess[0x%08x]", pRequest->DesiredAccess) );
    TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tFileAttributes[0x%08x]", pRequest->ExtFileAttributes) );
    TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tShareAccess[0x%08x]", pRequest->ShareAccess) );
    TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tDisposition[0x%08x]", pRequest->CreateDisposition) );
    TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tOptions[0x%08x]", pRequest->CreateOptions) );
    TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tImpersonationLevel[0x%08x]", pRequest->ImpersonationLevel) );
    TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tSecurityFlags[0x%02x]", (ULONG) pRequest->SecurityFlags) );
    TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tAllocationSize [0x%08x%08x]", pRequest->AllocationSize.HighPart, pRequest->AllocationSize.LowPart) );
}
void TraceProtocolSmbNtCreateRequestFileName( WCHAR* lpszFileName, SMB_NT_CREATE_CLIENT_REQUEST* pRequest )
{
    ASSERT( NULL != pRequest );

    TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tFileNameLength[%d]", pRequest->NameLength) );
    TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tFileName[%s]", lpszFileName) );
}



//
//  TraceProtocolSmbNtCreateResponse:
//
//      - dumps the outgoing response packet parameters
//
void TraceProtocolSmbNtCreateResponse( DWORD dwRetCode, SMB_NT_CREATE_SERVER_RESPONSE* pResponse )
{
    ASSERT( NULL != pResponse );

    TRACEMSG( ZONE_PROTOCOL, (L"\t\t[SMB_NT_CREATE_ANDX] Response [0x%08x]", dwRetCode ) );

    if( ERROR_SUCCESS == dwRetCode )
        {
        TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tFID[0x%04x]", pResponse->FID) );
        TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tDirectory?[%s]", ( pResponse->Directory ) ? L"yes" : L"no") );
        TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tOpLockLevel[0x%02x]", (ULONG) pResponse->OplockLevel) );
        TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tDeviceState[0x%04x]", (ULONG) pResponse->DeviceState) );
        TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tFileType[0x%04x]", (ULONG) pResponse->FileType) );
        TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tFileAttributes[0x%08x]", pResponse->ExtFileAttributes) );
        TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tCreateionAction[0x%08x]", pResponse->CreationAction) );
        TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tCreationTime[0x%08x%08x]", pResponse->CreationTime.dwHighDateTime, pResponse->CreationTime.dwLowDateTime) );
        TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tLastAccessTime[0x%08x%08x]", pResponse->LastAccessTime.dwHighDateTime, pResponse->LastAccessTime.dwLowDateTime) );
        TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tLastWriteTime[0x%08x%08x]", pResponse->LastWriteTime.dwHighDateTime, pResponse->LastWriteTime.dwLowDateTime) );
        TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tChangeTime[0x%08x%08x]", pResponse->ChangeTime.dwHighDateTime, pResponse->ChangeTime.dwLowDateTime) );
        TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tEndOfFile [0x%08x%08x]", pResponse->EndOfFile.HighPart, pResponse->EndOfFile.LowPart) );
        TRACEMSG( ZONE_PROTOCOL, (L"\t\t\tAllocationSize [0x%08x%08x]", pResponse->AllocationSize.HighPart, pResponse->AllocationSize.LowPart) );
        }
}
#endif
