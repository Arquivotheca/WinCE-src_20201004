//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "SMB_Globals.h"
#include "FileServer.h"
#include "Utils.h"
#include "SMBErrors.h"
#include "ShareInfo.h"
#include "RAPI.h"
#include "Cracker.h"
#include "ConnectionManager.h"
#include "SMBCommands.h"
#include "SMBPackets.h"
#include "PC_NET_PROG.h"

using namespace SMB_FILE;

ClassPoolAllocator<10, FileObject> FileObjectAllocator;
extern VOID SendSMBResponse(SMB_PACKET *pSMB, UINT uiUsed, DWORD dwErr);


//
// From CIFS9F.DOC section 3.12
WORD
Win32AttributeToDos(DWORD attributes)
{
    WORD attr = 0;

    if (attributes & FILE_ATTRIBUTE_READONLY)
        attr |= 0x1;
    if (attributes & FILE_ATTRIBUTE_HIDDEN)
        attr |= 0x2;
    if (attributes & FILE_ATTRIBUTE_SYSTEM)
        attr |= 0x4;
    if (attributes & FILE_ATTRIBUTE_DIRECTORY)
        attr |= 0x10;
    if (attributes & FILE_ATTRIBUTE_ARCHIVE)
        attr |= 0x20;
    if (attributes & FILE_ATTRIBUTE_NORMAL)
        attr |= 0x80;
    if (attributes & FILE_ATTRIBUTE_TEMPORARY)
        attr |= 0x100;
    if (attributes & FILE_FLAG_WRITE_THROUGH)
        attr |= 0x80000000;
    return attr;
}

//
// From CIFS9F.DOC section 3.12
WORD
DosAttributeToWin32(DWORD attributes)
{
    WORD attr = 0;

    if (attributes & 0x1)
        attr |= FILE_ATTRIBUTE_READONLY;
    if (attributes & 0x2)
        attr |= FILE_ATTRIBUTE_HIDDEN;
    if (attributes & 0x4)
        attr |= FILE_ATTRIBUTE_SYSTEM;
    if (attributes & 0x10)
        attr |= FILE_ATTRIBUTE_DIRECTORY;
    if (attributes & 0x20)
        attr |= FILE_ATTRIBUTE_ARCHIVE;
    if (attributes & 0x80)
        attr |= FILE_ATTRIBUTE_NORMAL;
    if (attributes & 0x100)
        attr |= FILE_ATTRIBUTE_TEMPORARY;
    if (attributes & 0x80000000)
        attr |= FILE_FLAG_WRITE_THROUGH;
    return attr;
}



//
// Util to return TRUE if the passed thing is a directory, FALSE if not
HRESULT IsDirectory(const WCHAR *pFile, BOOL *fStatus, BOOL *fExists=NULL)
{
//#error use GetFileAttributes

    WIN32_FIND_DATA dta;
    BOOL fIsDir = FALSE;
    HRESULT hr = E_FAIL;
    StringConverter CheckFile;

    if(NULL == pFile) {
        ASSERT(FALSE);
        hr = E_INVALIDARG;
        goto Done;
    }

    //
    // If there is a trailing \ remove it
    //   NOTE: that we will switch out buffers to prevent mods to the calling buffer
    if('\\' == pFile[wcslen(pFile)-1]) {
        WCHAR *pTemp;
        if(FAILED(hr = CheckFile.append(pFile))) {
            goto Done;
        }
        pTemp = CheckFile.GetUnsafeString();
        pTemp[wcslen(pTemp)-1] = NULL;
        pFile = pTemp;
    }

    HANDLE h = FindFirstFile(pFile, &dta);

    if(INVALID_HANDLE_VALUE == h) {
        if(fExists) {
            *fExists = FALSE;
        }
        hr = S_OK;
        goto Done;
    }
    FindClose(h);

    //
    // Figure out what the file/dir is
    if(dta.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        fIsDir = TRUE;
    }
    //
    // Success
    hr = S_OK;

    if(fExists) {
        *fExists = TRUE;
    }

    Done:
        *fStatus = fIsDir;
        return hr;
}

//
// Util to get a file time w/o having a handle
HRESULT GetFileTimesFromFileName(const WCHAR *pFile, FILETIME *pCreation, FILETIME *pAccess, FILETIME *pWrite)
{
    WIN32_FIND_DATA dta;
    BOOL fIsDir = FALSE;
    HRESULT hr = E_FAIL;
    HANDLE h = FindFirstFile(pFile, &dta);

    if(INVALID_HANDLE_VALUE == h) {
        goto Done;
    }
    FindClose(h);

    if(NULL != pCreation) {
        *pCreation = dta.ftCreationTime;
    }
    if(NULL != pAccess) {
        *pAccess = dta.ftLastAccessTime;
    }
    if(NULL != pWrite) {
        *pWrite = dta.ftLastWriteTime;
    }
    //
    // Success
    hr = S_OK;

    Done:
        return hr;
}



DWORD SMB_Trans2_Query_FS_Information(SMB_COM_TRANSACTION2_CLIENT_REQUEST *pRequest,
                                   StringTokenizer *pTokenizer,
                                   RAPIBuilder *pRAPIBuilder,
                                   SMB_COM_TRANSACTION2_SERVER_RESPONSE *pResponse,
                                   USHORT usTID,
                                   SMB_PACKET *pSMB)
{
    WORD wLevel;
    DWORD dwRet;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    BYTE *pLabel =  NULL;

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
      TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
      ASSERT(FALSE);
      dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
      goto Done;
    }

    if(FAILED(pTokenizer->GetWORD(&wLevel))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- not enough memory in request -- sending back request for more"));
        goto Error;
    }

    switch(wLevel) {
        case SMB_INFO_ALLOCATION: //0x01
        {
            SMB_QUERY_INFO_ALLOCATION_SERVER_RESPONSE *pAllocResponse;

            ce::smart_ptr<TIDState> pTIDState = NULL;
            ULARGE_INTEGER FreeToCaller;
            ULARGE_INTEGER NumberBytes;
            ULARGE_INTEGER TotalFree;

            if(FAILED(pRAPIBuilder->ReserveParams(0, (BYTE**)&pAllocResponse)))
              goto Error;
            if(FAILED(pRAPIBuilder->ReserveBlock(sizeof(SMB_QUERY_INFO_ALLOCATION_SERVER_RESPONSE), (BYTE**)&pAllocResponse)))
              goto Error;

            //
            // Find a share state
            if(FAILED(pMyConnection->FindTIDState(usTID, pTIDState, SEC_READ)) || !pTIDState)
            {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION -- couldnt find share state!!"));
                goto Error;
            }

            //
            // Get the amount of free disk space
            if(0 == GetDiskFreeSpaceEx(pTIDState->GetShare()->GetDirectoryName(),
                                       &FreeToCaller,
                                       &NumberBytes,
                                       &TotalFree)) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION -- couldnt get free disk space!!"));
                goto Error;

            }

            //
            // Fill in parameters
            pAllocResponse->idFileSystem = 0; //we dont have a filesysem ID
            pAllocResponse->cSectorUnit = 32768;
            pAllocResponse->cUnit = (ULONG)(NumberBytes.QuadPart / 32768);
            pAllocResponse->cUnitAvail = (ULONG)(FreeToCaller.QuadPart / 32768);
            pAllocResponse->cbSector = 1;
            break;
        }
            break;
        case SMB_QUERY_FS_ATTRIBUTE_INFO: //0x105
        {
            StringConverter Label;
            UINT uiLabelLen;
            BYTE *pLabelInPacket;

            SMB_QUERY_FS_ATTRIBUTE_INFO_SERVER_RESPONSE *pAttrResponse;

            if(FAILED(Label.append(L"FAT"))) {
                goto Error;
            }
            if(NULL == (pLabel = Label.NewSTRING(&uiLabelLen, pMyConnection->SupportsUnicode(pSMB->pInSMB)))) {
                goto Error;
            }

            //
            // Reserve memory
            if(FAILED(pRAPIBuilder->ReserveParams(0, (BYTE**)&pAttrResponse)))  {
                goto Error;
            }
            if(FAILED(pRAPIBuilder->ReserveBlock(sizeof(SMB_QUERY_FS_ATTRIBUTE_INFO_SERVER_RESPONSE), (BYTE**)&pAttrResponse))) {
                goto Error;
            }
            if(FAILED(pRAPIBuilder->ReserveBlock(uiLabelLen, (BYTE**)&pLabelInPacket))) {
                goto Error;
            }

            pAttrResponse->FileSystemAttributes = 0x20 | 0x03;
            pAttrResponse->MaxFileNameComponent = 0xFF;
            pAttrResponse->NumCharsInLabel = uiLabelLen;
            memcpy(pLabelInPacket, pLabel, uiLabelLen);
            break;
        }
        case SMB_INFO_VOLUME: //0x02
        {
            SMB_INFO_VOLUME_SERVER_RESPONSE *pVolResponse;

            if(FAILED(pRAPIBuilder->ReserveParams(0, (BYTE**)&pVolResponse))) {
                goto Error;
            }
            if(FAILED(pRAPIBuilder->ReserveBlock(sizeof(SMB_INFO_VOLUME_SERVER_RESPONSE), (BYTE**)&pVolResponse))) {
                goto Error;
            }

            pVolResponse->ulVolumeSerialNumber = 0x00000000;
            pVolResponse->NumCharsInLabel= 0;
            break;
        }
        case SMB_QUERY_FS_VOLUME_INFO: //0x0102
        {
            SMB_QUERY_FS_VOLUME_INFO_SERVER_RESPONSE *pVolResponse = NULL;
            SMB_QUERY_FS_VOLUME_INFO_SERVER_RESPONSE VolResponse;
            BYTE *pLabelInPacket = NULL;
            UINT uiLabelLen = 0;
            StringConverter Label;

            if(FAILED(Label.append(L""))) {
                goto Error;
            }
            if(NULL == (pLabel = Label.NewSTRING(&uiLabelLen, pMyConnection->SupportsUnicode(pSMB->pInSMB)))) {
                goto Error;
            }
            if(FAILED(pRAPIBuilder->ReserveParams(0, (BYTE**)&pVolResponse))) {
                goto Error;
            }
            if(FAILED(pRAPIBuilder->ReserveBlock(sizeof(SMB_QUERY_FS_VOLUME_INFO_SERVER_RESPONSE), (BYTE**)&pVolResponse))) {
                goto Error;
            }
            if(FAILED(pRAPIBuilder->ReserveBlock(uiLabelLen, (BYTE**)&pLabelInPacket))) {
                goto Error;
            }
            VolResponse.VolumeCreationTime.QuadPart = 0;
            VolResponse.VolumeSerialNumber = 0x00000000;
            VolResponse.LengthOfLabel = Label.Length();
            VolResponse.Reserved1 = 0;
            VolResponse.Reserved2 = 0;
            memcpy(pVolResponse, &VolResponse, sizeof(SMB_QUERY_FS_VOLUME_INFO_SERVER_RESPONSE));
            memcpy(pLabelInPacket, pLabel, uiLabelLen);
            break;
        }
        case SMB_QUERY_DISK_ALLOCATION_NT: //0x0103
        {
            SMB_DISK_ALLOCATION_NT *pAllocResponse;
            SMB_DISK_ALLOCATION_NT AllocResponse;

            ce::smart_ptr<TIDState> pTIDState = NULL;
            ULARGE_INTEGER FreeToCaller;
            ULARGE_INTEGER NumberBytes;
            ULARGE_INTEGER TotalFree;

            if(FAILED(pRAPIBuilder->ReserveParams(0, (BYTE**)&pAllocResponse)))
              goto Error;
            if(FAILED(pRAPIBuilder->ReserveBlock(sizeof(SMB_DISK_ALLOCATION_NT), (BYTE**)&pAllocResponse)))
              goto Error;

            //
            // Find a share state
            if(FAILED(pMyConnection->FindTIDState(usTID, pTIDState, SEC_READ)) || !pTIDState)
            {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION -- couldnt find share state!!"));
                goto Error;
            }

            //
            // Get the amount of free disk space
            if(0 == GetDiskFreeSpaceEx(pTIDState->GetShare()->GetDirectoryName(),
                                       &FreeToCaller,
                                       &NumberBytes,
                                       &TotalFree)) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION -- couldnt get free disk space!!"));
                goto Error;

            }

            //
            // Fill in parameters
            AllocResponse.TotalAllocationUnits.QuadPart = NumberBytes.QuadPart / (512 *  64);
            AllocResponse.AvailableAllocationUnits.QuadPart = FreeToCaller.QuadPart / (512 * 64);
            AllocResponse.BlocksPerUnit = 64;
            AllocResponse.BytesPerBlock = 512;
            memcpy(pAllocResponse, &AllocResponse, sizeof(SMB_DISK_ALLOCATION_NT));
            break;
        }
        default:
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- unknown Trans2 query for SMB_Trans2_Query_FS_Information %d", wLevel));
            ASSERT(FALSE);
            goto Error;
    }
    dwRet = 0;
    goto Done;
    Error:
        dwRet = ERROR_CODE(STATUS_NOT_SUPPORTED);
    Done:
        if(NULL != pLabel) {
            LocalFree(pLabel);
        }
        return dwRet;
}


DWORD SMB_Trans2_Set_File_Information(USHORT usTid,
                                    SMB_COM_TRANSACTION2_CLIENT_REQUEST *pRequest,
                                    StringTokenizer *pTokenizer,
                                    SMB_PROCESS_CMD *_pRawRequest,
                                    SMB_PROCESS_CMD *_pRawResponse,
                                    SMB_COM_TRANSACTION2_SERVER_RESPONSE *pResponse,
                                    SMB_PACKET *pSMB,
                                    UINT *puiUsed)
{
    WORD wLevel;
    WORD wReserved;
    USHORT usFID;
    DWORD dwRet;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    UINT uiFileNameLen = 0;
    BYTE *pFileName = NULL;
    ce::smart_ptr<TIDState> pTIDState = NULL;
    StringTokenizer DataTokenizer;
    BYTE *pTransData = NULL;
    HRESULT hr;

    RAPIBuilder RAPI((BYTE *)(pResponse+1),
                     _pRawResponse->uiDataSize-sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE),
                     pRequest->MaxParameterCount,
                     pRequest->MaxDataCount);

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
      TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Set_File_Information: -- cant find connection 0x%x!", pSMB->ulConnectionID));
      ASSERT(FALSE);
      dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
      goto Done;
    }

    //
    // Fetch the FID and info level
    if(FAILED(pTokenizer->GetWORD(&usFID))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Set_File_Information -- not enough memory in request -- sending back request for more"));
        goto Error;
    }
    if(FAILED(pTokenizer->GetWORD(&wLevel))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Set_File_Information -- not enough memory in request -- sending back request for more"));
        goto Error;
    }

    if(FAILED(pTokenizer->GetWORD(&wReserved))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Set_File_Information -- not enough memory in request -- sending back request for more"));
        goto Error;
    }

    pTransData = (BYTE *)_pRawRequest->pSMBHeader + pRequest->DataOffset;
    DataTokenizer.Reset(pTransData, pRequest->DataCount);

    //
    // Find a share state
    if(FAILED(hr = pMyConnection->FindTIDState(usTid, pTIDState, SEC_WRITE)) || !pTIDState) {
        if(E_ACCESSDENIED == hr) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Set_File_Information -- access denied!!"));
            dwRet = ERROR_CODE(STATUS_NETWORK_ACCESS_DENIED);
            goto Done;
        } else {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Set_File_Information -- couldnt find share state!!"));
            goto Error;
        }
    }

    switch(wLevel) {
        case SMB_SET_FILE_BASIC_INFO:    //0x0101
        {
            BYTE *pFileTemp = NULL;
            SMB_FILE_BASIC_INFO FileInfo;
            ce::smart_ptr<SMBFileStream> pStream = NULL;
            FILETIME Creation;
            FILETIME Access;
            FILETIME Write;
            FILETIME *pCreation = NULL;
            FILETIME *pAccess = NULL;
            FILETIME *pWrite = NULL;

            USHORT usAttributes;
            if(FAILED(DataTokenizer.GetByteArray((BYTE **)&pFileTemp, sizeof(SMB_FILE_BASIC_INFO)))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Set_File_Information -- get EOF Info!!"));
                goto Error;
            }

            memcpy(&FileInfo, pFileTemp, sizeof(SMB_FILE_BASIC_INFO));


            if(!(usAttributes = DosAttributeToWin32(FileInfo.Attributes))) {
                usAttributes = FILE_ATTRIBUTE_NORMAL;
            }

            if(0!=FileInfo.CreationTime.QuadPart) {
                pCreation = &Creation;
                Creation.dwLowDateTime = FileInfo.CreationTime.LowPart;
                Creation.dwHighDateTime = FileInfo.CreationTime.HighPart;
            }
            if(0!=FileInfo.LastAccessTime.QuadPart) {
                pAccess = &Access;
                Access.dwLowDateTime = FileInfo.LastAccessTime.LowPart;
                Access.dwHighDateTime = FileInfo.LastAccessTime.HighPart;
            }
            if(0!=FileInfo.LastWriteTime.QuadPart) {
                pWrite = &Write;
                Write.dwLowDateTime = FileInfo.LastWriteTime.LowPart;
                Write.dwHighDateTime = FileInfo.LastWriteTime.HighPart;
            }

            //
            // Set the actual file time
            if(FAILED(hr = pTIDState->SetFileTime(usFID, pCreation, pAccess, pWrite))) {
               TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Set_File_Information -- couldnt set filetime!!"));

               dwRet = ConvertHRToError(hr, pSMB);
               goto Done;
            }

            if(FAILED(pTIDState->FindFileStream(usFID, pStream))) {
               TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Set_File_Information -- couldnt get file stream!!"));
               goto Error;
            }

            ASSERT(usAttributes);
            if(0 == SetFileAttributes(pStream->GetFileName(), usAttributes)) {
               TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Set_File_Information -- set file attributes %d -- GLE:%d!!",usAttributes, GetLastError()));
               goto Error;
            }

            break;
        }
        case SMB_SET_FILE_DISPOSITION_INFO: //0x0102
        {
            SMB_FILE_DISPOSITION_INFO *pFileInfo;
            if(FAILED(DataTokenizer.GetByteArray((BYTE **)&pFileInfo, sizeof(SMB_FILE_DISPOSITION_INFO)))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Set_File_Information -- get EOF Info!!"));
                goto Error;
            }
            if(TRUE == pFileInfo->FileIsDeleted && FAILED(pTIDState->Delete(usFID))) {
               TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Set_File_Information -- couldnt set EOF!!"));
               goto Error;
            }
            break;
        }
        case SMB_SET_FILE_ALLOCATION_INFO:       //0x0103
        {
            SMB_FILE_ALLOCATION_INFO *pFileInfo;
            if(FAILED(DataTokenizer.GetByteArray((BYTE **)&pFileInfo, sizeof(SMB_FILE_END_OF_FILE_INFO)))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Set_File_Information -- get EOF Info!!"));
                goto Error;
            }
            ASSERT(0 == pFileInfo->EndOfFile.HighPart);
            if(FAILED(pTIDState->SetEndOfStream(usFID, pFileInfo->EndOfFile.LowPart))) {
               TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Set_File_Information -- couldnt set EOF!!"));
                goto Error;
            }
            break;
        }
        case SMB_SET_FILE_END_OF_FILE_INFO:   //0x0104
        {
            SMB_FILE_END_OF_FILE_INFO *pFileInfo;
            if(FAILED(DataTokenizer.GetByteArray((BYTE **)&pFileInfo, sizeof(SMB_FILE_END_OF_FILE_INFO)))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Set_File_Information -- get EOF Info!!"));
                goto Error;
            }
            ASSERT(0 == pFileInfo->EndOfFile.HighPart);
            if(FAILED(hr = pTIDState->SetEndOfStream(usFID, pFileInfo->EndOfFile.LowPart))) {
               TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Set_File_Information -- couldnt set EOF!!"));
               dwRet = ConvertHRToError(hr, pSMB);
               goto Done;
            }
            break;
        }

        default:
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- unknown Trans2 query for SMB_Trans2_Set_File_Information %d", wLevel));

            //
            // Mark that we are using NT status codes
            dwRet = ERROR_CODE(STATUS_INVALID_LEVEL);
            goto Done;
    }
    dwRet = 0;
    goto Done;
    Error:
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
    Done:
        {
            BYTE *pTempBuf = NULL;

            if(SUCCEEDED(RAPI.ReserveParams(2, (BYTE**)&pTempBuf))) {
                //
                // We wont be alligned if we dont make a gap
                while(0 != ((UINT)RAPI.DataPtr() % sizeof(DWORD)))
                    RAPI.MakeParamByteGap(1);

                *pTempBuf = 0;
                *(pTempBuf+1) = 0;
                *(pTempBuf+2) = 0;
            }

            //fill out response SMB
            memset(pResponse, 0, sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE));

            //word count is 3 bytes (1=WordCount 2=ByteCount) less than the response
            pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE) - 3) / sizeof(WORD);
            pResponse->TotalParameterCount = RAPI.ParamBytesUsed();
            pResponse->TotalDataCount = RAPI.DataBytesUsed();
            pResponse->ParameterCount = RAPI.ParamBytesUsed();
            pResponse->ParameterOffset = RAPI.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
            pResponse->ParameterDisplacement = 0;
            pResponse->DataCount = RAPI.DataBytesUsed();
            pResponse->DataOffset = RAPI.DataOffset((BYTE *)_pRawResponse->pSMBHeader);
            pResponse->DataDisplacement = 0;
            pResponse->SetupCount = 0;
            pResponse->ByteCount = RAPI.TotalBytesUsed();

            ASSERT(10 == pResponse->WordCount);


            //set the number of bytes we used
            *puiUsed = RAPI.TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE);
        }

        //
        // return any error
        return dwRet;
}




DWORD SMB_Trans2_Query_File_Information(USHORT usTid,
                                    SMB_COM_TRANSACTION2_CLIENT_REQUEST *pRequest,
                                    StringTokenizer *pTokenizer,
                                    SMB_PROCESS_CMD *_pRawResponse,
                                    SMB_COM_TRANSACTION2_SERVER_RESPONSE *pResponse,
                                    SMB_PACKET *pSMB,
                                    UINT *puiUsed)
{
    WORD wLevel;
    USHORT usFID;
    DWORD dwRet;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    UINT uiFileNameLen = 0;
    BYTE *pFileName = NULL;
    HRESULT hr;
    RAPIBuilder RAPI((BYTE *)(pResponse+1),
                     _pRawResponse->uiDataSize-sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE),
                     pRequest->MaxParameterCount,
                     pRequest->MaxDataCount);

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
      TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Query_File_Information: -- cant find connection 0x%x!", pSMB->ulConnectionID));
      ASSERT(FALSE);
      dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
      goto Done;
    }

    //
    // Fetch the FID and info level
    if(FAILED(pTokenizer->GetWORD(&usFID))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Query_File_Information -- not enough memory in request -- sending back request for more"));
        goto Error;
    }
    if(FAILED(pTokenizer->GetWORD(&wLevel))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Query_File_Information -- not enough memory in request -- sending back request for more"));
        goto Error;
    }

    switch(wLevel) {
        case SMB_QUERY_FILE_EA_INFO_ID: //0x0103
        {
            SMB_QUERY_FILE_EA_INFO QueryResponse;
            SMB_QUERY_FILE_EA_INFO *pQueryResponse = NULL;
            QueryResponse.EASize = 0;

            if(FAILED(RAPI.ReserveParams(0, (BYTE**)&pQueryResponse)))
              goto Error;
            if(FAILED(RAPI.ReserveBlock(sizeof(SMB_QUERY_FILE_EA_INFO), (BYTE**)&pQueryResponse)))
              goto Error;

            memcpy(pQueryResponse, &QueryResponse, sizeof(SMB_QUERY_FILE_EA_INFO));
            break;
        }
        case SMB_QUERY_FILE_STANDARD_INFO_ID: //0x0102
        {
            SMB_QUERY_FILE_STANDARD_INFO QueryResponse;
            SMB_QUERY_FILE_STANDARD_INFO *pQueryResponse = NULL;

            ce::smart_ptr<TIDState> pTIDState = NULL;
            DWORD dwFileOffset = 0;
            WIN32_FIND_DATA w32FindData;
            WORD attributes;

            if(FAILED(RAPI.ReserveParams(0, (BYTE**)&pQueryResponse)))
              goto Error;
            if(FAILED(RAPI.ReserveBlock(sizeof(SMB_QUERY_FILE_STANDARD_INFO), (BYTE**)&pQueryResponse)))
              goto Error;

            //
            // Find a share state
            if(FAILED(pMyConnection->FindTIDState(usTid, pTIDState, SEC_READ)) || !pTIDState) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Query_File_Information -- couldnt find share state!!"));
                goto Error;
            }
            if(FAILED(pTIDState->QueryFileInformation(usFID, &w32FindData))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Query_File_Information -- couldnt find FID!"));
                goto Error;
            }
            if(FAILED(hr = pTIDState->GetFileSize(usFID, &dwFileOffset))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Query_File_Information -- couldnt find share state!!"));
                dwRet = ConvertHRToError(hr, pSMB);
                goto ErrorSet;
            }
            attributes = Win32AttributeToDos(w32FindData.dwFileAttributes);
            QueryResponse.AllocationSize.HighPart = 0;
            QueryResponse.AllocationSize.LowPart = dwFileOffset;
            QueryResponse.EndofFile.HighPart = 0;
            QueryResponse.EndofFile.LowPart = dwFileOffset;
            QueryResponse.NumberOfLinks = 0;
            QueryResponse.DeletePending = FALSE;
            QueryResponse.Directory = (attributes & FILE_ATTRIBUTE_DIRECTORY)?TRUE:FALSE;

            memcpy(pQueryResponse, &QueryResponse, sizeof(SMB_QUERY_FILE_STANDARD_INFO));
            break;
        }

        case SMB_QUERY_FILE_ALL_INFO_ID: //0x0107
        {
            SMB_QUERY_FILE_ALL_INFO QueryResponse;
            SMB_QUERY_FILE_ALL_INFO *pQueryResponse = NULL;
            WIN32_FIND_DATA w32FindData;
            WORD attributes;
            ce::smart_ptr<TIDState> pTIDState = NULL;
            ULONG ulFileOffset = 0;
            ce::smart_ptr<FileObject> pMyFile = NULL;

            if(FAILED(RAPI.ReserveParams(0, (BYTE**)&pQueryResponse)))
              goto Error;
            if(FAILED(RAPI.ReserveBlock(sizeof(SMB_QUERY_FILE_ALL_INFO), (BYTE**)&pQueryResponse)))
              goto Error;

            //
            // Find a share state
            if(FAILED(pMyConnection->FindTIDState(usTid, pTIDState, SEC_READ)) || !pTIDState) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Query_File_Information -- couldnt find share state!!"));
                goto Error;
            }

            if(FAILED(hr = pTIDState->QueryFileInformation(usFID, &w32FindData))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Query_File_Information -- couldnt find FID!"));
                dwRet = ConvertHRToError(hr, pSMB);
                goto ErrorSet;
            }

            if(FAILED(pTIDState->SetFilePointer(usFID, 0, 1, &ulFileOffset))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Query_File_Information -- couldnt get file offset for FID: %d!", usFID));
                ulFileOffset = 0;
            }

            if(SUCCEEDED(SMB_Globals::g_pAbstractFileSystem->FindFile(usFID, pMyFile)) && !pMyFile) {
                StringConverter myString;
                myString.append(pMyFile->FileName());
                pFileName = myString.NewSTRING(&uiFileNameLen, pMyConnection->SupportsUnicode(pSMB->pInSMB));
            }
            if(NULL == pFileName) {
                StringConverter myString;
                myString.append(L"");
                pFileName = myString.NewSTRING(&uiFileNameLen, pMyConnection->SupportsUnicode(pSMB->pInSMB));
            }

            attributes = Win32AttributeToDos(w32FindData.dwFileAttributes);
            QueryResponse.CreationTime.LowPart = w32FindData.ftCreationTime.dwLowDateTime;
            QueryResponse.CreationTime.HighPart = w32FindData.ftCreationTime.dwHighDateTime;
            QueryResponse.LastAccessTime.LowPart = w32FindData.ftLastAccessTime.dwLowDateTime;
            QueryResponse.LastAccessTime.HighPart = w32FindData.ftLastAccessTime.dwHighDateTime;
            QueryResponse.LastWriteTime.LowPart = w32FindData.ftLastWriteTime.dwLowDateTime;
            QueryResponse.LastWriteTime.HighPart = w32FindData.ftLastWriteTime.dwHighDateTime;
            QueryResponse.ChangeTime.LowPart = w32FindData.ftLastWriteTime.dwLowDateTime;
            QueryResponse.ChangeTime.HighPart = w32FindData.ftLastWriteTime.dwHighDateTime;
            QueryResponse.Attributes = attributes;
            QueryResponse.AllocationSize.HighPart = w32FindData.nFileSizeHigh;
            QueryResponse.AllocationSize.LowPart = w32FindData.nFileSizeLow;

            

            QueryResponse.EndOfFile.QuadPart = 0;
            //LARGE_INTEGER EndOfFile;

            QueryResponse.NumberOfLinks = 0;
            //ULONG NumberOfLinks;


            QueryResponse.DeletePending = FALSE;
            QueryResponse.Directory = (attributes & FILE_ATTRIBUTE_DIRECTORY)?TRUE:FALSE;
            QueryResponse.Index_Num.QuadPart = 0;
            QueryResponse.EASize = 0; //we dont support extended attributes
            QueryResponse.AccessFlags = QFI_FILE_READ_DATA |
                                          QFI_FILE_WRITE_DATA |
                                          QFI_FILE_APPEND_DATA |
                                          QFI_FILE_READ_ATTRIBUTES |
                                          QFI_FILE_WRITE_ATTRIBUTES |
                                          QFI_DELETE;

            QueryResponse.IndexNumber.QuadPart = 0;
            QueryResponse.CurrentByteOffset.QuadPart = ulFileOffset;
            QueryResponse.Mode = QFI_FILE_WRITE_THROUGH;
            QueryResponse.AlignmentRequirement = 0;
            QueryResponse.FileNameLength = uiFileNameLen;

            memcpy(pQueryResponse, &QueryResponse, sizeof(SMB_QUERY_FILE_ALL_INFO));

            BYTE *pFileInBlob = NULL;
            if(FAILED(RAPI.ReserveBlock(uiFileNameLen, (BYTE**)&pFileInBlob)))
              goto Error;

            ASSERT(pFileInBlob == (BYTE*)(pQueryResponse+1));
            memcpy(pFileInBlob, pFileName, uiFileNameLen);
            break;
        }
        case SMB_QUERY_FILE_BASIC_INFO_ID: //0x0101
        {
            SMB_FILE_BASIC_INFO *pQueryResponse = NULL;
            SMB_FILE_BASIC_INFO QueryResponse;
            WIN32_FIND_DATA w32FindData;
            WORD attributes;
            ce::smart_ptr<TIDState> pTIDState = NULL;

            if(FAILED(RAPI.ReserveParams(0, (BYTE**)&pQueryResponse)))
              goto Error;
            if(FAILED(RAPI.ReserveBlock(sizeof(SMB_FILE_BASIC_INFO), (BYTE**)&pQueryResponse)))
              goto Error;

            //
            // Find a share state and then get the file info
            if(FAILED(pMyConnection->FindTIDState(usTid, pTIDState, SEC_READ)) || !pTIDState) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Query_File_Information -- couldnt find share state!!"));
                goto Error;
            }
            if(FAILED(hr = pTIDState->QueryFileInformation(usFID, &w32FindData))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Query_File_Information -- couldnt find FID!"));
                dwRet = ConvertHRToError(hr, pSMB);
                goto ErrorSet;
            }

            attributes = Win32AttributeToDos(w32FindData.dwFileAttributes);
            QueryResponse.CreationTime.LowPart = w32FindData.ftCreationTime.dwLowDateTime;
            QueryResponse.CreationTime.HighPart = w32FindData.ftCreationTime.dwHighDateTime;
            QueryResponse.LastAccessTime.LowPart = w32FindData.ftLastAccessTime.dwLowDateTime;
            QueryResponse.LastAccessTime.HighPart = w32FindData.ftLastAccessTime.dwHighDateTime;
            QueryResponse.LastWriteTime.LowPart = w32FindData.ftLastWriteTime.dwLowDateTime;
            QueryResponse.LastWriteTime.HighPart = w32FindData.ftLastWriteTime.dwHighDateTime;
            QueryResponse.ChangeTime.LowPart = w32FindData.ftLastWriteTime.dwLowDateTime;
            QueryResponse.ChangeTime.HighPart = w32FindData.ftLastWriteTime.dwHighDateTime;
            QueryResponse.Attributes = attributes;
            memcpy(pQueryResponse, &QueryResponse, sizeof(SMB_FILE_BASIC_INFO));
            dwRet = 0;
            break;
        }
        case SMB_QUERY_FILE_STREAM_INFO_ID: //0x0109
        {
            SMB_QUERY_FILE_STREAM_INFO QueryResponse;
            SMB_QUERY_FILE_STREAM_INFO *pQueryResponse = NULL;

            if(FAILED(RAPI.ReserveParams(0, (BYTE**)&pQueryResponse)))
              goto Error;
            if(FAILED(RAPI.ReserveBlock(sizeof(SMB_QUERY_FILE_STREAM_INFO), (BYTE**)&pQueryResponse)))
              goto Error;

            memset(&QueryResponse, 0, sizeof(SMB_QUERY_FILE_STREAM_INFO));
            memcpy(pQueryResponse, &QueryResponse, sizeof(SMB_QUERY_FILE_STREAM_INFO));
            break;
        }
        default:
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- unknown Trans2 query for SMB_Trans2_Query_File_Information %d", wLevel));

            //
            // Mark that we are using NT status codes
            dwRet = ERROR_CODE(STATUS_INVALID_LEVEL);
            goto ErrorSet;
    }
    dwRet = 0;
    goto Done;
    Error:
        dwRet = ERROR_CODE(STATUS_INVALID_LEVEL);
    ErrorSet:
        {
            BYTE *pTempBuf = (BYTE *)(pResponse+1);
            *pTempBuf = 0;
            *(pTempBuf+1) = 0;
            *(pTempBuf+2) = 0;
            *puiUsed = 3;
            ASSERT(0 != dwRet);
        }
    Done:

        //
        // On error just return that error
        if(0 != dwRet)
            return dwRet;

        //fill out response SMB
        memset(pResponse, 0, sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE));

        //word count is 3 bytes (1=WordCount 2=ByteCount) less than the response
        pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE) - 3) / sizeof(WORD);
        pResponse->TotalParameterCount = RAPI.ParamBytesUsed();
        pResponse->TotalDataCount = RAPI.DataBytesUsed();
        pResponse->ParameterCount = RAPI.ParamBytesUsed();
        pResponse->ParameterOffset = RAPI.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->ParameterDisplacement = 0;
        pResponse->DataCount = RAPI.DataBytesUsed();
        pResponse->DataOffset = RAPI.DataOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->DataDisplacement = 0;
        pResponse->SetupCount = 0;
        pResponse->ByteCount = RAPI.TotalBytesUsed();

        ASSERT(10 == pResponse->WordCount);

        //set the number of bytes we used
        *puiUsed = RAPI.TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE);

        if(NULL != pFileName) {
            LocalFree(pFileName);
        }
        return dwRet;
}


DWORD SMB_Trans2_Query_Path_Information(USHORT usTid,
                                    SMB_COM_TRANSACTION2_CLIENT_REQUEST *pRequest,
                                    StringTokenizer *pTokenizer,
                                    SMB_PROCESS_CMD *_pRawResponse,
                                    SMB_COM_TRANSACTION2_SERVER_RESPONSE *pResponse,
                                    SMB_PACKET *pSMB,
                                    UINT *puiUsed)
{
    WORD wLevel;
    ULONG ulReserved;

    DWORD dwRet;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    RAPIBuilder RAPI((BYTE *)(pResponse+1),
                     _pRawResponse->uiDataSize-sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE),
                     pRequest->MaxParameterCount,
                     pRequest->MaxDataCount);
    StringConverter FileName;
    StringConverter FullPath;
    HANDLE hFileHand = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATA w32FindData;
    ce::smart_ptr<TIDState> pTIDState = NULL;

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
      TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Query_Path_Information: -- cant find connection 0x%x!", pSMB->ulConnectionID));
      ASSERT(FALSE);
      dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
      goto Done;
    }

    //
    // Fetch the info level
    if(FAILED(pTokenizer->GetWORD(&wLevel))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Query_Path_Information -- not enough memory in request -- sending back request for more"));
        goto Error;

    }

    //
    // Fetch a reserved param
    if(FAILED(pTokenizer->GetDWORD(&ulReserved))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Query_Path_Information -- not enough memory in request -- sending back request for more"));
        goto Error;
    }

    //
    // Fetch the filename (STRING)
    if(TRUE == pMyConnection->SupportsUnicode(pSMB->pInSMB)) {
        WCHAR *pFileName = NULL;

        if(FAILED(pTokenizer->GetUnicodeString(&pFileName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Trans2_Query_Path_Information -- couldnt find filename!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        FileName.append(pFileName);
    } else {
        CHAR *pFileName = NULL;

        if(FAILED(pTokenizer->GetString(&pFileName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Trans2_Query_Path_Information -- couldnt find filename!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Check that we have the right string type
        if(0x04 != *pFileName) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER:  SMB_Trans2_Query_Path_Information -- invalid string type!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Advance beyond the 0x04 header
        pFileName ++;

        FileName.append(pFileName);
    }



    //
    // Find a share state and then get the file info
    if(FAILED(pMyConnection->FindTIDState(usTid, pTIDState, SEC_READ)) || !pTIDState) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Trans2_Query_Path_Information -- couldnt find share state!!"));
        goto Error;
    }
    if(FAILED(FullPath.append(pTIDState->GetShare()->GetDirectoryName()))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Trans2_Query_Path_Information -- putting root on filename FAILED!!"));
        goto Error;
    }
    if(FAILED(FullPath.append(FileName.GetString()))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Trans2_Query_Path_Information -- putting file on filename FAILED!!"));
        goto Error;
    }
    if(FAILED(pTIDState->GetShare()->IsValidPath(FullPath.GetString()))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Trans2_Query_Path_Information -- someone may be hacking us!! failing request"));
        ASSERT(FALSE);
        goto Error;
    }

    //
    // Find the file & fill in its values
    hFileHand = FindFirstFile(FullPath.GetString(), &w32FindData);

    if(INVALID_HANDLE_VALUE == hFileHand) {
        TRACEMSG(ZONE_FILES, (L"SMBSRV:  SMB_Trans2_Query_Path_Information -- could not find file %s (%d)!!", FullPath.GetString(), GetLastError()));

        DWORD dwGLE = GetLastError();

        //
        // because we are using FFF it will give us NO_MORE_FILEs if the file doesnt
        //    exist,  map this to file not found
        if(ERROR_NO_MORE_FILES == dwGLE) {
            dwRet = ERROR_CODE(STATUS_OBJECT_NAME_NOT_FOUND);
        } else {
            dwRet = ConvertGLEToError(dwGLE, pSMB);
        }
        goto ErrorSet;
    }
    FindClose(hFileHand);


    switch(wLevel) {
        case SMB_QUERY_FILE_EA_INFO_ID:       //0x0103
        {
            SMB_QUERY_FILE_EA_INFO *pQueryResponse = NULL;
            SMB_QUERY_FILE_EA_INFO QueryResponse;

            if(FAILED(RAPI.ReserveParams(0, (BYTE**)&pQueryResponse)))
              goto Error;
            if(FAILED(RAPI.ReserveBlock(sizeof(SMB_QUERY_FILE_EA_INFO), (BYTE**)&pQueryResponse)))
              goto Error;


            QueryResponse.EASize = 0;
            memcpy(pQueryResponse, &QueryResponse, sizeof(SMB_QUERY_FILE_EA_INFO));
            dwRet = 0;
            break;
        }
        case SMB_QUERY_FILE_STANDARD_INFO_ID: //0x0102
        {
            SMB_QUERY_FILE_STANDARD_INFO *pQueryResponse = NULL;
            SMB_QUERY_FILE_STANDARD_INFO QueryResponse;

            if(FAILED(RAPI.ReserveParams(0, (BYTE**)&pQueryResponse)))
              goto Error;
            if(FAILED(RAPI.ReserveBlock(sizeof(SMB_QUERY_FILE_STANDARD_INFO), (BYTE**)&pQueryResponse)))
              goto Error;

            QueryResponse.AllocationSize.LowPart= w32FindData.nFileSizeLow;
            QueryResponse.AllocationSize.HighPart= w32FindData.nFileSizeHigh;
            QueryResponse.EndofFile.LowPart= w32FindData.nFileSizeLow;
            QueryResponse.EndofFile.HighPart= w32FindData.nFileSizeHigh;
            QueryResponse.NumberOfLinks = 0;
            QueryResponse.DeletePending = FALSE;
            QueryResponse.Directory = (w32FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)?TRUE:FALSE;

            memcpy(pQueryResponse, &QueryResponse, sizeof(SMB_QUERY_FILE_STANDARD_INFO));
            dwRet = 0;
            break;
        }
        case SMB_QUERY_FILE_BASIC_INFO_ID: //0x0101
        {
            SMB_FILE_BASIC_INFO *pQueryResponse = NULL;
            SMB_FILE_BASIC_INFO QueryResponse;
            WORD attributes;

            if(FAILED(RAPI.ReserveParams(0, (BYTE**)&pQueryResponse)))
              goto Error;
            if(FAILED(RAPI.ReserveBlock(sizeof(SMB_FILE_BASIC_INFO), (BYTE**)&pQueryResponse)))
              goto Error;

            attributes = Win32AttributeToDos(w32FindData.dwFileAttributes);
            QueryResponse.CreationTime.LowPart = w32FindData.ftCreationTime.dwLowDateTime;
            QueryResponse.CreationTime.HighPart = w32FindData.ftCreationTime.dwHighDateTime;
            QueryResponse.LastAccessTime.LowPart = w32FindData.ftLastAccessTime.dwLowDateTime;
            QueryResponse.LastAccessTime.HighPart = w32FindData.ftLastAccessTime.dwHighDateTime;
            QueryResponse.LastWriteTime.LowPart = w32FindData.ftLastWriteTime.dwLowDateTime;
            QueryResponse.LastWriteTime.HighPart = w32FindData.ftLastWriteTime.dwHighDateTime;
            QueryResponse.ChangeTime.LowPart = w32FindData.ftLastWriteTime.dwLowDateTime;
            QueryResponse.ChangeTime.HighPart = w32FindData.ftLastWriteTime.dwHighDateTime;
            QueryResponse.Attributes = attributes;
            memcpy(pQueryResponse, &QueryResponse, sizeof(SMB_FILE_BASIC_INFO));
            dwRet = 0;
            break;
        }
        case SMB_QUERY_FILE_ALL_INFO_ID: //0x0107
        {
            SMB_QUERY_FILE_ALL_INFO QueryResponse;
            SMB_QUERY_FILE_ALL_INFO *pQueryResponse = NULL;
            WORD attributes;
            ce::smart_ptr<TIDState> pTIDState = NULL;
            ULONG ulFileOffset = 0;
            UINT  uiFileNameLen = 0;
            ce::smart_ptr<FileObject> pMyFile = NULL;
            BYTE *pFileName = NULL;

            if(FAILED(RAPI.ReserveParams(0, (BYTE**)&pQueryResponse)))
              goto Error;
            if(FAILED(RAPI.ReserveBlock(sizeof(SMB_QUERY_FILE_ALL_INFO), (BYTE**)&pQueryResponse)))
              goto Error;

            StringConverter myString;
            myString.append(w32FindData.cFileName);
            pFileName = myString.NewSTRING(&uiFileNameLen, pMyConnection->SupportsUnicode(pSMB->pInSMB));

            if(NULL == pFileName) {
                StringConverter myString;
                myString.append(L"");
                pFileName = myString.NewSTRING(&uiFileNameLen, pMyConnection->SupportsUnicode(pSMB->pInSMB));
            }

            attributes = Win32AttributeToDos(w32FindData.dwFileAttributes);
            QueryResponse.CreationTime.LowPart = w32FindData.ftCreationTime.dwLowDateTime;
            QueryResponse.CreationTime.HighPart = w32FindData.ftCreationTime.dwHighDateTime;
            QueryResponse.LastAccessTime.LowPart = w32FindData.ftLastAccessTime.dwLowDateTime;
            QueryResponse.LastAccessTime.HighPart = w32FindData.ftLastAccessTime.dwHighDateTime;
            QueryResponse.LastWriteTime.LowPart = w32FindData.ftLastWriteTime.dwLowDateTime;
            QueryResponse.LastWriteTime.HighPart = w32FindData.ftLastWriteTime.dwHighDateTime;
            QueryResponse.ChangeTime.LowPart = w32FindData.ftLastWriteTime.dwLowDateTime;
            QueryResponse.ChangeTime.HighPart = w32FindData.ftLastWriteTime.dwHighDateTime;
            QueryResponse.Attributes = attributes;
            QueryResponse.AllocationSize.HighPart = w32FindData.nFileSizeHigh;
            QueryResponse.AllocationSize.LowPart = w32FindData.nFileSizeLow;

            

            QueryResponse.EndOfFile.QuadPart = 0;
            //LARGE_INTEGER EndOfFile;

            QueryResponse.NumberOfLinks = 0;
            //ULONG NumberOfLinks;


            QueryResponse.DeletePending = FALSE;
            QueryResponse.Directory = (attributes & FILE_ATTRIBUTE_DIRECTORY)?TRUE:FALSE;
            QueryResponse.Index_Num.QuadPart = 0;
            QueryResponse.EASize = 0; //we dont support extended attributes
            QueryResponse.AccessFlags = QFI_FILE_READ_DATA |
                                          QFI_FILE_WRITE_DATA |
                                          QFI_FILE_APPEND_DATA |
                                          QFI_FILE_READ_ATTRIBUTES |
                                          QFI_FILE_WRITE_ATTRIBUTES |
                                          QFI_DELETE;

            QueryResponse.IndexNumber.QuadPart = 0;
            QueryResponse.CurrentByteOffset.QuadPart = ulFileOffset;
            QueryResponse.Mode = QFI_FILE_WRITE_THROUGH;
            QueryResponse.AlignmentRequirement = 0;
            QueryResponse.FileNameLength = uiFileNameLen;

            memcpy(pQueryResponse, &QueryResponse, sizeof(SMB_QUERY_FILE_ALL_INFO));

            BYTE *pFileInBlob = NULL;
            if(FAILED(RAPI.ReserveBlock(uiFileNameLen, (BYTE**)&pFileInBlob)))
              goto Error;

            ASSERT(pFileInBlob == (BYTE*)(pQueryResponse+1));
            memcpy(pFileInBlob, pFileName, uiFileNameLen);
            break;
        }
        default:
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- unknown Trans2 query for SMB_Trans2_Query_Path_Information %d", wLevel));
            //
            // Mark that we are using NT status codes
            dwRet = ERROR_CODE(STATUS_INVALID_LEVEL);
            goto ErrorSet;
    }
    dwRet = 0;
    goto Done;
    Error:
        dwRet = ERROR_CODE(STATUS_INVALID_LEVEL);
    ErrorSet:
            {
                BYTE *pTempBuf = (BYTE *)(pResponse+1);
                *pTempBuf = 0;
                *(pTempBuf+1) = 0;
                *(pTempBuf+2) = 0;
                *puiUsed = 3;
                ASSERT(0 != dwRet);
            }
    Done:

        //
        // On error just return that error
        if(0 != dwRet)
            return dwRet;

        //fill out response SMB
        memset(pResponse, 0, sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE));

        //word count is 3 bytes (1=WordCount 2=ByteCount) less than the response
        pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE) - 3) / sizeof(WORD);
        pResponse->TotalParameterCount = RAPI.ParamBytesUsed();
        pResponse->TotalDataCount = RAPI.DataBytesUsed();
        pResponse->ParameterCount = RAPI.ParamBytesUsed();
        pResponse->ParameterOffset = RAPI.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->ParameterDisplacement = 0;
        pResponse->DataCount = RAPI.DataBytesUsed();
        pResponse->DataOffset = RAPI.DataOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->DataDisplacement = 0;
        pResponse->SetupCount = 0;
        pResponse->ByteCount = RAPI.TotalBytesUsed();

        ASSERT(10 == pResponse->WordCount);

        //set the number of bytes we used
        *puiUsed = RAPI.TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE);

        return dwRet;
}


DWORD SMB_Trans2_Find_First(USHORT usTid,
                         SMB_COM_TRANSACTION2_CLIENT_REQUEST *pRequest,
                         StringTokenizer *pTokenizer,
                         SMB_PROCESS_CMD *_pRawResponse,
                         SMB_COM_TRANSACTION2_SERVER_RESPONSE *pResponse,
                         SMB_PACKET *pSMB,
                         UINT *puiUsed)
{
    DWORD dwRet = 0;
    SMB_TRANS2_FIND_FIRST2_CLIENT_REQUEST  *pffRequest = NULL;
    SMB_TRANS2_FIND_FIRST2_SERVER_RESPONSE *pffResponse = NULL;

    StringConverter SearchString;
    ce::smart_ptr<TIDState> pTIDState;
    USHORT usSID = 0xFFFF;
    SMB_FIND_FILE_BOTH_DIRECTORY_INFO_STRUCT *pPrevFile = NULL;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    ULONG uiResumeKeySize = 0;
    BOOL fUsingUnicode = FALSE;
    HRESULT hr = E_FAIL;

    RAPIBuilder MYRAPIBuilder((BYTE *)(pResponse+1),
               _pRawResponse->uiDataSize-sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE),
               pRequest->MaxParameterCount,
               pRequest->MaxDataCount);


    //
    // Make sure we have some memory to give an answer in
    if(FAILED(MYRAPIBuilder.ReserveParams(sizeof(SMB_TRANS2_FIND_FIRST2_SERVER_RESPONSE), (BYTE**)&pffResponse))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- not enough memory in request -- sending back request for more"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Get the TRANS2 Find First 2 structure
    if(FAILED(pTokenizer->GetByteArray((BYTE **)&pffRequest, sizeof(SMB_TRANS2_FIND_FIRST2_CLIENT_REQUEST)))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error -- not enough memory for find first 2 in trans2"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error -- trans2 had name but it was invalid"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Find a share state
    if(FAILED(hr = pMyConnection->FindTIDState(usTid, pTIDState, SEC_READ)) || !pTIDState) {
        if(E_ACCESSDENIED == hr) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- dont have permissions!!"));
            dwRet = ERROR_CODE(STATUS_ACCESS_DENIED);
        } else {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- couldnt find share state!!"));
            dwRet = ERROR_CODE(STATUS_INVALID_HANDLE);
        }
        goto Done;
    }

    


    if(FAILED(SearchString.append(pTIDState->GetShare()->GetDirectoryName()))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- couldnt build search string!!"));
        dwRet = ERROR_CODE(STATUS_INVALID_HANDLE);
        goto Done;
    }

    fUsingUnicode = pMyConnection->SupportsUnicode(pSMB->pInSMB, SMB_COM_TRANSACTION2, TRANS2_FIND_FIRST2);

    if(TRUE == fUsingUnicode) {
        //
        // Get the file name from the tokenizer
        WCHAR *pFileName;
        if(FAILED(pTokenizer->GetUnicodeString(&pFileName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error -- trans2 had name but it was invalid"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Put in a \ if its not already there
        if('\\' != pFileName[0]) {
            if(FAILED(SearchString.append("\\"))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error -- trans2 had name but it was invalid"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }
        }

        //
        // Append the filename
        if(FAILED(SearchString.append(pFileName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- couldnt build search string!!"));
            dwRet = ERROR_CODE(STATUS_INVALID_HANDLE);
            goto Done;
        }
    } else {
        //
        // Get the file name from the tokenizer
        CHAR *pFileName;
        if(FAILED(pTokenizer->GetString(&pFileName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error -- trans2 had name but it was invalid"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Put in a \ if its not already there
        if('\\' != pFileName[0]) {
            if(FAILED(SearchString.append("\\"))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error -- trans2 had name but it was invalid"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }
        }

        //
        // And append the filename
        if(FAILED(SearchString.append(pFileName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- couldnt build search string!!"));
            dwRet = ERROR_CODE(STATUS_INVALID_HANDLE);
            goto Done;
        }
    }

    //
    // Fill out the find first2 response structure with meaningful data
    pffResponse->Sid = 0;
    pffResponse->SearchCount = 0;
    pffResponse->EndOfSearch = 1;
    pffResponse->EaErrorOffset = 0;
    pffResponse->LastNameOffset = 0;

    //
    // Fill out the TRANS2 response structure
    pResponse->WordCount = 10;

    //
    // We wont be alligned if we dont make a gap
    MYRAPIBuilder.MakeParamByteGap(sizeof(DWORD) - ((UINT)MYRAPIBuilder.DataPtr() % sizeof(DWORD)));
    ASSERT(0 == (UINT)MYRAPIBuilder.DataPtr() % 4);

    //
    // See what flags are used
    if(0 != (pffRequest->Flags & FIND_NEXT_RETURN_RESUME_KEY)) {
        uiResumeKeySize = 0;//sizeof(ULONG);
    }

    //
    // Start searching for files....
    if (SUCCEEDED(pMyConnection->CreateNewFindHandle(SearchString.GetString(), &usSID, fUsingUnicode))) {
        TRACEMSG(ZONE_FILES, (L"SMB_SRV: FindFirstFile2 using SID: 0x%x", usSID));

        pffResponse->Sid = usSID; //dont do this in the CreateNewFindHandle b/c of alignment
        do {
            WIN32_FIND_DATA *pwfd = NULL;

            if(FAILED(pMyConnection->NextFile(pffResponse->Sid, &pwfd))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- ActiveConnection::NextFile failed!"));
                break;
            }

            //
            // See what information level the client wants
            switch(pffRequest->InformationLevel) {
                case SMB_INFO_STANDARD:
                {
                    StringConverter FileName;
                    UINT uiFileBlobSize = 0;
                    BYTE *pNewBlock = NULL;
                    BYTE *pFileBlob = NULL;
                    SMB_FIND_FILE_STANDARD_INFO_STRUCT *pFileStruct = NULL;
                    if(FAILED(FileName.append(pwfd->cFileName))) {
                        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- not enough memory (OOM) -- failing request"));
                        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                        goto Done;
                    }

                    //
                    // Get back a STRING
                    if(NULL == (pFileBlob = FileName.NewSTRING(&uiFileBlobSize,  fUsingUnicode))) {
                        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- not enough memory (OOM) -- failing request"));
                        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                        goto Done;
                    }

                    //
                    // If we cant reserve enough memory for this block we need to start up a
                    //   FindNext setup
                    if(FAILED(MYRAPIBuilder.ReserveBlock(uiResumeKeySize + uiFileBlobSize + sizeof(SMB_FIND_FILE_STANDARD_INFO_STRUCT), (BYTE**)&pFileStruct))) {
                        dwRet = 0;
                        pffResponse->EndOfSearch = 0;
                        LocalFree(pFileBlob);
                        goto SendOK;
                    }

                    //
                    // Set a bogus resume key value
                    if(0 != uiResumeKeySize) {
                        ULONG *ulVal = (ULONG *)pFileStruct;
                        ASSERT(sizeof(ULONG) == uiResumeKeySize);
                        *ulVal = 0;
                        pFileStruct = (SMB_FIND_FILE_STANDARD_INFO_STRUCT *)(((BYTE *)pFileStruct) + uiResumeKeySize);
                    }

                    pNewBlock = (BYTE *)(pFileStruct + 1);

                    SMB_DATE smbDate;
                    SMB_TIME smbTime;

                    if(FAILED(FileTimeToSMBTime(&pwfd->ftCreationTime, &smbTime, &smbDate))) {
                        ASSERT(FALSE);
                        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                        goto Done;
                    }
                    pFileStruct->CreationDate = smbDate;
                    pFileStruct->CreationTime = smbTime;

                    if(FAILED(FileTimeToSMBTime(&pwfd->ftLastAccessTime, &smbTime, &smbDate))) {
                        ASSERT(FALSE);
                        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                        goto Done;
                    }
                    pFileStruct->LastAccessDate = smbDate;
                    pFileStruct->LastAccessTime = smbTime;

                    if(FAILED(FileTimeToSMBTime(&pwfd->ftLastWriteTime, &smbTime, &smbDate))) {
                        ASSERT(FALSE);
                        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                        goto Done;
                    }
                    pFileStruct->LastWriteDate = smbDate;
                    pFileStruct->LastWriteTime = smbTime;

                    pFileStruct->DataSize = pwfd->nFileSizeLow;
                    pFileStruct->AllocationSize = 0;
                    pFileStruct->Attributes = Win32AttributeToDos(pwfd->dwFileAttributes);
                    pFileStruct->FileNameLen = uiFileBlobSize;

                    memcpy(pNewBlock, pFileBlob, uiFileBlobSize);
                    LocalFree(pFileBlob);

                    //
                    // Since we've successfully made a record, increase the search
                    //   count
                    pffResponse->SearchCount ++;
                    break;
                }
                case SMB_FIND_FILE_BOTH_DIRECTORY_INFO:
                {
                    StringConverter FileName;

                    UINT uiFileBlobSize = 0;
                    BYTE *pNewBlock = NULL;
                    BYTE *pFileBlob = NULL;
                    SMB_FIND_FILE_BOTH_DIRECTORY_INFO_STRUCT *pFileStruct = NULL;

                    if(FAILED(FileName.append(pwfd->cFileName))) {
                        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- not enough memory (OOM) -- failing request"));
                        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                        goto Done;
                    }

                    //
                    // Get back a STRING
                    if(NULL == (pFileBlob = FileName.NewSTRING(&uiFileBlobSize,  fUsingUnicode))) {
                        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- not enough memory (OOM) -- failing request"));
                        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                        goto Done;
                    }

                    //
                    // If we cant reserve enough memory for this block we need to start up a
                    //   FindNext setup
                    if(FAILED(MYRAPIBuilder.ReserveBlock(uiFileBlobSize + sizeof(SMB_FIND_FILE_BOTH_DIRECTORY_INFO_STRUCT), (BYTE**)&pFileStruct, sizeof(DWORD)))) {
                        dwRet = 0;
                        pffResponse->EndOfSearch = 0;
                        LocalFree(pFileBlob);
                        goto SendOK;
                    }
                    pNewBlock = (BYTE *)(pFileStruct + 1);

                    pFileStruct->FileIndex = 0;
                    pFileStruct->NextEntryOffset = 0;
                    if(pPrevFile)
                        pPrevFile->NextEntryOffset = (UINT)pFileStruct - (UINT)pPrevFile;
                    pFileStruct->CreationTime.LowPart = pwfd->ftCreationTime.dwLowDateTime;
                    pFileStruct->CreationTime.HighPart = pwfd->ftCreationTime.dwHighDateTime;
                    pFileStruct->LastAccessTime.LowPart = pwfd->ftLastAccessTime.dwLowDateTime;
                    pFileStruct->LastAccessTime.HighPart = pwfd->ftLastAccessTime.dwHighDateTime;
                    pFileStruct->LastWriteTime.LowPart = pwfd->ftLastWriteTime.dwLowDateTime;
                    pFileStruct->LastWriteTime.HighPart = pwfd->ftLastWriteTime.dwHighDateTime;
                    pFileStruct->ChangeTime.LowPart = pwfd->ftLastWriteTime.dwLowDateTime;
                    pFileStruct->ChangeTime.HighPart = pwfd->ftLastWriteTime.dwHighDateTime;
                    pFileStruct->EndOfFile.LowPart = pwfd->nFileSizeLow;
                    pFileStruct->EndOfFile.HighPart = pwfd->nFileSizeHigh;
                    pFileStruct->ExtFileAttributes = Win32AttributeToDos(pwfd->dwFileAttributes);
                    pFileStruct->FileNameLength = uiFileBlobSize;
                    pFileStruct->EaSize = 0;
                    pFileStruct->ShortNameLength = 0;
                    memset(pFileStruct->ShortName, 0, sizeof(pFileStruct->ShortName));
                    memcpy(pNewBlock, pFileBlob, uiFileBlobSize);
                    LocalFree(pFileBlob);

                    //
                    // Since we've successfully made a record, increase the search
                    //   count
                    pffResponse->SearchCount ++;
                    pPrevFile = pFileStruct;
                    break;
                }
                default:
                    TRACEMSG(ZONE_FILES, (L"SMB_FILE:  invalid level 0x%08x",pffRequest->InformationLevel));
                    SMB_COM_GENERIC_RESPONSE *pGenricResponse = (SMB_COM_GENERIC_RESPONSE *)(pResponse);
                    pGenricResponse->ByteCount = 0;
                    pGenricResponse->WordCount = 0;
                    *puiUsed = sizeof(SMB_COM_GENERIC_RESPONSE);
                    dwRet = ERROR_CODE(STATUS_INVALID_LEVEL);
            pMyConnection->CloseFindHandle(pffResponse->Sid);
                    goto Done;
            }

        } while (SUCCEEDED(pMyConnection->AdvanceToNextFile(pffResponse->Sid)));

        //
        // If we get here, yippie! we can fit the entire search into one packet!
        //    just close up shop
        pMyConnection->CloseFindHandle(pffResponse->Sid);
        pffResponse->Sid = 0;
    }else {
        TRACEMSG(ZONE_FILES, (L"SMB_FILE:  searching for %s failed! -- file not found",SearchString.GetString()));
        SMB_COM_GENERIC_RESPONSE *pGenricResponse = (SMB_COM_GENERIC_RESPONSE *)(pResponse);
        pGenricResponse->ByteCount = 0;
        pGenricResponse->WordCount = 0;
        *puiUsed = sizeof(SMB_COM_GENERIC_RESPONSE);
        dwRet = ERROR_CODE(STATUS_NO_SUCH_FILE);
        goto Done;
    }

    SendOK:
    dwRet = 0;
    pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE) - 3) / sizeof(WORD);
    pResponse->TotalParameterCount = MYRAPIBuilder.ParamBytesUsed();
    pResponse->TotalDataCount = MYRAPIBuilder.DataBytesUsed();
    pResponse->Reserved = 0;
    pResponse->ParameterCount = MYRAPIBuilder.ParamBytesUsed();
    pResponse->ParameterOffset = MYRAPIBuilder.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
    pResponse->ParameterDisplacement = 0;
    pResponse->DataCount = MYRAPIBuilder.DataBytesUsed();
    pResponse->DataOffset = MYRAPIBuilder.DataOffset((BYTE *)_pRawResponse->pSMBHeader);
    pResponse->DataDisplacement = 0;
    pResponse->SetupCount = 0;
    pResponse->ByteCount = MYRAPIBuilder.TotalBytesUsed();
    ASSERT(10 == pResponse->WordCount);
    *puiUsed = MYRAPIBuilder.TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE);

    Done:
        return dwRet;
}


DWORD SMB_Trans2_Find_Next(USHORT usTid,
                         SMB_COM_TRANSACTION2_CLIENT_REQUEST *pRequest,
                         StringTokenizer *pTokenizer,
                         SMB_PROCESS_CMD *_pRawResponse,
                         SMB_COM_TRANSACTION2_SERVER_RESPONSE *pResponse,
                         SMB_PACKET *pSMB,
                         UINT *puiUsed)
{
    DWORD dwRet = 0;
    SMB_TRANS2_FIND_NEXT2_CLIENT_REQUEST  *pffRequest = NULL;
    SMB_TRANS2_FIND_NEXT2_SERVER_RESPONSE *pffResponse = NULL;

    ce::smart_ptr<TIDState> pTIDState;
    USHORT usSID = 0xFFFF;
    SMB_FIND_FILE_BOTH_DIRECTORY_INFO_STRUCT *pPrevFile = NULL;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    BOOL fUsingUnicode = FALSE;

    RAPIBuilder MYRAPIBuilder((BYTE *)(pResponse+1),
               _pRawResponse->uiDataSize-sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE),
               pRequest->MaxParameterCount,
               pRequest->MaxDataCount);


    //
    // Make sure we have some memory to give an answer in
    if(FAILED(MYRAPIBuilder.ReserveParams(sizeof(SMB_TRANS2_FIND_NEXT2_SERVER_RESPONSE), (BYTE**)&pffResponse))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- not enough memory in request -- sending back request for more"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto JustExit;
    }

    //
    // Get the TRANS2 Find First 2 structure
    if(FAILED(pTokenizer->GetByteArray((BYTE **)&pffRequest, sizeof(SMB_TRANS2_FIND_NEXT2_CLIENT_REQUEST))) || NULL == pffRequest) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error -- not enough memory for find first 2 in trans2"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto JustExit;
    }

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error -- trans2 had name but it was invalid"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto JustExit;
    }

    //
    // Find a share state
    if(FAILED(pMyConnection->FindTIDState(usTid, pTIDState, SEC_READ)) || !pTIDState) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- couldnt find share state!!"));
        dwRet = ERROR_CODE(STATUS_INVALID_HANDLE);
        goto Done;
    }

    fUsingUnicode = pMyConnection->SupportsUnicode(pSMB->pInSMB, SMB_COM_TRANSACTION2, TRANS2_FIND_NEXT2);

    //
    // Fill out the find first2 response structure with meaningful data
    pffResponse->SearchCount = 0;
    pffResponse->EndOfSearch = 1;
    pffResponse->EaErrorOffset = 0;
    pffResponse->LastNameOffset = 0;

    //
    // We wont be alligned if we dont make a gap
    MYRAPIBuilder.MakeParamByteGap(sizeof(DWORD) - ((UINT)MYRAPIBuilder.DataPtr() % sizeof(DWORD)));
    ASSERT(0 == (UINT)MYRAPIBuilder.DataPtr() % 4);

    //
    // Start searching for files....
    usSID = pffRequest->Sid;

    //
    // Handle any flags that need to be taken care of here
    if(0 != (pffRequest->Flags & FIND_NEXT_RESUME_FROM_PREV)) {
        TRACEMSG(ZONE_FILES, (L"SMB_SRV:  FindNext -- starting from the beginning!"));
        if(FAILED(pMyConnection->ResetSearch(usSID))) {
            ASSERT(FALSE);
            TRACEMSG(ZONE_ERROR, (L"SMB_SRV:  FindNext -- reseting failed!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
    } else {
        TRACEMSG(ZONE_FILES, (L"SMB_SRV:  FindNext -- continuing from where we started!"));
    }

    TRACEMSG(ZONE_FILES, (L"SMB_SRV: FindNextFile2 using SID: 0x%x", usSID));
    do {
        SMB_FIND_FILE_BOTH_DIRECTORY_INFO_STRUCT *pFileStruct = NULL;
        WIN32_FIND_DATA *pwfd = NULL;

        if(FAILED(pMyConnection->NextFile(usSID, &pwfd))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- ActiveConnection::NextFile failed!"));
            break;
        }

        //
        // See what information level the client wants
        switch(pffRequest->InformationLevel) {
            case SMB_FIND_FILE_BOTH_DIRECTORY_INFO:
            {
                StringConverter FileName;
                UINT uiFileBlobSize = 0;
                BYTE *pNewBlock = NULL;
                BYTE *pFileBlob = NULL;
                if(FAILED(FileName.append(pwfd->cFileName))) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- not enough memory (OOM) -- failing request"));
                    dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                    goto Done;
                }

                //
                // Get back a STRING
                if(NULL == (pFileBlob = FileName.NewSTRING(&uiFileBlobSize,  fUsingUnicode))) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- not enough memory (OOM) -- failing request"));
                    dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                    goto Done;
                }

                //
                // If we cant reserve enough memory for this block we need to start up a
                //   FindNext setup
                if(FAILED(MYRAPIBuilder.ReserveBlock(uiFileBlobSize + sizeof(SMB_FIND_FILE_BOTH_DIRECTORY_INFO_STRUCT), (BYTE**)&pFileStruct, sizeof(DWORD)))) {
                    dwRet = 0;
                    pffResponse->EndOfSearch = 0;
                    LocalFree(pFileBlob);
                    goto SendOK;
                }
                pNewBlock = (BYTE *)(pFileStruct + 1);


                pFileStruct->FileIndex = 0;
                pFileStruct->NextEntryOffset = 0;
                if(pPrevFile)
                    pPrevFile->NextEntryOffset = (UINT)pFileStruct - (UINT)pPrevFile;
                pFileStruct->CreationTime.LowPart = pwfd->ftCreationTime.dwLowDateTime;
                pFileStruct->CreationTime.HighPart = pwfd->ftCreationTime.dwHighDateTime;
                pFileStruct->LastAccessTime.LowPart = pwfd->ftLastAccessTime.dwLowDateTime;
                pFileStruct->LastAccessTime.HighPart = pwfd->ftLastAccessTime.dwHighDateTime;
                pFileStruct->LastWriteTime.LowPart = pwfd->ftLastWriteTime.dwLowDateTime;
                pFileStruct->LastWriteTime.HighPart = pwfd->ftLastWriteTime.dwHighDateTime;
                pFileStruct->ChangeTime.LowPart = pwfd->ftLastWriteTime.dwLowDateTime;
                pFileStruct->ChangeTime.HighPart = pwfd->ftLastWriteTime.dwHighDateTime;
                pFileStruct->EndOfFile.LowPart = pwfd->nFileSizeLow;
                pFileStruct->EndOfFile.HighPart = pwfd->nFileSizeHigh;
                pFileStruct->ExtFileAttributes = Win32AttributeToDos(pwfd->dwFileAttributes);
                pFileStruct->FileNameLength = uiFileBlobSize;
                pFileStruct->EaSize = 0;
                pFileStruct->ShortNameLength = 0;
                memset(pFileStruct->ShortName, 0, sizeof(pFileStruct->ShortName));
                memcpy(pNewBlock, pFileBlob, uiFileBlobSize);
                LocalFree(pFileBlob);

                //
                // Since we've successfully made a record, increase the search
                //   count
                pffResponse->SearchCount ++;
            }
            break;
            default:
                ASSERT(FALSE); //not supported!!
                break;
        }
       pPrevFile = pFileStruct;
    } while (SUCCEEDED(pMyConnection->AdvanceToNextFile(usSID)));

    //
    // If get here, there arent any more files left -- see if we are supposed to close our handle
    if(0 != (pffRequest->Flags & FIND_NEXT_CLOSE_AT_END)) {
      TRACEMSG(ZONE_FILES, (L"SMB_SRV:  FindNext -- closing up because there are no more files!"));
      if(FAILED(pMyConnection->CloseFindHandle(usSID))) {
          ASSERT(FALSE);
          TRACEMSG(ZONE_ERROR, (L"SMB_SRV:  FindNext -- close failed!"));
      }
    }

    SendOK:
        dwRet = 0;
        pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE) - 3) / sizeof(WORD);
        pResponse->TotalParameterCount = MYRAPIBuilder.ParamBytesUsed();
        pResponse->TotalDataCount = MYRAPIBuilder.DataBytesUsed();
        pResponse->Reserved = 0;
        pResponse->ParameterCount = MYRAPIBuilder.ParamBytesUsed();
        pResponse->ParameterOffset = MYRAPIBuilder.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->ParameterDisplacement = 0;
        pResponse->DataCount = MYRAPIBuilder.DataBytesUsed();
        pResponse->DataOffset = MYRAPIBuilder.DataOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->DataDisplacement = 0;
        pResponse->SetupCount = 0;
        pResponse->ByteCount = MYRAPIBuilder.TotalBytesUsed();
        ASSERT(10 == pResponse->WordCount);
        *puiUsed = MYRAPIBuilder.TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE);

    Done:
        //
        // If they want us to close the key do it now
        if(0 != (pffRequest->Flags & FIND_NEXT_CLOSE_AFTER_REQUEST)) {
          TRACEMSG(ZONE_FILES, (L"SMB_SRV:  FindNext -- closing up due to request from client!"));
          if(FAILED(pMyConnection->CloseFindHandle(usSID))) {
              ASSERT(FALSE);
              TRACEMSG(ZONE_ERROR, (L"SMB_SRV:  FindNext -- close failed!"));
          }
        }
   JustExit:
        return dwRet;
}


DWORD SMB_FILE::SMB_Com_NT_Cancel(SMB_PROCESS_CMD *_pRawRequest,
                                   SMB_PROCESS_CMD *_pRawResponse,
                                   UINT *puiUsed,
                                   SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    SMB_COM_GENERIC_RESPONSE *pResponse =
            (SMB_COM_GENERIC_RESPONSE *)_pRawResponse->pDataPortion;

    //
    // Verify that we have enough memory
    if(_pRawResponse->uiDataSize < sizeof(SMB_COM_GENERIC_RESPONSE)) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_NT_Cancel -- not enough memory for response!"));
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }


    //
    //  see if we can cancel the request through the connection manager
    //  attempts to find the active connection and cancel wake up events
    //  this call will ensure that valid locking is maintained
    //
    HRESULT hr = SMB_Globals::g_pConnectionManager->CancelWakeUpEvent(
                                                            pSMB,
                                                            _pRawRequest->pSMBHeader->Uid,
                                                            _pRawRequest->pSMBHeader->Pid,
                                                            _pRawRequest->pSMBHeader->Tid,
                                                            _pRawRequest->pSMBHeader->Mid);
    if(FAILED( hr )) {
        ASSERT( FALSE );
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }


    pResponse->ByteCount = 0;
    pResponse->WordCount = 0;
    *puiUsed = sizeof(SMB_COM_GENERIC_RESPONSE);

    Done:
        return dwRet;
}



DWORD SMB_FILE::SMB_Com_FindClose2(SMB_PROCESS_CMD *_pRawRequest,
                                   SMB_PROCESS_CMD *_pRawResponse,
                                   UINT *puiUsed,
                                   SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    SMB_TRANS2_FIND_CLOSE_CLIENT_REQUEST *pRequest =
            (SMB_TRANS2_FIND_CLOSE_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_GENERIC_RESPONSE *pResponse =
            (SMB_COM_GENERIC_RESPONSE *)_pRawResponse->pDataPortion;

    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;

    //
    // Verify that we have enough memory
    if(_pRawRequest->uiDataSize < sizeof(SMB_TRANS2_FIND_CLOSE_CLIENT_REQUEST)) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_FindClose2 -- not enough memory for request!"));
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_COM_GENERIC_RESPONSE)) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_FindClose2 -- not enough memory for response!"));
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_FindClose2: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error -- trans2 had name but it was invalid"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Close up
    if(FAILED(pMyConnection->CloseFindHandle(pRequest->Sid))) {
          TRACEMSG(ZONE_ERROR, (L"SMB_SRV:  FindNext -- close failed! looking for SID:0x%x", pRequest->Sid));
    }
    pResponse->ByteCount = 0;
    pResponse->WordCount = 0;
    *puiUsed = sizeof(SMB_COM_GENERIC_RESPONSE);

    Done:
        return dwRet;
}





VOID
FileTimeToDosDateTime(
    FILETIME * ft,
    WORD    * ddate,
    WORD    * dtime
    )
{
    SYSTEMTIME st;
    WORD tmp;

    FileTimeToSystemTime(ft,&st);
    tmp = st.wYear - 1980;
    tmp <<= 7;
    tmp |= st.wMonth & 0x000f;
    tmp <<= 4;
    tmp |= st.wDay & 0x001f;
    *ddate = tmp;

    tmp = st.wHour;
    tmp <<= 5;
    tmp |= st.wMinute & 0x003f;
    tmp <<= 6;
    tmp |= (st.wSecond/2) & 0x001f;
    *dtime = tmp;
}

BOOL
DosDateTimeToFileTime(
    FILETIME * ft,
    WORD    ddate,
    WORD    dtime
    )
{
    SYSTEMTIME st;
    WORD Month, Day, Year;
    Day = ddate & 0x1F; //(first 5 bits)
    Month = (ddate >> 5) & 0xF; //(4 bits, after the Day)
    Year = ((ddate >> 9) & 0x7F) + 1980; //(7 bits)

    WORD Hour, Minute, Second;
    Second = ((dtime) & 0x1F) * 2;
    Minute = (dtime >> 5) & 0x3F;
    Hour = (dtime >> 11) & 0x1F;

    st.wYear = Year;
    st.wMonth = Month;
    st.wDayOfWeek = 0; //ignored
    st.wDay = Day;
    st.wHour = Hour;
    st.wMinute = Minute;
    st.wSecond = Second;
    st.wMilliseconds = 0;

    return SystemTimeToFileTime(&st, ft);
}



#define _70_to_80_bias    0x012CEA600L
#define SECS_IN_DAY (60L*60L*24L)
#define SEC2S_IN_DAY (30L*60L*24L)
#define FOURYEARS    (3*365+366)

WORD MonTotal[14] = { 0,        // dummy entry for month 0
    0,                                    // days before Jan 1
    31,                                    // days before Feb 1
    31+28,                                // days before Mar 1
    31+28+31,                            // days before Apr 1
    31+28+31+30,                        // days before May 1
    31+28+31+30+31,                        // days before Jun 1
    31+28+31+30+31+30,                    // days before Jul 1
    31+28+31+30+31+30+31,                // days before Aug 1
    31+28+31+30+31+30+31+31,             // days before Sep 1
    31+28+31+30+31+30+31+31+30,            // days before Oct 1
    31+28+31+30+31+30+31+31+30+31,        // days before Nov 1
    31+28+31+30+31+30+31+31+30+31+30,    // days before Dec 1
    31+28+31+30+31+30+31+31+30+31+30+31    // days before end of year
};

#define YR_MASK        0xFE00
#define LEAPYR_MASK    0x0600
#define YR_BITS        7
#define MON_MASK    0x01E0
#define MON_BITS    4
#define DAY_MASK    0x001F
#define DAY_BITS    5

#define HOUR_MASK    0xF800
#define HOUR_BITS    5
#define MIN_MASK    0x07E0
#define MIN_BITS    6
#define SEC2_MASK    0x001F
#define SEC2_BITS    5


DWORD
DosToNetTime(WORD time, WORD date)
{
    WORD days, secs2;

    days = date >> (MON_BITS + DAY_BITS);
    days = days*365 + days/4;            // # of years in days
    days += (date & DAY_MASK) + MonTotal[(date&MON_MASK) >> DAY_BITS];
    if ((date&LEAPYR_MASK) == 0 && (date&MON_MASK) <= (2<<DAY_BITS))
        --days;                        // adjust days for early in leap year

    secs2 = ( ((time&HOUR_MASK) >> (MIN_BITS+SEC2_BITS)) * 60
                + ((time&MIN_MASK) >> SEC2_BITS) ) * 30
                + (time&SEC2_MASK);
    return (DWORD)days*SECS_IN_DAY + _70_to_80_bias + (DWORD)secs2*2;
}


/*struct ChangeNotifiyCallbackStruct {
    ce::smart_ptr<ActiveConnection> pActiveConn;
    SMB_PACKET *pSMB;
};*/



ChangeNotificationWakeUpNode::ChangeNotificationWakeUpNode(
                                        SMB_PACKET *pSMB,
                                        ce::smart_ptr<ActiveConnection> pActiveConnection,
                                        HANDLE h,
                                        UINT uiMaxReturnBuffer) :
                                                                    m_pSMB(pSMB),
                                                                    m_pActiveConn(pActiveConnection),
                                                                    m_h(h),
                                                                    m_uiMaxReturnBuffer(uiMaxReturnBuffer)
{
    InitializeCriticalSection( &m_csSendLock );
}


ChangeNotificationWakeUpNode::~ChangeNotificationWakeUpNode()
{
    DeleteCriticalSection( &m_csSendLock );
}


VOID ChangeNotificationWakeUpNode::WakeUp()
{
    TRACEMSG(ZONE_FILES, (L"SMBSRVR: Sending change notification to connection: %d", m_pActiveConn->ConnectionID()));
    HRESULT hr;

    if(! SMB_Globals::g_pConnectionManager->SendPacket(m_pSMB, this) ) {
        TRACEMSG(ZONE_FILES, (L"SMBSRVR: Wokeup but no change notification of interest for connID: %d", m_pActiveConn->ConnectionID()));
        ASSERT(FALSE);
    }

    SMB_Globals::g_pWakeUpOnEvent->RemoveEvent(m_usID);
    hr = m_pActiveConn->RemoveWakeUpEvent(m_usID);
    ASSERT(SUCCEEDED(hr));
    this->Release();
}


VOID ChangeNotificationWakeUpNode::Terminate(bool nolock)
{
    TRACEMSG(ZONE_FILES, (L"SMBSRVR: Sending terminate for change notification to connection: %d", m_pActiveConn->ConnectionID()));
    HRESULT hr;

    m_pSMB->uiPacketType = SMB_DISCARD_PACKET;
    if( true == nolock ) {
        this->SendPacket();
    }
    else {
        SMB_Globals::g_pConnectionManager->SendPacket(m_pSMB, this);
    }


    hr = SMB_Globals::g_pWakeUpOnEvent->RemoveEvent(m_usID);
    ASSERT(SUCCEEDED(hr));
    this->Release();
}


BOOL ChangeNotificationWakeUpNode::SendPacket()
{
    //
    //  returns TRUE if we need to clean up (we may not send teh packet)
    //
    SMB_HEADER *pHeader = (SMB_HEADER *)(m_pSMB->OutSMB);
    DWORD dwRet = 0;
    DWORD dwUsed = 0;
    DWORD dwFileBlobSize = 0;
    BOOL fSentPacket = FALSE;

    CCritSection csLock( &m_csSendLock );
    csLock.Lock();

    SMB_COM_NT_TRANSACTION_SERVER_RESPONSE *pResponse =
            (SMB_COM_NT_TRANSACTION_SERVER_RESPONSE *)(BYTE*)(pHeader+1);

    FILE_NOTIFY_INFORMATION *pNotifyStructs = NULL;
    FILE_NOTIFY_INFORMATION *pNotifyStructsTemp = NULL;
    SMB_COM_NOTIFY_INFORMATION *pFirstInPacket = NULL;
    SMB_COM_NOTIFY_INFORMATION *pLastUsedInPacket = NULL;
    memset(pResponse, 0, sizeof(SMB_COM_NT_TRANSACTION_SERVER_RESPONSE));
    pResponse->WordCount = (sizeof(SMB_COM_NT_TRANSACTION_SERVER_RESPONSE)-3) / sizeof(WORD);
    ASSERT(18 == pResponse->WordCount);

    dwUsed += sizeof(SMB_HEADER);
    dwUsed += sizeof(SMB_COM_NT_TRANSACTION_SERVER_RESPONSE);

    //
    // Query the FS to see how much data we need
    /*if(0 != CeGetFileNotificationInfo(m_h, 0, NULL, 0, NULL, &dwFileBlobSize)) {

    //
    // If we cant allocate enough space, just return empty
    if(dwFileBlobSize && NULL != (pNotifyStructs = (FILE_NOTIFY_INFORMATION*)new BYTE[dwFileBlobSize])) {
    if(!CeGetFileNotificationInfo(m_h, 0, pNotifyStructs, dwFileBlobSize, NULL, NULL)) {
    delete [] (BYTE *)pNotifyStructs;
    pNotifyStructs = NULL;
    } else {
    pNotifyStructsTemp = pNotifyStructs;
    }
    }
    }*/

    if(pNotifyStructsTemp) {
        DWORD dwRemaining = sizeof(m_pSMB->OutSMB) - (dwRet);

        while(dwRemaining && pNotifyStructsTemp) {
            BOOL fSkip = FALSE;

            if(!(pNotifyStructsTemp->Action & (FILE_ACTION_ADDED|
                                            FILE_ACTION_REMOVED|
                                            FILE_ACTION_MODIFIED|
                                            FILE_ACTION_RENAMED_OLD_NAME|
                                            FILE_ACTION_RENAMED_NEW_NAME))) {
                fSkip = TRUE;
            }

            /*if(pFirstInPacket) {
              pTemp = pFirstInPacket;
              }

              while(pTemp) {
            //
            // If this was already added, just OR up the actions
            if(0 == wcscmp(pTemp->FileName, pNotifyStructsTemp->FileName)) {
            pTemp->Action |= pNotifyStructsTemp->Action;
            fFound = TRUE;
            }

            if(pTemp->NextEntryOffset) {
            pTemp = (FILE_NOTIFY_INFORMATION *)((BYTE *)pTemp + pTemp->NextEntryOffset);
            } else {
            pTemp = NULL;
            }
            }*/

            //
            // If the item is new, we are at the end of the notify structs list, so we add it to the outgoing packet
            if(!fSkip) {
                //
                // Put the pointer on the head of a new node
                if(!pFirstInPacket) {
                    pFirstInPacket = (SMB_COM_NOTIFY_INFORMATION *)(pResponse+1);

                    // we have to align pFirstInPacket on a LONG (per spec on SMB_COM_NT_TRANSACTION_SERVER_RESPONSE);
                    while(0 != (((UINT)pFirstInPacket % sizeof(LONG)))) {
                        pFirstInPacket = (SMB_COM_NOTIFY_INFORMATION *)((BYTE *)pFirstInPacket+1);
                        dwUsed++; //update dwUsed here b/c all offsets below are relative to pFirstInPacket
                    }

                    // now align on LONGLONG per spec of FILE_NOTIFY_INFORMATION
                    while(0 != (((UINT)pFirstInPacket % sizeof(LONGLONG)))) {
                        pFirstInPacket = (SMB_COM_NOTIFY_INFORMATION *)((BYTE *)pFirstInPacket+1);
                        dwUsed++; //update dwUsed here b/c all offsets below are relative to pFirstInPacket
                    }

                    pLastUsedInPacket = pFirstInPacket;
                } else {
                    ASSERT(0 == pLastUsedInPacket->NextEntryOffset);
                    pLastUsedInPacket->NextEntryOffset = pLastUsedInPacket->FileNameLength+sizeof(SMB_COM_NOTIFY_INFORMATION);

                    // Spec wants 8 byte alignment
                    while(0 != (((UINT)pLastUsedInPacket + pLastUsedInPacket->NextEntryOffset) % 8)) {
                        pLastUsedInPacket->NextEntryOffset ++;
                    }
                    pLastUsedInPacket = (SMB_COM_NOTIFY_INFORMATION *)((BYTE *)pLastUsedInPacket + pLastUsedInPacket->NextEntryOffset);
                }

                //
                // If we have space in the packet for this, add it
                if(dwRemaining >= (sizeof(SMB_COM_NOTIFY_INFORMATION)+pNotifyStructsTemp->FileNameLength)) {
                    dwRemaining -= sizeof(SMB_COM_NOTIFY_INFORMATION);
                    dwRemaining -= pNotifyStructsTemp->FileNameLength;

                    pLastUsedInPacket->NextEntryOffset = 0;
                    memcpy((VOID*)(pLastUsedInPacket+1), pNotifyStructsTemp->FileName, pNotifyStructsTemp->FileNameLength);
                    pLastUsedInPacket->Action = pNotifyStructsTemp->Action & (FILE_ACTION_ADDED|FILE_ACTION_REMOVED|FILE_ACTION_MODIFIED|FILE_ACTION_RENAMED_OLD_NAME|FILE_ACTION_RENAMED_NEW_NAME);

                    TRACEMSG(ZONE_FILES, (L"SMB_SERV:  Sending change notify for %s -- action: %d", pNotifyStructsTemp->FileName, pLastUsedInPacket->Action));
                    pLastUsedInPacket->FileNameLength = pNotifyStructsTemp->FileNameLength;
                }
            }

            // Advance to the next entry
            if(pNotifyStructsTemp->NextEntryOffset) {
                pNotifyStructsTemp = (FILE_NOTIFY_INFORMATION *)((BYTE *)pNotifyStructsTemp + pNotifyStructsTemp->NextEntryOffset);
            } else {
                pNotifyStructsTemp = NULL;
            }
        }

        //
        // Update memory usage
        if(pLastUsedInPacket) {
            dwRet += ((DWORD)pLastUsedInPacket - (DWORD)pFirstInPacket);
            dwRet += pLastUsedInPacket->FileNameLength;
            dwRet += sizeof(SMB_COM_NOTIFY_INFORMATION);
        } else {
            ASSERT(FALSE == fSentPacket);
            goto Done;
        }
    }

    //
    // Only send the packet if there is enough memory (requested by the client)
    if(m_uiMaxReturnBuffer >= dwRet) {
        dwUsed += dwRet;
    } else {
        dwRet = 0;
    }

    if(pFirstInPacket) {
        pResponse->ParameterOffset = (UINT)pFirstInPacket - (UINT)pHeader;
    } else {
        pResponse->ParameterOffset = 0;
        ASSERT(0 == pResponse->ParameterCount);
    }
    pResponse->ParameterCount = dwRet;
    pResponse->TotalParameterCount = dwRet;

    //
    // We need to pad the 'data' part of the NT_TRANS to LONG
    BYTE *pData = ((BYTE*)pHeader) + dwUsed;
    while(0 != (((UINT)pData % sizeof(LONG)))) {
        pData = pData+1;
        dwUsed++;
    }
    pResponse->DataOffset = dwUsed;

    SendSMBResponse(m_pSMB, dwUsed, 0);

    fSentPacket = TRUE;
Done:
    if(pNotifyStructs) {
        delete [] (BYTE *)pNotifyStructs;
    }
    return fSentPacket;
}



HRESULT AddChangeNotification(SMB_PACKET *pSMB, SMBFileStream *pMyStream, NT_NOTIFY_CHANGE_STRUCT *pStruct, ce::smart_ptr<ActiveConnection> pMyConnection, UINT uiMaxReturnBuffer)
{
    DWORD dwNotifyFilter = 0;
    BOOL fWatchTree = (pStruct->WatchTree)?TRUE:FALSE;
    USHORT usNoteID;
    HRESULT hr = E_FAIL;
    HANDLE h = INVALID_HANDLE_VALUE;
    ChangeNotificationWakeUpNode *pWakeUp = NULL;
    const WCHAR *pOldListen = NULL;

    //
    // First create a change notification
    if(pStruct->CompletionFilter & SMB_FILE_NOTIFY_CHANGE_FILE_NAME) {
        dwNotifyFilter |= FILE_NOTIFY_CHANGE_FILE_NAME;
    }
    if(pStruct->CompletionFilter & SMB_FILE_NOTIFY_CHANGE_DIR_NAME) {
        dwNotifyFilter |= FILE_NOTIFY_CHANGE_DIR_NAME;
    }
    if(pStruct->CompletionFilter & SMB_FILE_NOTIFY_CHANGE_ATTRIBUTES) {
        dwNotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
    }
    if(pStruct->CompletionFilter & SMB_FILE_NOTIFY_CHANGE_SIZE) {
        dwNotifyFilter |= FILE_NOTIFY_CHANGE_SIZE;
    }
    if(pStruct->CompletionFilter & SMB_FILE_NOTIFY_CHANGE_LAST_WRITE) {
        dwNotifyFilter |= FILE_NOTIFY_CHANGE_LAST_WRITE;
    }
    if(pStruct->CompletionFilter & SMB_FILE_NOTIFY_CHANGE_LAST_ACCESS) {
        dwNotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
    }
    if(pStruct->CompletionFilter & SMB_FILE_NOTIFY_CHANGE_CREATION) {
        dwNotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
    }
    if(pStruct->CompletionFilter & SMB_FILE_NOTIFY_CHANGE_SECURITY) {
        dwNotifyFilter |= FILE_NOTIFY_CHANGE_SECURITY;
    }

    //
    // Tell the FS we want to use CeGetFileNotificationInfo later
    //dwNotifyFilter |= FILE_NOTIFY_CHANGE_CEGETINFO;


    TRACEMSG(ZONE_FILES, (L"SMBSRVR: Creating change notification for FID: %d on %s", pMyStream->FID(), pMyStream->GetFileName()));

    // if we havent set a change notification, or if the directory has changed, create a new handle
    hr = pMyStream->GetChangeNotification(&h, &pOldListen);
    if(SUCCEEDED(hr) && wcscmp(pOldListen, pMyStream->GetFileName())) {
        CloseHandle(h);
        hr = E_FAIL;
    }
    if(FAILED(hr)) {
         if(INVALID_HANDLE_VALUE == (h=FindFirstChangeNotification(pMyStream->GetFileName(), fWatchTree, dwNotifyFilter))) {
            TRACEMSG(ZONE_ERROR, (L"SMB_SERV: FindFirstChangeNotificaiton failed for %s", pMyStream->GetFileName()));
            goto Done;
        }
        TRACEMSG(ZONE_FILES, (L"SMB_SERV: Created Change notification on %s (handle:0x%x)", pMyStream->GetFileName(), h));
        pMyStream->SetChangeNotification(h, pMyStream->GetFileName());
    } else if(!FindNextChangeNotification(h)){
        TRACEMSG(ZONE_ERROR, (L"SMB_SERV: FindNextChangeNotificaiton failed for %s", pMyStream->GetFileName()));
        goto Done;
    }


    if(NULL == (pWakeUp = new ChangeNotificationWakeUpNode(pSMB,pMyConnection, h, uiMaxReturnBuffer))) {
        hr = E_OUTOFMEMORY;
        goto Done;
    }

    if(FAILED(hr=SMB_Globals::g_pWakeUpOnEvent->AddEvent(h, pWakeUp, &usNoteID))) {
        TRACEMSG(ZONE_ERROR, (L"SMB_SERV: Couldnt add change event for %s", pMyStream->GetFileName()));
        goto Done;
    }

    if(FAILED(hr=pMyConnection->AddWakeUpEvent(pWakeUp, usNoteID, pSMB->pInSMB->Uid, pSMB->pInSMB->Pid, pSMB->pInSMB->Tid, pSMB->pInSMB->Mid))) {
        TRACEMSG(ZONE_ERROR, (L"SMB_SERV: Couldnt add notification to connection for %s", pMyStream->GetFileName()));
        SMB_Globals::g_pWakeUpOnEvent->RemoveEvent(usNoteID);
        goto Done;
    }

    Done:

        if(FAILED(hr)) {
            //
            //  if there is a failure and the event isn't properly added but is allocated
            //  we want to unallocate the event to make sure no memory is leaked
            //
            if(NULL != pWakeUp) {
                pWakeUp->Release();
        pWakeUp = NULL;
            }

            if(INVALID_HANDLE_VALUE != h) {
                CloseHandle(h);
            }
        }
        return hr;
}



DWORD SMB_FILE::SMB_Com_NT_TRASACT(SMB_PROCESS_CMD *_pRawRequest, SMB_PROCESS_CMD *_pRawResponse, UINT *puiUsed, SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    StringTokenizer RequestTokenizer;


    SMB_COM_NT_TRANSACTION_CLIENT_REQUEST *pRequest =
            (SMB_COM_NT_TRANSACTION_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_NT_TRANSACTION_SERVER_RESPONSE *pResponse =
            (SMB_COM_NT_TRANSACTION_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    DWORD dwSize = 0;
    dwSize += sizeof(SMB_HEADER);
    dwSize += sizeof(SMB_COM_NT_TRANSACTION_CLIENT_REQUEST);
    dwSize += (sizeof(WORD)*pRequest->SetupCount);

    //
    // Verify that we have enough memory
    if(_pRawRequest->uiDataSize < sizeof(SMB_COM_NT_TRANSACTION_CLIENT_REQUEST) ||
       _pRawRequest->uiDataSize < sizeof(SMB_COM_NT_TRANSACTION_CLIENT_REQUEST) +  (sizeof(WORD)*pRequest->SetupCount)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_NT_TRASACT -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_COM_NT_TRANSACTION_SERVER_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_NT_TRASACT -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    BYTE *pSMBparam = (BYTE *)_pRawRequest->pSMBHeader + sizeof(SMB_COM_NT_TRANSACTION_CLIENT_REQUEST) + sizeof(SMB_HEADER);
    RequestTokenizer.Reset(pSMBparam, (sizeof(WORD)*pRequest->SetupCount));

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_LockX: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Parse out the function
    switch(pRequest->Function) {
        case NT_TRANS_NOTIFY_CHANGE:
            {
                ce::smart_ptr<TIDState> pTIDState;
                ce::smart_ptr<SMBFileStream> pMyStream;
                //
                // Make sure we have the proper struct
                if(4 != pRequest->SetupCount) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV: NT_TRANS_NOTIFY_CHANGE -- not a supported notify change struct!!"));
                    dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                    goto Done;
                }

                //
                // Find a share state
                if(FAILED(pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_READ)) || !pTIDState) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV: NT_TRANS_NOTIFY_CHANGE -- couldnt find tid state!!"));
                    dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                    goto Done;
                }

                //
                // Get the notification struct
                NT_NOTIFY_CHANGE_STRUCT *pNotify;
                if(FAILED(RequestTokenizer.GetByteArray((BYTE **)&pNotify, sizeof(NT_NOTIFY_CHANGE_STRUCT)))) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV: NT_TRANS_NOTIFY_CHANGE -- couldnt get notify struct!!"));
                    dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                    goto Done;
                }

                //
                // Find the FID
                if(FAILED(pTIDState->FindFileStream(pNotify->Fid, pMyStream))) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV: NT_TRANS_NOTIFY_CHANGE -- couldnt get FID: %d!!", pNotify->Fid));
                    dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                    goto Done;
                }

                //
                // Setup the notification
                if(FAILED(AddChangeNotification(pSMB, pMyStream, pNotify, pMyConnection, pRequest->MaxParameterCount))){
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV: NT_TRANS_NOTIFY_CHANGE -- couldnt set change notification FID: %d!!", pNotify->Fid));
                    dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                    goto Done;
                }

                dwRet = SMB_ERR(ERRInternal, PACKET_QUEUED);
                goto Done;
            }
            break;
        default:
            goto SendError;
    }

    goto Done;

    SendError:
        {
            SMB_COM_GENERIC_RESPONSE *pGenericResponse =
                    (SMB_COM_GENERIC_RESPONSE *)_pRawResponse->pDataPortion;

            //
            // Fill out return params and send back the data
            pGenericResponse->ByteCount = 0;
            pGenericResponse->WordCount = 0;

            *puiUsed = sizeof(SMB_COM_GENERIC_RESPONSE);
            dwRet = 0x00020001;
        }
    Done:
        return dwRet;
}


HRESULT RetrieveLock(SMB_LARGELOCK_RANGE *pOut, BOOL fUsingLargeLock, StringTokenizer &RequestTokenizer)
{
    HRESULT hr = E_FAIL;
    if(fUsingLargeLock) {
         SMB_LARGELOCK_RANGE *pRange;
         if(FAILED(RequestTokenizer.GetByteArray((BYTE **)&pRange, sizeof(SMB_LARGELOCK_RANGE)))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_LockX -- problem getting range from packet!!"));
            goto Done;
         }
         memcpy(pOut, pRange, sizeof(SMB_LARGELOCK_RANGE));
     } else {
        SMB_LOCK_RANGE *pRange;
         if(FAILED(RequestTokenizer.GetByteArray((BYTE **)&pRange, sizeof(SMB_LOCK_RANGE)))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_LockX -- problem getting range from packet!!"));
            ASSERT(FALSE);
            goto Done;
         }
         pOut->LengthHigh = 0;
         pOut->OffsetHigh = 0;
         pOut->LengthLow = pRange->Length;
         pOut->OffsetLow = pRange->Offset;
     }
     hr = S_OK;
     Done:
        return hr;
}

//
// From cifs9f.doc
DWORD SMB_FILE::SMB_Com_LockX(SMB_PROCESS_CMD *_pRawRequest,
                               SMB_PROCESS_CMD  *_pRawResponse,
                               UINT *puiUsed,
                               SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    ce::smart_ptr<TIDState> pTIDState = NULL;

    SMB_LOCKX_CLIENT_REQUEST *pRequest =
            (SMB_LOCKX_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_LOCKX_SERVER_RESPONSE *pResponse =
            (SMB_LOCKX_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    StringTokenizer RequestTokenizer;
    UINT uiCnt = 0;
    BOOL fUsingLargeLock = FALSE;
    SMB_LARGELOCK_RANGE RangeLock;
    HRESULT hr;

    //
    // Verify that we have enough memory
    if(_pRawRequest->uiDataSize < sizeof(SMB_LOCKX_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_LockX -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_LOCKX_SERVER_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_LockX -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_LockX: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Find a share state
    if(FAILED(pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_READ)) || !pTIDState) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_LockX -- couldnt find share state!!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // See if we are using the large locks (NT)
    if(pRequest->LockType & LARGE_FILE_LOCK) {
        fUsingLargeLock = TRUE;
    }

    //
    // Make sure none of the locked ranges are locked
    RequestTokenizer.Reset((BYTE *)(pRequest + 1), pRequest->ByteCount);
    for(uiCnt = 0; uiCnt < pRequest->NumLocks; uiCnt++) {
         BOOL fLocked;

         if(FAILED(RetrieveLock(&RangeLock, fUsingLargeLock, RequestTokenizer))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_LockX -- cant get lock from packet!!!"));
            dwRet = ERROR_CODE(STATUS_WAS_UNLOCKED);
            goto Done;
         }

         if(FAILED(pTIDState->IsLocked(pRequest->FileID, &RangeLock, &fLocked)) ||
            TRUE == fLocked) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_LockX -- range already locked!!!"));
            dwRet = ERROR_CODE(STATUS_WAS_UNLOCKED);
            goto Done;
         }
    }

    //
    // Make sure the unlock ranges are locked locked
    for(uiCnt = 0; uiCnt < pRequest->NumUnlocks; uiCnt++) {
         BOOL fLocked;

         if(FAILED(RetrieveLock(&RangeLock, fUsingLargeLock, RequestTokenizer))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_LockX -- cant get lock from packet!!!"));
            dwRet = ERROR_CODE(STATUS_NOT_LOCKED);
            goto Done;
         }

         if(FAILED(pTIDState->IsLocked(pRequest->FileID, &RangeLock, &fLocked)) ||
            FALSE == fLocked) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_LockX -- unlock requested on range not already locked: FID: %d!!!",pRequest->FileID));
            ASSERT(FALSE);
         }
    }


    //
    // If we got here, perform the actual range locks
    RequestTokenizer.Reset((BYTE *)(pRequest + 1), pRequest->ByteCount);
    for(uiCnt = 0; uiCnt < pRequest->NumLocks; uiCnt ++) {

         if(FAILED(RetrieveLock(&RangeLock, fUsingLargeLock, RequestTokenizer))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_LockX -- cant get lock from packet!!!"));
            dwRet = ERROR_CODE(STATUS_LOCK_NOT_GRANTED);
            goto Done;
         }

         if(FAILED(hr = pTIDState->Lock(pRequest->FileID, &RangeLock))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_LockX -- cant lock range!!!"));
            dwRet = ConvertHRToError(hr, pSMB);
            goto Done;
         }
    }

    //
    // Also do any unlocks
    for(uiCnt = 0; uiCnt < pRequest->NumUnlocks; uiCnt ++) {

         if(FAILED(RetrieveLock(&RangeLock, fUsingLargeLock, RequestTokenizer))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_LockX -- cant get lock from packet!!!"));
            dwRet = ERROR_CODE(STATUS_LOCK_NOT_GRANTED);
            goto Done;
         }
         if(FAILED(pTIDState->UnLock(pRequest->FileID, &RangeLock))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_LockX -- can't unlock range!!!"));
            ASSERT(FALSE); //<--- we SHOULDNT be here unless something bad happened (b/c we verified above)
         }
    }

    //
    // Finally if they were requesting a oplock break, do that
    if(pRequest->LockType & BREAK_OPLOCK) {
        if(FAILED(pTIDState->BreakOpLock(pRequest->FileID))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_LockX cant break oplock for FID:%d", pRequest->FileID));
            ASSERT(FALSE);
        } else {
            // Dont send back a response on this one
            pSMB->uiPacketType = SMB_DISCARD_PACKET;
            dwRet = SMB_ERR(ERRInternal, PACKET_NORESPONSE);
        }
    }

    Done:
        //
        // Fill out return params and send back the data
        pResponse->ByteCount = 0;
        pResponse->ANDX.AndXCommand = 0xFF; //assume we are the last command
        pResponse->ANDX.AndXReserved = 0;
        pResponse->ANDX.AndXOffset = 0;
        pResponse->ANDX.WordCount = 2; 


        *puiUsed = sizeof(SMB_LOCKX_SERVER_RESPONSE);
        return dwRet;
}



//
//  from SMBHLP.zip
DWORD SMB_FILE::SMB_Com_MakeDirectory(SMB_PROCESS_CMD *_pRawRequest,
                                     SMB_PROCESS_CMD *_pRawResponse,
                                     UINT *puiUsed,
                                     SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    SMB_CREATE_DIRECTORY_CLIENT_REQUEST *pRequest =
            (SMB_CREATE_DIRECTORY_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_GENERIC_RESPONSE *pResponse =
            (SMB_COM_GENERIC_RESPONSE *)_pRawResponse->pDataPortion;
    StringTokenizer RequestTokenizer;
    BYTE *pSMBparam = NULL;
    ce::smart_ptr<TIDState> pTIDState = NULL;
    StringConverter FullPath;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    HRESULT hr;

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_MakeDirectory: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Find a share state

    if(FAILED(hr = pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_WRITE)) || !pTIDState) {
        if(E_ACCESSDENIED == hr) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_MakeDirectory -- access denied!!"));
            dwRet = ERROR_CODE(STATUS_NETWORK_ACCESS_DENIED);
            goto Done;
        } else {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- couldnt find share state!!"));
            dwRet = SMB_ERR(ERRSRV, ERRerror);
            goto Done;
        }
    }

    //
    // Verify that we have enough memory
    if(_pRawRequest->uiDataSize < sizeof(SMB_CREATE_DIRECTORY_CLIENT_REQUEST) + pRequest->ByteCount) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_COM_GENERIC_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // build a tokenizer to get the filename
    pSMBparam = (BYTE *)(pRequest + 1);
    RequestTokenizer.Reset(pSMBparam, pRequest->ByteCount);

    //
    // Build the full path name
    FullPath.append(pTIDState->GetShare()->GetDirectoryName());


    //
    // Get the directory name
    if(TRUE == pMyConnection->SupportsUnicode(pSMB->pInSMB)) {
        WCHAR *pDirectoryName = NULL;

        if(FAILED(RequestTokenizer.GetUnicodeString(&pDirectoryName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- couldnt find filename!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        FullPath.append(pDirectoryName);
    } else {
        CHAR *pDirectoryName = NULL;

        if(FAILED(RequestTokenizer.GetString(&pDirectoryName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- couldnt find filename!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Check that we have the right string type
        if(0x04 != *pDirectoryName) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER:  SMB_Com_MakeDirectory -- invalid string type!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Advance beyond the 0x04 header
        pDirectoryName ++;

        FullPath.append(pDirectoryName);
    }


    //
    // make sure the request doesnt have any unexpected characters
    if(FAILED(pTIDState->GetShare()->IsValidPath(FullPath.GetString()))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- error filename has invalid character!"));
        dwRet = ERROR_CODE(STATUS_OBJECT_PATH_NOT_FOUND);
        goto Done;
    }

    //
    // Actually create the directory
    TRACEMSG(ZONE_FILES, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- creating directory %s", FullPath.GetString()));
    if(0 == CreateDirectory(FullPath.GetString(), 0)) {
        dwRet = ConvertGLEToError(GetLastError(), pSMB);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- error CreateDirectory() on directory %s failed with %d!", FullPath.GetString(), GetLastError()));
        goto Done;
    }

    //
    // Success
    dwRet = 0;

    Done:
        pResponse->ByteCount = 0;
        pResponse->WordCount = 0;
        *puiUsed = sizeof(SMB_COM_GENERIC_RESPONSE);
        return dwRet;
}



//
//  from SMBHLP.zip
DWORD SMB_FILE::SMB_Com_DelDirectory(SMB_PROCESS_CMD *_pRawRequest,
                                     SMB_PROCESS_CMD *_pRawResponse,
                                     UINT *puiUsed,
                                     SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    SMB_DELETE_DIRECTORY_CLIENT_REQUEST *pRequest =
            (SMB_DELETE_DIRECTORY_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_GENERIC_RESPONSE *pResponse =
            (SMB_COM_GENERIC_RESPONSE *)_pRawResponse->pDataPortion;
    StringTokenizer RequestTokenizer;
    BYTE *pSMBparam = NULL;
    ce::smart_ptr<TIDState> pTIDState = NULL;
    StringConverter FullPath;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    HRESULT hr;

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Find a share state
    if(FAILED(hr = pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_WRITE)) || !pTIDState) {
        if(E_ACCESSDENIED == hr) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_MakeDirectory -- access denied!!"));
            dwRet = ERROR_CODE(STATUS_NETWORK_ACCESS_DENIED);
            goto Done;
        } else {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- couldnt find share state!!"));
            dwRet = SMB_ERR(ERRSRV, ERRerror);
            goto Done;
        }
    }


    //
    // Verify that we have enough memory
    if(_pRawRequest->uiDataSize < sizeof(SMB_DELETE_DIRECTORY_CLIENT_REQUEST) + pRequest->ByteCount) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_COM_GENERIC_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // build a tokenizer to get the filename
    pSMBparam = (BYTE *)(pRequest + 1);
    RequestTokenizer.Reset(pSMBparam, pRequest->ByteCount);

    //
    // Build the full path name
    FullPath.append(pTIDState->GetShare()->GetDirectoryName());

    //
    // Get the directory name
    if(TRUE == pMyConnection->SupportsUnicode(pSMB->pInSMB)) {
        WCHAR *pDirectoryName = NULL;

        if(FAILED(RequestTokenizer.GetUnicodeString(&pDirectoryName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- couldnt find filename!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        FullPath.append(pDirectoryName);
    } else {
        CHAR *pDirectoryName = NULL;

        if(FAILED(RequestTokenizer.GetString(&pDirectoryName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- couldnt find filename!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Check that we have the right string type
        if(0x04 != *pDirectoryName) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER:  SMB_Com_MakeDirectory -- invalid string type!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Advance beyond the 0x04 header
        pDirectoryName ++;

        FullPath.append(pDirectoryName);
    }

    //
    // make sure the request doesnt have any unexpected characters
    if(FAILED(pTIDState->GetShare()->IsValidPath(FullPath.GetString()))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- error filename has invalid character!"));
        dwRet = ERROR_CODE(STATUS_OBJECT_PATH_NOT_FOUND);
        goto Done;
    }

    //
    // Actually remove the directory
    if(0 == RemoveDirectory(FullPath.GetString())) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- error CreateDirectory() on directory %s failed with %d!", FullPath.GetString(), GetLastError()));
        dwRet = ConvertGLEToError(GetLastError(), pSMB);
        goto Done;
    }

    //
    // Success
    dwRet = 0;

    Done:
        pResponse->ByteCount = 0;
        pResponse->WordCount = 0;
        *puiUsed = sizeof(SMB_COM_GENERIC_RESPONSE);
        return dwRet;
}





DWORD SMB_FILE::SMB_Com_DeleteFile(SMB_PROCESS_CMD *_pRawRequest,
                                  SMB_PROCESS_CMD *_pRawResponse,
                                  UINT *puiUsed,
                                  SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    SMB_DELETE_FILE_CLIENT_REQUEST *pRequest =
            (SMB_DELETE_FILE_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_DELETE_FILE_SERVER_RESPONSE *pResponse =
            (SMB_DELETE_FILE_SERVER_RESPONSE *)_pRawResponse->pDataPortion;
    StringTokenizer RequestTokenizer;
    BYTE *pSMBparam = NULL;
    ce::smart_ptr<TIDState> pTIDState = NULL;
    StringConverter FullPath;
    ce::smart_ptr<ActiveConnection> pMyConnection;
    HRESULT hr;

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Find a share state
    if(FAILED(hr = pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_WRITE)) || !pTIDState) {
        if(E_ACCESSDENIED == hr) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_DeleteFile -- access denied!!"));
            dwRet = ERROR_CODE(STATUS_NETWORK_ACCESS_DENIED);
            goto Done;
        } else {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_DeleteFile -- couldnt find share state!!"));
            dwRet = SMB_ERR(ERRSRV, ERRerror);
            goto Done;
        }
    }

    //
    // Verify that we have enough memory
    if(_pRawRequest->uiDataSize < sizeof(SMB_DELETE_FILE_CLIENT_REQUEST) + pRequest->ByteCount) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_DeleteFile -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_DELETE_FILE_SERVER_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_DeleteFile -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // build a tokenizer to get the filename
    pSMBparam = (BYTE *)(pRequest + 1);
    RequestTokenizer.Reset(pSMBparam, pRequest->ByteCount);

    //
    // Build the full path name
    FullPath.append(pTIDState->GetShare()->GetDirectoryName());

    //
    // Get the directory name
    if(TRUE == pMyConnection->SupportsUnicode(pSMB->pInSMB)) {
       WCHAR *pDirectoryName = NULL;

       if(FAILED(RequestTokenizer.GetUnicodeString(&pDirectoryName))) {
           TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- couldnt find filename!!"));
           dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
           goto Done;
       }
       FullPath.append(pDirectoryName);
    } else {
       CHAR *pDirectoryName = NULL;

       if(FAILED(RequestTokenizer.GetString(&pDirectoryName))) {
           TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_MakeDirectory -- couldnt find filename!!"));
           dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
           goto Done;
       }

       //
       // Check that we have the right string type
       if(0x04 != *pDirectoryName) {
           TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER:  SMB_Com_MakeDirectory -- invalid string type!"));
           dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
           goto Done;
       }

       //
       // Advance beyond the 0x04 header
       pDirectoryName ++;

       FullPath.append(pDirectoryName);
    }

    //
    // make sure the request doesnt have any unexpected characters
    if(FAILED(pTIDState->GetShare()->IsValidPath(FullPath.GetString()))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_DeleteFile -- error filename has invalid character!"));
        dwRet = ERROR_CODE(STATUS_OBJECT_NAME_NOT_FOUND);
        goto Done;
    }

    //
    // Actually delete the file
    if(FAILED(SMB_Globals::g_pAbstractFileSystem->AFSDeleteFile(FullPath.GetString()))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_DeleteFile -- error DeleteFile() on filename %s failed with %d!", FullPath.GetString(), GetLastError()));
        dwRet = ConvertGLEToError(GetLastError(), pSMB);
        goto Done;
    }

    //
    // Success
    dwRet = 0;

    Done:
        pResponse->ByteCount = 0;
        pResponse->WordCount = 0;
        *puiUsed = sizeof(SMB_DELETE_FILE_SERVER_RESPONSE);
        return dwRet;
}






DWORD SMB_FILE::SMB_Com_RenameFile(SMB_PROCESS_CMD *_pRawRequest,
                                    SMB_PROCESS_CMD *_pRawResponse,
                                    UINT *puiUsed,
                                    SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    SMB_RENAME_FILE_CLIENT_REQUEST *pRequest =
            (SMB_RENAME_FILE_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_GENERIC_RESPONSE *pResponse =
            (SMB_COM_GENERIC_RESPONSE *)_pRawResponse->pDataPortion;
    StringTokenizer RequestTokenizer;
    BYTE *pSMBparam = NULL;
    ce::smart_ptr<TIDState> pTIDState = NULL;
    ce::smart_ptr<ActiveConnection> pMyConnection;
    StringConverter FullNewFile;
    StringConverter FullOldFile;
    HRESULT hr;

    //
    // Verify that we have enough memory
    if(_pRawRequest->uiDataSize < sizeof(SMB_RENAME_FILE_CLIENT_REQUEST) + pRequest->ByteCount) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_RenameFile -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_COM_GENERIC_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_RenameFile -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // build a tokenizer to get the filenames
    pSMBparam = (BYTE *)(pRequest + 1);
    RequestTokenizer.Reset(pSMBparam, pRequest->ByteCount);

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Find a share state
    if(FAILED(hr = pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState,SEC_WRITE)) || !pTIDState) {
        if(E_ACCESSDENIED == hr) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_RenameFile -- access denied!!"));
            dwRet = ERROR_CODE(STATUS_NETWORK_ACCESS_DENIED);
            goto Done;
        } else {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_RenameFile -- couldnt find share state!!"));
            dwRet = SMB_ERR(ERRSRV, ERRerror);
            goto Done;
        }
    }


    //
    // Build the full path name
    FullOldFile.append(pTIDState->GetShare()->GetDirectoryName());
    FullNewFile.append(pTIDState->GetShare()->GetDirectoryName());

    //
    // Get the old/new filename
    if(FALSE == pMyConnection->SupportsUnicode(pSMB->pInSMB)) {
        CHAR *pFileOld = NULL;
        CHAR *pFileNew = NULL;
        BYTE fileOldID, fileNewID;

        if(FAILED(RequestTokenizer.GetByte(&fileOldID)) || 0x04 != fileOldID) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_RenameFile -- improper string id on old filename!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        if(FAILED(RequestTokenizer.GetString(&pFileOld))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_RenameFile -- couldnt find old filename!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        if(FAILED(RequestTokenizer.GetByte(&fileNewID)) || 0x04 != fileNewID) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_RenameFile -- improper string id on new filename!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        if(FAILED(RequestTokenizer.GetString(&pFileNew))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_RenameFile -- couldnt find new filename!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        FullOldFile.append(pFileOld);
        FullNewFile.append(pFileNew);
    } else {
        WCHAR *pFileOld = NULL;
        WCHAR *pFileNew = NULL;
        BYTE fileOldID, fileNewID;
        if(FAILED(RequestTokenizer.GetByte(&fileOldID)) || 0x04 != fileOldID) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_RenameFile -- improper string id on old filename!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        if(FAILED(RequestTokenizer.GetUnicodeString(&pFileOld))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_RenameFile -- couldnt find old filename!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        if(FAILED(RequestTokenizer.GetByte(&fileNewID)) || 0x04 != fileNewID) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_RenameFile -- improper string id on new filename!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        if(FAILED(RequestTokenizer.GetUnicodeString(&pFileNew))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_RenameFile -- couldnt find new filename!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        FullOldFile.append(pFileOld);
        FullNewFile.append(pFileNew);
    }



    //
    // make sure the request doesnt have any unexpected characters
    if(FAILED(pTIDState->GetShare()->IsValidPath(FullOldFile.GetString()))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_RenameFile -- error old filename has invalid character!"));
        dwRet = ERROR_CODE(STATUS_OBJECT_NAME_NOT_FOUND);
        goto Done;
    }

    



    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Renamefile -- rename %s to %s", FullOldFile.GetString(), FullNewFile.GetString()));
    if(0 == wcscmp(FullOldFile.GetString(), FullNewFile.GetString())) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_RenameFile -- Files are the same... doing nothing %s->%s!", FullOldFile.GetString(), FullNewFile.GetString()));
    } else if(0 == MoveFile(FullOldFile.GetString(), FullNewFile.GetString())) {
           TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_RenameFile -- MoveFile() failed with %d on %s->%s!", GetLastError(), FullOldFile.GetString(), FullNewFile.GetString()));
           dwRet = ConvertGLEToError(GetLastError(), pSMB);
           goto Done;
    }

    //
    // Success
    dwRet = 0;
    Done:
        pResponse->ByteCount = 0;
        pResponse->WordCount = 0;
        *puiUsed = sizeof(SMB_COM_GENERIC_RESPONSE);
        return dwRet;
}



DWORD SMB_FILE::SMB_Com_TRANS2(SMB_PROCESS_CMD *_pRawRequest,
                                 SMB_PROCESS_CMD *_pRawResponse,
                                 UINT *puiUsed,
                                 SMB_PACKET *pSMB)
{
   DWORD dwRet = 0;
   BYTE *pSMBparam;
   SMB_COM_TRANSACTION2_CLIENT_REQUEST  *pRequest  = (SMB_COM_TRANSACTION2_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
   SMB_COM_TRANSACTION2_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION2_SERVER_RESPONSE *)_pRawResponse->pDataPortion;
   ASSERT(TRUE == g_fFileServer);

   WORD wAPI;
   StringTokenizer RequestTokenizer;
   BOOL fNotSupported = FALSE;

   //
   // Verify incoming parameters
   if(_pRawRequest->uiDataSize < sizeof(SMB_COM_TRANSACTION2_CLIENT_REQUEST) ||
       _pRawResponse->uiDataSize < sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE) ||
       _pRawRequest->uiDataSize + sizeof(SMB_HEADER) < pRequest->DataOffset ||
       _pRawRequest->uiDataSize < sizeof(SMB_HEADER) + pRequest->ParameterCount) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error -- transaction -- not enough memory"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
   }
   if(pRequest->WordCount != 15 || 1 != pRequest->SetupCount) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error -- word count on transaction has to be 15 and setupcount must be 1 -- error"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
   }

   //
   // Point the pSMBparam into the parameter section of the SMB
   pSMBparam = (BYTE *)_pRawRequest->pSMBHeader + pRequest->ParameterOffset;
   RequestTokenizer.Reset(pSMBparam, pRequest->ParameterCount);

   //
   // Get the API
   wAPI = pRequest->Setup;

   {
        switch(wAPI) {
             case TRANS2_QUERY_PATH_INFO:         //0x05
                dwRet = SMB_Trans2_Query_Path_Information(_pRawRequest->pSMBHeader->Tid,
                                              pRequest,
                                              &RequestTokenizer,
                                              _pRawResponse,
                                              pResponse,
                                              pSMB,
                                              puiUsed);
                break;
            case TRANS2_SET_FILE_INFORMATION:     //0x08
                dwRet = SMB_Trans2_Set_File_Information(_pRawRequest->pSMBHeader->Tid,
                                              pRequest,
                                              &RequestTokenizer,
                                              _pRawRequest,
                                              _pRawResponse,
                                              pResponse,
                                              pSMB,
                                              puiUsed);
                break;
            case TRANS2_QUERY_FILE_INFORMATION:   //0x07
                dwRet = SMB_Trans2_Query_File_Information(_pRawRequest->pSMBHeader->Tid,
                                              pRequest,
                                              &RequestTokenizer,
                                              _pRawResponse,
                                              pResponse,
                                              pSMB,
                                              puiUsed);
                break;
            case TRANS2_FIND_FIRST2:            //0x01
                dwRet = SMB_Trans2_Find_First(_pRawRequest->pSMBHeader->Tid,
                                              pRequest,
                                              &RequestTokenizer,
                                              _pRawResponse,
                                              pResponse,
                                              pSMB,
                                              puiUsed);
                break;
           case TRANS2_FIND_NEXT2:              //0x02
                dwRet = SMB_Trans2_Find_Next(_pRawRequest->pSMBHeader->Tid,
                                              pRequest,
                                              &RequestTokenizer,
                                              _pRawResponse,
                                              pResponse,
                                              pSMB,
                                              puiUsed);
                break;
            case TRANS2_QUERY_FS_INFORMATION:   //0x03
            {
                RAPIBuilder RAPI((BYTE *)(pResponse+1),
                                        _pRawResponse->uiDataSize-sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE),
                                        pRequest->MaxParameterCount,
                                        pRequest->MaxDataCount);

                dwRet = SMB_Trans2_Query_FS_Information(pRequest,
                                                        &RequestTokenizer,
                                                        &RAPI,
                                                        pResponse,
                                                        _pRawRequest->pSMBHeader->Tid,
                                                        pSMB);
                //
                // On error just return that error
                if(0 != dwRet)
                    goto Done;

                //fill out response SMB
                memset(pResponse, 0, sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE));

                //word count is 3 bytes (1=WordCount 2=ByteCount) less than the response
                pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE) - 3) / sizeof(WORD);
                pResponse->TotalParameterCount = RAPI.ParamBytesUsed();
                pResponse->TotalDataCount = RAPI.DataBytesUsed();
                pResponse->ParameterCount = RAPI.ParamBytesUsed();
                pResponse->ParameterOffset = RAPI.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
                pResponse->ParameterDisplacement = 0;
                pResponse->DataCount = RAPI.DataBytesUsed();
                pResponse->DataOffset = RAPI.DataOffset((BYTE *)_pRawResponse->pSMBHeader);
                pResponse->DataDisplacement = 0;
                pResponse->SetupCount = 0;
                pResponse->ByteCount = RAPI.TotalBytesUsed();

                ASSERT(10 == pResponse->WordCount);

                //set the number of bytes we used
                *puiUsed = RAPI.TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION2_SERVER_RESPONSE);
            }
            break;
            default:
                TRACEMSG(ZONE_ERROR, (L"SMB_SRV: TRANS2 API fn() unknown (%d)", wAPI));
                ASSERT(FALSE);
                break;
        }

     }
   Done:
       return dwRet;
}


DWORD SMB_FILE::SMB_Com_Query_Information(SMB_PROCESS_CMD *_pRawRequest,
                                         SMB_PROCESS_CMD *_pRawResponse,
                                         UINT *puiUsed,
                                         SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    *puiUsed = 0;
    HRESULT hr = S_OK;

    ce::smart_ptr<TIDState> pTIDState = NULL;
    SMB_COM_QUERY_INFO_REQUEST *pRequest = (SMB_COM_QUERY_INFO_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_QUERY_INFO_RESPONSE *pResponse = (SMB_COM_QUERY_INFO_RESPONSE *)_pRawResponse->pDataPortion;
    StringConverter FileString;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    StringTokenizer RequestTokenizer;
    BYTE *pSMBparam = NULL;

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Verify incoming params
    if(_pRawRequest->uiDataSize - sizeof(SMB_COM_QUERY_INFO_REQUEST) < (UINT)(pRequest->ByteCount - 1)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  COM_QUERY_INFO has a string larger than packet!! -- someone is tricking us?"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(0 != pRequest->WordCount) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  COM_QUERY_INFO -- unexpected word count!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Go get our TIDState information
    if(FAILED(hr = pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_READ)) || !pTIDState) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV:  COM_QUERY_INFO -- couldnt find Tid info for %d!", _pRawRequest->pSMBHeader->Tid));

        if(E_ACCESSDENIED == hr) {
            dwRet = ERROR_CODE(STATUS_ACCESS_DENIED);
            goto Done;
        } else {
            goto BadFile;
        }
    }
    if(FAILED(FileString.append(pTIDState->GetShare()->GetDirectoryName()))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  COM_QUERY_INFO -- putting path on filename FAILED!!"));
        goto BadFile;
    }
    //
    // build a tokenizer to get the filename
    pSMBparam = (BYTE *)(pRequest + 1);
    RequestTokenizer.Reset(pSMBparam, pRequest->ByteCount);

    if(FALSE == pMyConnection->SupportsUnicode(pSMB->pInSMB)) {
        CHAR *pFileName = (CHAR *)(pRequest + 1);
        if(FAILED(RequestTokenizer.GetString(&pFileName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV:  COM_QUERY_INFO -- getting filename failed!!"));
            goto BadFile;
        }

        if(0x04 != *pFileName) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV:  COM_QUERY_INFO -- invalid string type!"));
            ASSERT(FALSE);
            goto BadFile;
        }
        //
        // Advance beyond the string identifier
        pFileName ++;

        if(FAILED(FileString.append(pFileName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV:  COM_QUERY_INFO -- appending filename to path FAILED!!"));
            goto BadFile;
        }
    } else {
        WCHAR *pFileName = NULL;

        if(FAILED(RequestTokenizer.GetUnicodeString(&pFileName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV:  COM_QUERY_INFO -- getting filename failed!!"));
            goto BadFile;
        }

        if(FAILED(FileString.append(pFileName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV:  COM_QUERY_INFO -- appending filename to path FAILED!!"));
            goto BadFile;
        }
    }
    TRACEMSG(ZONE_FILES, (L"SMBSRV:  Checking attributes for file %s", FileString.GetString()));

    //
    // make sure the request doesnt have any unexpected characters
    if(FAILED(pTIDState->GetShare()->IsValidPath(FileString.GetString()))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: COM_QUERY_INFO -- error filename has invalid character!"));
        goto BadFile;
    }

    {
    WIN32_FIND_DATA fd;
    HANDLE hFileHand = FindFirstFile(FileString.GetString(), &fd);

    if(INVALID_HANDLE_VALUE == hFileHand) {
        TRACEMSG(ZONE_FILES, (L"SMBSRV:  COM_QUERY_INFO -- getting attributes for %s failed (%d)!!", FileString.GetString(), GetLastError()));
        goto BadFile;
    }
    FindClose(hFileHand);

    WORD attributes = Win32AttributeToDos(fd.dwFileAttributes);
    FILETIME UTCWriteTime;

    //WORDCOUNT = response struct - 3 ( for 'bytecount' and the actual word count byte) / size of a word
    pResponse->Fields.WordCount = (sizeof(SMB_COM_QUERY_INFO_RESPONSE)-3)/sizeof(WORD);
    pResponse->Fields.FileAttributes = attributes;

    if(0 == FileTimeToLocalFileTime(&fd.ftLastWriteTime, &UTCWriteTime)) {
        TRACEMSG(ZONE_FILES, (L"SMBSRV: COM_QUERY_INFO -- error converting file time to local time"));
        goto BadFile;
    }
    pResponse->Fields.LastModifyTime = SecSinceJan1970_0_0_0(&UTCWriteTime);
    pResponse->Fields.FileSize = fd.nFileSizeLow;
    pResponse->Fields.Unknown = 0;
    pResponse->Fields.Unknown2 = 0;
    pResponse->Fields.Unknown3 = 0;
    pResponse->ByteCount = 0;
    *puiUsed = sizeof(SMB_COM_QUERY_INFO_RESPONSE);
    }

    goto Done;
    BadFile:
        {
            BYTE *pTmp = (BYTE *)pResponse;
            DWORD dwErr = GetLastError();
            pTmp[0] = 0;
            pTmp[1] = 0;
            pTmp[2] = 0;
            *puiUsed = 3;
            if(ERROR_PATH_NOT_FOUND == dwErr) {
                dwRet = ERROR_CODE(STATUS_OBJECT_NAME_INVALID);
            } else {
                dwRet = ERROR_CODE(STATUS_NO_SUCH_FILE);
            }
        }

    Done:
        return dwRet;
}

DWORD SMB_FILE::SMB_Com_Query_Information_Disk(SMB_PROCESS_CMD *_pRawRequest,
                                         SMB_PROCESS_CMD *_pRawResponse,
                                         UINT *puiUsed,
                                         SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    *puiUsed = 0;

    ce::smart_ptr<TIDState> pTIDState = NULL;
    SMB_COM_QUERY_INFO_DISK_REQUEST *pRequest = (SMB_COM_QUERY_INFO_DISK_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_QUERY_INFO_DISK_RESPONSE *pResponse = (SMB_COM_QUERY_INFO_DISK_RESPONSE *)_pRawResponse->pDataPortion;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    ULARGE_INTEGER FreeToCaller;
    ULARGE_INTEGER NumberBytes;
    ULARGE_INTEGER TotalFree;

    BYTE *pSMBparam = NULL;

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_Query_Information_Disk: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Verify incoming params
    if(_pRawRequest->uiDataSize < sizeof(SMB_COM_QUERY_INFO_DISK_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_Query_Information_Disk has a string larger than packet!! -- someone is tricking us?"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(0 != pRequest->WordCount) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_Query_Information_Disk -- unexpected word count!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Go get our TIDState information
    if(FAILED(pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_READ)) || !pTIDState) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_Query_Information_Disk -- couldnt find Tid info for %d!", _pRawRequest->pSMBHeader->Tid));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Get the amount of free disk SPACEPARITY
    if(0 == GetDiskFreeSpaceEx(pTIDState->GetShare()->GetDirectoryName(),
                               &FreeToCaller,
                               &NumberBytes,
                               &TotalFree)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_Query_Information_Disk -- couldnt get free disk space!!"));
        goto Done;
    }

    //
    // Fill in parameters
    pResponse->WordCount = (sizeof(SMB_COM_QUERY_INFO_DISK_RESPONSE)-3)/sizeof(WORD);
    ASSERT(5 == pResponse->WordCount);
    pResponse->TotalUnits = 0;
    pResponse->BlocksPerUnit = 2048;
    pResponse->BlockSize = 512;
    ASSERT(NumberBytes.QuadPart / pResponse->BlocksPerUnit / pResponse->BlockSize <= 0xFFFF);

    //
    // Compute the number of units
    pResponse->TotalUnits = (USHORT)(NumberBytes.QuadPart / pResponse->BlocksPerUnit / pResponse->BlockSize);
    pResponse->FreeUnits = (USHORT)(TotalFree.QuadPart / pResponse->BlocksPerUnit / pResponse->BlockSize);
    pResponse->Reserved = 0;
    pResponse->ByteCount = 0;
    *puiUsed = sizeof(SMB_COM_QUERY_INFO_DISK_RESPONSE);


    Done:
        return dwRet;
}


DWORD SMB_FILE::SMB_Com_SetInformation(SMB_PROCESS_CMD *_pRawRequest,
                                      SMB_PROCESS_CMD *_pRawResponse,
                                      UINT *puiUsed,
                                      SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    *puiUsed = 0;

    ce::smart_ptr<TIDState> pTIDState = NULL;
    SMB_SET_ATTRIBUTE_CLIENT_REQUEST *pRequest = (SMB_SET_ATTRIBUTE_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_GENERIC_RESPONSE *pResponse = (SMB_COM_GENERIC_RESPONSE *)_pRawResponse->pDataPortion;

    FILETIME WriteTime;
    FILETIME *pWriteTime = &WriteTime;
    WORD     Win32Attributes  = 0;
    StringConverter FileName;
    HANDLE h = INVALID_HANDLE_VALUE;
    BOOL fIsDir = FALSE;
    BYTE *pSMBparam = NULL;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    StringTokenizer RequestTokenizer;
    HRESULT hr;

    //
    // Verify incoming params
    if(_pRawRequest->uiDataSize < sizeof(SMB_SET_ATTRIBUTE_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_SetInformation has a string larger than packet!! -- someone is tricking us?"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(8 != pRequest->WordCount) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_SetInformation -- unexpected word count!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_SetInformation: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Go get our TIDState information
    if(FAILED(hr = pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_WRITE)) || !pTIDState) {
        if(E_ACCESSDENIED == hr) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_SetInformation -- access denied!!"));
            dwRet = ERROR_CODE(STATUS_NETWORK_ACCESS_DENIED);
            goto Done;
        } else {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_SetInformation -- couldnt find Tid info for %d!", _pRawRequest->pSMBHeader->Tid));
            dwRet = SMB_ERR(ERRSRV, ERRerror);
            goto Done;
        }
    }
    //
    // build a tokenizer to get the filename
    pSMBparam = (BYTE *)(pRequest + 1);
    RequestTokenizer.Reset(pSMBparam, pRequest->ByteCount);

    //
    // See what we really need to set
    if(0 == pRequest->ModifyDate && 0 == pRequest->ModifyTime) {
        pWriteTime = NULL;
    } else if(0 == DosDateTimeToFileTime(pWriteTime, pRequest->ModifyDate, pRequest->ModifyTime)) {
        pWriteTime = NULL;
    }

    //
    // Parse out the file attributes
    if(!(Win32Attributes = DosAttributeToWin32(pRequest->FileAttributes))) {
        Win32Attributes = FILE_ATTRIBUTE_NORMAL;
    }


    //
    // Add the Dir+FileName
    FileName.append(pTIDState->GetShare()->GetDirectoryName());


    if(TRUE == pMyConnection->SupportsUnicode(pSMB->pInSMB)) {
        WCHAR *pFileName = NULL;

        if(FAILED(RequestTokenizer.GetUnicodeString(&pFileName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_SetInformation -- string is of wrong time!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            ASSERT(FALSE);
            goto Done;
        }
        FileName.append(pFileName);

    } else {
        CHAR *pFileName = NULL;

        if(FAILED(RequestTokenizer.GetString(&pFileName)) || 0x04 != *pFileName) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_SetInformation -- string is of wrong time!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            ASSERT(FALSE);
            goto Done;
        }
        FileName.append(pFileName+1);
    }

    //
    // Check to see if the string is a file or directory
    if(FAILED(IsDirectory(FileName.GetString(), &fIsDir))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_SetInformation -- file doesnt exit GetLastError(%d)!", FileName.GetString(), GetLastError()));
        dwRet = ERROR_CODE(STATUS_OBJECT_PATH_NOT_FOUND);
        goto Done;
    }

    //
    // Setting all the information is a two part process first open the file
    //    and SetFileTime, then close and do a SetFileAttributes
    if(FALSE == fIsDir && NULL != pWriteTime) {
        if(INVALID_HANDLE_VALUE == (h = CreateFile(FileName.GetString(),
                                                   GENERIC_WRITE,
                                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                   NULL,
                                                   OPEN_EXISTING,
                                                   FILE_ATTRIBUTE_NORMAL,
                                                   NULL))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_SetInformation -- couldnt open up %s GetLastError(%d)!", FileName.GetString(), GetLastError()));
            dwRet = ERROR_CODE(STATUS_OBJECT_PATH_NOT_FOUND);
            goto Done;
        }

        //
        // Set the file time
        FILETIME UTCft;
        if(0 == ::FileTimeToLocalFileTime(pWriteTime, &UTCft)) {
               TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_SetInformation -- couldnt SetFileTime for %s! GetLastError(%d)", FileName.GetString(), GetLastError()));
               ASSERT(FALSE);
               dwRet = ConvertGLEToError(GetLastError(), pSMB);
               goto Done;
        }
        if(0 == ::SetFileTime(h, NULL, NULL, &UTCft)) {
               TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_SetInformation -- couldnt SetFileTime for %s! GetLastError(%d)", FileName.GetString(), GetLastError()));
               dwRet = ConvertGLEToError(GetLastError(), pSMB);
               goto Done;
        }

        //
        // Close up
        CloseHandle(h);
        h = INVALID_HANDLE_VALUE;
    }
    ASSERT(INVALID_HANDLE_VALUE == h);

    //
    // Finally do the SetFileAttributes
    ASSERT(Win32Attributes);
    if(0 == SetFileAttributes(FileName.GetString(), Win32Attributes)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_SetInformation -- couldnt SetFileAttributes for %s! GetLastError(%d)", FileName.GetString(),GetLastError()));
        dwRet = ConvertGLEToError(GetLastError(), pSMB);
        goto Done;
    }

    //
    // Success
    dwRet = 0;
    Done:
        if(INVALID_HANDLE_VALUE != h) {
            CloseHandle(h);
        }

        pResponse->WordCount = 0;
        pResponse->ByteCount = 0;
        *puiUsed = sizeof(SMB_COM_GENERIC_RESPONSE);
        return dwRet;
}


DWORD SMB_FILE::SMB_Com_SetInformation2(SMB_PROCESS_CMD *_pRawRequest,
                                      SMB_PROCESS_CMD *_pRawResponse,
                                      UINT *puiUsed,
                                      SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    HRESULT hr;
    *puiUsed = 0;

    ce::smart_ptr<TIDState> pTIDState = NULL;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    SMB_SET_EXTENDED_ATTRIBUTE_CLIENT_REQUEST *pRequest = (SMB_SET_EXTENDED_ATTRIBUTE_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_GENERIC_RESPONSE *pResponse = (SMB_COM_GENERIC_RESPONSE *)_pRawResponse->pDataPortion;

    FILETIME CreationTime;
    FILETIME AccessTime;
    FILETIME WriteTime;
    FILETIME UTCCreationTime;
    FILETIME UTCAccessTime;
    FILETIME UTCWriteTime;
    FILETIME *pCreationTime = &UTCCreationTime;
    FILETIME *pAccessTime =  &UTCAccessTime;
    FILETIME *pWriteTime = &UTCWriteTime;

    //
    // Verify incoming params
    if(_pRawRequest->uiDataSize < sizeof(SMB_SET_EXTENDED_ATTRIBUTE_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_SetInformation2 has a string larger than packet!! -- someone is tricking us?"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(7 != pRequest->WordCount) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_SetInformation2 -- unexpected word count!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Go get our TIDState information
    if(FAILED(hr = pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_WRITE)) || !pTIDState) {
        if(E_ACCESSDENIED == hr) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_SetInformation -- access denied!!"));
            dwRet = ERROR_CODE(STATUS_NETWORK_ACCESS_DENIED);
            goto Done;
        } else {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_SetInformation2 -- couldnt find Tid info for %d!", _pRawRequest->pSMBHeader->Tid));
            dwRet = SMB_ERR(ERRSRV, ERRerror);
            goto Done;
        }
    }

    //
    // See what we really need to set, and do localtime to filetime conversions
    if(0 == pRequest->CreationDate && 0 == pRequest->CreationTime) {
        pCreationTime = NULL;
    } else {
        if(0 == DosDateTimeToFileTime(&CreationTime, pRequest->CreationDate, pRequest->CreationTime)) {
            pCreationTime = NULL;
        }else if(0 == LocalFileTimeToFileTime(&CreationTime, pCreationTime)) {
            pCreationTime = NULL;
        }
    }
    if(0 == pRequest->AccessDate && 0 == pRequest->AccessTime) {
        pAccessTime = NULL;
    } else {
        if(0 == DosDateTimeToFileTime(&AccessTime, pRequest->AccessDate, pRequest->AccessTime)) {
            pAccessTime = NULL;
        } else if(0 == LocalFileTimeToFileTime(&AccessTime, pAccessTime)) {
            pAccessTime = NULL;
        }
    }
    if(0 == pRequest->ModifyDate && 0 == pRequest->ModifyTime) {
        pWriteTime = NULL;
    } else {
        if(0 == DosDateTimeToFileTime(&WriteTime, pRequest->ModifyDate, pRequest->ModifyTime)) {
            pWriteTime = NULL;
        } else if(0 == LocalFileTimeToFileTime(&WriteTime, pWriteTime)) {
            pWriteTime = NULL;
        }
    }

    //
    // Set the file attributes
    if(FAILED(hr = pTIDState->SetFileTime(pRequest->FileID, pCreationTime, pAccessTime, pWriteTime))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_SetInformation2 -- couldnt set file time!!"));
        dwRet = ConvertHRToError(hr, pSMB);
        goto Done;
    }

    Done:
        pResponse->WordCount = 0;
        pResponse->ByteCount = 0;
        *puiUsed = sizeof(SMB_COM_GENERIC_RESPONSE);
        return dwRet;
}



DWORD
SMB_FILE::SMB_Com_Flush(SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    HRESULT hr;
    *puiUsed = 0;


    ce::smart_ptr<TIDState> pTIDState = NULL;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    SMB_FILE_FLUSH_CLIENT_REQUEST *pRequest = (SMB_FILE_FLUSH_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_GENERIC_RESPONSE *pResponse = (SMB_COM_GENERIC_RESPONSE *)_pRawResponse->pDataPortion;

    //
    // Verify incoming params
    if(_pRawRequest->uiDataSize < sizeof(SMB_FILE_FLUSH_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_Flush has a string larger than packet!! -- someone is tricking us?"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(1 != pRequest->WordCount) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_Flush -- unexpected word count!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }


    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Go get our TIDState information
    if(FAILED(pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_READ)) || !pTIDState) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_Flush -- couldnt find Tid info for %d!", _pRawRequest->pSMBHeader->Tid));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Perform the operation
    if(FAILED(hr = pTIDState->Flush(pRequest->FileID))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_Flush -- Flush() on TID:%d failed!", pRequest->FileID));
        dwRet = ConvertHRToError(hr, pSMB);
        goto Done;
    }

    Done:
        pResponse->WordCount = 0;
        pResponse->ByteCount = 0;
        *puiUsed = sizeof(SMB_COM_GENERIC_RESPONSE);
        return dwRet;
}


DWORD
SMB_FILE::SMB_Com_CheckPath(SMB_PROCESS_CMD *_pRawRequest,
                                     SMB_PROCESS_CMD *_pRawResponse,
                                     UINT *puiUsed,
                                     SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    *puiUsed = 0;

    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    SMB_CHECKPATH_CLIENT_REQUEST *pRequest = (SMB_CHECKPATH_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_CHECKPATH_SERVER_RESPONSE *pResponse = (SMB_CHECKPATH_SERVER_RESPONSE *)_pRawResponse->pDataPortion;
    ce::smart_ptr<TIDState> pTIDState = NULL;
    ULONG ulOffset = 0;
    Share *pMyShare = NULL;
    StringConverter FullPath;
    StringTokenizer RequestTokenizer;
    BYTE *pSMBparam = NULL;

    //
    // Verify incoming params
    if(_pRawRequest->uiDataSize < sizeof(SMB_CHECKPATH_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_CheckPath has a string larger than packet!! -- someone is tricking us?"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(0 != pRequest->WordCount) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_CheckPath -- unexpected word count!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_CheckPath: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Go get our TIDState information
    if(FAILED(pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_READ)) || !pTIDState) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_CheckPath -- couldnt find Tid info for %d!", _pRawRequest->pSMBHeader->Tid));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Get the share state
    if(NULL == (pMyShare = pTIDState->GetShare())) {
        ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_CheckPath -- couldnt find share state info for %d!", _pRawRequest->pSMBHeader->Tid));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Get the directory they are interested in
    pSMBparam = (BYTE *)(pRequest + 1);
    RequestTokenizer.Reset(pSMBparam, pRequest->ByteCount);
    FullPath.append(pMyShare->GetDirectoryName());
    if(TRUE == pMyConnection->SupportsUnicode(pSMB->pInSMB)) {
        WCHAR *pDirectoryName = NULL;

        if(FAILED(RequestTokenizer.GetUnicodeString(&pDirectoryName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_CheckPath -- couldnt find filename!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        FullPath.append(pDirectoryName);
    } else {
        CHAR *pDirectoryName = NULL;

        if(FAILED(RequestTokenizer.GetString(&pDirectoryName))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_CheckPath -- couldnt find filename!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Check that we have the right string type
        if(0x04 != *pDirectoryName) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER:  SMB_Com_CheckPath -- invalid string type!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Advance beyond the 0x04 header
        pDirectoryName ++;

        FullPath.append(pDirectoryName);
    }

    //
    // See if it exists (and is safe with the share)
    if(FAILED(pMyShare->IsValidPath(FullPath.GetString()))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER:  SMB_Com_CheckPath -- invalid string %s -- not safe!", FullPath.GetString()));
        dwRet = ERROR_CODE(STATUS_OBJECT_NAME_INVALID);
        goto Done;
    }

    BOOL fStatus;
    if(FAILED(IsDirectory(FullPath.GetString(), &fStatus)) || FALSE == fStatus) {
        dwRet = ERROR_CODE(STATUS_OBJECT_NAME_INVALID);
        goto Done;
    }

    //
    // Success
    dwRet = 0;

    Done:
        *puiUsed = sizeof(SMB_CHECKPATH_SERVER_RESPONSE);
        return dwRet;
}


DWORD
SMB_FILE::SMB_Com_Search(SMB_PROCESS_CMD *_pRawRequest,
                                 SMB_PROCESS_CMD *_pRawResponse,
                                 UINT *puiUsed,
                                 SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    *puiUsed = 0;

    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    SMB_COM_SEARCH_RESPONSE *pResponse = (SMB_COM_SEARCH_RESPONSE *)_pRawResponse->pDataPortion;

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_Search: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }
    TRACEMSG(ZONE_ERROR, (L"SMB_SRV:  SEARCH not supported b/c WinCE doesnt have 8.3 fileformat! sending no files back!"));
    dwRet = 0;
    *puiUsed = sizeof(SMB_COM_SEARCH_RESPONSE);

    pResponse->WordCount = 1;
    pResponse->Count = 0;
    pResponse->ByteCount = 3;
    pResponse->BufferFormat = 0x05;
    pResponse->DataLength = 0;
    Done:
        return dwRet;
}

DWORD
SMB_FILE::SMB_Com_Seek(SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    HRESULT hr;
    *puiUsed = 0;

    ce::smart_ptr<TIDState> pTIDState = NULL;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    SMB_SEEK_CLIENT_REQUEST *pRequest = (SMB_SEEK_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_SEEK_SERVER_RESPONSE *pResponse = (SMB_SEEK_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    ULONG ulOffset = 0;
    //
    // Verify incoming params
    if(_pRawRequest->uiDataSize < sizeof(SMB_SEEK_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_Seek has a string larger than packet!! -- someone is tricking us?"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(4 != pRequest->WordCount) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_Seek -- unexpected word count!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_Seek: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Go get our TIDState information
    if(FAILED(pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_READ)) || !pTIDState) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_Seek -- couldnt find Tid info for %d!", _pRawRequest->pSMBHeader->Tid));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Perform the operation
    if(FAILED(hr = pTIDState->SetFilePointer(pRequest->FileID, pRequest->Offset, pRequest->Mode, &ulOffset))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_Seek -- Flush() on TID:%d failed!", pRequest->FileID));
        dwRet = ConvertHRToError(hr, pSMB);
        goto Done;
    }

    //
    // SUCCESS
    dwRet = 0;

    Done:
        pResponse->WordCount = (sizeof(SMB_SEEK_SERVER_RESPONSE) - 3) / sizeof(WORD);
        pResponse->Offset = ulOffset;
        pResponse->ByteCount = 0;
        *puiUsed = sizeof(SMB_SEEK_SERVER_RESPONSE);
        ASSERT(2 == pResponse->WordCount);
        return dwRet;
}


DWORD SMB_FILE::SMB_Com_Query_EX_Information(SMB_PACKET *pSMB,
                                            SMB_PROCESS_CMD *_pRawRequest,
                                            SMB_PROCESS_CMD *_pRawResponse,
                                            UINT *puiUsed)
{
    DWORD dwRet = 0;
    *puiUsed = 0;

    ce::smart_ptr<TIDState> pTIDState = NULL;
    SMB_COM_QUERY_EXINFO_REQUEST *pRequest = (SMB_COM_QUERY_EXINFO_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_QUERY_EXINFO_RESPONSE *pResponse = (SMB_COM_QUERY_EXINFO_RESPONSE *)_pRawResponse->pDataPortion;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    HRESULT hr = E_FAIL;
    WIN32_FIND_DATA fd;
    WORD attributes;

    //
    // Verify incoming params
    if(_pRawRequest->uiDataSize < sizeof(SMB_COM_QUERY_EXINFO_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  COM_QUERY_EX_INFO has a string larger than packet!! -- someone is tricking us?"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(1 != pRequest->WordCount) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  COM_QUERY_EX_INFO -- unexpected word count!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Com_Seek: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Go get our TIDState information
    if(FAILED(pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_READ)) || !pTIDState) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  SMB_Com_Seek -- couldnt find Tid info for %d!", _pRawRequest->pSMBHeader->Tid));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Seek out the FID
    if(FAILED(hr = pTIDState->QueryFileInformation(pRequest->FID, &fd))) {
        TRACEMSG(ZONE_FILES, (L"SMBSRV:  COM_QUERY_EX_INFO: couldnt QueryFileInformation with FID: 0x%x \n", pRequest->FID));
        dwRet = ConvertHRToError(GetLastError(), pSMB);
        goto BadFile;
    }

    attributes = Win32AttributeToDos(fd.dwFileAttributes);

    FILETIME UTCCreationTime;
    FILETIME UTCWriteTime;
    FILETIME UTCAccessTime;

    if(0 == FileTimeToLocalFileTime(&fd.ftCreationTime, &UTCCreationTime) ||
       0 == FileTimeToLocalFileTime(&fd.ftLastWriteTime, &UTCWriteTime) ||
       0 == FileTimeToLocalFileTime(&fd.ftLastAccessTime, &UTCAccessTime))
    {
        ASSERT(FALSE);
        dwRet = ERROR_CODE(STATUS_NO_SUCH_FILE);
        goto BadFile;
    }
    if(FAILED(FileTimeToSMBTime(&UTCCreationTime,  &pResponse->Fields.CreateTime, &pResponse->Fields.CreateDate)) ||
       FAILED(FileTimeToSMBTime(&UTCWriteTime,     &pResponse->Fields.ModifyTime, &pResponse->Fields.ModifyDate)) ||
       FAILED(FileTimeToSMBTime(&UTCAccessTime,    &pResponse->Fields.AccessTime, &pResponse->Fields.AccessDate))) {
        ASSERT(FALSE);
        dwRet = ERROR_CODE(STATUS_NO_SUCH_FILE);
        goto BadFile;
    }


    //WORDCOUNT = response struct - 3 ( for 'bytecount' and the actual word count byte) / size of a word
    pResponse->Fields.WordCount = (sizeof(SMB_COM_QUERY_EXINFO_RESPONSE)-3)/sizeof(WORD);
    pResponse->Fields.FileSize = fd.nFileSizeLow;
    pResponse->Fields.Allocation = 0;
    pResponse->Fields.Attributes = attributes;
    pResponse->ByteCount = 0;
    *puiUsed = sizeof(SMB_COM_QUERY_EXINFO_RESPONSE);

    goto Done;
    BadFile:
        {
            BYTE *pTmp = (BYTE *)pResponse;
            pTmp[0] = 0;
            pTmp[1] = 0;
            pTmp[2] = 0;
            *puiUsed = 3;
        }

    Done:
        return dwRet;
}




FileStream::FileStream(TIDState *pMyState, USHORT usFID) : SMBFileStream(FILE_STREAM, pMyState)
{
    m_hChangeNotification = INVALID_HANDLE_VALUE;

    //
    // get a Unique ID and set it
    ASSERT(usFID != 0xFFFF);
    ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    this->SetFID(usFID);
    m_fOpened = FALSE;
}

FileStream::~FileStream()
{
    ASSERT(0xFFFF != this->FID());
    ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);

    //
    // If our file hasnt been closed, close it now
    if(m_fOpened) {
        TRACEMSG(ZONE_FILES, (L"SMB-FILESTREAM: Closing up file because the TID is going away and Close() wasnt called!"));
        this->Close();
    }

    if(INVALID_HANDLE_VALUE != m_hChangeNotification) {
        FindCloseChangeNotification(m_hChangeNotification);
        m_hChangeNotification = INVALID_HANDLE_VALUE;
    }
}

BOOL
FileStream::CanRead() {
    return (m_dwAccess & GENERIC_READ)?TRUE:FALSE;
}

BOOL
FileStream::CanWrite() {
    return (m_dwAccess & GENERIC_WRITE)?TRUE:FALSE;
}

HRESULT
FileStream::Read(BYTE *pDest, DWORD dwOffset, DWORD dwReqSize, DWORD *pdwRead)
{
    PREFAST_ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    ce::smart_ptr<FileObject> pObject = NULL;
    HRESULT hr = E_FAIL;

    if(FALSE == CanRead()) {
        return E_ACCESSDENIED;
    }
    if(FAILED(SMB_Globals::g_pAbstractFileSystem->FindFile(this->FID(), pObject))) {
        TRACEMSG(ZONE_FILES, (L"SMB-FILESTREAM: couldnt find file with FID:0x%x on read\n", this->FID()));
        goto Done;
    }
    if(!pObject || FAILED(hr = pObject->Read(this->FID(), pDest, dwOffset, dwReqSize, pdwRead))) {
        TRACEMSG(ZONE_FILES, (L"SMB-FILESTREAM: couldnt Read() to  file with FID:0x%x\n", this->FID()));
        goto Done;
    }

    hr = S_OK;

    //
    // Update our current offset
    m_dwCurrentOffset = dwOffset + *pdwRead;
    Done:
        return hr;
}

HRESULT
FileStream::Write(BYTE *pSrc, DWORD dwOffset, DWORD dwReqSize, DWORD *pdwWritten)
{
    PREFAST_ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    ce::smart_ptr<FileObject> pObject = NULL;
    HRESULT hr = E_FAIL;

    if(FALSE == CanWrite()) {
        return E_ACCESSDENIED;
    }
    if(FAILED(SMB_Globals::g_pAbstractFileSystem->FindFile(this->FID(), pObject))) {
        TRACEMSG(ZONE_FILES, (L"SMB-FILESTREAM: couldnt find file with FID:0x%x\n", this->FID()));
        goto Done;
    }
    if(!pObject || FAILED(hr = pObject->Write(this->FID(), pSrc, dwOffset, dwReqSize, pdwWritten))) {
        TRACEMSG(ZONE_FILES, (L"SMB-FILESTREAM: couldnt Write() to  file with FID:0x%x\n", this->FID()));
        goto Done;
    }

    hr = S_OK;

    //
    // Update our current offset
    m_dwCurrentOffset = dwOffset + *pdwWritten;
    Done:
        return hr;
}


HRESULT
FileStream::Open(const WCHAR *pFileName,
                 DWORD dwAccess,
                 DWORD dwDisposition,
                 DWORD dwAttributes,
                 DWORD dwShareMode,
                 DWORD *pdwActionTaken,
                 SMBFile_OpLock_PassStruct *pdwOpLock)
{
    HRESULT hr = E_FAIL;
    SMBPrintQueue *pPrintQueue = NULL;
    StringConverter FullPath;
    TIDState *pTIDState;
    BOOL fIsDirectory = FALSE;
    BOOL fFileExists = FALSE;

    //
    // Make sure this stream hasnt already been opened
    if(m_fOpened) {
        ASSERT(FALSE);
        hr = E_UNEXPECTED;
        goto Done;
    }

    //
    // Get our TIDState so we can start opening the file
    if(NULL == (pTIDState = this->GetTIDState())) {
        ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_OpenX -- cant get TID STATE!!!!"));
        hr = E_UNEXPECTED;
        goto Done;
    }


//    #error handle case where these fail!
    FullPath.append(pTIDState->GetShare()->GetDirectoryName());
    FullPath.append(pFileName);

    //
    // If there is a trailing \ remove it
    //   NOTE: that we will switch out buffers to prevent mods to the calling buffer
    if('\\' == pFileName[wcslen(pFileName)-1]) {
        WCHAR *pTemp;
        pTemp = FullPath.GetUnsafeString();
        pTemp[wcslen(pTemp)-1] = NULL;
    }

    TRACEMSG(ZONE_FILES, (L"SMBSRV-FILE: OpenX for %s", FullPath.GetString()));

    //
    // make sure the request doesnt have any unexpected characters
    if(FAILED(pTIDState->GetShare()->IsValidPath(FullPath.GetString()))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_OpenX -- error filename has invalid character!"));
        hr = E_ACCESSDENIED;
        goto Done;
    }

    //
    // Opening a directory has certain rules,  there are attributes
    //  that say if we are allowed to open a file as a directory or not
    //  honor those here
    if(FAILED(hr = IsDirectory(FullPath.GetString(), &fIsDirectory, &fFileExists))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_OpenX -- error cant check file as directory!! %s", FullPath.GetString()));
        ASSERT(FALSE);
        hr = E_ACCESSDENIED;
        goto Done;
    }
    if(fIsDirectory && (dwAttributes & FILE_ATTRIBUTE_CANT_BE_DIR)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_OpenX -- cant open because the file is a directory!! %s", FullPath.GetString()));
        hr = HRESULT_FROM_WIN32(ERROR_DIRECTORY);
        goto Done;
    } else if(!fIsDirectory && fFileExists && (dwAttributes & FILE_ATTRIBUTE_MUST_BE_DIR)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_OpenX -- cant open because the file is a directory!! %s", FullPath.GetString()));
        hr = HRESULT_FROM_WIN32(ERROR_FILE_EXISTS);
        goto Done;
    }

    //
    // Conditions to open up the file as a directory
    //    if it says directory from the call & we got through the FILE_ATTRIBUTE_CANT_BE_DIR flag
    //    if the FILE_ATTRIBUTE_MUST_BE_DIR is set
    if(fIsDirectory || (dwAttributes & FILE_ATTRIBUTE_MUST_BE_DIR)) {
        TRACEMSG(ZONE_FILES, (L"SMBSRV-CRACKER: SMB_OpenX -- opening up as directory!! %s", FullPath.GetString()));
        dwAttributes |= FILE_ATTRIBUTE_DIRECTORY;
    }


    //
    // Pull out attributes we've masked in that arent real
    dwAttributes = (dwAttributes & ~(FILE_ATTRIBUTE_MUST_BE_DIR|FILE_ATTRIBUTE_CANT_BE_DIR));

    //
    // Open up the file
    if(FAILED(hr = SMB_Globals::g_pAbstractFileSystem->Open(FullPath.GetString(),
                                                       this->FID(),
                                                       dwAccess,
                                                       dwDisposition,
                                                       dwAttributes,
                                                       dwShareMode,
                                                       pdwActionTaken,
                                                       pdwOpLock)))
    {
        TRACEMSG(ZONE_FILES, (L"SMBSRV-CRACKER: SMB_OpenX -- error -- cant open up from abstract file system!"));
        goto Done;
    }
    m_fOpened = TRUE;
    m_dwCurrentOffset = 0;
    m_dwAccess = dwAccess;
    hr = S_OK;
    Done:
        return hr;
}



HRESULT
FileStream::SetEndOfStream(DWORD dwOffset)
{
    PREFAST_ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    ce::smart_ptr<FileObject> pObject = NULL;
    HRESULT hr = E_FAIL;

    if(FALSE == CanWrite() && FALSE == CanRead()) {
        return E_ACCESSDENIED;
    }
    if(FAILED(SMB_Globals::g_pAbstractFileSystem->FindFile(this->FID(), pObject))) {
        TRACEMSG(ZONE_FILES, (L"SMBSRVR: FileStream -- couldnt find file with FID:0x%x", this->FID()));
        goto Done;
    }
    if(!pObject || FAILED(hr = pObject->SetEndOfStream(dwOffset))) {
        TRACEMSG(ZONE_FILES, (L"SMBSRVR: FileStream -- couldnt set endof stream for file with FID:0x%x", this->FID()));
        goto Done;
    }

    hr = S_OK;

    //
    // Update our current offset
    m_dwCurrentOffset = dwOffset;

    Done:
        return hr;
}


//
// NOTE: this API is totally stupid -- its mainly used as an old way of figuring out
//   how big a file is.  we wont hack up our FileObject just to support it
//   (no file operations depend on it...)
HRESULT
FileStream::SetFilePointer(LONG lDistance, DWORD dwMoveMethod, ULONG *pOffset)
{
    PREFAST_ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    ce::smart_ptr<FileObject> pObject = NULL;
    HRESULT hr = E_FAIL;
    DWORD dwFileSize = 0;
    DWORD dwNewOffset = 0;

    *pOffset = 0;

    if(FALSE == CanWrite() && FALSE == CanRead()) {
        return E_ACCESSDENIED;
    }

    //
    // Find our file
    if(FAILED(SMB_Globals::g_pAbstractFileSystem->FindFile(this->FID(), pObject))) {
        TRACEMSG(ZONE_FILES, (L"SMBSRVR: FileStream -- couldnt find file with FID:0x%x", this->FID()));
        goto Done;
    }

    //
    // Figure out our old file size_t
    if(FAILED(hr = pObject->GetFileSize(&dwFileSize))) {
        goto Done;
    }

    //
    // Figure out the offset we would be at
    switch(dwMoveMethod) {
        case 0: // seek from start of file
            dwNewOffset = lDistance;
            break;
        case 1: // seek from current file pointer
            dwNewOffset = m_dwCurrentOffset + lDistance;
            break;
        case 2: // seek from end of file
            if(lDistance > 0 && (DWORD)lDistance > dwFileSize) {
                hr = E_INVALIDARG;
                goto Done;
            }
            dwNewOffset = dwFileSize - lDistance;
            break;
        default:
            ASSERT(FALSE);
            goto Done;
    }

    //
    // If after moving the offsets around we are larger than we used to
    //   be set the end of stream
    if(CanWrite () && m_dwCurrentOffset >= dwFileSize) {
        if(FAILED(hr = pObject->SetEndOfStream(m_dwCurrentOffset))) {
            goto Done;
        }
    }
    //
    // Update our offset since we are successful
    m_dwCurrentOffset = dwNewOffset;
    *pOffset = m_dwCurrentOffset;
    hr = S_OK;
    Done:
        return hr;
}

HRESULT
FileStream::SetFileTime(FILETIME *pCreation,
                    FILETIME *pAccess,
                    FILETIME *pWrite)
{
    PREFAST_ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    ce::smart_ptr<FileObject> pObject = NULL;
    HRESULT hr = E_FAIL;

    if(FALSE == CanWrite()) {
        return E_ACCESSDENIED;
    }
    if(FAILED(SMB_Globals::g_pAbstractFileSystem->FindFile(this->FID(), pObject))) {
        TRACEMSG(ZONE_FILES, (L"SMBSRVR: FileStream -- couldnt find file with FID:0x%x", this->FID()));
        goto Done;
    }
    if(!pObject || FAILED(hr = pObject->SetFileTime(pCreation, pAccess, pWrite))) {
        TRACEMSG(ZONE_FILES, (L"SMBSRVR: FileStream -- couldnt set file time for file with FID:0x%x", this->FID()));
        goto Done;
    }
    hr = S_OK;
    Done:
        return hr;
}

HRESULT
FileStream::Close() {
    PREFAST_ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    HRESULT hr = E_FAIL;


    //
    // Make sure we are really open
    if(FALSE == m_fOpened) {
        ASSERT(FALSE);
        goto Done;
    }

    //
    // Close up the file in the abstract filesystem
    TRACEMSG(ZONE_FILES, (L"SMBSRVR: FileServer -- closing up file with FID:0x%x", this->FID()));
    if(FAILED(hr = SMB_Globals::g_pAbstractFileSystem->Close(this->FID()))) {
        goto Done;
    }

    m_fOpened = FALSE;

    //
    // Update our current offset
    m_dwCurrentOffset = 0;

    hr = S_OK;
    Done:
        return hr;
}


HRESULT
FileStream::GetFileSize(DWORD *pdwSize) {
    PREFAST_ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    ce::smart_ptr<FileObject> pObject = NULL;
    HRESULT hr = E_FAIL;

    if(FAILED(SMB_Globals::g_pAbstractFileSystem->FindFile(this->FID(), pObject))) {
        goto Done;
    }
    hr = pObject->GetFileSize(pdwSize);
    TRACEMSG(ZONE_FILES, (L"SMBSRVR: FileServer -- GetFileSize on FID:0x%x", this->FID()));
    Done:
        return hr;
}


HRESULT
FileStream::GetFileTime(LPFILETIME lpCreationTime,
                       LPFILETIME lpLastAccessTime,
                       LPFILETIME lpLastWriteTime)
{
    PREFAST_ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    ce::smart_ptr<FileObject> pObject = NULL;
    HRESULT hr = E_FAIL;

    if(FAILED(SMB_Globals::g_pAbstractFileSystem->FindFile(this->FID(), pObject))) {
       goto Done;
    }
    hr = pObject->GetFileTime(lpCreationTime, lpLastAccessTime, lpLastWriteTime);

    Done:
       if(FAILED(hr)) {
            TRACEMSG(ZONE_FILES, (L"SMBSRVR: FileServer -- GetFileTime on FID:0x%x failed", this->FID()));
       }
       return hr;
}


HRESULT
FileStream::Flush() {
    PREFAST_ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    ce::smart_ptr<FileObject> pObject = NULL;
    HRESULT hr = E_FAIL;

    if(FAILED(SMB_Globals::g_pAbstractFileSystem->FindFile(this->FID(), pObject))) {
        goto Done;
    }
    hr = pObject->Flush();
    TRACEMSG(ZONE_FILES, (L"SMBSRVR: FileServer -- Flush on FID:0x%x", this->FID()));
    Done:
        return hr;
}

HRESULT
FileStream::QueryFileInformation(WIN32_FIND_DATA *fd)
{
    PREFAST_ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    ce::smart_ptr<FileObject> pObject = NULL;
    HRESULT hr = E_FAIL;
    HANDLE hFileHand = INVALID_HANDLE_VALUE;

    if(FAILED(SMB_Globals::g_pAbstractFileSystem->FindFile(this->FID(), pObject))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  COM_QUERY_EX_INFO -- cant find FID %d!!", this->FID()));
        goto Done;
    }
    memset(fd, 0, sizeof(WIN32_FIND_DATA));
    if(INVALID_HANDLE_VALUE == (hFileHand = FindFirstFile(pObject->FileName(), fd))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  COM_QUERY_EX_INFO -- getting attributes for %s failed (%d)!!", pObject->FileName(), GetLastError()));
        goto Done;
    }
    TRACEMSG(ZONE_FILES, (L"SMBSRVR: FileServer -- QueryFileInformation on FID:0x%x", this->FID()));

    //
    // Overwrite the file times with what comes out of GetFileTime (b/c it may
    //   be more accurate if the handle is okay to use)
    pObject->GetFileTime(&fd->ftCreationTime, &fd->ftLastAccessTime, &fd->ftLastWriteTime);

    //
    // Success
    hr = S_OK;
    Done:
        if(INVALID_HANDLE_VALUE != hFileHand) {
            FindClose(hFileHand);
        }
        return hr;
}




HRESULT
FileStream::IsLocked(SMB_LARGELOCK_RANGE *pRangeLock, BOOL *pfLocked)
{
    PREFAST_ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    ce::smart_ptr<FileObject> pObject = NULL;
    HRESULT hr = E_FAIL;

    if(FAILED(SMB_Globals::g_pAbstractFileSystem->FindFile(this->FID(), pObject))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  IsLocked -- cant find FID %d!!", this->FID()));
        goto Done;
    }

    hr = pObject->IsLocked(pRangeLock, pfLocked);

    Done:
        return hr;
}

HRESULT
FileStream::Lock(SMB_LARGELOCK_RANGE *pRangeLock)
{
    PREFAST_ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    ce::smart_ptr<FileObject> pObject = NULL;
    HRESULT hr = E_FAIL;

    if(FAILED(SMB_Globals::g_pAbstractFileSystem->FindFile(this->FID(), pObject))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  IsLocked -- cant find FID %d!!", this->FID()));
        goto Done;
    }

    hr = pObject->Lock(pRangeLock);

    Done:
        return hr;
}

HRESULT
FileStream::UnLock(SMB_LARGELOCK_RANGE *pRangeLock)
{
    PREFAST_ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    ce::smart_ptr<FileObject> pObject = NULL;
    HRESULT hr = E_FAIL;

    if(FAILED(SMB_Globals::g_pAbstractFileSystem->FindFile(this->FID(), pObject))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  IsLocked -- cant find FID %d!!", this->FID()));
        goto Done;
    }

    hr = pObject->UnLock(pRangeLock);

    Done:
        return hr;

}


HRESULT
FileStream::BreakOpLock()
{
    PREFAST_ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    ce::smart_ptr<FileObject> pObject = NULL;
    ce::smart_ptr<PerFileState> pState = NULL;
    HRESULT hr = E_FAIL;

    if(FAILED(SMB_Globals::g_pAbstractFileSystem->FindFile(this->FID(), pObject))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  BreakOpLock -- cant find FID %d!!", this->FID()));
        goto Done;
    }

    if(!(pState = pObject->GetPerFileState())) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  BreakOpLock -- cant get per file state for FID:%d!!", this->FID()));
        ASSERT(FALSE);
        goto Done;
    }

    //
    // If anyone is waiting on our break to wakeup (oplock) do that now
    while(pState->m_BlockedOnOpLockList.size()) {
        SetEvent(pState->m_BlockedOnOpLockList.front());
        pState->m_BlockedOnOpLockList.pop_front();
    }

    hr = S_OK;

    Done:
        return hr;

}




HRESULT
FileStream::Delete()
{
    PREFAST_ASSERT(NULL != SMB_Globals::g_pAbstractFileSystem);
    ce::smart_ptr<FileObject> pObject = NULL;
    HRESULT hr = E_FAIL;

    if(FAILED(SMB_Globals::g_pAbstractFileSystem->FindFile(this->FID(), pObject))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  IsLocked -- cant find FID %d!!", this->FID()));
        goto Done;
    }

    hr = pObject->Delete();

    Done:
        return hr;

}

const WCHAR *
FileStream::GetFileName()
{
    ce::smart_ptr<FileObject> pObject = NULL;
    const WCHAR *pRet = NULL;

    if(FAILED(SMB_Globals::g_pAbstractFileSystem->FindFile(this->FID(), pObject))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV:  GetFileName -- cant find FID %d!!", this->FID()));
        goto Done;
    }

    pRet = pObject->GetFileName();
    Done:
        return pRet;
}

HRESULT
FileStream::GetChangeNotification(HANDLE *pHand, const WCHAR **pListenOn)
{
    if(INVALID_HANDLE_VALUE == m_hChangeNotification) {
        return E_FAIL;
    }

    *pHand = m_hChangeNotification;
    *pListenOn = m_ChangeListeningTo.GetString();
    return S_OK;
}

HRESULT
FileStream::SetChangeNotification(HANDLE h, const WCHAR *pListenOn)
{
    ASSERT(INVALID_HANDLE_VALUE == m_hChangeNotification);
    m_hChangeNotification = h;
    m_ChangeListeningTo.Clear();
    return m_ChangeListeningTo.append(pListenOn);
}



//
// Abstraction used for all file operations -- mainly here to implement range locking
FileObject::FileObject(USHORT usFID)
{
    m_usFID = usFID;
    m_hFile = INVALID_HANDLE_VALUE;
    m_LockNode = NULL;
    InitializeCriticalSection(&m_csFileLock);
}

FileObject::~FileObject()
{
    DeleteCriticalSection(&m_csFileLock);
}

HRESULT
FileObject::Delete()
{
    CCritSection csLock(&m_csFileLock);
    csLock.Lock();
    ASSERT(m_PerFileState);
    ASSERT(0 == m_PerFileState->m_uiNonShareDeleteOpens || (!m_fShareDelete && 1 == m_PerFileState->m_uiNonShareDeleteOpens));
    m_PerFileState->m_fDeleteOnClose = TRUE;
    return S_OK;
}

HRESULT
FileObject::Close()
{
    PREFAST_ASSERT(NULL != m_LockNode);
    CCritSection csLock(&m_csFileLock);
    csLock.Lock();

    m_LockNode->UnLockAll(m_usFID);

    if(INVALID_HANDLE_VALUE != m_hFile) {
            TRACEMSG(ZONE_FILES, (L"Closing %s FID:%d with handle 0x%x:", FileName(), m_usFID, (UINT)m_hFile));
            CloseHandle(m_hFile);
            m_hFile = INVALID_HANDLE_VALUE;
    }
    return S_OK;
}

const WCHAR *
FileObject::GetFileName() {
    CCritSection csLock(&m_csFileLock);
    csLock.Lock();
    return m_PerFileState->m_FileName.GetString();
}


HRESULT
FileObject::Open(const WCHAR *pFile,
                 DWORD _dwCreationDispostion,
                 DWORD _dwAttributes,
                 DWORD _dwAccess,
                 DWORD _dwShareMode,
                 DWORD *_pdwActionTaken)
{
    HRESULT hr = E_FAIL;

    TRACEMSG(ZONE_FILES, (L"Opening %s with following attributes:", pFile));

    CCritSection csLock(&m_csFileLock);
    csLock.Lock();

    //
    // Display all creation disposition stuff
#ifdef DEBUG
    if(CREATE_NEW == _dwCreationDispostion) {
        TRACEMSG(ZONE_FILES, (L"   CREATE_NEW"));
    } else if(CREATE_ALWAYS == _dwCreationDispostion) {
        TRACEMSG(ZONE_FILES, (L"   CREATE_ALWAYS"));
    } else if(OPEN_EXISTING == _dwCreationDispostion) {
        TRACEMSG(ZONE_FILES, (L"   OPEN_EXISTING"));
    } else if(OPEN_ALWAYS == _dwCreationDispostion) {
        TRACEMSG(ZONE_FILES, (L"   OPEN_ALWAYS"));
    } else if(TRUNCATE_EXISTING == _dwCreationDispostion) {
        TRACEMSG(ZONE_FILES, (L"   TRUNCATE_EXISTING"));
    }

    if(_dwAccess & GENERIC_READ) {
        TRACEMSG(ZONE_FILES, (L"   GENERIC_READ"));
    }
    if(_dwAccess & GENERIC_WRITE) {
        TRACEMSG(ZONE_FILES, (L"   GENERIC_WRITE"));
    }

    if(_dwAttributes & FILE_ATTRIBUTE_ARCHIVE) {
        TRACEMSG(ZONE_FILES, (L"   FILE_ATTRIBUTE_ARCHIVE"));
    }
    if(_dwAttributes & FILE_ATTRIBUTE_HIDDEN) {
        TRACEMSG(ZONE_FILES, (L"   FILE_ATTRIBUTE_HIDDEN"));
    }
    if(_dwAttributes & FILE_ATTRIBUTE_NORMAL) {
        TRACEMSG(ZONE_FILES, (L"   FILE_ATTRIBUTE_NORMAL"));
    }
    if(_dwAttributes & FILE_ATTRIBUTE_READONLY) {
        TRACEMSG(ZONE_FILES, (L"   FILE_ATTRIBUTE_READONLY"));
    }
    if(_dwAttributes & FILE_ATTRIBUTE_SYSTEM) {
        TRACEMSG(ZONE_FILES, (L"   FILE_ATTRIBUTE_SYSTEM"));
    }
    if(_dwAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        TRACEMSG(ZONE_FILES, (L"   FILE_ATTRIBUTE_DIRECTORY"));
    }

    if(_dwAttributes & FILE_FLAG_WRITE_THROUGH) {
        TRACEMSG(ZONE_FILES, (L"   FILE_FLAG_WRITE_THROUGH"));
    }

    //
    // Take care of share mode
    if(_dwShareMode & FILE_SHARE_READ) {
        TRACEMSG(ZONE_FILES, (L"   FILE_SHARE_READ"));
    }
    if(_dwShareMode & FILE_SHARE_WRITE) {
        TRACEMSG(ZONE_FILES, (L"   FILE_SHARE_WRITE"));
    }
#endif

    if(m_hFile != INVALID_HANDLE_VALUE) {
        ASSERT(FALSE);
        hr = E_FAIL;
        goto Done;
    }

    //
    // Open up the file
    if(!(_dwAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        m_hFile = CreateFile(pFile, _dwAccess, _dwShareMode, NULL, _dwCreationDispostion ,_dwAttributes,NULL);
        if(INVALID_HANDLE_VALUE == m_hFile) {
           hr = HRESULT_FROM_WIN32(GetLastError());
           TRACEMSG(ZONE_FILES, (L"SMBSRV-FILE: CreateFile for %s failed with GLE: %d", pFile, GetLastError()));
           goto Done;
       }
    } else {
        if(CREATE_ALWAYS == _dwCreationDispostion ||
           CREATE_NEW    == _dwCreationDispostion ||
           OPEN_ALWAYS   == _dwCreationDispostion) {

           if(0 == CreateDirectory(pFile, NULL)) {
              DWORD dwErr = GetLastError();
              TRACEMSG(ZONE_ERROR, (L"SMBSRV-FILE: CreateDirectory for %s failed with GLE:%d!", pFile, dwErr));

              //
              // Special case already exist errors
              if(ERROR_ALREADY_EXISTS == dwErr || ERROR_FILE_EXISTS == dwErr) {
                  BOOL fIsDirectory;

                  switch(_dwCreationDispostion) {
                    case CREATE_NEW:
                        dwErr = ERROR_ALREADY_EXISTS;
                        break;
                    case CREATE_ALWAYS:
                    case OPEN_ALWAYS:
                    default:
                       // If the file exists and is a directory its okay b/c we are CREATE_ALWAYS, CREATE_NEW, or OPEN_ALWAYS
                      if(FAILED(hr = IsDirectory(pFile, &fIsDirectory)) || !fIsDirectory) {
                          dwErr = ERROR_ALREADY_EXISTS;
                          goto Done;
                      } else {
                          dwErr = ERROR_ALREADY_EXISTS;
                      }

                  }
              }

/*            //
              // I'd argue invalid_name is correct, but XP's redir disagrees
              if(ERROR_INVALID_NAME == dwErr) {
                  dwErr = ERROR_DIR_NOT_ROOT;
              }*/

              if(FAILED(hr = HRESULT_FROM_WIN32(dwErr))) {
                  TRACEMSG(ZONE_FILES, (L"SMBSRV-FILE: CreateDirectory for %s failed with GLE: %d", pFile, dwErr));
                  goto Done;
              }
           }
        } else if(OPEN_EXISTING == _dwCreationDispostion) {
            BOOL fStatus = FALSE;
            hr = HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
            if(SUCCEEDED(IsDirectory(pFile, &fStatus)) && fStatus) {
                hr = S_OK;
            }
        }
    }

    TRACEMSG(ZONE_FILES, (L"Createfile on %s FID: %d got handle 0x%x:", FileName(), m_usFID, (UINT)m_hFile));

    if(NULL != _pdwActionTaken) {
        if(CREATE_ALWAYS == _dwCreationDispostion || OPEN_ALWAYS == _dwCreationDispostion) {
            if(ERROR_ALREADY_EXISTS == GetLastError()) {
                *_pdwActionTaken = 1;
            } else {
                *_pdwActionTaken = 2;
            }
        } else if (CREATE_NEW == _dwCreationDispostion) {
            *_pdwActionTaken = 2;
        } else {
            *_pdwActionTaken = 1;
        }
    }
    if(FAILED(hr = m_FileName.append(pFile))) {
        ASSERT(FALSE);
        goto Done;
    }

    hr = S_OK;
    Done:
        if(FAILED(hr)) {
            if(INVALID_HANDLE_VALUE != m_hFile) {
                CloseHandle(m_hFile);
                m_hFile = INVALID_HANDLE_VALUE;
            }
        }
        return hr;
}


HRESULT FileObject::Read(USHORT usFID,
                        BYTE *pDest,
                        LONG lOffset,
                        DWORD dwReqSize,
                        DWORD *pdwRead)
{
    HRESULT hr = S_OK;
    DWORD dwTotalRead = 0;
    DWORD dwJustRead = 0;
    USHORT usRangeFID = 0xFFFF;
    BOOL   fIsLocked = TRUE;
    CCritSection csLock(&m_csFileLock);

    ASSERT(INVALID_HANDLE_VALUE != m_hFile);
    IFDBG(DWORD dwRequestOrig = dwReqSize);
    TRACEMSG(ZONE_FILES, (L"Read on %s FID: %d got handle 0x%x:", FileName(), m_usFID, (UINT)m_hFile));

    //
    // First make sure that this region isnt locked (and if it is locked, that its locked by us)
    LARGE_INTEGER Offset;
    LARGE_INTEGER Size;
    SMB_LARGELOCK_RANGE RangeLock;
    Offset.QuadPart = lOffset;
    Size.QuadPart = dwReqSize;
    RangeLock.LengthHigh = Size.HighPart;
    RangeLock.LengthLow = Size.LowPart;
    RangeLock.OffsetHigh = Offset.HighPart;
    RangeLock.OffsetLow = Offset.LowPart;

    csLock.Lock();
    if(FAILED(m_LockNode->IsLocked(&RangeLock, &fIsLocked, &usRangeFID))) {
        TRACEMSG(ZONE_ERROR, (L"SMBFILEOBJECT: Can't test range lock (FID: 0x%x)failed on Read", usFID));
        ASSERT(FALSE);
        hr = HRESULT_FROM_WIN32(ERROR_LOCK_VIOLATION);
        goto Done;
    }

    //
    // Set the file pointer.
    if(0xFFFFFFFF == ::SetFilePointer(m_hFile, lOffset, NULL, FILE_BEGIN)) {
        if(ERROR_SUCCESS != GetLastError()) {
            TRACEMSG(ZONE_ERROR, (L"SMBFILEOBJECT: Setting file pointer (FID: 0x%x)failed on Read -- GetLastError(%d)", usFID, GetLastError()));
            hr = HRESULT_FROM_WIN32(GetLastError());
            csLock.UnLock();
            goto Done;
        }
    }

    //
    // Do the file operation
    while(dwReqSize) {
        if(0 == ReadFile(m_hFile, pDest, dwReqSize, &dwJustRead, NULL)) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRVR: File -- call to ReadFile(FID:0x%x) for %d bytes failed -- GetLastError(%d)", usFID, dwReqSize, GetLastError()));
            hr = HRESULT_FROM_WIN32(GetLastError());
            csLock.UnLock();
            goto Done;
        }

        if(0 == dwJustRead) {
            csLock.UnLock();
            goto Done;
        }
        dwReqSize -= dwJustRead;
        pDest += dwJustRead;
        dwTotalRead += dwJustRead;
    }

    csLock.UnLock();
    if(TRUE == fIsLocked && usFID != usRangeFID) {
        TRACEMSG(ZONE_ERROR, (L"SMBFILEOBJECT: Can't read (FID: 0x%x) (Offset: %d)  (Size: %d) because its locked by (FID: %d)", usFID, lOffset,dwReqSize, usRangeFID));
        hr = HRESULT_FROM_WIN32(ERROR_LOCK_VIOLATION);
        goto Done;
    }
Done:
    IFDBG(TRACEMSG(ZONE_FILES, (L"SMBSRV: Readfile -- FID: %d  Offset: %d  ReqSize: %d  Got: %d  GLE: %d", usFID, lOffset, dwRequestOrig, dwTotalRead, GetLastError())));
    *pdwRead = dwTotalRead;
    return hr;

}

HRESULT FileObject::Write(USHORT usFID,
                         BYTE *pSrc,
                         LONG lOffset,
                         DWORD dwReqSize,
                         DWORD *pdwWritten)
{
    HRESULT hr = S_OK;
    DWORD dwJustWritten = 0;
    DWORD dwTotalWritten = 0;
    ASSERT(INVALID_HANDLE_VALUE != m_hFile);
    IFDBG(DWORD dwOrigRequest = dwReqSize);
    TRACEMSG(ZONE_FILES, (L"Write on %s FID: %d got handle 0x%x:", FileName(), m_usFID, (UINT)m_hFile));
    USHORT usRangeFID = 0xFFFF;
    BOOL   fIsLocked = TRUE;
    CCritSection csLock(&m_csFileLock);

    //
    // First make sure that this region isnt locked (and if it is locked, that its locked by us)
    LARGE_INTEGER Offset;
    LARGE_INTEGER Size;
    SMB_LARGELOCK_RANGE RangeLock;
    Offset.QuadPart = lOffset;
    Size.QuadPart = dwReqSize;

    RangeLock.LengthHigh = Size.HighPart;
    RangeLock.LengthLow = Size.LowPart;
    RangeLock.OffsetHigh = Offset.HighPart;
    RangeLock.OffsetLow = Offset.LowPart;
    csLock.Lock();
    if(FAILED(m_LockNode->IsLocked(&RangeLock, &fIsLocked, &usRangeFID))) {
        TRACEMSG(ZONE_ERROR, (L"SMBFILEOBJECT: Can't test range lock (FID: 0x%x)failed on Write", usFID));
        ASSERT(FALSE);
        hr = HRESULT_FROM_WIN32(ERROR_LOCK_VIOLATION);
        goto Done;
    }
    csLock.UnLock();

    if(TRUE == fIsLocked && usFID != usRangeFID) {
        TRACEMSG(ZONE_ERROR, (L"SMBFILEOBJECT: Can't write (FID: 0x%x) (Offset: %d)  (Size: %d) because its locked by (FID:%d)", usFID, lOffset,dwReqSize, usRangeFID));
        hr = HRESULT_FROM_WIN32(ERROR_LOCK_VIOLATION);
        goto Done;
    }

    //
    // Set the file pointer.
    if(0xFFFFFFFF == ::SetFilePointer(m_hFile, lOffset, NULL, FILE_BEGIN)) {
        if(ERROR_SUCCESS != GetLastError()) {
            TRACEMSG(ZONE_ERROR, (L"SMBFILEOBJECT: Setting file pointer (FID: 0x%x)failed on Write-- GetLastError(%d)", usFID, GetLastError()));
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto Done;
        }
    }

    //
    // Do the file operation
    while(dwReqSize) {
        *pdwWritten = 0;
        ASSERT(!csLock.IsLocked());
        if(0 == WriteFile(m_hFile, pSrc, dwReqSize, &dwJustWritten, NULL)){
            TRACEMSG(ZONE_ERROR, (L"SMBSRVR: File -- call to WriteFile (FID: 0x%x) for %d bytes failed -- GetLastError(%d)", usFID, dwReqSize, GetLastError()));
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto Done;
        }

        dwReqSize -= dwJustWritten;
        pSrc += dwJustWritten;
        dwTotalWritten += dwJustWritten;
    }

    Done:
        *pdwWritten = dwTotalWritten;
        IFDBG(TRACEMSG(ZONE_FILES, (L"SMBSRV: Writefile -- FID: %d  Offset: %d  ReqSize: %d  Wrote: %d  GLE: %d", usFID, lOffset, dwOrigRequest, dwTotalWritten, GetLastError())));
        return hr;
}


HRESULT
FileObject::SetEndOfStream(DWORD dwOffset)
{
    HRESULT hr = S_OK;
    ASSERT(INVALID_HANDLE_VALUE != m_hFile);
    TRACEMSG(ZONE_FILES, (L"SetEndOfStream on %s FID: %d got handle 0x%x:", FileName(), m_usFID, (UINT)m_hFile));

    //
    // Set the file pointer.
    if(0xFFFFFFFF == ::SetFilePointer(m_hFile, dwOffset, NULL, FILE_BEGIN)) {
        if(ERROR_SUCCESS != GetLastError()) {
            TRACEMSG(ZONE_ERROR, (L"SMBFILEOBJECT: Setting file pointer failed on SetEndOfStream-- GetLastError(%d)", GetLastError()));
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto Done;
        }
    }

    //
    // Do the set file operation
    if(0 == SetEndOfFile(m_hFile)) {
        TRACEMSG(ZONE_ERROR, (L"SMBFILEOBJECT: Setting end of file failed on SetEndOfStream-- GetLastError(%d)", GetLastError()));
        hr = HRESULT_FROM_WIN32(GetLastError());
    }

    Done:
        return hr;
}

HRESULT FileObject::SetFileTime(FILETIME *pCreation,
                               FILETIME *pAccess,
                               FILETIME *pWrite)
{
    HRESULT hr = E_FAIL;;
    TRACEMSG(ZONE_FILES, (L"SetFileTime on %s FID: %d got handle 0x%x:", FileName(), m_usFID, (UINT)m_hFile));

    if(INVALID_HANDLE_VALUE == m_hFile) {
       hr = E_ACCESSDENIED;
       goto Done;
    }
    //
    // Do the set file operation
    if(0 == ::SetFileTime(m_hFile, pCreation, pAccess, pWrite)) {
        TRACEMSG(ZONE_ERROR, (L"SMBFILEOBJECT: Setting filetime failed on SetFileTime-- GetLastError(%d)", GetLastError()));
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Done;
    }

    //
    // Success
    hr = S_OK;

    Done:
        return hr;
}

HRESULT
FileObject::GetFileSize(DWORD *pdwSize)
{
    TRACEMSG(ZONE_FILES, (L"GetFileSize on %s FID: %d got handle 0x%x:", FileName(), m_usFID, (UINT)m_hFile));

    if(INVALID_HANDLE_VALUE != m_hFile) {
        *pdwSize = ::GetFileSize(m_hFile, NULL);
    } else {
        *pdwSize = 0;
    }
    return S_OK;
}

USHORT
FileObject::FID()
{
    return m_usFID;
}

const WCHAR  *
FileObject::FileName()
{
    return m_FileName.GetString();
}


HRESULT
FileObject::Flush()
{
    HRESULT hr = E_FAIL;
    ASSERT(INVALID_HANDLE_VALUE != m_hFile);
    TRACEMSG(ZONE_FILES, (L"Flush on %s FID: %d got handle 0x%x:", FileName(), m_usFID, (UINT)m_hFile));

    //
    // Try the operation
    if(0 == FlushFileBuffers(m_hFile)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRVR: FileObject::FlushFileBuffers() failed!"));
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Done;
    }

    //
    // if we get here, everything is okay
    hr = S_OK;

    Done:
        return hr;
}

HRESULT
FileObject::GetFileTime(LPFILETIME lpCreationTime,
                        LPFILETIME lpLastAccessTime,
                        LPFILETIME lpLastWriteTime)
{
    BOOL    ret = FALSE;
    HRESULT hr = E_FAIL;
    TRACEMSG(ZONE_FILES, (L"GetFileTime on %s FID: %d got handle 0x%x:", FileName(), m_usFID, (UINT)m_hFile));

    //
    //  if the m_hFile handle is invalid then immediately try by name
    //  try the operation with a real GetFileTime, if that succeeds great else
    //
    //  NOTE: we may have to do this by filename (with findfirstfile) b/c the file may not be open for read
    //
    if(INVALID_HANDLE_VALUE == m_hFile) {
        //
        //  try getting the file time information by filename
        //
        if(FAILED(GetFileTimesFromFileName(FileName(), lpCreationTime, lpLastAccessTime, lpLastWriteTime))) {
            //
            //  failure
            //
            TRACEMSG(ZONE_ERROR, (L"SMBSRVR: FileObject::GetFileTime() failed! (%d)", GetLastError()));
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto Done;
        }
    } else if(::GetFileTime(m_hFile, lpCreationTime, lpLastAccessTime, lpLastWriteTime)) {
        //
        //  SUCCESS
        //
    } else if(FAILED(GetFileTimesFromFileName(FileName(), lpCreationTime, lpLastAccessTime, lpLastWriteTime))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRVR: FileObject::GetFileTime() failed! (%d)", GetLastError()));
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Done;
    }

    //
    // if we get here, everything is okay
    hr = S_OK;

Done:
    return hr;
}


VOID
FileObject::SetPerFileState(ce::smart_ptr<PerFileState> &pNode)
{
    CCritSection csLock(&m_csFileLock);
    csLock.Lock();

    ASSERT(pNode);
    m_LockNode = &(pNode->m_RangeLock);
    m_PerFileState = pNode;
}

ce::smart_ptr<PerFileState>
FileObject::GetPerFileState()

{
    CCritSection csLock(&m_csFileLock);
    csLock.Lock();
    return m_PerFileState;
}

HRESULT
FileObject::IsLocked(SMB_LARGELOCK_RANGE *pRangeLock, BOOL *pfLocked)
{
    CCritSection csLock(&m_csFileLock);
    csLock.Lock();
    return m_LockNode->IsLocked(pRangeLock, pfLocked);
}

HRESULT
FileObject::Lock(SMB_LARGELOCK_RANGE *pRangeLock)
{
    CCritSection csLock(&m_csFileLock);
    csLock.Lock();
    return m_LockNode->Lock(m_usFID, pRangeLock);
}

HRESULT
FileObject::UnLock(SMB_LARGELOCK_RANGE *pRangeLock)
{
    CCritSection csLock(&m_csFileLock);
    csLock.Lock();
    return m_LockNode->UnLock(m_usFID, pRangeLock);
}



//
// Abstraction for file system (it makes up for things the CE filesystem lacks)
AbstractFileSystem::AbstractFileSystem()
{
    ASSERT(0 == m_FileObjectList.size());
    InitializeCriticalSection(&AFSLock);
}

AbstractFileSystem::~AbstractFileSystem()
{
    //
    // Go through all the file objects deleting (and closing) them
    while(m_FileObjectList.size()) {
        m_FileObjectList.pop_front();
    }
    DeleteCriticalSection(&AFSLock);
}


//NOTE:  this API must call SetLastError!!!
HRESULT
AbstractFileSystem::AFSDeleteFile(const WCHAR *pFileName)
{
    CCritSection csLock(&AFSLock);
    csLock.Lock();

    ce::list<ce::smart_ptr<PerFileState>, AFS_PERFILESTATE_ALLOC >::iterator it;
    ce::smart_ptr<PerFileState> pFileState = NULL;
    HRESULT hr = E_FAIL;

    //
    // Search out a per file node
    ASSERT(csLock.IsLocked());
    for(it = m_PerFileState.begin(); it != m_PerFileState.end(); ++it) {
        if(0 == wcscmp(pFileName, (*it)->m_FileName.GetString())) {
            pFileState = *it;
            ASSERT(0 != (*it)->m_uiRefCnt);
            break;
        }
    }

    if(!pFileState) {
        hr = (0 != DeleteFile(pFileName))?S_OK:E_FAIL;
    } else {
        if(0 == (*it)->m_uiNonShareDeleteOpens) {
            (*it)->m_fDeleteOnClose = TRUE;
            hr = S_OK;
        } else {
            hr = E_ACCESSDENIED;
            SetLastError(ERROR_SHARING_VIOLATION);
        }
    }
    return hr;
}


HRESULT
AbstractFileSystem::BreakOpLock(PerFileState *pFileState)

{
    CCritSection csLock(&AFSLock);
    csLock.Lock();

    HANDLE hWakeup;
    DWORD dwWaitRet;
    HRESULT hr = E_FAIL;

    if(NULL == (hWakeup = CreateEvent(NULL, TRUE, FALSE, NULL))) {
        hr = E_OUTOFMEMORY;
        goto Done;
    }

    if(!pFileState->m_BlockedOnOpLockList.push_back(hWakeup)) {
        hr = E_OUTOFMEMORY;
        goto Done;
    }

    //
    // If nobody has sent a break notice, do it now
    if(FALSE == pFileState->m_fOpBreakSent) {
        SMB_PACKET *pNewPacket;
        UINT uiPacketSize = sizeof(SMB_HEADER)+sizeof(SMB_LOCKX_CLIENT_REQUEST);

        if(NULL == (pNewPacket = SMB_Globals::g_SMB_Pool.Alloc())) {
            hr = E_OUTOFMEMORY;
            goto Done;
        }

        pNewPacket->pInSMB = NULL;
        pNewPacket->uiPacketType = SMB_NORMAL_PACKET;
        ASSERT(pFileState->m_pTransToken);
        pNewPacket->pToken = pFileState->m_pTransToken;
        pNewPacket->pOutSMB = (SMB_HEADER *)pNewPacket->OutSMB;
        pNewPacket->uiOutSize = uiPacketSize;
        pNewPacket->pfnQueueFunction = pFileState->m_pfnQueueFunction;
        pNewPacket->ulConnectionID = pFileState->m_ulLockConnectionID;
        pNewPacket->dwDelayBeforeSending = 0;
#ifdef DEBUG
        pNewPacket->uiPacketNumber = InterlockedIncrement(&SMB_Globals::g_PacketID);
#endif



        SMB_HEADER *pHeader = pNewPacket->pOutSMB;
        SMB_LOCKX_CLIENT_REQUEST *pLockHeader = (SMB_LOCKX_CLIENT_REQUEST*)(pHeader+1);
        memset(pHeader, 0, sizeof(SMB_HEADER));
        memset(pLockHeader, 0, sizeof(SMB_LOCKX_CLIENT_REQUEST));

        pHeader->Protocol[0] = 0xFF;
        pHeader->Protocol[1] = 'S';
        pHeader->Protocol[2] = 'M';
        pHeader->Protocol[3] = 'B';
        pHeader->Command = SMB_COM_LOCKING_ANDX;
        pHeader->StatusFields.Fields.Status = 0;
        pHeader->Flags = 0;
        pHeader->Flags2 = 0;
        pHeader->Tid = pFileState->m_usLockTID;
        pHeader->Pid = 0xFFFF;
        pHeader->Uid = 0;
        pHeader->Mid = 0xFFFF;

        pLockHeader->ANDX.WordCount = (sizeof(SMB_LOCKX_CLIENT_REQUEST)-3)/2;
        pLockHeader->ANDX.AndXCommand = 0xFF;
        pLockHeader->ANDX.AndXOffset = 0;
        pLockHeader->ANDX.AndXReserved = 0;
        pLockHeader->FileID = pFileState->m_usLockFID;
        pLockHeader->LockType = 2;

        if(FAILED(hr = pNewPacket->pfnQueueFunction(pNewPacket, TRUE))) {
            ASSERT(FALSE);
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: Sending packet to Queue function FAILED!"));
            goto Done;
        }
        pFileState->m_pTransToken = NULL;
        pFileState->m_fOpBreakSent = TRUE;
    }
    pFileState->m_fBatchOpLock = FALSE;


    //
    // Free up the cracker, and wait for our event to finish processing
    csLock.UnLock();
    dwWaitRet = WaitForSingleObject(hWakeup, INFINITE);

    CloseHandle(hWakeup);

    if(WAIT_OBJECT_0 != dwWaitRet) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Waiting on OpLock failed -- aborting request"));
        ASSERT(FALSE);
        hr = E_ACCESSDENIED;
        goto Done;
    }

    hr = S_OK;

    Done:
        return hr;
}

HRESULT
AbstractFileSystem::Open(const WCHAR *pFileName,
             USHORT usFID,
             DWORD dwAccess,
             DWORD dwCreationDispostion,
             DWORD dwAttributes,
             DWORD dwShareMode,
             DWORD *pdwActionTaken,
             SMBFile_OpLock_PassStruct *pOpLock)
{
    CCritSection csLock(&AFSLock);
    HRESULT hr = E_FAIL;
    ce::list<ce::smart_ptr<PerFileState>, AFS_PERFILESTATE_ALLOC >::iterator it;
    ce::smart_ptr<PerFileState> pFileState = NULL;
    ce::smart_ptr<FileObject> pNewFile = NULL;
    BOOL fShareDelete = FALSE;
    DWORD dwOpLockRequested;

    if(!(pNewFile = new FileObject(usFID))) {
        hr = E_OUTOFMEMORY;
        goto Done;
    }

    //
    //  see what kind of oplock was requsted
    //  if no oplock assume the client doesnt support and do not give an oplock
    //  valid values are "batch" or "no"
    //
    //  we also do not want to give the client an oplock if the open request is for a directory
    //
    if(!pOpLock || FILE_ATTRIBUTE_DIRECTORY & dwAttributes ) {
        dwOpLockRequested = NT_CREATE_NO_OPLOCK;
    } else {
        dwOpLockRequested = pOpLock->dwRequested;
    }
    ASSERT( dwOpLockRequested == NT_CREATE_BATCH_OPLOCK || dwOpLockRequested == NT_CREATE_NO_OPLOCK );

    //
    // If they arent requesting any access (we only know read and write) -- assume we can share out this file
    // This is b/c the filesystem requres us to put in some kind of access, if it didnt we wouldnt need to do this
    if(0 == dwAccess) {
        TRACEMSG(ZONE_FILES, (L"SMBSRV: Because no access was requested, assume we can share out this file totally as no ops would work anyway"));
        dwShareMode |= (FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE);
    }



    //
    // Search out a per file node -- if one doesnt exist, make one
    //    #error put in a hash map
    csLock.Lock();
    for(it = m_PerFileState.begin(); it != m_PerFileState.end(); ++it) {
        if(0 == wcscmp(pFileName, (*it)->m_FileName.GetString())) {
            pFileState = (*it);
            ASSERT(0 != (*it)->m_uiRefCnt);
            break;
        }
    }
    csLock.UnLock();

    //
    // If the file has already been opened, see if we can open it
    //   (if its been deleted we must give access denied)
    if(pFileState && pFileState->m_fDeleteOnClose) {
       hr = E_ACCESSDENIED;
       goto Done;
    }

    //
    // See what the OPLock status is for this file -- if there is a batch on this already
    //   we need to notify the owner of the lock & take appropriate action
    while(pFileState && pFileState->m_fBatchOpLock) {

        // NOTIFY the client to break their oplock
        if(FAILED(BreakOpLock(pFileState))) {
            ASSERT(FALSE);
            hr = E_ACCESSDENIED;
            goto Done;
        }

        //
        // Even though pFileState is refcnted (and would still be around)
        //   Because the file could have been closed it may be gone from the AFS so we
        //   search for it again -- this time there should be no BatchOpLock unless they
        //   closed up and someone came in really quickly... so we will loop
        pFileState = NULL;
        csLock.Lock();
        for(it = m_PerFileState.begin(); it != m_PerFileState.end(); ++it) {
            if(0 == wcscmp(pFileName, (*it)->m_FileName.GetString())) {
                pFileState = (*it);
                ASSERT(0 != (*it)->m_uiRefCnt);
                break;
            }
        }
        csLock.UnLock();

        if(pOpLock) {
            pOpLock->cGiven = NT_CREATE_NO_OPLOCK;
        }
    }
    ASSERT(pFileState?!pFileState->m_fBatchOpLock:TRUE);

    //
    // Remove the SHARE_DELETE if it exists b/c the FS doesnt support it :(
    //
    //  If they are requesting share access & someone already has exclusive deny the request
//    #error double check the spec on this... why not let them in?
    if(FILE_SHARE_DELETE & dwShareMode) {

        if(dwAccess && pFileState && pFileState->m_uiNonShareDeleteOpens) {
            hr = E_ACCESSDENIED;
            goto Done;
        }
        fShareDelete = TRUE;
        dwShareMode &= ~FILE_SHARE_DELETE;
    }

    //
    // Try to open up the file
    if(FAILED(hr = pNewFile->Open(pFileName, dwCreationDispostion, dwAttributes, dwAccess, dwShareMode, pdwActionTaken))) {
        goto Done;
    }

    csLock.Lock();

    //
    // If earlier we couldnt find the node, create a new one
    if(!pFileState) {
//        #error do this w/o calling new!
        PerFileState *pNew = new PerFileState();

        if(!pNew) {
            hr = E_OUTOFMEMORY;
            goto Done;
        }
        if(!m_PerFileState.push_front(pNew)) {
            hr = E_OUTOFMEMORY;
            goto Done;
        }
        pFileState = m_PerFileState.front();

        pFileState->m_uiRefCnt = 0;
        pFileState->m_uiNonShareDeleteOpens = 0;
        pFileState->m_fDeleteOnClose = FALSE;
        pFileState->m_FileName.append(pFileName);
        pFileState->m_fBatchOpLock = FALSE;
        pFileState->m_fOpBreakSent = FALSE;
        pFileState->m_usLockFID = usFID;

        if(pOpLock) {
            pFileState->m_usLockTID = pOpLock->usLockTID;
            pFileState->m_pfnQueueFunction = pOpLock->pfnQueuePacket;
            pFileState->m_pfnDeleteToken = pOpLock->pfnDeleteTransportToken;
            if(FAILED(pOpLock->pfnCopyTranportToken(pOpLock->pTransportToken, &pFileState->m_pTransToken))) {
                //#remove pfileState from list
                hr = E_OUTOFMEMORY;
                goto Done;
            }
            pFileState->m_ulLockConnectionID = pOpLock->ulConnectionID;
        } else {
            pFileState->m_usLockTID = 0xFFFF;
            pFileState->m_pfnQueueFunction = NULL;
            pFileState->m_pfnDeleteToken = NULL;
            pFileState->m_pTransToken = NULL;
        }
    }

    ASSERT(pFileState);
    pFileState->m_uiRefCnt ++;

    if(pOpLock) {
        if(1 == pFileState->m_uiRefCnt && NT_CREATE_BATCH_OPLOCK == dwOpLockRequested) {
            pFileState->m_fBatchOpLock = TRUE;
            pOpLock->cGiven = NT_CREATE_BATCH_OPLOCK;
        } else {
            pOpLock->cGiven = NT_CREATE_NO_OPLOCK;
        }
    }

    pNewFile->SetPerFileState(pFileState);
    pNewFile->SetShareDelete(fShareDelete);

    //
    // If the file isnt opened share delete, inc a counter so we know how many
    //   nonshare deletes are outstanding
    if(!fShareDelete) {
        pFileState->m_uiNonShareDeleteOpens ++;
    }

    IFDBG(ASSERT(csLock.IsLocked()));
    if(!m_FileObjectList.push_back(pNewFile)) {
        hr = E_OUTOFMEMORY;
    } else {
        hr = S_OK;
    }

    Done:
        return hr;
}

HRESULT
AbstractFileSystem::Close(USHORT usFID)
{
    CCritSection csLock(&AFSLock);
    csLock.Lock();

    ce::list<ce::smart_ptr<FileObject> , AFS_FILEOBJECT_ALLOC >::iterator it;
    ce::list<ce::smart_ptr<FileObject> , AFS_FILEOBJECT_ALLOC >::iterator itEnd = m_FileObjectList.end();
    HRESULT hr = E_FAIL;

    //
    // Search out our file
    ASSERT(csLock.IsLocked());
    for(it = m_FileObjectList.begin(); it != itEnd; ++it) {
        if(usFID == (*it)->FID()) {
           StringConverter myFile;

           IFDBG(BOOL fFound = FALSE);
           ce::list<ce::smart_ptr<PerFileState>, AFS_PERFILESTATE_ALLOC >::iterator itPerFile;
           (*it)->Close();
//#error check ALL .appends for error!!!
           myFile.append((*it)->FileName());

           //
           // Search out a per-file node -- dec the ref cnt, and if its 0 delete the node
           ASSERT(csLock.IsLocked());
           for(itPerFile = m_PerFileState.begin(); itPerFile != m_PerFileState.end(); ++itPerFile) {
               PerFileState *pState = *itPerFile;

               ASSERT(0 !=pState->m_uiRefCnt);
               ASSERT(NULL != pState->m_FileName.GetString());
               ASSERT(NULL != myFile.GetString());
//#error do the hash map
               if(0 == wcscmp(pState->m_FileName.GetString(), myFile.GetString())) {
                    IFDBG(fFound = TRUE);
                    pState->m_uiRefCnt --;

                    //
                    // If the file ISNT shared delete, dec our counter
                    if(!(*it)->IsShareDelete()) {
                        pState->m_uiNonShareDeleteOpens --;
                    }

                    //
                    // If anyone is waiting on this close to wakeup (oplock) do that now
                    while(pState->m_BlockedOnOpLockList.size()) {
                        SetEvent(pState->m_BlockedOnOpLockList.front());
                        pState->m_BlockedOnOpLockList.pop_front();
                    }

                    //
                    // If we are the owner of the transport token, delete that
                    if(usFID == pState->m_usLockFID && pState->m_pTransToken) {
                        pState->m_pfnDeleteToken(pState->m_pTransToken);
                        pState->m_pTransToken = NULL;
                    }

                    //
                    // If we had the last reference, delete our internal state
                    //    and delete the file if we were told to
                    if(0 == pState->m_uiRefCnt) {
                        //
                        // If were told do delete the file, do so now
                        if(TRUE == pState->m_fDeleteOnClose) {
                            DeleteFile(myFile.GetString());
                        }
                        ASSERT(csLock.IsLocked());
                        m_PerFileState.erase(itPerFile);
                    }
                    break;
               }
           }
           IFDBG(ASSERT(TRUE == fFound));
           ASSERT(csLock.IsLocked());
           m_FileObjectList.erase(it);

           hr = S_OK;
           break;
        }
    }
    ASSERT(SUCCEEDED(hr));
    return hr;
}

HRESULT
AbstractFileSystem::FindFile(USHORT usFID, ce::smart_ptr<FileObject> &pFileObject)
{
    CCritSection csLock(&AFSLock);
    csLock.Lock();

    ce::list<ce::smart_ptr<FileObject> , AFS_FILEOBJECT_ALLOC >::iterator it;
    ce::list<ce::smart_ptr<FileObject> , AFS_FILEOBJECT_ALLOC >::iterator itEnd = m_FileObjectList.end();
    HRESULT hr = E_FAIL;

    //
    // Search out our file
    ASSERT(csLock.IsLocked());
    for(it = m_FileObjectList.begin(); it != itEnd; ++it) {
        if(usFID == (*it)->FID()) {
            pFileObject = (*it);
            hr = S_OK;
            goto Done;
        }
    }
    Done:
        return hr;
}



//  Compare two range nodes
//  0 = equals
//  + = pOne > pTwo
//  - = pOne < pTwo
INT CompareRange(RANGE_NODE *pOne, RANGE_NODE *pTwo)
{
    RANGE_NODE *pA, *pB;
    BOOL fFlip = FALSE;
    INT  iRet;

    //
    // Sort the two points
    if(pOne->Offset.QuadPart >= pTwo->Offset.QuadPart) {
        pA = pTwo;
        pB = pOne;
        fFlip = TRUE;
    } else {
        pA = pOne;
        pB = pTwo;
        fFlip = FALSE;
    }

    //
    // 3 cases (since they are sorted)
    //
    // 1. A and B are equal (then are equal)
    // 2. A and B overlap   (and thus are equal)
    // 3. A is less than B  (then are not equal)
    if(pA->Offset.QuadPart == pB->Offset.QuadPart && pA->Length.QuadPart == pB->Length.QuadPart) {
        //
        // Equal
        iRet = 0;
    }
    else if(pA->Offset.QuadPart + pA->Length.QuadPart > pB->Offset.QuadPart) {
        //
        // Overlap
        iRet = 0;
    } else {
        //
        // One is greater (using the fFlip flag to determine which one)
        iRet = (fFlip)?-1:1;
    }

    return iRet;
}

HRESULT RangeLock::IsLocked(SMB_LARGELOCK_RANGE *pRangeLock, BOOL *pfVal, USHORT *pFID)
{
    ce::list<RANGE_NODE, RANGE_NODE_ALLOC >::iterator it;
    RANGE_NODE NewNode;
    NewNode.Offset.LowPart = pRangeLock->OffsetLow;
    NewNode.Offset.HighPart= pRangeLock->OffsetHigh;
    NewNode.Length.LowPart = pRangeLock->LengthLow;
    NewNode.Length.HighPart = pRangeLock->LengthHigh;

    BOOL fFound = FALSE;

    for(it = m_LockList.begin(); it != m_LockList.end(); ++it) {
        if(0 == CompareRange(&NewNode, &(*it))) {
            if(NULL != pFID) {
                *pFID = (*it).usFID;
            }
            fFound = TRUE;
            goto Done;
        }
    }
    Done:
        *pfVal = fFound;
        return S_OK;
}

HRESULT
RangeLock::Lock(USHORT usFID, SMB_LARGELOCK_RANGE *pRangeLock)
{
    ce::list<RANGE_NODE, RANGE_NODE_ALLOC >::iterator it;

    HRESULT hr = S_OK;
    RANGE_NODE NewNode;
    NewNode.Offset.LowPart = pRangeLock->OffsetLow;
    NewNode.Offset.HighPart= pRangeLock->OffsetHigh;
    NewNode.Length.LowPart = pRangeLock->LengthLow;
    NewNode.Length.HighPart = pRangeLock->LengthHigh;
    NewNode.usFID = usFID;

    //
    // Make sure the lock isnt for zero length (anything else is valid)
    if(!NewNode.Length.QuadPart) {
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    //
    // Search out and lock the node
    for(it = m_LockList.begin(); it != m_LockList.end(); ++it) {
        INT uiCompare = CompareRange(&(*it), &NewNode);
        ASSERT(0 != uiCompare);
        if(uiCompare > 0) {
            break;
        }
    }
    m_LockList.insert(it, NewNode);

    #ifdef DEBUG
        //
        // Perform a sanity check on the list
        ce::list<RANGE_NODE, RANGE_NODE_ALLOC >::iterator itPrev = m_LockList.begin();
        ce::list<RANGE_NODE, RANGE_NODE_ALLOC >::iterator itVar = m_LockList.begin();
        itVar ++;
        for(; itVar != m_LockList.end(); ++itVar) {
            ASSERT(CompareRange(&(*itPrev), &(*itVar)) < 0);
            itPrev = itVar;
        }
    #endif

    return S_OK;
}

HRESULT
RangeLock::UnLock(USHORT usFID, SMB_LARGELOCK_RANGE *pRangeLock)
{
    ce::list<RANGE_NODE, RANGE_NODE_ALLOC >::iterator it;
    RANGE_NODE NewNode;
    NewNode.Offset.LowPart = pRangeLock->OffsetLow;
    NewNode.Offset.HighPart= pRangeLock->OffsetHigh;
    NewNode.Length.LowPart = pRangeLock->LengthLow;
    NewNode.Length.HighPart = pRangeLock->LengthHigh;
    BOOL fFound = FALSE;

    //
    // Find and unlock our node
    for(it = m_LockList.begin(); it != m_LockList.end(); ++it) {

        //
        // If the ranges are equal and the FID's match, erase it
        if((0 == CompareRange(&NewNode, &(*it))) && (usFID == (*it).usFID)) {
            m_LockList.erase(it);
            fFound = TRUE;
            goto Done;
        }
    }
    Done:
        return fFound?S_OK:E_FAIL;
}

HRESULT
RangeLock::UnLockAll(USHORT usFID)
{
    ce::list<RANGE_NODE, RANGE_NODE_ALLOC >::iterator it;

    //
    // Find and unlock our node
    for(it = m_LockList.begin(); it != m_LockList.end();) {
        //
        // If the FID's match, erase it
        if(usFID == (*it).usFID) {
            m_LockList.erase(it++);
        } else {
            ++it;
        }
    }
    return S_OK;
}


