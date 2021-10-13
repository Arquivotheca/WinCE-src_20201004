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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#include <SMB_Globals.h>
#include <SMBErrors.h>
#include <ShareInfo.h>
#include <utils.h>
#include <SMBPackets.h>
#include <ipcstream.h>
#include <RAPI.h>
#include <nmpipe.h>
#include <nmpioctl.h>
#include <connectionmanager.h>
#include <PktHandler.h>
#include <pc_net_prog.h>

static DWORD ConstructResponse(SMB_PROCESS_CMD *pRawResponse, RAPIBuilder *pRapi, UINT *puiUsed)
{
    SMB_COM_TRANSACTION_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION_SERVER_RESPONSE *)pRawResponse->pDataPortion;

    memset(pResponse, 0, sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE));

    pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE) - 3) / sizeof(WORD);  
    pResponse->TotalParameterCount = pRapi->ParamBytesUsed();
    pResponse->TotalDataCount = pRapi->DataBytesUsed();
    pResponse->ParameterCount = pRapi->ParamBytesUsed();
    pResponse->ParameterOffset = pRapi->ParamOffset((BYTE *)pRawResponse->pSMBHeader); 
    pResponse->ParameterDisplacement = 0;
    pResponse->DataCount = pRapi->DataBytesUsed();
    pResponse->DataOffset = pRapi->DataOffset((BYTE *)pRawResponse->pSMBHeader);
    pResponse->DataDisplacement = 0;
    pResponse->SetupCount = 0;
    pResponse->Reserved2 = 0;
    pResponse->ByteCount = pRapi->TotalBytesUsed();
    
    *puiUsed = pRapi->TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
    return 0;
}

static DWORD Smb_SetNamedPipeHandleState(SMB_PACKET *pSMB, IPCStream *pIpcStream, SMB_PROCESS_CMD *_pRawRequest, SMB_PROCESS_CMD *_pRawResponse, UINT *puiUsed, StringTokenizer &RequestTokenizer)
{
    DWORD dwState;
    HRESULT hr = RequestTokenizer.GetWORD((WORD *)&dwState);
    if (FAILED(hr))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error fetching device state from params!"));
        return ERROR_CODE(STATUS_INTERNAL_ERROR);
    }

    if (!SetNamedPipeHandleState(pIpcStream->GetPipeHandle(), &dwState, NULL, NULL))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-SetNamedPipeHandleState-- SetNamedPipeHandleState failed. GLE=0x%08x!!", GetLastError()));
        return ERROR_CODE(STATUS_BAD_DEVICE_TYPE);
    }

    SMB_COM_TRANSACTION_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    memset(pResponse, 0, sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE));
    pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE) - 3) / sizeof(WORD);  
    pResponse->TotalParameterCount = 0;
    pResponse->TotalDataCount = 0;
    pResponse->ParameterCount = 0;
    pResponse->ParameterOffset = (PBYTE)(pResponse+1) - (PBYTE)_pRawResponse->pSMBHeader;
    pResponse->ParameterDisplacement = 0;
    pResponse->DataCount = 0;
    pResponse->DataOffset = 0;
    pResponse->DataDisplacement = 0;
    pResponse->SetupCount = 0;
    pResponse->Reserved2 = 0;
    pResponse->ByteCount = 0;
    
    *puiUsed = sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
    return 0;
}

DWORD Smb_QueryNamedPipeHandleState(SMB_PACKET *pSMB, IPCStream *pIpcStream, SMB_PROCESS_CMD *_pRawRequest, SMB_PROCESS_CMD *_pRawResponse, UINT *puiUsed, StringTokenizer &RequestTokenizer)
{
    HRESULT hr;
    DWORD dwState, dwInst;
    WORD wReturnedState = 0;

    SMB_COM_TRANSACTION_CLIENT_REQUEST *pRequest = (SMB_COM_TRANSACTION_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_TRANSACTION_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    if (!GetNamedPipeHandleState(pIpcStream->GetPipeHandle(), &dwState, NULL, NULL, NULL, NULL, 0))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-Smb_GetNamedPipeHandleState-- GetNamedPipeHandleState failed. GLE=0x%08x!!", GetLastError()));
        return ERROR_CODE(STATUS_INTERNAL_ERROR);
    }

    if (!GetNamedPipeInfo(pIpcStream->GetPipeHandle(), NULL, NULL, NULL, &dwInst))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-Smb_GetNamedPipeHandleState-- GetNamedPipeInfo failed. GLE=0x%08x!!", GetLastError()));
        return ERROR_CODE(STATUS_INTERNAL_ERROR);
    }

    if (dwState & PIPE_NOWAIT)
        wReturnedState |= (1 << PIPE_COMPLETION_MODE_BITS);
    if (dwState & PIPE_READMODE_MESSAGE)
        wReturnedState |= (1 << PIPE_READ_MODE_BITS);

    wReturnedState |= (1 << PIPE_PIPE_END_BITS);    //it is the server end of the pipe

    wReturnedState |= LOWORD(dwInst) << PIPE_MAXIMUM_INSTANCES_BITS;

    WORD wRecvBufferSize = _pRawResponse->uiDataSize - sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
    RAPIBuilder RAPI((BYTE *)(pResponse+1), wRecvBufferSize, pRequest->MaxParameterCount, pRequest->MaxDataCount);

    WORD *pwRetParam = NULL;
    hr = RAPI.ReserveParams(sizeof(WORD), (PBYTE *)&pwRetParam);
    if (FAILED(hr) || !pwRetParam)
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-Smb_GetNamedPipeHandleState-- Out of memory!!"));
        return ERROR_CODE(STATUS_INTERNAL_ERROR);
    }

    ASSERT(pwRetParam);

    *pwRetParam = wReturnedState;

    return ConstructResponse(_pRawResponse, &RAPI, puiUsed);
}

DWORD Smb_QueryNamedPipeInfo(SMB_PACKET *pSMB, IPCStream *pIpcStream, SMB_PROCESS_CMD *_pRawRequest, SMB_PROCESS_CMD *_pRawResponse, UINT *puiUsed, StringTokenizer &RequestTokenizer)
{
    HRESULT hr;
    WORD wLevel;
    DWORD dwFlags, dwOutBuf, dwInBuf, dwMaxInst, dwCurInst;

    SMB_COM_TRANSACTION_CLIENT_REQUEST *pRequest = (SMB_COM_TRANSACTION_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_TRANSACTION_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    hr = RequestTokenizer.GetWORD(&wLevel);
    if (FAILED(hr))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error fetching device state from params!"));
        return ERROR_CODE(STATUS_INTERNAL_ERROR);
    }

    if (wLevel != 1)
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Smb_QueryNamedPipeInfo - Level %d information not supported!", wLevel));
        return ERROR_CODE(STATUS_INTERNAL_ERROR);
    }

    if ((!GetNamedPipeInfo(pIpcStream->GetPipeHandle(), &dwFlags, &dwOutBuf, &dwInBuf, &dwMaxInst)) || 
        (dwOutBuf > 0xffff) || (dwInBuf > 0xffff) || (dwMaxInst > PIPE_UNLIMITED_INSTANCES) )
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Smb_QueryNamedPipeInfo failed. !"));
        return ERROR_CODE(STATUS_INTERNAL_ERROR);
    }

    if (!GetNamedPipeHandleState(pIpcStream->GetPipeHandle(), NULL, &dwCurInst, NULL, NULL, NULL, 0))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Smb_QueryNamedPipeInfo -- GetNamedPipeHandleState failed. !"));
        return ERROR_CODE(STATUS_INTERNAL_ERROR);
    }

    WORD wRecvBufferSize = _pRawResponse->uiDataSize - sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
    RAPIBuilder RAPI((BYTE *)(pResponse+1), wRecvBufferSize, pRequest->MaxParameterCount, pRequest->MaxDataCount);

    PBYTE pbParams;
    hr = RAPI.ReserveParams(0, (PBYTE *)&pbParams);
    if (FAILED(hr))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-Smb_GetNamedPipeHandleState-- Out of memory!!"));
        return ERROR_CODE(STATUS_INTERNAL_ERROR);
    }

    NAMED_PIPE_INFORMATION_1 *pNmInfo;
    int nSize = sizeof(NAMED_PIPE_INFORMATION_1) + ( (wcslen(pIpcStream->GetPipeName())+1) * sizeof(WCHAR)) + sizeof(CHAR);    //+sizeof(CHAR) because PipeName falls on an odd address and needs to be aligned to copy a unicode string
    hr = RAPI.ReserveBlock(nSize, (PBYTE *)&pNmInfo, sizeof(WORD));    
    if (FAILED(hr))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-Smb_GetNamedPipeHandleState-- Out of memory!!"));
        return ERROR_CODE(STATUS_INTERNAL_ERROR);
    }

    ASSERT(pNmInfo);

    pNmInfo->OutputBufferSize = LOWORD(dwOutBuf);
    pNmInfo->InputBufferSize = LOWORD(dwInBuf);
    pNmInfo->MaximumInstances = LOBYTE(dwMaxInst);
    pNmInfo->CurrentInstances = LOBYTE(dwCurInst);
    pNmInfo->PipeNameLength = wcslen(pIpcStream->GetPipeName());

    WCHAR *pAlignedName = (WCHAR *)( ((DWORD )pNmInfo->PipeName) & (~1));
    wcscpy(pAlignedName, pIpcStream->GetPipeName() + 3);    //+3 to point to the pipe name beyond the initial "\\."

    return ConstructResponse(_pRawResponse, &RAPI, puiUsed);
}

DWORD Smb_TransactNamedPipe(SMB_PACKET *pSMB, IPCStream *pIpcStream, SMB_PROCESS_CMD *_pRawRequest, SMB_PROCESS_CMD *_pRawResponse, UINT *puiUsed, StringTokenizer &RequestTokenizer)
{
    SMB_COM_TRANSACTION_CLIENT_REQUEST *pRequest = (SMB_COM_TRANSACTION_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_TRANSACTION_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    DWORD dwInBufSize = pRequest->DataCount;
    PBYTE pInBuf = (PBYTE )_pRawRequest->pSMBHeader + pRequest->DataOffset;

    WORD wRecvBufferSize = _pRawResponse->uiDataSize - sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
    RAPIBuilder RAPI((BYTE *)(pResponse+1), wRecvBufferSize, pRequest->MaxParameterCount, pRequest->MaxDataCount);

    PBYTE pbParams;
    HRESULT hr = RAPI.ReserveParams(0, (PBYTE *)&pbParams);
    if (FAILED(hr))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-Smb_TransactNamedPipe-- Out of memory!!"));
        return ERROR_CODE(STATUS_INTERNAL_ERROR);
    }
    
    PBYTE pOutBuf;
    DWORD dwOutBufSize = pRequest->MaxDataCount;

    hr = RAPI.ReserveBlock(dwOutBufSize, &pOutBuf);
    if (FAILED(hr))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-Smb_TransactNamedPipe-- Out of memory!!"));
        return ERROR_CODE(STATUS_INTERNAL_ERROR);
    }

    DWORD dwRet;
    if (!TransactNamedPipe(pIpcStream->GetPipeHandle(), pInBuf, dwInBufSize, pOutBuf, dwOutBufSize, &dwRet, NULL))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-Smb_TransactNamedPipe-- TransactNamedPipe failed. GLE=0x%08x!!", GetLastError()));
        return ERROR_CODE(STATUS_INTERNAL_ERROR);    
    }

    ASSERT(dwRet <= 0xffff);

    ConstructResponse(_pRawResponse, &RAPI, puiUsed);
    pResponse->DataCount = LOWORD(dwRet);    //fix up data count to what TransactNamedPipe returned

    return 0;
}


DWORD SMB_TRANS_API_HandleNamedPipeFunction(WORD wFunc, WORD wFileId, SMB_PROCESS_CMD *_pRawRequest, SMB_PROCESS_CMD *_pRawResponse, UINT *puiUsed, StringTokenizer &RequestTokenizer, SMB_PACKET *pSMB)
{
    HRESULT hr;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    ce::smart_ptr<TIDState> pTIDState = NULL;
    
    //
    // Find our connection state        
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_TRANS_API_HandleNamedPipeFunction: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error -- trans2 had name but it was invalid"));
        return ERROR_CODE(STATUS_INTERNAL_ERROR);      
    }
        
    //
    // Find a share state 
    if(FAILED(pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_READ)) || !pTIDState) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_TRANS_API_HandleNamedPipeFunction -- couldnt find share state!!"));
        return ERROR_CODE(STATUS_INVALID_HANDLE);
    }    
    ce::smart_ptr<SMBFileStream> pStream;
    IPCStream *pIpcStream = NULL;
    hr = pTIDState->FindFileStream(wFileId, pStream);
    pIpcStream = (IPCStream *)((SMBFileStream *)pStream);
    
    if(FAILED(hr) || !pTIDState)
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-SetNamedPipeHandleState: Couldnt find file stream!!"));
        return ERROR_CODE(STATUS_INTERNAL_ERROR);
    }      
    
    DWORD dwRet;
    switch(wFunc)
    {
    case TRANSACT_SETNMPHANDSTATE:
        dwRet = Smb_SetNamedPipeHandleState(pSMB, pIpcStream, _pRawRequest, _pRawResponse, puiUsed, RequestTokenizer);
        break;
    case TRANSACT_QNMPHANDSTATE:
        dwRet = Smb_QueryNamedPipeHandleState(pSMB, pIpcStream, _pRawRequest, _pRawResponse, puiUsed, RequestTokenizer);
        break;
    case TRANSACT_NMPIPEINFO:
        dwRet = Smb_QueryNamedPipeInfo(pSMB, pIpcStream, _pRawRequest, _pRawResponse, puiUsed, RequestTokenizer);
        break;
    case TRANSACT_TRANSACTNMPIPE:
        dwRet = Smb_TransactNamedPipe(pSMB, pIpcStream, _pRawRequest, _pRawResponse, puiUsed, RequestTokenizer);
        break;
    default:
        ASSERT(FALSE);
        dwRet = ERROR_CODE(STATUS_NOT_SUPPORTED);
        break;
    }

    return dwRet;
}
