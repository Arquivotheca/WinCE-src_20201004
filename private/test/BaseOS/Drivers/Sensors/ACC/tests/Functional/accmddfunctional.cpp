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

#include "AccApiFunctional.h"
#include "AccApiFuncWrap.h"
#include "AccHelper.h"

extern AccTestParam g_AccParam;




//------------------------------------------------------------------------------
// tst_ACC_MDD_IOCTL
//
void tst_ACC_MDD_IOCTL( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    BOOL bResult = TRUE;
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    //Do we have what we need....
    VERIFY_STEP("Varifying Client Payload", (pPayload!=NULL), TRUE );
    VERIFY_STEP("Varifying Client Payload Data", (pPayload->pBufIn!=NULL), TRUE );
    VERIFY_STEP("Varifying Client Payload Size", (pPayload->dwLenIn==sizeof(AccIoControlTstData_t)), TRUE );

    //Make life easier
    AccIoControlTstData_t* pData = (AccIoControlTstData_t*)pPayload->pBufIn;

    BOOL bValidHandle = (pData->hDevice_in != NULL && pData->hDevice_in != INVALID_HANDLE_VALUE );


    //What are we dealing with
    LOG( "tst_ACC_MDD_IOCTL:" );
    LOG( "[in]  Context...................(0x%08X)", (pData->hDevice_in)?pData->hDevice_in:0x00000000 );
    LOG( "[in]  IOCTL.....................(0x%08X)", pData->dwIOCTL_in);
    LOG( "[in]  In Buffer.................(0x%08X)", (pData->lpInBuffer_in)?pData->lpInBuffer_in:0x00000000 );
    LOG( "[in]  In Buffer Size............(%d)", pData->nInBufferSize_in );
    LOG( "[in]  Out Buffer Size...........(%d)", pData->nOutBufferSize_in );
    LOG( "[out] Bytes Returned............(%d)", (pData->lpBytesReturned_out)?*(pData->lpBytesReturned_out):0 );
    LOG( "[exp] Bytes Returned............(%d)", pData->bytesReturned_exp );
    LOG( "[exp] IO Return.................(%s)", pData->bIoReturn_exp?_T("TRUE"):_T("FALSE"));
    LOG( "[exp] Expected Error............(%d)\n\n", pData->dwLastErr_exp );

    BOOL bIoResult = DeviceIoControl(
        pData->hDevice_in,
        pData->dwIOCTL_in,
        pData->lpInBuffer_in,
        pData->nInBufferSize_in,
        pData->lpOutBuffer_out,
        pData->nOutBufferSize_in,
        pData->lpBytesReturned_out,
        NULL);
    DWORD dwLatError = GetLastError();
    
    //Did we meet expectations?  
    if( bIoResult != pData->bIoReturn_exp )
    {
        LOG_ERROR( "IO Result - Expected:(%s)  Received:(%s)", ((pData->bIoReturn_exp)?_T("TRUE"):_T("FALSE")) ,(bIoResult?_T("TRUE"):_T("FALSE")));
    }

    //Were we expecting anything else?
    if( pData->lpBytesReturned_out != NULL &&
        pData->lpOutBuffer_out != NULL  &&
        pData->hDevice_in != NULL       && 
        pData->hDevice_in != INVALID_HANDLE_VALUE )
    {
        if( pData->dwLastErr_exp != *((DWORD*)(pData->lpOutBuffer_out )) )
        {
            LOG_ERROR( "Last Error - Expected:(%d)  Received:(%d)", pData->dwLastErr_exp,*((DWORD*)(pData->lpOutBuffer_out )) );
        }

        if( pData->bytesReturned_exp != *(pData->lpBytesReturned_out) )
        {
            LOG_WARN( "Bytes Returned - Expected:(0x%08X)  Received:(0x%08X)",pData->bytesReturned_exp, *(pData->lpBytesReturned_out) );
        }
    }


DONE:

    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_ACC_MDD_IOCTL


//------------------------------------------------------------------------------
// tst_SNS_MDD_IOCTL
//
void tst_SNS_MDD_IOCTL( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    BOOL bResult = TRUE;
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    //Do we have what we need....
    VERIFY_STEP("Varifying Client Payload", (pPayload!=NULL), TRUE );
    VERIFY_STEP("Varifying Client Payload Data", (pPayload->pBufIn!=NULL), TRUE );
    VERIFY_STEP("Varifying Client Payload Size", (pPayload->dwLenIn==sizeof(AccIoControlTstData_t)), TRUE );

    //Make life easier
    AccIoControlTstData_t* pData = (AccIoControlTstData_t*)pPayload->pBufIn;

    //What are we dealing with
    LOG( "tst_SNS_MDD_IOCTL:" );
    LOG( "[in]  Context...................(0x%08X)", (pData->hDevice_in)?pData->hDevice_in:0x00000000 );
    LOG( "[in]  IOCTL.....................(0x%08X)", pData->dwIOCTL_in);
    LOG( "[in]  In Buffer.................(0x%08X)", (pData->lpInBuffer_in)?pData->lpInBuffer_in:0x00000000 );
    LOG( "[in]  In Buffer Size............(%d)", pData->nInBufferSize_in );
    LOG( "[in]  Out Buffer Size...........(%d)", pData->nOutBufferSize_in );
    LOG( "[out] Bytes Returned............(%d)", (pData->lpBytesReturned_out)?*(pData->lpBytesReturned_out):0 );
    LOG( "[exp] Bytes Returned............(%d)", pData->bytesReturned_exp );
    LOG( "[exp] IO Return.................(%s)", pData->bIoReturn_exp?_T("TRUE"):_T("FALSE"));
    LOG( "[exp] Expected Error............(%d)\n\n", pData->dwLastErr_exp );

    BOOL bIoResult = DeviceIoControl(
        pData->hDevice_in,
        pData->dwIOCTL_in,
        pData->lpInBuffer_in,
        pData->nInBufferSize_in,
        pData->lpOutBuffer_out,
        pData->nOutBufferSize_in,
        pData->lpBytesReturned_out,
        NULL);
    DWORD dwLatError = GetLastError();
    
    //Did we meet expectations?  
    if( bIoResult != pData->bIoReturn_exp )
    {
        LOG_ERROR( "IO Result - Expected:(%s)  Received:(%s)", ((pData->bIoReturn_exp)?_T("TRUE"):_T("FALSE")) ,(bIoResult?_T("TRUE"):_T("FALSE")));
    }

    //Were we expecting anything else?
    if( pData->lpBytesReturned_out != NULL &&
        pData->lpOutBuffer_out != NULL  &&
        pData->hDevice_in != NULL       && 
        pData->hDevice_in != INVALID_HANDLE_VALUE )
    {
        if( pData->bytesReturned_exp != *(pData->lpBytesReturned_out) )
        {
            LOG_WARN( "Bytes Returned - Expected:(0x%08X)  Received:(0x%08X)",pData->bytesReturned_exp, *(pData->lpBytesReturned_out) );
        }
    }


DONE:

    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_SNS_MDD_IOCTL