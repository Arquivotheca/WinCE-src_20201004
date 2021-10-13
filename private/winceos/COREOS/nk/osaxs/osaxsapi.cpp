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

#include "osaxs_p.h"
 
HRESULT OsAxsApiGetFlexiPTMInfo(OSAXS_COMMAND *cmd, STRING *MessageData)
{
    HRESULT hr;
    DWORD bufsize;

    MessageData->Length = 0;
    hr = E_FAIL;

    bufsize = MessageData->MaximumLength;
    hr = GetFPTMI(&cmd->u.FlexiReq, &bufsize,
            reinterpret_cast<BYTE*>(MessageData->Buffer));
    if (SUCCEEDED(hr))
    {
        MessageData->Length = static_cast<unsigned short>(bufsize);
    }

    return hr;
}

#ifdef x86
HRESULT OsAxsApiGetExceptionRegistration(OSAXS_COMMAND *cmd, STRING* MessageData)
{
    MessageData->Length = 0;
    return GetExceptionRegistration(cmd->u.ExReg.rgdwBuf);
}
#endif

HRESULT OsAxsApiTranslateAddress(OSAXS_COMMAND *cmd, STRING *MessageData)
{
    HRESULT hr;
    void *taddr;
    HANDLE const hproc = reinterpret_cast<HANDLE>(cmd->u.TranslateAddress.dwProcessHandle);
    void * const addr  = reinterpret_cast<void *>(cmd->u.TranslateAddress.Address);

    MessageData->Length = 0;
    
    hr = OsAxsTranslateAddress(hproc, addr, cmd->u.TranslateAddress.fReturnKVA,
                               &taddr);
    if (SUCCEEDED(hr))
    {
        cmd->u.TranslateAddress.PhysicalAddrOrKVA = reinterpret_cast<DWORD>(taddr);
    }

    return hr;
}

HRESULT OsAxsApiTranslateHPCI(OSAXS_COMMAND *cmd, STRING *MessageData)
{
    PCCINFO cinfo;
    HRESULT hr;

    MessageData->Length = 0;
    cinfo = OsAxsHandleToPCInfo(reinterpret_cast<HANDLE>(cmd->u.Handle.dwHandle),
                                cmd->u.Handle.dwProcessHandle);
    if (cinfo)
    {
        KDASSERT(sizeof(*cinfo) <= MessageData->MaximumLength);
        OSAXS_HANDLE_CINFO cinfoApi = {0};
        memcpy (&cinfoApi, cinfo, min (sizeof (*cinfo), sizeof (cinfoApi)));
        memcpy (MessageData->Buffer, &cinfoApi, sizeof (cinfoApi));
        MessageData->Length = sizeof (cinfoApi);        
        hr = S_OK;
    }
    else
    {
        hr = E_FAIL;
    }

    return hr;
}

HRESULT OsAxsApiGetHdata(OSAXS_COMMAND *cmd, STRING *MessageData)
{
    HRESULT hr;
    PHDATA hdata;

    MessageData->Length = 0;
    hdata = OsAxsHandleToPHData(reinterpret_cast<HANDLE>(cmd->u.Handle.dwHandle),
                                cmd->u.Handle.dwProcessHandle);
    if (hdata)
    {
        OSAXS_HANDLE_DATA hdataApi = {0};
        memcpy (&hdataApi, hdata, min (sizeof (hdataApi), sizeof (*hdata)));
        memcpy (MessageData->Buffer, &hdataApi, sizeof (hdataApi));
        MessageData->Length = sizeof (hdataApi);        
        hr = S_OK;
    }
    else
    {
        hr = E_FAIL;
    }

    return hr;
}

HRESULT OsAxsApiTranslateRA(OSAXS_COMMAND *cmd, STRING *MessageData)
{
    HRESULT hr;
    DWORD dwState;
    DWORD dwReturn;
    DWORD dwFrame;
    DWORD dwFrameProc;

    MessageData->Length = 0;
    
    dwState     = cmd->u.ThreadTranslateReturn.dwState;
    dwReturn    = cmd->u.ThreadTranslateReturn.dwReturn;
    dwFrame     = cmd->u.ThreadTranslateReturn.dwFrame;
    dwFrameProc = cmd->u.ThreadTranslateReturn.dwFrameCurProcHnd;
    hr = OsAxsTranslateReturnAddress(cmd->u.ThreadTranslateReturn.hThread,
                                     &dwState,
                                     &dwReturn,
                                     &dwFrame,
                                     &dwFrameProc);
    if (SUCCEEDED(hr))
    {
        cmd->u.ThreadTranslateReturn.dwNewState = dwState;
        cmd->u.ThreadTranslateReturn.dwNewReturn = dwReturn;
        cmd->u.ThreadTranslateReturn.dwNewFrame = dwFrame;
        cmd->u.ThreadTranslateReturn.dwNewProcHnd = dwFrameProc;
    }
    return hr;
}

HRESULT OsAxsApiGetCpuPCur(OSAXS_COMMAND *cmd, STRING *MessageData)
{
    HRESULT hr;
    PROCESS *vm;
    PROCESS *actv;
    THREAD *thd;

    MessageData->Length = 0;
    
    hr = OsAxsGetCpuPCur(cmd->u.Cpu.dwCpuNum, &vm, &actv, &thd);
    if (SUCCEEDED(hr))
    {
        if (vm)
        {
            cmd->u.Cpu.hCurrentVM = vm->dwId;
            cmd->u.Cpu.ullCurrentVM = reinterpret_cast<ULONGLONG>(vm);
        }
        if (actv)
        {
            cmd->u.Cpu.hCurrentProcess = actv->dwId;
            cmd->u.Cpu.ullCurrentProcess = reinterpret_cast<ULONGLONG>(actv);
        }
        if (thd)
        {
            cmd->u.Cpu.hCurrentThread = thd->dwId;
            cmd->u.Cpu.ullCurrentThread = reinterpret_cast<ULONGLONG>(thd);
        }
    }

    return hr;
}

HRESULT OsAxsApiGetThreadContext(OSAXS_COMMAND *cmd, STRING *MessageData)
{
    HRESULT hr;
    THREAD *thd;

    MessageData->Length = 0;

    thd = OsAxsHandleToThread(cmd->u.hThread);
    if (thd)
    {
        hr = OsAxsGetThreadContext(thd,
                reinterpret_cast<PCONTEXT>(MessageData->Buffer),
                sizeof(CONTEXT));
        if (SUCCEEDED(hr))
        {
            MessageData->Length = sizeof(CONTEXT);
        }
    }
    else
    {
        hr = E_FAIL;
    }

    return hr;
}

HRESULT OsAxsApiSetThreadContext(OSAXS_COMMAND *cmd, STRING *MessageData)
{
    HRESULT hr;
    THREAD *thd;

    thd = OsAxsHandleToThread(cmd->u.hThread);
    if (thd)
    {
        hr = OsAxsSetThreadContext(thd,
                reinterpret_cast<PCONTEXT>(MessageData->Buffer),
                MessageData->Length);
    }
    else
    {
        hr = E_FAIL;
    }

    MessageData->Length = 0;
    return hr;
}

static BOOL resendNotification;
static BOOL validOsAxsPacket;

void OsAxsApiReset()
{

}

BOOL OsAxsSendResponse(
    STRING * pResponseHeader,
    STRING * pAdditionalData
)
{
    OSAXS_COMMAND cmd;
    GUID guidClient;
    ULONG Length;
    STRING MessageData;
    USHORT ReturnCode;
    BOOL fSuccess = FALSE;
    OSAXS_API_WATSON_DUMP_OTF *pOtfResponse = &(((OSAXS_COMMAND *) pResponseHeader->Buffer)->u.GetWatsonOTF);
    BOOL fProcessPacket = TRUE;

    DD_KDBGSend(PACKET_TYPE_KD_CMD, GUID_KDDBGCLIENT_OSAXS, pResponseHeader, pAdditionalData);
    
    validOsAxsPacket = FALSE;
    
    MessageData.MaximumLength = static_cast<unsigned short>(DD_kdbgBufferSize);
    MessageData.Buffer = (PCHAR) DD_kdbgBuffer;

    ReturnCode = DD_KDBGRecv(&MessageData, &Length, &guidClient);
    if (ReturnCode == (USHORT) KDP_PACKET_RESEND)
    { // This means the host disconnected and reconnected
        DEBUGGERMSG(OXZONE_ALERT, (L"  KdpSendOsAxsResponse: RESENDING NOTIFICATION\r\n"));
        resendNotification = TRUE;
    }
    else
    {
        KDASSERT(MessageData.Length >= sizeof(cmd));
        
        memcpy(&cmd, MessageData.Buffer, sizeof(cmd));
        MessageData.Length -= sizeof(cmd);
        memmove(MessageData.Buffer, MessageData.Buffer + sizeof(cmd),
                MessageData.Length);

        if (fProcessPacket && memcmp(&guidClient, &GUID_KDDBGCLIENT_OSAXS, sizeof(GUID)))
        {
            DEBUGGERMSG (OXZONE_ALERT, (L"  KdpSendOsAxsResponse: incorrect client type, not OsAxs client.\r\n"));
            fProcessPacket = FALSE;
        }

        if (fProcessPacket && cmd.dwVersion < OSAXS_PROTOCOL_LATEST_VERSION_TSBC_WITH)
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  KdpSendOsAxsResponse: Host is too old. Incompatible with protocol.\r\n"));
            fProcessPacket = FALSE;
        }

        if (fProcessPacket &&
            OSAXS_API_GET_WATSON_DUMP_FILE != cmd.dwApi &&
            OSAXS_API_GET_WATSON_DUMP_FILE_COMPAT != cmd.dwApi)
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  KdpSendOsAxsResponse: Unexpected API number, Expecting %d or %d, got %d\r\n",
                        OSAXS_API_GET_WATSON_DUMP_FILE,
                        OSAXS_API_GET_WATSON_DUMP_FILE_COMPAT,
                        cmd.dwApi));
            fProcessPacket = FALSE;
        }

        if (fProcessPacket && cmd.u.GetWatsonOTF.dwDumpType != 0)
        {
            if (cmd.u.GetWatsonOTF.dwDumpType != pOtfResponse->dwDumpType)
            {
                DEBUGGERMSG (OXZONE_ALERT, (L"  KdpSendOsAxsResponse: Wrong dump type.\r\n"));
                fProcessPacket = FALSE;
            }
            else if (cmd.u.GetWatsonOTF.dw64SegmentIndex != (pOtfResponse->dw64SegmentIndex + pOtfResponse->dwSegmentBufferSize))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  KdpSendOsAxsResponse: Next segment index is wrong.\r\n"));
                fProcessPacket = FALSE;
            }
        }

        if (fProcessPacket)
        {
            validOsAxsPacket = TRUE; 
            if (cmd.u.GetWatsonOTF.dwDumpType == 0)
            { // Cancel - abort
                DEBUGGERMSG(OXZONE_ALERT, (L"  KdpSendOsAxsResponse: Cancel upload\r\n"));
            }
            else
            { // Success - continue
                fSuccess = TRUE;
            }
        }
        else
        { // Cancel - retry
            DEBUGGERMSG(OXZONE_ALERT, (L"  KdpSendOsAxsResponse: unexpected command while waiting for Next Response Block command\r\n"));
            DEBUGGERMSG(OXZONE_ALERT, (L"  KdpSendOsAxsResponse: OsAxsVersion=%i, OsAxsApi=%i, NewDumpType=%i, OldDumpType=%i, NewSegIdx=%08X, OldSegIdx=%08X\r\n", 
                    cmd.dwVersion, cmd.dwApi, cmd.u.GetWatsonOTF.dwDumpType,
                    pOtfResponse->dwDumpType, (DWORD) cmd.u.GetWatsonOTF.dw64SegmentIndex,
                    (DWORD) (pOtfResponse->dw64SegmentIndex + pOtfResponse->dwSegmentBufferSize)));

            validOsAxsPacket = FALSE;
        }
    }
    return fSuccess;
}

HRESULT OsAxsApiGetWatsonDump(OSAXS_COMMAND *cmd, STRING *MessageData)
{
    HRESULT hr;
    DWORD size;

    resendNotification = FALSE;
    validOsAxsPacket = FALSE;

    size = MessageData->MaximumLength;
    hr = CaptureDumpFileOnTheFly(&cmd->u.GetWatsonOTF, reinterpret_cast<BYTE*>(MessageData->Buffer),
            &size);
    if (FAILED(hr))
    {
        // What happened?
        if (resendNotification)
        {
        }
    }
    else
    {
        MessageData->Length = (USHORT)size;        
    }

    return hr;
}

void OsAxsHandleOsAxsApi(STRING *MessageData)
{
    OSAXS_COMMAND cmd;
    STRING ResponseHeader;
    HRESULT hr;

    ResponseHeader.Length = ResponseHeader.MaximumLength = sizeof(cmd);
    ResponseHeader.Buffer = reinterpret_cast<CHAR *>(&cmd);

    KDASSERT(MessageData->Length >= sizeof(cmd));
    if (MessageData->Length >= sizeof(cmd))
    {
        memcpy(&cmd, MessageData->Buffer, sizeof(cmd));
        MessageData->Length -= sizeof(cmd);
        memmove(MessageData->Buffer, MessageData->Buffer + sizeof(cmd),
                MessageData->Length);
        hr = E_FAIL;
        
        if (cmd.dwVersion >= OSAXS_PROTOCOL_LATEST_VERSION_TSBC_WITH)
        {
            switch (cmd.dwApi)
            {
            case OSAXS_API_GET_FLEXIPTMINFO:
                hr = OsAxsApiGetFlexiPTMInfo(&cmd, MessageData);
                break;
 
            case OSAXS_API_GET_WATSON_DUMP_FILE:
            case OSAXS_API_GET_WATSON_DUMP_FILE_COMPAT:
                    hr = OsAxsApiGetWatsonDump(&cmd, MessageData);
                break;
 
#ifdef x86
            case OSAXS_API_GET_EXCEPTION_REGISTRATION:
                hr = OsAxsApiGetExceptionRegistration(&cmd, MessageData);
                break;
#endif
            case OSAXS_API_SWITCH_PROCESS_VIEW:
#if 0
                g_pFocusProcOverride = (PPROCESS) (DWORD) pOsAxsCmd->u.Addr;
#endif
                hr = S_OK;
                break;

            case OSAXS_API_TRANSLATE_ADDRESS:
                hr = OsAxsApiTranslateAddress(&cmd, MessageData);
                break;

            case OSAXS_API_TRANSLATE_HPCI:
                hr = OsAxsApiTranslateHPCI(&cmd, MessageData);
                break;

            case OSAXS_API_GET_HDATA:
                hr = OsAxsApiGetHdata(&cmd, MessageData);
                break;

            case OSAXS_API_TRANSLATE_RA:
                hr = OsAxsApiTranslateRA(&cmd, MessageData);
                break;
                
            case OSAXS_API_CPU_PCUR:
                hr = OsAxsApiGetCpuPCur(&cmd, MessageData);
                break;

            case OSAXS_API_GET_THREAD_CONTEXT:
                hr = OsAxsApiGetThreadContext(&cmd, MessageData);
                break;
            case OSAXS_API_SET_THREAD_CONTEXT:
                hr = OsAxsApiSetThreadContext(&cmd, MessageData);
                break;
            
            default:
                hr = OSAXS_E_APINUMBER;
                break;
            }
        }
        else
        {
            hr = OSAXS_E_PROTOCOLVERSION;
        }

        // Manufacture response
        cmd.hr = hr;
        cmd.dwVersion = OSAXS_PROTOCOL_LATEST_VERSION;
        
        DD_KDBGSend(PACKET_TYPE_KD_CMD, GUID_KDDBGCLIENT_OSAXS, &ResponseHeader, MessageData);
    }
}

/* OsAxsSendKeepAlive()
 * Send a debug message to the desktop to let the desktop know
 * the device is still alive.
 */
void OsAxsSendKeepAlive()
{
    CHAR cKeepAlive[] = "OsAxs KeepAlive\r\n";
    DD_KDBGDebugMessage(cKeepAlive, _countof(cKeepAlive));
}
