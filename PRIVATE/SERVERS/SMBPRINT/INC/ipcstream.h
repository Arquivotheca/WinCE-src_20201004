//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef _IPCSTREAM_H_
#define _IPCSTREAM_H_

#include <cracker.h>
#include <smbpackets.h>

class IPCStream : public SMBFileStream
{
public:
	IPCStream(TIDState *pMyState, USHORT usFID);
	~IPCStream();

public:
        HRESULT Read(BYTE *pDest, 
                     DWORD dwOffset, 
                     DWORD dwReqSize, 
                     DWORD *pdwRead);
        HRESULT Write(BYTE *pSrc, 
                     DWORD dwOffset, 
                     DWORD dwReqSize, 
                     DWORD *pdwWritten);
        HRESULT Open(const WCHAR *pFileName, 
                     DWORD dwAccess,
                     DWORD dwDisposition,
                     DWORD dwAttributes, 
                     DWORD dwShareMode,
					 DWORD *pdwActionTaken,
					 SMBFile_OpLock_PassStruct *pdwOpLock = NULL);
        HRESULT SetEndOfStream(DWORD dwOffset);
        HRESULT GetFileSize(DWORD *pdwSize);
        HRESULT SetFileTime(FILETIME *pCreation, 
                            FILETIME *pAccess, 
                            FILETIME *pWrite);
		HRESULT GetFileTime(LPFILETIME lpCreationTime,
                            LPFILETIME lpAccessTime,
                            LPFILETIME lpWriteTime);
        HRESULT Flush();
        HRESULT Close();

public:
	HANDLE GetPipeHandle()	{	return _hPipe;	}
	PWSTR  GetPipeName()	{	return _szPipeName;	}

protected:
	WCHAR	_szPipeName[MAX_PATH];
	BOOL	_fOpened;
	HANDLE	_hPipe;
};

DWORD SMB_TRANS_API_HandleNamedPipeFunction(
	WORD wFunc, 
	WORD wFileId, 
	SMB_PROCESS_CMD *_pRawRequest, 
	SMB_PROCESS_CMD *_pRawResponse, 
	UINT *puiUsed, 
	StringTokenizer &RequestTokenizer,
	SMB_PACKET *pSMB);

#endif	
