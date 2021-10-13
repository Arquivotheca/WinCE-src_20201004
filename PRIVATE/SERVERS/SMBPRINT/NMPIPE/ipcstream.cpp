//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <SMB_Globals.h>
#include <ShareInfo.h>
#include <ipcstream.h>
#include <nmpipe.h>

const WCHAR c_szPipePrefix[] = L"\\\\.\\pipe\\";

IPCStream::IPCStream(TIDState *pMyState, USHORT usFID) : SMBFileStream(FILE_STREAM, pMyState)    
{
    ASSERT(usFID != 0xFFFF);
    ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);

    this->SetFID(usFID);
    _fOpened = FALSE;
}

IPCStream::~IPCStream()
{    
    ASSERT(0xFFFF != this->FID());
    ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    
    //
    // If our file hasnt been closed, close it now
    if(_fOpened) {
        TRACEMSG(ZONE_IPC, (L"SMB-FILESTREAM: Closing up file because the TID is going away and Close() wasnt called!"));
        this->Close();
    }
}

HRESULT 
IPCStream::Read(BYTE *pDest, 
                DWORD dwOffset, 
                DWORD dwReqSize, 
                DWORD *pdwRead)
{
    TRACEMSG(ZONE_IPC, (L"IPCStream::Read pRead=0x%08x, dwOffset=0x%x, dwReqSize=0x%x, pdwRead=0x%08x\n", pDest, dwOffset, dwReqSize, pdwRead));
    if (ReadNamedPipe(_hPipe, pDest+dwOffset, dwReqSize, pdwRead, NULL)) {
        return S_OK;
    }
    return E_FAIL;
}

HRESULT 
IPCStream::Write(BYTE *pSrc, 
                DWORD dwOffset, 
                DWORD dwReqSize, 
                DWORD *pdwWritten)
{
    TRACEMSG(ZONE_IPC, (L"IPCStream::Write pSrc=0x%08x, dwOffset=0x%x, dwReqSize=0x%x, pdwWritten=0x%08x\n", pSrc, dwOffset, dwReqSize, pdwWritten));

    if (WriteNamedPipe(_hPipe, pSrc+dwOffset, dwReqSize, pdwWritten, NULL)) {
        return S_OK;
    }
    return E_FAIL;
}

HRESULT 
IPCStream::Open(const WCHAR *pFileName, 
                DWORD dwAccess,
                DWORD dwDisposition,
                DWORD dwAttributes, 
                DWORD dwShareMode,
                DWORD *pdwActionTaken,             
                SMBFile_OpLock_PassStruct *pdwOpLock)
{
     TRACEMSG(ZONE_IPC, (L"IPCStream::Open pFileName=%s, dwAccess=0x%x, dwDisp=0x%08x dwAttrib=0x%x dwShare=0x%x\n", 
        pFileName, dwAccess, dwDisposition, dwAttributes, dwShareMode));

    HRESULT hr = E_FAIL;   
    SMBPrintQueue *pPrintQueue = NULL;
    StringConverter FullPath;
    TIDState *pMyTIDState;
    
    //the client cannot create new pipes on the server
    //it can only connect to already existing pipes
    if (dwDisposition != OPEN_EXISTING) {
        TRACEMSG(ZONE_ERROR, (L"Unexpected dwDisposition=0x%x", dwDisposition));
        return HRESULT_FROM_WIN32(ERROR_PIPE_NOT_CONNECTED);
    }

    //
    // Make sure this stream hasnt already been opened
    if(_fOpened)  {
        ASSERT(FALSE);
        return HRESULT_FROM_WIN32(ERROR_PIPE_NOT_CONNECTED);
    }
    
    //
    // Get our TIDState so we can start opening the file
    if(NULL == (pMyTIDState = this->GetTIDState()))  {        
        ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_OpenX -- cant get TID STATE!!!!"));
        return HRESULT_FROM_WIN32(ERROR_PIPE_NOT_CONNECTED);
    }
    
    FullPath.append(L"\\\\.");
    FullPath.append(pFileName);
   
    if ( (wcslen(FullPath.GetString()) >= MAX_PATH) ||
         (0 != wcsnicmp(FullPath.GetString(), c_szPipePrefix, SVSUTIL_CONSTSTRLEN(c_szPipePrefix))  ) )
    {
        TRACEMSG(ZONE_ERROR, (L"Bad pipe name: %s", FullPath.GetString()));
        return HRESULT_FROM_WIN32(ERROR_PIPE_NOT_CONNECTED);
    }

    TRACEMSG(ZONE_IPC, (L"SMBSRV-FILE: OpenX for %s", FullPath.GetString()));

    wcscpy(_szPipeName, FullPath.GetString());

    DWORD dwOpenMode = 0;
    if (dwAccess & GENERIC_READ)
    {
        if (dwAccess & GENERIC_WRITE) {
            dwOpenMode = PIPE_ACCESS_DUPLEX;
        }
        else {
            dwOpenMode = PIPE_ACCESS_INBOUND;
        }
    }
    else if (dwAccess & GENERIC_WRITE) {
        dwOpenMode = PIPE_ACCESS_OUTBOUND;
    } 
    
    _hPipe = CreateNamedPipe(_szPipeName, dwOpenMode, PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_TYPE_CLIENT|PIPE_WAIT, 
        PIPE_UNLIMITED_INSTANCES, 0, 0, 0, NULL);

    if (_hPipe == INVALID_HANDLE_VALUE) {
        TRACEMSG(ZONE_ERROR, (L"Failed to create pipe. GLE=0x%08x", GetLastError()));
        return HRESULT_FROM_WIN32(ERROR_PIPE_NOT_CONNECTED);
    }

    _fOpened = TRUE;

    return S_OK;
}

HRESULT 
IPCStream::SetEndOfStream(DWORD dwOffset)
{
    TRACEMSG(ZONE_IPC, (L"IPCStream::SetEndOfStream pFileName=0x%x\n", dwOffset));
    return E_FAIL;
}

HRESULT 
IPCStream::GetFileSize(DWORD *pdwSize)
{
    TRACEMSG(ZONE_IPC, (L"IPCStream::GetFileSize pdwSize=0x%08x\n", pdwSize));
    *pdwSize = 0;
    return S_OK;
}

HRESULT 
IPCStream::SetFileTime(FILETIME *pCreation, 
                    FILETIME *pAccess, 
                    FILETIME *pWrite)
{
    TRACEMSG(ZONE_IPC, (L"IPCStream::SetFileTime Creation(0x%08x, 0x%08x), Access(0x%08x, 0x%08x) Write(0x%08x, 0x%08x)\n", 
        (pCreation) ? pCreation->dwHighDateTime : NULL, 
        (pCreation) ? pCreation->dwLowDateTime : NULL, 
        (pAccess) ? pAccess->dwHighDateTime : NULL, 
        (pAccess) ? pAccess->dwLowDateTime : NULL, 
        (pWrite) ? pWrite->dwHighDateTime : NULL, 
        (pWrite) ? pWrite->dwLowDateTime : NULL));

    return E_FAIL;
}

HRESULT 
IPCStream::GetFileTime(FILETIME *pCreation, 
                    FILETIME *pAccess, 
                    FILETIME *pWrite)
{
    TRACEMSG(ZONE_IPC, (L"IPCStream::GetFileTime \n"));
    SYSTEMTIME st;
    FILETIME ft;
    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &ft);
    if (pCreation) {
        *pCreation = ft;
    }
    if (pAccess) {
        *pAccess = ft;
    }
    if (pWrite) {
        *pWrite = ft;
    }
    return S_OK;
}

HRESULT 
IPCStream::Flush()
{
    TRACEMSG(ZONE_IPC, (L"IPCStream::Flush\n"));
    return E_FAIL;
}

HRESULT 
IPCStream::Close()
{
    TRACEMSG(ZONE_IPC, (L"IPCStream::Close\n"));
    if (!_fOpened) {
        TRACEMSG(ZONE_IPC, (L"IPCStream::Pipe not opened\n"));
        return S_OK;
    }
    ASSERT(_hPipe != INVALID_HANDLE_VALUE);

    CloseNamedPipe(_hPipe);
    _hPipe = INVALID_HANDLE_VALUE;

    _fOpened = FALSE;
    return S_OK;
}
