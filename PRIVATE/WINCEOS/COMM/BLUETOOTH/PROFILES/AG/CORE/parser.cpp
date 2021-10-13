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

#include "btagpriv.h"

typedef void (CAGEngine::*PFNCMDPROC)(LPSTR pszParams, int cchParam);

typedef struct _AT_CMD_TBL {
    LPSTR pszCommand;
    UINT uiCmdLen;
    PFNCMDPROC pfnHandler;
} AT_CMD_TBL, *PAT_CMD_TBL;

AT_CMD_TBL ATCmdTable[] = {
    "AT+CKPD=200", 11, &CAGEngine::OnHeadsetButton,
    "AT+VGM=", 7, &CAGEngine::OnMicVol,
    "AT+VGS=", 7, &CAGEngine::OnSpeakerVol,
    "AT+BRSF=", 8, &CAGEngine::OnSupportedFeatures,
    "AT+CIND?", 8, &CAGEngine::OnReadIndicators,
    "AT+CIND=?", 9, &CAGEngine::OnTestIndicators,
    "AT+CMER=", 8, &CAGEngine::OnRegisterIndicatorUpdates,
    "ATA", 3, &CAGEngine::OnAnswerCall,
    "AT+CHUP", 7, &CAGEngine::OnHangupCall,
    "ATD>", 4, &CAGEngine::OnDialMemory,
    "ATD", 3, &CAGEngine::OnDial,
    "AT+BLDN", 7, &CAGEngine::OnDialLast,
    "AT+CCWA=", 8, &CAGEngine::OnEnableCallWaiting,
    "AT+CLIP=", 8, &CAGEngine::OnEnableCLI,
    "AT+VTS=", 7, &CAGEngine::OnDTMF,
    "AT+CHLD=", 8, &CAGEngine::OnCallHold,
    "AT+BVRA=", 8, &CAGEngine::OnVoiceRecognition,
    "ATH", 3, &CAGEngine::OnHangupCall,
    "", 0, NULL
};

#ifdef DEBUG

void DbgPrintATCmd(DWORD dwZone, LPCSTR szCommand, int cbCommand)
{
    CHAR szDebug[MAX_DEBUG_BUF];

    for (int i = 0, j = 0; i < cbCommand; i++, j++) {
        if (szCommand[i] == '\r') {
            szDebug[j] = '<'; j++;
            szDebug[j] = 'c'; j++;
            szDebug[j] = 'r'; j++;
            szDebug[j] = '>';
        }
        else if (szCommand[i] == '\n') {
            szDebug[j] = '<'; j++;
            szDebug[j] = 'l'; j++;
            szDebug[j] = 'f'; j++;
            szDebug[j] = '>';
        }
        else {
            szDebug[j] = szCommand[i];
        }
    }

    szDebug[j] = '\0';

    DEBUGMSG(dwZone, (L"%hs", szDebug));
}

#endif // DEBUG


// This function wraps winsock recv
DWORD MyRecv(SOCKET s, LPSTR szBuf, DWORD cbBuf)
{ 
    DEBUGMSG(ZONE_PARSER, (L"BTAGSVC: My Recv on sock:%d szBuf:0x%X cbBuf:%d\n", s, szBuf, cbBuf));
    
    DWORD cbTotalRead = recv(s, szBuf, cbBuf, 0);

    if (0 == cbTotalRead) {
        // Socket was gracefully disconnected
        DEBUGMSG(ZONE_PARSER, (L"BTAGSVC: The client socket was gracefully disconnected.\n"));
    }
    else if (SOCKET_ERROR == cbTotalRead) {
        // Socket was forcefully closed
        DEBUGMSG(ZONE_PARSER, (L"BTAGSVC: The client socket was forcefully disconnected.\n"));
        cbTotalRead = 0;
    }

#ifdef DEBUG
    DEBUGMSG(ZONE_PARSER, (L"BTAGSVC: MyRecv - Read a chunk of data (size:%d) (read:%d) -- ", cbBuf, cbTotalRead));
    PREFAST_SUPPRESS(386, "cbTotalRead is <= cbBuf");
    DbgPrintATCmd(ZONE_PARSER, szBuf, cbTotalRead);
    DEBUGMSG(ZONE_PARSER, (L"\n"));
#endif // DEBUG

    return cbTotalRead;
}


CATParser::CATParser(void)
{
    m_sockClient = INVALID_SOCKET;
    m_pHandler = NULL;
    m_hThread = NULL;
    m_fShutdown = FALSE;
    m_cbUnRead = 0;

    m_hBtExtHandler = NULL;
    m_hPBHandler = NULL;
    
    m_pfnBthAGExtATHandler = NULL;
    m_pfnBthAGExtATSetCallback = NULL;
    m_pfnBthAGOnVoiceTag = NULL;

    m_pfnBthAGPBHandler = NULL;
    m_pfnBthAGPBSetCallback = NULL;
}

DWORD WINAPI CATParser::ATParserThread(LPVOID pv)
{
    CATParser* pInst = (CATParser*)pv;
    pInst->ATParserThread_Int();

    return 0;
}


// This method reads one command from the peer device.
DWORD CATParser::ReadCommand(SOCKET s, CBuffer& buffCommand)
{
    CHAR* pszBuf = (LPSTR) m_buffRecv.GetBuffer(0);
    DWORD cbTotalRead = m_cbUnRead; 
    DWORD cbCommand = 0;

    ASSERT(IsLocked());

    while (1) {
        if (cbCommand == cbTotalRead) {
            // Need to recv more data
            int cbBuff = cbCommand*2;

            pszBuf = (LPSTR) m_buffRecv.GetBuffer(cbBuff, TRUE);
            if (! pszBuf) {
                DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Parser - Out of resources.\n"));
                cbTotalRead = 0;                
                break;
            }

            cbBuff = m_buffRecv.GetSize();

            pszBuf += cbTotalRead;

            Unlock();
            DWORD cbRead = MyRecv(s, pszBuf, cbBuff-cbTotalRead);
            Lock();

            // Reset pointer to start of buffer
            pszBuf = (LPSTR) m_buffRecv.GetBuffer(0);
            
            if (0 == cbRead) {
                cbTotalRead = 0;
                break;
            }

            cbTotalRead += cbRead;
        }
        
        if ((cbCommand > 0) && (pszBuf[cbCommand] == '\r')) {
            // We have read the trailing <cr>.  Check for <lf> and return the command.
            if (pszBuf[cbCommand+1] == '\n') {
                cbCommand++;
            }

            cbCommand++;
            
            LPSTR pszCommand = (LPSTR) buffCommand.GetBuffer(cbCommand + 1);
            if (! pszCommand) {
                DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Out of resources!\n"));
                cbTotalRead = 0;
                break;
            }
            
            memcpy(pszCommand, pszBuf, cbCommand);
            break;
        }

        cbCommand++;
    }

    if (cbTotalRead) {
        // We have successfully read a command.  See if we have valid data left in our
        // recv buffer and keep track of how much for next call to ReadCommand.

        ASSERT(cbCommand <= cbTotalRead);

        if (cbCommand < cbTotalRead) {
            m_cbUnRead = cbTotalRead - cbCommand;
            memmove(pszBuf, (pszBuf + cbCommand), m_cbUnRead);
        }
        else {
            m_cbUnRead = 0;
        }
    }
    else {
        m_cbUnRead = 0;
        cbCommand = 0;
    }
    
    return cbCommand;
}


// This method reads and parses a command from the peer device.
void CATParser::ATParserThread_Int(void)
{
    DEBUGMSG(ZONE_PARSER, (L"BTAGSVC: ATParserThread started.\n"));
    
    Lock();
    
    while (1) {
        CBuffer buffCommand;
        CBuffer buffParam;
        LPSTR pszBuf;
        LPSTR pszParam;
        DWORD cbCommand;
        SOCKET s = m_sockClient;

        DEBUGMSG(ZONE_PARSER, (L"BTAGSVC: Parser thread is calling recv again.\n"));

        cbCommand = ReadCommand(s, buffCommand);

        if (m_fShutdown || (0 == cbCommand)) {
            DEBUGMSG(ZONE_PARSER, (L"BTAGSVC: Signalled to break from ATParserThread_Int.\n"));
            
            Unlock();
            m_pHandler->CloseAGConnection(TRUE);
            Lock();
            
            break;
        }

        pszBuf = (LPSTR) buffCommand.GetBuffer(0);
        if (! pszBuf) {
            DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Out of resources!\n"));
            break;
        }

        pszBuf[cbCommand] = '\0';

#ifdef DEBUG
        DEBUGMSG(ZONE_PARSER, (L"BTAGSVC: ATParserThread - Data was received: "));
        DbgPrintATCmd(ZONE_PARSER, pszBuf, cbCommand);
        DEBUGMSG(ZONE_PARSER, (L"\n"));
#endif // DEBUG

        //
        // See if external parsers wants to handle this
        //
        if (m_pfnBthAGExtATHandler && m_pfnBthAGExtATHandler(pszBuf, cbCommand)) {
            // Command handled
            continue;
        }

        if (m_pfnBthAGPBHandler && m_pfnBthAGPBHandler(pszBuf, cbCommand)) {
            // Command handled
            continue;
        }

        //
        // Loop through table of AT commands trying to find a match.
        //
        
        int i = 0;
        BOOL fHandled = FALSE;
        while ((! fHandled) && (! m_fShutdown)) {
            if (ATCmdTable[i].pszCommand[0] == 0) {
                if (! _stricmp(pszBuf, "\r\nOK\r\n")) {
                    m_pHandler->OnOK();
                }
                else if (! _stricmp(pszBuf, "\r\nERROR\r\n")) {
                    m_pHandler->OnError();                
                }
                else {
                    DEBUGMSG(ZONE_PARSER, (L"BTAGSVC: AT Command is not handled.\n"));
                    m_pHandler->OnUnknownCommand();
                }

                fHandled = TRUE;
            }
            else if (! _strnicmp(pszBuf, ATCmdTable[i].pszCommand, ATCmdTable[i].uiCmdLen)) {
                // Handle AT command
                //
                DEBUGMSG(ZONE_PARSER, (L"BTAGSVC: AT Command is being handled.\n"));

                int cchParam = (cbCommand - ATCmdTable[i].uiCmdLen) - 1;

                pszParam = (LPSTR) buffParam.GetBuffer(cchParam + 1);
                if (! pszParam) {
                    DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: Out of resources!\n"));
                    break;
                }
                
                if (0 == cchParam) {
                    pszParam[0] = '\0';
                }
                else {
                    strncpy(pszParam, (pszBuf + ATCmdTable[i].uiCmdLen), cchParam);
                    pszParam[cchParam] = '\0'; // Null terminate removing trailing <cr>
                }

                Unlock();
                (m_pHandler->*(ATCmdTable[i].pfnHandler))(pszParam, cchParam);
                Lock();
                fHandled = TRUE;
            }
            
            i++;
        }
    }

    Unlock();

    if (! m_fShutdown) {
        Stop();
    }

    DEBUGMSG(ZONE_PARSER, (L"BTAGSVC: ATParserThread exiting.\n"));
}


DWORD SendATCommandCallback(LPSTR szCommand, DWORD cbCommand)
{
    DEBUGMSG(ZONE_WARN, (L"BTAGSVC: SendATCommandCallback - external handler is sending a command.\n"));

    if (! g_pAGEngine) {
        return ERROR_NOT_READY;
    }
    
    return g_pAGEngine->ExternalSendATCommand(szCommand, cbCommand);
}

void SetCallback(PFN_BthAGATSetCallback pfn)
{
    __try {
        pfn(SendATCommandCallback);
    } __except (1) {
        DEBUGMSG(ZONE_ERROR, (L"BTAGSVC: SetCallback - exception in call to BthAGExtATSetCallback.\n", BTATHANDLER_HANDLER_API));
    }
}


// This method initializes the parser
DWORD CATParser::Init(void)
{
    DWORD cbRead;
    WCHAR wszExtHandlerMod[MAX_PATH];
    WCHAR wszPBHandlerMod[MAX_PATH];
    BOOL fUseDefaultMod = TRUE;
    BOOL fPhoneBook = FALSE;
    HKEY hk;

    DWORD dwErr = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RK_AUDIO_GATEWAY, 0, 0, &hk);
    if (dwErr == ERROR_SUCCESS) {
        cbRead = sizeof(wszExtHandlerMod);    
        dwErr = RegQueryValueEx(hk, _T("BTAGExtModule"), 0, NULL, (PBYTE)wszExtHandlerMod, &cbRead);
        if (dwErr == ERROR_SUCCESS) {
            fUseDefaultMod = FALSE;
        }

        // We need to add a 2nd extension for Windows Mobile.  On Windows Mobile we will call into
        // this extension to allow it to parse HFP Phone Book commands.  This needs to be different
        // from the above extension since that one is for OEMs.
        cbRead = sizeof(wszPBHandlerMod);    
        dwErr = RegQueryValueEx(hk, _T("BTAGPBModule"), 0, NULL, (PBYTE)wszPBHandlerMod, &cbRead);
        if (dwErr == ERROR_SUCCESS) {
            fPhoneBook = TRUE;
        }

        RegCloseKey(hk);
    }

    if (fUseDefaultMod) {
        m_hBtExtHandler = LoadLibrary(DEFAULT_BTEXTHANDLER_MODULE);
    }
    else {
        m_hBtExtHandler = LoadLibrary(wszExtHandlerMod);
    }

    if (m_hBtExtHandler) {
        // We check these function pointers are valid before calling the functions
        m_pfnBthAGOnVoiceTag = (PFN_BthAGOnVoiceTag) GetProcAddress(m_hBtExtHandler, BTAGEXT_ON_VOICETAG);
        m_pfnBthAGExtATHandler = (PFN_BthAGATHandler) GetProcAddress(m_hBtExtHandler, BTATHANDLER_HANDLER_API);
        m_pfnBthAGExtATSetCallback = (PFN_BthAGATSetCallback) GetProcAddress(m_hBtExtHandler, BTATHANDLER_SETCALLBACK_API);
        
        if (m_pfnBthAGExtATSetCallback) {
            SetCallback(m_pfnBthAGExtATSetCallback);
        }        
    }
    
    if (fPhoneBook) {
        m_hPBHandler = LoadLibrary(wszPBHandlerMod);
        if (m_hPBHandler) {
            // We check these function pointers are valid before calling the functions    
            m_pfnBthAGPBHandler = (PFN_BthAGATHandler) GetProcAddress(m_hPBHandler, BTATHANDLER_HANDLER_API);
            m_pfnBthAGPBSetCallback = (PFN_BthAGATSetCallback) GetProcAddress(m_hPBHandler, BTATHANDLER_SETCALLBACK_API);
            
            if (m_pfnBthAGPBSetCallback) {
                SetCallback(m_pfnBthAGPBSetCallback);
            }
        }
    }       

    return ERROR_SUCCESS;
}


// This method deinitializes the parser
void CATParser::Deinit(void)
{
    if (m_hBtExtHandler) {
       FreeLibrary(m_hBtExtHandler);
       m_hBtExtHandler = NULL;
    }

    if (m_hPBHandler) {
        FreeLibrary(m_hPBHandler);
        m_hPBHandler = NULL;
    }

    m_pfnBthAGExtATHandler = NULL;
    m_pfnBthAGExtATSetCallback = NULL;
    m_pfnBthAGOnVoiceTag = NULL;

    m_pfnBthAGPBHandler = NULL;
    m_pfnBthAGPBSetCallback = NULL;
}


// This method is called to start the parser module
DWORD CATParser::Start(CAGEngine* pHandler, SOCKET sockClient)
{
    DWORD dwRetVal = ERROR_SUCCESS;

    DEBUGMSG(ZONE_PARSER, (L"BTAGSVC: AT Command Parser is being started.\n"));

    Lock();

    if (m_hThread) {
        DEBUGMSG(ZONE_WARN, (L"BTAGSVC: Already started AT Parser.\n"));
        dwRetVal = ERROR_ALREADY_INITIALIZED;
        goto exit;
    }

    m_pHandler      = pHandler;
    m_sockClient    = sockClient;
    m_fShutdown     = FALSE;
    m_cbUnRead      = 0;
    
    m_hThread = CreateThread(NULL, 0, ATParserThread, this, 0, NULL);
    if (! m_hThread) {
        dwRetVal = GetLastError();
        goto exit;
    }

exit:
    Unlock();
    return dwRetVal;
}


// This method is called to stop the parser module
void CATParser::Stop(void)
{
    DEBUGMSG(ZONE_PARSER, (L"BTAGSVC: Stopping AT Command Parser.\n"));
    
    Lock();

    HANDLE h = m_hThread;
    m_fShutdown = TRUE;
    m_hThread = NULL;

    if (m_sockClient != INVALID_SOCKET) {
        DEBUGMSG(ZONE_PARSER, (L"BTAGSVC: Closing client socket %d.\n", m_sockClient));
        closesocket(m_sockClient);
        m_sockClient = INVALID_SOCKET;
        BthAGPhoneExtEvent(AG_PHONE_EVENT_BT_CTRL, 0, NULL);
    }
    
    Unlock();

    if (h) {
        if (h != (HANDLE) GetCurrentThreadId()) {
            DEBUGMSG(ZONE_PARSER, (L"BTAGSVC: Waiting for ATParserThread (id=%d) to exit.\n", h));
            WaitForSingleObject(h, INFINITE);
        }

        CloseHandle(h);
    }

    DEBUGMSG(ZONE_PARSER, (L"BTAGSVC: AT Command Parser is stopped.\n"));
}



